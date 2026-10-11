#include "Engine.h"
#include <cmath>

namespace prysm
{
namespace
{
constexpr float kPi = juce::MathConstants<float>::pi;

inline float hermite (const float* d, int n, double pos) noexcept
{
    const int i1 = (int) pos;
    const float f = (float) (pos - (double) i1);
    const int i0 = juce::jmax (0, i1 - 1), i2 = juce::jmin (n - 1, i1 + 1), i3 = juce::jmin (n - 1, i1 + 2);
    const float y0 = d[i0], y1 = d[juce::jlimit (0, n - 1, i1)], y2 = d[i2], y3 = d[i3];
    const float c0 = y1;
    const float c1 = 0.5f * (y2 - y0);
    const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
    return ((c3 * f + c2) * f + c1) * f + c0;
}

inline void panGains (float pan, float& gl, float& gr) noexcept
{
    const float a = (juce::jlimit (-1.0f, 1.0f, pan) + 1.0f) * 0.25f * kPi;
    gl = std::cos (a) * 1.41421356f;
    gr = std::sin (a) * 1.41421356f;
}
} // namespace

SamplerEngine::SamplerEngine() : oversampler (2, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false) {}

void SamplerEngine::prepare (double sampleRate)
{
    sr = sampleRate > 0 ? sampleRate : 44100.0;
    fxBuffer.setSize (2, kChunk, false, true, true);
    oversampler.initProcessing ((size_t) kChunk);
    oversampler.reset();
    reverb.setSampleRate (sr);
    reverb.reset();
    smGain.reset (sr, 0.02);  smPan.reset (sr, 0.02);  smDrive.reset (sr, 0.02);
    smWidth.reset (sr, 0.02); smReverbMix.reset (sr, 0.03); smResonance.reset (sr, 0.02);
    smPitchSemis.reset (sr, 0.01);
    smCutoff.reset (sr, 0.015);
    reset();
}

void SamplerEngine::reset()
{
    for (auto& v : voice) { v.active = false; v.sample.reset(); v.adsr.reset(); v.svf[0].reset(); v.svf[1].reset(); }
    sustainPedal = false;
    bendNorm = 0.0f;
    tailCounter = 0;
    lastReverbSize = -1.0f;
    lastSeed = -1;
    oversampler.reset();
    reverb.reset();
    smGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (params.gainDb));
    smPan.setCurrentAndTargetValue (params.pan);
    smDrive.setCurrentAndTargetValue (params.drive);
    smWidth.setCurrentAndTargetValue (params.width);
    smReverbMix.setCurrentAndTargetValue (params.reverbMix);
    smResonance.setCurrentAndTargetValue (params.resonance);
    smCutoff.setCurrentAndTargetValue (juce::jmax (20.0f, params.cutoff));
    smPitchSemis.setCurrentAndTargetValue ((float) (params.transpose) + params.tuneCents * 0.01f - (float) params.root);
    monitor.activeVoices = 0;
    monitor.playhead = -1.0f;
}

int SamplerEngine::getActiveVoiceCount() const noexcept
{
    int c = 0;
    for (auto& v : voice) if (v.active) ++c;
    return c;
}

float SamplerEngine::nextRandom() noexcept
{
    // xorshift32 -> [-1, 1]
    rngState ^= rngState << 13; rngState ^= rngState >> 17; rngState ^= rngState << 5;
    return (float) ((double) rngState / 2147483647.5 - 1.0);
}

