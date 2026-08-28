/*
  ==============================================================================
    MainComponent.cpp
  ==============================================================================
*/

#include "MainComponent.h"

// ─────────────────────────────────────────────────────────────────────────────
// HelpPanel
// Full-feature instruction guide, shown in a CallOutBox when the ? button is
// pressed. Scrollable viewport so all content is reachable on any window size.
// ─────────────────────────────────────────────────────────────────────────────
class HelpPanel : public juce::Component
{
public:
    HelpPanel()
    {
        setSize(500, 560);

        addAndMakeVisible(viewport);
        viewport.setViewedComponent(&contentComp, false);
        viewport.setScrollBarsShown(true, false);

        buildContent();
    }

    void resized() override
    {
        viewport.setBounds(getLocalBounds().reduced(6));
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff1a1a1a));
        g.setColour(juce::Colours::grey.withAlpha(0.5f));
        g.drawRect(getLocalBounds(), 1);
    }

private:
    // ── Colours ───────────────────────────────────────────────────────────────
    static constexpr uint32_t kHeaderBg = 0xff1e3a5c;
    static constexpr uint32_t kAccentCyan = 0xff00d4ff;
    static constexpr uint32_t kAccentAmb = 0xffffaa00;
    static constexpr uint32_t kAccentGrn = 0xff4ade80;
    static constexpr uint32_t kAccentRed = 0xffff4444;
    static constexpr uint32_t kTextLight = 0xffe0e0e0;
    static constexpr uint32_t kTextDim = 0xff9a9a9a;

    // ── ContentComponent  -  renders the actual help text ───────────────────────
    struct ContentComponent : public juce::Component
    {
        struct Section { juce::String heading; uint32_t colour; std::vector<std::pair<juce::String, juce::String>> items; };
        std::vector<Section> sections;

        void paint(juce::Graphics& g) override
        {
            const int padX = 12;
            const int padY = 10;
            const int lineH = 18;
            const int gapAfterSection = 10;
            int y = padY;

            for (const auto& sec : sections)
            {
                // Section header bar
                g.setColour(juce::Colour(kHeaderBg));
                g.fillRoundedRectangle((float)padX, (float)y, (float)(getWidth() - padX * 2), 24.0f, 4.0f);
                g.setColour(juce::Colour(sec.colour));
                g.setFont(juce::Font(13.0f, juce::Font::bold));
                g.drawText(sec.heading, padX + 8, y + 2, getWidth() - padX * 2 - 8, 20,
                    juce::Justification::centredLeft);
                y += 30;

                for (const auto& [label, desc] : sec.items)
                {
                    // Label (bold, coloured)
                    g.setColour(juce::Colour(sec.colour).withAlpha(0.9f));
                    g.setFont(juce::Font(12.0f, juce::Font::bold));
                    g.drawText(label, padX + 8, y, 130, lineH,
                        juce::Justification::centredLeft);

                    // Description (normal weight, light grey), word-wrapped
                    g.setColour(juce::Colour(kTextLight));
                    g.setFont(juce::Font(11.5f));
                    juce::AttributedString as;
                    as.append(desc, juce::Font(11.5f), juce::Colour(kTextLight));
                    as.setWordWrap(juce::AttributedString::byWord);
                    juce::TextLayout tl;
                    tl.createLayout(as, (float)(getWidth() - padX * 2 - 150));
                    tl.draw(g, juce::Rectangle<float>(padX + 146.0f, (float)y,
                        (float)(getWidth() - padX * 2 - 150), (float)(lineH * 4)));

                    // Advance y by the number of wrapped lines
                    int wrappedLines = juce::jmax(1, (int)std::ceil(
                        (double)desc.length() / 55.0));
                    y += lineH * wrappedLines + 4;
                }
                y += gapAfterSection;
            }

            setSize(getWidth(), y + padY);
        }
    };

    void buildContent()
    {
        using Item = std::pair<juce::String, juce::String>;

        contentComp.sections =
        {
            {
                "  PLAYBACK",
                kAccentCyan,
                {
                    { "PLAY / PAUSE",   "Press the Play button on either deck to start playback. Press again to pause. The spinning turntable and waveform playhead both animate in real time." },
                    { "Load a Track",   "Drag an audio file from your file system directly onto a deck, or drag a track from the Music Library (bottom panel) onto either deck." },
                    { "SYNC",           "Press SYNC on one deck to match its speed to the other deck's current speed setting instantly." },
                }
            },
            {
                "  VOLUME & SPEED",
                kAccentCyan,
                {
                    { "Volume Fader",   "The vertical fader on each deck controls that deck's output level. The segmented LED bar shows level  -  yellow segments indicate high volume, red means clipping." },
                    { "Speed / Pitch",  "The narrow vertical slider adjusts playback speed from 0.5x (half tempo) to 1.5x (one and a half tempo). The BPM readout updates live. Centre position = normal speed." },
                    { "Crossfader",     "The horizontal slider at the centre blends between the two decks using a constant-power curve. Far left = Deck 1 only; centre = both equal; far right = Deck 2 only." },
                }
            },
            {
                "  HOT CUE BUTTONS  (1 - 8)",
                kAccentGrn,
                {
                    { "Set a Cue",      "Click any unlit numbered button (1-8) to mark the current playback position as a cue point. The button turns green." },
                    { "Trigger a Cue",  "Click a lit (green) button to instantly jump playback to that saved position." },
                    { "Reassign a Cue", "Hold SHIFT and click a lit button to move that cue point to the current playback position without affecting any other cues." },
                    { "CLEAR CUES",     "Press the CLEAR CUES button below the cue grid to delete all 8 cue points for the current track at once." },
                    { "Persistence",    "Cue points are saved automatically to disk per track. They reload the next time you load the same file." },
                }
            },
            {
                "  LOOP",
                kAccentAmb,
                {
                    { "Loop Toggle",    "Press the loop icon button to activate a loop starting from the current position. The waveform shows a blue overlay over the looped region. Press again to deactivate." },
                    { "Loop Size",      "Use the up/down arrows beside the loop size label to change the loop length: 1/4, 1/2, 1, 2, 4, 8, 16, or 32 beats." },
                }
            },
            {
                "  EQ & FILTER KNOBS",
                kAccentAmb,
                {
                    { "HIGH knob",      "Boosts or cuts high frequencies (5 kHz shelf). Centre = flat. Turn right to brighten, left to muffle the highs." },
                    { "MID knob",       "Boosts or cuts midrange frequencies (1 kHz peak). Centre = flat. Used to cut vocals or emphasise melodic elements." },
                    { "LOW knob",       "Boosts or cuts bass frequencies (200 Hz shelf). Centre = flat. Cut the bass before mixing in a new track." },
                    { "FILTER knob",    "DJ-style sweep filter. Centre = off (all frequencies pass). Turn left for a low-pass (bass only); turn right for a high-pass (treble only)." },
                    { "Persistence",    "EQ and filter settings save automatically per track and restore next time you load that file." },
                }
            },
            {
                "  FX (EFFECTS)",
                kAccentGrn,
                {
                    { "Open FX",        "Click the FX button on a deck to open the effects panel in a popup." },
                    { "Select Effect",  "Choose one of six effects from the grid: Echo, Slicer, Flanger, Reverb, Crusher, or Brake." },
                    { "XY Pad",         "After selecting an effect, an XY pad appears. Drag within the pad to control the effect in real time  -  X and Y axes control different parameters (shown as labels)." },
                    { "LOCK IN",        "Press LOCK IN to keep the effect active after you release the pad. Press LOCKED again to turn the effect off." },
                    { "Echo",           "Delay effect. X = delay time (50-800 ms). Y = feedback (repeat intensity)." },
                    { "Slicer",         "Rhythmic gate that chops the audio. X = chop speed. Y = duty cycle (how wide each gate opening is)." },
                    { "Flanger",        "Sweeping comb-filter effect (jet-plane sound). X = LFO rate. Y = sweep depth." },
                    { "Reverb",         "Room/hall reverb. X = room size. Y = wet/dry blend." },
                    { "Crusher",        "Lo-fi bit-crusher. X = bit depth (16 down to 4 bits). Y = sample-rate hold." },
                    { "Brake",          "Vinyl brake  -  fades the audio to silence over ~2 seconds as if the motor stopped. Activate the XY pad to trigger. Set FX to None to restore full volume." },
                }
            },
            {
                "  RECORDING",
                kAccentRed,
                {
                    { "Record button",  "The red circle button at the top centre opens the Recordings panel. It blinks while a recording is active." },
                    { "Start recording","Inside the Recordings panel, press Start New Audio Recording. The master mix (both decks + crossfader) is captured to a WAV file." },
                    { "Stop recording", "Press the Stop button that appears during recording, or press the main toggle button again. The file is saved automatically." },
                    { "Play back",      "Each completed recording appears as a row. Press > to play, || to pause, and X to delete." },
                    { "File location",  "Recordings are saved to your Documents folder under 'OtoDecks Recordings' as 'Recording N.wav'." },
                }
            },
            {
                "  RESTART",
                kAccentRed,
                {
                    { "RESTART button", "Resets both decks to their default state: stops playback, clears the loaded track, waveform, cue points, loop, EQ, and effects. The Music Library is not affected." },
                }
            },
            {
                "  MUSIC LIBRARY",
                kTextDim,
                {
                    { "Add Tracks",     "Click the Add Tracks button in the bottom panel and select one or more audio files (MP3, WAV, AIFF, FLAC, OGG, M4A)." },
                    { "Load to Deck",   "Drag any row from the library and drop it onto a deck waveform to load that track." },
                    { "Duration",       "The Duration column shows the length of each track." },
                    { "Persistence",    "Your library is saved automatically and reloads the next time you open OtoDecks." },
                    { "Clear Library",  "Press the Clear Library button to remove all tracks from the list (files on disk are not deleted)." },
                }
            },
        };

        int estimatedH = 0;
        for (const auto& sec : contentComp.sections)
            estimatedH += 30 + (int)sec.items.size() * 38 + 10;
        contentComp.setSize(488, estimatedH + 20);
        contentComp.repaint();
    }

    juce::Viewport    viewport;
    ContentComponent  contentComp;
};


