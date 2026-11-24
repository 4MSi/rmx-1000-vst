#include "PluginEditor.h"

RMX1000AudioProcessorEditor::RMX1000AudioProcessorEditor(RMX1000AudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    setLookAndFeel(&lookAndFeel);
    setWantsKeyboardFocus(true);

    // Top bar
    configureKnob(inputGainKnob, inputGainLabel, "INPUT", false, false, rmx::Palette::pioneerGreen);
    configureKnob(outputGainKnob, outputGainLabel, "OUTPUT", false, false, rmx::Palette::pioneerAmber);
    addAndMakeVisible(bpmDisplay);

    configureLedButton(tapBtn, "TAP", rmx::Palette::pioneerBlue);
    tapBtn.setClickingTogglesState(false);
    tapBtn.onClick = [this] { audioProcessor.triggerTapTempo(); };

    configureLedButton(autoBpmBtn, "AUTO BPM", rmx::Palette::pioneerAmber);
    configureLedButton(quantizeBtn, "QUANTIZE", rmx::Palette::pioneerBlue);
    configureLedButton(bypassBtn, "BYPASS", rmx::Palette::pioneerRed);

    // Isolator FX
    configureKnob(isoHighKnob, isoHighLabel, "HI", false, true, rmx::Palette::pioneerAmber);
    configureKnob(isoMidKnob,  isoMidLabel,  "MID", false, true, rmx::Palette::pioneerAmber);
    configureKnob(isoLowKnob,  isoLowLabel,  "LOW", false, true, rmx::Palette::pioneerAmber);

    static const char* kIsoNames[3] = { "CUT / ADD", "TRANS / ROLL", "GATE / DRIVE" };
    for (int i = 0; i < 3; ++i) {
        configureLedButton(isoModeBtns[static_cast<size_t>(i)], kIsoNames[i], rmx::Palette::pioneerAmber);
        isoModeBtns[static_cast<size_t>(i)].setClickingTogglesState(false);
        isoModeBtns[static_cast<size_t>(i)].onClick = [this, i] {
            int cur = static_cast<int>(audioProcessor.apvts.getRawParameterValue("isoMode")->load());
            setIntParam("isoMode", cur == (i + 1) ? 0 : (i + 1));
            updateButtonStatesFromParams();
        };
    }

    // Scene FX
    configureKnob(sceneMainKnob, sceneMainLabel, "SCENE FX", true, false, rmx::Palette::pioneerRed);
    configureKnob(sceneSub1Knob, sceneSub1Label, "SUB PARAM 1", false, false, rmx::Palette::pioneerBlue);
    configureKnob(sceneSub2Knob, sceneSub2Label, "SUB PARAM 2", false, false, rmx::Palette::pioneerBlue);

    static const char* kSceneNames[10] = {
        "BPF ECHO", "ECHO", "NOISE", "SPIRAL UP", "REVERB UP",
        "HPF ECHO", "LPF ECHO", "CRUSH ECHO", "SPIRAL DN", "REVERB DN"
    };
    for (int i = 0; i < 10; ++i) {
        juce::Colour col = (i < 5) ? rmx::Palette::pioneerRed : rmx::Palette::pioneerBlue;
        configureLedButton(sceneFxBtns[static_cast<size_t>(i)], kSceneNames[i], col);
        sceneFxBtns[static_cast<size_t>(i)].setClickingTogglesState(false);
        sceneFxBtns[static_cast<size_t>(i)].onClick = [this, i] {
            int cur = static_cast<int>(audioProcessor.apvts.getRawParameterValue("sceneType")->load());
            setIntParam("sceneType", cur == (i + 1) ? 0 : (i + 1));
            updateButtonStatesFromParams();
        };
    }

    // X-Pad FX
    static const char* kDrumNames[4] = { "KICK", "SNARE", "CLAP", "HI-HAT" };
    for (int i = 0; i < 4; ++i) {
        configureLedButton(drumBtns[static_cast<size_t>(i)], kDrumNames[i], rmx::Palette::pioneerBlue);
        drumBtns[static_cast<size_t>(i)].setClickingTogglesState(false);
        drumBtns[static_cast<size_t>(i)].onClick = [this, i] {
            setIntParam("xpadDrum", i);
            audioProcessor.triggerDrumPad(static_cast<rmx::DrumVoiceType>(i));
            updateButtonStatesFromParams();
        };
    }

    configureLedButton(overdubBtn, "OVERDUB", rmx::Palette::pioneerRed);
    configureLedButton(clearOverdubBtn, "CLEAR", rmx::Palette::pioneerAmber);
    clearOverdubBtn.setClickingTogglesState(false);
    clearOverdubBtn.onClick = [this] { audioProcessor.clearOverdubPattern(); };

    configureKnob(xpadPitchKnob, xpadPitchLabel, "PITCH", false, true, rmx::Palette::pioneerBlue);
    configureKnob(xpadLevelKnob, xpadLevelLabel, "LEVEL", false, false, rmx::Palette::pioneerBlue);

    addAndMakeVisible(xpadStrip);
    xpadStrip.onTouchChanged = [this](bool active, float normX) {
        audioProcessor.setXPadTouch(active, normX);
    };

    // Release FX & Meters
    addAndMakeVisible(inMeter);
    addAndMakeVisible(outMeter);

    static const char* kReleaseNames[3] = { "VINYL BRAKE", "ECHO", "BACK SPIN" };
    for (int i = 0; i < 3; ++i) {
        configureLedButton(releaseModeBtns[static_cast<size_t>(i)], kReleaseNames[i], rmx::Palette::pioneerRed);
        releaseModeBtns[static_cast<size_t>(i)].setClickingTogglesState(false);
        releaseModeBtns[static_cast<size_t>(i)].onClick = [this, i] {
            setIntParam("releaseMode", i);
            updateButtonStatesFromParams();
        };
    }

    configureKnob(releaseTimeKnob, releaseTimeLabel, "TIME / DECAY", false, false, rmx::Palette::pioneerRed);

    addAndMakeVisible(releaseLever);
    releaseLever.onLeverChanged = [this](bool engaged) {
        audioProcessor.setReleaseLever(engaged);
    };

    // Bind APVTS Attachments
    auto& vts = audioProcessor.apvts;
    inputGainAtt   = std::make_unique<SliderAttachment>(vts, "inputGain",   inputGainKnob);
    outputGainAtt  = std::make_unique<SliderAttachment>(vts, "outputGain",  outputGainKnob);
    isoHighAtt     = std::make_unique<SliderAttachment>(vts, "isoHigh",     isoHighKnob);
    isoMidAtt      = std::make_unique<SliderAttachment>(vts, "isoMid",      isoMidKnob);
    isoLowAtt      = std::make_unique<SliderAttachment>(vts, "isoLow",      isoLowKnob);
    sceneMainAtt   = std::make_unique<SliderAttachment>(vts, "sceneKnob",   sceneMainKnob);
    sceneSub1Att   = std::make_unique<SliderAttachment>(vts, "sceneSub1",   sceneSub1Knob);
    sceneSub2Att   = std::make_unique<SliderAttachment>(vts, "sceneSub2",   sceneSub2Knob);
    xpadPitchAtt   = std::make_unique<SliderAttachment>(vts, "xpadPitch",   xpadPitchKnob);
    xpadLevelAtt   = std::make_unique<SliderAttachment>(vts, "xpadLevel",   xpadLevelKnob);
    releaseTimeAtt = std::make_unique<SliderAttachment>(vts, "releaseTime", releaseTimeKnob);

    autoBpmAtt  = std::make_unique<ButtonAttachment>(vts, "autoBpm",     autoBpmBtn);
    quantizeAtt = std::make_unique<ButtonAttachment>(vts, "quantize",    quantizeBtn);
    bypassAtt   = std::make_unique<ButtonAttachment>(vts, "bypassAll",   bypassBtn);
    overdubAtt  = std::make_unique<ButtonAttachment>(vts, "xpadOverdub", overdubBtn);

    // Double-click resets to neutral
    isoHighKnob.setDoubleClickReturnValue(true, 0.5);
    isoMidKnob.setDoubleClickReturnValue(true, 0.5);
    isoLowKnob.setDoubleClickReturnValue(true, 0.5);
    sceneMainKnob.setDoubleClickReturnValue(true, 0.0);
    xpadPitchKnob.setDoubleClickReturnValue(true, 0.5);

    updateButtonStatesFromParams();
    setSize(1120, 620);
    startTimerHz(30);
}

