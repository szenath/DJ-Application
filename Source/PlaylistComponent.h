/*
  ==============================================================================

    PlaylistComponent.h
    Created: 1 Feb 2026 6:36:00pm
    Modified: Feb 2026 - Added drag-to-deck functionality & JSON persistence

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <vector>
#include <string>

//==============================================================================
struct TrackInfo
{
    juce::String filename;
    juce::String filepath;
    double duration;

    TrackInfo() : duration(0.0) {}
    TrackInfo(juce::String name, juce::String path, double dur)
        : filename(name), filepath(path), duration(dur) {
    }
};

//==============================================================================
class PlaylistComponent : public juce::Component,
    public juce::TableListBoxModel,
    public juce::Button::Listener,
    public juce::DragAndDropContainer
{
public:
    PlaylistComponent();
    ~PlaylistComponent() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    // TableListBoxModel
    int getNumRows() override;
    void paintRowBackground(juce::Graphics& g, int rowNumber, int width, int height,
        bool rowIsSelected) override;
    void paintCell(juce::Graphics& g, int rowNumber, int columnId,
        int width, int height, bool rowIsSelected) override;

    var getDragSourceDescription(const juce::SparseSet<int>& selectedRows) override;

    // Button::Listener
    void buttonClicked(juce::Button* button) override;

    // Track management
    void addTrack(const juce::File& file);
    void removeTrack(int index);
    void clearLibrary();
    TrackInfo* getTrackAt(int index);

private:
    juce::TableListBox tableComponent;
    std::vector<TrackInfo> tracks;

    juce::TextButton addButton{ "ADD TRACKS" };
    juce::TextButton clearButton{ "CLEAR ALL" };

    juce::FileChooser fileChooser{ "Select audio files..." };

    // Persistent format manager — registered once, reused for duration queries
    juce::AudioFormatManager formatManager;

    void loadTracksFromFiles(const juce::Array<juce::File>& files);
    double getTrackDuration(const juce::File& file);
    juce::String formatDuration(double seconds);

    // Persistence methods
    void savePlaylistToFile();
    void loadPlaylistFromFile();
    juce::File getPlaylistFile();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PlaylistComponent)
};