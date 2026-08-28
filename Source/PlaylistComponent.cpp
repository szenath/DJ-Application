/*
  ==============================================================================

    PlaylistComponent.cpp
    Created: 1 Feb 2026 6:36:00pm
    Modified: Feb 2026 - Added drag-to-deck support & JSON persistence

  ==============================================================================
*/

#include <JuceHeader.h>
#include "PlaylistComponent.h"

//==============================================================================
PlaylistComponent::PlaylistComponent()
{
    // Register audio formats once for the lifetime of this component
    formatManager.registerBasicFormats();

    // Setup table
    addAndMakeVisible(tableComponent);
    tableComponent.setModel(this);
    tableComponent.setColour(juce::ListBox::backgroundColourId, juce::Colour(0xff1a1a1a));
    tableComponent.setColour(juce::ListBox::outlineColourId, juce::Colour(0xff3a3a3a));
    tableComponent.setOutlineThickness(1);
    tableComponent.setRowHeight(28);

    // Add columns
    tableComponent.getHeader().addColumn("Track Name", 1, 350, 150, 500,
        juce::TableHeaderComponent::defaultFlags);
    tableComponent.getHeader().addColumn("Duration", 2, 80, 60, 120,
        juce::TableHeaderComponent::defaultFlags);
    tableComponent.getHeader().addColumn("File Path", 3, 450, 200, 700,
        juce::TableHeaderComponent::defaultFlags);

    // Setup buttons
    addAndMakeVisible(addButton);
    addAndMakeVisible(clearButton);
    addButton.addListener(this);
    clearButton.addListener(this);

    addButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2a7a2a));
    clearButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff7a2a2a));

    // Load saved playlist from file
    loadPlaylistFromFile();
}

PlaylistComponent::~PlaylistComponent()
{
    // Save playlist when component is destroyed
    savePlaylistToFile();
}

void PlaylistComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff0a0a0a));

    g.setColour(juce::Colours::grey);
    g.drawRect(getLocalBounds(), 1);

    // Title
    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(16.0f, juce::Font::bold));
    g.drawText("MUSIC LIBRARY", 10, 8, 200, 25,
        juce::Justification::centredLeft, true);
}

void PlaylistComponent::resized()
{
    auto bounds = getLocalBounds();

    const int buttonHeight = 30;
    const int buttonWidth = 120;
    const int padding = 10;
    const int topBarHeight = 45;

    // Position buttons at top
    addButton.setBounds(bounds.getWidth() - buttonWidth - padding,
        10, buttonWidth, buttonHeight);
    clearButton.setBounds(bounds.getWidth() - (buttonWidth * 2) - (padding * 2),
        10, buttonWidth, buttonHeight);

    // Table takes up remaining space
    tableComponent.setBounds(padding, topBarHeight,
        bounds.getWidth() - (padding * 2),
        bounds.getHeight() - topBarHeight - padding);
}

int PlaylistComponent::getNumRows()
{
    return static_cast<int>(tracks.size());
}

void PlaylistComponent::paintRowBackground(juce::Graphics& g, int rowNumber,
    int width, int height, bool rowIsSelected)
{
    if (rowIsSelected)
        g.fillAll(juce::Colour(0xff3a5a7a));
    else if (rowNumber % 2 == 0)
        g.fillAll(juce::Colour(0xff1a1a1a));
    else
        g.fillAll(juce::Colour(0xff2a2a2a));
}

void PlaylistComponent::paintCell(juce::Graphics& g, int rowNumber, int columnId,
    int width, int height, bool rowIsSelected)
{
    if (rowNumber < 0 || rowNumber >= static_cast<int>(tracks.size()))
        return;

    g.setColour(rowIsSelected ? juce::Colours::white : juce::Colours::lightgrey);
    g.setFont(juce::Font(12.0f));

    const auto& track = tracks[rowNumber];
    juce::String text;

    switch (columnId)
    {
    case 1: // Track name
        text = track.filename;
        break;
    case 2: // Duration
        text = formatDuration(track.duration);
        break;
    case 3: // File path
        text = track.filepath;
        break;
    }

    g.drawText(text, 5, 0, width - 10, height,
        juce::Justification::centredLeft, true);
}

juce::var PlaylistComponent::getDragSourceDescription(const juce::SparseSet<int>& selectedRows)
{
    if (selectedRows.size() > 0)
    {
        int rowNumber = selectedRows[0];
        if (rowNumber >= 0 && rowNumber < static_cast<int>(tracks.size()))
        {
            return tracks[rowNumber].filepath;
        }
    }
    return juce::var();
}

void PlaylistComponent::buttonClicked(juce::Button* button)
{
    if (button == &addButton)
    {
        auto fileChooserFlags = juce::FileBrowserComponent::canSelectFiles |
            juce::FileBrowserComponent::canSelectMultipleItems;

        fileChooser.launchAsync(fileChooserFlags, [this](const juce::FileChooser& chooser)
            {
                auto files = chooser.getResults();
                loadTracksFromFiles(files);
            });
    }
    else if (button == &clearButton)
    {
        clearLibrary();
    }
}

