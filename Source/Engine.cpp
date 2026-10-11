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

SamplerEngine::SamplerEngine() = default;

void SamplerEngine::prepare (double sampleRate)
{
    sr = sampleRate > 0 ? sampleRate : 44100.0;
    smGain.reset (sr, 0.02);  smPan.reset (sr, 0.02);
    smPitchSemis.reset (sr, 0.01);
    reset();
}

void SamplerEngine::reset()
{
    for (auto& v : voice) { v.active = false; v.sample.reset(); v.adsr.reset(); }
    sustainPedal = false;
    bendNorm = 0.0f;
    smGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (params.gainDb));
    smPan.setCurrentAndTargetValue (params.pan);
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

//==============================================================================
void SamplerEngine::process (juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midi)
{
    const int numSamples = buffer.getNumSamples();
    buffer.clear();
    if (buffer.getNumChannels() < 2) return;

    smGain.setTargetValue (juce::Decibels::decibelsToGain (params.gainDb, -48.0f));
    smPan.setTargetValue (params.pan);

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
    monitor.activity = juce::jmax (juce::jmax (pl, pr), monitor.activity.load() * decay);
    monitor.activeVoices = getActiveVoiceCount();
}

//==============================================================================
void SamplerEngine::handleMidi (const juce::MidiMessage& m)
{
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

    v.active = true; v.killing = false; v.noteHeld = true; v.pendingRelease = false;
    v.note = note; v.age = ++ageCounter;
    v.sample = sample;
    v.reverse = params.reverse;
    v.regionStart = s0; v.regionEnd = e0;
    v.pos = v.reverse ? e0 - 1.0 : s0;
    v.velGain = ((1.0f - params.velSens) + params.velSens * velocity);
    v.voiceRatio = (sample->sampleRate / sr) * std::exp2 ((double) note / 12.0);
    v.killGain = 1.0f;

    v.adsr.setSampleRate (sr);
    v.adsr.setParameters (params.env);
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

    v.adsr.setParameters (params.env);

    constexpr double fadeLen = 0.003 * 48000.0; // short fade at the end of the region (click prevention)
    const double fade = fadeLen * (sr / 48000.0);
    const double dir = v.reverse ? -1.0 : 1.0;

    for (int i = 0; i < n; ++i)
    {
        const double rate = v.voiceRatio * (double) ratioArr[(size_t) i];

        // --- read (Hermite)
        float l = hermite (dL, N, v.pos), r = (dR == dL) ? l : hermite (dR, N, v.pos);

        // --- amplitude
        float g = v.adsr.getNextSample() * v.velGain;
        const double remaining = v.reverse ? (v.pos - v.regionStart) : (v.regionEnd - v.pos); // source frames
        const double remainingOut = remaining / juce::jmax (1.0e-6, rate);
        if (remainingOut < fade) g *= (float) juce::jmax (0.0, remainingOut / fade);
        if (v.killing)
        {
            v.killGain -= 1.0f / (0.004f * (float) sr);
            if (v.killGain <= 0.0f) { v.killGain = 0.0f; v.active = false; }
            g *= v.killGain;
        }
        l *= g; r *= g;

        mixL[(size_t) i] += l;
        mixR[(size_t) i] += r;

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
    }

    bool anyActive = false;
    for (auto& v : voice)
        if (v.active) { anyActive = true; renderVoice (v, n); }

    if (anyActive)
    {
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

    // Master volume + pan
    for (int i = 0; i < n; ++i)
    {
        const float g = smGain.getNextValue();
        float pl, pr;
        panGains (smPan.getNextValue(), pl, pr);
        float a = mixL[(size_t) i] * g * pl, b = mixR[(size_t) i] * g * pr;
        if (! std::isfinite (a)) a = 0.0f;
        if (! std::isfinite (b)) b = 0.0f;
        outL[i] = a; outR[i] = b;
    }
}
} // namespace prysm
