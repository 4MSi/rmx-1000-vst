#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>
#include <cmath>
#include <vector>

namespace rmx {

enum class IsolatorFxMode : int {
    Off = 0,
    CutAdd = 1,
    TransRoll = 2,
    GateDrive = 3
};

class IsolatorFX {
public:
    void prepare(double sampleRate, int maxBlockSize) {
        sr = sampleRate > 0.0 ? sampleRate : 44100.0;
        juce::dsp::ProcessSpec spec { sr, static_cast<juce::uint32>(maxBlockSize), 1 };

        // Pioneer DJ RMX-1000 crossover frequencies: 250 Hz (Low/Mid) and 4000 Hz (Mid/High)
        for (int ch = 0; ch < 2; ++ch) {
            lowPass1[ch].prepare(spec);
            lowPass1[ch].setType(juce::dsp::LinkwitzRileyFilterType::lowpass);
            lowPass1[ch].setCutoffFrequency(250.0f);

            highPass1[ch].prepare(spec);
            highPass1[ch].setType(juce::dsp::LinkwitzRileyFilterType::highpass);
            highPass1[ch].setCutoffFrequency(250.0f);

            lowPass2[ch].prepare(spec);
            lowPass2[ch].setType(juce::dsp::LinkwitzRileyFilterType::lowpass);
            lowPass2[ch].setCutoffFrequency(4000.0f);

            highPass2[ch].prepare(spec);
            highPass2[ch].setType(juce::dsp::LinkwitzRileyFilterType::highpass);
            highPass2[ch].setCutoffFrequency(4000.0f);

            // Allpass phase alignment for low band around 4000 Hz
            lowAllPass[ch].prepare(spec);
            lowAllPass[ch].setType(juce::dsp::LinkwitzRileyFilterType::allpass);
            lowAllPass[ch].setCutoffFrequency(4000.0f);
        }

        // Roll loop buffer (up to 2 seconds stereo)
        int bufLen = static_cast<int>(sr * 2.0) + 16;
        for (int ch = 0; ch < 2; ++ch) {
            rollBuffer[ch].assign(static_cast<size_t>(bufLen), 0.0f);
        }
        rollWritePos = 0;
        rollReadPos = 0;
        rollActive = false;

        lowGainSmoothed.reset(sr, 0.015);
        midGainSmoothed.reset(sr, 0.015);
        highGainSmoothed.reset(sr, 0.015);
        gateEnv = 1.0f;
    }

    // Converts normalized Pioneer knob [0..1] (0.5 = 0 dB, 0.0 = -inf kill, 1.0 = +6 dB)
    static float knobToIsolatorGain(float norm) noexcept {
        if (norm <= 0.005f) return 0.0f;
        if (norm <= 0.5f) {
            float t = norm * 2.0f;
            return t * t; // smooth musical kill curve
        }
        float boostNorm = (norm - 0.5f) * 2.0f;
        return juce::Decibels::decibelsToGain(boostNorm * 6.0f);
    }

