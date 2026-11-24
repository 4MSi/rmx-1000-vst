#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "DSP/BpmEngine.h"
#include "DSP/IsolatorFX.h"
#include "DSP/SceneFX.h"
#include "DSP/XPadFX.h"
#include "DSP/ReleaseFX.h"
#include <atomic>

class RMX1000AudioProcessor : public juce::AudioProcessor {
public:
    RMX1000AudioProcessor();
    ~RMX1000AudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Default"; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // UI interactive triggers (real-time safe)
    void triggerTapTempo() noexcept { bpmEngine.tap(); }
    void setXPadTouch(bool active, float normX) noexcept {
        xpadTouchActive.store(active, std::memory_order_relaxed);
        xpadTouchPos.store(std::clamp(normX, 0.0f, 1.0f), std::memory_order_relaxed);
    }
    void triggerDrumPad(rmx::DrumVoiceType drum) noexcept {
        pendingDrumMask.fetch_or(1u << static_cast<unsigned>(drum), std::memory_order_relaxed);
    }
    void clearOverdubPattern() noexcept {
        clearOverdubFlag.store(true, std::memory_order_relaxed);
    }
    void setReleaseLever(bool engaged) noexcept {
        releaseLeverEngaged.store(engaged, std::memory_order_relaxed);
    }

    // Meter & BPM telemetry for UI
    double getCurrentBpm() const noexcept { return bpmEngine.getBpm(); }
    bool isHostBpmSynced() const noexcept { return bpmEngine.isHostSynced(); }
    double getBeatPhase() const noexcept { return bpmEngine.getBeatPhase(); }
    float getInputLevelL() const noexcept { return inLevelL.load(std::memory_order_relaxed); }
    float getInputLevelR() const noexcept { return inLevelR.load(std::memory_order_relaxed); }
    float getOutputLevelL() const noexcept { return outLevelL.load(std::memory_order_relaxed); }
    float getOutputLevelR() const noexcept { return outLevelR.load(std::memory_order_relaxed); }

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    rmx::BpmEngine bpmEngine;
    rmx::IsolatorFX isolatorFx;
    rmx::SceneFX sceneFx;
    rmx::XPadFX xpadFx;
    rmx::ReleaseFX releaseFx;

    std::atomic<bool> xpadTouchActive { false };
    std::atomic<float> xpadTouchPos { 0.5f };
    std::atomic<unsigned int> pendingDrumMask { 0u };
    std::atomic<bool> clearOverdubFlag { false };
    std::atomic<bool> releaseLeverEngaged { false };

    std::atomic<float> inLevelL { 0.0f }, inLevelR { 0.0f };
    std::atomic<float> outLevelL { 0.0f }, outLevelR { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RMX1000AudioProcessor)
};
