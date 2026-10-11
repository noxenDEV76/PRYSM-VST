#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

namespace prysm
{
namespace ids
{
    // Sample / pitch
    inline constexpr const char* gain       = "gain";
    inline constexpr const char* pan        = "pan";
    inline constexpr const char* root       = "root";
    inline constexpr const char* tune       = "tune";
    inline constexpr const char* pitchRange = "pitchRange";
    inline constexpr const char* transpose  = "transpose";
    inline constexpr const char* mode       = "mode";
    inline constexpr const char* reverse    = "reverse";
    inline constexpr const char* start      = "start";
    inline constexpr const char* end        = "end";
    inline constexpr const char* velSens    = "velSens";
    inline constexpr const char* voices     = "voices";
    // Envelope
    inline constexpr const char* attack     = "attack";
    inline constexpr const char* decay      = "decay";
    inline constexpr const char* sustain    = "sustain";
    inline constexpr const char* release    = "release";
    // Mutation
    inline constexpr const char* filterType = "filterType";
    inline constexpr const char* cutoff     = "cutoff";
    inline constexpr const char* resonance  = "resonance";
    inline constexpr const char* drive      = "drive";
    inline constexpr const char* width      = "width";
    inline constexpr const char* motion     = "motion";
    inline constexpr const char* motionRate = "motionRate";
    inline constexpr const char* motionDest = "motionDest";
    inline constexpr const char* random     = "random";
    inline constexpr const char* randomSeed = "randomSeed";
    inline constexpr const char* reverbMix  = "reverbMix";
    inline constexpr const char* reverbSize = "reverbSize";
    inline constexpr const char* tail       = "tail";
}

enum class PlayMode   { OneShot = 0, Gate = 1 };
enum class FilterMode { Off = 0, LowPass = 1, HighPass = 2 };
enum class MotionDest { Filter = 0, Pitch = 1, Amp = 2, Pan = 3 };

/** MIDI note name using the FL Studio convention (MIDI 60 = C5). */
inline juce::String noteName (int midi)
{
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    midi = juce::jlimit (0, 127, midi);
    return juce::String (names[midi % 12]) + juce::String (midi / 12);
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using namespace juce;
    using P  = AudioParameterFloat;
    using PI = AudioParameterInt;
    using PC = AudioParameterChoice;
    using PB = AudioParameterBool;
    using Attr = AudioParameterFloatAttributes;
    constexpr int v = 1;

    auto secs = [] (float lo, float hi, float mid)
    {
        NormalisableRange<float> r (lo, hi);
        r.setSkewForCentre (mid);
        return r;
    };
    auto secsText = [] (float x, int) { return x < 1.0f ? String (roundToInt (x * 1000.0f)) + " ms" : String (x, 2) + " s"; };
    auto pct      = [] (float x, int) { return String (roundToInt (x * 100.0f)) + " %"; };

    AudioProcessorValueTreeState::ParameterLayout l;

    l.add (std::make_unique<P> (ParameterID { ids::gain, v }, "Volume", NormalisableRange<float> (-48.0f, 12.0f, 0.1f, 2.0f), 0.0f,
                                Attr().withStringFromValueFunction ([] (float x, int) { return String (x, 1) + " dB"; })));
    l.add (std::make_unique<P> (ParameterID { ids::pan, v }, "Pan", NormalisableRange<float> (-1.0f, 1.0f, 0.001f), 0.0f,
                                Attr().withStringFromValueFunction ([] (float x, int) { return x == 0.0f ? String ("C") : (x < 0 ? "L " : "R ") + String (roundToInt (std::abs (x) * 100.0f)); })));
    l.add (std::make_unique<PI> (ParameterID { ids::root, v }, "Root Note", 0, 127, 60,
                                 AudioParameterIntAttributes().withStringFromValueFunction ([] (int x, int) { return noteName (x); })));
    l.add (std::make_unique<P> (ParameterID { ids::tune, v }, "Tune", NormalisableRange<float> (-100.0f, 100.0f, 0.1f), 0.0f,
                                Attr().withStringFromValueFunction ([] (float x, int) { return String (x, 1) + " ct"; })));
    l.add (std::make_unique<PI> (ParameterID { ids::pitchRange, v }, "Pitch Range", 0, 24, 2));
    l.add (std::make_unique<PI> (ParameterID { ids::transpose, v }, "Transpose", -24, 24, 0));
    l.add (std::make_unique<PC> (ParameterID { ids::mode, v }, "Playback Mode", StringArray { "ONE-SHOT", "GATE" }, 0));
    l.add (std::make_unique<PB> (ParameterID { ids::reverse, v }, "Reverse", false));
    l.add (std::make_unique<P> (ParameterID { ids::start, v }, "Start", NormalisableRange<float> (0.0f, 1.0f, 0.0001f), 0.0f,
                                Attr().withStringFromValueFunction (pct)));
    l.add (std::make_unique<P> (ParameterID { ids::end, v }, "End", NormalisableRange<float> (0.0f, 1.0f, 0.0001f), 1.0f,
                                Attr().withStringFromValueFunction (pct)));
    l.add (std::make_unique<P> (ParameterID { ids::velSens, v }, "Velocity Sensitivity", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.75f,
                                Attr().withStringFromValueFunction (pct)));
    l.add (std::make_unique<PI> (ParameterID { ids::voices, v }, "Voices", 1, 16, 8));

    l.add (std::make_unique<P> (ParameterID { ids::attack, v }, "Attack", secs (0.001f, 5.0f, 0.15f), 0.001f, Attr().withStringFromValueFunction (secsText)));
    l.add (std::make_unique<P> (ParameterID { ids::decay, v }, "Decay", secs (0.001f, 5.0f, 0.4f), 0.4f, Attr().withStringFromValueFunction (secsText)));
    l.add (std::make_unique<P> (ParameterID { ids::sustain, v }, "Sustain", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 1.0f, Attr().withStringFromValueFunction (pct)));
    l.add (std::make_unique<P> (ParameterID { ids::release, v }, "Release", secs (0.003f, 8.0f, 0.5f), 0.08f, Attr().withStringFromValueFunction (secsText)));

    l.add (std::make_unique<PC> (ParameterID { ids::filterType, v }, "Filter Type", StringArray { "OFF", "LOW-PASS", "HIGH-PASS" }, 0));
    l.add (std::make_unique<P> (ParameterID { ids::cutoff, v }, "Filter Cutoff", secs (20.0f, 20000.0f, 1500.0f), 8000.0f,
                                Attr().withStringFromValueFunction ([] (float x, int) { return x >= 1000.0f ? String (x / 1000.0f, 2) + " kHz" : String (roundToInt (x)) + " Hz"; })));
    l.add (std::make_unique<P> (ParameterID { ids::resonance, v }, "Filter Resonance", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.2f, Attr().withStringFromValueFunction (pct)));
    l.add (std::make_unique<P> (ParameterID { ids::drive, v }, "Drive", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f, Attr().withStringFromValueFunction (pct)));
    l.add (std::make_unique<P> (ParameterID { ids::width, v }, "Stereo Width", NormalisableRange<float> (0.0f, 2.0f, 0.001f), 1.0f, Attr().withStringFromValueFunction ([] (float x, int) { return String (roundToInt (x * 100.0f)) + " %"; })));
    l.add (std::make_unique<P> (ParameterID { ids::motion, v }, "Motion Amount", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f, Attr().withStringFromValueFunction (pct)));
    l.add (std::make_unique<P> (ParameterID { ids::motionRate, v }, "Motion Rate", secs (0.05f, 20.0f, 2.0f), 2.0f, Attr().withStringFromValueFunction ([] (float x, int) { return String (x, 2) + " Hz"; })));
    l.add (std::make_unique<PC> (ParameterID { ids::motionDest, v }, "Motion Target", StringArray { "FILTER", "PITCH", "AMP", "PAN" }, 0));
    l.add (std::make_unique<P> (ParameterID { ids::random, v }, "Random Amount", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f, Attr().withStringFromValueFunction (pct)));
    l.add (std::make_unique<PI> (ParameterID { ids::randomSeed, v }, "Random Seed", 0, 999, 1));
    l.add (std::make_unique<P> (ParameterID { ids::reverbMix, v }, "Reverb Mix", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f, Attr().withStringFromValueFunction (pct)));
    l.add (std::make_unique<P> (ParameterID { ids::reverbSize, v }, "Reverb Size", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f, Attr().withStringFromValueFunction (pct)));
    l.add (std::make_unique<P> (ParameterID { ids::tail, v }, "Tail", secs (0.0f, 2000.0f, 150.0f), 0.0f,
                                Attr().withStringFromValueFunction ([] (float x, int) { return String (roundToInt (x)) + " ms"; })));
    return l;
}
} // namespace prysm
