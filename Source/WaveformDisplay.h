/*
  ==============================================================================
    WaveformDisplay.h
    Performance-optimized: cached waveform image, threshold-gated repaints,
    dirty flag to avoid redundant pixel-level work on every timer tick.
  ==============================================================================
*/

#pragma once

#include "../JuceLibraryCode/JuceHeader.h"
#include <array>

class WaveformDisplay : public juce::Component,
    public juce::ChangeListener
{
public:
    WaveformDisplay(juce::AudioFormatManager& formatManagerToUse,
        juce::AudioThumbnailCache& cacheToUse);
    ~WaveformDisplay() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;

    void changeListenerCallback(juce::ChangeBroadcaster* source) override;

    void loadURL(juce::URL audioURL);
    void setPositionRelative(double pos);
    void setCustomColour(juce::Colour c);

 

    // Speed-reactive zoom: 1.0 = normal, <1.0 = spread out, >1.0 = compressed
    // Call this whenever the deck speed slider changes.
    void setPlaybackSpeed(double speed);

    // Loop region overlay — pass absolute seconds from DJAudioPlayer.
    // Call setLoopRegion(start, end) when a loop activates/changes.
    // Call clearLoopRegion() when the loop is disabled.
    void setLoopRegion(double loopStartSeconds, double loopEndSeconds);
    void clearLoopRegion();

    // Hot Cue visualization
    void setHotCueMarker(int cueIndex, double position);
    void clearHotCueMarker(int cueIndex);
    void clearAllHotCueMarkers();

    std::function<void(double)> onPositionChange;

private:
    juce::AudioThumbnail audioThumb;
    bool fileLoaded;
    double position;
    double currentSpeed{ 1.0 };   // playback speed ratio — drives zoom level
    juce::Colour customColour{ juce::Colours::orange };

    // Loop region (-1.0 = not active)
    double loopStartSecs{ -1.0 };
    double loopEndSecs{ -1.0 };

    // -----------------------------------------------------------------------
    // PERFORMANCE: Cached waveform image
    // Cache stores the current visible 10-second window at full component
    // resolution. Rebuilt only when: new file loaded, thumbnail updates,
    // component resized, or playhead scrolls far enough to shift the window.
    // -----------------------------------------------------------------------
    juce::Image waveformCache;
    bool   waveformDirty{ true };
    double cachedStartTime{ -1.0 };
    double cachedEndTime{ -1.0 };

    // Only repaint when position moves by more than this fraction
    static constexpr double kPositionRepaintThreshold = 0.00015;
    double lastDrawnPosition{ -1.0 };

    void rebuildWaveformCache(int width, int height, double startTime, double endTime);
    void getVisibleWindow(double totalLength, double& startTime, double& endTime) const;

    juce::Colour getSpectralColour(float amplitude);

    // Hot Cue marker positions (-1.0 = not set)
    std::array<double, 8> hotCueMarkers{ -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0 };

    std::array<juce::Colour, 8> cueColours{
        juce::Colour(0xffff4444),
        juce::Colour(0xffff8844),
        juce::Colour(0xffffcc44),
        juce::Colour(0xff44ff44),
        juce::Colour(0xff44ffff),
        juce::Colour(0xff4444ff),
        juce::Colour(0xffff44ff),
        juce::Colour(0xffff88ff)
    };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WaveformDisplay)
};