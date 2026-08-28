/*
  ==============================================================================
    FXUI.h
    FX grid + XY pad.

    Behaviour
    ─────────
    • Drag the pad → effect activates immediately (live preview).
    • Release without LOCK → effect deactivates (onFXReleased fires → player
      receives setFXType(None)).
    • Click LOCK IN while dragging or after placing cursor → effect stays on.
    • Click LOCK IN again (now shows LOCKED) → effect deactivates.
    • Each effect shows its own X / Y axis description so the user knows what
      they are controlling.
  ==============================================================================
*/

#pragma once
#include "../JuceLibraryCode/JuceHeader.h"

// ─────────────────────────────────────────────────────────────────────────────
// XYPadComponent
// C1 note: method bodies are defined here in the header rather than in FXUI.cpp
// because XYPadComponent is an internal helper used only by FXPopupWindow (which
// is itself defined in FXUI.cpp). Keeping it header-only avoids a circular
// dependency and is an accepted JUCE idiom for lightweight private helpers.
// ─────────────────────────────────────────────────────────────────────────────
class XYPadComponent : public juce::Component
{
public:
    /** Constructs the pad for the named effect.
        Sets up axis labels appropriate to that effect's X/Y parameter mapping.
        @param fxName  Display name of the effect (e.g. "Echo", "Flanger"). */
    explicit XYPadComponent(const juce::String& fxName) : name(fxName)
    {
        setupAxisLabels();
    }

    /** Fires continuously while the user drags the pad, and once when lock turns on.
        Receives normalised X (0–1) and Y (0–1) pad coordinates. */
    std::function<void(float x, float y)> onParameterChange;

    /** Fires when the effect should be fully deactivated (pad released without lock). */
    std::function<void()> onFXReleased;

    // ─── paint ───────────────────────────────────────────────────────────────
    void paint(juce::Graphics& g) override
    {
        const int w = getWidth();
        const int h = getHeight();
        const auto bounds = getLocalBounds().toFloat();

        // Background
        g.fillAll(juce::Colour(0xff1c1c1c));

        // Grid lines
        g.setColour(juce::Colours::white.withAlpha(0.06f));
        for (int i = 1; i < 4; ++i)
        {
            float x = bounds.getWidth() * i / 4.0f;
            float y = bounds.getHeight() * i / 4.0f;
            g.drawLine(x, 0, x, bounds.getHeight(), 0.5f);
            g.drawLine(0, y, bounds.getWidth(), y, 0.5f);
        }

        // Centre cross
        g.setColour(juce::Colours::white.withAlpha(0.18f));
        g.drawLine(bounds.getCentreX(), 0, bounds.getCentreX(), bounds.getHeight(), 1.0f);
        g.drawLine(0, bounds.getCentreY(), bounds.getWidth(), bounds.getCentreY(), 1.0f);

        // Outer border — glows when active
        g.setColour(isEffectActive
            ? juce::Colour(0xff4ade80).withAlpha(0.7f)
            : juce::Colours::grey.withAlpha(0.3f));
        g.drawRect(bounds, isEffectActive ? 2.0f : 1.0f);

        // FX name (top-centre, bold)
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(15.0f, juce::Font::bold));
        g.drawText(name, 0, 6, w, 22, juce::Justification::centred);

        // Axis labels (bottom left / right)
        g.setColour(juce::Colours::lightgrey.withAlpha(0.65f));
        g.setFont(10.0f);
        g.drawText(xLabel, 6, h - 16, w / 2 - 8, 14, juce::Justification::centredLeft);
        g.drawText(yLabel, w / 2, h - 16, w / 2 - 6, 14, juce::Justification::centredRight);

        // LOCK button
        lockBtnArea = juce::Rectangle<float>((float)w - 86.0f, 6.0f, 80.0f, 26.0f);
        juce::Colour lockBg = isLocked ? juce::Colour(0xff4ade80) : juce::Colour(0xff333333);
        g.setColour(lockBg);
        g.fillRoundedRectangle(lockBtnArea, 5.0f);
        g.setColour(isLocked ? juce::Colour(0xff1a6e3a) : juce::Colours::grey.withAlpha(0.6f));
        g.drawRoundedRectangle(lockBtnArea, 5.0f, 1.0f);
        g.setColour(isLocked ? juce::Colours::black : juce::Colours::white);
        g.setFont(juce::Font(11.0f, juce::Font::bold));
        g.drawText(isLocked ? "LOCKED" : "LOCK IN", lockBtnArea, juce::Justification::centred);