MainComponent::MainComponent()
{
    setLookAndFeel(&djLookAndFeel);
    setSize(1200, 900);

    setAudioChannels(0, 2);
    formatManager.registerBasicFormats();

    addAndMakeVisible(deckGUI1);
    addAndMakeVisible(deckGUI2);
    addAndMakeVisible(playlistComponent);

    // Crossfader
    addAndMakeVisible(crossfader);
    crossfader.setSliderStyle(juce::Slider::LinearHorizontal);
    crossfader.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    crossfader.setRange(0.0, 1.0);
    crossfader.setValue(0.5);
    crossfader.addListener(this);

    // Link Waveforms
    deckGUI1.getWaveformDisplay()->onPositionChange = [this](double pos) { player1.setPositionRelative(pos); };
    deckGUI2.getWaveformDisplay()->onPositionChange = [this](double pos) { player2.setPositionRelative(pos); };

    // Link Overview strips (clicking overview also seeks)
    deckGUI1.getOverviewDisplay()->onPositionChange = [this](double pos) { player1.setPositionRelative(pos); };
    deckGUI2.getOverviewDisplay()->onPositionChange = [this](double pos) { player2.setPositionRelative(pos); };

    // Waveform Colors
    deckGUI1.getWaveformDisplay()->setCustomColour(juce::Colour(0xff00ffff));
    deckGUI2.getWaveformDisplay()->setCustomColour(juce::Colour(0xffffaa00));

    // Sync Logic
    deckGUI1.getSyncButton()->onClick = [this] {
        double targetSpeed = deckGUI2.getSpeedSlider()->getValue();
        deckGUI1.getSpeedSlider()->setValue(targetSpeed, juce::sendNotification);
        };
    deckGUI2.getSyncButton()->onClick = [this] {
        double targetSpeed = deckGUI1.getSpeedSlider()->getValue();
        deckGUI2.getSpeedSlider()->setValue(targetSpeed, juce::sendNotification);
        };

    // ── Record Button ────────────────────────────────────────────────────────
    addAndMakeVisible(recordButton);
    recordButton.setRecordingManager(&recordingManager);
    recordButton.onClick = [this] { openRecordingsPanel(); };

    // ── Restart Button ───────────────────────────────────────────────────────
    addAndMakeVisible(restartButton);
    restartButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1a3a5c));
    restartButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    restartButton.onClick = [this] { resetAll(); };

    // ── Help Button ──────────────────────────────────────────────────────────
    addAndMakeVisible(helpButton);
    helpButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2a3a2a));
    helpButton.setColour(juce::TextButton::textColourOffId, juce::Colours::lightgreen);
    helpButton.onClick = [this] { openHelpPanel(); };

    startTimerHz(30);
}

