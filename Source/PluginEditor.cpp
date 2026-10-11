#include "PluginEditor.h"
#include "UI/SampleCore.h"

using namespace prysm;
using namespace prysm::ui;

namespace
{
//==============================================================================
class GearButton : public juce::Button
{
public:
    GearButton() : juce::Button ("settings") { setTooltip ("Settings"); setMouseCursor (juce::MouseCursor::PointingHandCursor); }
    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        auto b = getLocalBounds().toFloat().reduced (3.0f);
        const auto c = b.getCentre();
        const float R = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f;
        juce::Path gear;
        const int teeth = 8;
        for (int i = 0; i < teeth * 2; ++i)
        {
            const float a0 = juce::MathConstants<float>::twoPi * (float) i / (float) (teeth * 2);
            const float a1 = juce::MathConstants<float>::twoPi * ((float) i + 1.0f) / (float) (teeth * 2);
            const float r = (i % 2 == 0) ? R : R * 0.76f;
            gear.addArc (c.x - r, c.y - r, r * 2, r * 2, a0, a1, i == 0);
        }
        gear.closeSubPath();
        g.setColour (over || down ? col::acid : col::grey);
        g.strokePath (gear, juce::PathStrokeType (1.5f));
        g.drawEllipse (c.x - R * 0.28f, c.y - R * 0.28f, R * 0.56f, R * 0.56f, 1.5f);
    }
};

//==============================================================================
class PitchPanel : public Panel
{
public:
    explicit PitchPanel (PrysmProcessor& p)
        : Panel ("PITCH ENGINE"), proc (p),
          root (p.apvts, ids::root, "ROOT NOTE", "Key that plays the sample at its original pitch (MIDI 60 = C5)"),
          tune (p.apvts, ids::tune, "TUNE", "Fine tuning in cents (100 cents = 1 semitone)", true),
          range (p.apvts, ids::pitchRange, "PITCH RANGE", "Pitch wheel range in semitones"),
          transpose (p.apvts, ids::transpose, "TRANSPOSE", "Shift every note by whole semitones", true),
          mode (p.apvts, ids::mode, { "ONE-SHOT", "GATE" }, 2, "ONE-SHOT: plays to the end of the sample, note-off ignored.  GATE: stops when the key is released."),
          reverseAttach (p.apvts, ids::reverse, reverse)
    {
        for (auto* k : { &root, &tune, &range, &transpose }) addAndMakeVisible (*k);
        addAndMakeVisible (mode);
        reverse.setButtonText ("REVERSE");
        reverse.setTooltip ("Play the sample backwards");
        addAndMakeVisible (reverse);
    }
    void resized() override
    {
        auto a = contentArea();
        auto row = a.removeFromTop (102);
        const int w = row.getWidth() / 4;
        for (auto* k : { &root, &tune, &range, &transpose }) k->setBounds (row.removeFromLeft (w).reduced (3, 0));
        a.removeFromTop (6);
        auto r2 = a.removeFromTop (34);
        mode.setBounds (r2.removeFromLeft (196).reduced (3, 0));
        r2.removeFromLeft (8);
        reverse.setBounds (r2.removeFromLeft (132));
        modeLabelY = mode.getBottom() + 3;
    }
    void paint (juce::Graphics& g) override
    {
        paintFrame (g);
        const int rootNote = (int) proc.apvts.getRawParameterValue (ids::root)->load();
        const int bend = (int) proc.apvts.getRawParameterValue (ids::pitchRange)->load();
        g.setFont (Fonts::title (13.0f, 0.2f));
        g.setColour (col::grey);
        g.drawText ("PLAYBACK MODE", mode.getX(), modeLabelY, 196, 14, juce::Justification::centred, false);
        auto info = getLocalBounds().withTrimmedTop (modeLabelY + 28).reduced (14, 0).withHeight (46);
        g.setColour (col::edge); g.drawHorizontalLine (info.getY(), 14.0f, (float) getWidth() - 14.0f);
        g.setFont (Fonts::value (12.5f));
        g.setColour (col::grey);
        g.drawText ("UNITY KEY", info.getX(), info.getY() + 6, 100, 16, juce::Justification::left, false);
        g.setColour (col::acid);
        g.drawText (noteName (rootNote) + "  (MIDI " + juce::String (rootNote) + ")", info.getX() + 100, info.getY() + 6, 200, 16, juce::Justification::left, false);
        g.setColour (col::grey);
        g.drawText ("PITCH WHEEL", info.getX(), info.getY() + 24, 100, 16, juce::Justification::left, false);
        g.setColour (col::text);
        g.drawText ("+/- " + juce::String (bend) + " semitones", info.getX() + 100, info.getY() + 24, 200, 16, juce::Justification::left, false);
    }
    void tick() { const int r = (int) proc.apvts.getRawParameterValue (ids::root)->load(); const int b = (int) proc.apvts.getRawParameterValue (ids::pitchRange)->load(); if (r != lastRoot || b != lastBend) { lastRoot = r; lastBend = b; repaint(); } }

private:
    PrysmProcessor& proc;
    ParamKnob root, tune, range, transpose;
    ChoiceSelector mode;
    juce::ToggleButton reverse;
    juce::AudioProcessorValueTreeState::ButtonAttachment reverseAttach;
    int modeLabelY = 0, lastRoot = -1, lastBend = -1;
};