        // Cursor — only when effect is active
        if (isEffectActive)
        {
            float cx = xPos * (float)w;
            float cy = (1.0f - yPos) * (float)h;
            float r = 13.0f;

            // Soft glow ring
            g.setColour(juce::Colour(0xffb09b72).withAlpha(0.25f));
            g.fillEllipse(cx - r * 1.8f, cy - r * 1.8f, r * 3.6f, r * 3.6f);

            // Solid cursor
            g.setColour(juce::Colour(0xffb09b72));
            g.fillEllipse(cx - r, cy - r, r * 2.0f, r * 2.0f);
            g.setColour(juce::Colours::white);
            g.drawEllipse(cx - r, cy - r, r * 2.0f, r * 2.0f, 1.5f);

            // Crosshair lines
            g.setColour(juce::Colours::white.withAlpha(0.3f));
            g.drawLine(cx, 0, cx, (float)h, 0.5f);
            g.drawLine(0, cy, (float)w, cy, 0.5f);

            // Value readout
            g.setColour(juce::Colours::white.withAlpha(0.75f));
            g.setFont(9.0f);
            juce::String val = "X:" + juce::String(xPos, 2) + "  Y:" + juce::String(yPos, 2);
            g.drawText(val, 6, h - 30, 90, 12, juce::Justification::centredLeft);
        }
        else
        {
            // Idle hint
            g.setColour(juce::Colours::grey.withAlpha(0.4f));
            g.setFont(12.0f);
            g.drawText("Tap & drag to activate", bounds, juce::Justification::centred);
        }
    }

    // ─── mouse ───────────────────────────────────────────────────────────────
    void mouseDown(const juce::MouseEvent& e) override
    {
        if (lockBtnArea.contains(e.position))
        {
            toggleLock();
            return;
        }
        isDragging = true;
        isEffectActive = true;
        updatePosition(e.position);
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (!isDragging) return;
        updatePosition(e.position);
    }

    void mouseUp(const juce::MouseEvent&) override
    {
        isDragging = false;
        if (!isLocked)
        {
            xPos = 0.5f;
            yPos = 0.5f;
            isEffectActive = false;
            if (onFXReleased) onFXReleased();
        }
        repaint();
    }

private:
    void toggleLock()
    {
        isLocked = !isLocked;
        if (!isLocked && !isDragging)
        {
            // Lock turned off while not dragging → deactivate
            xPos = 0.5f;
            yPos = 0.5f;
            isEffectActive = false;
            if (onFXReleased) onFXReleased();
        }
        else if (isLocked && isEffectActive)
        {
            if (onParameterChange) onParameterChange(xPos, yPos);
        }
        repaint();
    }

    void updatePosition(juce::Point<float> p)
    {
        xPos = juce::jlimit(0.0f, 1.0f, p.x / (float)getWidth());
        yPos = juce::jlimit(0.0f, 1.0f, 1.0f - p.y / (float)getHeight());
        if (onParameterChange) onParameterChange(xPos, yPos);
        repaint();
    }

    void setupAxisLabels()
    {
        if (name == "Echo") { xLabel = "X: Delay time";    yLabel = "Y: Feedback"; }
        else if (name == "Slicer") { xLabel = "X: Gate speed";    yLabel = "Y: Duty cycle"; }
        else if (name == "Flanger") { xLabel = "X: LFO rate";      yLabel = "Y: Depth / Mix"; }
        else if (name == "Reverb") { xLabel = "X: Room size";     yLabel = "Y: Wet level"; }
        else if (name == "Crusher") { xLabel = "X: Bit depth";     yLabel = "Y: Sample hold"; }
        else if (name == "Brake") { xLabel = "X: activate";      yLabel = ""; }
        else { xLabel = "X";                yLabel = "Y"; }
    }

    juce::String name, xLabel, yLabel;
    bool  isLocked{ false };
    bool  isDragging{ false };
    bool  isEffectActive{ false };
    float xPos{ 0.5f };
    float yPos{ 0.5f };
    juce::Rectangle<float> lockBtnArea;
};

// ─────────────────────────────────────────────────────────────────────────────
// FXGridComponent  –  3-row × 2-col button grid
// C1 note: implemented inline in this header for the same reason as XYPadComponent
// (internal helper used only within FXUI.cpp).
// ─────────────────────────────────────────────────────────────────────────────
class FXGridComponent : public juce::Component
{
public:
    /** Constructs the grid and adds all six effect buttons (Echo, Slicer,
        Flanger, Reverb, Crusher, Brake). */
    FXGridComponent()
    {
        for (const auto& name : { "Echo","Slicer","Flanger","Reverb","Crusher","Brake" })
            addFXButton(name);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(10);
        int  bw = area.getWidth() / 2;
        int  bh = area.getHeight() / 3;
        int  col = 0, row = 0;
        for (auto* b : buttons)
        {
            b->setBounds(area.getX() + col * bw + 4,
                area.getY() + row * bh + 4,
                bw - 8, bh - 8);
            if (++col > 1) { col = 0; ++row; }
        }
    }

    /** Fires when the user selects an effect from the grid.
        Receives the effect name string (e.g. "Echo"). */
    std::function<void(juce::String)> onSelection;

private:
    void addFXButton(const juce::String& name)
    {
        auto* b = buttons.add(new juce::TextButton(name));
        addAndMakeVisible(b);
        b->setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2a2a2a));
        b->setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        b->onClick = [this, name]
            {
                for (auto* btn : buttons)
                    btn->setColour(juce::TextButton::buttonColourId,
                        btn->getButtonText() == name ? juce::Colour(0xff4ade80)
                        : juce::Colour(0xff2a2a2a));
                if (onSelection) onSelection(name);
            };
    }
    juce::OwnedArray<juce::TextButton> buttons;
};

// ─────────────────────────────────────────────────────────────────────────────
// FXButtonComponent  –  deck-level button that opens the popup
// ─────────────────────────────────────────────────────────────────────────────
class FXButtonComponent : public juce::Component
{
public:
    /** Constructs the FX button and wires its click handler to open the popup. */
    FXButtonComponent();

    /** Lays out the inner TextButton to fill the component bounds. */
    void resized() override;

    /** Called while dragging / on lock-on: set FX type + update parameters. */
    std::function<void(juce::String fxName, float x, float y)> onFXChange;

    /** Called when the pad is released without locking: deactivate FX. */
    std::function<void()> onFXOff;

private:
    void showFXPopup();
    juce::TextButton button;
};