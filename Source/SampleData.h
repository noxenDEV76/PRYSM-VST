#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include <atomic>
#include <cmath>
#include <memory>
#include <vector>

namespace prysm
{
/** Immutable, fully decoded sample. Once published it is never modified,
    so the audio thread and the UI thread can both read it without locks. */
struct SampleData
{
    static constexpr int kPeakBins = 1024;
    static constexpr juce::int64 kMaxFrames = 12'000'000; // ~4.2 min @ 48 kHz

    juce::AudioBuffer<float> audio; // 1 or 2 channels
    double sampleRate = 44100.0;
    juce::String name;
    juce::String path;
    std::vector<float> peakMin, peakMax; // kPeakBins entries (UI waveform)

    int numFrames() const noexcept   { return audio.getNumSamples(); }
    int numChannels() const noexcept { return audio.getNumChannels(); }
    double durationSeconds() const noexcept { return sampleRate > 0 ? numFrames() / sampleRate : 0.0; }

    void computePeaks()
    {
        peakMin.assign (kPeakBins, 0.0f);
        peakMax.assign (kPeakBins, 0.0f);
        const int n = numFrames();
        if (n <= 0) return;
        for (int b = 0; b < kPeakBins; ++b)
        {
            const int i0 = (int) ((juce::int64) b * n / kPeakBins);
            const int i1 = juce::jmax (i0 + 1, (int) ((juce::int64) (b + 1) * n / kPeakBins));
            float lo = 0.0f, hi = 0.0f;
            for (int ch = 0; ch < numChannels(); ++ch)
            {
                const float* d = audio.getReadPointer (ch);
                for (int i = i0; i < i1 && i < n; ++i) { lo = juce::jmin (lo, d[i]); hi = juce::jmax (hi, d[i]); }
            }
            peakMin[(size_t) b] = lo;
            peakMax[(size_t) b] = hi;
        }
    }
};

using SamplePtr = std::shared_ptr<const SampleData>;

struct LoadResult
{
    SamplePtr sample;
    juce::String error; // empty on success (English, shown in the UI)
    bool ok() const noexcept { return sample != nullptr; }
};

/** Validates and decodes an audio file. Runs on a background thread, never on the audio thread. */
inline LoadResult decodeSampleFile (const juce::File& file, juce::AudioFormatManager& formats)
{
    LoadResult r;
    if (! file.existsAsFile())                    { r.error = "SAMPLE NOT FOUND"; return r; }
    if (file.getSize() <= 0)                      { r.error = "FILE IS EMPTY"; return r; }

    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
    if (reader == nullptr)                        { r.error = "UNSUPPORTED OR CORRUPT FILE"; return r; }
    if (reader->lengthInSamples <= 0)             { r.error = "FILE CONTAINS NO AUDIO"; return r; }
    if (reader->lengthInSamples > SampleData::kMaxFrames) { r.error = "SAMPLE TOO LONG"; return r; }
    if (reader->numChannels < 1)                  { r.error = "FILE CONTAINS NO AUDIO"; return r; }
    if (reader->sampleRate < 8000.0 || reader->sampleRate > 768000.0) { r.error = "UNSUPPORTED SAMPLE RATE"; return r; }

    const int frames   = (int) reader->lengthInSamples;
    const int channels = juce::jmin (2, (int) reader->numChannels);

    auto data = std::make_shared<SampleData>();
    data->audio.setSize (channels, frames);
    data->audio.clear();
    if (! reader->read (&data->audio, 0, frames, 0, true, channels > 1))
                                                  { r.error = "COULD NOT DECODE FILE"; return r; }

    // Sanitise: replace NaN/Inf, reject pure silence only as a warning-free pass.
    for (int ch = 0; ch < channels; ++ch)
    {
        float* d = data->audio.getWritePointer (ch);
        for (int i = 0; i < frames; ++i)
            if (! std::isfinite (d[i])) d[i] = 0.0f;
    }

    data->sampleRate = reader->sampleRate;
    data->name = file.getFileName();
    data->path = file.getFullPathName();
    data->computePeaks();
    r.sample = std::move (data);
    return r;
}
} // namespace prysm
