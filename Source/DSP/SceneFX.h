#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>
#include <cmath>
#include <random>
#include <vector>

namespace rmx {

enum class SceneFxType : int {
    Off = 0,
    // BUILD UP (Red)
    BpfEcho = 1,
    Echo = 2,
    Noise = 3,
    SpiralUp = 4,
    ReverbUp = 5,
    // BREAK DOWN (Blue)
    HpfEcho = 6,
    LpfEcho = 7,
    CrushEcho = 8,
    SpiralDown = 9,
    ReverbDown = 10
};

class SceneFX {
public:
    void prepare(double sampleRate, int maxBlockSize) {
        sr = sampleRate > 0.0 ? sampleRate : 44100.0;
        juce::dsp::ProcessSpec spec { sr, static_cast<juce::uint32>(maxBlockSize), 1 };

        const int delayLen = static_cast<int>(sr * 4.0) + 16;
        for (int ch = 0; ch < 2; ++ch) {
            delayBuf[ch].assign(static_cast<size_t>(delayLen), 0.0f);
            svf[ch].prepare(spec);
            svf[ch].reset();
            noiseSvf[ch].prepare(spec);
            noiseSvf[ch].reset();
            crushHold[ch] = 0.0f;
            dcPrevIn[ch] = 0.0f;
            dcPrevOut[ch] = 0.0f;
        }
        writePos = 0;
        crushCounter = 0;
        spiralPhase = 0.0;

        reverb.setSampleRate(sr);
        reverb.reset();

        knobSmoothed.reset(sr, 0.02);
        sub1Smoothed.reset(sr, 0.02);
        sub2Smoothed.reset(sr, 0.02);
        delaySmoothed.reset(sr, 0.04);

        rngState = 0x12345678u;
    }

    void resetTails() noexcept {
        for (int ch = 0; ch < 2; ++ch)
            std::fill(delayBuf[ch].begin(), delayBuf[ch].end(), 0.0f);
        reverb.reset();
    }