RMX1000AudioProcessorEditor::~RMX1000AudioProcessorEditor() {
    stopTimer();
    setLookAndFeel(nullptr);
}

void RMX1000AudioProcessorEditor::configureKnob(juce::Slider& slider, juce::Label& label,
                                                const juce::String& text, bool isGiantScene,
                                                bool isIso, juce::Colour accent)
{
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    slider.getProperties().set("giantScene", isGiantScene);
    slider.getProperties().set("isoKnob", isIso);
    slider.getProperties().set("accentColour", static_cast<int>(accent.getARGB()));
    addAndMakeVisible(slider);

    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.setFont(juce::FontOptions(10.5f, juce::Font::bold));
    label.setColour(juce::Label::textColourId, rmx::Palette::textDim);
    addAndMakeVisible(label);
}

void RMX1000AudioProcessorEditor::configureLedButton(juce::TextButton& btn,
                                                     const juce::String& text,
                                                     juce::Colour ledCol)
{
    btn.setButtonText(text);
    btn.setClickingTogglesState(true);
    btn.getProperties().set("ledColour", static_cast<int>(ledCol.getARGB()));
    addAndMakeVisible(btn);
}

void RMX1000AudioProcessorEditor::setIntParam(const juce::String& paramId, int value) {
    if (auto* param = audioProcessor.apvts.getParameter(paramId)) {
        param->setValueNotifyingHost(param->convertTo0to1(static_cast<float>(value)));
    }
}

