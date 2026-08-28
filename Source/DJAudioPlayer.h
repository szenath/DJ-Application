/*
  ==============================================================================
    DJAudioPlayer.h
  ==============================================================================
*/

#pragma once
#include "../JuceLibraryCode/JuceHeader.h"
#include <array>
#include <atomic>
#include <vector>

class DJAudioPlayer : public juce::AudioSource
{
public:
    explicit DJAudioPlayer(juce::AudioFormatManager& formatManagerToUse);
    ~DJAudioPlayer() override;

    /** Prepares the transport and all DSP objects for streaming.
        @param samplesPerBlockExpected  Expected audio block size per callback.
        @param sampleRate              Device sample rate in Hz. */
    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;

    /** Fills bufferToFill with the next mixed audio block (transport -> EQ -> FX -> RMS). */
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override;

    /** Releases all audio resources held by the transport and resampler. */
    void releaseResources() override;

    /** Loads an audio file from a URL. Pass an empty URL to reset the player.
        @param audioURL  Local file or network URL to load. */
    void   loadURL(juce::URL audioURL);

    /** Sets the output gain (0.0–1.0), combined multiplicatively with crossfadeGain. */
    void   setGain(double gain);

    /** Sets the crossfader gain contribution (0.0–1.0). */
    void   setCrossfadeGain(double gain);

    /** Sets the playback speed as a ratio of original tempo (0.0 < ratio <= 4.0).
        1.0 = normal speed, 0.5 = half speed, 2.0 = double speed. */
    void   setSpeed(double ratio);

    /** Seeks to an absolute position in seconds. */
    void   setPosition(double posInSecs);

    /** Seeks to a relative position within the track (0.0 = start, 1.0 = end). */
    void   setPositionRelative(double pos);

    /** Starts playback from the current position. */
    void   start();

    /** Pauses playback without losing the current position. */
    void   stop();

    /** Returns true if the transport is currently playing. */
    bool   isPlaying() const;

    /** Returns the current playback position as a 0.0–1.0 fraction. */
    double getPositionRelative() const;

    /** Returns total track duration in seconds, or 0 if no track is loaded. */
    double getLengthInSeconds() const;

    /** Returns the latest RMS output level (0.0–1.0) for VU metering. */
    float  getLevel() const { return currentRMSLevel.load(); }

    /** Returns the current playback speed ratio. */
    double getSpeed() const { return userSpeedRatio; }

    /** Returns a reference BPM of 120.0 (actual BPM detection would require
        onset/autocorrelation analysis; the displayed value is scaled by speed). */
    double getBPM()   const { return 120.0; }

    /** Stores a hot cue at the given relative track position.
        @param cueIndex  0–7.
        @param position  Relative position to store (0.0–1.0). */
    void   setHotCue(int cueIndex, double position);

    /** Seeks to a stored hot cue. No-op if the slot is empty.
        @param cueIndex  0–7. */
    void   triggerHotCue(int cueIndex);

    /** Clears a single hot cue slot.
        @param cueIndex  0–7. */
    void   clearHotCue(int cueIndex);

    /** Clears all 8 hot cue slots. */
    void   clearAllHotCues();

    /** Returns true if the given cue slot holds a stored position. */
    bool   hasHotCue(int cueIndex) const;

    /** Returns the stored relative position for a cue, or -1.0 if empty. */
    double getHotCuePosition(int cueIndex) const;

    /** Returns the full file path of the currently loaded track. */
    juce::String getCurrentTrackPath() const { return currentTrackPath; }

    /** Starts a beat-synchronised loop from the current transport position.
        @param numBeats  Loop length in beats.
        @param bpm       Reference BPM used to convert beats to seconds. */
    void startLoop(double numBeats, double bpm);

    /** Disables the active loop (transport continues past the loop end point). */
    void stopLoop();

    /** Adjusts the loop end point without stopping the loop.
        @param numBeats  New loop length in beats.
        @param bpm       Reference BPM. */
    void updateLoopSize(double numBeats, double bpm);

    /** Returns true if a loop is currently active. */
    bool   isLoopingEnabled()    const { return isLooping.load(); }

    /** Returns the loop start position in seconds. */
    double getLoopStartSeconds() const { return loopStart.load(); }

    /** Returns the loop end position in seconds. */
    double getLoopEndSeconds()   const { return loopEnd.load(); }

    /** Sets the gain for one EQ band using a JUCE DSP IIR filter.
        @param band        0 = low shelf (200 Hz), 1 = peak (1 kHz), 2 = high shelf (5 kHz).
        @param gainFactor  Linear multiplier (1.0 = flat, 2.0 = +6 dB boost). */
    void setEqGain(int band, double gainFactor);

    /** Sets the DJ-style sweep filter.
        @param val  0.0 = low-pass, 0.5 = bypass (all-pass), 1.0 = high-pass. */
    void setFilter(double val);

    /** Identifies each available real-time audio effect. */
    enum class FXType { None, Echo, Slicer, Flanger, Reverb, Crusher, Brake };

    /** Activates an effect, resetting its internal state to avoid stale-buffer artefacts.
        Pass FXType::None to bypass all effects. */
    void setFXType(FXType type);

