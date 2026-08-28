/*
  ==============================================================================
    MainComponent.h
  ==============================================================================
*/

#pragma once

#include "../JuceLibraryCode/JuceHeader.h"

#include "DJAudioPlayer.h"
#include "DeckGUI.h"
#include "PlaylistComponent.h"
#include "DJLookAndFeel.h"
#include "RecordingManager.h"

class MainComponent : public juce::AudioAppComponent,
    public juce::Slider::Listener,
    public juce::Timer
{
public:
    MainComponent();
    ~MainComponent() override;

    // AudioAppComponent

    /** Prepares both players and the mixer for streaming.
        @param samplesPerBlockExpected  Expected audio block size.
        @param sampleRate              Device sample rate in Hz. */
    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;

    /** Fills the output buffer from the mixer and forwards to RecordingManager if recording. */
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override;

    /** Releases all audio resources. */
    void releaseResources() override;

    // Component

    /** Draws the background, separator lines, master VU meter, and turntables. */
    void paint(juce::Graphics& g) override;

    /** Lays out all child components relative to the current window size. */
    void resized() override;

    // Slider::Listener

    /** Handles the crossfader using a constant-power panning law. */
    void sliderValueChanged(juce::Slider* slider) override;

    // Timer

    /** Triggers a repaint at ~30 Hz to animate turntables and VU meters. */
    void timerCallback() override;

private:
    /** Draws a spinning turntable graphic driven by the player's playback position.
        @param g        Graphics context.
        @param centerX  Centre X in pixels.
        @param centerY  Centre Y in pixels.
        @param radius   Outer platter radius in pixels.
        @param player   Player whose position drives the rotation angle.
        @param isLeft   True = cyan needle (deck 1), false = amber (deck 2). */
    void drawTurntable(juce::Graphics& g, int centerX, int centerY, int radius,
        DJAudioPlayer* player, bool isLeftDeck);

    /** Draws the two-row master VU meter at the top of the window.
        @param g     Graphics context.
        @param w     Current window width in pixels.
        @param yPos  Top Y coordinate of the meter bar. */
    void drawMasterVUMeter(juce::Graphics& g, int w, int yPos);

    // ── Recording helpers ────────────────────────────────────────────────────
    void openRecordingsPanel();
    void startRecording();

    /** Opens the help popup panel with instructions for all features. */
    void openHelpPanel();

    void resetAll();

    DJLookAndFeel djLookAndFeel;

    juce::AudioFormatManager formatManager;
    juce::AudioThumbnailCache thumbCache{ 100 };

    DJAudioPlayer player1{ formatManager };
    DJAudioPlayer player2{ formatManager };

    juce::MixerAudioSource mixerSource;

    DeckGUI deckGUI1{ &player1, formatManager, thumbCache, true };
    DeckGUI deckGUI2{ &player2, formatManager, thumbCache, false };

    juce::Slider crossfader;
    juce::Label  crossfaderLabel;

    PlaylistComponent playlistComponent;

    // ── Recording ────────────────────────────────────────────────────────────
    RecordingManager recordingManager;
    RecordButton     recordButton;

    // SafePointer automatically becomes nullptr when the CallOutBox closes
    // and the panel is deleted — no dangling pointer crash on Stop
    juce::Component::SafePointer<RecordingsPanel> activePanel;

    // ── Restart ──────────────────────────────────────────────────────────────
    juce::TextButton restartButton{ "RESTART" };

    // ── Help ─────────────────────────────────────────────────────────────────
    juce::TextButton helpButton{ "?" };

    // Level atomics (kept for compatibility)
    std::atomic<float> currentLeftLevel{ 0.0f };
    std::atomic<float> currentRightLevel{ 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};