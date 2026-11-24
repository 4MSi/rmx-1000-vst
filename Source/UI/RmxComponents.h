#pragma once

#include "RmxLookAndFeel.h"
#include <functional>

namespace rmx {

//==============================================================================
// Capacitive Pioneer RMX-1000 X-Pad Touchstrip (16 glowing LED segments)
//==============================================================================
class XPadTouchStrip : public juce::Component {
public:
    std::function<void(bool active, float normX)> onTouchChanged;

    void setVisualState(bool active, float normX) {
        isTouching = active;
        posNorm = std::clamp(normX, 0.0f, 1.0f);
        repaint();
    }

    void paint(juce::Graphics& g) override {
        auto b = getLocalBounds().toFloat();

        // Outer metallic frame
        g.setColour(juce::Colour(0xFF0A0C10));
        g.fillRoundedRectangle(b, 6.0f);
        g.setColour(Palette::panelBorder);
        g.drawRoundedRectangle(b.reduced(0.5f), 6.0f, 1.2f);

        // Capacitive glass ribbon area
        auto strip = b.reduced(8.0f, 14.0f).translated(0.0f, -4.0f);
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xFF141821), strip.getTopLeft(),
                                               juce::Colour(0xFF0C0E14), strip.getBottomLeft(), false));
        g.fillRoundedRectangle(strip, 4.0f);

        // 16 LED segments
        const int numSegs = 16;
        const float gap = 4.0f;
        const float segW = (strip.getWidth() - gap * (numSegs + 1)) / static_cast<float>(numSegs);
        const float segH = strip.getHeight() * 0.56f;
        const float segY = strip.getCentreY() - segH * 0.5f;
        const int activeIdx = static_cast<int>(std::round(posNorm * static_cast<float>(numSegs - 1)));

        for (int i = 0; i < numSegs; ++i) {
            float sx = strip.getX() + gap + static_cast<float>(i) * (segW + gap);
            auto segRect = juce::Rectangle<float>(sx, segY, segW, segH);

            bool lit = isTouching && (i <= activeIdx);
            bool head = isTouching && (i == activeIdx);

            if (head) {
                g.setColour(Palette::pioneerBlue.withAlpha(0.35f));
                g.fillRoundedRectangle(segRect.expanded(2.5f), 3.0f);
                g.setColour(juce::Colours::white);
                g.fillRoundedRectangle(segRect, 2.0f);
            } else if (lit) {
                g.setColour(Palette::pioneerBlue);
                g.fillRoundedRectangle(segRect, 2.0f);
            } else {
                g.setColour(Palette::pioneerBlue.withAlpha(0.14f));
                g.fillRoundedRectangle(segRect, 2.0f);
            }
        }

        // Division markings beneath touchstrip
        static const char* kDivLabels[6] = { "1/1", "1/2", "1/4", "1/8", "1/16", "1/32" };
        g.setFont(juce::FontOptions(9.5f, juce::Font::bold));
        g.setColour(Palette::textDim);
        float zoneW = strip.getWidth() / 6.0f;
        for (int z = 0; z < 6; ++z) {
            g.drawFittedText(kDivLabels[z],
                             static_cast<int>(strip.getX() + z * zoneW),
                             static_cast<int>(b.getBottom() - 14.0f),
                             static_cast<int>(zoneW), 12,
                             juce::Justification::centred, 1);
        }
    }

    void mouseDown(const juce::MouseEvent& e) override { updateFromMouse(e, true); }
    void mouseDrag(const juce::MouseEvent& e) override { updateFromMouse(e, true); }
    void mouseUp(const juce::MouseEvent& e) override   { updateFromMouse(e, false); }

private:
    void updateFromMouse(const juce::MouseEvent& e, bool active) {
        float x = std::clamp(static_cast<float>(e.x - 8) / std::max(1.0f, static_cast<float>(getWidth() - 16)),
                             0.0f, 1.0f);
        isTouching = active;
        posNorm = x;
        if (onTouchChanged) onTouchChanged(isTouching, posNorm);
        repaint();
    }

    bool isTouching = false;
    float posNorm = 0.5f;
};

//==============================================================================
// Pioneer RMX-1000 Mechanical Spring-Loaded Release FX Lever
//==============================================================================
class ReleaseLeverComponent : public juce::Component {
public:
    std::function<void(bool engaged)> onLeverChanged;

    void setEngaged(bool state) {
        if (engaged != state) {
            engaged = state;
            repaint();
        }
    }

    bool isEngaged() const noexcept { return engaged; }

