# OtoDecks DJ Application

OtoDecks is a two-deck DJ application built in C++ using the JUCE framework. It allows a DJ to load, play, and mix audio tracks in real time, with a full suite of professional-grade features: per-deck three-band EQ, six real-time audio effects (Echo, Flanger, Reverb, Slicer, Crusher, Brake), eight hot cue buttons per deck, beat-synchronised looping, a persistent music library, master-mix recording, and animated turntable and waveform graphics. All user data persists between sessions via JSON files.

![Screenshot placeholder](docs/screenshot.png)

## Source Code

All application source code (`.cpp` and `.h` files) is located in the **`Source/`** folder.

## Tech Stack

| | |
|---|---|
| Language | C++17 (JUCE 7 framework) |
| Platform | Windows / macOS / Linux (desktop) |
| Persistence | JSON files (playlist, hot cues, EQ settings) |
| Audio I/O | JUCE `AudioAppComponent` + `MixerAudioSource` |

## Features

- **Core playback** — load, play, and mix two tracks simultaneously with independent volume faders and a constant-power crossfader
- **Speed control** — 0.5x–1.5x playback speed with a live BPM readout
- **Music library** — persistent track list with name, duration, and file path; drag-and-drop onto either deck
- **Hot cues** — 8 cue points per deck, Shift+click to reassign, all persisted per track
- **Three-band EQ + filter** — LOW/MID/HIGH rotary knobs plus a low-pass↔high-pass sweep filter, saved per track
- **Beat-synchronised looping** — loop lengths from 1/4 to 32 beats, derived from BPM
- **Real-time FX panel** — Echo, Slicer, Flanger, Reverb, Crusher, and Brake, each controlled via an XY touch pad
- **Master recording** — records the mixed output to WAV, with a built-in playback/delete panel for past recordings
- **Animated turntables & waveforms** — spinning platter graphics and a full-track waveform overview strip
- **Built-in help panel** — in-app reference for every control
- **Custom look & feel** — dark, hardware-inspired UI (cyan/amber deck accents, vector-drawn icons)

## Controls

| Control | Action |
|---|---|
| Play/Pause button | Start or stop playback |
| Volume fader | Adjust deck output level |
| Crossfader | Blend between the two decks |
| Speed slider | Change playback speed (0.5x–1.5x) |
| SYNC | Match BPM readout between decks |
| Hot cue buttons (1–8) | Set (if empty) or jump to a cue point |
| Shift + Hot cue button | Reassign that cue to the current position |
| CLEAR CUES | Clear all 8 hot cues on a deck |
| LOW / MID / HIGH knobs | Adjust EQ bands |
| FILTER knob | Sweep between low-pass and high-pass |
| LOOP + ▲/▼ | Set and resize a beat-synced loop |
| FX button | Open the effects panel (Echo, Slicer, Flanger, Reverb, Crusher, Brake) |
| REC button | Start/stop master mix recording |
| RESTART button | Reset the app to its default state |
| ? button | Open the help panel |

## How to Run

This project uses **Projucer** (JUCE's project management tool) to generate a Visual Studio solution.

1. **Install JUCE** (includes Projucer) from [juce.com/get-juce](https://juce.com/get-juce)
2. **Clone this repository**
3. Open **`OtoDecks.jucer`** in Projucer
4. Click **Save and Open in IDE** — this generates the Visual Studio solution in `Builds/`
5. Build and run from Visual Studio (Debug or Release)

> The `Builds/` folder is excluded from version control (see `.gitignore`), so this export step is required after cloning.

## Data Persistence

All user data is saved as JSON under `userApplicationDataDirectory/OtoDecks/`, and is restored automatically on the next launch.

| Data | File | Key |
|---|---|---|
| Music library | `playlist.json` | — (array of track objects) |
| Hot cues | `hotcues.json` | Track file path → array of 8 cue positions |
| EQ settings | `eq_settings.json` | Track file path → `{low, mid, high, filter}` |
| Recordings | `Documents/OtoDecks Recordings/` | Filename → WAV file |

Missing audio files are silently skipped on load — moving or deleting a track since the last session won't crash the app.

## Design Notes

- **Reference BPM**: The base BPM is fixed at 120.0 rather than computed via audio analysis, since real onset-detection requires a multi-second offline pass outside the scope of JUCE's real-time audio callback. The displayed BPM still scales correctly with the speed slider for beatmatching.
- **Supported formats**: MP3, WAV, AIFF, FLAC, OGG, and M4A — all covered by JUCE's built-in `AudioFormatManager`.
- **Thread safety**: FX and EQ parameters are written on the message thread and read on the audio thread using atomic types and JUCE's thread-safe patterns.