    /** Updates the XY pad parameters for the active effect.
        @param x  Horizontal axis value (0.0–1.0).
        @param y  Vertical axis value   (0.0–1.0). */
    void updateFXParameters(float x, float y);

private:
    void applyEffectiveGain();

    /** Dispatches to one of the five focused FX processing methods below. */
    void processFX(juce::AudioBuffer<float>& buffer, int startSample, int numSamples);

    // ── Per-effect processing helpers (C4: each has a single, clear purpose) ──
    /** Processes one audio block through the parallel-wet/dry echo delay line. */
    void processEchoBlock(juce::AudioBuffer<float>& buf, int startSample, int numSamples);
    /** Processes one audio block through the sine-LFO comb-filter flanger. */
    void processFlangerBlock(juce::AudioBuffer<float>& buf, int startSample, int numSamples);
    /** Processes one audio block through the JUCE dsp::Reverb module. */
    void processReverbBlock(juce::AudioBuffer<float>& buf, int startSample, int numSamples);
    /** Processes one audio block through the rhythmic gate slicer. */
    void processSlicerBlock(juce::AudioBuffer<float>& buf, int startSample, int numSamples);
    /** Processes one audio block through the bit-crusher + sample-rate reducer. */
    void processCrusherBlock(juce::AudioBuffer<float>& buf, int startSample, int numSamples);

    // ── Transport ─────────────────────────────────────────────────────────────
    juce::AudioFormatManager& formatManager;
    juce::AudioTransportSource                     transportSource;
    juce::ResamplingAudioSource                    resampleSource{ &transportSource, false, 2 };
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;

    double baseGain{ 1.0 };
    double crossfadeGain{ 1.0 };
    double userSpeedRatio{ 1.0 };
    std::atomic<float> currentRMSLevel{ 0.0f };

    // ── Hot Cues / Loop ───────────────────────────────────────────────────────
    std::array<double, 8> hotCues{ -1.0,-1.0,-1.0,-1.0,-1.0,-1.0,-1.0,-1.0 };
    juce::String          currentTrackPath;
    std::atomic<bool>     isLooping{ false };
    std::atomic<double>   loopStart{ 0.0 };
    std::atomic<double>   loopEnd{ 0.0 };

    // ── EQ ────────────────────────────────────────────────────────────────────
    struct StereoIIRFilter
    {
        juce::dsp::IIR::Filter<float> L, R;
        void prepare(const juce::dsp::ProcessSpec& s) { L.prepare(s); R.prepare(s); }
        void reset() { L.reset();   R.reset(); }
        void process(juce::dsp::ProcessContextReplacing<float>& ctx)
        {
            auto& blk = ctx.getOutputBlock();
            if (blk.getNumChannels() > 0) { auto b = blk.getSingleChannelBlock(0); juce::dsp::ProcessContextReplacing<float> c(b); L.process(c); }
            if (blk.getNumChannels() > 1) { auto b = blk.getSingleChannelBlock(1); juce::dsp::ProcessContextReplacing<float> c(b); R.process(c); }
        }
        void updateCoefficients(const juce::ReferenceCountedObjectPtr<juce::dsp::IIR::Coefficients<float>>& c)
        {
            if (c) { L.coefficients = c; R.coefficients = c; }
        }
    };
    StereoIIRFilter lowShelfFilter, peakFilter, highShelfFilter, djFilter;

    // ── FX shared state ───────────────────────────────────────────────────────
    double currentSampleRate{ 44100.0 };
    FXType currentFXType{ FXType::None };
    float  currentFxX{ 0.5f };
    float  currentFxY{ 0.5f };

    // --- REVERB ---
    juce::dsp::Reverb             reverb;
    juce::dsp::Reverb::Parameters reverbParams;

    // --- ECHO ---
    // Manual circular delay buffers so we get real distinct repetitions.
    static constexpr int kMaxEchoSamples = 192001;
    std::vector<float> echoBuffer[2];
    int   echoWritePos{ 0 };
    // Smoothed parameters to avoid zipper noise
    float echoDelaySmooth{ 22050.0f };
    float echoFeedSmooth{ 0.5f };

    // --- FLANGER ---
    // Classic comb-filter flanger: short circular buffer + sine LFO on delay time
    static constexpr int kFlangerBufSize = 8192; // must be power of 2
    std::vector<float> flangerBuf[2];
    int    flangerWritePos{ 0 };
    double flangerLFOPhase{ 0.0 };

    // --- SLICER ---
    int   slicerCounter{ 0 };
    float slicerGateEnv[2]{ 1.0f, 1.0f };   // per-channel envelope to avoid clicks

    // --- CRUSHER ---
    float crusherHeld[2]{ 0.0f, 0.0f };
    int   crusherCount[2]{ 0, 0 };

    // --- BRAKE ---
    // Simple manual ramp: brakeCurrentSpeed starts at 1.0 and decrements
    // by brakeDecrement each audio block until it reaches 0.
    float brakeCurrentSpeed{ 1.0f };   // 1.0 = full speed, 0.0 = stopped
    float brakeDecrement{ 0.0f };      // set in setFXType(Brake) based on ramp time
    int   actualBlockSize{ 512 };      // stored in prepareToPlay for accurate timing

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DJAudioPlayer)
};