//==============================================================================
void SamplerEngine::process (juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midi)
{
    const int numSamples = buffer.getNumSamples();
    buffer.clear();
    if (buffer.getNumChannels() < 2) return;

    if (lastSeed != params.randomSeed)
    {
        lastSeed = params.randomSeed;
        rngState = 2463534242u ^ (juce::uint32) (params.randomSeed * 2654435761u + 1u);
        if (rngState == 0) rngState = 1;
    }

    smGain.setTargetValue (juce::Decibels::decibelsToGain (params.gainDb, -48.0f));
    smPan.setTargetValue (params.pan);
    smDrive.setTargetValue (params.drive);
    smWidth.setTargetValue (params.width);
    smReverbMix.setTargetValue (params.reverbMix);
    smResonance.setTargetValue (params.resonance);
    smCutoff.setTargetValue (juce::jlimit (20.0f, 20000.0f, params.cutoff));

    auto* outL = buffer.getWritePointer (0);
    auto* outR = buffer.getWritePointer (1);

    auto it = midi.begin();
    const auto endIt = midi.end();
    int pos = 0;
    while (pos < numSamples)
    {
        while (it != endIt && (*it).samplePosition <= pos)
        {
            handleMidi ((*it).getMessage());
            ++it;
        }
        int next = numSamples;
        if (it != endIt) next = juce::jlimit (pos + 1, numSamples, (*it).samplePosition);
        int n = juce::jmin (next - pos, kChunk);
        renderChunk (outL + pos, outR + pos, n);
        pos += n;
    }
    for (; it != endIt; ++it) handleMidi ((*it).getMessage()); // events at/after the block end

    // Monitor
    const float pl = buffer.getMagnitude (0, 0, numSamples);
    const float pr = buffer.getMagnitude (1, 0, numSamples);
    const float decay = std::pow (0.5f, (float) numSamples / (float) sr / 0.25f);
    monitor.peakL = juce::jmax (pl, monitor.peakL.load() * decay);
    monitor.peakR = juce::jmax (pr, monitor.peakR.load() * decay);
    monitor.activity = juce::jmax (juce::jmax (pl, pr), monitor.activity.load() * decay);
    if (pl > 1.0f || pr > 1.0f) monitor.overload = true;
    monitor.activeVoices = getActiveVoiceCount();
}

//==============================================================================
void SamplerEngine::handleMidi (const juce::MidiMessage& m)
{
    monitor.midiCounter.fetch_add (1, std::memory_order_relaxed);
    auto setHeld = [this] (int n, bool on)
    {
        auto& word = n < 64 ? monitor.heldLo : monitor.heldHi;
        const juce::uint64 bit = (juce::uint64) 1 << (n & 63);
        if (on) word.fetch_or (bit, std::memory_order_relaxed); else word.fetch_and (~bit, std::memory_order_relaxed);
    };
    if (m.isNoteOn())            { setHeld (m.getNoteNumber(), true);  noteOn (m.getNoteNumber(), m.getFloatVelocity()); }
    else if (m.isNoteOff())      { setHeld (m.getNoteNumber(), false); noteOff (m.getNoteNumber()); }
    else if (m.isPitchWheel())   bendNorm = (float) (m.getPitchWheelValue() - 8192) / 8192.0f;
    else if (m.isAllNotesOff())  { monitor.heldLo = 0; monitor.heldHi = 0; allNotesOff (false); }
    else if (m.isAllSoundOff())  { monitor.heldLo = 0; monitor.heldHi = 0; allNotesOff (true); }
    else if (m.isController() && m.getControllerNumber() == 64)
    {
        const bool down = m.getControllerValue() >= 64;
        if (sustainPedal && ! down)
            for (auto& v : voice)
                if (v.active && v.pendingRelease) { v.pendingRelease = false; releaseVoice (v); }
        sustainPedal = down;
    }
}

void SamplerEngine::startKill (Voice& v)
{
    v.killing = true;
}

void SamplerEngine::releaseVoice (Voice& v)
{
    v.noteHeld = false;
    if (params.mode == PlayMode::Gate)
        v.adsr.noteOff();
    // ONE-SHOT: the sample always plays to its end; note-off is ignored.
}

void SamplerEngine::allNotesOff (bool immediate)
{
    for (auto& v : voice)
    {
        if (! v.active) continue;
        if (immediate) startKill (v);
        else { v.pendingRelease = false; v.adsr.noteOff(); v.noteHeld = false; }
    }
}

