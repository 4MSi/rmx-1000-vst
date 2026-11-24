#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "UI/RmxLookAndFeel.h"
#include "UI/RmxComponents.h"
#include <array>
#include <memory>

class RMX1000AudioProcessorEditor : public juce::AudioProcessorEditor,
                                   private juce::Timer
{
public:
    explicit RMX1000AudioProcessorEditor(RMX1000AudioProcessor&);
    ~RMX1000AudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress& key) override;
    bool keyStateChanged(bool isKeyDown) override;

private:
    void timerCallback() override;
    void updateButtonStatesFromParams();
    void setIntParam(const juce::String& paramId, int value);
    void setFloatParam(const juce::String& paramId, float value);
    void toggleIsoKill(const juce::String& paramId);
    void configureKnob(juce::Slider& slider, juce::Label& label, const juce::String& text,
                       bool isGiantScene = false, bool isIso = false,
                       juce::Colour accent = rmx::Palette::pioneerBlue);
    void configureLedButton(juce::TextButton& btn, const juce::String& text, juce::Colour ledCol);

    RMX1000AudioProcessor& audioProcessor;
    rmx::RmxLookAndFeel lookAndFeel;

    // Top bar controls
    juce::Slider inputGainKnob, outputGainKnob;
    juce::Label inputGainLabel, outputGainLabel;
    rmx::BpmDisplayComponent bpmDisplay;
    juce::TextButton tapBtn, autoBpmBtn, quantizeBtn, bypassBtn;

    // Isolator FX
    juce::Slider isoHighKnob, isoMidKnob, isoLowKnob;
    juce::Label isoHighLabel, isoMidLabel, isoLowLabel;
    std::array<juce::TextButton, 3> isoModeBtns; // Cut/Add, Trans/Roll, Gate/Drive

    // Scene FX
    juce::Slider sceneMainKnob, sceneSub1Knob, sceneSub2Knob;
    juce::Label sceneMainLabel, sceneSub1Label, sceneSub2Label;
    std::array<juce::TextButton, 10> sceneFxBtns; // 0..4 Build Up, 5..9 Break Down

    // X-Pad FX
    std::array<juce::TextButton, 4> drumBtns; // Kick, Snare, Clap, Hi-Hat
    juce::TextButton overdubBtn, clearOverdubBtn;
    juce::Slider xpadPitchKnob, xpadLevelKnob;
    juce::Label xpadPitchLabel, xpadLevelLabel;
    rmx::XPadTouchStrip xpadStrip;

    // Release FX + Meters
    rmx::StereoLedMeter inMeter, outMeter;
    std::array<juce::TextButton, 3> releaseModeBtns; // Vinyl Brake, Echo, Back Spin
    juce::Slider releaseTimeKnob;
    juce::Label releaseTimeLabel;
    rmx::ReleaseLeverComponent releaseLever;

    // APVTS Attachments
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::unique_ptr<SliderAttachment> inputGainAtt, outputGainAtt;
    std::unique_ptr<SliderAttachment> isoHighAtt, isoMidAtt, isoLowAtt;
    std::unique_ptr<SliderAttachment> sceneMainAtt, sceneSub1Att, sceneSub2Att;
    std::unique_ptr<SliderAttachment> xpadPitchAtt, xpadLevelAtt;
    std::unique_ptr<SliderAttachment> releaseTimeAtt;
    std::unique_ptr<ButtonAttachment> autoBpmAtt, quantizeAtt, bypassAtt, overdubAtt;

    // Live Keyboard Performance State
    bool spaceLeverDown = false;
    int heldRollKey = 0; // '5', '6', '7', '8'

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RMX1000AudioProcessorEditor)
};