MainComponent::~MainComponent()
{
    stopTimer();
    if (recordingManager.isRecording())
        recordingManager.stopRecording();
    shutdownAudio();
    setLookAndFeel(nullptr);
}

// ── Reset ─────────────────────────────────────────────────────────────────────

void MainComponent::resetAll()
{
    // Reset both decks fully (sliders, EQ, cues, loop, playback, FX)
    deckGUI1.resetToDefaults();
    deckGUI2.resetToDefaults();

    // Reset crossfader to centre
    crossfader.setValue(0.5, juce::sendNotification);

    // NOTE: playlistComponent is intentionally NOT touched here
}

// ── Audio ─────────────────────────────────────────────────────────────────────

void MainComponent::prepareToPlay(int samplesPerBlockExpected, double sampleRate)
{
    mixerSource.removeAllInputs();

    player1.prepareToPlay(samplesPerBlockExpected, sampleRate);
    player2.prepareToPlay(samplesPerBlockExpected, sampleRate);

    mixerSource.addInputSource(&player1, false);
    mixerSource.addInputSource(&player2, false);
    mixerSource.prepareToPlay(samplesPerBlockExpected, sampleRate);

    // Tell recording manager the stream format
    recordingManager.prepareToRecord(sampleRate, 2);
}

void MainComponent::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill)
{
    mixerSource.getNextAudioBlock(bufferToFill);

    if (recordingManager.isRecording())
        recordingManager.processBlock(*bufferToFill.buffer,
            bufferToFill.startSample,
            bufferToFill.numSamples);
}

