/*

  ==============================================================================

    WaveformDisplay.cpp

    Performance-optimized DJ Waveform Display.

    APPROACH:

    - The cache renders the VISIBLE 10-second window (not the full file).

    - It is only rebuilt when position moves enough to scroll (> 0.5px worth).

    - The expensive pixel loop + ColourGradient allocation are removed.

    - Playhead and hot-cue markers are drawn on top each frame (cheap).

    - setPositionRelative() only triggers repaint when position moves enough

      to matter visually (threshold-gated).

  ==============================================================================

*/

#include "WaveformDisplay.h"

WaveformDisplay::WaveformDisplay(juce::AudioFormatManager& formatManagerToUse,
    juce::AudioThumbnailCache& cacheToUse)

    : audioThumb(1000, formatManagerToUse, cacheToUse),
    fileLoaded(false),
    position(0.0)
{
    audioThumb.addChangeListener(this);
}

WaveformDisplay::~WaveformDisplay()

{
    audioThumb.removeChangeListener(this);
}

void WaveformDisplay::setCustomColour(juce::Colour c)

{
    customColour = c;
    waveformDirty = true;
    repaint();
}

void WaveformDisplay::setPlaybackSpeed(double speed)

{
    // Clamp to safe range and only act if meaningfully changed
    speed = juce::jlimit(0.25, 4.0, speed);
    if (std::abs(speed - currentSpeed) < 0.001) return;
    currentSpeed = speed;
    waveformDirty = true;   // window size changed — must redraw cache
    repaint();
}

void WaveformDisplay::setLoopRegion(double loopStartSeconds, double loopEndSeconds)

{
    loopStartSecs = loopStartSeconds;
    loopEndSecs = loopEndSeconds;
    repaint();  // overlay only — no need to invalidate waveform cache
}

void WaveformDisplay::clearLoopRegion()

{
    loopStartSecs = -1.0;
    loopEndSecs = -1.0;
    repaint();
}

juce::Colour WaveformDisplay::getSpectralColour(float amplitude)

{
    if (amplitude < 0.15f)
    {
        float t = amplitude / 0.15f;
        return juce::Colour(0xff1565c0).interpolatedWith(juce::Colour(0xff00acc1), t);
    }
    if (amplitude < 0.45f)
    {
        float t = (amplitude - 0.15f) / 0.30f;
        return juce::Colour(0xff00acc1).interpolatedWith(juce::Colour(0xff66bb6a), t);
    }
    if (amplitude < 0.70f)
    {
        float t = (amplitude - 0.45f) / 0.25f;
        return juce::Colour(0xff66bb6a).interpolatedWith(juce::Colour(0xffffa726), t);
    }
    float t = juce::jmin(1.0f, (amplitude - 0.70f) / 0.30f);
    return juce::Colour(0xffffa726).interpolatedWith(juce::Colour(0xfff44336), t);
}

// ─────────────────────────────────────────────────────────────────────────────

// rebuildWaveformCache

// Renders the CURRENT visible 10-second window into the cache image.

// Called only when waveformDirty == true (new file, thumbnail update, or
// position scrolled enough to need a new window).

// ─────────────────────────────────────────────────────────────────────────────

void WaveformDisplay::rebuildWaveformCache(int width, int height,
    double startTime, double endTime)

