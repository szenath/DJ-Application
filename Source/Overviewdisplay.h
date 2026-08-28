/*
  ==============================================================================
    OverviewDisplay.h

    Full-track static waveform overview strip — like the top bar in Rekordbox.

    Features
    ────────
    • Renders the ENTIRE track at once (not a scrolling window).
    • Amplitude → colour gradient per deck:
        Deck 1 (cyan)  : dark navy → mid cyan → bright white/cyan
        Deck 2 (amber) : dark brown → amber → bright white/yellow
      Quiet bars are dark/desaturated; loud transients are bright.
    • White vertical playhead line that moves as the track plays.
    • Thin horizontal centre line dividing top/bottom halves.
    • Clicking/dragging seeks the deck (calls onPositionChange).
    • The overview image is cached and only rebuilt when the track changes
      or the component is resized — so it's cheap to repaint each frame.
  ==============================================================================
*/

#pragma once
#include "../JuceLibraryCode/JuceHeader.h"
#include <array>

class OverviewDisplay : public juce::Component,
    public juce::ChangeListener
{
public:
    /** Constructs the overview strip.
        @param fmt        Shared AudioFormatManager (must have formats registered).
        @param cache      Shared AudioThumbnailCache.
        @param isLeftDeck True = cyan colour scheme; false = amber. */
    OverviewDisplay(juce::AudioFormatManager& fmt,
        juce::AudioThumbnailCache& cache,
        bool isLeftDeck)
        : audioThumb(512, fmt, cache), isLeft(isLeftDeck)
    {
        audioThumb.addChangeListener(this);
    }

    ~OverviewDisplay() override
    {
        audioThumb.removeChangeListener(this);
    }

    // ── Public API ────────────────────────────────────────────────────────────

    /** Loads a new track into the thumbnail and redraws.
        @param url  Local file URL or network URL to load. */
    void loadURL(juce::URL url)
    {
        audioThumb.clear();
        fileLoaded = false;
        position = 0.0;
        overviewDirty = true;

        if (url.isLocalFile())
            fileLoaded = audioThumb.setSource(
                new juce::FileInputSource(url.getLocalFile()));
        else if (!url.isEmpty())
            fileLoaded = audioThumb.setSource(
                new juce::URLInputSource(url));

        repaint();
    }

    /** Clears the loaded track and resets the strip to the empty state. */
    void clearTrack()
    {
        audioThumb.clear();
        fileLoaded = false;
        position = 0.0;
        overviewDirty = true;
        repaint();
    }

    /** Updates the playhead position and repaints (cheap — no cache rebuild).
        @param pos  Relative playback position (0.0 = start, 1.0 = end). */
    void setPositionRelative(double pos)
    {
        pos = juce::jlimit(0.0, 1.0, pos);
        if (std::abs(pos - position) < 0.0001) return;
        position = pos;
        repaint();
    }

    /** Callback invoked when the user clicks or drags the overview strip.
        Receives the clicked position as a 0.0–1.0 fraction. */
    std::function<void(double)> onPositionChange;

    // ── Component ─────────────────────────────────────────────────────────────

    void paint(juce::Graphics& g) override
    {
        const int w = getWidth();
        const int h = getHeight();

        if (!fileLoaded || audioThumb.getTotalLength() <= 0.0)
        {
            g.fillAll(juce::Colour(0xff080808));
            g.setColour(juce::Colour(0xff1a1a1a));
            g.drawRect(getLocalBounds(), 1);
            g.setColour(juce::Colours::grey.withAlpha(0.3f));
            g.setFont(10.0f);
            g.drawText("No track loaded", getLocalBounds(), juce::Justification::centred);
            return;
        }

        if (overviewDirty ||
            overviewCache.getWidth() != w ||
            overviewCache.getHeight() != h)
        {
            rebuildOverviewCache(w, h);
        }

        g.drawImageAt(overviewCache, 0, 0);

        // Played-region dim overlay
        int playedW = (int)(position * w);
        if (playedW > 0)
        {
            g.setColour(juce::Colours::black.withAlpha(0.30f));
            g.fillRect(0, 0, playedW, h);
        }

        // Playhead line and triangle pointer
        float phX = (float)(position * w);

        g.setColour(juce::Colours::white.withAlpha(0.12f));
        g.fillRect(phX - 2.0f, 0.0f, 5.0f, (float)h);

        g.setColour(juce::Colours::white.withAlpha(0.92f));
        g.drawLine(phX, 0.0f, phX, (float)h, 1.5f);

        juce::Path tri;
        tri.addTriangle(phX - 4.5f, 0.0f, phX + 4.5f, 0.0f, phX, 7.0f);
        g.setColour(juce::Colours::white);
        g.fillPath(tri);

        g.setColour(juce::Colours::white.withAlpha(0.08f));
        g.drawRect(getLocalBounds(), 1);
    }

    void resized() override
    {
        overviewDirty = true;
        repaint();
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (fileLoaded && onPositionChange)
            onPositionChange(juce::jlimit(0.0, 1.0,
                e.position.x / (double)getWidth()));
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (fileLoaded && onPositionChange)
            onPositionChange(juce::jlimit(0.0, 1.0,
                e.position.x / (double)getWidth()));
    }

    void changeListenerCallback(juce::ChangeBroadcaster*) override
    {
        overviewDirty = true;
        repaint();
    }

private:
    /** Returns the colour for a waveform bar based on amplitude and deck side.
        Maps 0.0–1.0 amplitude through a three-stop gradient (dark → mid → bright).
        @param amplitude  Normalised peak amplitude (0.0–1.0).
        @return           ARGB colour for the bar. */
    juce::Colour getBarColour(float amplitude) const
    {
        amplitude = juce::jlimit(0.0f, 1.0f, amplitude);

        if (isLeft)
        {
            if (amplitude < 0.30f)
            {
                float t = amplitude / 0.30f;
                return juce::Colour(0xff0a1a2e).interpolatedWith(juce::Colour(0xff007a99), t);
            }
            if (amplitude < 0.65f)
            {
                float t = (amplitude - 0.30f) / 0.35f;
                return juce::Colour(0xff007a99).interpolatedWith(juce::Colour(0xff00d4ff), t);
            }
            float t = (amplitude - 0.65f) / 0.35f;
            return juce::Colour(0xff00d4ff).interpolatedWith(juce::Colour(0xffe0faff), t);
        }
        else
        {
            if (amplitude < 0.30f)
            {
                float t = amplitude / 0.30f;
                return juce::Colour(0xff1e1000).interpolatedWith(juce::Colour(0xff8a4a00), t);
            }
            if (amplitude < 0.65f)
            {
                float t = (amplitude - 0.30f) / 0.35f;
                return juce::Colour(0xff8a4a00).interpolatedWith(juce::Colour(0xffffa500), t);
            }
            float t = (amplitude - 0.65f) / 0.35f;
            return juce::Colour(0xffffa500).interpolatedWith(juce::Colour(0xfffff0c0), t);
        }
    }

    /** Renders the full-track waveform into the cached image.
        Called only when overviewDirty is true or the component is resized.
        @param w  Current component width in pixels.
        @param h  Current component height in pixels. */
    void rebuildOverviewCache(int w, int h)
    {
        overviewCache = juce::Image(juce::Image::ARGB, w, h, true);
        juce::Graphics cg(overviewCache);

        cg.fillAll(juce::Colour(0xff080808));

        const double totalLen = audioThumb.getTotalLength();
        if (totalLen <= 0.0) { overviewDirty = false; return; }

        const int centerY = h / 2;
        const int barW = 2;
        const int gapW = 1;
        const int stride = barW + gapW;

        for (int x = 0; x + barW <= w; x += stride)
        {
            double t0 = (x / (double)w) * totalLen;
            double t1 = ((x + barW) / (double)w) * totalLen;

            float minVal = 0.0f, maxVal = 0.0f;
            audioThumb.getApproximateMinMax(t0, t1, 0, minVal, maxVal);

            float amplitude = juce::jmax(std::abs(minVal), std::abs(maxVal));
            amplitude = juce::jmax(amplitude, 0.015f);

            juce::Colour col = getBarColour(amplitude);

            float scaledAmp = amplitude * (centerY * 0.88f);
            float topY = (float)centerY - scaledAmp;
            float botY = (float)centerY + scaledAmp;
            float bwF = (float)barW;
            float capH = juce::jmax(1.5f, scaledAmp * 0.10f);

            cg.setColour(col.brighter(1.0f).withAlpha(0.95f));
            cg.fillRect((float)x, topY, bwF, capH);

            cg.setColour(col.withAlpha(0.90f));
            cg.fillRect((float)x, topY + capH, bwF, (float)centerY - (topY + capH));

            cg.setColour(col.darker(0.25f).withAlpha(0.78f));
            cg.fillRect((float)x, (float)centerY, bwF, botY - (float)centerY - capH);

            cg.setColour(col.brighter(0.6f).withAlpha(0.85f));
            cg.fillRect((float)x, botY - capH, bwF, capH);
        }

        cg.setColour(juce::Colours::black.withAlpha(0.5f));
        cg.fillRect(0.0f, (float)centerY - 0.5f, (float)w, 1.0f);

        overviewDirty = false;
    }

    juce::AudioThumbnail audioThumb;
    bool   isLeft;
    bool   fileLoaded{ false };
    double position{ 0.0 };
    bool   overviewDirty{ true };
    juce::Image overviewCache;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OverviewDisplay)
};