//==============================================================================
class EnvelopePanel : public Panel
{
public:
    explicit EnvelopePanel (PrysmProcessor& p)
        : Panel ("ENVELOPE LAB"), graph (p.apvts),
          a (p.apvts, ids::attack, "ATTACK", "Time to reach full level"),
          d (p.apvts, ids::decay, "DECAY", "Time to fall to the sustain level"),
          s (p.apvts, ids::sustain, "SUSTAIN", "Level held while the sample plays"),
          r (p.apvts, ids::release, "RELEASE", "Fade-out time after note-off (GATE mode)")
    {
        addAndMakeVisible (graph);
        for (auto* k : { &a, &d, &s, &r }) { k->setCompact (true); addAndMakeVisible (*k); }
    }
    void resized() override
    {
        auto c = contentArea();
        graph.setBounds (c.removeFromTop (104).reduced (2, 0));
        c.removeFromTop (4);
        const int w = c.getWidth() / 4;
        for (auto* k : { &a, &d, &s, &r }) k->setBounds (c.removeFromLeft (w));
    }
    void tick() { graph.tick(); }
private:
    EnvelopeGraph graph;
    ParamKnob a, d, s, r;
};

//==============================================================================
class MutationPanel : public Panel
{
public:
    explicit MutationPanel (PrysmProcessor& p)
        : Panel ("MUTATION ENGINE"), scope (p.apvts),
          filterType (p.apvts, ids::filterType, { "OFF", "LP", "HP" }, 1, "Filter mode: off, low-pass or high-pass"),
          cutoff (p.apvts, ids::cutoff, "CUTOFF", "Filter cutoff frequency"),
          res (p.apvts, ids::resonance, "RESONANCE", "Filter resonance"),
          drive (p.apvts, ids::drive, "DRIVE", "Saturation with level compensation (2x oversampled)"),
          width (p.apvts, ids::width, "WIDTH", "Stereo width. 0% = mono. The mono sum is never altered.", false),
          revMix (p.apvts, ids::reverbMix, "MIX", "Reverb level"),
          revSize (p.apvts, ids::reverbSize, "SIZE", "Reverb decay length"),
          tail (p.apvts, ids::tail, "TAIL", "Fade-out applied to the end of the sample region (separate from RELEASE and the reverb)"),
          motion (p.apvts, ids::motion, "AMOUNT", "Depth of the per-note LFO"),
          motionRate (p.apvts, ids::motionRate, "RATE", "LFO speed"),
          motionDest (p.apvts, ids::motionDest, { "FILTER", "PITCH", "AMP", "PAN" }, 2, "What the LFO modulates (FILTER needs the filter enabled)"),
          rnd (p.apvts, ids::random, "AMOUNT", "Subtle per-note variation of pitch, filter, pan and level"),
          seed (p.apvts, ids::randomSeed, "SEED", "Same seed = same variation sequence every time")
    {
        for (auto* k : { &cutoff, &res, &drive, &width, &revMix, &revSize, &tail, &motion, &motionRate, &rnd, &seed }) addAndMakeVisible (*k);
        addAndMakeVisible (filterType); addAndMakeVisible (motionDest); addAndMakeVisible (scope);
    }