void SamplerEngine::noteOn (int note, float velocity)
{
    if (sample == nullptr || sample->numFrames() < 2) return;

    // Enforce the user polyphony limit by fading out the oldest voice(s).
    const int limit = juce::jlimit (1, 16, params.voices);
    for (;;)
    {
        int count = 0;
        Voice* oldest = nullptr;
        for (auto& v : voice)
            if (v.active && ! v.killing)
            {
                ++count;
                if (oldest == nullptr || v.age < oldest->age) oldest = &v;
            }
        if (count < limit || oldest == nullptr) break;
        startKill (*oldest);
    }

    Voice* slot = nullptr;
    for (auto& v : voice) if (! v.active) { slot = &v; break; }
    if (slot == nullptr) // every physical slot busy (extreme case): reuse the oldest one
    {
        slot = &voice[0];
        for (auto& v : voice) if (v.age < slot->age) slot = &v;
    }

    Voice& v = *slot;
    const int N = sample->numFrames();
    double s0 = std::floor ((double) juce::jlimit (0.0f, 1.0f, params.start) * N);
    double e0 = std::ceil  ((double) juce::jlimit (0.0f, 1.0f, params.end)   * N);
    if (e0 < s0) std::swap (s0, e0);
    if (e0 - s0 < 16.0) { e0 = juce::jmin ((double) N, s0 + 16.0); s0 = juce::jmax (0.0, e0 - 16.0); }

    const float rnd = params.random;
    const float rPitchCents = nextRandom() * rnd * 25.0f;
    v.randCutoffOct = nextRandom() * rnd * 1.0f;
    v.randPan       = nextRandom() * rnd * 0.4f;
    const float rGainDb = nextRandom() * rnd * 2.0f;

    v.active = true; v.killing = false; v.noteHeld = true; v.pendingRelease = false;
    v.note = note; v.age = ++ageCounter;
    v.sample = sample;
    v.reverse = params.reverse;
    v.regionStart = s0; v.regionEnd = e0;
    v.pos = v.reverse ? e0 - 1.0 : s0;
    v.velGain = ((1.0f - params.velSens) + params.velSens * velocity) * juce::Decibels::decibelsToGain (rGainDb);
    v.voiceRatio = (sample->sampleRate / sr) * std::exp2 ((double) note / 12.0 + (double) rPitchCents / 1200.0);
    v.killGain = 1.0f;
    v.lfoPhase = 0.0f;
    v.svf[0].reset(); v.svf[1].reset();

    juce::ADSR::Parameters ap;
    ap.attack = params.attack; ap.decay = params.decay; ap.sustain = params.sustain; ap.release = params.release;
    v.adsr.setSampleRate (sr);
    v.adsr.setParameters (ap);
    v.adsr.noteOn();
    monitor.playhead = (float) (v.pos / N);
}

void SamplerEngine::noteOff (int note)
{
    for (auto& v : voice)
        if (v.active && v.noteHeld && v.note == note && ! v.killing)
        {
            if (sustainPedal) v.pendingRelease = true;
            else releaseVoice (v);
        }
}

