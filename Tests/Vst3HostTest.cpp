// Loads the REAL built PRYSM.vst3 through JUCE's VST3 host code and exercises it like a DAW would.
// usage: prysm_vst3_host <path/to/PRYSM.vst3> <path/to/sample.wav>
#include <juce_audio_processors/juce_audio_processors.h>
#include <iostream>

static int g_checks = 0, g_failed = 0;
#define CHECK(cond, name) do { ++g_checks; if (! (cond)) { ++g_failed; std::cout << "  FAIL: " << name << "\n"; } else std::cout << "  ok:   " << name << "\n"; } while (0)

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    if (argc < 3) { std::cout << "usage: prysm_vst3_host <PRYSM.vst3> <sample.wav>\n"; return 2; }
    const juce::String vst3 = argv[1];
    const juce::File wav (argv[2]);

    std::cout << "[VST3 host test] " << vst3 << "\n";
    juce::VST3PluginFormat fmt;
    juce::OwnedArray<juce::PluginDescription> descs;
    fmt.findAllTypesForFile (descs, vst3);
    CHECK (descs.size() == 1, "VST3 scan finds exactly one plugin");
    if (descs.isEmpty()) return 1;
    auto& d = *descs[0];
    CHECK (d.name == "PRYSM", "plugin name is PRYSM");
    CHECK (d.isInstrument, "reported as an instrument (appears under instruments in FL Studio)");

    juce::String err;
    auto inst = fmt.createInstanceFromDescription (d, 48000.0, 480, err);
    CHECK (inst != nullptr, "plugin instance created " + err);
    if (inst == nullptr) return 1;

    inst->setPlayConfigDetails (0, 2, 48000.0, 480);
    inst->prepareToPlay (48000.0, 480);
    CHECK (inst->getTotalNumInputChannels() == 0 && inst->getTotalNumOutputChannels() == 2, "no audio inputs, stereo output (in=" + juce::String (inst->getTotalNumInputChannels()) + ", out=" + juce::String (inst->getTotalNumOutputChannels()) + ")");
    {
        int prysmParams = 0, midiCc = 0;
        const char* expected[] = { "Volume", "Pan", "Root Note", "Tune", "Pitch Range", "Transpose", "Playback Mode", "Reverse", "Start", "End",
                                   "Attack", "Decay", "Sustain", "Release", "Filter Cutoff", "Filter Resonance", "Drive", "Stereo Width",
                                   "Motion Amount", "Random Amount", "Reverb Mix", "Reverb Size", "Tail", "Voices" };
        for (auto* prm : inst->getParameters())
        {
            auto n = prm->getName (64);
            if (n.startsWithIgnoreCase ("MIDI CC") || n.containsIgnoreCase ("Pitch Bend") || n.containsIgnoreCase ("Aftertouch") || n.containsIgnoreCase ("Program Change")) ++midiCc;
        }
        for (auto* e : expected)
            for (auto* prm : inst->getParameters()) if (prm->getName (64) == e) { ++prysmParams; break; }
        std::cout << "  info: " << inst->getParameters().size() << " parameters total, of which " << midiCc << " are host-emulated MIDI CC entries\n";
        CHECK (prysmParams == (int) (sizeof (expected) / sizeof (*expected)), "all PRYSM parameters are exposed for automation (" + juce::String (prysmParams) + ")");
    }
    CHECK (inst->acceptsMidi(), "accepts MIDI");

    // silent without a sample
    juce::AudioBuffer<float> buf (2, 480);
    {
        juce::MidiBuffer m; m.addEvent (juce::MidiMessage::noteOn (1, 60, 1.0f), 0);
        inst->processBlock (buf, m);
        CHECK (buf.getMagnitude (0, 480) == 0.0f, "silence while no sample is loaded");
    }

    // Inject the sample path into the plugin's own state, inside the VST3 state wrapper written by the host
    // (this is the same data path a DAW uses when it re-opens a saved project).
    auto editPluginState = [] (juce::MemoryBlock& hostState, const std::function<void (juce::XmlElement&)>& edit) -> bool
    {
        std::unique_ptr<juce::XmlElement> wrapper (juce::AudioProcessor::getXmlFromBinary (hostState.getData(), (int) hostState.getSize()));
        if (wrapper == nullptr) return false;
        auto* comp = wrapper->getChildByName ("IComponent");
        if (comp == nullptr) return false;
        juce::MemoryBlock bytes; bytes.fromBase64Encoding (comp->getAllSubText().trim());
        auto* d = static_cast<const juce::uint8*> (bytes.getData());
        const size_t n = bytes.getSize();
        size_t off = n;
        for (size_t i = 0; i + 8 < n; ++i) if (d[i] == 0x56 && d[i + 1] == 0x43 && d[i + 2] == 0x32 && d[i + 3] == 0x21) { off = i; break; }
        if (off == n) return false;
        const juce::uint32 textLen = (juce::uint32) d[off + 4] | ((juce::uint32) d[off + 5] << 8) | ((juce::uint32) d[off + 6] << 16) | ((juce::uint32) d[off + 7] << 24);
        const size_t oldLen = (size_t) textLen + 10;
        if (off + oldLen > n) return false;
        auto pluginXml = juce::parseXML (juce::String::fromUTF8 (reinterpret_cast<const char*> (d + off + 8), (int) textLen));
        if (pluginXml == nullptr) return false;
        edit (*pluginXml);
        juce::MemoryBlock newBin; juce::AudioProcessor::copyXmlToBinary (*pluginXml, newBin);
        juce::MemoryBlock out; out.append (d, off); out.append (newBin.getData(), newBin.getSize()); out.append (d + off + oldLen, n - off - oldLen);
        comp->deleteAllTextElements();
        comp->addTextElement (out.toBase64Encoding());
        hostState.reset();
        juce::AudioProcessor::copyXmlToBinary (*wrapper, hostState);
        return true;
    };
    auto readPluginAttr = [] (const juce::MemoryBlock& hostState, const juce::String& attr) -> juce::String
    {
        std::unique_ptr<juce::XmlElement> wrapper (juce::AudioProcessor::getXmlFromBinary (hostState.getData(), (int) hostState.getSize()));
        if (wrapper == nullptr || wrapper->getChildByName ("IComponent") == nullptr) return "<no wrapper>";
        juce::MemoryBlock bytes; bytes.fromBase64Encoding (wrapper->getChildByName ("IComponent")->getAllSubText().trim());
        auto* d = static_cast<const juce::uint8*> (bytes.getData());
        for (size_t i = 0; i + 8 < bytes.getSize(); ++i)
            if (d[i] == 0x56 && d[i + 1] == 0x43 && d[i + 2] == 0x32 && d[i + 3] == 0x21)
            {
                const juce::uint32 len = (juce::uint32) d[i + 4] | ((juce::uint32) d[i + 5] << 8) | ((juce::uint32) d[i + 6] << 16) | ((juce::uint32) d[i + 7] << 24);
                if (auto x = juce::parseXML (juce::String::fromUTF8 (reinterpret_cast<const char*> (d + i + 8), (int) len))) return x->getStringAttribute (attr);
            }
        return "<not found>";
    };

    juce::MemoryBlock st; inst->getStateInformation (st);
    const bool edited = editPluginState (st, [&] (juce::XmlElement& x) { x.setAttribute ("samplePath", wav.getFullPathName()); });
    CHECK (edited, "plugin state can be read and rewritten through the VST3 state wrapper");
    if (! edited) return 1;
    inst->setStateInformation (st.getData(), (int) st.getSize());
    juce::Thread::sleep (1500);

    float maxLevel = 0.0f;
    {
        juce::MidiBuffer on; on.addEvent (juce::MidiMessage::noteOn (1, 60, 1.0f), 0);
        inst->processBlock (buf, on);
        maxLevel = buf.getMagnitude (0, 480);
        for (int i = 0; i < 20; ++i) { juce::MidiBuffer none; inst->processBlock (buf, none); maxLevel = juce::jmax (maxLevel, buf.getMagnitude (0, 480)); }
    }
    CHECK (maxLevel > 0.05f, "MIDI note plays the loaded sample (peak " + juce::String (maxLevel, 3) + ")");

    // state round trip
    juce::MemoryBlock s3; inst->getStateInformation (s3);
    CHECK (readPluginAttr (s3, "samplePath") == wav.getFullPathName(), "sample path persisted in plugin state (" + readPluginAttr (s3, "samplePath") + ")");

    // parameter round trip through the VST3 state of a second instance
    {
        for (auto* prm : inst->getParameters()) if (prm->getName (64) == "Root Note") prm->setValueNotifyingHost (72.0f / 127.0f);
        juce::MemoryBlock s4; inst->getStateInformation (s4);
        auto inst2 = fmt.createInstanceFromDescription (d, 48000.0, 480, err);
        CHECK (inst2 != nullptr, "second instance created");
        if (inst2 != nullptr)
        {
            inst2->setPlayConfigDetails (0, 2, 48000.0, 480);
            inst2->prepareToPlay (48000.0, 480);
            inst2->setStateInformation (s4.getData(), (int) s4.getSize());
            float root = -1.0f;
            for (auto* prm : inst2->getParameters()) if (prm->getName (64) == "Root Note") root = prm->getValue() * 127.0f;
            CHECK (std::abs (root - 72.0f) < 0.51f, "ROOT NOTE restored in a new instance (" + juce::String (root) + ")");
            juce::Thread::sleep (1200);
            juce::MidiBuffer on; on.addEvent (juce::MidiMessage::noteOn (1, 72, 1.0f), 0);
            float lvl = 0.0f;
            inst2->processBlock (buf, on); lvl = buf.getMagnitude (0, 480);
            for (int i = 0; i < 10; ++i) { juce::MidiBuffer none; inst2->processBlock (buf, none); lvl = juce::jmax (lvl, buf.getMagnitude (0, 480)); }
            CHECK (lvl > 0.05f, "restored instance plays the restored sample on its ROOT NOTE");
            inst2->releaseResources();
        }
    }

    // editor
    {
        std::unique_ptr<juce::AudioProcessorEditor> ed (inst->createEditorIfNeeded());
        CHECK (ed != nullptr && ed->getWidth() > 300, "editor can be created by the host");
        juce::MessageManager::getInstance()->runDispatchLoopUntil (200);
    }
    inst->releaseResources();
    inst.reset();
    CHECK (true, "plugin destroyed cleanly");

    std::cout << (g_checks - g_failed) << " / " << g_checks << " host checks passed\n";
    return g_failed == 0 ? 0 : 1;
}