    void resized() override
    {
        auto c = contentArea();
        const int cw = c.getWidth() / 8;
        const int rowH = c.getHeight() / 2;
        row1 = c.removeFromTop (rowH);
        row2 = c;
        auto cell = [&] (juce::Rectangle<int> row, int col, int span = 1) { return juce::Rectangle<int> (row.getX() + col * cw, row.getY() + 14, cw * span, row.getHeight() - 14).reduced (3, 0); };

        filterType.setBounds (cell (row1, 0).withTrimmedTop (2).withHeight (60));
        cutoff.setBounds (cell (row1, 1)); res.setBounds (cell (row1, 2)); drive.setBounds (cell (row1, 3)); width.setBounds (cell (row1, 4));
        revMix.setBounds (cell (row1, 5)); revSize.setBounds (cell (row1, 6)); tail.setBounds (cell (row1, 7));

        motion.setBounds (cell (row2, 0)); motionRate.setBounds (cell (row2, 1));
        motionDest.setBounds (cell (row2, 2, 2).withTrimmedTop (2).withHeight (60).reduced (6, 0));
        rnd.setBounds (cell (row2, 4)); seed.setBounds (cell (row2, 5));
        scope.setBounds (cell (row2, 6, 2).withTrimmedBottom (14).reduced (6, 2));
        cw_ = cw;
    }

    void paint (juce::Graphics& g) override
    {
        paintFrame (g);
        auto caption = [&] (juce::Rectangle<int> row, int c0, int span, const juce::String& t)
        {
            auto r = juce::Rectangle<int> (row.getX() + c0 * cw_ + 4, row.getY() + 1, span * cw_ - 8, 12);
            g.setColour (col::greyDim); g.setFont (Fonts::title (12.0f, 0.3f));
            g.drawText (t, r, juce::Justification::left, false);
            g.setColour (col::edge); g.drawHorizontalLine (r.getBottom() + 1, (float) r.getX(), (float) r.getRight());
        };
        caption (row1, 0, 3, "FILTER"); caption (row1, 3, 1, "DRIVE"); caption (row1, 4, 1, "STEREO"); caption (row1, 5, 3, "SPACE  +  TAIL");
        caption (row2, 0, 4, "MOTION"); caption (row2, 4, 2, "RANDOM"); caption (row2, 6, 2, "LFO PREVIEW");
        g.setColour (col::grey); g.setFont (Fonts::title (12.5f, 0.2f));
        g.drawText ("TYPE", filterType.getX(), filterType.getBottom() + 3, filterType.getWidth(), 14, juce::Justification::centred, false);
        g.drawText ("TARGET", motionDest.getX(), motionDest.getBottom() + 3, motionDest.getWidth(), 14, juce::Justification::centred, false);
    }
    void tick (float dt, bool animate) { scope.tick (dt, animate); }

private:
    LfoScope scope;
    ChoiceSelector filterType;
    ParamKnob cutoff, res, drive, width, revMix, revSize, tail, motion, motionRate;
    ChoiceSelector motionDest;
    ParamKnob rnd, seed;
    juce::Rectangle<int> row1, row2;
    int cw_ = 76;
};

//==============================================================================
class OutputPanel : public Panel
{
public:
    explicit OutputPanel (PrysmProcessor& p)
        : Panel ("OUTPUT"),
          vol (p.apvts, ids::gain, "VOLUME", "Master volume"),
          pan (p.apvts, ids::pan, "PAN", "Master pan", true),
          vel (p.apvts, ids::velSens, "VELOCITY", "How strongly note velocity changes the level"),
          voices (p.apvts, ids::voices, "VOICES", "Maximum polyphony. The oldest note fades out when the limit is reached.")
    {
        for (auto* k : { &vol, &pan, &vel, &voices }) { k->setCompact (true); addAndMakeVisible (*k); }
    }
    void resized() override
    {
        auto c = contentArea().reduced (0, 2);
        const int w = c.getWidth() / 2, h = c.getHeight() / 2;
        vol.setBounds (c.getX(), c.getY(), w, h);           pan.setBounds (c.getX() + w, c.getY(), w, h);
        vel.setBounds (c.getX(), c.getY() + h, w, h);       voices.setBounds (c.getX() + w, c.getY() + h, w, h);
    }
private:
    ParamKnob vol, pan, vel, voices;
};
} // namespace