//==============================================================================
void SamplerEngine::renderVoice (Voice& v, int n)
{
    const SampleData& s = *v.sample;
    const int N = s.numFrames();
    const float* dL = s.audio.getReadPointer (0);
    const float* dR = s.numChannels() > 1 ? s.audio.getReadPointer (1) : dL;

    juce::ADSR::Parameters ap;
    ap.attack = params.attack; ap.decay = params.decay; ap.sustain = params.sustain; ap.release = params.release;
    v.adsr.setParameters (ap);

    const bool filterOn = params.filterMode != FilterMode::Off;
    const bool highPass = params.filterMode == FilterMode::HighPass;
    const float motionAmt = params.motion;
    const bool motionOn = motionAmt > 0.0001f;
    const float lfoInc = params.motionRate / (float) sr;
    const double fadeLen = juce::jmax (0.003, (double) params.tailMs * 0.001) * sr; // output frames
    const double dir = v.reverse ? -1.0 : 1.0;

    float panL, panR;
    panGains (v.randPan, panL, panR);

    for (int i = 0; i < n; ++i)
    {
        // --- LFO (per voice, restarts on note-on so every note moves identically)
        float lfo = 0.0f;
        if (motionOn)
        {
            lfo = std::sin (2.0f * kPi * v.lfoPhase);
            v.lfoPhase += lfoInc; if (v.lfoPhase >= 1.0f) v.lfoPhase -= 1.0f;
        }

        // --- pitch
        double rate = v.voiceRatio * (double) ratioArr[(size_t) i];
        if (motionOn && params.motionDest == MotionDest::Pitch)
            rate *= std::exp2 ((double) (lfo * motionAmt) / 12.0); // up to +/- 1 semitone

        // --- read (Hermite)
        float l = hermite (dL, N, v.pos), r = (dR == dL) ? l : hermite (dR, N, v.pos);

        // --- filter (coefficients refreshed every 8 samples)
        if (filterOn)
        {
            if ((i & 7) == 0)
            {
                float fc = cutoffArr[(size_t) i] * std::exp2 (v.randCutoffOct);
                if (motionOn && params.motionDest == MotionDest::Filter) fc *= std::exp2 (lfo * motionAmt * 3.0f);
                const float q = 0.707f * std::pow (18.0f, resArr[(size_t) i]);
                v.svf[0].setup (fc, q, (float) sr);
                v.svf[1].a1 = v.svf[0].a1; v.svf[1].a2 = v.svf[0].a2; v.svf[1].a3 = v.svf[0].a3; v.svf[1].k = v.svf[0].k;
            }
            l = v.svf[0].process (l, highPass);
            r = v.svf[1].process (r, highPass);
        }

        // --- amplitude
        float g = v.adsr.getNextSample() * v.velGain;
        if (motionOn && params.motionDest == MotionDest::Amp)
            g *= 1.0f - motionAmt * 0.5f * (1.0f - lfo);
        const double remaining = v.reverse ? (v.pos - v.regionStart) : (v.regionEnd - v.pos); // source frames
        const double remainingOut = remaining / juce::jmax (1.0e-6, rate);
        if (remainingOut < fadeLen) g *= (float) juce::jmax (0.0, remainingOut / fadeLen);
        if (v.killing)
        {
            v.killGain -= 1.0f / (0.004f * (float) sr);
            if (v.killGain <= 0.0f) { v.killGain = 0.0f; v.active = false; }
            g *= v.killGain;
        }
        l *= g; r *= g;

        // --- pan (per voice)
        if (motionOn && params.motionDest == MotionDest::Pan)
            panGains (v.randPan + lfo * motionAmt, panL, panR);
        mixL[(size_t) i] += l * panL;
        mixR[(size_t) i] += r * panR;

        // --- advance / end
        v.pos += dir * rate;
        if (remaining <= 0.0 || (v.reverse ? v.pos < v.regionStart : v.pos >= v.regionEnd))
            v.active = false;
        if (v.active && ! v.adsr.isActive()) v.active = false;
        if (! v.active) break;
    }
}

