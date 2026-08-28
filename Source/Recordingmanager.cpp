/*
  ==============================================================================
    RecordingManager.cpp
  ==============================================================================
*/

#include "RecordingManager.h"

// ─────────────────────────────────────────────────────────────────────────────
// RecordingManager
// ─────────────────────────────────────────────────────────────────────────────

RecordingManager::RecordingManager()
{
    writerThread.startThread();
}

RecordingManager::~RecordingManager()
{
    if (recording.load())
        stopRecording();
    writerThread.stopThread(1000);
}

void RecordingManager::prepareToRecord(double sampleRate, int numChannels)
{
    currentSampleRate = sampleRate;
    currentNumChannels = numChannels;
}

juce::File RecordingManager::getRecordingsDirectory() const
{
    juce::File dir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
        .getChildFile("OtoDecks Recordings");
    if (!dir.exists())
        dir.createDirectory();
    return dir;
}

bool RecordingManager::startRecording()
{
    if (recording.load()) return false;

    juce::File recordingsDir = getRecordingsDirectory();
    juce::String fileName = "Recording " + juce::String(recordingIndex) + ".wav";
    juce::File outFile = recordingsDir.getChildFile(fileName);

    int suffix = 2;
    while (outFile.existsAsFile())
        outFile = recordingsDir.getChildFile("Recording " + juce::String(recordingIndex)
            + " (" + juce::String(suffix++) + ").wav");

    juce::FileOutputStream* fileStream = outFile.createOutputStream().release();
    if (!fileStream) return false;

    auto* rawWriter = wavFormat.createWriterFor(fileStream, currentSampleRate,
        (unsigned int)currentNumChannels, 16, {}, 0);
    if (!rawWriter) { delete fileStream; return false; }

    threadedWriter.reset(new juce::AudioFormatWriter::ThreadedWriter(rawWriter, writerThread, 32768));
    samplesWritten.store(0);
    recordingStartTime = juce::Time::getCurrentTime();
    currentOutputFile = outFile;
    recording.store(true);
    return true;
}

RecordingInfo RecordingManager::stopRecording()
{
    recording.store(false);

    while (isProcessingBlock.load())
        juce::Thread::yield();

    threadedWriter.reset();

    double       duration = samplesWritten.load() / currentSampleRate;
    juce::String name = "Recording " + juce::String(recordingIndex);
    RecordingInfo info(currentOutputFile, name, duration, recordingStartTime);
    recordings.push_back(info);
    ++recordingIndex;
    return info;
}

void RecordingManager::processBlock(const juce::AudioBuffer<float>& buffer,
    int startSample, int numSamples)
{
    isProcessingBlock.store(true);

    if (recording.load() && threadedWriter != nullptr)
    {
        const float* ptrs[2] = { nullptr, nullptr };
        int numChannelsToCopy = juce::jmin(2, buffer.getNumChannels());

        for (int i = 0; i < numChannelsToCopy; ++i)
            ptrs[i] = buffer.getReadPointer(i, startSample);

        if (numChannelsToCopy == 1 && currentNumChannels == 2)
            ptrs[1] = ptrs[0];

        threadedWriter->write(ptrs, numSamples);
        samplesWritten.fetch_add(numSamples);
    }

    isProcessingBlock.store(false);
}

double RecordingManager::getElapsedSeconds() const noexcept
{
    if (!recording.load()) return 0.0;
    return samplesWritten.load() / currentSampleRate;
}

void RecordingManager::deleteRecording(int index)
{
    if (index < 0 || index >= (int)recordings.size()) return;
    recordings[index].file.deleteFile();
    recordings.erase(recordings.begin() + index);
}

// ─────────────────────────────────────────────────────────────────────────────
// RecordingsPanel
// ─────────────────────────────────────────────────────────────────────────────

RecordingsPanel::RecordingsPanel(RecordingManager& mgr,
    juce::AudioDeviceManager& sharedDevMgr)
    : manager(mgr), sharedDeviceManager(sharedDevMgr)
{
    setSize(430, 500);

    addAndMakeVisible(startButton);
    startButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffE53935));
    startButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    startButton.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    startButton.onClick = [this]
        {
            if (manager.isRecording()) { if (onStopRecording)    onStopRecording(); }
            else { if (onStartNewRecording) onStartNewRecording(); }
        };

    addAndMakeVisible(statusLabel);
    statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xffff6666));
    statusLabel.setFont(juce::Font(12.0f, juce::Font::bold));
    statusLabel.setJustificationType(juce::Justification::centred);
    statusLabel.setVisible(false);

    addAndMakeVisible(stopButton);
    stopButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff7a2a2a));
    stopButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    stopButton.setVisible(false);
    stopButton.onClick = [this] { if (onStopRecording) onStopRecording(); };

    addAndMakeVisible(viewport);
    viewport.setViewedComponent(&rowContainer, false);
    viewport.setScrollBarsShown(true, false);

    playbackFormatManager.registerBasicFormats();

    rebuildRecordingRows();
    startTimerHz(4);
}