//==============================================================================
class PrysmEditor::Impl : public juce::Component
{
public:
    explicit Impl (PrysmProcessor& p, PrysmEditor& o)
        : proc (p), owner (o), tooltip (this, 450),
          core (p), pitch (p), env (p), mutation (p), output (p), monitor (p), keys (p)
    {
        setLookAndFeel (&laf);
        for (auto* c : std::initializer_list<juce::Component*> { &core, &pitch, &env, &mutation, &output, &monitor, &keys, &gear })
            addAndMakeVisible (*c);
        gear.onClick = [this] { showSettings(); };
        core.onLoadClicked = [this] { browse(); };
        core.setAnimationsEnabled (! proc.reduceMotion.load());
        updateState();
    }
    ~Impl() override { setLookAndFeel (nullptr); }

    void resized() override
    {
        core.setBounds (16, 64, 700, 272);
        pitch.setBounds (728, 64, 376, 272);
        env.setBounds (16, 346, 296, 228);
        mutation.setBounds (324, 346, 632, 228);
        output.setBounds (968, 346, 136, 228);
        monitor.setBounds (16, 582, 1088, 28);
        keys.setBounds (16, 618, 1088, 70);
        gear.setBounds (1072, 14, 28, 28);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (col::bg0);
        // atmosphere: faint lab grid + header glow
        g.setColour (col::acid.withAlpha (0.025f));
        for (int x = 0; x < kDesignW; x += 28) g.drawVerticalLine (x, 0.0f, (float) kDesignH);
        for (int y = 0; y < kDesignH; y += 28) g.drawHorizontalLine (y, 0.0f, (float) kDesignW);
        g.setGradientFill (juce::ColourGradient (col::radio.withAlpha (0.10f), 60.0f, 0.0f, juce::Colours::transparentBlack, 520.0f, 90.0f, true));
        g.fillRect (0, 0, kDesignW, 120);
        g.setGradientFill (juce::ColourGradient (col::bg0.withAlpha (0.0f), 0.0f, 0.0f, col::bg0.withAlpha (0.6f), 0.0f, (float) kDesignH, false));
        g.fillRect (getLocalBounds());

        // ---- header
        drawLogo (g, { 22.0f, 6.0f, 44.0f, 44.0f });
        g.setColour (col::text);
        g.setFont (Fonts::title (36.0f, 0.34f));
        g.drawText ("PRYSM", 82, 2, 220, 38, juce::Justification::centredLeft, false);
        g.setColour (col::grey);
        g.setFont (Fonts::title (11.0f, 0.5f));
        g.drawText ("ALIEN SAMPLE LAB", 84, 37, 220, 14, juce::Justification::centredLeft, false);
        g.setColour (col::edge);
        g.drawHorizontalLine (55, 16.0f, (float) kDesignW - 16.0f);

        // engine state
        const float blink = (status == PrysmProcessor::SampleStatus::Loading) ? (0.5f + 0.5f * std::sin ((float) juce::Time::getMillisecondCounter() * 0.012f)) : 1.0f;
        auto led = juce::Rectangle<float> (10.0f, 10.0f).withCentre ({ 322.0f, 28.0f });
        g.setColour (stateColour.withAlpha (0.18f * blink)); g.fillEllipse (led.expanded (5.0f));
        g.setColour (stateColour.withAlpha (blink)); g.fillEllipse (led);
        g.setColour (col::text); g.setFont (Fonts::title (15.0f, 0.22f));
        g.drawText (stateText, 336, 17, 190, 22, juce::Justification::centredLeft, false);

        // current sample
        g.setColour (col::greyDim); g.setFont (Fonts::title (11.0f, 0.4f));
        g.drawText ("SAMPLE", 560, 13, 80, 12, juce::Justification::left, false);
        g.setColour (col::text); g.setFont (Fonts::title (17.0f, 0.06f));
        g.drawText (sampleName, 560, 25, 330, 22, juce::Justification::left, true);

        // discreet audio activity
        for (int i = 0; i < 5; ++i)
        {
            const float h = 4.0f + 24.0f * bars[(size_t) i];
            auto b = juce::Rectangle<float> (4.0f, h).withCentre ({ 1008.0f + (float) i * 8.0f, 28.0f });
            g.setColour (col::acid.withAlpha (0.25f + 0.75f * bars[(size_t) i]));
            g.fillRoundedRectangle (b, 1.5f);
        }
    }

