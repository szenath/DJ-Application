/*
  ==============================================================================
    DJAudioPlayer.cpp

    FX design guide
    ───────────────
    ECHO     Pure delay line. X → delay time (50–800 ms). Y → feedback (0–80%).
             Wet added on top of unmodified dry so the track is always audible.
             Each echo repeat decays naturally via the feedback chain.

    SLICER   Rhythmic gate. X → gate speed (fast/slow). Y → duty-cycle (thin/wide).
             A tiny attack/release envelope (5 ms) smooths each gate edge so
             there are no harsh clicks.

    FLANGER  Comb-filter flanger built from a short circular delay buffer and a
             sine LFO.  X → LFO rate (0.05–4 Hz).  Y → modulation depth / mix.
             Feedback is set to a moderate positive value for a metallic sweep.
             A true flanger delay range of 0.1–13 ms is used (not juce::Chorus).

    REVERB   JUCE dsp::Reverb (Schroeder / Moorer algorithm).
             X → room size (0–1).  Y → wet level / damping.

    CRUSHER  Two-stage degradation:
             X → bit-depth reduction (16 bit → 1 bit).
             Y → sample-rate hold (1 sample → 48 samples).
             Both stages run in series so even modest X+Y settings produce
             clearly audible lo-fi / aliasing artefacts.

    BRAKE    Exponential pitch ramp-down simulating a vinyl motor stopping.
             When the internal speed reaches ~0.3 % of normal the transport is
             paused.  Calling setFXType(None) restores full speed and restarts.
  ==============================================================================
*/

#include "DJAudioPlayer.h"

DJAudioPlayer::DJAudioPlayer(juce::AudioFormatManager& fmt)
    : formatManager(fmt)
{
}

DJAudioPlayer::~DJAudioPlayer() = default;

// ─────────────────────────────────────────────────────────────────────────────
void DJAudioPlayer::prepareToPlay(int blockSize, double sampleRate)
{
    transportSource.prepareToPlay(blockSize, sampleRate);
    resampleSource.prepareToPlay(blockSize, sampleRate);
    currentSampleRate = sampleRate;
    actualBlockSize = juce::jmax(1, blockSize);

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = (juce::uint32)blockSize;
    spec.numChannels = 2;

    lowShelfFilter.prepare(spec);
    peakFilter.prepare(spec);
    highShelfFilter.prepare(spec);
    djFilter.prepare(spec);
    reverb.prepare(spec);

    setEqGain(0, 1.0);
    setEqGain(1, 1.0);
    setEqGain(2, 1.0);
    setFilter(0.5);

    // ── Echo buffers ──────────────────────────────────────────────────────────
    for (int ch = 0; ch < 2; ++ch)
    {
        echoBuffer[ch].assign(kMaxEchoSamples, 0.0f);
    }
    echoWritePos = 0;
    echoDelaySmooth = (float)(0.375 * sampleRate); // default 375 ms
    echoFeedSmooth = 0.45f;

    // ── Flanger buffers ───────────────────────────────────────────────────────
    for (int ch = 0; ch < 2; ++ch)
    {
        flangerBuf[ch].assign(kFlangerBufSize, 0.0f);
    }
    flangerWritePos = 0;
    flangerLFOPhase = 0.0;

    // ── Slicer ────────────────────────────────────────────────────────────────
    slicerCounter = 0;
    slicerGateEnv[0] = slicerGateEnv[1] = 1.0f;

    // ── Crusher ───────────────────────────────────────────────────────────────
    crusherHeld[0] = crusherHeld[1] = 0.0f;
    crusherCount[0] = crusherCount[1] = 0;

    // ── Brake ─────────────────────────────────────────────────────────────────
    brakeCurrentSpeed = 1.0f;
    brakeDecrement = 0.0f;
}