void RMX1000AudioProcessorEditor::updateButtonStatesFromParams() {
    const int isoMode = static_cast<int>(audioProcessor.apvts.getRawParameterValue("isoMode")->load());
    for (int i = 0; i < 3; ++i)
        isoModeBtns[static_cast<size_t>(i)].setToggleState(isoMode == (i + 1), juce::dontSendNotification);

    const int sceneType = static_cast<int>(audioProcessor.apvts.getRawParameterValue("sceneType")->load());
    for (int i = 0; i < 10; ++i)
        sceneFxBtns[static_cast<size_t>(i)].setToggleState(sceneType == (i + 1), juce::dontSendNotification);

    // Dynamic Scene FX Halo Color: Red for Build Up (1..5), Blue for Break Down (6..10)
    juce::Colour sceneAccent = (sceneType >= 1 && sceneType <= 5)
                                   ? rmx::Palette::pioneerRed
                                   : rmx::Palette::pioneerBlue;
    sceneMainKnob.getProperties().set("accentColour", static_cast<int>(sceneAccent.getARGB()));
    sceneMainKnob.repaint();

    const int drumIdx = static_cast<int>(audioProcessor.apvts.getRawParameterValue("xpadDrum")->load());
    for (int i = 0; i < 4; ++i)
        drumBtns[static_cast<size_t>(i)].setToggleState(drumIdx == i, juce::dontSendNotification);

    const int relMode = static_cast<int>(audioProcessor.apvts.getRawParameterValue("releaseMode")->load());
    for (int i = 0; i < 3; ++i)
        releaseModeBtns[static_cast<size_t>(i)].setToggleState(relMode == i, juce::dontSendNotification);
}

void RMX1000AudioProcessorEditor::timerCallback() {
    bpmDisplay.update(audioProcessor.getCurrentBpm(),
                      audioProcessor.isHostBpmSynced(),
                      audioProcessor.getBeatPhase());
    inMeter.setLevels(audioProcessor.getInputLevelL(), audioProcessor.getInputLevelR());
    outMeter.setLevels(audioProcessor.getOutputLevelL(), audioProcessor.getOutputLevelR());
    updateButtonStatesFromParams();
}

bool RMX1000AudioProcessorEditor::keyPressed(const juce::KeyPress& key) {
    if (key.getKeyCode() == juce::KeyPress::spaceKey) {
        if (!spaceLeverDown) {
            spaceLeverDown = true;
            releaseLever.setEngaged(true);
            audioProcessor.setReleaseLever(true);
        }
        return true;
    }
    if (key.getTextCharacter() == 't' || key.getTextCharacter() == 'T') {
        audioProcessor.triggerTapTempo();
        return true;
    }
    if (key.getTextCharacter() == '1') { audioProcessor.triggerDrumPad(rmx::DrumVoiceType::Kick); return true; }
    if (key.getTextCharacter() == '2') { audioProcessor.triggerDrumPad(rmx::DrumVoiceType::Snare); return true; }
    if (key.getTextCharacter() == '3') { audioProcessor.triggerDrumPad(rmx::DrumVoiceType::Clap); return true; }
    if (key.getTextCharacter() == '4') { audioProcessor.triggerDrumPad(rmx::DrumVoiceType::HiHat); return true; }
    return false;
}