RecordingsPanel::~RecordingsPanel()
{
    stopTimer();
    stopPlayback();   // cleans up transport chain safely before destruction
}

// ── Playback helpers ──────────────────────────────────────────────────────────

/*  stopPlayback()
    Fully tears down the transport chain.
    IMPORTANT: we must call transportSource.stop() BEFORE removing the audio
    callback, otherwise the device thread may still be inside the player when
    we reset the unique_ptrs, causing the phantom-playback / crash bug.         */
void RecordingsPanel::stopPlayback()
{
    // 1. Tell the transport to stop producing audio immediately
    if (playbackTransport != nullptr)
        playbackTransport->stop();

    // 2. Remove the callback — device thread will no longer call into our player
    if (playbackPlayer != nullptr)
        sharedDeviceManager.removeAudioCallback(playbackPlayer.get());

    // 3. Now it is safe to destroy everything
    playbackPlayer.reset();
    playbackTransport.reset();
    playbackReaderSource.reset();

    playingIndex = -1;
    transportPaused = false;
}

/*  pauseResume()
    Toggles pause on the currently loaded transport.
    Does NOT touch the audio callback registration — the player stays registered
    but the transport simply stops/starts producing samples.                     */
void RecordingsPanel::pauseResume()
{
    if (playbackTransport == nullptr) return;

    if (transportPaused)
    {
        playbackTransport->start();
        transportPaused = false;
    }
    else
    {
        playbackTransport->stop();
        transportPaused = true;
    }

    updateRowVisuals();
}

/*  playRecording(index)
    If the same row is clicked while it is already playing/paused → stop it.
    If a different row is clicked → stop any existing playback, load the new
    file, build a fresh transport chain, and start.                             */
void RecordingsPanel::playRecording(int index)
{
    if (index < 0 || index >= (int)manager.getRecordings().size()) return;

    // Clicking the active row's play/stop button → stop
    if (index == playingIndex)
    {
        stopPlayback();
        updateRowVisuals();
        return;
    }

    // Switching to a different track — stop the old one cleanly first
    stopPlayback();   // resets playingIndex to -1

    const juce::File& file = manager.getRecordings()[index].file;
    if (!file.existsAsFile()) return;

    juce::AudioFormatReader* reader = playbackFormatManager.createReaderFor(file);
    if (!reader) return;

    // Build transport chain
    playbackReaderSource.reset(new juce::AudioFormatReaderSource(reader, true));
    playbackTransport.reset(new juce::AudioTransportSource());
    playbackTransport->setSource(playbackReaderSource.get(), 0, nullptr, reader->sampleRate);

    playbackPlayer.reset(new juce::AudioSourcePlayer());
    playbackPlayer->setSource(playbackTransport.get());

    // Register callback THEN start — guarantees no audio produced before ready
    sharedDeviceManager.addAudioCallback(playbackPlayer.get());
    playbackTransport->start();

    playingIndex = index;
    transportPaused = false;

    updateRowVisuals();
}

// ── Row visual state ──────────────────────────────────────────────────────────

void RecordingsPanel::updateRowVisuals()
{
    for (auto* row : rows)
    {
        bool playing = (row->index == playingIndex) && !transportPaused;
        bool paused = (row->index == playingIndex) && transportPaused;
        row->applyState(playing, paused);
    }
}

// ── Timer ─────────────────────────────────────────────────────────────────────

void RecordingsPanel::timerCallback()
{
    // Update recording status label
    if (manager.isRecording())
    {
        double e = manager.getElapsedSeconds();
        int mins = (int)(e / 60.0);
        int secs = (int)std::fmod(e, 60.0);
        statusLabel.setText(juce::String::formatted("REC  %02d:%02d", mins, secs),
            juce::dontSendNotification);
        statusLabel.setVisible(true);
        stopButton.setVisible(true);
        startButton.setButtonText("Recording...");
        startButton.setEnabled(false);
        startButton.setAlpha(0.5f);
    }
    else
    {
        statusLabel.setVisible(false);
        stopButton.setVisible(false);
        startButton.setButtonText("Start New Audio Recording");
        startButton.setEnabled(true);
        startButton.setAlpha(1.0f);
    }

    // Auto-reset when playback finishes naturally (transport ran to end)
    if (playingIndex >= 0 && !transportPaused
        && playbackTransport != nullptr
        && !playbackTransport->isPlaying())
    {
        stopPlayback();
        updateRowVisuals();
    }
}

