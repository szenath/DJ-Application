/*
  ==============================================================================
    DJLookAndFeel.h

    C1 note: DJLookAndFeel is implemented entirely in this header (no .cpp) because
    it is a pure customisation of LookAndFeel_V4 with no dependencies that could
    cause circular includes. Splitting it into a .cpp would provide no benefit for
    a single-class file of this size. This is an accepted JUCE pattern for small
    look-and-feel overrides.
  ==============================================================================
*/

#pragma once

#include "../JuceLibraryCode/JuceHeader.h"

class DJLookAndFeel : public juce::LookAndFeel_V4
{
public:
    /** Constructs the look and feel, setting default colours for TextButtons
        (dark grey background, white text) and rotary sliders (orange fill). */
    DJLookAndFeel()
    {
        setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2d2d2d));
        setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        setColour(juce::Slider::rotarySliderFillColourId, juce::Colours::orange);
        setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colours::darkgrey);
    }

    /** Draws the button background and custom vector icons for named icon buttons
        (PLAY_PAUSE, LOOP_TOGGLE, LOOP_UP, LOOP_DOWN, RESTART). All other buttons
        receive a standard filled rectangle. Hover and press states apply
        brightness adjustments.
        @param backgroundColour   Base colour set on the button.
        @param isMouseOverButton  True when the cursor is over the button.
        @param isButtonDown       True when the button is being pressed. */
    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
        const juce::Colour& backgroundColour, bool isMouseOverButton, bool isButtonDown) override
    {
        auto bounds = button.getLocalBounds().toFloat();
        auto baseColour = backgroundColour;
        if (isButtonDown) baseColour = baseColour.darker(0.2f);
        else if (isMouseOverButton) baseColour = baseColour.brighter(0.1f);

        // Default text buttons skip custom background drawing here if they aren't one of our specific icon buttons
        if (button.getName() != "PLAY_PAUSE" && button.getName() != "LOOP_TOGGLE" &&
            button.getName() != "LOOP_UP" && button.getName() != "LOOP_DOWN" && button.getName() != "RESTART")
        {
            g.setColour(baseColour);
            g.fillRect(bounds.getX(), bounds.getY(), bounds.getWidth(), bounds.getHeight());
            g.setColour(juce::Colours::black.withAlpha(0.5f));
            g.drawRect(bounds, 1.0f);
            return;
        }

        // Draw background for icon buttons
        g.setColour(baseColour);
        g.fillRect(bounds.getX(), bounds.getY(), bounds.getWidth(), bounds.getHeight());
        g.setColour(juce::Colours::black.withAlpha(0.5f));
        g.drawRect(bounds, 1.0f);

        if (button.getName() == "PLAY_PAUSE")
        {
            g.setColour(juce::Colours::white);
            if (button.getToggleState()) {
                float w = 5.0f; float h = 16.0f; float gap = 5.0f;
                float cx = bounds.getCentreX(); float cy = bounds.getCentreY();
                g.fillRect(cx - w - gap / 2, cy - h / 2, w, h);
                g.fillRect(cx + gap / 2, cy - h / 2, w, h);
            }
            else {
                juce::Path p; float s = 10.0f;
                float cx = bounds.getCentreX() + 2.0f; float cy = bounds.getCentreY();
                p.addTriangle(cx - s, cy - s, cx - s, cy + s, cx + s, cy);
                g.fillPath(p);
            }
        }
        else if (button.getName() == "LOOP_TOGGLE") {
            g.setColour(button.getToggleState() ? juce::Colours::orange : juce::Colours::white);
            juce::Path p; float cx = bounds.getCentreX(); float cy = bounds.getCentreY(); float r = 8.0f;
            p.addRoundedRectangle(cx - r, cy - r + 2, r * 2, r * 2 - 4, 3.0f);
            juce::PathStrokeType stroke(2.0f); g.strokePath(p, stroke);
            juce::Path arrowHead; arrowHead.addTriangle(cx + r - 3, cy - r - 2, cx + r + 3, cy - r + 2, cx + r - 3, cy - r + 6);
            g.fillPath(arrowHead);
        }
        else if (button.getName() == "LOOP_UP") {
            g.setColour(juce::Colours::grey); juce::Path p;
            float w = 4.0f; float h = 3.0f; float cx = bounds.getCentreX(); float cy = bounds.getCentreY();
            p.addTriangle(cx - w, cy + h, cx, cy - h, cx + w, cy + h); g.fillPath(p);
        }
        else if (button.getName() == "LOOP_DOWN") {
            g.setColour(juce::Colours::grey); juce::Path p;
            float w = 4.0f; float h = 3.0f; float cx = bounds.getCentreX(); float cy = bounds.getCentreY();
            p.addTriangle(cx - w, cy - h, cx, cy + h, cx + w, cy - h); g.fillPath(p);
        }
        else if (button.getName() == "RESTART") {
            g.setColour(juce::Colours::white);
            float cx = bounds.getCentreX();
            float cy = bounds.getCentreY();
            float r = 7.0f;

            // Draw a 3/4 circle line
            juce::Path p;
            p.addArc(cx - r, cy - r, r * 2.0f, r * 2.0f,
                0.0f, juce::MathConstants<float>::pi * 1.5f, true);
            g.strokePath(p, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            // Draw the arrowhead pointing counter-clockwise
            juce::Path arrow;
            arrow.addTriangle(cx + 2.0f, cy - r - 4.5f,
                cx + 2.0f, cy - r + 4.5f,
                cx - 4.5f, cy - r);
            g.fillPath(arrow);
        }
    }

    /** Suppresses text rendering for icon buttons (PLAY_PAUSE, LOOP_TOGGLE,
        LOOP_UP, LOOP_DOWN, RESTART) whose labels are drawn as vector graphics
        in drawButtonBackground. All other buttons render normally. */
    void drawButtonText(juce::Graphics& g, juce::TextButton& button, bool isMouseOverButton, bool isButtonDown) override
    {
        // Ignore drawing text for our custom icon buttons
        if (button.getName() == "PLAY_PAUSE" || button.getName() == "LOOP_TOGGLE" ||
            button.getName() == "LOOP_UP" || button.getName() == "LOOP_DOWN" ||
            button.getName() == "RESTART") return;

        juce::LookAndFeel_V4::drawButtonText(g, button, isMouseOverButton, isButtonDown);
    }

    // =================================================================
    // DRAW ROTARY SLIDER (Custom Color #b09b72)
    // =================================================================
    /** Draws a custom rotary knob: dark circular background with a thin gold
        needle indicator (#b09b72). Used for all EQ and filter knobs.
        @param sliderPos  Normalised slider value (0.0–1.0). */
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
        const float rotaryStartAngle, const float rotaryEndAngle, juce::Slider& slider) override
    {
        auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat();
        auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f;
        auto toAngle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
        auto center = bounds.getCentre();

        // Background Circle (Dark)
        g.setColour(juce::Colour(0xff151515));
        g.fillEllipse(center.getX() - radius, center.getY() - radius, radius * 2, radius * 2);
        g.setColour(juce::Colours::black);
        g.drawEllipse(center.getX() - radius, center.getY() - radius, radius * 2, radius * 2, 1.0f);

        // Indicator Needle Shape
        juce::Path p;
        p.addRectangle(-2.0f, -radius + 4.0f, 4.0f, radius * 0.5f);

        // Set to the exact color from your image (#b09b72)
        g.setColour(juce::Colour(0xffb09b72));

        g.fillPath(p, juce::AffineTransform::rotation(toAngle).translated(center));
    }

    /** Draws custom linear sliders. SpeedSlider receives a pitch-fader style
        (narrow track, wide thumb, centre reference line). Vertical sliders
        receive a segmented LED-style fader. Horizontal sliders receive a
        simple filled rectangle handle.
        @param sliderPos     Current thumb position in component pixels.
        @param style         Slider orientation/style enum. */
    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
        float sliderPos, float minSliderPos, float maxSliderPos,
        const juce::Slider::SliderStyle style, juce::Slider& slider) override
    {
        auto bounds = juce::Rectangle<float>(x, y, width, height);
        if (slider.getComponentID() == "SpeedSlider") {
            float trackWidth = 4.0f;
            juce::Rectangle<float> trackRect(bounds.getCentreX() - trackWidth / 2, bounds.getY(), trackWidth, bounds.getHeight());
            g.setColour(juce::Colour(0xff151515));
            g.fillRect(trackRect.getX(), trackRect.getY(), trackRect.getWidth(), trackRect.getHeight());
            g.setColour(juce::Colours::darkgrey.withAlpha(0.5f));
            g.drawRect(trackRect, 1.0f);
            float centerY = bounds.getCentreY();
            g.setColour(juce::Colours::grey.withAlpha(0.7f));
            g.drawLine(bounds.getCentreX() - 10, centerY, bounds.getCentreX() + 10, centerY, 1.0f);
            float thumbHeight = 30.0f; float thumbWidth = width * 0.9f;
            juce::Rectangle<float> thumb(bounds.getCentreX() - thumbWidth / 2, sliderPos - thumbHeight / 2, thumbWidth, thumbHeight);
            g.setColour(juce::Colour(0xff333333));
            g.fillRect(thumb.getX(), thumb.getY(), thumb.getWidth(), thumb.getHeight());
            g.setColour(juce::Colours::black);
            g.drawRect(thumb, 1.0f);
            g.setColour(juce::Colours::white.withAlpha(0.9f));
            g.drawLine(thumb.getX() + 2, thumb.getCentreY(), thumb.getRight() - 2, thumb.getCentreY(), 2.0f);
        }
        else if (style == juce::Slider::LinearVertical || style == juce::Slider::LinearBarVertical) {
            g.setColour(juce::Colour(0xff111111));
            g.fillRect(bounds.getX(), bounds.getY(), bounds.getWidth(), bounds.getHeight());
            int numSegments = 25; float segmentGap = 2.0f; float segmentHeight = (bounds.getHeight() - (numSegments * segmentGap)) / numSegments;
            float val = (float)slider.getValue(); if (slider.getMaximum() > 1.0f) val = val / slider.getMaximum();
            for (int i = 0; i < numSegments; ++i) {
                float segY = bounds.getBottom() - ((i + 1) * (segmentHeight + segmentGap));
                float threshold = (float)i / (float)numSegments;
                if (val > threshold) {
                    if (threshold > 0.85f) g.setColour(juce::Colours::red.withAlpha(0.9f));
                    else if (threshold > 0.6f) g.setColour(juce::Colours::yellow.withAlpha(0.9f));
                    else g.setColour(juce::Colour(0xff00dd00).withAlpha(0.9f));
                }
                else { g.setColour(juce::Colours::grey.withAlpha(0.1f)); }
                g.fillRect(bounds.getX() + 4.0f, segY, bounds.getWidth() - 8.0f, segmentHeight);
            }
            g.setColour(juce::Colour(0xff444444));
            float handleY = sliderPos - 7.0f;
            juce::Rectangle<float> handle(bounds.getX() - 5, handleY, bounds.getWidth() + 10, 14.0f);
            g.fillRect(handle.getX(), handle.getY(), handle.getWidth(), handle.getHeight());
            g.setColour(juce::Colours::black);
            g.drawLine(handle.getX(), handle.getCentreY(), handle.getRight(), handle.getCentreY(), 2.0f);
        }
        else {
            auto center = bounds.getCentreY();
            g.setColour(juce::Colours::black);
            g.fillRect(bounds.getX(), center - 4, bounds.getWidth(), 8.0f);
            g.setColour(juce::Colours::lightgrey);
            juce::Rectangle<float> handle(sliderPos - 10, center - 15, 20, 30);
            g.fillRect(handle.getX(), handle.getY(), handle.getWidth(), handle.getHeight());
        }
    }
};