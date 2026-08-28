/*
  ==============================================================================
    DeckGUI.cpp
  ==============================================================================
*/

#include "DeckGUI.h"

DeckGUI::DeckGUI(DJAudioPlayer* _player,
    juce::AudioFormatManager& formatManagerToUse,
    juce::AudioThumbnailCache& cacheToUse,
    bool _isLeftDeck)
    : player(_player),
    isLeftDeck(_isLeftDeck),
    waveformDisplay(formatManagerToUse, cacheToUse),
    overviewDisplay(formatManagerToUse, cacheToUse, _isLeftDeck)
{
    addAndMakeVisible(playPauseButton);
    addAndMakeVisible(syncButton);
    addAndMakeVisible(volSlider);
    addAndMakeVisible(speedSlider);
    addAndMakeVisible(bpmLabel);
    addAndMakeVisible(bpmPercentLabel);
    addAndMakeVisible(waveformDisplay);
    addAndMakeVisible(overviewDisplay);
    addAndMakeVisible(titleLabel);
    titleLabel.setText("Drag song here", juce::dontSendNotification);
    titleLabel.setJustificationType(juce::Justification::centred);
    titleLabel.setFont(juce::Font(16.0f, juce::Font::bold));
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::grey);

    playPauseButton.setName("PLAY_PAUSE");
    playPauseButton.setClickingTogglesState(true);
    playPauseButton.addListener(this);

    syncButton.setName("SYNC");
    syncButton.setButtonText("SYNC");

    for (int i = 0; i < 8; ++i)
    {
        addAndMakeVisible(hotCueButtons[i]);
        hotCueButtons[i].setButtonText(juce::String(i + 1));
        hotCueButtons[i].addListener(this);
        hotCueButtons[i].setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1a1a1a));
        hotCueButtons[i].setColour(juce::TextButton::textColourOffId, juce::Colours::grey);
    }

    addAndMakeVisible(clearCuesButton);
    clearCuesButton.addListener(this);
    clearCuesButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff004749));

    addAndMakeVisible(loopToggleButton);
    loopToggleButton.setName("LOOP_TOGGLE");
    loopToggleButton.setClickingTogglesState(true);
    loopToggleButton.addListener(this);

    addAndMakeVisible(loopSizeUpButton);
    loopSizeUpButton.setName("LOOP_UP");
    loopSizeUpButton.addListener(this);

    addAndMakeVisible(loopSizeDownButton);
    loopSizeDownButton.setName("LOOP_DOWN");
    loopSizeDownButton.addListener(this);

    addAndMakeVisible(loopSizeLabel);
    loopSizeLabel.setJustificationType(juce::Justification::centred);
    loopSizeLabel.setFont(juce::Font(14.0f, juce::Font::bold));
    loopSizeLabel.setColour(juce::Label::textColourId, juce::Colours::white);

    updateLoopDisplay();

    auto setupEqKnob = [this](juce::Slider& slider, juce::Label& label, juce::String name, juce::String id) {
        addAndMakeVisible(slider);
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);

        if (name == "FILTER") {
            slider.setRange(0.0, 1.0, 0.01);
            slider.setValue(0.5);
        }
        else {
            slider.setRange(0.2, 3.0, 0.01);
            slider.setValue(1.0);
        }

        slider.setComponentID(id);
        slider.addListener(this);

        addAndMakeVisible(label);
        label.setText(name, juce::dontSendNotification);
        label.setFont(12.0f);
        label.setJustificationType(juce::Justification::centred);
        label.attachToComponent(&slider, false);
        };

    setupEqKnob(highEqSlider, highEqLabel, "HIGH", "HighEq");
    setupEqKnob(midEqSlider, midEqLabel, "MID", "MidEq");
    setupEqKnob(lowEqSlider, lowEqLabel, "LOW", "LowEq");
    setupEqKnob(filterSlider, filterLabel, "FILTER", "FilterEq");

    // FX button — activate effect while dragging / locked
    addAndMakeVisible(fxButton);
    fxButton.onFXChange = [this](juce::String fxName, float x, float y)
        {
            DJAudioPlayer::FXType type = DJAudioPlayer::FXType::None;
            if (fxName == "Echo")    type = DJAudioPlayer::FXType::Echo;
            else if (fxName == "Slicer")  type = DJAudioPlayer::FXType::Slicer;
            else if (fxName == "Flanger") type = DJAudioPlayer::FXType::Flanger;
            else if (fxName == "Reverb")  type = DJAudioPlayer::FXType::Reverb;
            else if (fxName == "Crusher") type = DJAudioPlayer::FXType::Crusher;
            else if (fxName == "Brake")   type = DJAudioPlayer::FXType::Brake;
            player->setFXType(type);
            player->updateFXParameters(x, y);
        };

    // FX button — deactivate when released without locking
    fxButton.onFXOff = [this]()
        {
            player->setFXType(DJAudioPlayer::FXType::None);
        };

    volSlider.addListener(this);
    volSlider.setRange(0.0, 1.0);
    volSlider.setValue(0.5);
    volSlider.setSliderStyle(juce::Slider::LinearVertical);
    volSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    volSlider.setComponentID("VolSlider");

    speedSlider.addListener(this);
    speedSlider.setRange(0.5, 1.5);
    speedSlider.setValue(1.0);
    speedSlider.setSkewFactorFromMidPoint(1.0);
    speedSlider.setSliderStyle(juce::Slider::LinearVertical);
    speedSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    speedSlider.setComponentID("SpeedSlider");
    speedSlider.onDragStart = [this] { isDraggingSpeed = true; };
    speedSlider.onDragEnd = [this] { isDraggingSpeed = false; };

    bpmLabel.setText("120.0", juce::dontSendNotification);
    bpmLabel.setJustificationType(juce::Justification::centred);
    bpmLabel.setFont(juce::Font(18.0f, juce::Font::bold));
    bpmLabel.setColour(juce::Label::textColourId, juce::Colours::white);

    bpmPercentLabel.setText("+0.0%", juce::dontSendNotification);
    bpmPercentLabel.setJustificationType(juce::Justification::centred);
    bpmPercentLabel.setFont(juce::Font(11.0f));
    bpmPercentLabel.setColour(juce::Label::textColourId, juce::Colours::grey);

    setInterceptsMouseClicks(false, true);
    startTimer(33);
}

