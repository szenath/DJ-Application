/*
  ==============================================================================
    RecordingManager.h
    Captures the master mix output to a WAV file.
    Thread-safe: audio thread writes samples; UI thread controls start/stop.
  ==============================================================================
*/

#pragma once

#include "../JuceLibraryCode/JuceHeader.h"
#include <atomic>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
// RecordingInfo  — metadata for one completed recording
// ─────────────────────────────────────────────────────────────────────────────
struct RecordingInfo
{
    juce::File   file;
    juce::String displayName;
    double       durationSeconds{ 0.0 };
    juce::Time   timestamp;

    RecordingInfo() = default;
    RecordingInfo(const juce::File& f, const juce::String& name,
        double dur, juce::Time ts)
        : file(f), displayName(name), durationSeconds(dur), timestamp(ts) {
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// RecordingManager
// ─────────────────────────────────────────────────────────────────────────────
class RecordingManager
{
public:
    RecordingManager();
    ~RecordingManager();

    void prepareToRecord(double sampleRate, int numChannels);

    /** Called from the audio thread — writes samples when recording is active. */
    void processBlock(const juce::AudioBuffer<float>& buffer,
        int startSample, int numSamples);

    /** UI thread: begin a new recording. Returns false if already recording. */
    bool startRecording();

    /** UI thread: end the current recording and save it. */
    RecordingInfo stopRecording();

    bool isRecording() const noexcept { return recording.load(); }
    double getElapsedSeconds() const noexcept;
    const std::vector<RecordingInfo>& getRecordings() const { return recordings; }
    void deleteRecording(int index);
    juce::File getRecordingsDirectory() const;

private:
    double  currentSampleRate{ 44100.0 };
    int     currentNumChannels{ 2 };

    std::atomic<bool>    recording{ false };
    std::atomic<bool>    isProcessingBlock{ false };
    std::atomic<int64_t> samplesWritten{ 0 };

    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> threadedWriter;
    juce::TimeSliceThread writerThread{ "RecordingWriterThread" };

    juce::WavAudioFormat wavFormat;
    juce::Time           recordingStartTime;
    int                  recordingIndex{ 1 };

    std::vector<RecordingInfo> recordings;
    juce::File                 currentOutputFile;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RecordingManager)
};

// ─────────────────────────────────────────────────────────────────────────────
// RecordingsPanel
// ─────────────────────────────────────────────────────────────────────────────
class RecordingsPanel : public juce::Component,
    public juce::Timer
{
public:
    explicit RecordingsPanel(RecordingManager& mgr,
        juce::AudioDeviceManager& sharedDeviceManager);
    ~RecordingsPanel() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;
    void refresh();

    std::function<void()> onStartNewRecording;
    std::function<void()> onStopRecording;

private:
    // ── Playback control ─────────────────────────────────────────────────────
    // Three clearly-separate actions:
    //   playRecording(idx) — loads a new file and starts it from the beginning
    //   stopPlayback()     — completely stops & tears down the transport chain
    //   pauseResume()      — toggles pause without destroying the transport
    void playRecording(int index);
    void stopPlayback();
    void pauseResume();

    void rebuildRecordingRows();
    void updateRowVisuals();           // refreshes button colours / enabled state
    juce::String formatDuration(double secs) const;

    RecordingManager& manager;
    juce::AudioDeviceManager& sharedDeviceManager;

    juce::TextButton startButton{ "Start New Audio Recording" };
    juce::Label      statusLabel;
    juce::TextButton stopButton{ "Stop" };

    // ── Playback state ────────────────────────────────────────────────────────
    // playingIndex  : which recording row is loaded (-1 = none)
    // transportPaused: transport exists but is stopped mid-play
    int  playingIndex{ -1 };
    bool transportPaused{ false };

    // ── RecordingRow ──────────────────────────────────────────────────────────
    struct RecordingRow : public juce::Component
    {
        juce::Label      nameLabel, durationLabel, dateLabel;
        juce::TextButton playButton{ ">" };   // play / stop toggle
        juce::TextButton pauseButton{ "||" };   // pause / resume
        juce::TextButton deleteButton{ "X" };

        int index{ 0 };

        // Callbacks assigned by RecordingsPanel
        std::function<void(int)> onPlay;
        std::function<void(int)> onPause;
        std::function<void(int)> onDelete;

        RecordingRow()
        {
            addAndMakeVisible(nameLabel);
            addAndMakeVisible(durationLabel);
            addAndMakeVisible(dateLabel);
            addAndMakeVisible(playButton);
            addAndMakeVisible(pauseButton);
            addAndMakeVisible(deleteButton);

            nameLabel.setColour(juce::Label::textColourId, juce::Colours::white);
            durationLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
            dateLabel.setColour(juce::Label::textColourId, juce::Colours::grey);

            nameLabel.setFont(juce::Font(13.0f, juce::Font::bold));
            durationLabel.setFont(juce::Font(11.0f));
            dateLabel.setFont(juce::Font(10.0f));

            // Default colours
            playButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2d8a4e));
            pauseButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff7a6020));
            deleteButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff7a2a2a));

            // Pause starts disabled — only enabled when this row is playing
            pauseButton.setEnabled(false);
            pauseButton.setAlpha(0.35f);

            playButton.onClick = [this] { if (onPlay)   onPlay(index);   };
            pauseButton.onClick = [this] { if (onPause)  onPause(index);  };
            deleteButton.onClick = [this] { if (onDelete) onDelete(index); };
        }

        // Called by RecordingsPanel::updateRowVisuals() whenever state changes
        void applyState(bool isThisRowPlaying, bool isThisRowPaused)
        {
            if (isThisRowPlaying)
            {
                // Row is actively playing — play button acts as STOP (dark green)
                playButton.setColour(juce::TextButton::buttonColourId,
                    juce::Colour(0xff145c28));
                playButton.setButtonText("[]");

                pauseButton.setEnabled(true);
                pauseButton.setAlpha(1.0f);
                pauseButton.setButtonText("||");
                pauseButton.setColour(juce::TextButton::buttonColourId,
                    juce::Colour(0xff7a6020));
            }
            else if (isThisRowPaused)
            {
                // Row is paused — play button still acts as STOP
                playButton.setColour(juce::TextButton::buttonColourId,
                    juce::Colour(0xff145c28));
                playButton.setButtonText("[]");

                // Pause button now shows resume arrow
                pauseButton.setEnabled(true);
                pauseButton.setAlpha(1.0f);
                pauseButton.setButtonText(">");
                pauseButton.setColour(juce::TextButton::buttonColourId,
                    juce::Colour(0xff2d8a4e));
            }
            else
            {
                // Idle — normal play button
                playButton.setColour(juce::TextButton::buttonColourId,
                    juce::Colour(0xff2d8a4e));
                playButton.setButtonText(">");

                pauseButton.setEnabled(false);
                pauseButton.setAlpha(0.35f);
                pauseButton.setButtonText("||");
                pauseButton.setColour(juce::TextButton::buttonColourId,
                    juce::Colour(0xff7a6020));
            }
        }

        void resized() override
        {
            auto b = getLocalBounds().reduced(6, 3);

            // Right side compact cluster: delete | pause | play
            deleteButton.setBounds(b.removeFromRight(28));
            b.removeFromRight(3);
            pauseButton.setBounds(b.removeFromRight(28));
            b.removeFromRight(3);
            playButton.setBounds(b.removeFromRight(28));
            b.removeFromRight(8);

            // Duration and time sit just left of the buttons — fixed widths
            durationLabel.setBounds(b.removeFromRight(42));
            b.removeFromRight(4);
            dateLabel.setBounds(b.removeFromRight(38));
            b.removeFromRight(8);

            // Name label gets the remaining left space
            nameLabel.setBounds(b);
        }
    };

    juce::OwnedArray<RecordingRow> rows;
    juce::Viewport                 viewport;
    juce::Component                rowContainer;

    // ── Transport chain ───────────────────────────────────────────────────────
    juce::AudioFormatManager                       playbackFormatManager;
    std::unique_ptr<juce::AudioFormatReaderSource> playbackReaderSource;
    std::unique_ptr<juce::AudioTransportSource>    playbackTransport;
    std::unique_ptr<juce::AudioSourcePlayer>       playbackPlayer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RecordingsPanel)
};

// ─────────────────────────────────────────────────────────────────────────────
// RecordButton
// ─────────────────────────────────────────────────────────────────────────────
class RecordButton : public juce::Component,
    public juce::Timer
{
public:
    RecordButton();
    ~RecordButton() override;

    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void timerCallback() override;

    void setRecordingManager(RecordingManager* mgr) { manager = mgr; }
    std::function<void()> onClick;

private:
    RecordingManager* manager{ nullptr };
    bool blinkState{ false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RecordButton)
};