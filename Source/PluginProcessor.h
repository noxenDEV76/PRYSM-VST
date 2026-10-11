#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "Engine.h"
#include "Parameters.h"

class PrysmProcessor : public juce::AudioProcessor,
                       public juce::ChangeBroadcaster
{
public:
    enum class SampleStatus { Empty = 0, Loading, Loaded, Missing, Error };

    PrysmProcessor();
    ~PrysmProcessor() override;

    // AudioProcessor
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "PRYSM"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override;
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // ---- Sample management (message thread / any non-audio thread) ----
    /** Decodes on a background thread; on failure the previous sample is kept. */
    void loadSampleAsync (const juce::File& file);
    /** Blocking variant used by tests and offline tools. Returns an empty string on success, else the error. */
    juce::String loadSampleBlocking (const juce::File& file);
    void clearSample();
    prysm::SamplePtr getUiSample() const;
    SampleStatus getStatus() const noexcept { return (SampleStatus) status.load(); }
    juce::String getStatusText() const;
    juce::String getMissingFileName() const;
    juce::String getSavedSamplePath() const;
    /** Frees decoded samples that no voice or UI references any more. */
    void reclaimUnusedSamples();
    int getKeptSampleCount() const;
    juce::StringArray getSupportedExtensions() const;
    bool isSupportedFile (const juce::File&) const;

    /** Notes played with the on-screen keyboard (message thread) -> audio thread, lock-free. */
    void pushUiNote (int note, float velocity, bool on);

    // ---- UI persistence ----
    std::atomic<int>  uiWidth { 1120 }, uiHeight { 700 };
    std::atomic<bool> reduceMotion { false };

    juce::AudioProcessorValueTreeState apvts;
    prysm::SamplerEngine engine;

private:
    void installSample (prysm::SamplePtr s, bool publishNull);
    void publishToAudio (prysm::SamplePtr s);
    void setError (const juce::String& message);
    prysm::EngineParams readParams() const;

    struct UiNote { int note = 0; float vel = 0.0f; bool on = false; };
    juce::AbstractFifo uiFifo { 256 };
    std::array<UiNote, 256> uiNotes;

    juce::AudioFormatManager formats;
    juce::ThreadPool loadPool { 1 };
    std::atomic<int> loadGeneration { 0 };
    std::atomic<int> status { 0 };

    mutable juce::CriticalSection lock; // never taken on the audio thread
    std::vector<prysm::SamplePtr> keepAlive;
    prysm::SamplePtr uiSample;
    juce::String savedPath, missingPath, statusText;

    // Wait-free triple buffer: producer = any non-audio thread (under `lock`), consumer = audio thread.
    std::array<prysm::SamplePtr, 3> mailbox;
    std::atomic<int> mailboxState { 1 };
    int mailboxWrite = 0, mailboxRead = 2;
    static constexpr int kDirty = 4;

    struct Raw
    {
        std::atomic<float> *gain, *pan, *root, *tune, *pitchRange, *transpose, *mode, *reverse, *start, *end, *velSens, *voices,
            *attack, *decay, *sustain, *release, *filterType, *cutoff, *resonance, *drive, *width, *motion, *motionRate,
            *motionDest, *random, *randomSeed, *reverbMix, *reverbSize, *tail;
    } raw {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PrysmProcessor)
};