DeckGUI::~DeckGUI() { stopTimer(); }

void DeckGUI::resetToDefaults()
{
    player->stop();
    player->setPosition(0.0);
    player->loadURL(juce::URL{});   // also zeros RMS level inside DJAudioPlayer

    playPauseButton.setToggleState(false, juce::dontSendNotification);

    // Wipe waveform, reset position to 0 so turntable shows 00:00 / no duration
    waveformDisplay.loadURL(juce::URL{});
    waveformDisplay.setPositionRelative(0.0);
    overviewDisplay.clearTrack();
    titleLabel.setText("Drag song here", juce::dontSendNotification);
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::grey);

    volSlider.setValue(0.5, juce::sendNotification);
    speedSlider.setValue(1.0, juce::sendNotification);

    highEqSlider.setValue(1.0, juce::sendNotification);
    midEqSlider.setValue(1.0, juce::sendNotification);
    lowEqSlider.setValue(1.0, juce::sendNotification);
    filterSlider.setValue(0.5, juce::sendNotification);

    loopToggleButton.setToggleState(false, juce::dontSendNotification);
    player->stopLoop();
    waveformDisplay.clearLoopRegion();
    currentLoopIndex = 4;
    updateLoopDisplay();

    player->clearAllHotCues();
    waveformDisplay.clearAllHotCueMarkers();
    updateHotCueButtons();

    player->setFXType(DJAudioPlayer::FXType::None);

    // Reset BPM label to default (no track loaded)
    bpmLabel.setText("120.0", juce::dontSendNotification);
    bpmPercentLabel.setText("+0.0%", juce::dontSendNotification);
}

