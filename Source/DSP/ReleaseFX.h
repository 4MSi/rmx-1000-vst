#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>
#include <cmath>
#include <vector>

namespace rmx {

enum class ReleaseFxMode : int {
    VinylBrake = 0,
    Echo = 1,
    BackSpin = 2
};

class ReleaseFX {
public:
    void prepare(double sampleRate, int /*maxBlockSize*/) {
        sr = sampleRate > 0.0 ? sampleRate : 44100.0;
        const int bufLen = static_cast<int>(sr * 4.0) + 16; // 4-second stereo ring buffer
        for (int ch = 0; ch < 2; ++ch) {
            ringBuffer[ch].assign(static_cast<size_t>(bufLen), 0.0f);
            echoBuffer[ch].assign(static_cast<size_t>(bufLen), 0.0f);
            echoHpState[ch] = 0.0f;
            echoPrevIn[ch] = 0.0f;
        }
        writePos = 0;
        echoWritePos = 0;
        active = false;
        wasActive = false;
        readPosFrac = 0.0;
        speed = 1.0;
        elapsedSamples = 0;
        releaseFade = 1.0f;
    }

    // Trigger or hold the Release FX lever
    void process(juce::AudioBuffer<float>& buffer,
                 ReleaseFxMode mode,
                 bool engaged,
                 double bpm,
                 float subTimeNorm = 0.5f) noexcept
    {
        const int numChannels = std::min(buffer.getNumChannels(), 2);
        const int numSamples = buffer.getNumSamples();
        if (numChannels == 0 || numSamples == 0) return;

        const int bufSize = static_cast<int>(ringBuffer[0].size());
        const double samplesPerBeat = (60.0 / std::max(40.0, bpm)) * sr;

        // Edge detection for lever pull
        if (engaged && !wasActive) {
            active = true;
            elapsedSamples = 0;
            releaseFade = 1.0f;
            if (mode == ReleaseFxMode::VinylBrake) {
                readPosFrac = static_cast<double>(writePos);
                speed = 1.0;
            } else if (mode == ReleaseFxMode::BackSpin) {
                readPosFrac = static_cast<double>(writePos);
                speed = -2.8; // Iconic fast Pioneer reverse turntable spin
            }
        } else if (!engaged && wasActive) {
            active = false;
        }
        wasActive = engaged;

        // Echo delay time: 1/2 beat, 3/4 beat, or 1 beat based on subTimeNorm
        double echoBeats = (subTimeNorm < 0.33f) ? 0.5 : (subTimeNorm < 0.66f ? 0.75 : 1.0);
        const int echoDelaySamples = std::clamp(static_cast<int>(samplesPerBeat * echoBeats), 128, bufSize - 8);

        // Brake duration: 0.4s to 2.2s
        const double brakeDurationSamples = sr * (0.45 + 1.65 * static_cast<double>(subTimeNorm));
        // Backspin duration: 0.5s to 1.4s
        const double backSpinDurationSamples = sr * (0.55 + 0.85 * static_cast<double>(subTimeNorm));

        for (int i = 0; i < numSamples; ++i) {
            // Always record dry audio into ring buffer and echo buffer when not engaged
            for (int ch = 0; ch < numChannels; ++ch) {
                float dry = buffer.getSample(ch, i);
                if (!active) {
                    ringBuffer[ch][static_cast<size_t>(writePos)] = dry;
                    // Feed echo buffer continuously so Echo release has immediate tail
                    echoBuffer[ch][static_cast<size_t>(echoWritePos)] = dry;
                }
            }

            if (active) {
                if (mode == ReleaseFxMode::VinylBrake) {
                    double progress = static_cast<double>(elapsedSamples) / brakeDurationSamples;
                    if (progress >= 1.0) {
                        speed = 0.0;
                        releaseFade *= 0.995f;
                    } else {
                        // Quadratic deceleration like a Technics SL-1200 platter stopping
                        double rem = 1.0 - progress;
                        speed = rem * rem;
                        releaseFade = (progress > 0.85) ? static_cast<float>((1.0 - progress) / 0.15) : 1.0f;
                    }

                    // Keep writing incoming audio during first half so brake slows down live music
                    for (int ch = 0; ch < numChannels; ++ch) {
                        ringBuffer[ch][static_cast<size_t>(writePos)] = buffer.getSample(ch, i);
                        float s = readInterpolated(ringBuffer[ch], readPosFrac, bufSize);
                        buffer.setSample(ch, i, s * releaseFade);
                    }
                    readPosFrac += speed;
                    if (readPosFrac >= bufSize) readPosFrac -= bufSize;
                } else if (mode == ReleaseFxMode::BackSpin) {
                    double progress = static_cast<double>(elapsedSamples) / backSpinDurationSamples;
                    if (progress >= 1.0) {
                        releaseFade *= 0.992f;
                        speed *= 0.99f;
                    } else {
                        // Fast initial whip back, then smooth spin-down
                        double env = std::sin(progress * juce::MathConstants<double>::pi);
                        speed = -(1.2 + 3.6 * env);
                        releaseFade = (progress > 0.78) ? static_cast<float>((1.0 - progress) / 0.22) : 1.0f;
                    }

                    for (int ch = 0; ch < numChannels; ++ch) {
                        float s = readInterpolated(ringBuffer[ch], readPosFrac, bufSize);
                        buffer.setSample(ch, i, s * releaseFade);
                    }
                    readPosFrac += speed;
                    while (readPosFrac < 0.0) readPosFrac += bufSize;
                } else if (mode == ReleaseFxMode::Echo) {
                    // Mute dry input, let tempo-synced high-passed dub echo ring out!
                    int readIdx = (echoWritePos - echoDelaySamples + bufSize) % bufSize;
                    for (int ch = 0; ch < numChannels; ++ch) {
                        float delayed = echoBuffer[ch][static_cast<size_t>(readIdx)];
                        // Gentle 1-pole high-pass in feedback loop for classic Pioneer echo wash
                        float hp = 0.96f * (echoHpState[ch] + delayed - echoPrevIn[ch]);
                        echoPrevIn[ch] = delayed;
                        echoHpState[ch] = hp;

                        float feedback = 0.72f + 0.14f * subTimeNorm;
                        echoBuffer[ch][static_cast<size_t>(echoWritePos)] = hp * feedback;
                        buffer.setSample(ch, i, delayed);
                    }
                }
                ++elapsedSamples;
            }

            writePos = (writePos + 1) % bufSize;
            echoWritePos = (echoWritePos + 1) % bufSize;
        }
    }

private:
    static float readInterpolated(const std::vector<float>& buf, double pos, int size) noexcept {
        int i0 = static_cast<int>(std::floor(pos)) % size;
        if (i0 < 0) i0 += size;
        int i1 = (i0 + 1) % size;
        float frac = static_cast<float>(pos - std::floor(pos));
        return buf[static_cast<size_t>(i0)] * (1.0f - frac) + buf[static_cast<size_t>(i1)] * frac;
    }

    double sr = 44100.0;
    std::array<std::vector<float>, 2> ringBuffer;
    std::array<std::vector<float>, 2> echoBuffer;
    std::array<float, 2> echoHpState { 0.0f, 0.0f };
    std::array<float, 2> echoPrevIn { 0.0f, 0.0f };

    int writePos = 0;
    int echoWritePos = 0;
    bool active = false;
    bool wasActive = false;
    double readPosFrac = 0.0;
    double speed = 1.0;
    int elapsedSamples = 0;
    float releaseFade = 1.0f;
};

} // namespace rmx