    //==========================================================================
    void tick (float dt)
    {
        const bool animate = ! proc.reduceMotion.load();
        updateState();
        core.tick (dt);
        env.tick();
        pitch.tick();
        mutation.tick (dt, animate);
        monitor.tick();
        keys.tick();

        // activity bars
        const float act = juce::jlimit (0.0f, 1.0f, proc.engine.monitor.activity.load() * 1.4f);
        static const float shape[5] = { 0.55f, 0.9f, 1.0f, 0.75f, 0.45f };
        bool dirty = false;
        for (int i = 0; i < 5; ++i)
        {
            const float wobble = animate ? 0.75f + 0.25f * std::sin ((float) juce::Time::getMillisecondCounter() * 0.01f * (1.0f + 0.4f * (float) i) + (float) i) : 1.0f;
            const float target = act * shape[(size_t) i] * wobble;
            const float nv = bars[(size_t) i] + (target - bars[(size_t) i]) * 0.5f;
            if (std::abs (nv - bars[(size_t) i]) > 0.01f) dirty = true;
            bars[(size_t) i] = nv;
        }
        if (dirty || status == PrysmProcessor::SampleStatus::Loading) repaint (540, 0, kDesignW - 540, 56);
    }

    void updateState()
    {
        const auto s = proc.getStatus();
        const auto sample = proc.getUiSample();
        const int voices = proc.engine.monitor.activeVoices.load();
        juce::String text; juce::Colour c;
        switch (s)
        {
            case PrysmProcessor::SampleStatus::Loading: text = "LOADING SAMPLE"; c = col::warn; break;
            case PrysmProcessor::SampleStatus::Missing: text = "SAMPLE MISSING"; c = col::warn; break;
            case PrysmProcessor::SampleStatus::Error:   text = "LOAD ERROR";     c = col::error; break;
            case PrysmProcessor::SampleStatus::Loaded:  text = voices > 0 ? "ENGINE ACTIVE" : "ENGINE READY"; c = voices > 0 ? col::radio : col::acid; break;
            default:                                    text = "ENGINE IDLE";    c = col::greyDim; break;
        }
        const juce::String name = sample ? sample->name
                                         : (s == PrysmProcessor::SampleStatus::Missing ? proc.getMissingFileName() : juce::String ("NO SAMPLE"));
        if (text != stateText || c != stateColour || name != sampleName || s != status)
        {
            stateText = text; stateColour = c; sampleName = name; status = s;
            repaint (0, 0, kDesignW, 56);
        }
        const bool controlsActive = sample != nullptr || s == PrysmProcessor::SampleStatus::Missing;
        for (auto* p : std::initializer_list<Panel*> { &pitch, &env, &mutation, &output }) p->setDormant (! controlsActive);
    }

    void setAnimations (bool on)
    {
        proc.reduceMotion = ! on;
        core.setAnimationsEnabled (on);
    }

