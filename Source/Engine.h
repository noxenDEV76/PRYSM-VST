#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include "SampleData.h"
#include "Parameters.h"

namespace prysm
{
/** Plain-data snapshot of every parameter the engine needs. Filled once per block by the processor. */
struct EngineParams
{
    float gainDb = 0.0f, pan = 0.0f;
    int   root = 60, pitchRange = 2, transpose = 0, voices = 8;
    float tuneCents = 0.0f;
    PlayMode mode = PlayMode::OneShot;
    bool  reverse = false;
    float start = 0.0f, end = 1.0f, velSens = 0.75f;
    float attack = 0.001f, decay = 0.4f, sustain = 1.0f, release = 0.08f;
    FilterMode filterMode = FilterMode::Off;
    float cutoff = 8000.0f, resonance = 0.2f, drive = 0.0f, width = 1.0f;
    float motion = 0.0f, motionRate = 2.0f;
    MotionDest motionDest = MotionDest::Filter;
    float random = 0.0f;
    int   randomSeed = 1;
    float reverbMix = 0.0f, reverbSize = 0.5f, tailMs = 0.0f;
};

/** Lock-free values read by the UI (voice monitor, playhead). Written by the audio thread. */
struct EngineMonitor
{
    std::atomic<int>      activeVoices { 0 };
    std::atomic<float>    peakL { 0.0f }, peakR { 0.0f };
    std::atomic<juce::uint32> midiCounter { 0 };
    std::atomic<bool>     overload { false };
    std::atomic<float>    playhead { -1.0f };   // 0..1 across the whole sample, -1 = none
    std::atomic<float>    activity { 0.0f };    // smoothed output level for animations
    std::atomic<juce::uint64> heldLo { 0 }, heldHi { 0 }; // held MIDI keys (bit n = note n) for the on-screen keyboard
};

/** Topology-preserving state-variable filter (stable under fast modulation). */
struct Svf
{
    float s1 = 0, s2 = 0, a1 = 0, a2 = 0, a3 = 0, k = 1;
    void reset() noexcept { s1 = s2 = 0; }
    void setup (float fc, float q, float sr) noexcept
    {
        const float g = std::tan (juce::MathConstants<float>::pi * juce::jlimit (10.0f, 0.46f * sr, fc) / sr);
        k  = 1.0f / q;
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
    inline float process (float x, bool highPass) noexcept
    {
        const float v3 = x - s2;
        const float v1 = a1 * s1 + a2 * v3;
        const float v2 = s2 + a2 * s1 + a3 * v3;
        s1 = 2.0f * v1 - s1;
        s2 = 2.0f * v2 - s2;
        return highPass ? x - k * v1 - v2 : v2;
    }
};

class SamplerEngine
{
public:
    static constexpr int kMaxVoices = 32;  // physical slots (user limit is <= 16; extra slots host fading voices)
    static constexpr int kChunk     = 256; // internal render chunk, independent of host block size

    SamplerEngine();

    void prepare (double sampleRate);
    void reset();

    void setParams (const EngineParams& p) noexcept { params = p; }

    /** Audio thread only. Replaces the sample used for NEW notes; playing voices keep their own reference. */
    void setSample (SamplePtr s) noexcept { sample = std::move (s); }
    const SamplePtr& getSample() const noexcept { return sample; }

    /** Renders `buffer` (stereo, replaced), consuming MIDI with sample-accurate timing. */
    void process (juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midi);

    int getActiveVoiceCount() const noexcept;
    float getLatencySamples() const noexcept { return oversampler.getLatencyInSamples(); }
    EngineMonitor monitor;

private:
    struct Voice
    {
        bool active = false, killing = false, noteHeld = false, pendingRelease = false;
        int note = 0;
        juce::uint64 age = 0;
        SamplePtr sample;
        double pos = 0.0, regionStart = 0.0, regionEnd = 0.0;
        bool reverse = false;
        float velGain = 1.0f;
        double voiceRatio = 1.0;  // sampleRate ratio * note ratio * random pitch
        juce::ADSR adsr;
        Svf svf[2];
        float lfoPhase = 0.0f;
        float randCutoffOct = 0.0f, randPan = 0.0f;
        float killGain = 1.0f;
    };

    void handleMidi (const juce::MidiMessage&);
    void noteOn (int note, float velocity);
    void noteOff (int note);
    void releaseVoice (Voice&);
    void allNotesOff (bool immediate);
    void startKill (Voice&);
    void renderChunk (float* outL, float* outR, int n);
    void renderVoice (Voice&, int n);
    float nextRandom() noexcept;  // uniform [-1, 1], deterministic stream

    double sr = 44100.0;
    EngineParams params;
    SamplePtr sample;
    std::array<Voice, kMaxVoices> voice;
    juce::uint64 ageCounter = 0;
    bool sustainPedal = false;
    float bendNorm = 0.0f; // -1..1

    // Smoothed controls
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smGain, smPan, smDrive, smWidth, smReverbMix, smResonance, smPitchSemis;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> smCutoff;

    // Per-chunk scratch (preallocated)
    std::array<float, kChunk> mixL {}, mixR {}, cutoffArr {}, resArr {}, ratioArr {}, tmpA {}, tmpB {};
    juce::AudioBuffer<float> fxBuffer;
    juce::dsp::Oversampling<float> oversampler;
    juce::Reverb reverb;
    juce::Reverb::Parameters reverbParams;
    float lastReverbSize = -1.0f;
    juce::int64 tailCounter = 0;
    juce::uint32 rngState = 1;
    int lastSeed = -1;
    float peakDecay = 0.0f;
};
} // namespace prysm