void MainComponent::releaseResources()
{
    player1.releaseResources();
    player2.releaseResources();
    mixerSource.releaseResources();
}

// ── Recording helpers ────────────────────────────────────────────────────────

void MainComponent::openRecordingsPanel()
{
    auto* panel = new RecordingsPanel(recordingManager, deviceManager);
    activePanel = panel;

    panel->onStartNewRecording = [this] { startRecording(); };

    panel->onStopRecording = [this]
        {
            juce::MessageManager::callAsync([this]
                {
                    recordingManager.stopRecording();
                    if (activePanel != nullptr)
                        activePanel->refresh();
                });
        };

    juce::CallOutBox::launchAsynchronously(
        std::unique_ptr<juce::Component>(panel),
        recordButton.getScreenBounds(),
        nullptr);
}

void MainComponent::startRecording()
{
    recordingManager.startRecording();
    if (activePanel != nullptr)
        activePanel->refresh();
}

void MainComponent::openHelpPanel()
{
    juce::CallOutBox::launchAsynchronously(
        std::make_unique<HelpPanel>(),
        helpButton.getScreenBounds(),
        nullptr);
}

// ── Paint ────────────────────────────────────────────────────────────────────

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff121212));
    auto area = getLocalBounds();
    int w = area.getWidth();

    g.setColour(juce::Colour(0xff2a2a2a));

    // Separator lines
    g.drawLine(0.0f, 383.0f, (float)w, 383.0f, 1.0f);

    float leftLineX = (float)(w / 2) - 160.0f;
    g.drawLine(leftLineX, 387.0f, leftLineX, 497.0f, 1.0f);
    float rightLineX = (float)(w / 2) + 160.0f;
    g.drawLine(rightLineX, 387.0f, rightLineX, 497.0f, 1.0f);

    g.drawLine(0.0f, 137.0f, (float)w, 137.0f, 1.0f);

    drawMasterVUMeter(g, w, 40);

    int leftDeckX = w / 8.6;
    int rightDeckX = (w * 5) / 5.62;
    int deckY = 260;
    int radius = 80;

    drawTurntable(g, leftDeckX, deckY, radius, &player1, true);
    drawTurntable(g, rightDeckX, deckY, radius, &player2, false);
}

