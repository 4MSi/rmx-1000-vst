#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>

namespace rmx {

class BpmEngine {
public:
    void prepare(double sampleRate) noexcept {
        sr = sampleRate > 0.0 ? sampleRate : 44100.0;
        beatPhase = 0.0;
        barPhase = 0.0;
        onsetEnv = 0.0f;
        prevEnergy = 0.0f;
        samplesSinceLastBeat = 0;
        tapIndex = 0;
        tapCount = 0;
    }

    void updateFromPlayHead(juce::AudioPlayHead* playHead) noexcept {
        if (playHead != nullptr) {
            if (auto pos = playHead->getPosition()) {
                if (auto hostBpm = pos->getBpm()) {
                    if (*hostBpm >= 40.0 && *hostBpm <= 240.0) {
                        currentBpm.store(*hostBpm, std::memory_order_relaxed);
                        hostSynced.store(true, std::memory_order_relaxed);
                        if (auto ppq = pos->getPpqPosition()) {
                            double p = *ppq;
                            beatPhase = p - std::floor(p);
                            barPhase = (p * 0.25) - std::floor(p * 0.25);
                        }
                        return;
                    }
                }
            }
        }
        hostSynced.store(false, std::memory_order_relaxed);
    }

    void setManualBpm(double bpm) noexcept {
        double clamped = std::clamp(bpm, 60.0, 200.0);
        currentBpm.store(clamped, std::memory_order_relaxed);
    }

    void tap() noexcept {
        double nowSec = juce::Time::getMillisecondCounterHiRes() * 0.001;
        if (lastTapSec > 0.0) {
            double dt = nowSec - lastTapSec;
            if (dt > 0.25 && dt < 1.5) {
                tapIntervals[tapIndex] = dt;
                tapIndex = (tapIndex + 1) % static_cast<int>(tapIntervals.size());
                tapCount = std::min(tapCount + 1, static_cast<int>(tapIntervals.size()));

                double sum = 0.0;
                for (int i = 0; i < tapCount; ++i)
                    sum += tapIntervals[i];
                double avgDt = sum / static_cast<double>(tapCount);
                double bpm = std::clamp(60.0 / avgDt, 60.0, 200.0);
                currentBpm.store(bpm, std::memory_order_relaxed);
            } else {
                tapCount = 0;
                tapIndex = 0;
            }
        }
        lastTapSec = nowSec;
        beatPhase = 0.0;
    }

    void advanceBlock(const juce::AudioBuffer<float>& buffer, bool autoBpmEnabled) noexcept {
        const int numSamples = buffer.getNumSamples();
        const double bpm = currentBpm.load(std::memory_order_relaxed);
        const double beatsPerSample = (bpm / 60.0) / sr;

        if (!hostSynced.load(std::memory_order_relaxed)) {
            beatPhase += beatsPerSample * static_cast<double>(numSamples);
            beatPhase -= std::floor(beatPhase);

            barPhase += (beatsPerSample * 0.25) * static_cast<double>(numSamples);
            barPhase -= std::floor(barPhase);

            // Lightweight live onset detector for Standalone Auto-BPM
            if (autoBpmEnabled && buffer.getNumChannels() > 0 && numSamples > 0) {
                float energy = 0.0f;
                const float* ch0 = buffer.getReadPointer(0);
                for (int i = 0; i < numSamples; ++i)
                    energy += ch0[i] * ch0[i];
                energy = std::sqrt(energy / static_cast<float>(numSamples));

                float flux = energy - prevEnergy;
                prevEnergy = prevEnergy * 0.85f + energy * 0.15f;
                samplesSinceLastBeat += numSamples;

                const int minBeatSamples = static_cast<int>(sr * 0.30); // <= 200 BPM
                const int maxBeatSamples = static_cast<int>(sr * 1.00); // >= 60 BPM

                if (flux > 0.08f && energy > 0.12f && samplesSinceLastBeat > minBeatSamples) {
                    if (samplesSinceLastBeat < maxBeatSamples) {
                        double detectedBpm = 60.0 / (static_cast<double>(samplesSinceLastBeat) / sr);
                        while (detectedBpm < 85.0) detectedBpm *= 2.0;
                        while (detectedBpm > 170.0) detectedBpm *= 0.5;
                        double smoothed = bpm * 0.92 + detectedBpm * 0.08;
                        currentBpm.store(std::clamp(smoothed, 60.0, 200.0), std::memory_order_relaxed);
                    }
                    samplesSinceLastBeat = 0;
                }
            }
        }
    }

    double getBpm() const noexcept { return currentBpm.load(std::memory_order_relaxed); }
    bool isHostSynced() const noexcept { return hostSynced.load(std::memory_order_relaxed); }
    double getBeatPhase() const noexcept { return beatPhase; }
    double getBarPhase() const noexcept { return barPhase; }
    double getSamplesPerBeat() const noexcept { return (60.0 / getBpm()) * sr; }
    double getSampleRate() const noexcept { return sr; }

private:
    double sr = 44100.0;
    std::atomic<double> currentBpm { 128.0 };
    std::atomic<bool> hostSynced { false };
    double beatPhase = 0.0;
    double barPhase = 0.0;

    float onsetEnv = 0.0f;
    float prevEnergy = 0.0f;
    int samplesSinceLastBeat = 0;

    double lastTapSec = 0.0;
    std::array<double, 4> tapIntervals { 0.46875, 0.46875, 0.46875, 0.46875 };
    int tapIndex = 0;
    int tapCount = 0;
};

} // namespace rmx