    void browse()
    {
        static juce::File lastDir = juce::File::getSpecialLocation (juce::File::userMusicDirectory);
        juce::String wild;
        for (auto& e : proc.getSupportedExtensions()) wild += (wild.isEmpty() ? "*" : ";*") + e;
        chooser = std::make_unique<juce::FileChooser> (proc.getStatus() == PrysmProcessor::SampleStatus::Missing ? "LOCATE SAMPLE" : "LOAD SAMPLE", lastDir, wild);
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc)
                              {
                                  auto f = fc.getResult();
                                  if (f.existsAsFile()) { lastDir = f.getParentDirectory(); proc.loadSampleAsync (f); }
                              });
    }

    void showSettings()
    {
        juce::PopupMenu m;
        m.setLookAndFeel (&laf);
        m.addItem (1, "REDUCE ANIMATIONS", true, proc.reduceMotion.load());
        m.addItem (2, "CLEAR SAMPLE", proc.getUiSample() != nullptr || proc.getStatus() == PrysmProcessor::SampleStatus::Missing);
        m.addSeparator();
        m.addItem (3, "RESET WINDOW SIZE");
        m.addItem (4, "ABOUT PRYSM");
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&gear).withMinimumWidth (190),
                         [this] (int r)
                         {
                             if (r == 1) setAnimations (proc.reduceMotion.load());   // toggles: reduceMotion=true -> animations on
                             if (r == 2) proc.clearSample();
                             if (r == 3) owner.setSize (kDesignW, kDesignH);
                             if (r == 4)
                                 juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::NoIcon, "PRYSM  1.0.0",
                                     "Alien sample instrument (VST3).\n\n"
                                     "Drop a one-shot, play it chromatically, mutate it.\n\n"
                                     "Built with JUCE (AGPLv3).\n"
                                     "Fonts: Rajdhani and Share Tech Mono (SIL Open Font License).",
                                     "CLOSE", this);
                         });
    }

    void setDragHover (bool hover, bool supported) { core.setDragHover (hover, supported); }

    PrysmProcessor& proc;
    PrysmEditor& owner;
    PrysmLookAndFeel laf;
    juce::TooltipWindow tooltip;
    SampleCore core;
    PitchPanel pitch;
    EnvelopePanel env;
    MutationPanel mutation;
    OutputPanel output;
    VoiceMonitor monitor;
    KeyboardStrip keys;
    GearButton gear;
    std::unique_ptr<juce::FileChooser> chooser;

    PrysmProcessor::SampleStatus status = PrysmProcessor::SampleStatus::Empty;
    juce::String stateText = "ENGINE IDLE", sampleName = "NO SAMPLE";
    juce::Colour stateColour = col::greyDim;
    std::array<float, 5> bars {};
};

//==============================================================================
PrysmEditor::PrysmEditor (PrysmProcessor& p) : AudioProcessorEditor (p), proc (p)
{
    impl = std::make_unique<Impl> (p, *this);
    addAndMakeVisible (*impl);

    constrainer.setFixedAspectRatio ((double) kDesignW / (double) kDesignH);
    constrainer.setSizeLimits (560, 350, 2800, 1750);
    setConstrainer (&constrainer);
    setResizable (true, true);

    const int w = juce::jlimit (560, 2800, proc.uiWidth.load());
    setSize (w, juce::roundToInt ((double) w * kDesignH / kDesignW));

    proc.addChangeListener (this);
    startTimerHz (proc.reduceMotion.load() ? 15 : 30);
}

PrysmEditor::~PrysmEditor()
{
    stopTimer();
    proc.removeChangeListener (this);
}

void PrysmEditor::paint (juce::Graphics& g) { g.fillAll (col::bg0); }

void PrysmEditor::resized()
{
    const float scale = (float) getWidth() / (float) kDesignW;
    impl->setBounds (0, 0, kDesignW, kDesignH);
    impl->setTransform (juce::AffineTransform::scale (scale));
    proc.uiWidth = getWidth();
    proc.uiHeight = getHeight();
}

void PrysmEditor::timerCallback()
{
    const bool reduced = proc.reduceMotion.load();
    const int hz = reduced ? 15 : 30;
    if (getTimerInterval() != 1000 / hz) startTimerHz (hz);
    impl->tick (1.0f / (float) hz);
}

void PrysmEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    impl->updateState();
    impl->repaint();
}

bool PrysmEditor::isInterestedInFileDrag (const juce::StringArray& files) { return files.size() > 0; }

void PrysmEditor::fileDragEnter (const juce::StringArray& files, int, int)
{
    impl->setDragHover (true, files.size() > 0 && proc.isSupportedFile (juce::File (files[0])));
}
void PrysmEditor::fileDragExit (const juce::StringArray&) { impl->setDragHover (false, true); }

void PrysmEditor::filesDropped (const juce::StringArray& files, int, int)
{
    impl->setDragHover (false, true);
    if (files.size() > 0) proc.loadSampleAsync (juce::File (files[0]));
}