{
    waveformCache = juce::Image(juce::Image::ARGB, width, height, true);
    juce::Graphics cg(waveformCache);
    cg.fillAll(juce::Colour(0xff0a0a0a));
    double totalLength = audioThumb.getTotalLength();
    if (totalLength <= 0.0) { waveformDirty = false; return; }
    int centerY = height / 2;
    // =========================================================================
    // PROFESSIONAL DJ WAVEFORM RENDERING
    //
    // Looking at Rekordbox/Serato/VirtualDJ: waveforms are drawn as distinct
    // vertical bars, grouped in clusters of ~3px wide with a 1px dark gap
    // between each bar. This gives the clear "teeth" / spike look where each
    // beat transient is a visible sharp column rather than a solid smear.
    //
    // Each bar samples a small time slice. The gap between bars is left as the
    // dark background, so individual peaks are clearly separated.
    //
    // We also layer the bar: a thin bright "cap" at the top of each peak (like
    // the bright yellow/white top edge in Rekordbox), then the main colour body,
    // then a dimmer mirror below centre. This gives the 3D depth look.
    // =========================================================================
    const int   barWidth = 3;   // pixels wide per bar
    const int   gapWidth = 1;   // dark gap between bars
    const int   stride = barWidth + gapWidth;
    for (int x = 0; x + barWidth <= width; x += stride)
    {
        // Sample a time slice that corresponds to this bar's width
        double timeAtX = startTime + (x / (double)width) * (endTime - startTime);
        double timeAtNextX = startTime + ((x + barWidth) / (double)width) * (endTime - startTime);
        float minVal = 0.0f, maxVal = 0.0f;
        audioThumb.getApproximateMinMax(timeAtX, timeAtNextX, 0, minVal, maxVal);
        float amplitude = juce::jmax(std::abs(minVal), std::abs(maxVal));
        // Clamp minimum visible height so even quiet parts show a tiny bar
        amplitude = juce::jmax(amplitude, 0.02f);
        juce::Colour barColour = getSpectralColour(amplitude);
        float scaledAmp = amplitude * (height * 0.46f);
        float topY = (float)centerY - scaledAmp;
        float botY = (float)centerY + scaledAmp;
        float bw = (float)barWidth;
        // --- Cap line at top peak (bright highlight, 2px tall) ---
        float capHeight = juce::jmax(2.0f, scaledAmp * 0.08f);
        cg.setColour(barColour.brighter(0.9f).withAlpha(0.95f));
        cg.fillRect((float)x, topY, bw, capHeight);
        // --- Top half main body ---
        cg.setColour(barColour.withAlpha(0.88f));
        cg.fillRect((float)x, topY + capHeight, bw, (float)centerY - (topY + capHeight));
        // --- Bottom half (slightly darker mirror) ---
        cg.setColour(barColour.darker(0.3f).withAlpha(0.75f));
        cg.fillRect((float)x, (float)centerY, bw, botY - (float)centerY - capHeight);
        // --- Cap line at bottom peak ---
        cg.setColour(barColour.brighter(0.6f).withAlpha(0.85f));
        cg.fillRect((float)x, botY - capHeight, bw, capHeight);
        // The gap (gapWidth pixels) is left as the dark background — no draw needed
    }
    // Subtle centre divider line
    cg.setColour(juce::Colours::black.withAlpha(0.6f));
    cg.fillRect(0.0f, (float)centerY - 1.0f, (float)width, 2.0f);
    cachedStartTime = startTime;
    cachedEndTime = endTime;
    waveformDirty = false;
}

// ─────────────────────────────────────────────────────────────────────────────

// getVisibleWindow

//

// The visible time window scales with playback speed to produce the

// "rubber band" effect seen in professional DJ software:

//

//   speed < 1.0  →  smaller window  →  waveform spread out  (zoomed in)

//   speed = 1.0  →  10 seconds base window

//   speed > 1.0  →  larger window   →  waveform compressed  (zoomed out)

//

// Formula:  window = baseWindow * speed

// At 0.5x speed → 5s window  → beats appear twice as wide

// At 2.0x speed → 20s window → beats appear half as wide

// ─────────────────────────────────────────────────────────────────────────────

void WaveformDisplay::getVisibleWindow(double totalLength,
    double& startTime, double& endTime) const

{
    const double baseWindow = 10.0;
    const double safeSpeed = juce::jlimit(0.25, 4.0, currentSpeed);
    const double visibleTimeWindow = juce::jmin(baseWindow * safeSpeed, totalLength);
    double currentTime = position * totalLength;
    if (currentTime < visibleTimeWindow / 2.0)
    {
        startTime = 0.0;
        endTime = visibleTimeWindow;
    }
    else if (currentTime > totalLength - visibleTimeWindow / 2.0)
    {
        endTime = totalLength;
        startTime = totalLength - visibleTimeWindow;
    }
    else
    {
        startTime = currentTime - visibleTimeWindow / 2.0;
        endTime = currentTime + visibleTimeWindow / 2.0;
    }
    startTime = juce::jmax(0.0, startTime);
    endTime = juce::jmin(totalLength, endTime);
}

// ─────────────────────────────────────────────────────────────────────────────

// paint

// ─────────────────────────────────────────────────────────────────────────────

void WaveformDisplay::paint(juce::Graphics& g)