    void process(juce::AudioBuffer<float>& buffer,
                 float lowKnob,
                 float midKnob,
                 float highKnob,
                 IsolatorFxMode fxMode,
                 double bpm,
                 double beatPhase) noexcept
    {
        const int numChannels = std::min(buffer.getNumChannels(), 2);
        const int numSamples = buffer.getNumSamples();
        if (numChannels == 0 || numSamples == 0) return;

        lowGainSmoothed.setTargetValue(knobToIsolatorGain(lowKnob));
        midGainSmoothed.setTargetValue(knobToIsolatorGain(midKnob));
        highGainSmoothed.setTargetValue(knobToIsolatorGain(highKnob));

        const double samplesPerBeat = (60.0 / std::max(40.0, bpm)) * sr;
        const int rollBufSize = static_cast<int>(rollBuffer[0].size());

        // Determine if any band is pushed away from 12 o'clock to drive Isolator FX depth
        const float lowDev = std::abs(lowKnob - 0.5f) * 2.0f;
        const float midDev = std::abs(midKnob - 0.5f) * 2.0f;
        const float highDev = std::abs(highKnob - 0.5f) * 2.0f;
        const float maxDev = std::max({ lowDev, midDev, highDev });

        // Beat-synced roll length for TRANS/ROLL when knob > 0.5
        const bool isRolling = (fxMode == IsolatorFxMode::TransRoll && maxDev > 0.12f &&
                                (lowKnob > 0.56f || midKnob > 0.56f || highKnob > 0.56f));
        int rollDiv = 4; // 1/4 beat (16th)
        if (maxDev < 0.35f) rollDiv = 1;       // 1 beat
        else if (maxDev < 0.65f) rollDiv = 2;  // 1/2 beat
        else if (maxDev < 0.85f) rollDiv = 4;  // 1/4 beat
        else rollDiv = 8;                      // 1/8 beat

        const int rollLen = std::clamp(static_cast<int>(samplesPerBeat / static_cast<double>(rollDiv)), 64, rollBufSize - 8);

        if (isRolling && !rollActive) {
            rollActive = true;
            rollStartPos = (rollWritePos - rollLen + rollBufSize) % rollBufSize;
            rollReadPos = 0;
        } else if (!isRolling) {
            rollActive = false;
        }

        double localPhase = beatPhase;
        const double phaseInc = (bpm / 60.0) / sr;

        for (int i = 0; i < numSamples; ++i) {
            const float gLow = lowGainSmoothed.getNextValue();
            const float gMid = midGainSmoothed.getNextValue();
            const float gHigh = highGainSmoothed.getNextValue();

            // Rhythmic modulation envelopes
            double step4 = std::fmod(localPhase * 4.0, 1.0); // 16th note pulse
            double step2 = std::fmod(localPhase * 2.0, 1.0); // 8th note pulse

            float transGate = (step4 < 0.55) ? 1.0f : 0.05f;
            float cutAddPulse = (step2 < 0.50) ? 1.0f : 0.0f;

            // Smooth gate envelope to prevent clicks
            float targetGate = 1.0f;
            if (fxMode == IsolatorFxMode::TransRoll && !isRolling && maxDev > 0.08f) {
                targetGate = 1.0f - maxDev * (1.0f - transGate);
            } else if (fxMode == IsolatorFxMode::CutAdd && maxDev > 0.08f) {
                targetGate = 1.0f - (maxDev * 0.85f) * (1.0f - cutAddPulse);
            }
            gateEnv += (targetGate - gateEnv) * 0.08f;

            for (int ch = 0; ch < numChannels; ++ch) {
                float x = buffer.getSample(ch, i);

                // Always feed circular roll buffer
                rollBuffer[ch][static_cast<size_t>(rollWritePos)] = x;

                // 3-band Linkwitz-Riley split
                float low = lowAllPass[ch].processSample(0, lowPass1[ch].processSample(0, x));
                float midHigh = highPass1[ch].processSample(0, x);
                float mid = lowPass2[ch].processSample(0, midHigh);
                float high = highPass2[ch].processSample(0, midHigh);

                float lOut = low * gLow;
                float mOut = mid * gMid;
                float hOut = high * gHigh;

                if (fxMode == IsolatorFxMode::CutAdd) {
                    if (lowDev > 0.08f)  lOut *= (lowKnob < 0.5f ? gateEnv : (1.0f + 0.5f * lowDev * cutAddPulse));
                    if (midDev > 0.08f)  mOut *= (midKnob < 0.5f ? gateEnv : (1.0f + 0.5f * midDev * cutAddPulse));
                    if (highDev > 0.08f) hOut *= (highKnob < 0.5f ? gateEnv : (1.0f + 0.5f * highDev * cutAddPulse));
                } else if (fxMode == IsolatorFxMode::TransRoll) {
                    if (rollActive) {
                        int idx = (rollStartPos + rollReadPos) % rollBufSize;
                        float rolled = rollBuffer[ch][static_cast<size_t>(idx)];
                        float mix = std::clamp(maxDev * 1.1f, 0.0f, 1.0f);
                        lOut = lOut * (1.0f - mix) + rolled * mix * 0.35f;
                        mOut = mOut * (1.0f - mix) + rolled * mix * 0.45f;
                        hOut = hOut * (1.0f - mix) + rolled * mix * 0.20f;
                    } else {
                        if (lowDev > 0.08f)  lOut *= gateEnv;
                        if (midDev > 0.08f)  mOut *= gateEnv;
                        if (highDev > 0.08f) hOut *= gateEnv;
                    }
                } else if (fxMode == IsolatorFxMode::GateDrive) {
                    auto processGateDrive = [&](float s, float knob) noexcept -> float {
                        if (knob > 0.54f) {
                            // Warm Pioneer analog overdrive
                            float drive = 1.0f + (knob - 0.5f) * 14.0f;
                            return std::tanh(s * drive) / std::tanh(drive * 0.65f + 0.35f);
                        } else if (knob < 0.46f) {
                            // Dynamic noise gate
                            float thresh = (0.5f - knob) * 0.45f;
                            float a = std::abs(s);
                            float g = (a > thresh) ? 1.0f : (a / (thresh + 1e-5f));
                            return s * g * g;
                        }
                        return s;
                    };
                    lOut = processGateDrive(lOut, lowKnob);
                    mOut = processGateDrive(mOut, midKnob);
                    hOut = processGateDrive(hOut, highKnob);
                }

                buffer.setSample(ch, i, lOut + mOut + hOut);
            }

            rollWritePos = (rollWritePos + 1) % rollBufSize;
            if (rollActive) {
                rollReadPos = (rollReadPos + 1) % rollLen;
            }
            localPhase += phaseInc;
            if (localPhase >= 1.0) localPhase -= 1.0;
        }
    }

private:
    double sr = 44100.0;
    std::array<juce::dsp::LinkwitzRileyFilter<float>, 2> lowPass1, highPass1, lowPass2, highPass2, lowAllPass;
    std::array<std::vector<float>, 2> rollBuffer;
    int rollWritePos = 0;
    int rollStartPos = 0;
    int rollReadPos = 0;
    bool rollActive = false;

    juce::SmoothedValue<float> lowGainSmoothed, midGainSmoothed, highGainSmoothed;
    float gateEnv = 1.0f;
};

} // namespace rmx