void DeckGUI::paint(juce::Graphics& g)
{
    if (loopToggleButton.isVisible())
    {
        auto iconBounds = loopToggleButton.getBounds();
        auto labelBounds = loopSizeLabel.getBounds();
        auto loopBox = iconBounds.getUnion(labelBounds);
        g.setColour(juce::Colour(0xff111111));
        g.fillRect(loopBox);
        g.setColour(juce::Colour(0xff333333));
        g.drawRect(loopBox, 1);
    }
}

void DeckGUI::resized()
{
    auto area = getLocalBounds();
    int w = area.getWidth();

    int lCx = w / 6;
    int rCx = (w * 5) / 6;
    int controlsY = 340;

    int playButtonY = controlsY + 57;
    int playButtonHeight = 30;

    int startX = 0;
    int startY = playButtonY + playButtonHeight + 5;

    if (isLeftDeck) startX = lCx - 193;
    else            startX = rCx + 115;

    int rowHeight = 32;
    int iconWidth = 32;
    int labelWidth = 30;
    int arrowWidth = 20;
    int arrowHeight = rowHeight / 2;

    loopToggleButton.setBounds(startX, startY, iconWidth, rowHeight);
    loopSizeLabel.setBounds(startX + iconWidth, startY, labelWidth, rowHeight);
    loopSizeUpButton.setBounds(startX + iconWidth + labelWidth, startY, arrowWidth, arrowHeight);
    loopSizeDownButton.setBounds(startX + iconWidth + labelWidth, startY + arrowHeight, arrowWidth, rowHeight - arrowHeight);

    if (isLeftDeck)
    {
        bpmLabel.setBounds(lCx - 200, 10, 100, 30);
        bpmPercentLabel.setBounds(lCx - 200, 35, 100, 20);
        titleLabel.setBounds(lCx - 235, 200, 200, 24);
    }
    else
    {
        bpmLabel.setBounds(rCx + 100, 10, 100, 30);
        bpmPercentLabel.setBounds(rCx + 100, 35, 100, 20);
        titleLabel.setBounds(rCx + 30, 200, 200, 24);
    }

    if (isLeftDeck)
    {
        volSlider.setBounds((w / 2) - 140, 385, 50, 110);
        speedSlider.setBounds(lCx - 203, controlsY - 143, 30, 140);
        syncButton.setBounds(lCx - 213, controlsY, 60, 30);
        playPauseButton.setBounds(lCx - 193, controlsY + 57, 80, 30);
    }
    else
    {
        volSlider.setBounds((w / 2) + 90, 385, 50, 110);
        speedSlider.setBounds(rCx + 169, controlsY - 143, 30, 140);
        syncButton.setBounds(rCx + 154, controlsY, 60, 30);
        playPauseButton.setBounds(rCx + 115, controlsY + 57, 80, 30);
    }

    int cueButtonWidth = 30;
    int cueButtonHeight = 30;
    int cueGap = 3;

    if (isLeftDeck)
    {
        int cueStartX = lCx - 193 + 90;
        int cueY = controlsY + 58;
        for (int i = 0; i < 4; ++i) hotCueButtons[i].setBounds(cueStartX + i * (cueButtonWidth + cueGap), cueY, cueButtonWidth, cueButtonHeight);
        for (int i = 4; i < 8; ++i) hotCueButtons[i].setBounds(cueStartX + (i - 4) * (cueButtonWidth + cueGap), cueY + cueButtonHeight + cueGap, cueButtonWidth, cueButtonHeight);
        clearCuesButton.setBounds(cueStartX, cueY + (cueButtonHeight + cueGap) * 2 + 5, 100, 24);
    }
    else
    {
        int cueStartX2 = rCx + 115 - (4 * (cueButtonWidth + cueGap)) - 10;
        int cueY2 = controlsY + 58;
        for (int i = 0; i < 4; ++i) hotCueButtons[i].setBounds(cueStartX2 + i * (cueButtonWidth + cueGap), cueY2, cueButtonWidth, cueButtonHeight);
        for (int i = 4; i < 8; ++i) hotCueButtons[i].setBounds(cueStartX2 + (i - 4) * (cueButtonWidth + cueGap), cueY2 + cueButtonHeight + cueGap, cueButtonWidth, cueButtonHeight);
        clearCuesButton.setBounds(cueStartX2 + 30, cueY2 + (cueButtonHeight + cueGap) * 2 + 5, 100, 24);
    }

    int eqY = controlsY + 78;
    int fxY = eqY - 15;
    int knobSize = 40; int knobGap = 10;
    int fxButtonWidth = 50; int fxButtonHeight = 50;
    int eqX = 0; int fxX = 0;

    if (isLeftDeck)
    {
        eqX = lCx + 45;
        fxX = eqX + (knobSize + knobGap) * 4;

        lowEqSlider.setBounds(eqX, eqY, knobSize, knobSize);
        midEqSlider.setBounds(eqX + knobSize + knobGap, eqY, knobSize, knobSize);
        highEqSlider.setBounds(eqX + (knobSize + knobGap) * 2, eqY, knobSize, knobSize);
        filterSlider.setBounds(eqX + (knobSize + knobGap) * 3, eqY, knobSize, knobSize);
    }
    else
    {
        eqX = rCx - 227;
        fxX = eqX + (knobSize + knobGap) * 4 - 270;

        filterSlider.setBounds(eqX, eqY, knobSize, knobSize);
        highEqSlider.setBounds(eqX + knobSize + knobGap, eqY, knobSize, knobSize);
        midEqSlider.setBounds(eqX + (knobSize + knobGap) * 2, eqY, knobSize, knobSize);
        lowEqSlider.setBounds(eqX + (knobSize + knobGap) * 3, eqY, knobSize, knobSize);
    }

    fxButton.setBounds(fxX, fxY, fxButtonWidth, fxButtonHeight);
}

