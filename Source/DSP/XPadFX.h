#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>

namespace rmx {

enum class DrumVoiceType : int {
    Kick = 0,
    Snare = 1,
    Clap = 2,
    HiHat = 3
};

class XPadFX {
public:
    void prepare(double sampleRate) noexcept {
        sr = sampleRate > 0.0 ? sampleRate : 44100.0;
        clearPattern();
        for (auto& v : voices) v.active = false;
        prevStep16 = -1;
        rollSampleCounter = 0;
        rngState = 0xCAFEBABEu;
    }

    void triggerDrum(DrumVoiceType type, float pitchNorm) noexcept {
        int idx = static_cast<int>(type);
        if (idx < 0 || idx >= 4) return;
        auto& v = voices[static_cast<size_t>(idx)];
        v.active = true;
        v.samplePos = 0;
        v.phase = 0.0;
        // pitchNorm [0..1] -> -12 to +12 semitones
        float semitones = (pitchNorm - 0.5f) * 24.0f;
        v.pitchRatio = std::pow(2.0f, semitones / 12.0f);
    }

    void clearPattern() noexcept {
        for (auto& row : overdubPattern)
            row.fill(false);
    }

    void process(juce::AudioBuffer<float>& buffer,
                 DrumVoiceType selectedDrum,
                 bool touchActive,
                 float touchPosNorm,
                 float pitchNorm,
                 bool overdubEnabled,
                 bool quantizeEnabled,
                 float xpadLevel,
                 double bpm,
                 double barPhase) noexcept
    {
        const int numChannels = std::min(buffer.getNumChannels(), 2);
        const int numSamples = buffer.getNumSamples();
        if (numChannels == 0 || numSamples == 0) return;

        const double samplesPerBeat = (60.0 / std::max(40.0, bpm)) * sr;
        const double barInc = ((bpm / 60.0) * 0.25) / sr;

        // X-Pad touchstrip roll divisions: left=1/1, mid-left=1/2, center=1/4, mid-right=1/8, right=1/16, far-right=1/32
        static constexpr double kRollBeats[6] = { 1.0, 0.5, 0.25, 0.125, 0.0625, 0.03125 };
        int zone = std::clamp(static_cast<int>(touchPosNorm * 5.99f), 0, 5);
        int rollIntervalSamples = std::max(64, static_cast<int>(samplesPerBeat * kRollBeats[zone]));

        if (touchActive && !wasTouching) {
            if (!quantizeEnabled) {
                triggerDrum(selectedDrum, pitchNorm);
            }
            rollSampleCounter = 0;
        }
        wasTouching = touchActive;

        double localBarPhase = barPhase;
        const float gain = std::clamp(xpadLevel, 0.0f, 1.5f);

        for (int i = 0; i < numSamples; ++i) {
            // 16-step grid for Overdub & Quantize
            int step16 = static_cast<int>(std::floor(localBarPhase * 16.0)) & 15;
            bool newStep = (step16 != prevStep16);
            if (newStep) {
                prevStep16 = step16;

                // Record into Overdub loop if active & touching
                if (overdubEnabled && touchActive) {
                    overdubPattern[static_cast<size_t>(selectedDrum)][static_cast<size_t>(step16)] = true;
                }

                // Play Overdubbed pattern steps
                if (overdubEnabled) {
                    for (int d = 0; d < 4; ++d) {
                        if (overdubPattern[static_cast<size_t>(d)][static_cast<size_t>(step16)]) {
                            triggerDrum(static_cast<DrumVoiceType>(d), pitchNorm);
                        }
                    }
                } else if (touchActive && quantizeEnabled && zone <= 2) {
                    // Quantized slower hits lock strictly to 16th grid
                    int stepMod = (zone == 0) ? 4 : (zone == 1 ? 2 : 1);
                    if ((step16 % stepMod) == 0) {
                        triggerDrum(selectedDrum, pitchNorm);
                    }
                }
            }

            // Free / fast roll triggers when finger holds X-Pad
            if (touchActive) {
                if (!quantizeEnabled || zone >= 3) {
                    if (++rollSampleCounter >= rollIntervalSamples) {
                        rollSampleCounter = 0;
                        triggerDrum(selectedDrum, pitchNorm);
                    }
                }
            }

            // Render active drum voices
            float drumSum = 0.0f;
            if (voices[0].active) drumSum += renderKick(voices[0]);
            if (voices[1].active) drumSum += renderSnare(voices[1]);
            if (voices[2].active) drumSum += renderClap(voices[2]);
            if (voices[3].active) drumSum += renderHiHat(voices[3]);

            if (drumSum != 0.0f) {
                float scaled = std::tanh(drumSum * gain * 0.9f);
                for (int ch = 0; ch < numChannels; ++ch) {
                    buffer.setSample(ch, i, buffer.getSample(ch, i) + scaled);
                }
            }

            localBarPhase += barInc;
            if (localBarPhase >= 1.0) localBarPhase -= 1.0;
        }
    }

private:
    struct Voice {
        bool active = false;
        int samplePos = 0;
        double phase = 0.0;
        float pitchRatio = 1.0f;
    };

