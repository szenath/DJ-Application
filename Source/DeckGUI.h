/*
  ==============================================================================
    DeckGUI.h
  ==============================================================================
*/

#pragma once

#include "../JuceLibraryCode/JuceHeader.h"
#include "DJAudioPlayer.h"
#include "WaveformDisplay.h"
#include "OverviewDisplay.h"
#include "FXUI.h"

class DeckGUI : public juce::Component,
    public juce::Button::Listener,
    public juce::Slider::Listener,
    public juce::FileDragAndDropTarget,
    public juce::DragAndDropTarget,
    public juce::Timer
{
public:
    /** Constructs a deck GUI wired to the given audio player.
        @param player              The DJAudioPlayer this GUI controls.
        @param formatManagerToUse  Shared format manager for waveform thumbnails.
        @param cacheToUse          Shared thumbnail cache.
        @param isLeftDeck          True = left deck (cyan), false = right deck (amber). */
    DeckGUI(DJAudioPlayer* player,
        juce::AudioFormatManager& formatManagerToUse,
        juce::AudioThumbnailCache& cacheToUse,
        bool isLeftDeck);

    ~DeckGUI() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void buttonClicked(juce::Button* button) override;
    void sliderValueChanged(juce::Slider* slider) override;

    /** Accepts any audio file dragged onto this deck. */
    bool isInterestedInFileDrag(const juce::StringArray& files) override;

    /** Loads the first dropped file into this deck. */
    void filesDropped(const juce::StringArray& files, int x, int y) override;

    /** Accepts drag-and-drop from the playlist component. */
    bool isInterestedInDragSource(const SourceDetails& dragSourceDetails) override;

    /** Loads a file dragged in from the playlist. */
    void itemDropped(const SourceDetails& dragSourceDetails) override;

    /** Polls player state to keep the UI in sync (playhead, loop, hot cue colours). */
    void timerCallback() override;

    /** Resets the deck to defaults: stops playback, clears waveform,
        resets all sliders, EQ, cues, loop, and FX. */
    void resetToDefaults();

    // ── Accessors (used by MainComponent for layout and sync) ─────────────────

    /** Returns the play/pause toggle button. */
    juce::TextButton* getPlayPauseButton() { return &playPauseButton; }

    /** Returns the BPM sync button. */
    juce::TextButton* getSyncButton() { return &syncButton; }

    /** Returns the track title label. */
    juce::Label* getTitleLabel() { return &titleLabel; }

    /** Returns the scrolling waveform display. */
    WaveformDisplay* getWaveformDisplay() { return &waveformDisplay; }

    /** Returns the volume fader. */
    juce::Slider* getVolSlider() { return &volSlider; }

    /** Returns the pitch/speed slider. */
    juce::Slider* getSpeedSlider() { return &speedSlider; }

    /** Returns the BPM readout label. */
    juce::Label* getBpmLabel() { return &bpmLabel; }

    /** Returns the BPM percentage offset label. */
    juce::Label* getBpmPercentLabel() { return &bpmPercentLabel; }

    /** Returns a hot cue button by index (0-7), or nullptr if out of range. */
    juce::TextButton* getHotCueButton(int index) { return (index >= 0 && index < 8) ? &hotCueButtons[index] : nullptr; }

    /** Returns the "Clear All Cues" button. */
    juce::TextButton* getClearCuesButton() { return &clearCuesButton; }

    /** Returns the full-track overview strip. */
    OverviewDisplay* getOverviewDisplay() { return &overviewDisplay; }

private:
    /** Updates BPM and speed-offset labels from the current speed slider value. */
    void updateBPMLabel();

    /** Loads a file into the player, waveform, and overview strip,
        and restores persisted hot cues and EQ for the track.
        @param file  The audio file to load. */
    void loadFile(const juce::File& file);

    /** Handles a hot cue button click: sets the cue if empty, or seeks to it if set.
        @param cueIndex  0-7. */
    void handleHotCueClick(int cueIndex);

    /** Refreshes hot cue button colours to reflect current set/empty state. */
    void updateHotCueButtons();

    /** Saves all 8 hot cue positions to hotcues.json for the given track path. */
    void saveHotCuesForTrack(const juce::String& trackPath);

    /** Loads persisted hot cue positions from hotcues.json for the given track path. */
    void loadHotCuesForTrack(const juce::String& trackPath);

    /** Returns the path to the shared hot cues JSON file. */
    juce::File getHotCuesFile();

    /** Saves EQ/filter knob values to eq_settings.json for the given track path. */
    void saveEqSettings(const juce::String& trackPath);

    /** Loads EQ/filter values from eq_settings.json for the given track path. */
    void loadEqSettings(const juce::String& trackPath);

    /** Returns the path to the shared EQ settings JSON file. */
    juce::File getEqSettingsFile();

    /** Refreshes the loop size label (e.g. "1/4", "4") from currentLoopIndex. */
    void updateLoopDisplay();

    DJAudioPlayer* player;
    bool isLeftDeck;

    juce::TextButton playPauseButton{ "PLAY_PAUSE" };
    juce::TextButton syncButton{ "SYNC" };
    juce::Label titleLabel;
    juce::Slider volSlider;
    juce::Slider speedSlider;
    juce::Label bpmLabel;
    juce::Label bpmPercentLabel;
    WaveformDisplay waveformDisplay;
    OverviewDisplay overviewDisplay;

    std::array<juce::TextButton, 8> hotCueButtons;
    juce::TextButton clearCuesButton{ "CLEAR CUES" };

    juce::TextButton loopToggleButton{ "LOOP_TOGGLE" };
    juce::Label loopSizeLabel;
    juce::TextButton loopSizeUpButton{ "LOOP_UP" };
    juce::TextButton loopSizeDownButton{ "LOOP_DOWN" };
    const std::vector<double> loopSizes{ 0.25, 0.5, 1.0, 2.0, 4.0, 8.0, 16.0, 32.0 };
    int currentLoopIndex = 4;

    juce::Slider highEqSlider;
    juce::Slider midEqSlider;
    juce::Slider lowEqSlider;
    juce::Label highEqLabel;
    juce::Label midEqLabel;
    juce::Label lowEqLabel;

    juce::Slider filterSlider;
    juce::Label filterLabel;

    FXButtonComponent fxButton;

    bool ignoreTimerUpdates{ false };
    bool isDraggingSpeed{ false };
    uint8_t lastHotCueState{ 0xFF };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DeckGUI)
};