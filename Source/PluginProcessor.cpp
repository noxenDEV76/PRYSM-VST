#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace prysm;

PrysmProcessor::PrysmProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PRYSM_STATE", createParameterLayout())
{
    formats.registerBasicFormats(); // WAV, AIFF, FLAC, Ogg Vorbis (decoders bundled with JUCE)

    auto p = [this] (const char* id) { return apvts.getRawParameterValue (id); };
    raw = { p (ids::gain), p (ids::pan), p (ids::root), p (ids::tune), p (ids::pitchRange), p (ids::transpose), p (ids::mode),
            p (ids::reverse), p (ids::start), p (ids::end), p (ids::velSens), p (ids::voices), p (ids::attack), p (ids::decay),
            p (ids::sustain), p (ids::release), p (ids::filterType), p (ids::cutoff), p (ids::resonance), p (ids::drive),
            p (ids::width), p (ids::motion), p (ids::motionRate), p (ids::motionDest), p (ids::random), p (ids::randomSeed),
            p (ids::reverbMix), p (ids::reverbSize), p (ids::tail) };
    engine.setParams (readParams());
    engine.prepare (44100.0);
}

PrysmProcessor::~PrysmProcessor()
{
    loadPool.removeAllJobs (true, 10000);
}

//==============================================================================
bool PrysmProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void PrysmProcessor::prepareToPlay (double sampleRate, int)
{
    engine.setParams (readParams());
    engine.prepare (sampleRate);
    setLatencySamples (juce::roundToInt (engine.getLatencySamples()));
    // The engine keeps its current sample across prepare(); only voices are reset.
}

EngineParams PrysmProcessor::readParams() const
{
    EngineParams e;
    e.gainDb = raw.gain->load();  e.pan = raw.pan->load();
    e.root = (int) raw.root->load();  e.tuneCents = raw.tune->load();
    e.pitchRange = (int) raw.pitchRange->load();  e.transpose = (int) raw.transpose->load();
    e.mode = raw.mode->load() > 0.5f ? PlayMode::Gate : PlayMode::OneShot;
    e.reverse = raw.reverse->load() > 0.5f;
    e.start = raw.start->load();  e.end = raw.end->load();  e.velSens = raw.velSens->load();
    e.voices = (int) raw.voices->load();
    e.attack = raw.attack->load();  e.decay = raw.decay->load();  e.sustain = raw.sustain->load();  e.release = raw.release->load();
    e.filterMode = (FilterMode) juce::jlimit (0, 2, (int) raw.filterType->load());
    e.cutoff = raw.cutoff->load();  e.resonance = raw.resonance->load();
    e.drive = raw.drive->load();  e.width = raw.width->load();
    e.motion = raw.motion->load();  e.motionRate = raw.motionRate->load();
    e.motionDest = (MotionDest) juce::jlimit (0, 3, (int) raw.motionDest->load());
    e.random = raw.random->load();  e.randomSeed = (int) raw.randomSeed->load();
    e.reverbMix = raw.reverbMix->load();  e.reverbSize = raw.reverbSize->load();  e.tailMs = raw.tail->load();
    return e;
}

void PrysmProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    // Pick up a freshly published sample (wait-free).
    if (mailboxState.load (std::memory_order_acquire) & kDirty)
    {
        const int old = mailboxState.exchange (mailboxRead, std::memory_order_acq_rel);
        mailboxRead = old & 3;
        engine.setSample (std::move (mailbox[(size_t) mailboxRead]));
        mailbox[(size_t) mailboxRead] = nullptr;
    }

    // Notes from the on-screen keyboard
    {
        int s1, n1, s2, n2;
        uiFifo.prepareToRead (uiFifo.getNumReady(), s1, n1, s2, n2);
        auto emit = [&] (int start, int count)
        {
            for (int i = 0; i < count; ++i)
            {
                const auto& u = uiNotes[(size_t) (start + i)];
                midi.addEvent (u.on ? juce::MidiMessage::noteOn (1, u.note, u.vel) : juce::MidiMessage::noteOff (1, u.note), 0);
            }
        };
        emit (s1, n1); emit (s2, n2);
        uiFifo.finishedRead (n1 + n2);
    }

    for (int ch = 2; ch < buffer.getNumChannels(); ++ch) buffer.clear (ch, 0, buffer.getNumSamples());
    engine.setParams (readParams());
    engine.process (buffer, midi);
    midi.clear(); // instrument: nothing is forwarded
}

double PrysmProcessor::getTailLengthSeconds() const
{
    const float mix = raw.reverbMix != nullptr ? raw.reverbMix->load() : 0.0f;
    const float size = raw.reverbSize != nullptr ? raw.reverbSize->load() : 0.0f;
    return mix > 0.001f ? 1.5 + 8.0 * size : 0.0;
}

void PrysmProcessor::pushUiNote (int note, float velocity, bool on)
{
    int s1, n1, s2, n2;
    uiFifo.prepareToWrite (1, s1, n1, s2, n2);
    if (n1 + n2 < 1) return;
    uiNotes[(size_t) (n1 > 0 ? s1 : s2)] = { juce::jlimit (0, 127, note), juce::jlimit (0.0f, 1.0f, velocity), on };
    uiFifo.finishedWrite (1);
}

juce::AudioProcessorEditor* PrysmProcessor::createEditor() { return new PrysmEditor (*this); }

//==============================================================================
// Sample management
void PrysmProcessor::publishToAudio (SamplePtr s)
{
    // Caller holds `lock` or is single-threaded (constructor/prepare); the audio thread is the only consumer.
    mailbox[(size_t) mailboxWrite] = std::move (s);
    const int old = mailboxState.exchange (mailboxWrite | kDirty, std::memory_order_acq_rel);
    mailboxWrite = old & 3;
}