    float nextNoise() noexcept {
        uint32_t x = rngState;
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        rngState = x;
        return (static_cast<float>(x & 0x00FFFFFFu) / 8388608.0f) - 1.0f;
    }

    // Punchy TR-909 style Club Kick
    float renderKick(Voice& v) noexcept {
        double t = static_cast<double>(v.samplePos++) / sr;
        if (t > 0.42) { v.active = false; return 0.0f; }

        double freq = (48.0 + 185.0 * std::exp(-t * 38.0)) * v.pitchRatio;
        v.phase += (juce::MathConstants<double>::twoPi * freq) / sr;
        float body = static_cast<float>(std::sin(v.phase));
        float env = static_cast<float>(std::exp(-t * 9.5));
        float click = (t < 0.004) ? nextNoise() * static_cast<float>(1.0 - t / 0.004) * 0.45f : 0.0f;
        return std::tanh((body * env * 1.35f) + click) * 0.85f;
    }

    // Crisp Club Snare
    float renderSnare(Voice& v) noexcept {
        double t = static_cast<double>(v.samplePos++) / sr;
        if (t > 0.28) { v.active = false; return 0.0f; }

        double freq = (195.0 + 90.0 * std::exp(-t * 45.0)) * v.pitchRatio;
        v.phase += (juce::MathConstants<double>::twoPi * freq) / sr;
        float tone = static_cast<float>(std::sin(v.phase) * std::exp(-t * 22.0)) * 0.55f;
        float snap = nextNoise() * static_cast<float>(std::exp(-t * 16.0)) * 0.70f;
        return std::tanh(tone + snap) * 0.78f;
    }

    // Iconic TR-909 Multi-Tap Club Clap
    float renderClap(Voice& v) noexcept {
        double t = static_cast<double>(v.samplePos++) / sr;
        if (t > 0.30) { v.active = false; return 0.0f; }

        float env = 0.0f;
        if (t < 0.010) env = static_cast<float>(std::exp(-t * 180.0));
        else if (t < 0.020) env = static_cast<float>(std::exp(-(t - 0.010) * 180.0));
        else if (t < 0.030) env = static_cast<float>(std::exp(-(t - 0.020) * 180.0));
        else env = static_cast<float>(std::exp(-(t - 0.030) * 14.0)) * 0.85f;

        // Simple resonant bandpass feel via phase-modulated noise
        v.phase += (juce::MathConstants<double>::twoPi * 1250.0 * v.pitchRatio) / sr;
        float n = nextNoise() * (0.65f + 0.35f * static_cast<float>(std::sin(v.phase)));
        return std::tanh(n * env * 1.4f) * 0.72f;
    }

    // Metallic Club Hi-Hat
    float renderHiHat(Voice& v) noexcept {
        double t = static_cast<double>(v.samplePos++) / sr;
        if (t > 0.14) { v.active = false; return 0.0f; }

        double f1 = 6200.0 * v.pitchRatio;
        v.phase += (juce::MathConstants<double>::twoPi * f1) / sr;
        float metal = static_cast<float>(std::sin(v.phase) * std::sin(v.phase * 1.47));
        float n = nextNoise();
        float env = static_cast<float>(std::exp(-t * 36.0));
        return (n * 0.72f + metal * 0.28f) * env * 0.65f;
    }

    double sr = 44100.0;
    std::array<Voice, 4> voices;
    std::array<std::array<bool, 16>, 4> overdubPattern {};
    bool wasTouching = false;
    int prevStep16 = -1;
    int rollSampleCounter = 0;
    uint32_t rngState = 0xCAFEBABEu;
};

} // namespace rmx