void RecordingsPanel::refresh()
{
    rebuildRecordingRows();
    resized();
}

// ── Row construction ──────────────────────────────────────────────────────────

void RecordingsPanel::rebuildRecordingRows()
{
    rows.clear();
    rowContainer.removeAllChildren();

    const auto& recs = manager.getRecordings();

    // Show newest first
    for (int i = (int)recs.size() - 1; i >= 0; --i)
    {
        auto* row = rows.add(new RecordingRow());
        row->index = i;
        row->nameLabel.setText(recs[i].displayName, juce::dontSendNotification);
        row->durationLabel.setText(formatDuration(recs[i].durationSeconds),
            juce::dontSendNotification);
        row->dateLabel.setText(recs[i].timestamp.formatted("%H:%M"),
            juce::dontSendNotification);

        row->onPlay = [this](int idx) { playRecording(idx); };

        row->onPause = [this](int idx)
            {
                if (idx == playingIndex) pauseResume();
            };

        row->onDelete = [this](int idx)
            {
                juce::MessageManager::callAsync([this, idx]()
                    {
                        if (idx == playingIndex) stopPlayback();
                        manager.deleteRecording(idx);
                        rebuildRecordingRows();
                        resized();
                    });
            };

        rowContainer.addAndMakeVisible(row);
    }

    updateRowVisuals();
    resized();
}

juce::String RecordingsPanel::formatDuration(double secs) const
{
    int mins = (int)(secs / 60.0);
    int remainingSecs = (int)std::fmod(secs, 60.0);
    return juce::String::formatted("%d:%02d", mins, remainingSecs);
}

// ── Paint / Resized ───────────────────────────────────────────────────────────

void RecordingsPanel::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1a1a1a));
    g.setColour(juce::Colours::grey);
    g.drawRect(getLocalBounds(), 1);

    if (rows.isEmpty() && !manager.isRecording())
    {
        g.setColour(juce::Colours::white.withAlpha(0.7f));
        g.setFont(juce::Font(16.0f, juce::Font::bold));
        g.drawText("No Recordings", 0, getHeight() / 2 - 30, getWidth(), 30,
            juce::Justification::centred);

        g.setColour(juce::Colours::grey);
        g.setFont(14.0f);
        g.drawText("Any recordings you have made are shown here.",
            20, getHeight() / 2, getWidth() - 40, 40,
            juce::Justification::centred);
    }
}

void RecordingsPanel::resized()
{
    auto area = getLocalBounds().reduced(10);

    startButton.setBounds(area.removeFromTop(44));
    area.removeFromTop(2);

    if (manager.isRecording())
    {
        auto statusRow = area.removeFromTop(30);
        statusLabel.setBounds(statusRow.removeFromLeft(statusRow.getWidth() - 70));
        stopButton.setBounds(statusRow);
        area.removeFromTop(4);
    }

    area.removeFromTop(6);
    viewport.setBounds(area);

    int rowH = 44;
    rowContainer.setBounds(0, 0, area.getWidth(), rows.size() * rowH);
    for (int i = 0; i < rows.size(); ++i)
        rows[i]->setBounds(0, i * rowH, area.getWidth(), rowH - 2);
}

// ─────────────────────────────────────────────────────────────────────────────
// RecordButton
// ─────────────────────────────────────────────────────────────────────────────

RecordButton::RecordButton()
{
    setSize(34, 34);
    startTimerHz(2);
}

RecordButton::~RecordButton()
{
    stopTimer();
}

void RecordButton::timerCallback()
{
    blinkState = !blinkState;
    if (manager && manager->isRecording())
        repaint();
}

void RecordButton::paint(juce::Graphics& g)
{
    auto  bounds = getLocalBounds().toFloat().reduced(2.0f);
    float cx = bounds.getCentreX();
    float cy = bounds.getCentreY();
    float r = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;

    g.setColour(juce::Colours::darkgrey.withAlpha(0.6f));
    g.fillEllipse(cx - r, cy - r, r * 2.0f, r * 2.0f);

    float innerR = r * 0.60f;
    bool  isRec = (manager != nullptr && manager->isRecording());

    if (isRec)
        g.setColour(blinkState ? juce::Colour(0xffff2222) : juce::Colour(0xff880000));
    else
        g.setColour(juce::Colour(0xffcc1111));

    g.fillEllipse(cx - innerR, cy - innerR, innerR * 2.0f, innerR * 2.0f);

    if (isMouseOver())
    {
        g.setColour(juce::Colours::white.withAlpha(0.4f));
        g.drawEllipse(cx - r, cy - r, r * 2.0f, r * 2.0f, 1.5f);
    }
}

void RecordButton::mouseDown(const juce::MouseEvent&)
{
    if (onClick) onClick();
}