void PrysmProcessor::installSample (SamplePtr s, bool publishNull)
{
    {
        const juce::ScopedLock sl (lock);
        if (s != nullptr)
        {
            keepAlive.push_back (s);
            uiSample = s;
            savedPath = s->path;
            missingPath.clear();
            statusText.clear();
            status = (int) SampleStatus::Loaded;
            publishToAudio (s);
        }
        else if (publishNull)
        {
            uiSample = nullptr;
            publishToAudio (nullptr);
        }
    }
    reclaimUnusedSamples();
    sendChangeMessage();
}

void PrysmProcessor::setError (const juce::String& message)
{
    {
        const juce::ScopedLock sl (lock);
        statusText = message;
        // Keep the previous sample playing; only the status line reports the failure.
        status = (int) (uiSample != nullptr ? SampleStatus::Loaded
                                            : (missingPath.isNotEmpty() ? SampleStatus::Missing : SampleStatus::Error));
    }
    sendChangeMessage();
}

void PrysmProcessor::loadSampleAsync (const juce::File& file)
{
    const int gen = ++loadGeneration;
    {
        const juce::ScopedLock sl (lock);
        statusText.clear();
        if (uiSample == nullptr) status = (int) SampleStatus::Loading;
    }
    sendChangeMessage();

    loadPool.addJob ([this, file, gen]
    {
        auto result = decodeSampleFile (file, formats);
        if (gen != loadGeneration.load()) return; // a newer request superseded this one
        if (result.ok()) installSample (result.sample, false);
        else             setError (result.error);
    });
}

juce::String PrysmProcessor::loadSampleBlocking (const juce::File& file)
{
    ++loadGeneration;
    auto result = decodeSampleFile (file, formats);
    if (result.ok()) { installSample (result.sample, false); return {}; }
    setError (result.error);
    return result.error;
}

void PrysmProcessor::clearSample()
{
    ++loadGeneration;
    {
        const juce::ScopedLock sl (lock);
        savedPath.clear(); missingPath.clear(); statusText.clear();
        status = (int) SampleStatus::Empty;
    }
    installSample (nullptr, true);
}

SamplePtr PrysmProcessor::getUiSample() const { const juce::ScopedLock sl (lock); return uiSample; }

juce::String PrysmProcessor::getStatusText() const { const juce::ScopedLock sl (lock); return statusText; }
juce::String PrysmProcessor::getSavedSamplePath() const { const juce::ScopedLock sl (lock); return savedPath; }
juce::String PrysmProcessor::getMissingFileName() const
{
    const juce::ScopedLock sl (lock);
    return missingPath.isEmpty() ? juce::String() : juce::File (missingPath).getFileName();
}

void PrysmProcessor::reclaimUnusedSamples()
{
    const juce::ScopedLock sl (lock);
    // use_count() == 1 means only this list references the sample: no voice, engine, mailbox or UI uses it,
    // and no new reference can appear because references are only ever copied from existing holders.
    keepAlive.erase (std::remove_if (keepAlive.begin(), keepAlive.end(),
                                     [this] (const SamplePtr& p) { return p != uiSample && p.use_count() == 1; }),
                     keepAlive.end());
}

int PrysmProcessor::getKeptSampleCount() const { const juce::ScopedLock sl (lock); return (int) keepAlive.size(); }

juce::StringArray PrysmProcessor::getSupportedExtensions() const
{
    juce::StringArray exts;
    exts.addTokens (formats.getWildcardForAllFormats().replace ("*", ""), ";", "");
    exts.trim(); exts.removeEmptyStrings();
    return exts;
}

bool PrysmProcessor::isSupportedFile (const juce::File& f) const
{
    for (auto& e : getSupportedExtensions())
        if (f.hasFileExtension (e.trimCharactersAtStart (".")))
            return true;
    return false;
}

//==============================================================================
// State
void PrysmProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    {
        const juce::ScopedLock sl (lock);
        // A missing file keeps its original path so the project can be repaired later.
        state.setProperty ("samplePath", missingPath.isNotEmpty() ? missingPath : savedPath, nullptr);
    }
    state.setProperty ("uiWidth", uiWidth.load(), nullptr);
    state.setProperty ("uiHeight", uiHeight.load(), nullptr);
    state.setProperty ("reduceMotion", reduceMotion.load(), nullptr);
    state.setProperty ("stateVersion", 1, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void PrysmProcessor::setStateInformation (const void* data, int size)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, size));
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;

    auto tree = juce::ValueTree::fromXml (*xml);
    if (! tree.isValid()) return;

    const juce::String path = tree.getProperty ("samplePath", "").toString();
    uiWidth  = juce::jlimit (560, 2800, (int) tree.getProperty ("uiWidth", 1120));
    uiHeight = juce::jlimit (350, 1750, (int) tree.getProperty ("uiHeight", 700));
    reduceMotion = (bool) tree.getProperty ("reduceMotion", false);
    apvts.replaceState (tree);

    engine.setParams (readParams());

    if (path.isEmpty())
    {
        clearSample();
        return;
    }

    juce::File f (path);
    if (f.existsAsFile())
    {
        loadSampleAsync (f);
    }
    else
    {
        // Never substitute another file: silence the slot and let the user locate the original.
        ++loadGeneration;
        {
            const juce::ScopedLock sl (lock);
            savedPath.clear();
            missingPath = path;
            statusText = "SAMPLE NOT FOUND";
            status = (int) SampleStatus::Missing;
        }
        installSample (nullptr, true);
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new PrysmProcessor(); }