void DeckGUI::buttonClicked(juce::Button* button)
{
    if (button == &playPauseButton)
    {
        bool shouldPlay = !player->isPlaying();
        if (shouldPlay) { player->start(); playPauseButton.setToggleState(true, juce::dontSendNotification); }
        else { player->stop(); playPauseButton.setToggleState(false, juce::dontSendNotification); }
        ignoreTimerUpdates = true;
        juce::Timer::callAfterDelay(100, [this]() { ignoreTimerUpdates = false; });
    }
    else if (button == &clearCuesButton)
    {
        player->clearAllHotCues();
        waveformDisplay.clearAllHotCueMarkers();
        updateHotCueButtons();
        if (player->getCurrentTrackPath().isNotEmpty()) saveHotCuesForTrack(player->getCurrentTrackPath());
    }
    else if (button == &loopToggleButton)
    {
        if (loopToggleButton.getToggleState())
        {
            double beats = loopSizes[currentLoopIndex];
            player->startLoop(beats, 120.0 * speedSlider.getValue());
            waveformDisplay.setLoopRegion(player->getLoopStartSeconds(),
                player->getLoopEndSeconds());
        }
        else
        {
            player->stopLoop();
            waveformDisplay.clearLoopRegion();
        }
    }
    else if (button == &loopSizeUpButton)
    {
        if (currentLoopIndex < (int)loopSizes.size() - 1)
        {
            currentLoopIndex++;
            updateLoopDisplay();
            player->updateLoopSize(loopSizes[currentLoopIndex], 120.0 * speedSlider.getValue());
            if (loopToggleButton.getToggleState())
                waveformDisplay.setLoopRegion(player->getLoopStartSeconds(),
                    player->getLoopEndSeconds());
        }
    }
    else if (button == &loopSizeDownButton)
    {
        if (currentLoopIndex > 0)
        {
            currentLoopIndex--;
            updateLoopDisplay();
            player->updateLoopSize(loopSizes[currentLoopIndex], 120.0 * speedSlider.getValue());
            if (loopToggleButton.getToggleState())
                waveformDisplay.setLoopRegion(player->getLoopStartSeconds(),
                    player->getLoopEndSeconds());
        }
    }
    else
    {
        for (int i = 0; i < 8; ++i) {
            if (button == &hotCueButtons[i]) { handleHotCueClick(i); break; }
        }
    }
}

