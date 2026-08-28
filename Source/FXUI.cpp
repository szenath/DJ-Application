/*
  ==============================================================================
    FXUI.cpp
  ==============================================================================
*/

#include "FXUI.h"

// ─────────────────────────────────────────────────────────────────────────────
// FXPopupWindow  –  shown in a CallOutBox; hosts the grid and the XY pad
//
// C1 note: FXPopupWindow is declared and implemented here in the .cpp rather
// than in a header because it is an internal implementation detail of
// FXButtonComponent only. It is never referenced outside this translation unit,
// so exposing it in a header would leak private implementation detail.
// ─────────────────────────────────────────────────────────────────────────────
class FXPopupWindow : public juce::Component
{
public:
    /** Constructs the popup: sets initial size, adds grid and close button,
        and wires the grid's onSelection callback to show the XY pad. */
    FXPopupWindow()
    {
        setSize(580, 400);
        addAndMakeVisible(grid);
        addAndMakeVisible(closeBtn);
        closeBtn.setButtonText("X");
        closeBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff5a1a1a));

        closeBtn.onClick = [this]
            {
                if (auto* cb = findParentComponentOfClass<juce::CallOutBox>())
                    cb->dismiss();
            };

        grid.onSelection = [this](juce::String fx) { showXYPad(fx); };
    }

    /** Draws the dark background, border, "FX" title, and breadcrumb trail
        showing the currently selected effect name (if an XY pad is active). */
    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff181818));
        g.setColour(juce::Colours::grey.withAlpha(0.4f));
        g.drawRect(getLocalBounds(), 1);

        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(18.0f, juce::Font::bold));
        g.drawText("FX", 16, 8, 60, 28, juce::Justification::centredLeft);

        // Breadcrumb
        if (currentPadName.isNotEmpty())
        {
            g.setColour(juce::Colours::grey);
            g.setFont(12.0f);
            g.drawText("< Select FX  /  " + currentPadName, 50, 12, 300, 20,
                juce::Justification::centredLeft);
        }
    }

    /** Positions the close button and fills remaining space with either the
        effect grid (no selection yet) or the active XY pad. */
    void resized() override
    {
        closeBtn.setBounds(getWidth() - 36, 6, 28, 28);

        if (xyPad != nullptr)
            xyPad->setBounds(12, 44, getWidth() - 24, getHeight() - 56);
        else
            grid.setBounds(12, 44, getWidth() - 24, getHeight() - 56);
    }

    /** Fires when the XY pad is moved or locked: forwards effect name + X/Y coords. */
    std::function<void(juce::String, float, float)> onFXParameterChange;

    /** Fires when the XY pad is released without locking: deactivate the effect. */
    std::function<void()>                            onFXOff;

private:
    void showXYPad(const juce::String& fxName)
    {
        currentPadName = fxName;
        xyPad.reset(new XYPadComponent(fxName));
        addAndMakeVisible(xyPad.get());
        grid.setVisible(false);

        xyPad->onParameterChange = [this, fxName](float x, float y)
            {
                if (onFXParameterChange) onFXParameterChange(fxName, x, y);
            };

        xyPad->onFXReleased = [this]()
            {
                if (onFXOff) onFXOff();
            };

        resized();
        repaint();
    }

    FXGridComponent              grid;
    juce::TextButton             closeBtn;
    std::unique_ptr<XYPadComponent> xyPad;
    juce::String                 currentPadName;
};

// ─────────────────────────────────────────────────────────────────────────────
// FXButtonComponent
// ─────────────────────────────────────────────────────────────────────────────
FXButtonComponent::FXButtonComponent()
{
    addAndMakeVisible(button);
    button.setButtonText("FX");
    button.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2a2a2a));
    button.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    button.onClick = [this] { showFXPopup(); };
}

void FXButtonComponent::resized() { button.setBounds(getLocalBounds()); }

void FXButtonComponent::showFXPopup()
{
    auto* popup = new FXPopupWindow();

    popup->onFXParameterChange = [this](juce::String fx, float x, float y)
        {
            if (onFXChange) onFXChange(fx, x, y);
        };

    popup->onFXOff = [this]()
        {
            if (onFXOff) onFXOff();
        };

    juce::CallOutBox::launchAsynchronously(
        std::unique_ptr<juce::Component>(popup),
        button.getScreenBounds(),
        nullptr);
}