bool RMX1000AudioProcessorEditor::keyStateChanged(bool /*isKeyDown*/) {
    if (spaceLeverDown && !juce::KeyPress::isKeyCurrentlyDown(juce::KeyPress::spaceKey)) {
        spaceLeverDown = false;
        releaseLever.setEngaged(false);
        audioProcessor.setReleaseLever(false);
        return true;
    }
    return false;
}

void RMX1000AudioProcessorEditor::paint(juce::Graphics& g) {
    // Brushed dark aluminum RMX-1000 chassis
    g.setGradientFill(juce::ColourGradient(rmx::Palette::chassisTop, 0.0f, 0.0f,
                                           rmx::Palette::chassisBottom, 0.0f, static_cast<float>(getHeight()), false));
    g.fillAll();

    // Subtle horizontal brushed-metal texture lines
    g.setColour(juce::Colours::white.withAlpha(0.015f));
    for (int y = 0; y < getHeight(); y += 3)
        g.drawHorizontalLine(y, 0.0f, static_cast<float>(getWidth()));

    // Top Header Bar
    g.setColour(juce::Colour(0xFF0E1015));
    g.fillRect(0, 0, getWidth(), 64);
    g.setColour(rmx::Palette::panelBorder);
    g.drawHorizontalLine(64, 0.0f, static_cast<float>(getWidth()));

    // Pioneer RMX-1000 Branding
    g.setFont(juce::FontOptions(22.0f, juce::Font::bold));
    g.setColour(rmx::Palette::textMain);
    g.drawText("RMX-1000", 20, 10, 160, 26, juce::Justification::centredLeft);

    g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    g.setColour(rmx::Palette::pioneerBlue);
    g.drawText("REMIX STATION  //  LIVE DSP EDITION", 22, 36, 260, 16, juce::Justification::centredLeft);

    // Helper to draw recessed hardware panels with silkscreen titles
    auto drawPanel = [&](juce::Rectangle<int> area, const juce::String& title, juce::Colour accent) {
        auto f = area.toFloat();
        g.setColour(rmx::Palette::panelBg);
        g.fillRoundedRectangle(f, 8.0f);
        g.setColour(rmx::Palette::panelBorder);
        g.drawRoundedRectangle(f, 8.0f, 1.3f);

        // Title pill line
        g.setColour(accent);
        g.fillRect(f.getX() + 12.0f, f.getY() + 10.0f, 4.0f, 13.0f);
        g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
        g.setColour(rmx::Palette::textMain);
        g.drawText(title, area.getX() + 22, area.getY() + 7, area.getWidth() - 32, 20, juce::Justification::centredLeft);
    };

    // 4 Signature RMX-1000 Sections
    drawPanel({ 14,  76, 246, 530 }, "ISOLATOR FX", rmx::Palette::pioneerAmber);
    drawPanel({ 272, 76, 576, 324 }, "SCENE FX",    rmx::Palette::pioneerRed);
    drawPanel({ 272, 410, 576, 196 }, "X-PAD FX",   rmx::Palette::pioneerBlue);
    drawPanel({ 860, 76, 246, 530 }, "RELEASE FX",  rmx::Palette::pioneerRed);

    // Sub-headers inside Scene FX (BUILD UP / BREAK DOWN)
    g.setFont(juce::FontOptions(10.5f, juce::Font::bold));
    g.setColour(rmx::Palette::pioneerRed);
    g.drawText("BUILD UP", 286, 102, 132, 16, juce::Justification::centred);

    g.setColour(rmx::Palette::pioneerBlue);
    g.drawText("BREAK DOWN", 702, 102, 132, 16, juce::Justification::centred);

    // Meter labels
    g.setFont(juce::FontOptions(9.5f, juce::Font::bold));
    g.setColour(rmx::Palette::textDim);
    g.drawText("IN",  914, 106, 44, 14, juce::Justification::centred);
    g.drawText("OUT", 1008, 106, 44, 14, juce::Justification::centred);

    // Corner hex screws for authentic rack/hardware look
    auto drawScrew = [&](float cx, float cy) {
        g.setColour(juce::Colour(0xFF0A0B0E));
        g.fillEllipse(cx - 5.0f, cy - 5.0f, 10.0f, 10.0f);
        g.setColour(juce::Colour(0xFF3A3F4B));
        g.drawEllipse(cx - 5.0f, cy - 5.0f, 10.0f, 10.0f, 1.0f);
        g.drawLine(cx - 3.0f, cy, cx + 3.0f, cy, 1.0f);
    };
    drawScrew(10.0f, 72.0f);
    drawScrew(static_cast<float>(getWidth() - 10), 72.0f);
    drawScrew(10.0f, static_cast<float>(getHeight() - 10));
    drawScrew(static_cast<float>(getWidth() - 10), static_cast<float>(getHeight() - 10));
}