void DeckGUI::updateLoopDisplay()
{
    double size = loopSizes[currentLoopIndex];
    juce::String text;
    if (size < 1.0)
    {
        if (size == 0.5) text = "1/2";
        else if (size == 0.25) text = "1/4";
    }
    else
    {
        text = juce::String((int)size);
    }
    loopSizeLabel.setText(text, juce::dontSendNotification);
}

void DeckGUI::sliderValueChanged(juce::Slider* slider)
{
    if (slider == &volSlider) player->setGain(slider->getValue());
    else if (slider == &speedSlider) {
        player->setSpeed(slider->getValue());
        waveformDisplay.setPlaybackSpeed(slider->getValue());
        updateBPMLabel();
    }
    else if (slider == &highEqSlider) { player->setEqGain(2, slider->getValue()); saveEqSettings(player->getCurrentTrackPath()); }
    else if (slider == &midEqSlider) { player->setEqGain(1, slider->getValue()); saveEqSettings(player->getCurrentTrackPath()); }
    else if (slider == &lowEqSlider) { player->setEqGain(0, slider->getValue()); saveEqSettings(player->getCurrentTrackPath()); }
    else if (slider == &filterSlider) { player->setFilter(slider->getValue()); saveEqSettings(player->getCurrentTrackPath()); }
}

bool DeckGUI::isInterestedInFileDrag(const juce::StringArray& files) { return true; }

void DeckGUI::filesDropped(const juce::StringArray& files, int x, int y) { if (files.size() > 0) loadFile(juce::File(files[0])); }
bool DeckGUI::isInterestedInDragSource(const SourceDetails& details) { return true; }
void DeckGUI::itemDropped(const SourceDetails& details) { loadFile(juce::File(details.description.toString())); }

void DeckGUI::loadFile(const juce::File& file)
{
    if (file.existsAsFile())
    {
        player->loadURL(juce::URL{ file });
        waveformDisplay.loadURL(juce::URL{ file });
        overviewDisplay.loadURL(juce::URL{ file });
        titleLabel.setText(file.getFileName(), juce::dontSendNotification);
        titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
        playPauseButton.setToggleState(false, juce::dontSendNotification);

        loopToggleButton.setToggleState(false, juce::dontSendNotification);
        waveformDisplay.clearLoopRegion();

        // Always clear FX when loading a new track
        player->setFXType(DJAudioPlayer::FXType::None);

        highEqSlider.setValue(1.0, juce::dontSendNotification);
        midEqSlider.setValue(1.0, juce::dontSendNotification);
        lowEqSlider.setValue(1.0, juce::dontSendNotification);
        filterSlider.setValue(0.5, juce::dontSendNotification);

        waveformDisplay.setPlaybackSpeed(speedSlider.getValue());
        updateBPMLabel();

        juce::String trackPath = file.getFullPathName();
        loadHotCuesForTrack(trackPath);
        loadEqSettings(trackPath);

        for (int i = 0; i < 8; ++i) {
            if (player->hasHotCue(i)) waveformDisplay.setHotCueMarker(i, player->getHotCuePosition(i));
        }
        updateHotCueButtons();
    }
}

void DeckGUI::updateBPMLabel()
{
    double effectiveBPM = 120.0 * speedSlider.getValue();
    bpmLabel.setText(juce::String(effectiveBPM, 1), juce::dontSendNotification);
    double percent = (speedSlider.getValue() - 1.0) * 100.0;
    juce::String sign = (percent >= 0) ? "+" : "";
    bpmPercentLabel.setText(sign + juce::String(percent, 1) + "%", juce::dontSendNotification);
}