void MainComponent::drawMasterVUMeter(juce::Graphics& g, int w, int yPos)
{
    int meterWidth = 300;
    int meterHeight = 24;
    int meterX = (w - meterWidth) / 2;

    g.setColour(juce::Colour(0xff1a1a1a));
    g.fillRoundedRectangle((float)meterX, (float)yPos, (float)meterWidth, (float)meterHeight, 3.0f);
    g.setColour(juce::Colours::grey.withAlpha(0.5f));
    g.drawRoundedRectangle((float)meterX, (float)yPos, (float)meterWidth, (float)meterHeight, 3.0f, 1.0f);

    float p1Level = juce::jlimit(0.0f, 1.0f, player1.getLevel() * 1.5f);
    float p2Level = juce::jlimit(0.0f, 1.0f, player2.getLevel() * 1.5f);

    int width1 = (int)(p1Level * meterWidth);
    int width2 = (int)(p2Level * meterWidth);

    juce::Rectangle<int> bar1(meterX + 2, yPos + 2, width1 - 4, (meterHeight / 2) - 3);
    if (p1Level > 0.9f) g.setColour(juce::Colours::red);
    else                g.setColour(juce::Colour(0xff00ffff));
    g.fillRect(bar1);

    juce::Rectangle<int> bar2(meterX + 2, yPos + (meterHeight / 2) + 1, width2 - 4, (meterHeight / 2) - 3);
    if (p2Level > 0.9f) g.setColour(juce::Colours::red);
    else                g.setColour(juce::Colour(0xffffaa00));
    g.fillRect(bar2);
}

static juce::String formatTimeSeconds(double seconds)
{
    if (seconds < 0) return "00:00";
    int mins = (int)(seconds / 60.0);
    int secs = (int)(std::fmod(seconds, 60.0));
    return juce::String::formatted("%02d:%02d", mins, secs);
}

void MainComponent::drawTurntable(juce::Graphics& g, int cx, int cy, int r,
    DJAudioPlayer* player, bool isLeft)
{
    double pos = player->getPositionRelative();
    double totalLen = player->getLengthInSeconds();
    double currentSecs = pos * totalLen;

    g.setColour(juce::Colour(0xff2a2a2a));
    g.fillEllipse(cx - r, cy - r, r * 2, r * 2);
    g.setColour(juce::Colour(0xff050505));
    g.fillEllipse(cx - r + 10, cy - r + 10, (r - 10) * 2, (r - 10) * 2);

    float angle = (float)(pos * juce::MathConstants<double>::twoPi * 10.0);
    float markerLen = r - 20;

    g.setColour(isLeft ? juce::Colour(0xff00ffff).withAlpha(0.7f)
        : juce::Colour(0xffffaa00).withAlpha(0.7f));
    juce::Path p;
    p.startNewSubPath(cx, cy);
    p.lineTo(cx + std::sin(angle) * markerLen, cy - std::cos(angle) * markerLen);
    g.strokePath(p, juce::PathStrokeType(4.0f));

    int hubSize = 44;
    g.setColour(juce::Colours::darkgrey.withAlpha(0.9f));
    g.fillEllipse(cx - hubSize / 2, cy - hubSize / 2, hubSize, hubSize);

    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(14.0f, juce::Font::bold));
    g.drawText(formatTimeSeconds(currentSecs),
        cx - 30, cy - 8, 60, 16, juce::Justification::centred, false);

    if (totalLen > 0)
    {
        g.setColour(juce::Colours::lightgrey);
        g.setFont(10.0f);
        g.drawText(formatTimeSeconds(totalLen),
            cx - 30, cy + 8, 60, 12, juce::Justification::centred, false);
    }
}

// ── Resized ───────────────────────────────────────────────────────────────────

