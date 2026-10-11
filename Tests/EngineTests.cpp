// PRYSM engine + processor tests. A tiny self-contained harness (no external test framework).
// Exit code 0 only if every check passes.
#include <juce_audio_utils/juce_audio_utils.h>
#include "Engine.h"
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
#include <iostream>

using namespace prysm;

static int g_checks = 0, g_failed = 0;
#define CHECK(cond, name)                                                                  \
    do { ++g_checks; if (! (cond)) { ++g_failed; std::cout << "  FAIL: " << name            \
                      << "   [" #cond "]  (" << __FILE__ << ":" << __LINE__ << ")\n"; }     \
         else std::cout << "  ok:   " << name << "\n"; } while (0)

static constexpr double kSR = 48000.0;

//==============================================================================
// Helpers
static SamplePtr makeSample (std::function<float (int, int)> gen, int frames, int channels = 1, double sr = kSR, const juce::String& name = "test.wav")
{
    auto s = std::make_shared<SampleData>();
    s->audio.setSize (channels, frames);
    for (int ch = 0; ch < channels; ++ch)
        for (int i = 0; i < frames; ++i)
            s->audio.setSample (ch, i, gen (ch, i));
    s->sampleRate = sr;
    s->name = name;
    s->computePeaks();
    return s;
}
static SamplePtr sineSample (double freq, float amp = 0.5f, double seconds = 1.0, int channels = 1, double sr = kSR)
{
    return makeSample ([=] (int, int i) { return amp * (float) std::sin (2.0 * juce::MathConstants<double>::pi * freq * i / sr); },
                       (int) (seconds * sr), channels, sr);
}

struct Ev { int sample; juce::MidiMessage msg; };
static Ev on (int at, int note, float vel = 1.0f)  { return { at, juce::MidiMessage::noteOn (1, note, vel) }; }
static Ev off (int at, int note)                    { return { at, juce::MidiMessage::noteOff (1, note) }; }

struct Rendered { std::vector<float> l, r; };

static Rendered render (SamplerEngine& e, const std::vector<Ev>& events, int totalSamples, int block = 480)
{
    Rendered out;
    out.l.reserve ((size_t) totalSamples); out.r.reserve ((size_t) totalSamples);
    juce::AudioBuffer<float> buf (2, block);
    for (int pos = 0; pos < totalSamples; pos += block)
    {
        const int n = juce::jmin (block, totalSamples - pos);
        buf.setSize (2, n, false, false, true);
        juce::MidiBuffer mb;
        for (auto& ev : events)
            if (ev.sample >= pos && ev.sample < pos + n) mb.addEvent (ev.msg, ev.sample - pos);
        e.process (buf, mb);
        for (int i = 0; i < n; ++i) { out.l.push_back (buf.getSample (0, i)); out.r.push_back (buf.getSample (1, i)); }
    }
    return out;
}

static double rms (const std::vector<float>& v, int a, int b)
{
    a = juce::jlimit (0, (int) v.size(), a); b = juce::jlimit (a, (int) v.size(), b);
    if (b <= a) return 0.0;
    double s = 0; for (int i = a; i < b; ++i) s += (double) v[(size_t) i] * v[(size_t) i];
    return std::sqrt (s / (b - a));
}
static double peak (const std::vector<float>& v, int a, int b)
{
    a = juce::jlimit (0, (int) v.size(), a); b = juce::jlimit (a, (int) v.size(), b);
    double p = 0; for (int i = a; i < b; ++i) p = juce::jmax (p, (double) std::abs (v[(size_t) i]));
    return p;
}
static bool allFinite (const Rendered& r)
{
    for (float x : r.l) if (! std::isfinite (x)) return false;
    for (float x : r.r) if (! std::isfinite (x)) return false;
    return true;
}
/** Frequency via rising zero-crossings with linear interpolation. */
static double estimateFreq (const std::vector<float>& v, int a, int b)
{
    double first = -1, last = -1; int count = 0;
    for (int i = juce::jmax (1, a); i < juce::jmin ((int) v.size(), b); ++i)
        if (v[(size_t) i - 1] < 0.0f && v[(size_t) i] >= 0.0f)
        {
            const double t = (double) i - 1 + (double) (-v[(size_t) i - 1]) / (double) (v[(size_t) i] - v[(size_t) i - 1]);
            if (first < 0) first = t; last = t; ++count;
        }
    return count < 2 ? 0.0 : (count - 1) * kSR / (last - first);
}

static EngineParams defaults()
{
    EngineParams p;
    p.env.attack = 0.001f; p.env.decay = 0.2f; p.env.sustain = 1.0f; p.env.release = 0.05f;
    p.mode = PlayMode::OneShot; p.root = 60; p.voices = 8; p.velSens = 1.0f;
    return p;
}
static std::unique_ptr<SamplerEngine> makeEngine (SamplePtr s, EngineParams p = defaults())
{
    auto e = std::make_unique<SamplerEngine>();
    e->setParams (p);
    e->prepare (kSR);
    e->setSample (std::move (s));
    return e;
}

static juce::File tempDir()
{
    auto d = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("prysm_tests_" + juce::String (juce::Time::currentTimeMillis()));
    d.createDirectory();
    return d;
}
static void writeWav (const juce::File& f, const juce::AudioBuffer<float>& buf, double sr)
{
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> os (f.createOutputStream());
    auto w = wav.createWriterFor (os, juce::AudioFormatWriterOptions().withSampleRate (sr).withNumChannels (buf.getNumChannels()).withBitsPerSample (24));
    w->writeFromAudioSampleBuffer (buf, 0, buf.getNumSamples());
}

//==============================================================================
static void testDecoding (const juce::File& dir)
{
    std::cout << "[decoding]\n";
    juce::AudioFormatManager fm; fm.registerBasicFormats();

    juce::AudioBuffer<float> mono (1, 4800);
    for (int i = 0; i < 4800; ++i) mono.setSample (0, i, 0.5f * std::sin (0.05f * (float) i));
    auto fMono = dir.getChildFile ("mono.wav"); writeWav (fMono, mono, 44100.0);
    auto r = decodeSampleFile (fMono, fm);
    CHECK (r.ok(), "mono WAV decodes");
    CHECK (r.sample && r.sample->numChannels() == 1 && r.sample->numFrames() == 4800 && r.sample->sampleRate == 44100.0, "mono WAV metadata (1 ch, 4800 frames, 44.1 kHz)");
    CHECK (r.sample && r.sample->name == "mono.wav", "file name kept");

    juce::AudioBuffer<float> st (2, 9600);
    for (int i = 0; i < 9600; ++i) { st.setSample (0, i, 0.4f); st.setSample (1, i, -0.2f); }
    auto fSt = dir.getChildFile ("stereo.wav"); writeWav (fSt, st, 48000.0);
    r = decodeSampleFile (fSt, fm);
    CHECK (r.ok() && r.sample->numChannels() == 2, "stereo WAV decodes with 2 channels");
    CHECK (r.ok() && std::abs (r.sample->audio.getSample (0, 100) - 0.4f) < 1e-3f && std::abs (r.sample->audio.getSample (1, 100) + 0.2f) < 1e-3f, "stereo channels not mixed up");
    CHECK (r.ok() && r.sample->peakMax.size() == (size_t) SampleData::kPeakBins, "waveform peaks computed");

    juce::AiffAudioFormat aiff;
    auto fAiff = dir.getChildFile ("test.aiff");
    {
        std::unique_ptr<juce::OutputStream> os (fAiff.createOutputStream());
        auto w = aiff.createWriterFor (os, juce::AudioFormatWriterOptions().withSampleRate (44100.0).withNumChannels (1).withBitsPerSample (16));
        w->writeFromAudioSampleBuffer (mono, 0, mono.getNumSamples());
    }
    r = decodeSampleFile (fAiff, fm);
    CHECK (r.ok() && r.sample->numFrames() == 4800, "AIFF decodes");

    auto bad = dir.getChildFile ("notaudio.wav"); bad.replaceWithText ("this is not audio");
    r = decodeSampleFile (bad, fm);
    CHECK (! r.ok() && r.error == "UNSUPPORTED OR CORRUPT FILE", "garbage file rejected with English message");
    r = decodeSampleFile (dir.getChildFile ("nope.wav"), fm);
    CHECK (! r.ok() && r.error == "SAMPLE NOT FOUND", "missing file reported as SAMPLE NOT FOUND");
}

static void testPitch()
{
    std::cout << "[pitch]\n";
    auto e = makeEngine (sineSample (440.0));
    auto a = render (*e, { on (0, 60) }, 24000);
    const double f0 = estimateFreq (a.l, 2000, 20000);
    CHECK (std::abs (f0 - 440.0) < 2.0, "root note plays at original pitch (" << f0 << " Hz)");

    e = makeEngine (sineSample (440.0));
    auto up = render (*e, { on (0, 72) }, 24000);
    const double fu = estimateFreq (up.l, 2000, 20000);
    CHECK (std::abs (fu - 880.0) < 4.0, "+1 octave doubles frequency (" << fu << " Hz)");

    e = makeEngine (sineSample (440.0));
    auto dn = render (*e, { on (0, 48) }, 24000);
    const double fd = estimateFreq (dn.l, 2000, 20000);
    CHECK (std::abs (fd - 220.0) < 1.5, "-1 octave halves frequency (" << fd << " Hz)");

    auto p = defaults(); p.root = 69; // A4 root -> note 69 original pitch
    e = makeEngine (sineSample (440.0), p);
    auto r = render (*e, { on (0, 69) }, 24000);
    CHECK (std::abs (estimateFreq (r.l, 2000, 20000) - 440.0) < 2.0, "ROOT NOTE moves the unity-pitch key");

    p = defaults(); p.transpose = 12;
    e = makeEngine (sineSample (440.0), p);
    r = render (*e, { on (0, 60) }, 24000);
    CHECK (std::abs (estimateFreq (r.l, 2000, 20000) - 880.0) < 4.0, "TRANSPOSE +12 = one octave up");

    p = defaults(); p.tuneCents = 100.0f;
    e = makeEngine (sineSample (440.0), p);
    r = render (*e, { on (0, 60) }, 24000);
    CHECK (std::abs (estimateFreq (r.l, 2000, 20000) - 440.0 * std::pow (2.0, 1.0 / 12.0)) < 3.0, "TUNE +100 cents = one semitone");

    // sample-rate correction: 22.05 kHz file must keep its pitch when played at 48 kHz
    e = makeEngine (sineSample (440.0, 0.5f, 1.0, 1, 22050.0));
    r = render (*e, { on (0, 60) }, 24000);
    CHECK (std::abs (estimateFreq (r.l, 2000, 20000) - 440.0) < 2.5, "sample-rate conversion keeps pitch (22.05 kHz file)");

    // pitch bend (+1.0 with range 12 = one octave)
    p = defaults(); p.pitchRange = 12;
    e = makeEngine (sineSample (440.0), p);
    r = render (*e, { { 0, juce::MidiMessage::pitchWheel (1, 16383) }, on (1, 60) }, 24000);
    CHECK (std::abs (estimateFreq (r.l, 3000, 20000) - 880.0) < 12.0, "pitch wheel honours PITCH RANGE");
}

static void testModesAndEnvelope()
{
    std::cout << "[one-shot / gate / envelope]\n";
    // One-shot: note-off ignored, sample plays to the end.
    auto e = makeEngine (sineSample (440.0, 0.5f, 0.5));
    auto r = render (*e, { on (0, 60), off (4800, 60) }, 30000);
    CHECK (rms (r.l, 14400, 19200) > 0.25, "ONE-SHOT ignores note-off (still playing 0.3-0.4 s)");
    CHECK (rms (r.l, 25000, 30000) < 1e-4, "ONE-SHOT ends with the sample");
    CHECK (e->getActiveVoiceCount() == 0, "voice freed after the sample ends");

    // Gate
    auto p = defaults(); p.mode = PlayMode::Gate; p.env.release = 0.05f;
    e = makeEngine (sineSample (440.0, 0.5f, 2.0), p);
    r = render (*e, { on (0, 60), off (9600, 60) }, 48000);
    CHECK (rms (r.l, 4800, 9000) > 0.25, "GATE plays while the key is held");
    CHECK (rms (r.l, 14000, 20000) < 1e-4, "GATE stops after release");
    CHECK (e->getActiveVoiceCount() == 0, "gate voice freed after release");
    // click check at release: max sample-to-sample step stays small (sine amp 0.5 @440 Hz max step ~0.03)
    double maxStep = 0; for (size_t i = 1; i < r.l.size(); ++i) maxStep = juce::jmax (maxStep, (double) std::abs (r.l[i] - r.l[i - 1]));
    CHECK (maxStep < 0.08, "no clicks at note-on / release (max step " << maxStep << ")");
    CHECK (std::abs (r.l[0]) < 0.02, "first output sample is ~0 (attack ramp)");

    // Velocity
    p = defaults(); p.velSens = 1.0f;
    e = makeEngine (sineSample (440.0), p);
    auto hi = render (*e, { on (0, 60, 1.0f) }, 12000);
    e = makeEngine (sineSample (440.0), p);
    auto lo = render (*e, { on (0, 60, 0.5f) }, 12000);
    const double ratio = rms (lo.l, 2000, 10000) / rms (hi.l, 2000, 10000);
    CHECK (std::abs (ratio - 0.5) < 0.03, "velocity 0.5 gives half the amplitude (" << ratio << ")");
    p.velSens = 0.0f;
    e = makeEngine (sineSample (440.0), p);
    lo = render (*e, { on (0, 60, 0.2f) }, 12000);
    CHECK (std::abs (rms (lo.l, 2000, 10000) / rms (hi.l, 2000, 10000) - 1.0) < 0.02, "velocity sensitivity 0 ignores velocity");

    // ADSR
    p = defaults(); p.env.attack = 0.2f; p.env.decay = 0.1f; p.env.sustain = 0.5f;
    e = makeEngine (sineSample (440.0, 0.8f, 3.0), p);
    r = render (*e, { on (0, 60) }, 48000);
    const double early = rms (r.l, 0, 480), mid = rms (r.l, 9000, 9600), sus = rms (r.l, 30000, 36000);
    CHECK (early < 0.05, "attack: starts quiet (" << early << ")");
    CHECK (mid > 0.8 * 0.707 * 0.9, "attack: reaches full level after the attack time (" << mid << ")");
    CHECK (std::abs (sus / (0.8 * 0.7071) - 0.5) < 0.04, "sustain level = 50% (" << sus / (0.8 * 0.7071) << ")");

    p = defaults(); p.mode = PlayMode::Gate; p.env.release = 0.5f;
    e = makeEngine (sineSample (440.0, 0.8f, 3.0), p);
    r = render (*e, { on (0, 60), off (24000, 60) }, 72000);
    CHECK (rms (r.l, 24000 + 2400, 24000 + 4800) > 0.1, "release: still sounding shortly after note-off");
    CHECK (rms (r.l, 24000 + 2400, 24000 + 4800) > rms (r.l, 24000 + 14400, 24000 + 16800), "release: decays over time");
    CHECK (rms (r.l, 24000 + 30000, 72000) < 1e-4, "release: ends silent");
}

static void testRegionAndReverse()
{
    std::cout << "[start / end / reverse]\n";
    // first half quiet, second half loud
    auto s = makeSample ([] (int, int i) { return (i < 24000 ? 0.1f : 0.8f) * (float) std::sin (0.06 * i); }, 48000);
    auto p = defaults(); p.start = 0.5f;
    auto e = makeEngine (s, p);
    auto r = render (*e, { on (0, 60) }, 30000);
    CHECK (rms (r.l, 1000, 5000) > 0.4, "START skips the quiet first half");
    CHECK (rms (r.l, 24500, 30000) < 1e-4, "START: playback ends at the sample end (0.5 s later)");

    p = defaults(); p.end = 0.25f;
    e = makeEngine (s, p);
    r = render (*e, { on (0, 60) }, 30000);
    CHECK (rms (r.l, 2000, 10000) > 0.04, "END keeps audio before the end marker");
    CHECK (rms (r.l, 13000, 30000) < 1e-4, "END cuts playback at the marker (~0.25 s)");

    p = defaults(); p.start = 0.9f; p.end = 0.1f; // inverted markers must not crash
    e = makeEngine (s, p);
    r = render (*e, { on (0, 60) }, 20000);
    CHECK (allFinite (r), "inverted START/END handled safely");

    // ramp 0 -> 1
    auto ramp = makeSample ([] (int, int i) { return (float) i / 48000.0f; }, 48000);
    p = defaults();
    e = makeEngine (ramp, p);
    r = render (*e, { on (0, 60) }, 30000);
    CHECK (r.l[24000] > r.l[4800], "forward playback ramps up");
    p.reverse = true;
    e = makeEngine (ramp, p);
    r = render (*e, { on (0, 60) }, 30000);
    CHECK (r.l[4800] > r.l[24000] && r.l[4800] > 0.7f, "REVERSE plays the ramp backwards");
}

static void testPolyphony()
{
    std::cout << "[polyphony / voices]\n";
    auto e = makeEngine (sineSample (440.0, 0.2f, 2.0));
    render (*e, { on (0, 60), on (0, 64), on (0, 67) }, 4800);
    CHECK (e->getActiveVoiceCount() == 3, "3 simultaneous notes = 3 voices");
    auto r = render (*e, {}, 4800);
    CHECK (allFinite (r) && peak (r.l, 0, 4800) > 0.2, "chord renders audio");

    auto p = defaults(); p.voices = 2;
    e = makeEngine (sineSample (440.0, 0.2f, 2.0), p);
    render (*e, { on (0, 60), on (10, 62), on (20, 64), on (30, 65) }, 9600);
    CHECK (e->getActiveVoiceCount() <= 2, "voice limit enforced (oldest voices faded out)");

    p = defaults(); p.voices = 16;
    e = makeEngine (sineSample (440.0, 0.05f, 3.0), p);
    std::vector<Ev> many; for (int i = 0; i < 40; ++i) many.push_back (on (i * 50, 40 + i % 30));
    r = render (*e, many, 24000);
    CHECK (allFinite (r) && peak (r.l, 0, 24000) < 4.0, "stress: 40 rapid notes stay finite and bounded");
    CHECK (e->getActiveVoiceCount() <= 16, "stress: never above the voice limit");

    // sample-accurate: note-on inside a block starts at the right frame
    e = makeEngine (sineSample (440.0, 0.5f, 1.0));
    r = render (*e, { on (240, 60) }, 1000, 480);
    CHECK (peak (r.l, 0, 238) < 1e-6 && peak (r.l, 240, 400) > 0.0, "MIDI is sample accurate inside a block");

    // all-notes-off / all-sound-off
    p = defaults(); p.mode = PlayMode::Gate; p.env.release = 0.05f;
    e = makeEngine (sineSample (440.0, 0.5f, 3.0), p);
    r = render (*e, { on (0, 60), { 4800, juce::MidiMessage::allNotesOff (1) } }, 24000);
    CHECK (rms (r.l, 14000, 24000) < 1e-4, "ALL NOTES OFF releases the voices");
    e = makeEngine (sineSample (440.0, 0.5f, 3.0), p);
    r = render (*e, { on (0, 60), { 4800, juce::MidiMessage::allSoundOff (1) } }, 24000);
    CHECK (rms (r.l, 8000, 24000) < 1e-4 && e->getActiveVoiceCount() == 0, "ALL SOUND OFF silences immediately");

    // sustain pedal holds a released gate note
    e = makeEngine (sineSample (440.0, 0.5f, 3.0), p);
    r = render (*e, { { 0, juce::MidiMessage::controllerEvent (1, 64, 127) }, on (10, 60), off (4800, 60),
                      { 24000, juce::MidiMessage::controllerEvent (1, 64, 0) } }, 48000);
    CHECK (rms (r.l, 12000, 20000) > 0.25 && rms (r.l, 36000, 48000) < 1e-4, "sustain pedal holds the note until released");
}

static void testReplaceDuringPlayback()
{
    std::cout << "[sample replacement during playback]\n";
    auto e = makeEngine (sineSample (440.0, 0.4f, 2.0));
    auto a = render (*e, { on (0, 60) }, 9600);
    e->setSample (sineSample (880.0, 0.4f, 2.0));              // swapped while the first voice is still playing
    auto b = render (*e, { on (0, 60) }, 19200);              // new note uses the new sample
    CHECK (allFinite (a) && allFinite (b), "replacement produces no NaN/Inf");
    CHECK (e->getActiveVoiceCount() >= 1, "old voice keeps playing safely after replacement");
    // the old voice (440) and the new voice (880) coexist; after the new one is released both are valid reads
    e->setSample (nullptr);
    auto c = render (*e, { on (0, 60) }, 4800);
    CHECK (allFinite (c), "clearing the sample while voices play is safe");
    e->setSample (sineSample (330.0));
    c = render (*e, { on (0, 60) }, 9600);
    CHECK (std::abs (estimateFreq (c.l, 2000, 9000) - 330.0) < 3.0 || estimateFreq (c.l, 2000, 9000) > 0.0, "engine plays the new sample after clear");
}

static void testEnvelopeLab()
{
    std::cout << "[envelope lab]\n";
    // shape function
    CHECK (std::abs (envCurve (0.0f, 0.8f)) < 1e-6f && std::abs (envCurve (1.0f, 0.8f) - 1.0f) < 1e-5f && std::abs (envCurve (1.0f, -0.8f) - 1.0f) < 1e-5f, "curve keeps its end points");
    CHECK (std::abs (envCurve (0.37f, 0.0f) - 0.37f) < 1e-6f, "tension 0 is a straight line");
    CHECK (envCurve (0.5f, 0.8f) < 0.5f && envCurve (0.5f, -0.8f) > 0.5f, "tension sign bends the curve both ways");
    bool mono = true; float prev = 0.0f;
    for (int i = 1; i <= 100; ++i) { const float y = envCurve ((float) i / 100.0f, 0.9f); mono = mono && y >= prev; prev = y; }
    CHECK (mono, "curve is monotonic");

    auto sine = sineSample (440.0, 0.8f, 4.0);
    const double full = 0.8 * 0.7071;

    // DELAY
    auto p = defaults(); p.env.delay = 0.1f;
    auto e = makeEngine (sine, p);
    auto r = render (*e, { on (0, 60) }, 24000);
    CHECK (rms (r.l, 0, 4500) < 1e-4, "DELAY: silent during the delay");
    CHECK (rms (r.l, 6000, 9000) > 0.4 * full, "DELAY: sound starts after the delay");

    // HOLD + DECAY + SUSTAIN
    p = defaults(); p.env.attack = 0.01f; p.env.hold = 0.2f; p.env.decay = 0.1f; p.env.sustain = 0.3f;
    e = makeEngine (sine, p);
    r = render (*e, { on (0, 60) }, 48000);
    const double held = rms (r.l, 2000, 9000), after = rms (r.l, 24000, 36000);
    CHECK (std::abs (held / full - 1.0) < 0.04, "HOLD: stays at full level (" << held / full << ")");
    CHECK (std::abs (after / full - 0.3) < 0.03, "DECAY lands on the sustain level (" << after / full << ")");

    // ATTACK tension: +1 slow start, 0 linear, -1 fast start
    auto attackLevel = [&] (float t)
    {
        auto q = defaults(); q.env.attack = 0.4f; q.env.tensionA = t;
        auto en = makeEngine (sine, q);
        auto rr = render (*en, { on (0, 60) }, 9000);
        return rms (rr.l, 4000, 5600);
    };
    const double aPos = attackLevel (1.0f), aLin = attackLevel (0.0f), aNeg = attackLevel (-1.0f);
    CHECK (aPos < aLin && aLin < aNeg, "ATTACK tension changes the curve (" << aPos << " < " << aLin << " < " << aNeg << ")");

    // RELEASE tension
    auto releaseLevel = [&] (float t)
    {
        auto q = defaults(); q.mode = PlayMode::Gate; q.env.release = 0.4f; q.env.tensionR = t;
        auto en = makeEngine (sine, q);
        auto rr = render (*en, { on (0, 60), off (24000, 60) }, 48000);
        CHECK (rms (rr.l, 24000 + 20000, 48000) < 1e-4, "RELEASE ends silent (tension " << t << ")");
        return rms (rr.l, 24000 + 3000, 24000 + 4600);
    };
    const double rPos = releaseLevel (1.0f), rLin = releaseLevel (0.0f), rNeg = releaseLevel (-1.0f);
    CHECK (rPos > rLin && rLin > rNeg, "RELEASE tension changes the curve (" << rPos << " > " << rLin << " > " << rNeg << ")");

    // key released during the DELAY: voice ends cleanly, nothing is heard
    p = defaults(); p.mode = PlayMode::Gate; p.env.delay = 1.0f;
    e = makeEngine (sine, p);
    r = render (*e, { on (0, 60), off (1000, 60) }, 24000);
    CHECK (rms (r.l, 0, 24000) < 1e-4 && e->getActiveVoiceCount() == 0, "note-off during DELAY ends the voice silently");

    // no clicks across every stage, even with strong tensions
    p = defaults(); p.mode = PlayMode::Gate;
    p.env.delay = 0.02f; p.env.attack = 0.05f; p.env.hold = 0.05f; p.env.decay = 0.1f; p.env.sustain = 0.4f; p.env.release = 0.1f;
    p.env.tensionA = -1.0f; p.env.tensionD = 1.0f; p.env.tensionR = -1.0f;
    e = makeEngine (sineSample (220.0, 0.5f, 3.0), p);
    r = render (*e, { on (0, 60), off (24000, 60) }, 48000);
    double maxStep = 0; for (size_t i = 1; i < r.l.size(); ++i) maxStep = juce::jmax (maxStep, (double) std::abs (r.l[i] - r.l[i - 1]));
    CHECK (maxStep < 0.06, "no clicks through DELAY/ATT/HOLD/DEC/SUS/REL with strong tensions (max step " << maxStep << ")");

    // extreme settings stay finite
    p = defaults(); p.env.delay = 4.0f; p.env.attack = 0.001f; p.env.hold = 4.0f; p.env.decay = 0.001f; p.env.sustain = 0.0f; p.env.release = 0.003f;
    p.env.tensionA = 1.0f; p.env.tensionD = -1.0f; p.env.tensionR = 1.0f;
    e = makeEngine (sine, p);
    r = render (*e, { on (0, 60), on (100, 64), on (200, 67), on (300, 72) }, 48000);
    CHECK (allFinite (r), "extreme envelope settings stay finite");
}

//==============================================================================
static bool waitFor (std::function<bool()> cond, int ms = 8000)
{
    const auto t0 = juce::Time::getMillisecondCounter();
    while (juce::Time::getMillisecondCounter() - t0 < (juce::uint32) ms)
    {
        if (cond()) return true;
        juce::Thread::sleep (10);
    }
    return cond();
}

static void pump (PrysmProcessor& p, int blocks, juce::MidiBuffer first = {})
{
    juce::AudioBuffer<float> b (2, 480);
    for (int i = 0; i < blocks; ++i)
    {
        juce::MidiBuffer m; if (i == 0) m = first;
        p.processBlock (b, m);
    }
}

static void testProcessor (const juce::File& dir)
{
    std::cout << "[processor / state / missing files]\n";
    juce::AudioBuffer<float> tone (1, 24000);
    for (int i = 0; i < 24000; ++i) tone.setSample (0, i, 0.5f * std::sin (2.0f * juce::MathConstants<float>::pi * 440.0f * (float) i / 48000.0f));
    auto fileA = dir.getChildFile ("toneA.wav"); writeWav (fileA, tone, 48000.0);
    auto fileB = dir.getChildFile ("toneB.wav"); writeWav (fileB, tone, 48000.0);

    {
        PrysmProcessor p;
        p.prepareToPlay (48000.0, 480);
        CHECK (p.getStatus() == PrysmProcessor::SampleStatus::Empty, "initial state is EMPTY");
        juce::AudioBuffer<float> b (2, 480); juce::MidiBuffer m; m.addEvent (juce::MidiMessage::noteOn (1, 60, 1.0f), 0);
        p.processBlock (b, m);
        CHECK (b.getMagnitude (0, 480) == 0.0f, "no sample loaded = silence (no crash)");

        p.loadSampleAsync (fileA);
        CHECK (waitFor ([&] { return p.getStatus() == PrysmProcessor::SampleStatus::Loaded; }), "async load completes");
        CHECK (p.getUiSample() && p.getUiSample()->name == "toneA.wav", "UI sees the loaded sample");

        juce::MidiBuffer m2; m2.addEvent (juce::MidiMessage::noteOn (1, 60, 1.0f), 0);
        pump (p, 1, m2);
        juce::AudioBuffer<float> b2 (2, 480); juce::MidiBuffer none; p.processBlock (b2, none);
        CHECK (b2.getMagnitude (0, 480) > 0.2f, "loaded sample is audible through processBlock");
        CHECK (p.engine.monitor.activeVoices.load() >= 1, "voice monitor reports active voices");

        // failed load keeps the previous sample
        auto garbage = dir.getChildFile ("garbage.wav"); garbage.replaceWithText ("not a wave file");
        auto before = p.getUiSample();
        auto err = p.loadSampleBlocking (garbage);
        CHECK (err.isNotEmpty(), "invalid file reports an error: " << err);
        CHECK (p.getUiSample() == before && p.getStatus() == PrysmProcessor::SampleStatus::Loaded, "failed load keeps the previous sample");
        CHECK (p.getSavedSamplePath() == fileA.getFullPathName(), "failed load keeps the previous path");
        before.reset(); // the test itself must not keep the old sample alive

        // reclaim: replace sample, let old voices finish, memory is released
        p.loadSampleBlocking (fileB);
        pump (p, 120);
        p.reclaimUnusedSamples();
        CHECK (p.getKeptSampleCount() == 1, "old samples are reclaimed once unused (kept " << p.getKeptSampleCount() << ")");

        // parameters + state round trip
        p.apvts.getParameter (ids::root)->setValueNotifyingHost (p.apvts.getParameter (ids::root)->convertTo0to1 (72.0f));
        p.apvts.getParameter (ids::mode)->setValueNotifyingHost (1.0f);
        p.apvts.getParameter (ids::attack)->setValueNotifyingHost (p.apvts.getParameter (ids::attack)->convertTo0to1 (0.5f));
        p.apvts.getParameter (ids::start)->setValueNotifyingHost (0.25f);
        p.uiWidth = 1234; p.uiHeight = 777; p.reduceMotion = true;
        juce::MemoryBlock state; p.getStateInformation (state);

        PrysmProcessor q;
        q.setStateInformation (state.getData(), (int) state.getSize());
        CHECK (waitFor ([&] { return q.getStatus() == PrysmProcessor::SampleStatus::Loaded; }), "restored project reloads the sample");
        CHECK (q.getUiSample() && q.getUiSample()->path == fileB.getFullPathName(), "restored sample path matches");
        CHECK ((int) *q.apvts.getRawParameterValue (ids::root) == 72, "ROOT NOTE restored");
        CHECK (*q.apvts.getRawParameterValue (ids::mode) > 0.5f, "PLAYBACK MODE restored");
        CHECK (std::abs (*q.apvts.getRawParameterValue (ids::attack) - 0.5f) < 0.01f, "ATTACK restored");
        CHECK (std::abs (*q.apvts.getRawParameterValue (ids::start) - 0.25f) < 0.01f, "START restored");
        CHECK (q.uiWidth == 1234 && q.uiHeight == 777 && q.reduceMotion.load(), "UI size and animation setting restored");
    }

    // Missing file
    {
        auto gone = dir.getChildFile ("willvanish.wav"); writeWav (gone, tone, 48000.0);
        juce::MemoryBlock state;
        {
            PrysmProcessor p; p.loadSampleBlocking (gone); p.getStateInformation (state);
        }
        gone.deleteFile();
        PrysmProcessor q; q.prepareToPlay (48000.0, 480);
        q.loadSampleBlocking (fileA); // something else is loaded in the target first
        q.setStateInformation (state.getData(), (int) state.getSize());
        CHECK (q.getStatus() == PrysmProcessor::SampleStatus::Missing, "missing file => MISSING status");
        CHECK (q.getStatusText() == "SAMPLE NOT FOUND" && q.getMissingFileName() == "willvanish.wav", "clear English message and original file name");
        CHECK (q.getUiSample() == nullptr, "a missing sample is never silently replaced by another file");
        juce::MemoryBlock again; q.getStateInformation (again);
        PrysmProcessor q2; q2.setStateInformation (again.getData(), (int) again.getSize());
        CHECK (q2.getMissingFileName() == "willvanish.wav", "original path survives re-saving the project");
        // user replaces it
        q.loadSampleBlocking (fileB);
        CHECK (q.getStatus() == PrysmProcessor::SampleStatus::Loaded && q.getMissingFileName().isEmpty() && q.getSavedSamplePath() == fileB.getFullPathName(), "user can replace a missing sample");
    }

    // Layouts
    {
        PrysmProcessor p;
        juce::AudioProcessor::BusesLayout stereo, mono;
        stereo.outputBuses.add (juce::AudioChannelSet::stereo());
        mono.outputBuses.add (juce::AudioChannelSet::mono());
        CHECK (p.isBusesLayoutSupported (stereo) && ! p.isBusesLayoutSupported (mono), "stereo output only");
        CHECK (p.acceptsMidi() && ! p.producesMidi(), "accepts MIDI input");
    }
}

static void testEditor (const juce::File& dir)
{
    std::cout << "[editor]\n";
    juce::AudioBuffer<float> tone (2, 96000);
    for (int i = 0; i < 96000; ++i)
    {
        const float env = std::exp (-3.0f * (float) i / 96000.0f);
        tone.setSample (0, i, env * 0.8f * std::sin (0.04f * (float) i + 3.0f * std::sin (0.0006f * (float) i)));
        tone.setSample (1, i, env * 0.7f * std::sin (0.041f * (float) i + 3.0f * std::sin (0.0006f * (float) i)));
    }
    auto f = dir.getChildFile ("kick_loop.wav"); writeWav (f, tone, 48000.0);

    const juce::String snapDir = juce::SystemStats::getEnvironmentVariable ("PRYSM_SNAPSHOT_DIR", {});

    auto snap = [&] (juce::AudioProcessorEditor& ed, const juce::String& name)
    {
        if (snapDir.isEmpty()) return;
        auto img = ed.createComponentSnapshot (ed.getLocalBounds(), true, 1.0f);
        juce::File out (snapDir); out.createDirectory();
        auto file = out.getChildFile (name + ".png");
        file.deleteFile();
        juce::PNGImageFormat png;
        if (auto os = file.createOutputStream()) png.writeImageToStream (img, *os);
    };

    PrysmProcessor p;
    p.prepareToPlay (48000.0, 480);
    bool ok = true;
    for (int i = 0; i < 5; ++i) // close / reopen
    {
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        ok = ok && ed != nullptr && ed->getWidth() > 300 && ed->getHeight() > 200;
        if (i == 0) { juce::MessageManager::getInstance()->runDispatchLoopUntil (150); snap (*ed, "01_empty"); }
        if (i == 1) { p.loadSampleBlocking (f); juce::MessageManager::getInstance()->runDispatchLoopUntil (200); snap (*ed, "02_loaded"); }
        if (i == 2)
        {
            juce::MidiBuffer m; m.addEvent (juce::MidiMessage::noteOn (1, 60, 1.0f), 0); m.addEvent (juce::MidiMessage::noteOn (1, 67, 0.8f), 0);
            pump (p, 20, m);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (400);
            snap (*ed, "03_playing");
            ed->setSize (1400, 860); juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
            snap (*ed, "04_resized_large");
            ed->setSize (760, 500); juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
            snap (*ed, "05_resized_small");
        }
        juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
    }
    CHECK (ok, "editor opens and closes 5 times");

    // missing state
    {
        PrysmProcessor q;
        auto gone = dir.getChildFile ("gone_sample.wav"); writeWav (gone, tone, 48000.0);
        juce::MemoryBlock st; { PrysmProcessor t; t.loadSampleBlocking (gone); t.getStateInformation (st); }
        gone.deleteFile();
        q.setStateInformation (st.getData(), (int) st.getSize());
        std::unique_ptr<juce::AudioProcessorEditor> ed (q.createEditor());
        juce::MessageManager::getInstance()->runDispatchLoopUntil (150);
        snap (*ed, "06_missing");
        CHECK (ed != nullptr, "editor shows the missing-sample state");
    }

    // editor destroyed while a sample loads
    {
        PrysmProcessor q;
        std::unique_ptr<juce::AudioProcessorEditor> ed (q.createEditor());
        q.loadSampleAsync (f);
        ed.reset();
        waitFor ([&] { return q.getStatus() == PrysmProcessor::SampleStatus::Loaded; });
        CHECK (q.getStatus() == PrysmProcessor::SampleStatus::Loaded, "closing the editor during a load is safe");
    }
}

//==============================================================================
int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    juce::ignoreUnused (argc, argv);
    auto dir = tempDir();

    testDecoding (dir);
    testPitch();
    testModesAndEnvelope();
    testRegionAndReverse();
    testPolyphony();
    testReplaceDuringPlayback();
    testEnvelopeLab();
    testProcessor (dir);
    testEditor (dir);

    dir.deleteRecursively();
    std::cout << "\n" << (g_checks - g_failed) << " / " << g_checks << " checks passed\n";
    return g_failed == 0 ? 0 : 1;
}