// ─────────────────────────────────────────────────────────────────────────────
void DJAudioPlayer::getNextAudioBlock(const juce::AudioSourceChannelInfo& info)
{
    // ── Loop ─────────────────────────────────────────────────────────────────
    if (isLooping.load() && transportSource.getLengthInSeconds() > 0.0)
    {
        double pos = transportSource.getCurrentPosition();
        if (pos >= loopEnd.load())
            transportSource.setPosition(loopStart.load());
    }

    // ── Brake — volume fade to silence ───────────────────────────────────────
    // brakeCurrentSpeed ramps 1.0 → 0.0. The transport ALWAYS runs at full
    // userSpeedRatio — we never change the resampling ratio. The "stopping"
    // effect is achieved purely by multiplying the output by brakeCurrentSpeed.
    // Never touch transportSource. Never set a low resampling ratio.
    if (currentFXType == FXType::Brake)
    {
        if (brakeCurrentSpeed > 0.0f)
        {
            brakeCurrentSpeed -= brakeDecrement;
            if (brakeCurrentSpeed < 0.0f) brakeCurrentSpeed = 0.0f;
        }
    }

    // Always run at normal speed regardless of brake state
    if (resampleSource.getResamplingRatio() != userSpeedRatio)
        resampleSource.setResamplingRatio(userSpeedRatio);

    resampleSource.getNextAudioBlock(info);

    // Multiply output by brakeCurrentSpeed — fades to silence as ramp hits 0
    if (currentFXType == FXType::Brake)
    {
        float gain = juce::jlimit(0.0f, 1.0f, brakeCurrentSpeed);
        for (int ch = 0; ch < info.buffer->getNumChannels(); ++ch)
        {
            float* data = info.buffer->getWritePointer(ch, info.startSample);
            for (int i = 0; i < info.numSamples; ++i)
                data[i] *= gain;
        }
    }

    // ── EQ / DJ filter (dsp::ProcessContext path) ─────────────────────────────
    {
        juce::dsp::AudioBlock<float>        blk(*info.buffer);
        auto                                sub = blk.getSubBlock((size_t)info.startSample,
            (size_t)info.numSamples);
        juce::dsp::ProcessContextReplacing<float> ctx(sub);
        lowShelfFilter.process(ctx);
        peakFilter.process(ctx);
        highShelfFilter.process(ctx);
        djFilter.process(ctx);
    }

    // ── FX ────────────────────────────────────────────────────────────────────
    processFX(*info.buffer, info.startSample, info.numSamples);

    // ── RMS metering ──────────────────────────────────────────────────────────
    float rms = 0.0f;
    int   nCh = info.buffer->getNumChannels();
    for (int c = 0; c < nCh; ++c)
        rms += info.buffer->getRMSLevel(c, info.startSample, info.numSamples);
    if (nCh > 0) rms /= nCh;
    currentRMSLevel.store(rms);
}