void MainComponent::resized()
{
    auto area = getLocalBounds();
    int w = area.getWidth();
    int h = area.getHeight();

    deckGUI1.setBounds(area);
    deckGUI2.setBounds(area);

    int waveWidth = w * 0.63;
    int waveHeight = 120;
    int waveX = (w - waveWidth) / 2;
    int waveY1 = 140;
    int waveY2 = waveY1 + waveHeight + 1;

    deckGUI1.getWaveformDisplay()->setBounds(waveX, waveY1, waveWidth, waveHeight);
    deckGUI2.getWaveformDisplay()->setBounds(waveX, waveY2, waveWidth, waveHeight);

    // ── Overview strips  -  full width edge-to-edge, split at centre ───────────
    const int ovY = 95;
    const int ovH = 38;
    const int ovGap = 105;
    int halfW = w / 2 - ovGap / 2;

    deckGUI1.getOverviewDisplay()->setBounds(0, ovY, halfW, ovH);
    deckGUI2.getOverviewDisplay()->setBounds(w / 2 + ovGap / 2, ovY, halfW, ovH);

    // ── Top bar buttons ───────────────────────────────────────────────────────
    int meterX = (w - 300) / 2;
    int recordBtnX = meterX + 300 + 10;
    recordButton.setBounds(recordBtnX, 33, 34, 34);
    helpButton.setBounds(recordBtnX + 38, 33, 34, 34);

    // Restart button  -  sits to the LEFT of the VU meter
    restartButton.setBounds(meterX - 90, 33, 80, 34);

    int lCx = w / 6;
    int rCx = (w * 5) / 6;
    int controlsY = 340;

    // LEFT DECK controls
    deckGUI1.getTitleLabel()->setBounds(lCx - 235, controlsY - 200, 200, 24);
    deckGUI1.getBpmLabel()->setBounds(lCx - 213, controlsY - 175, 60, 20);
    deckGUI1.getBpmPercentLabel()->setBounds(lCx - 213, controlsY - 155, 60, 20);
    deckGUI1.getVolSlider()->setBounds((w / 2) - 140, 385, 50, 110);
    deckGUI1.getSpeedSlider()->setBounds(lCx - 203, controlsY - 143, 30, 140);
    deckGUI1.getSyncButton()->setBounds(lCx - 213, controlsY, 60, 30);
    deckGUI1.getPlayPauseButton()->setBounds(lCx - 193, controlsY + 57, 80, 30);

    int cueStartX = lCx - 193 + 90;
    int cueY = controlsY + 58;
    int cueButtonWidth = 30, cueButtonHeight = 30, cueGap = 3;

    for (int i = 0; i < 4; ++i)
        deckGUI1.getHotCueButton(i)->setBounds(cueStartX + i * (cueButtonWidth + cueGap), cueY, cueButtonWidth, cueButtonHeight);
    for (int i = 4; i < 8; ++i)
        deckGUI1.getHotCueButton(i)->setBounds(cueStartX + (i - 4) * (cueButtonWidth + cueGap), cueY + cueButtonHeight + cueGap, cueButtonWidth, cueButtonHeight);
    deckGUI1.getClearCuesButton()->setBounds(cueStartX, cueY + (cueButtonHeight + cueGap) * 2 + 5, 100, 24);

    // RIGHT DECK controls
    deckGUI2.getTitleLabel()->setBounds(rCx + 30, controlsY - 200, 200, 24);
    deckGUI2.getBpmLabel()->setBounds(rCx + 150, controlsY - 175, 60, 20);
    deckGUI2.getBpmPercentLabel()->setBounds(rCx + 150, controlsY - 155, 60, 20);
    deckGUI2.getVolSlider()->setBounds((w / 2) + 90, 385, 50, 110);
    deckGUI2.getSpeedSlider()->setBounds(rCx + 169, controlsY - 143, 30, 140);
    deckGUI2.getSyncButton()->setBounds(rCx + 154, controlsY, 60, 30);
    deckGUI2.getPlayPauseButton()->setBounds(rCx + 115, controlsY + 57, 80, 30);

    int cueStartX2 = rCx + 115 - (4 * (cueButtonWidth + cueGap)) - 10;
    int cueY2 = controlsY + 58;

    for (int i = 0; i < 4; ++i)
        deckGUI2.getHotCueButton(i)->setBounds(cueStartX2 + i * (cueButtonWidth + cueGap), cueY2, cueButtonWidth, cueButtonHeight);
    for (int i = 4; i < 8; ++i)
        deckGUI2.getHotCueButton(i)->setBounds(cueStartX2 + (i - 4) * (cueButtonWidth + cueGap), cueY2 + cueButtonHeight + cueGap, cueButtonWidth, cueButtonHeight);
    deckGUI2.getClearCuesButton()->setBounds(cueStartX2 + 30, cueY2 + (cueButtonHeight + cueGap) * 2 + 5, 100, 24);

    crossfader.setBounds(w / 2 - 100, 415, 200, 30);

    int playlistY = 500;
    playlistComponent.setBounds(0, playlistY, w, h - playlistY);
}

// ── Listeners ─────────────────────────────────────────────────────────────────

void MainComponent::sliderValueChanged(juce::Slider* slider)
{
    if (slider == &crossfader)
    {
        double val = slider->getValue();
        player1.setCrossfadeGain(std::cos(val * juce::MathConstants<double>::halfPi));
        player2.setCrossfadeGain(std::sin(val * juce::MathConstants<double>::halfPi));
    }
}

void MainComponent::timerCallback()
{
    // Always repaint  -  ensures reset clears turntable/VU meter immediately
    // even when neither player is playing.
    repaint();
}