void DeckGUI::timerCallback()
{
    waveformDisplay.setPositionRelative(player->getPositionRelative());
    overviewDisplay.setPositionRelative(player->getPositionRelative());
    if (!ignoreTimerUpdates) {
        if (player->isPlaying() != playPauseButton.getToggleState())
            playPauseButton.setToggleState(player->isPlaying(), juce::dontSendNotification);

        if (player->isLoopingEnabled() != loopToggleButton.getToggleState())
        {
            loopToggleButton.setToggleState(player->isLoopingEnabled(), juce::dontSendNotification);
            if (player->isLoopingEnabled())
                waveformDisplay.setLoopRegion(player->getLoopStartSeconds(),
                    player->getLoopEndSeconds());
            else
                waveformDisplay.clearLoopRegion();
        }
    }

    uint8_t newCueState = 0;
    for (int i = 0; i < 8; ++i)
        if (player->hasHotCue(i)) newCueState |= (1 << i);

    if (newCueState != lastHotCueState)
    {
        lastHotCueState = newCueState;
        updateHotCueButtons();
    }
}

void DeckGUI::handleHotCueClick(int cueIndex)
{
    // R3B: Shift+click on a set cue re-assigns it to the current position (modify individually).
    // Normal click on a set cue triggers (seeks to) it.
    // Click on an empty slot sets a new cue at the current position.
    if (player->hasHotCue(cueIndex))
    {
        if (juce::ModifierKeys::currentModifiers.isShiftDown())
        {
            double currentPos = player->getPositionRelative();
            player->setHotCue(cueIndex, currentPos);
            waveformDisplay.setHotCueMarker(cueIndex, currentPos);
            updateHotCueButtons();
            if (player->getCurrentTrackPath().isNotEmpty())
                saveHotCuesForTrack(player->getCurrentTrackPath());
        }
        else
        {
            player->triggerHotCue(cueIndex);
        }
    }
    else
    {
        double currentPos = player->getPositionRelative();
        player->setHotCue(cueIndex, currentPos);
        waveformDisplay.setHotCueMarker(cueIndex, currentPos);
        updateHotCueButtons();
        if (player->getCurrentTrackPath().isNotEmpty())
            saveHotCuesForTrack(player->getCurrentTrackPath());
    }
}

void DeckGUI::updateHotCueButtons()
{
    for (int i = 0; i < 8; ++i) {
        if (player->hasHotCue(i)) {
            hotCueButtons[i].setColour(juce::TextButton::buttonColourId, juce::Colour(0xff00aa00));
            hotCueButtons[i].setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        }
        else {
            hotCueButtons[i].setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1a1a1a));
            hotCueButtons[i].setColour(juce::TextButton::textColourOffId, juce::Colours::grey);
        }
    }
}

juce::File DeckGUI::getHotCuesFile()
{
    juce::File appDataDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
    juce::File ourAppDir = appDataDir.getChildFile("OtoDecks");
    if (!ourAppDir.exists()) ourAppDir.createDirectory();
    // C1: Use deck-specific files so Deck 1 and Deck 2 never share or overwrite
    // each other's cue data for the same track path.
    return ourAppDir.getChildFile(isLeftDeck ? "hotcues_deck1.json" : "hotcues_deck2.json");
}

juce::File DeckGUI::getEqSettingsFile()
{
    juce::File appDataDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
    juce::File ourAppDir = appDataDir.getChildFile("OtoDecks");
    return ourAppDir.getChildFile("eq_settings.json");
}

