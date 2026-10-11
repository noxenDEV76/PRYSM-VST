#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include "SampleData.h"
#include "Parameters.h"
#include "Envelope.h"

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
    Dahdsr::Params env;
};

/** Lock-free values read by the UI (voice monitor, playhead). Written by the audio thread. */
struct EngineMonitor
{
    std::atomic<int>      activeVoices { 0 };
    std::atomic<float>    playhead { -1.0f };   // 0..1 across the whole sample, -1 = none
    std::atomic<float>    activity { 0.0f };    // smoothed output level for animations
    std::atomic<juce::uint64> heldLo { 0 }, heldHi { 0 }; // held MIDI keys (bit n = note n) for the on-screen keyboard
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
        double voiceRatio = 1.0;  // sampleRate ratio * note ratio
        Dahdsr adsr;
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

    double sr = 44100.0;
    EngineParams params;
    SamplePtr sample;
    std::array<Voice, kMaxVoices> voice;
    juce::uint64 ageCounter = 0;
    bool sustainPedal = false;
    float bendNorm = 0.0f; // -1..1

    // Smoothed controls
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smGain, smPan, smPitchSemis;

    // Per-chunk scratch (preallocated)
    std::array<float, kChunk> mixL {}, mixR {}, ratioArr {};
};
} // namespace prysm
