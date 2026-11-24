#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

RMX1000AudioProcessor::RMX1000AudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "RMX1000_STATE", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout RMX1000AudioProcessor::createParameterLayout() {
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Input / Output Level
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("inputGain", 1), "Input Gain",
        juce::NormalisableRange<float>(-18.0f, 12.0f, 0.1f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("outputGain", 1), "Output Gain",
        juce::NormalisableRange<float>(-18.0f, 12.0f, 0.1f), 0.0f));

    // Isolator FX
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("isoLow", 1), "Isolator Low", 0.0f, 1.0f, 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("isoMid", 1), "Isolator Mid", 0.0f, 1.0f, 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("isoHigh", 1), "Isolator High", 0.0f, 1.0f, 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID("isoMode", 1), "Isolator FX Mode", 0, 3, 0));

    // Scene FX (0 = Off, 1..5 = Build Up, 6..10 = Break Down)
    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID("sceneType", 1), "Scene FX Type", 0, 10, 1));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("sceneKnob", 1), "Scene FX Knob", 0.0f, 1.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("sceneSub1", 1), "Sub Parameter 1", 0.0f, 1.0f, 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("sceneSub2", 1), "Sub Parameter 2", 0.0f, 1.0f, 0.5f));

    // X-Pad FX
    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID("xpadDrum", 1), "X-Pad Drum", 0, 3, 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("xpadPitch", 1), "X-Pad Pitch", 0.0f, 1.0f, 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("xpadLevel", 1), "X-Pad Level", 0.0f, 1.2f, 0.85f));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID("xpadOverdub", 1), "X-Pad Overdub", false));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID("quantize", 1), "Quantize", true));

    // Release FX
    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID("releaseMode", 1), "Release FX Mode", 0, 2, 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("releaseTime", 1), "Release FX Time", 0.0f, 1.0f, 0.5f));

    // Global / BPM
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID("autoBpm", 1), "Auto BPM", true));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID("bypassAll", 1), "Bypass", false));

    return { params.begin(), params.end() };
}

void RMX1000AudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    bpmEngine.prepare(sampleRate);
    isolatorFx.prepare(sampleRate, samplesPerBlock);
    sceneFx.prepare(sampleRate, samplesPerBlock);
    xpadFx.prepare(sampleRate);
    releaseFx.prepare(sampleRate, samplesPerBlock);
}

void RMX1000AudioProcessor::releaseResources() {
}

bool RMX1000AudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
    return true;
}