void DeckGUI::saveHotCuesForTrack(const juce::String& trackPath)
{
    juce::File hotCuesFile = getHotCuesFile();
    juce::var hotCuesData;
    if (hotCuesFile.existsAsFile()) {
        juce::String jsonString = hotCuesFile.loadFileAsString();
        if (juce::JSON::parse(jsonString, hotCuesData).failed()) hotCuesData = juce::var(new juce::DynamicObject());
    }
    else { hotCuesData = juce::var(new juce::DynamicObject()); }

    juce::Array<juce::var> cuesArray;
    for (int i = 0; i < 8; ++i) cuesArray.add(player->getHotCuePosition(i));

    // Use a deck-prefixed key so Deck 1 and Deck 2 cues are stored separately
    // even when the same track file is loaded on both decks.
    juce::String deckKey = (isLeftDeck ? "deck1_" : "deck2_") + trackPath;

    if (auto* obj = hotCuesData.getDynamicObject()) obj->setProperty(deckKey, cuesArray);
    hotCuesFile.replaceWithText(juce::JSON::toString(hotCuesData, true));
}

void DeckGUI::loadHotCuesForTrack(const juce::String& trackPath)
{
    // Always clear existing cues first — prevents stale cues from a previous
    // track (or the other deck) bleeding into the newly loaded track.
    player->clearAllHotCues();
    waveformDisplay.clearAllHotCueMarkers();

    juce::File hotCuesFile = getHotCuesFile();
    if (!hotCuesFile.existsAsFile()) return;
    juce::var hotCuesData;
    if (juce::JSON::parse(hotCuesFile.loadFileAsString(), hotCuesData).failed()) return;

    // Prefix the key with the deck ID so Deck 1 and Deck 2 never share cues
    // for the same track file.
    juce::String deckKey = (isLeftDeck ? "deck1_" : "deck2_") + trackPath;

    if (hotCuesData.hasProperty(deckKey)) {
        if (auto* cuesArray = hotCuesData.getProperty(deckKey, juce::var()).getArray()) {
            for (int i = 0; i < 8 && i < cuesArray->size(); ++i) {
                double position = (*cuesArray)[i];
                if (position >= 0.0) player->setHotCue(i, position);
            }
        }
    }
}

void DeckGUI::saveEqSettings(const juce::String& trackPath)
{
    if (trackPath.isEmpty()) return;

    juce::File eqFile = getEqSettingsFile();
    juce::var eqData;

    if (eqFile.existsAsFile()) {
        juce::String jsonString = eqFile.loadFileAsString();
        if (juce::JSON::parse(jsonString, eqData).failed()) eqData = juce::var(new juce::DynamicObject());
    }
    else { eqData = juce::var(new juce::DynamicObject()); }

    juce::DynamicObject* entry = new juce::DynamicObject();
    entry->setProperty("low", lowEqSlider.getValue());
    entry->setProperty("mid", midEqSlider.getValue());
    entry->setProperty("high", highEqSlider.getValue());
    entry->setProperty("filter", filterSlider.getValue());

    if (auto* obj = eqData.getDynamicObject()) obj->setProperty(trackPath, juce::var(entry));
    eqFile.replaceWithText(juce::JSON::toString(eqData, true));
}

void DeckGUI::loadEqSettings(const juce::String& trackPath)
{
    juce::File eqFile = getEqSettingsFile();
    if (!eqFile.existsAsFile()) return;

    juce::var eqData;
    if (juce::JSON::parse(eqFile.loadFileAsString(), eqData).failed()) return;

    if (eqData.hasProperty(trackPath))
    {
        juce::var trackEq = eqData.getProperty(trackPath, juce::var());
        if (trackEq.isObject())
        {
            double low = trackEq.getProperty("low", 1.0);
            double mid = trackEq.getProperty("mid", 1.0);
            double high = trackEq.getProperty("high", 1.0);
            double filter = trackEq.getProperty("filter", 0.5);

            lowEqSlider.setValue(low, juce::sendNotification);
            midEqSlider.setValue(mid, juce::sendNotification);
            highEqSlider.setValue(high, juce::sendNotification);
            filterSlider.setValue(filter, juce::sendNotification);
        }
    }
}