void RMX1000AudioProcessorEditor::resized() {
    // Top bar layout
    inputGainKnob.setBounds(300, 6, 44, 44);
    inputGainLabel.setBounds(294, 48, 56, 14);

    outputGainKnob.setBounds(362, 6, 44, 44);
    outputGainLabel.setBounds(356, 48, 56, 14);

    bpmDisplay.setBounds(440, 12, 146, 40);
    tapBtn.setBounds(596, 14, 68, 36);
    autoBpmBtn.setBounds(672, 14, 88, 36);
    quantizeBtn.setBounds(768, 14, 88, 36);
    bypassBtn.setBounds(1010, 14, 90, 36);

    // 1. ISOLATOR FX (Left Panel: x=14..260, y=76..606)
    isoHighKnob.setBounds(88, 114, 98, 98);
    isoHighLabel.setBounds(88, 212, 98, 16);

    isoMidKnob.setBounds(88, 234, 98, 98);
    isoMidLabel.setBounds(88, 332, 98, 16);

    isoLowKnob.setBounds(88, 354, 98, 98);
    isoLowLabel.setBounds(88, 452, 98, 16);

    for (int i = 0; i < 3; ++i) {
        isoModeBtns[static_cast<size_t>(i)].setBounds(34, 478 + i * 40, 206, 34);
    }

    // 2. SCENE FX (Center Top Panel: x=272..848, y=76..400)
    for (int i = 0; i < 5; ++i) {
        sceneFxBtns[static_cast<size_t>(i)].setBounds(288, 122 + i * 44, 130, 36);       // BUILD UP (Red)
        sceneFxBtns[static_cast<size_t>(i + 5)].setBounds(702, 122 + i * 44, 130, 36);   // BREAK DOWN (Blue)
    }

    sceneMainKnob.setBounds(472, 108, 176, 176);
    sceneMainLabel.setBounds(472, 282, 176, 16);

    sceneSub1Knob.setBounds(456, 306, 72, 72);
    sceneSub1Label.setBounds(444, 378, 96, 15);

    sceneSub2Knob.setBounds(592, 306, 72, 72);
    sceneSub2Label.setBounds(580, 378, 96, 15);

    // 3. X-PAD FX (Center Bottom Panel: x=272..848, y=410..606)
    for (int i = 0; i < 4; ++i) {
        drumBtns[static_cast<size_t>(i)].setBounds(288 + i * 74, 444, 68, 44);
    }
    overdubBtn.setBounds(590, 444, 72, 22);
    clearOverdubBtn.setBounds(590, 468, 72, 20);

    xpadPitchKnob.setBounds(678, 434, 60, 60);
    xpadPitchLabel.setBounds(674, 492, 68, 14);

    xpadLevelKnob.setBounds(758, 434, 60, 60);
    xpadLevelLabel.setBounds(754, 492, 68, 14);

    xpadStrip.setBounds(288, 512, 544, 80);

    // 4. RELEASE FX & METERS (Right Panel: x=860..1106, y=76..606)
    inMeter.setBounds(918, 122, 36, 118);
    outMeter.setBounds(1012, 122, 36, 118);

    for (int i = 0; i < 3; ++i) {
        releaseModeBtns[static_cast<size_t>(i)].setBounds(880, 256 + i * 38, 206, 32);
    }

    releaseTimeKnob.setBounds(951, 378, 64, 64);
    releaseTimeLabel.setBounds(934, 442, 98, 15);

    releaseLever.setBounds(918, 464, 130, 128);
}