    void paint(juce::Graphics& g) override {
        auto b = getLocalBounds().toFloat().reduced(6.0f);

        // Recessed mechanical slot
        auto slot = juce::Rectangle<float>(b.getCentreX() - 14.0f, b.getY() + 12.0f, 28.0f, b.getHeight() - 24.0f);
        g.setColour(juce::Colour(0xFF07080B));
        g.fillRoundedRectangle(slot, 10.0f);
        g.setColour(engaged ? Palette::pioneerRed : Palette::panelBorder);
        g.drawRoundedRectangle(slot, 10.0f, engaged ? 2.0f : 1.2f);

        // Glowing indicator inside slot when pulled
        if (engaged) {
            g.setColour(Palette::pioneerRed.withAlpha(0.28f));
            g.fillRoundedRectangle(slot.reduced(3.0f), 7.0f);
        }

        // Metallic lever handle (top = resting, bottom = engaged!)
        float handleY = engaged ? (slot.getBottom() - 26.0f) : (slot.getY() + 6.0f);
        auto handle = juce::Rectangle<float>(b.getCentreX() - 28.0f, handleY, 56.0f, 22.0f);

        // Shadow
        g.setColour(juce::Colours::black.withAlpha(0.65f));
        g.fillRoundedRectangle(handle.translated(2.0f, 4.0f), 5.0f);

        // Brushed chrome/steel lever cap
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xFF5D6475), handle.getTopLeft(),
                                               juce::Colour(0xFF1F232C), handle.getBottomLeft(), false));
        g.fillRoundedRectangle(handle, 5.0f);
        g.setColour(juce::Colours::white.withAlpha(0.25f));
        g.drawRoundedRectangle(handle, 5.0f, 1.0f);

        // Center grip groove + LED line
        auto groove = handle.withHeight(4.0f).withCentre(handle.getCentre()).reduced(8.0f, 0.0f);
        g.setColour(engaged ? Palette::pioneerRed.brighter(0.25f) : Palette::textDim.withAlpha(0.6f));
        g.fillRoundedRectangle(groove, 2.0f);

        // Label
        g.setFont(juce::FontOptions(9.5f, juce::Font::bold));
        g.setColour(engaged ? Palette::pioneerRed.brighter(0.4f) : Palette::textDim);
        g.drawFittedText(engaged ? "RELEASE ON" : "PULL / HOLD",
                         getLocalBounds().removeFromBottom(14),
                         juce::Justification::centred, 1);
    }

    void mouseDown(const juce::MouseEvent&) override {
        engaged = true;
        if (onLeverChanged) onLeverChanged(true);
        repaint();
    }

    void mouseUp(const juce::MouseEvent&) override {
        engaged = false;
        if (onLeverChanged) onLeverChanged(false);
        repaint();
    }

private:
    bool engaged = false;
};

//==============================================================================
// 12-Segment Stereo Hardware LED Level Meter
//==============================================================================
class StereoLedMeter : public juce::Component {
public:
    void setLevels(float leftLinear, float rightLinear) {
        levelL = std::clamp(leftLinear, 0.0f, 1.2f);
        levelR = std::clamp(rightLinear, 0.0f, 1.2f);
        repaint();
    }

    void paint(juce::Graphics& g) override {
        auto b = getLocalBounds().toFloat().reduced(2.0f);
        g.setColour(juce::Colour(0xFF090A0D));
        g.fillRoundedRectangle(b, 3.0f);

        const int numSegs = 12;
        const float chGap = 3.0f;
        const float barW = (b.getWidth() - chGap - 6.0f) * 0.5f;
        const float segGap = 2.0f;
        const float segH = (b.getHeight() - 6.0f - segGap * (numSegs - 1)) / static_cast<float>(numSegs);

        auto drawBar = [&](float x, float level) {
            float db = juce::Decibels::gainToDecibels(level, -48.0f);
            float norm = std::clamp((db + 48.0f) / 51.0f, 0.0f, 1.0f);
            int litCount = static_cast<int>(std::round(norm * static_cast<float>(numSegs)));

            for (int i = 0; i < numSegs; ++i) {
                float y = b.getBottom() - 3.0f - static_cast<float>(i + 1) * segH - static_cast<float>(i) * segGap;
                juce::Colour col = (i >= 10) ? Palette::pioneerRed
                                             : (i >= 7 ? Palette::pioneerAmber : Palette::pioneerGreen);
                g.setColour(i < litCount ? col : col.withAlpha(0.14f));
                g.fillRoundedRectangle(x, y, barW, segH, 1.0f);
            }
        };

        drawBar(b.getX() + 3.0f, levelL);
        drawBar(b.getX() + 3.0f + barW + chGap, levelR);
    }

private:
    float levelL = 0.0f;
    float levelR = 0.0f;
};

//==============================================================================
// Digital BPM LED Readout Display
//==============================================================================
class BpmDisplayComponent : public juce::Component {
public:
    void update(double bpm, bool synced, double beatPhase) {
        currentBpm = bpm;
        isSynced = synced;
        beatFlash = (beatPhase < 0.18);
        repaint();
    }

    void paint(juce::Graphics& g) override {
        auto b = getLocalBounds().toFloat();
        g.setColour(juce::Colour(0xFF07090D));
        g.fillRoundedRectangle(b, 5.0f);
        g.setColour(Palette::panelBorder);
        g.drawRoundedRectangle(b.reduced(0.5f), 5.0f, 1.0f);

        // Beat LED dot
        auto dot = juce::Rectangle<float>(b.getX() + 8.0f, b.getCentreY() - 4.0f, 8.0f, 8.0f);
        g.setColour(beatFlash ? Palette::pioneerBlue : Palette::pioneerBlue.withAlpha(0.18f));
        g.fillEllipse(dot);

        // BPM value
        g.setFont(juce::FontOptions(20.0f, juce::Font::bold));
        g.setColour(Palette::pioneerBlue.brighter(0.35f));
        g.drawText(juce::String(currentBpm, 1),
                   b.reduced(18.0f, 2.0f).withTrimmedRight(30.0f),
                   juce::Justification::centredRight);

        // BPM / SYNC label
        g.setFont(juce::FontOptions(8.5f, juce::Font::bold));
        g.setColour(isSynced ? Palette::pioneerGreen : Palette::pioneerAmber);
        g.drawText(isSynced ? "SYNC" : "AUTO",
                   b.removeFromRight(36.0f).reduced(2.0f, 5.0f),
                   juce::Justification::centred);
    }

private:
    double currentBpm = 128.0;
    bool isSynced = false;
    bool beatFlash = false;
};

} // namespace rmx