void RMX1000AudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) {
    juce::ScopedNoDenormals noDenormals;

    const int totalNumInputChannels  = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    for (int i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, numSamples);

    // Handle MIDI CC & Drum Triggers
    const float xpadPitch = apvts.getRawParameterValue("xpadPitch")->load();
    for (const auto metadata : midiMessages) {
        const auto msg = metadata.getMessage();
        if (msg.isNoteOn()) {
            const int note = msg.getNoteNumber();
            if (note == 36) xpadFx.triggerDrum(rmx::DrumVoiceType::Kick, xpadPitch);
            else if (note == 38) xpadFx.triggerDrum(rmx::DrumVoiceType::Snare, xpadPitch);
            else if (note == 39) xpadFx.triggerDrum(rmx::DrumVoiceType::Clap, xpadPitch);
            else if (note == 42) xpadFx.triggerDrum(rmx::DrumVoiceType::HiHat, xpadPitch);
            else if (note == 48) releaseLeverEngaged.store(true, std::memory_order_relaxed);
        } else if (msg.isNoteOff()) {
            if (msg.getNoteNumber() == 48)
                releaseLeverEngaged.store(false, std::memory_order_relaxed);
        } else if (msg.isController()) {
            const int cc = msg.getControllerNumber();
            const float norm = static_cast<float>(msg.getControllerValue()) / 127.0f;
            if (cc == 1 || cc == 19) {
                if (auto* p = apvts.getParameter("sceneKnob")) p->setValueNotifyingHost(norm);
            } else if (cc == 16) {
                if (auto* p = apvts.getParameter("isoLow")) p->setValueNotifyingHost(norm);
            } else if (cc == 17) {
                if (auto* p = apvts.getParameter("isoMid")) p->setValueNotifyingHost(norm);
            } else if (cc == 18) {
                if (auto* p = apvts.getParameter("isoHigh")) p->setValueNotifyingHost(norm);
            } else if (cc == 22) {
                xpadTouchPos.store(norm, std::memory_order_relaxed);
            }
        }
    }

    // Consume UI drum pad clicks
    unsigned int drumMask = pendingDrumMask.exchange(0u, std::memory_order_relaxed);
    if (drumMask != 0u) {
        for (unsigned d = 0; d < 4; ++d) {
            if ((drumMask & (1u << d)) != 0u)
                xpadFx.triggerDrum(static_cast<rmx::DrumVoiceType>(d), xpadPitch);
        }
    }
    if (clearOverdubFlag.exchange(false, std::memory_order_relaxed)) {
        xpadFx.clearPattern();
    }

    // Update BPM from Host or Standalone Audio
    bpmEngine.updateFromPlayHead(getPlayHead());
    const bool autoBpm = apvts.getRawParameterValue("autoBpm")->load() > 0.5f;
    bpmEngine.advanceBlock(buffer, autoBpm);

    // Input Gain
    const float inGain = juce::Decibels::decibelsToGain(apvts.getRawParameterValue("inputGain")->load());
    buffer.applyGain(inGain);

    // Input Metering
    if (buffer.getNumChannels() >= 1)
        inLevelL.store(buffer.getMagnitude(0, 0, numSamples), std::memory_order_relaxed);
    if (buffer.getNumChannels() >= 2)
        inLevelR.store(buffer.getMagnitude(1, 0, numSamples), std::memory_order_relaxed);
    else
        inLevelR.store(inLevelL.load(std::memory_order_relaxed), std::memory_order_relaxed);

    const bool bypassAll = apvts.getRawParameterValue("bypassAll")->load() > 0.5f;

    if (!bypassAll) {
        const double bpm = bpmEngine.getBpm();
        const double beatPhase = bpmEngine.getBeatPhase();
        const double barPhase = bpmEngine.getBarPhase();

        // 1. ISOLATOR FX
        isolatorFx.process(
            buffer,
            apvts.getRawParameterValue("isoLow")->load(),
            apvts.getRawParameterValue("isoMid")->load(),
            apvts.getRawParameterValue("isoHigh")->load(),
            static_cast<rmx::IsolatorFxMode>(static_cast<int>(apvts.getRawParameterValue("isoMode")->load())),
            bpm,
            beatPhase
        );

        // 2. SCENE FX
        sceneFx.process(
            buffer,
            static_cast<rmx::SceneFxType>(static_cast<int>(apvts.getRawParameterValue("sceneType")->load())),
            apvts.getRawParameterValue("sceneKnob")->load(),
            apvts.getRawParameterValue("sceneSub1")->load(),
            apvts.getRawParameterValue("sceneSub2")->load(),
            bpm
        );

        // 3. X-PAD FX (Drums + Touchstrip Roll + Overdub)
        xpadFx.process(
            buffer,
            static_cast<rmx::DrumVoiceType>(static_cast<int>(apvts.getRawParameterValue("xpadDrum")->load())),
            xpadTouchActive.load(std::memory_order_relaxed),
            xpadTouchPos.load(std::memory_order_relaxed),
            xpadPitch,
            apvts.getRawParameterValue("xpadOverdub")->load() > 0.5f,
            apvts.getRawParameterValue("quantize")->load() > 0.5f,
            apvts.getRawParameterValue("xpadLevel")->load(),
            bpm,
            barPhase
        );

        // 4. RELEASE FX (Vinyl Brake / Echo / Back Spin)
        releaseFx.process(
            buffer,
            static_cast<rmx::ReleaseFxMode>(static_cast<int>(apvts.getRawParameterValue("releaseMode")->load())),
            releaseLeverEngaged.load(std::memory_order_relaxed),
            bpm,
            apvts.getRawParameterValue("releaseTime")->load()
        );
    }

    // Output Gain + Club-Safe Soft Limiter
    const float outGain = juce::Decibels::decibelsToGain(apvts.getRawParameterValue("outputGain")->load());
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch) {
        float* data = buffer.getWritePointer(ch);
        for (int i = 0; i < numSamples; ++i) {
            float s = data[i] * outGain;
            // Smooth ceiling above -0.5 dBFS
            if (std::abs(s) > 0.92f)
                s = std::tanh(s);
            data[i] = s;
        }
    }

    // Output Metering
    if (buffer.getNumChannels() >= 1)
        outLevelL.store(buffer.getMagnitude(0, 0, numSamples), std::memory_order_relaxed);
    if (buffer.getNumChannels() >= 2)
        outLevelR.store(buffer.getMagnitude(1, 0, numSamples), std::memory_order_relaxed);
    else
        outLevelR.store(outLevelL.load(std::memory_order_relaxed), std::memory_order_relaxed);
}

juce::AudioProcessorEditor* RMX1000AudioProcessor::createEditor() {
    return new RMX1000AudioProcessorEditor(*this);
}

void RMX1000AudioProcessor::getStateInformation(juce::MemoryBlock& destData) {
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void RMX1000AudioProcessor::setStateInformation(const void* data, int sizeInBytes) {
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new RMX1000AudioProcessor();
}