// ─────────────────────────────────────────────────────────────────────────────
void DJAudioPlayer::processFX(juce::AudioBuffer<float>& buf,
    int startSample, int numSamples)
{
    if (currentFXType == FXType::None || currentFXType == FXType::Brake)
        return;

    int numCh = buf.getNumChannels();

    // =========================================================================
    // ECHO
    // =========================================================================
    // X (0→1) → delay time 50 ms to 800 ms
    // Y (0→1) → feedback  0 % to 80 %
    //
    // Architecture: parallel wet/dry.
    //   dry signal is written unchanged.
    //   echo return is popped from the delay line and added at wetLevel.
    //   (dry + feedback*echo) is pushed back into the delay line.
    // =========================================================================
    if (currentFXType == FXType::Echo)
    {
        // Smoothly approach target parameters to avoid zippering
        const float targetDelay = (float)(0.050 * currentSampleRate)
            + currentFxX * (float)(0.750 * currentSampleRate);
        const float targetFeedback = currentFxY * 0.75f;
        // Wet level: 0 at Y=0 (dry only), rises to 0.85 at Y=1
        const float wetLevel = currentFxY * 0.85f;

        // Smooth parameter changes over this block
        const float alphaDelay = 0.002f;
        const float alphaFeed = 0.005f;

        for (int i = 0; i < numSamples; ++i)
        {
            // Smooth delay & feedback values sample-by-sample
            echoDelaySmooth += alphaDelay * (targetDelay - echoDelaySmooth);
            echoFeedSmooth += alphaFeed * (targetFeedback - echoFeedSmooth);

            int delayInt = juce::jlimit(1, kMaxEchoSamples - 1,
                (int)echoDelaySmooth);

            for (int ch = 0; ch < juce::jmin(numCh, 2); ++ch)
            {
                float* data = buf.getWritePointer(ch, startSample);

                // Read from delay buffer
                int readPos = echoWritePos - delayInt;
                if (readPos < 0) readPos += kMaxEchoSamples;
                readPos = juce::jlimit(0, kMaxEchoSamples - 1, readPos);

                float echoSample = echoBuffer[ch][readPos];

                // Write new input + feedback into delay line
                echoBuffer[ch][echoWritePos] = data[i] + echoSample * echoFeedSmooth;

                // Mix: original dry + echo return
                data[i] = data[i] + echoSample * wetLevel;
            }

            // Advance write pointer (once per sample, same for all channels)
            if (++echoWritePos >= kMaxEchoSamples)
                echoWritePos = 0;
        }
        return;
    }

    // =========================================================================
    // FLANGER
    // =========================================================================
    // Classic comb-filter flanger — the "jet plane / swoosh" effect.
    //
    // HOW IT WORKS:
    //   1. The dry signal is written into a short circular delay buffer.
    //   2. A delayed copy is read back. The delay time is modulated by a
    //      sine-wave LFO so it sweeps continuously between a min and max.
    //   3. The delayed copy is mixed back with the dry signal. Because the
    //      delayed copy is slightly out of phase, comb-filter peaks and notches
    //      form across the frequency spectrum.
    //   4. As the LFO sweeps the delay time, these peaks/notches move up and
    //      down in frequency — producing the distinctive swooshing sweep.
    //   5. Feedback feeds the delayed signal back into the delay buffer,
    //      deepening the resonance and making the comb more metallic.
    //
    // X (0->1) -> LFO rate:  0.1 Hz (very slow, wide sweep) to 2 Hz (fast jet)
    // Y (0->1) -> depth + feedback:
    //             - depth controls how wide the delay sweeps (1 ms to 15 ms)
    //             - feedback scales alongside depth (0.0 to 0.75)
    //             At Y=0: gentle shimmer. At Y=1: deep metallic jet-plane roar.
    //
    // Delay range: 0.5 ms (min) to 15 ms (max) — within the classic
    // flanger zone of < 20 ms described in the specification.
    // Wet/dry: 50/50 blend so the original track always stays audible.
    // =========================================================================
    if (currentFXType == FXType::Flanger)
    {
        const float sr = (float)currentSampleRate;

        // LFO rate: 0.1 Hz (slow dreamy sweep) to 3.0 Hz (fast jet-plane)
        const float lfoRate = 0.1f + currentFxX * 2.9f;

        // Delay sweep: min always 0.5 ms; max grows with Y (deeper sweep = stronger effect)
        const float minDelayMs = 0.5f;
        const float maxDelayMs = 2.0f + currentFxY * 13.0f;  // 2 ms to 15 ms
        const float minDelaySmp = minDelayMs * 0.001f * sr;
        const float maxDelaySmp = maxDelayMs * 0.001f * sr;

        // Feedback: 0.3 base + Y pushes it to 0.85 for deep metallic resonance
        // A base of 0.3 means the comb effect is always present even at Y=0
        const float feedback = 0.30f + currentFxY * 0.55f;

        // wetMix: 0.7 base so the comb interference is clearly audible from the start
        // rises to 1.0 at Y=1 for a full jet-plane roar
        const float wetMix = 0.70f + currentFxY * 0.30f;
        const float dryMix = 1.0f;

        const double phaseInc = juce::MathConstants<double>::twoPi
            * (double)lfoRate / currentSampleRate;

        for (int i = 0; i < numSamples; ++i)
        {
            float lfo = 0.5f + 0.5f * (float)std::sin(flangerLFOPhase);
            flangerLFOPhase += phaseInc;
            if (flangerLFOPhase >= juce::MathConstants<double>::twoPi)
                flangerLFOPhase -= juce::MathConstants<double>::twoPi;

            float delaySmp = minDelaySmp + lfo * (maxDelaySmp - minDelaySmp);
            delaySmp = juce::jlimit(1.0f, (float)(kFlangerBufSize - 2), delaySmp);

            int   di = (int)delaySmp;
            float frac = delaySmp - (float)di;

            for (int ch = 0; ch < juce::jmin(numCh, 2); ++ch)
            {
                float* data = buf.getWritePointer(ch, startSample);
                float  dry = data[i];

                // Read delayed copy with linear interpolation
                int rp0 = (flangerWritePos - di + kFlangerBufSize) & (kFlangerBufSize - 1);
                int rp1 = (flangerWritePos - di - 1 + kFlangerBufSize) & (kFlangerBufSize - 1);
                float delayed = flangerBuf[ch][rp0] * (1.0f - frac)
                    + flangerBuf[ch][rp1] * frac;

                // Write dry + feedback*delayed into buffer.
                // Feedback recirculates the delayed signal, deepening the comb resonance.
                flangerBuf[ch][flangerWritePos] = dry + delayed * feedback;

                // Output: dry + wetMix * delayed.
                // Phase interference between dry and the fractionally-delayed copy
                // creates moving comb-filter notches — the swoosh/jet-plane sound.
                data[i] = dryMix * dry + wetMix * delayed;
            }

            flangerWritePos = (flangerWritePos + 1) & (kFlangerBufSize - 1);
        }
        return;
    }

    // =========================================================================
    // REVERB
    // =========================================================================
    // Mimics sound bouncing off surfaces in a large physical space.
    // X (0->1) -> room size: small room (0) up to cathedral / cave (1)
    // Y (0->1) -> wet/dry mix: at Y=0 subtle ambience; at Y=1 fully drowns
    //             the track in reverb wash — dreamy, atmospheric effect.
    //
    // Design choices for an immersive DJ reverb:
    //   - roomSize always starts from 0.5 so even X=0 sounds spacious
    //   - wetLevel can reach 1.0 at Y=1 so the effect is fully immersive
    //   - dryLevel scales DOWN as Y rises so at high Y only the reverb wash
    //     is heard (classic "drowning in reverb" DJ breakdown technique)
    //   - damping is kept low (bright) so the tail remains lush and musical
    //   - width = 1.0 for full stereo spread
    // =========================================================================
    if (currentFXType == FXType::Reverb)
    {
        float newRoom = 0.5f + currentFxX * 0.5f;
        float newDamping = 0.2f + (1.0f - currentFxY) * 0.3f;
        float newWet = currentFxY;
        float newDry = 1.0f - (currentFxY * 0.7f);

        // Only update parameters when they actually change — calling
        // setParameters() every block causes the reverb tail to reset/glitch.
        if (newRoom != reverbParams.roomSize ||
            newDamping != reverbParams.damping ||
            newWet != reverbParams.wetLevel ||
            newDry != reverbParams.dryLevel)
        {
            reverbParams.roomSize = newRoom;
            reverbParams.damping = newDamping;
            reverbParams.wetLevel = newWet;
            reverbParams.dryLevel = newDry;
            reverbParams.width = 1.0f;
            reverbParams.freezeMode = 0.0f;
            reverb.setParameters(reverbParams);
        }

        juce::dsp::AudioBlock<float>              blk(buf);
        auto sub = blk.getSubBlock((size_t)startSample, (size_t)numSamples);
        juce::dsp::ProcessContextReplacing<float> ctx(sub);
        reverb.process(ctx);
        return;
    }

    // =========================================================================
    // SLICER
    // =========================================================================
    // Rhythmic gate that chops the audio into repeated on/off slices.
    //
    // X (0→1) → gate frequency: moves from slow (long period) to very fast
    //           Period maps 24000 → 800 samples so the full X range gives
    //           roughly 2 Hz (slow chop) to 60 Hz (stutter).
    // Y (0→1) → duty cycle: how wide the ON gate is per period (10 %–90 %).
    //
    // A 5 ms attack/release envelope is applied at each gate edge to prevent
    // the harsh clicking that plagued the previous implementation.
    // =========================================================================
    if (currentFXType == FXType::Slicer)
    {
        const int   period = juce::jmax(200, (int)(24000.0f - currentFxX * 23200.0f));
        const float duty = 0.10f + currentFxY * 0.80f;
        const int   onSamples = juce::jmax(1, (int)(period * duty));

        // Linear ramp step per sample (5 ms attack/release)
        const float rampStep = 1.0f / juce::jmax(1.0f,
            (float)(0.005 * currentSampleRate));

        for (int i = 0; i < numSamples; ++i)
        {
            int  phase = slicerCounter % period;
            bool gateIsOpen = (phase < onSamples);
            ++slicerCounter;

            float target = gateIsOpen ? 1.0f : 0.0f;

            for (int ch = 0; ch < numCh; ++ch)
            {
                float& env = slicerGateEnv[juce::jmin(ch, 1)];
                float* data = buf.getWritePointer(ch, startSample);

                // Linear ramp — guaranteed to reach 0 or 1 regardless of gate speed
                if (env < target)       env = juce::jmin(target, env + rampStep);
                else if (env > target)  env = juce::jmax(target, env - rampStep);

                data[i] *= env;
            }
        }
        return;
    }

    // =========================================================================
    // CRUSHER  (Bit-Crusher + Sample-Rate Reducer)
    // =========================================================================
    // Stage 1 – Bit depth reduction (X axis):
    //   X=0 → 16 bits (pristine, barely any change)
    //   X=1 →  4 bits (heavy gritty lo-fi but song still recognisable)
    //   This range keeps the music audible at all X values.
    //
    // Stage 2 – Sample-rate hold (Y axis):
    //   Y=0 → hold 1 sample  (no downsampling)
    //   Y=1 → hold 32 samples (metallic/robotic aliasing)
    //
    // Wet/dry blend: dry signal is always mixed in at (1 - wetAmount) so
    // the original track remains audible even at extreme settings.
    // wetAmount rises from 0.3 at X/Y=0 to 0.85 at X/Y=1.
    // =========================================================================
    if (currentFXType == FXType::Crusher)
    {
        // Bit depth: 16 bits (X=0, clean) down to 4 bits (X=1, heavy lo-fi)
        float bits = 16.0f - currentFxX * 12.0f;
        bits = juce::jmax(4.0f, bits);
        float steps = std::pow(2.0f, bits - 1.0f);

        // Sample-hold: 1 sample (Y=0, no effect) to 32 samples (Y=1, robotic)
        int holdLen = 1 + (int)(currentFxY * 31.0f);

        // Wet amount: 0 at X=0,Y=0 (completely transparent) up to 0.85 at X=1,Y=1
        // This means the crusher is completely bypassed at the pad centre/minimum
        float intensity = (currentFxX + currentFxY) * 0.5f;   // 0.0 to 1.0
        float wetAmount = intensity * 0.85f;
        float dryAmount = 1.0f - wetAmount * 0.5f;  // always keep meaningful dry

        for (int ch = 0; ch < juce::jmin(numCh, 2); ++ch)
        {
            float* data = buf.getWritePointer(ch, startSample);
            float  held = crusherHeld[ch];
            int    cnt = crusherCount[ch];

            for (int i = 0; i < numSamples; ++i)
            {
                float dry = data[i];

                if (cnt <= 0)
                {
                    held = std::round(dry * steps) / steps;
                    cnt = holdLen;
                }
                --cnt;

                data[i] = dry * dryAmount + held * wetAmount;
            }

            crusherHeld[ch] = held;
            crusherCount[ch] = cnt;
        }
        return;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
void DJAudioPlayer::releaseResources()
{
    transportSource.releaseResources();
    resampleSource.releaseResources();
}

// ─────────────────────────────────────────────────────────────────────────────
void DJAudioPlayer::loadURL(juce::URL audioURL)
{
    clearAllHotCues();
    stopLoop();

    // Always restore speed so a new track starts at full pitch
    brakeCurrentSpeed = 1.0f;
    brakeDecrement = 0.0f;
    resampleSource.setResamplingRatio(userSpeedRatio);

    if (audioURL.isEmpty())
    {
        transportSource.setSource(nullptr);
        readerSource.reset();
        currentTrackPath.clear();
        currentRMSLevel.store(0.0f);   // clear VU meter immediately
        return;
    }

    juce::AudioFormatReader* reader = nullptr;

    if (audioURL.isLocalFile())
    {
        juce::File f = audioURL.getLocalFile();
        currentTrackPath = f.getFullPathName();
        reader = formatManager.createReaderFor(f);
    }
    else
    {
        std::unique_ptr<juce::InputStream> s(audioURL.createInputStream(false));
        if (s) reader = formatManager.createReaderFor(std::move(s));
    }

    if (reader != nullptr)
    {
        auto newSrc = std::make_unique<juce::AudioFormatReaderSource>(reader, true);
        transportSource.setSource(newSrc.get(), 0, nullptr, reader->sampleRate);
        readerSource.reset(newSrc.release());
        applyEffectiveGain();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
void DJAudioPlayer::applyEffectiveGain()
{
    transportSource.setGain((float)juce::jlimit(0.0, 1.0, baseGain * crossfadeGain));
}

void DJAudioPlayer::setGain(double g) { baseGain = g; applyEffectiveGain(); }
void DJAudioPlayer::setCrossfadeGain(double g) { crossfadeGain = juce::jlimit(0.0, 1.0, g); applyEffectiveGain(); }

void DJAudioPlayer::setSpeed(double ratio)
{
    if (ratio > 0.0 && ratio <= 4.0)
    {
        userSpeedRatio = ratio;
        if (currentFXType != FXType::Brake)
            resampleSource.setResamplingRatio(ratio);
    }
}

void DJAudioPlayer::setPosition(double s) { transportSource.setPosition(s); }
void DJAudioPlayer::setPositionRelative(double p)
{
    double len = transportSource.getLengthInSeconds();
    if (len > 0.0) transportSource.setPosition(len * p);
}
void   DJAudioPlayer::start() { transportSource.start(); }
void   DJAudioPlayer::stop() { transportSource.stop(); }
bool   DJAudioPlayer::isPlaying() const { return transportSource.isPlaying(); }
double DJAudioPlayer::getPositionRelative() const
{
    double len = transportSource.getLengthInSeconds();
    return len > 0.0 ? transportSource.getCurrentPosition() / len : 0.0;
}
double DJAudioPlayer::getLengthInSeconds() const { return transportSource.getLengthInSeconds(); }

// ── Hot Cues ──────────────────────────────────────────────────────────────────
void   DJAudioPlayer::setHotCue(int i, double p) { if (i >= 0 && i < 8) hotCues[i] = juce::jlimit(0.0, 1.0, p); }
void   DJAudioPlayer::triggerHotCue(int i) { if (i >= 0 && i < 8 && hotCues[i] >= 0.0) setPositionRelative(hotCues[i]); }
void   DJAudioPlayer::clearHotCue(int i) { if (i >= 0 && i < 8) hotCues[i] = -1.0; }
void   DJAudioPlayer::clearAllHotCues() { for (auto& c : hotCues) c = -1.0; }
bool   DJAudioPlayer::hasHotCue(int i) const { return i >= 0 && i < 8 && hotCues[i] >= 0.0; }
double DJAudioPlayer::getHotCuePosition(int i) const { return (i >= 0 && i < 8) ? hotCues[i] : -1.0; }

// ── Loop ──────────────────────────────────────────────────────────────────────
void DJAudioPlayer::startLoop(double numBeats, double bpm)
{
    if (bpm <= 0.0) bpm = 120.0;
    double spb = 60.0 / bpm;
    double start = transportSource.getCurrentPosition();
    double end = juce::jmin(start + numBeats * spb,
        transportSource.getLengthInSeconds());
    loopStart.store(start); loopEnd.store(end); isLooping.store(true);
}
void DJAudioPlayer::stopLoop() { isLooping.store(false); }
void DJAudioPlayer::updateLoopSize(double numBeats, double bpm)
{
    if (!isLooping.load()) return;
    if (bpm <= 0.0) bpm = 120.0;
    double end = juce::jmin(loopStart.load() + numBeats * (60.0 / bpm),
        transportSource.getLengthInSeconds());
    loopEnd.store(end);
}

// ── EQ / Filter ───────────────────────────────────────────────────────────────
void DJAudioPlayer::setEqGain(int band, double g)
{
    if (currentSampleRate <= 0) return;
    float gf = (float)g;
    if (band == 0) lowShelfFilter.updateCoefficients(juce::dsp::IIR::Coefficients<float>::makeLowShelf(currentSampleRate, 200.0f, 0.7f, gf));
    else if (band == 1) peakFilter.updateCoefficients(juce::dsp::IIR::Coefficients<float>::makePeakFilter(currentSampleRate, 1000.0f, 0.7f, gf));
    else if (band == 2) highShelfFilter.updateCoefficients(juce::dsp::IIR::Coefficients<float>::makeHighShelf(currentSampleRate, 5000.0f, 0.7f, gf));
}

void DJAudioPlayer::setFilter(double val)
{
    if (currentSampleRate <= 0) return;
    val = juce::jlimit(0.0, 1.0, val);
    if (val < 0.45) { float f = juce::jmax(20.0f, 20000.0f - (float)(val * 44000.0)); djFilter.updateCoefficients(juce::dsp::IIR::Coefficients<float>::makeLowPass(currentSampleRate, f)); }
    else if (val > 0.55) { float f = juce::jmin(20000.0f, 20.0f + (float)((val - 0.55) * 44000.0)); djFilter.updateCoefficients(juce::dsp::IIR::Coefficients<float>::makeHighPass(currentSampleRate, f)); }
    else { djFilter.updateCoefficients(juce::dsp::IIR::Coefficients<float>::makeFirstOrderAllPass(currentSampleRate, 1000.0f)); }
}

// ── FX control ────────────────────────────────────────────────────────────────
void DJAudioPlayer::setFXType(FXType type)
{
    // Reset state of incoming effect so no stale data bleeds through
    if (type == FXType::Echo)
    {
        for (int ch = 0; ch < 2; ++ch)
            std::fill(echoBuffer[ch].begin(), echoBuffer[ch].end(), 0.0f);
        echoWritePos = 0;
    }
    else if (type == FXType::Flanger)
    {
        for (int ch = 0; ch < 2; ++ch)
            std::fill(flangerBuf[ch].begin(), flangerBuf[ch].end(), 0.0f);
        flangerWritePos = 0;
        flangerLFOPhase = 0.0;
    }
    else if (type == FXType::Slicer)
    {
        slicerCounter = 0;
        slicerGateEnv[0] = slicerGateEnv[1] = 1.0f;
    }
    else if (type == FXType::Crusher)
    {
        crusherHeld[0] = crusherHeld[1] = 0.0f;
        crusherCount[0] = crusherCount[1] = 0;
    }
    else if (type == FXType::Brake)
    {
        resampleSource.setResamplingRatio(userSpeedRatio);
        brakeCurrentSpeed = 1.0f;

        // Use the real block size stored in prepareToPlay for accurate 2-second ramp
        double rampSecs = 2.0;
        double numBlocks = (currentSampleRate * rampSecs) / (double)actualBlockSize;
        brakeDecrement = (float)(1.0 / numBlocks);
    }
    else if (type == FXType::None)
    {
        // Restore full speed — song was always playing, just reset the ratio
        brakeCurrentSpeed = 1.0f;
        brakeDecrement = 0.0f;
        resampleSource.setResamplingRatio(userSpeedRatio);
    }

    currentFXType = type;
}

void DJAudioPlayer::updateFXParameters(float x, float y)
{
    currentFxX = juce::jlimit(0.0f, 1.0f, x);
    currentFxY = juce::jlimit(0.0f, 1.0f, y);

    // If the brake has fully ramped down and the user moves the pad again,
    // re-trigger the ramp from full speed for another brake hit.
    // The song never stopped, so we just reset the speed counter.
    if (currentFXType == FXType::Brake && brakeCurrentSpeed <= 0.0f)
    {
        resampleSource.setResamplingRatio(userSpeedRatio);
        brakeCurrentSpeed = 1.0f;
        // brakeDecrement already set — reuse it
    }
}