void SamplerEngine::renderChunk (float* outL, float* outR, int n)
{
    std::fill (mixL.begin(), mixL.begin() + n, 0.0f);
    std::fill (mixR.begin(), mixR.begin() + n, 0.0f);

    // Shared per-sample control signals
    const float semisTarget = (float) params.transpose + params.tuneCents * 0.01f - (float) params.root;
    smPitchSemis.setTargetValue (semisTarget);
    const float bendSemis = bendNorm * (float) params.pitchRange;
    for (int i = 0; i < n; ++i)
    {
        ratioArr[(size_t) i]  = std::exp2 ((smPitchSemis.getNextValue() + bendSemis) / 12.0f);
        cutoffArr[(size_t) i] = smCutoff.getNextValue();
        resArr[(size_t) i]    = smResonance.getNextValue();
    }

    bool anyActive = false;
    for (auto& v : voice)
        if (v.active) { anyActive = true; renderVoice (v, n); }

    if (anyActive)
    {
        tailCounter = (juce::int64) ((1.5 + 8.0 * params.reverbSize) * sr);
        // newest voice playhead
        const Voice* newest = nullptr;
        for (auto& v : voice) if (v.active && ! v.killing && (newest == nullptr || v.age > newest->age)) newest = &v;
        monitor.playhead = newest ? (float) (newest->pos / juce::jmax (1, newest->sample->numFrames())) : -1.0f;
    }
    else
        monitor.playhead = -1.0f;

    // Drop references held by finished voices (the processor's keep-alive list guarantees
    // this only decrements a counter and never frees memory on the audio thread).
    for (auto& v : voice) if (! v.active && v.sample != nullptr) v.sample.reset();

    if (! anyActive && tailCounter <= 0)
    {
        std::fill (outL, outL + n, 0.0f);
        std::fill (outR, outR + n, 0.0f);
        smGain.skip (n); smPan.skip (n); smDrive.skip (n); smWidth.skip (n); smReverbMix.skip (n);
        return;
    }
    if (! anyActive) tailCounter -= n;

    // --- Global FX chain on fxBuffer
    float* fl = fxBuffer.getWritePointer (0);
    float* fr = fxBuffer.getWritePointer (1);
    std::copy (mixL.begin(), mixL.begin() + n, fl);
    std::copy (mixR.begin(), mixR.begin() + n, fr);

    // Drive (2x oversampled tanh, level-compensated, dry/wet blended so 0 = transparent)
    {
        juce::dsp::AudioBlock<float> block (fxBuffer);
        auto sub = block.getSubBlock (0, (size_t) n);
        auto up = oversampler.processSamplesUp (sub);
        const size_t un = up.getNumSamples();
        float* ul = up.getChannelPointer (0);
        float* ur = up.getChannelPointer (1);
        // drive amount is constant over the chunk after smoothing evaluation
        float driveAmt = 0.0f;
        for (int i = 0; i < n; ++i) driveAmt = smDrive.getNextValue();
        const float g = 1.0f + 24.0f * driveAmt * driveAmt;
        const float comp = (0.5f / std::tanh (0.5f * g)) / (1.0f + 0.06f * (g - 1.0f)); // level-matched to the dry signal
        for (size_t i = 0; i < un; ++i)
        {
            const float dl = ul[i], dr = ur[i];
            ul[i] = (1.0f - driveAmt) * dl + driveAmt * std::tanh (g * dl) * comp;
            ur[i] = (1.0f - driveAmt) * dr + driveAmt * std::tanh (g * dr) * comp;
        }
        oversampler.processSamplesDown (sub);
    }

    // Stereo width (mid/side; mono sum is unchanged => mono compatible)
    for (int i = 0; i < n; ++i)
    {
        const float w = smWidth.getNextValue();
        const float m = 0.5f * (fl[i] + fr[i]);
        const float sd = 0.5f * (fl[i] - fr[i]) * w;
        fl[i] = m + sd; fr[i] = m - sd;
    }

    // Reverb (send style: wet only, blended by mix)
    {
        if (std::abs (lastReverbSize - params.reverbSize) > 1.0e-4f)
        {
            lastReverbSize = params.reverbSize;
            reverbParams.roomSize = 0.2f + 0.78f * params.reverbSize;
            reverbParams.damping = 0.45f;
            reverbParams.wetLevel = 0.33f;
            reverbParams.dryLevel = 0.0f;
            reverbParams.width = 1.0f;
            reverbParams.freezeMode = 0.0f;
            reverb.setParameters (reverbParams);
        }
        std::copy (fl, fl + n, tmpA.begin());
        std::copy (fr, fr + n, tmpB.begin());
        reverb.processStereo (tmpA.data(), tmpB.data(), n);
        for (int i = 0; i < n; ++i)
        {
            const float mix = smReverbMix.getNextValue();
            fl[i] += mix * tmpA[(size_t) i];
            fr[i] += mix * tmpB[(size_t) i];
        }
    }

    // Master volume + pan
    for (int i = 0; i < n; ++i)
    {
        const float g = smGain.getNextValue();
        float pl, pr;
        panGains (smPan.getNextValue(), pl, pr);
        float a = fl[i] * g * pl, b = fr[i] * g * pr;
        if (! std::isfinite (a)) a = 0.0f;
        if (! std::isfinite (b)) b = 0.0f;
        outL[i] = a; outR[i] = b;
    }
}
} // namespace prysm