void PlaylistComponent::addTrack(const juce::File& file)
{
    if (!file.existsAsFile())
        return;

    // Check for duplicates
    for (const auto& track : tracks)
    {
        if (track.filepath == file.getFullPathName())
            return; // Already in library
    }

    double duration = getTrackDuration(file);
    tracks.push_back(TrackInfo(file.getFileName(),
        file.getFullPathName(),
        duration));
    tableComponent.updateContent();

    // Save after adding track
    savePlaylistToFile();
}

void PlaylistComponent::removeTrack(int index)
{
    if (index >= 0 && index < static_cast<int>(tracks.size()))
    {
        tracks.erase(tracks.begin() + index);
        tableComponent.updateContent();

        // Save after removing track
        savePlaylistToFile();
    }
}

void PlaylistComponent::clearLibrary()
{
    tracks.clear();
    tableComponent.updateContent();

    // Save after clearing library
    savePlaylistToFile();
}

TrackInfo* PlaylistComponent::getTrackAt(int index)
{
    if (index >= 0 && index < static_cast<int>(tracks.size()))
        return &tracks[index];
    return nullptr;
}

void PlaylistComponent::loadTracksFromFiles(const juce::Array<juce::File>& files)
{
    for (const auto& file : files)
    {
        if (file.hasFileExtension("mp3;wav;aiff;aif;flac;ogg;m4a"))
        {
            addTrack(file);
        }
    }
}

double PlaylistComponent::getTrackDuration(const juce::File& file)
{
    // Use the member formatManager (already has basic formats registered)
    std::unique_ptr<juce::AudioFormatReader> reader(
        formatManager.createReaderFor(file));

    if (reader != nullptr)
        return (double)reader->lengthInSamples / reader->sampleRate;

    return 0.0;
}

juce::String PlaylistComponent::formatDuration(double seconds)
{
    int mins = static_cast<int>(seconds / 60);
    int secs = static_cast<int>(seconds) % 60;
    return juce::String::formatted("%d:%02d", mins, secs);
}

//==============================================================================
// PERSISTENCE METHODS
//==============================================================================

juce::File PlaylistComponent::getPlaylistFile()
{
    // Get the application data directory
    juce::File appDataDir = juce::File::getSpecialLocation(
        juce::File::userApplicationDataDirectory);

    // Create a subdirectory for our app (if it doesn't exist)
    juce::File ourAppDir = appDataDir.getChildFile("OtoDecks");

    if (!ourAppDir.exists())
        ourAppDir.createDirectory();

    // Return the playlist.json file path
    return ourAppDir.getChildFile("playlist.json");
}

void PlaylistComponent::savePlaylistToFile()
{
    juce::File playlistFile = getPlaylistFile();

    // Create JSON object using DynamicObject (compatible with older JUCE)
    juce::DynamicObject::Ptr root = new juce::DynamicObject();

    // Create tracks array
    juce::Array<juce::var> tracksArray;

    for (const auto& track : tracks)
    {
        juce::DynamicObject::Ptr trackData = new juce::DynamicObject();

        trackData->setProperty("filename", track.filename);
        trackData->setProperty("filepath", track.filepath);
        trackData->setProperty("duration", track.duration);

        tracksArray.add(juce::var(trackData.get()));
    }

    root->setProperty("tracks", tracksArray);

    // Convert to JSON string
    juce::var jsonData(root.get());
    juce::String jsonString = juce::JSON::toString(jsonData, true);

    // Write to file
    if (playlistFile.replaceWithText(jsonString))
    {
        DBG("Playlist saved successfully to: " + playlistFile.getFullPathName());
    }
    else
    {
        DBG("Failed to save playlist to: " + playlistFile.getFullPathName());
    }
}

void PlaylistComponent::loadPlaylistFromFile()
{
    juce::File playlistFile = getPlaylistFile();

    // Check if file exists
    if (!playlistFile.existsAsFile())
    {
        DBG("No saved playlist found at: " + playlistFile.getFullPathName());
        return;
    }

    // Read file content
    juce::String jsonString = playlistFile.loadFileAsString();

    // Parse JSON
    juce::var parsedJson;
    juce::Result result = juce::JSON::parse(jsonString, parsedJson);

    if (result.failed())
    {
        DBG("Failed to parse playlist JSON: " + result.getErrorMessage());
        return;
    }

    // Clear existing tracks
    tracks.clear();

    // Extract tracks array
    if (parsedJson.hasProperty("tracks"))
    {
        juce::Array<juce::var>* tracksArray = parsedJson.getProperty("tracks", juce::var()).getArray();

        if (tracksArray != nullptr)
        {
            for (const auto& trackVar : *tracksArray)
            {
                if (trackVar.isObject())
                {
                    juce::String filename = trackVar.getProperty("filename", "");
                    juce::String filepath = trackVar.getProperty("filepath", "");
                    double duration = trackVar.getProperty("duration", 0.0);

                    // Verify file still exists before adding
                    juce::File audioFile(filepath);
                    if (audioFile.existsAsFile())
                    {
                        tracks.push_back(TrackInfo(filename, filepath, duration));
                    }
                    else
                    {
                        DBG("Skipping missing file: " + filepath);
                    }
                }
            }
        }
    }

    // Update table display
    tableComponent.updateContent();

    DBG("Loaded " + juce::String(tracks.size()) + " tracks from playlist");
}