    void process(juce::AudioBuffer<float>& buffer,
                 SceneFxType fxType,
                 float mainKnob,
                 float subParam1,
                 float subParam2,
                 double bpm) noexcept
    {
        const int numChannels = std::min(buffer.getNumChannels(), 2);
        const int numSamples = buffer.getNumSamples();
        if (numChannels == 0 || numSamples == 0) return;

        if (fxType == SceneFxType::Off || mainKnob <= 0.002f) {
            // Keep delay line gently decaying so tails don't click when turned off
            if (mainKnob <= 0.002f) {
                knobSmoothed.setTargetValue(0.0f);
            }
            return;
        }

        knobSmoothed.setTargetValue(std::clamp(mainKnob, 0.0f, 1.0f));
        sub1Smoothed.setTargetValue(std::clamp(subParam1, 0.0f, 1.0f));
        sub2Smoothed.setTargetValue(std::clamp(subParam2, 0.0f, 1.0f));

        const int bufSize = static_cast<int>(delayBuf[0].size());
        const double samplesPerBeat = (60.0 / std::max(40.0, bpm)) * sr;

        // Beat division from Sub Parameter 1: 1/8, 1/4, 1/2, 3/4, 1 beat
        static constexpr double kBeatDivs[5] = { 0.125, 0.25, 0.5, 0.75, 1.0 };
        int divIdx = std::clamp(static_cast<int>(subParam1 * 4.99f), 0, 4);
        double targetDelay = std::clamp(samplesPerBeat * kBeatDivs[divIdx], 64.0, static_cast<double>(bufSize - 16));

        // For Spiral Up/Down, mainKnob smoothly bends the delay time for classic pitch spiral
        if (fxType == SceneFxType::SpiralUp) {
            double spiralScale = std::pow(2.0, -static_cast<double>(mainKnob) * 2.2); // shorter delay -> higher pitch
            targetDelay = std::clamp(samplesPerBeat * 0.5 * spiralScale, 48.0, static_cast<double>(bufSize - 16));
        } else if (fxType == SceneFxType::SpiralDown) {
            double spiralScale = 1.0 + static_cast<double>(mainKnob) * 2.5; // longer delay -> lower pitch
            targetDelay = std::clamp(samplesPerBeat * 0.35 * spiralScale, 64.0, static_cast<double>(bufSize - 16));
        }

        delaySmoothed.setTargetValue(static_cast<float>(targetDelay));

        // Update Reverb parameters once per block if Reverb Up/Down is selected
        if (fxType == SceneFxType::ReverbUp || fxType == SceneFxType::ReverbDown) {
            juce::Reverb::Parameters rp;
            rp.roomSize = std::clamp(0.45f + mainKnob * 0.52f, 0.0f, 0.98f);
            rp.damping = (fxType == SceneFxType::ReverbUp)
                             ? std::clamp(0.25f * (1.0f - subParam2), 0.02f, 0.8f)
                             : std::clamp(0.45f + mainKnob * 0.5f, 0.1f, 0.98f);
            rp.wetLevel = std::clamp(mainKnob * (0.55f + 0.45f * subParam2), 0.0f, 1.0f);
            rp.dryLevel = (fxType == SceneFxType::ReverbDown)
                              ? std::clamp(1.0f - mainKnob * 0.85f, 0.05f, 1.0f)
                              : std::clamp(1.0f - mainKnob * 0.35f, 0.3f, 1.0f);
            rp.width = 0.95f;
            rp.freezeMode = (mainKnob > 0.96f && subParam1 > 0.85f) ? 1.0f : 0.0f;
            reverb.setParameters(rp);

            // Pre-filter for Reverb Up (HPF rise) or Reverb Down (LPF dark wash)
            float cutHz = (fxType == SceneFxType::ReverbUp)
                              ? (120.0f * std::pow(40.0f, mainKnob))
                              : (16000.0f * std::pow(0.03f, mainKnob));
            for (int ch = 0; ch < numChannels; ++ch) {
                svf[ch].setType(fxType == SceneFxType::ReverbUp
                                    ? juce::dsp::StateVariableTPTFilterType::highpass
                                    : juce::dsp::StateVariableTPTFilterType::lowpass);
                svf[ch].setCutoffFrequency(std::clamp(cutHz, 40.0f, 18000.0f));
                svf[ch].setResonance(0.75f + subParam2 * 2.2f);
            }

            // Copy dry, filter, apply reverb
            if (numChannels == 2) {
                for (int i = 0; i < numSamples; ++i) {
                    float l = buffer.getSample(0, i);
                    float r = buffer.getSample(1, i);
                    float fl = svf[0].processSample(0, l);
                    float fr = svf[1].processSample(0, r);
                    // Blend pre-filtered into reverb buffer
                    buffer.setSample(0, i, l * (1.0f - mainKnob * 0.5f) + fl * (mainKnob * 0.5f));
                    buffer.setSample(1, i, r * (1.0f - mainKnob * 0.5f) + fr * (mainKnob * 0.5f));
                }
                reverb.processStereo(buffer.getWritePointer(0), buffer.getWritePointer(1), numSamples);
            } else {
                reverb.processMono(buffer.getWritePointer(0), numSamples);
            }
            return;
        }

        // Configure SVF filters per block for Echo / Noise / Crush modes
        const float res = std::clamp(0.75f + subParam2 * 6.5f, 0.7f, 8.5f);
        if (fxType == SceneFxType::BpfEcho) {
            float freq = std::clamp(200.0f * std::pow(32.0f, mainKnob), 120.0f, 12000.0f);
            for (int ch = 0; ch < numChannels; ++ch) {
                svf[ch].setType(juce::dsp::StateVariableTPTFilterType::bandpass);
                svf[ch].setCutoffFrequency(freq);
                svf[ch].setResonance(res);
            }
        } else if (fxType == SceneFxType::HpfEcho) {
            float freq = std::clamp(40.0f * std::pow(200.0f, mainKnob), 40.0f, 12000.0f);
            for (int ch = 0; ch < numChannels; ++ch) {
                svf[ch].setType(juce::dsp::StateVariableTPTFilterType::highpass);
                svf[ch].setCutoffFrequency(freq);
                svf[ch].setResonance(0.75f + subParam2 * 3.5f);
            }
        } else if (fxType == SceneFxType::LpfEcho) {
            float freq = std::clamp(18000.0f * std::pow(0.012f, mainKnob), 140.0f, 18000.0f);
            for (int ch = 0; ch < numChannels; ++ch) {
                svf[ch].setType(juce::dsp::StateVariableTPTFilterType::lowpass);
                svf[ch].setCutoffFrequency(freq);
                svf[ch].setResonance(0.75f + subParam2 * 3.5f);
            }
        } else if (fxType == SceneFxType::Noise) {
            float freq = std::clamp(180.0f * std::pow(55.0f, mainKnob), 150.0f, 14000.0f);
            for (int ch = 0; ch < numChannels; ++ch) {
                noiseSvf[ch].setType(juce::dsp::StateVariableTPTFilterType::bandpass);
                noiseSvf[ch].setCutoffFrequency(freq);
                noiseSvf[ch].setResonance(1.2f + subParam2 * 7.0f);
            }
        }

        const int crushFactor = std::max(1, static_cast<int>(1.0f + mainKnob * (4.0f + subParam1 * 28.0f)));
        const float bitSteps = std::pow(2.0f, std::clamp(12.0f - mainKnob * 9.0f, 2.5f, 12.0f));

        for (int i = 0; i < numSamples; ++i) {
            const float k = knobSmoothed.getNextValue();
            const float s1 = sub1Smoothed.getNextValue();
            const float s2 = sub2Smoothed.getNextValue();
            const float dSamples = delaySmoothed.getNextValue();

            const bool updateCrush = (++crushCounter >= crushFactor);
            if (updateCrush) crushCounter = 0;

            for (int ch = 0; ch < numChannels; ++ch) {
                float dry = buffer.getSample(ch, i);
                float out = dry;

                if (fxType == SceneFxType::Noise) {
                    // Authentic Pioneer RMX-1000 Noise Sweep Riser
                    float wn = nextWhiteNoise();
                    float filteredNoise = noiseSvf[ch].processSample(0, wn) * (0.55f + 0.8f * s1);
                    // Gentle saturation & level matching
                    filteredNoise = std::tanh(filteredNoise * 1.4f);
                    float noiseLevel = k * k * 0.72f;
                    // Subtle high-pass ducks dry slightly at extreme knob values
                    out = dry * (1.0f - k * 0.28f) + filteredNoise * noiseLevel;
                } else {
                    // Read interpolated delay line
                    double readPos = static_cast<double>(writePos) - static_cast<double>(dSamples);
                    while (readPos < 0.0) readPos += static_cast<double>(bufSize);
                    float delayed = readInterpolated(delayBuf[ch], readPos, bufSize);

                    float feedback = std::clamp(0.32f + k * 0.56f + s2 * 0.09f, 0.1f, 0.93f);
                    float wetMix = std::clamp(k * 0.88f, 0.0f, 0.95f);

                    if (fxType == SceneFxType::BpfEcho) {
                        float filtIn = svf[ch].processSample(0, dry);
                        delayBuf[ch][static_cast<size_t>(writePos)] = std::tanh(filtIn + delayed * feedback);
                        out = dry * (1.0f - wetMix * 0.45f) + delayed * wetMix;
                    } else if (fxType == SceneFxType::Echo) {
                        // Gentle DC/HPF in echo loop
                        float hp = 0.965f * (dcPrevOut[ch] + delayed - dcPrevIn[ch]);
                        dcPrevIn[ch] = delayed;
                        dcPrevOut[ch] = hp;
                        delayBuf[ch][static_cast<size_t>(writePos)] = std::tanh(dry + hp * feedback);
                        out = dry * (1.0f - wetMix * 0.35f) + delayed * wetMix;
                    } else if (fxType == SceneFxType::HpfEcho || fxType == SceneFxType::LpfEcho) {
                        float filtDry = svf[ch].processSample(0, dry);
                        delayBuf[ch][static_cast<size_t>(writePos)] = std::tanh(filtDry + delayed * feedback);
                        out = filtDry * (1.0f - wetMix * 0.4f) + delayed * wetMix * 0.85f;
                    } else if (fxType == SceneFxType::CrushEcho) {
                        if (updateCrush) {
                            float q = std::round(dry * bitSteps) / bitSteps;
                            crushHold[ch] = q;
                        }
                        float crushed = dry * (1.0f - k) + crushHold[ch] * k;
                        delayBuf[ch][static_cast<size_t>(writePos)] = std::tanh(crushed + delayed * feedback * 0.85f);
                        out = crushed * (1.0f - wetMix * 0.45f) + delayed * wetMix * 0.75f;
                    } else if (fxType == SceneFxType::SpiralUp || fxType == SceneFxType::SpiralDown) {
                        // Classic Pioneer Spiral delay: feedback loop with smooth pitch bend
                        float fb = std::clamp(0.45f + k * 0.46f, 0.2f, 0.94f);
                        delayBuf[ch][static_cast<size_t>(writePos)] = std::tanh(dry * 0.85f + delayed * fb);
                        out = dry * (1.0f - wetMix * 0.5f) + delayed * wetMix;
                    }
                }

                buffer.setSample(ch, i, out);
            }

            writePos = (writePos + 1) % bufSize;
        }
    }

private:
    float nextWhiteNoise() noexcept {
        // Fast xorshift32 PRNG (real-time safe)
        uint32_t x = rngState;
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        rngState = x;
        return (static_cast<float>(x & 0x00FFFFFFu) / 8388608.0f) - 1.0f;
    }

    static float readInterpolated(const std::vector<float>& buf, double pos, int size) noexcept {
        int i0 = static_cast<int>(std::floor(pos)) % size;
        if (i0 < 0) i0 += size;
        int i1 = (i0 + 1) % size;
        float frac = static_cast<float>(pos - std::floor(pos));
        return buf[static_cast<size_t>(i0)] * (1.0f - frac) + buf[static_cast<size_t>(i1)] * frac;
    }

    double sr = 44100.0;
    std::array<std::vector<float>, 2> delayBuf;
    int writePos = 0;

    std::array<juce::dsp::StateVariableTPTFilter<float>, 2> svf;
    std::array<juce::dsp::StateVariableTPTFilter<float>, 2> noiseSvf;
    juce::Reverb reverb;

    std::array<float, 2> crushHold { 0.0f, 0.0f };
    int crushCounter = 0;
    std::array<float, 2> dcPrevIn { 0.0f, 0.0f };
    std::array<float, 2> dcPrevOut { 0.0f, 0.0f };
    double spiralPhase = 0.0;

    juce::SmoothedValue<float> knobSmoothed, sub1Smoothed, sub2Smoothed, delaySmoothed;
    uint32_t rngState = 0x12345678u;
};

} // namespace rmx
