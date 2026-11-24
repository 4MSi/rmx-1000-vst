#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>

namespace rmx {

struct Palette {
    static inline const juce::Colour chassisTop    { 0xFF1E2026 };
    static inline const juce::Colour chassisBottom { 0xFF101216 };
    static inline const juce::Colour panelBg       { 0xFF16181D };
    static inline const juce::Colour panelBorder   { 0xFF2D313B };
    static inline const juce::Colour textMain      { 0xFFE6EAF2 };
    static inline const juce::Colour textDim       { 0xFF8B93A4 };
    static inline const juce::Colour pioneerRed    { 0xFFFF2B3B };
    static inline const juce::Colour pioneerBlue   { 0xFF00B8FF };
    static inline const juce::Colour pioneerAmber  { 0xFFFF9E00 };
    static inline const juce::Colour pioneerGreen  { 0xFF23E56B };
};

class RmxLookAndFeel : public juce::LookAndFeel_V4 {
public:
    RmxLookAndFeel() {
        setColour(juce::Label::textColourId, Palette::textMain);
        setColour(juce::Slider::textBoxTextColourId, Palette::textMain);
        setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    }

    void drawRotarySlider(juce::Graphics& g,
                          int x, int y, int width, int height,
                          float sliderPosProportional,
                          float rotaryStartAngle,
                          float rotaryEndAngle,
                          juce::Slider& slider) override
    {
        const auto bounds = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y),
                                                   static_cast<float>(width), static_cast<float>(height)).reduced(4.0f);
        const float radius = std::min(bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto centre = bounds.getCentre();
        const float angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

        const bool isGiantScene = slider.getProperties().getWithDefault("giantScene", false);
        const bool isIsoKnob    = slider.getProperties().getWithDefault("isoKnob", false);
        const juce::Colour accent = juce::Colour(static_cast<juce::uint32>(
            static_cast<int>(slider.getProperties().getWithDefault("accentColour",
                                                                   static_cast<int>(Palette::pioneerBlue.getARGB())))));

        // Outer tick marks
        const int numTicks = isGiantScene ? 25 : 15;
        for (int i = 0; i < numTicks; ++i) {
            float t = static_cast<float>(i) / static_cast<float>(numTicks - 1);
            float a = rotaryStartAngle + t * (rotaryEndAngle - rotaryStartAngle) - juce::MathConstants<float>::halfPi;
            bool isCenter = (isIsoKnob && i == numTicks / 2);
            float r0 = radius * (isCenter ? 0.86f : 0.90f);
            float r1 = radius * 1.0f;
            bool lit = isGiantScene ? (t <= sliderPosProportional + 0.01f && sliderPosProportional > 0.01f) : false;

            g.setColour(lit ? accent : (isCenter ? Palette::textMain : Palette::textDim.withAlpha(0.45f)));
            g.drawLine(centre.x + std::cos(a) * r0, centre.y + std::sin(a) * r0,
                       centre.x + std::cos(a) * r1, centre.y + std::sin(a) * r1,
                       isCenter ? 2.0f : (lit ? 2.2f : 1.1f));
        }

        // Giant Scene FX glowing LED halo ring
        if (isGiantScene) {
            const float haloR = radius * 0.82f;
            juce::Path bgArc;
            bgArc.addCentredArc(centre.x, centre.y, haloR, haloR, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
            g.setColour(juce::Colour(0xFF0A0C10));
            g.strokePath(bgArc, juce::PathStrokeType(6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            if (sliderPosProportional > 0.005f) {
                juce::Path glowArc;
                glowArc.addCentredArc(centre.x, centre.y, haloR, haloR, 0.0f, rotaryStartAngle, angle, true);
                g.setColour(accent.withAlpha(0.28f));
                g.strokePath(glowArc, juce::PathStrokeType(11.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                g.setColour(accent);
                g.strokePath(glowArc, juce::PathStrokeType(4.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }
        }

        // Knob shadow
        const float knobR = radius * (isGiantScene ? 0.72f : 0.76f);
        g.setColour(juce::Colours::black.withAlpha(0.65f));
        g.fillEllipse(centre.x - knobR + 2.0f, centre.y - knobR + 4.0f, knobR * 2.0f, knobR * 2.0f);

        // Outer metallic skirt
        juce::ColourGradient skirtGrad(juce::Colour(0xFF3A3F4B), centre.x - knobR, centre.y - knobR,
                                       juce::Colour(0xFF0E1014), centre.x + knobR, centre.y + knobR, false);
        g.setGradientFill(skirtGrad);
        g.fillEllipse(centre.x - knobR, centre.y - knobR, knobR * 2.0f, knobR * 2.0f);

        // Inner brushed aluminum / rubber cap
        const float capR = knobR * 0.84f;
        juce::ColourGradient capGrad(juce::Colour(0xFF282C34), centre.x, centre.y - capR,
                                     juce::Colour(0xFF14161B), centre.x, centre.y + capR, false);
        g.setGradientFill(capGrad);
        g.fillEllipse(centre.x - capR, centre.y - capR, capR * 2.0f, capR * 2.0f);

        // Subtle rim highlight
        g.setColour(juce::Colours::white.withAlpha(0.12f));
        g.drawEllipse(centre.x - capR, centre.y - capR, capR * 2.0f, capR * 2.0f, 1.0f);

        // Pioneer white pointer line
        const float ptrAngle = angle - juce::MathConstants<float>::halfPi;
        const float p0 = capR * 0.25f;
        const float p1 = capR * 0.94f;
        g.setColour(isGiantScene && sliderPosProportional > 0.01f ? accent.brighter(0.5f) : Palette::textMain);
        g.drawLine(centre.x + std::cos(ptrAngle) * p0, centre.y + std::sin(ptrAngle) * p0,
                   centre.x + std::cos(ptrAngle) * p1, centre.y + std::sin(ptrAngle) * p1,
                   isGiantScene ? 3.2f : 2.4f);
    }

    void drawButtonBackground(juce::Graphics& g,
                              juce::Button& button,
                              const juce::Colour& /*backgroundColour*/,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override
    {
        auto b = button.getLocalBounds().toFloat().reduced(1.5f);
        const bool isOn = button.getToggleState();
        const juce::Colour ledColour = juce::Colour(static_cast<juce::uint32>(
            static_cast<int>(button.getProperties().getWithDefault("ledColour",
                                                                   static_cast<int>(Palette::pioneerBlue.getARGB())))));

        // Recessed bezel
        g.setColour(juce::Colour(0xFF0A0B0E));
        g.fillRoundedRectangle(b, 4.0f);

        auto inner = b.reduced(1.5f);
        juce::Colour topCol = isOn ? ledColour.withMultipliedBrightness(0.32f)
                                   : (shouldDrawButtonAsHighlighted ? juce::Colour(0xFF2B303B) : juce::Colour(0xFF21252E));
        juce::Colour botCol = isOn ? ledColour.withMultipliedBrightness(0.18f) : juce::Colour(0xFF15181E);
        if (shouldDrawButtonAsDown) topCol = topCol.darker(0.2f);

        g.setGradientFill(juce::ColourGradient(topCol, inner.getTopLeft(), botCol, inner.getBottomLeft(), false));
        g.fillRoundedRectangle(inner, 3.0f);

        // Glowing border / top LED strip when active
        if (isOn) {
            g.setColour(ledColour.withAlpha(0.25f));
            g.drawRoundedRectangle(b, 4.0f, 2.5f);
            g.setColour(ledColour);
            g.drawRoundedRectangle(inner, 3.0f, 1.4f);

            // Bright LED bar at top of button
            auto ledBar = inner.withHeight(3.0f).reduced(6.0f, 0.0f).translated(0.0f, 2.0f);
            g.setColour(ledColour.brighter(0.35f));
            g.fillRoundedRectangle(ledBar, 1.5f);
        } else {
            g.setColour(juce::Colour(0xFF343A46));
            g.drawRoundedRectangle(inner, 3.0f, 1.0f);

            // Dim LED bar
            auto ledBar = inner.withHeight(2.5f).reduced(8.0f, 0.0f).translated(0.0f, 2.0f);
            g.setColour(ledColour.withAlpha(0.20f));
            g.fillRoundedRectangle(ledBar, 1.2f);
        }
    }

    void drawButtonText(juce::Graphics& g,
                        juce::TextButton& button,
                        bool /*shouldDrawButtonAsHighlighted*/,
                        bool /*shouldDrawButtonAsDown*/) override
    {
        const bool isOn = button.getToggleState();
        const juce::Colour ledColour = juce::Colour(static_cast<juce::uint32>(
            static_cast<int>(button.getProperties().getWithDefault("ledColour",
                                                                   static_cast<int>(Palette::pioneerBlue.getARGB())))));

        g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        g.setColour(isOn ? ledColour.brighter(0.75f) : Palette::textMain.withAlpha(0.82f));
        g.drawFittedText(button.getButtonText(),
                         button.getLocalBounds().reduced(3, 5).translated(0, 2),
                         juce::Justification::centred, 2);
    }
};

} // namespace rmx