{
    auto bounds = getLocalBounds();
    int width = bounds.getWidth();
    int height = bounds.getHeight();
    if (!fileLoaded)
    {
        g.fillAll(juce::Colour(0xff0a0a0a));
        g.setColour(juce::Colour(0xff1a1a1a));
        g.drawRect(bounds, 1);
        return;
    }
    double totalLength = audioThumb.getTotalLength();
    if (totalLength <= 0.0) { g.fillAll(juce::Colour(0xff0a0a0a)); return; }
    double startTime, endTime;
    getVisibleWindow(totalLength, startTime, endTime);
    // C7 note: rebuildWaveformCache() modifies waveformCache, cachedStartTime,
    // cachedEndTime, and waveformDirty. This is a deliberate lazy-caching pattern —
    // the expensive pixel-rendering pass is deferred until the frame where the result
    // is actually needed. The cache is only ever rebuilt when the visible window has
    // genuinely changed, so this is far cheaper than rebuilding every frame.
    const bool windowChanged = (std::abs(startTime - cachedStartTime) > 0.001 ||
        std::abs(endTime - cachedEndTime) > 0.001);
    if (waveformDirty ||
        waveformCache.getWidth() != width ||
        waveformCache.getHeight() != height ||
        windowChanged)
    {
        rebuildWaveformCache(width, height, startTime, endTime);
    }
    // Blit the cache (O(1))
    g.drawImageAt(waveformCache, 0, 0);
    // ─── Active Loop Region Overlay ──────────────────────────────────────────
    // Draws a semi-transparent blue box over the loop section, with solid
    // edge lines marking the in/out points — exactly like Rekordbox/Serato.
    if (loopStartSecs >= 0.0 && loopEndSecs > loopStartSecs)
    {
        // Only draw if any part of the loop is within the visible window
        if (loopStartSecs < endTime && loopEndSecs > startTime)
        {
            // Clamp to visible window
            double visLoopStart = juce::jmax(loopStartSecs, startTime);
            double visLoopEnd = juce::jmin(loopEndSecs, endTime);
            float loopX1 = (float)(((visLoopStart - startTime) / (endTime - startTime)) * width);
            float loopX2 = (float)(((visLoopEnd - startTime) / (endTime - startTime)) * width);
            float loopW = loopX2 - loopX1;
            // Semi-transparent blue fill
            g.setColour(juce::Colour(0xff1e88e5).withAlpha(0.22f));
            g.fillRect(loopX1, 0.0f, loopW, (float)height);
            // Left edge — Loop In point (solid blue line)
            const float edgeLineWidth = 2.5f;
            g.setColour(juce::Colour(0xff42a5f5).withAlpha(0.95f));
            g.drawLine(loopX1, 0.0f, loopX1, (float)height, edgeLineWidth);
            // Right edge — Loop Out point (solid blue line)
            g.drawLine(loopX2, 0.0f, loopX2, (float)height, edgeLineWidth);
            // Small "LOOP" label in top-left corner of the region (if wide enough)
            if (loopW > 30.0f)
            {
                g.setColour(juce::Colour(0xff90caf9).withAlpha(0.85f));
                g.setFont(juce::Font(10.0f, juce::Font::bold));
                g.drawText("LOOP", (int)loopX1 + 4, 3, juce::jmin((int)loopW - 8, 40), 14,
                    juce::Justification::centredLeft, false);
            }
        }
    }
    // ─── Hot Cue Markers ────────────────────────────────────────────────────
    for (int i = 0; i < 8; ++i)
    {
        if (hotCueMarkers[i] < 0.0 || hotCueMarkers[i] > 1.0) continue;
        double cueTime = hotCueMarkers[i] * totalLength;
        if (cueTime < startTime || cueTime > endTime) continue;
        float cueX = (float)(((cueTime - startTime) / (endTime - startTime)) * width);
        g.setColour(cueColours[i].withAlpha(0.85f));
        g.drawLine(cueX, 0.0f, cueX, (float)height, 2.5f);
        g.setColour(cueColours[i]);
        g.fillRoundedRectangle(cueX - 7.0f, 3.0f, 14.0f, 14.0f, 2.0f);
        g.setColour(juce::Colours::black);
        g.setFont(juce::Font(9.0f, juce::Font::bold));
        g.drawText(juce::String(i + 1), (int)cueX - 7, 3, 14, 14,
            juce::Justification::centred, false);
    }
    // ─── Playhead ────────────────────────────────────────────────────────────
    double currentTime = position * totalLength;
    float playheadX;
    // FIX: Calculate 'visibleTimeWindow' exactly as getVisibleWindow() does.
    // Previously, this used a hardcoded '10.0' which caused the playhead to
    // snap to the center prematurely when speed was high (e.g. window size > 10.0).
    const double baseWindow = 10.0;
    const double safeSpeed = juce::jlimit(0.25, 4.0, currentSpeed);
    const double visibleTimeWindow = juce::jmin(baseWindow * safeSpeed, totalLength);
    if (currentTime < visibleTimeWindow / 2.0)
        playheadX = (float)((currentTime / (endTime - startTime)) * width);
    else if (currentTime > totalLength - visibleTimeWindow / 2.0)
        playheadX = (float)(((currentTime - startTime) / (endTime - startTime)) * width);
    else
        playheadX = (float)(width / 2);
    g.setColour(juce::Colours::white.withAlpha(0.95f));
    g.drawLine(playheadX, 0.0f, playheadX, (float)height, 2.5f);
    g.setColour(juce::Colours::white);
    juce::Path triangle;
    triangle.addTriangle(playheadX - 6.0f, 0.0f,
        playheadX + 6.0f, 0.0f,
        playheadX, 10.0f);
    g.fillPath(triangle);
}

void WaveformDisplay::resized()

{
    waveformDirty = true;
    repaint();
}

void WaveformDisplay::loadURL(juce::URL audioURL)

{
    audioThumb.clear();
    fileLoaded = false;
    position = 0.0;
    lastDrawnPosition = -1.0;   // force repaint regardless of position value
    waveformDirty = true;
    cachedStartTime = -1.0;
    cachedEndTime = -1.0;
    clearAllHotCueMarkers();
    clearLoopRegion();
    if (audioURL.isLocalFile())
        fileLoaded = audioThumb.setSource(new juce::FileInputSource(audioURL.getLocalFile()));
    else if (!audioURL.isEmpty())
        fileLoaded = audioThumb.setSource(new juce::URLInputSource(audioURL));
    // Always repaint: clears the waveform immediately on reset (fileLoaded=false path)
    repaint();
}

void WaveformDisplay::changeListenerCallback(juce::ChangeBroadcaster*)

{
    // Thumbnail still building — mark dirty so next paint rebuilds cache
    waveformDirty = true;
    repaint();
}

void WaveformDisplay::setPositionRelative(double pos)

{
    if (std::isnan(pos)) return;
    pos = juce::jlimit(0.0, 1.0, pos);
    // lastDrawnPosition == -1.0 signals a forced repaint (e.g. after loadURL/reset).
    // Otherwise skip repaints smaller than threshold to avoid jitter on paused tracks.
    if (lastDrawnPosition >= 0.0 &&
        std::abs(pos - lastDrawnPosition) < kPositionRepaintThreshold)
        return;
    position = pos;
    lastDrawnPosition = pos;
    repaint();
}

void WaveformDisplay::mouseDown(const juce::MouseEvent& event)

{
    if (!fileLoaded || !onPositionChange) return;
    double totalLength = audioThumb.getTotalLength();
    if (totalLength <= 0.0) return;
    // The waveform shows a scrolling window, NOT the full track.
    // We must convert the click pixel → visible-window time → full-track fraction.
    double startTime, endTime;
    getVisibleWindow(totalLength, startTime, endTime);
    double clickedTime = startTime + (event.position.x / (double)getWidth()) * (endTime - startTime);
    onPositionChange(juce::jlimit(0.0, 1.0, clickedTime / totalLength));
}

void WaveformDisplay::mouseDrag(const juce::MouseEvent& event)

{
    if (!fileLoaded || !onPositionChange) return;
    double totalLength = audioThumb.getTotalLength();
    if (totalLength <= 0.0) return;
    double startTime, endTime;
    getVisibleWindow(totalLength, startTime, endTime);
    double clickedTime = startTime + (juce::jlimit(0.0f, (float)getWidth(), event.position.x) / (double)getWidth()) * (endTime - startTime);
    onPositionChange(juce::jlimit(0.0, 1.0, clickedTime / totalLength));
}

void WaveformDisplay::setHotCueMarker(int cueIndex, double pos)

{
    if (cueIndex >= 0 && cueIndex < 8)
    {
        hotCueMarkers[cueIndex] = juce::jlimit(0.0, 1.0, pos);
        repaint();
    }
}

void WaveformDisplay::clearHotCueMarker(int cueIndex)

{
    if (cueIndex >= 0 && cueIndex < 8)
    {
        hotCueMarkers[cueIndex] = -1.0;
        repaint();
    }
}

void WaveformDisplay::clearAllHotCueMarkers()

{
    hotCueMarkers.fill(-1.0);
    repaint();
}