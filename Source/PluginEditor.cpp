#include "PluginEditor.h"
#include "UI/SampleCore.h"
#include "UI/Atmosphere.h"

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
        g.setColour (over || down ? col::neon : col::grey);
        g.strokePath (gear, juce::PathStrokeType (1.5f));
        g.drawEllipse (c.x - R * 0.28f, c.y - R * 0.28f, R * 0.56f, R * 0.56f, 1.5f);
    }
};

//==============================================================================
/** ROOT NOTE / FINE TUNE / playback mode / reverse: the only "tweak" controls, kept deliberately few. */
class PitchPanel : public Panel
{
public:
    explicit PitchPanel (PrysmProcessor& p)
        : Panel ("PITCH"),
          root (p.apvts, ids::root, "ROOT NOTE", "Key that plays the sample at its original pitch (C5 = MIDI 60, FL Studio convention)"),
          tune (p.apvts, ids::tune, "FINE TUNE", "Fine tuning in cents (100 cents = 1 semitone)", true),
          mode (p.apvts, ids::mode, { "ONE-SHOT", "GATE" }, 2, "ONE-SHOT: plays to the end of the sample, note-off ignored.  GATE: stops when the key is released."),
          reverseAttach (p.apvts, ids::reverse, reverse)
    {
        for (auto* k : { &root, &tune }) addAndMakeVisible (*k);
        addAndMakeVisible (mode);
        reverse.setButtonText ("REVERSE");
        reverse.setTooltip ("Play the sample backwards");
        addAndMakeVisible (reverse);
    }
    void resized() override
    {
        auto a = contentArea();
        auto row = a.removeFromTop (104);
        const int w = row.getWidth() / 2;
        root.setBounds (row.removeFromLeft (w).reduced (6, 0));
        tune.setBounds (row.reduced (6, 0));
        a.removeFromTop (4);
        mode.setBounds (a.removeFromTop (34).reduced (4, 0));
        a.removeFromTop (6);
        reverse.setBounds (a.removeFromTop (34).reduced (4, 0));
    }

private:
    ParamKnob root, tune;
    ChoiceSelector mode;
    juce::ToggleButton reverse;
    juce::AudioProcessorValueTreeState::ButtonAttachment reverseAttach;
};

//==============================================================================
/** Envelope Lab, laid out like FL Studio's: graph on top, DELAY ATT HOLD DEC SUS REL, then the three TENSION knobs. */
class EnvelopePanel : public Panel
{
public:
    explicit EnvelopePanel (PrysmProcessor& p)
        : Panel ("ENVELOPE LAB"), graph (p.apvts),
          del (p.apvts, ids::delay, "DELAY", "Silence before the sound starts"),
          a (p.apvts, ids::attack, "ATT", "Attack: time to reach full level"),
          h (p.apvts, ids::hold, "HOLD", "Time spent at full level before the decay"),
          d (p.apvts, ids::decay, "DEC", "Decay: time to fall to the sustain level"),
          s (p.apvts, ids::sustain, "SUS", "Sustain: level held while the key is down"),
          r (p.apvts, ids::release, "REL", "Release: fade-out after the key is released (GATE mode)"),
          ta (p.apvts, ids::tensionA, "ATT", "Attack curve. Left = fast start, right = slow start", true),
          td (p.apvts, ids::tensionD, "DEC", "Decay curve", true),
          tr (p.apvts, ids::tensionR, "REL", "Release curve", true)
    {
        addAndMakeVisible (graph);
        for (auto* k : { &del, &a, &h, &d, &s, &r, &ta, &td, &tr }) { k->setCompact (true); addAndMakeVisible (*k); }
    }
    void resized() override
    {
        auto c = contentArea();
        graph.setBounds (c.removeFromTop (118).reduced (2, 0));
        c.removeFromTop (14);
        const int w = c.getWidth() / 9;
        knobsY = c.getY();
        for (auto* k : { &del, &a, &h, &d, &s, &r }) k->setBounds (c.removeFromLeft (w));
        tensionX = c.getX();
        for (auto* k : { &ta, &td, &tr }) k->setBounds (c.removeFromLeft (w));
        tensionW = w * 3;
    }
    void paint (juce::Graphics& g) override
    {
        paintFrame (g);
        g.setColour (col::greyDim); g.setFont (Fonts::title (11.5f, 0.3f));
        g.drawText ("TENSION", tensionX, knobsY - 14, tensionW, 12, juce::Justification::centred, false);
        g.setColour (col::edge); g.drawVerticalLine (tensionX - 2, (float) knobsY - 12.0f, (float) getHeight() - 12.0f);
    }
    void tick() { graph.tick(); }
private:
    EnvelopeGraph graph;
    ParamKnob del, a, h, d, s, r, ta, td, tr;
    int knobsY = 0, tensionX = 0, tensionW = 0;
};
} // namespace

//==============================================================================
class PrysmEditor::Impl : public juce::Component
{
public:
    explicit Impl (PrysmProcessor& p, PrysmEditor& o)
        : proc (p), owner (o), tooltip (this, 450), core (p), pitch (p), env (p), keys (p)
    {
        setLookAndFeel (&laf);
        addAndMakeVisible (atmosphere);
        for (auto* c : std::initializer_list<juce::Component*> { &core, &pitch, &env, &keys, &gear })
            addAndMakeVisible (*c);
        pitch.setBufferedToImage (true);
        env.setBufferedToImage (true);
        gear.onClick = [this] { showSettings(); };
        core.onLoadClicked = [this] { browse(); };
        setAnimations (! proc.reduceMotion.load());
        updateState();
    }
    ~Impl() override { setLookAndFeel (nullptr); }

    void resized() override
    {
        atmosphere.setBounds (getLocalBounds());
        core.setBounds (24, 64, 952, 236);
        env.setBounds (24, 312, 628, 250);
        pitch.setBounds (664, 312, 312, 250);
        keys.setBounds (24, 574, 952, 66);
        gear.setBounds (944, 16, 28, 28);
    }

    void paintOverChildren (juce::Graphics& g) override
    {
        // Title only: no logo, no tagline.
        const juce::String title = "PRYSM";
        g.setFont (Fonts::headline (30.0f, 0.34f));
        const juce::Rectangle<int> r (28, 10, 320, 44);
        g.setColour (col::neon.withAlpha (0.20f));
        g.drawText (title, r.translated (0, 0).expanded (1), juce::Justification::centredLeft, false);
        g.setColour (col::text);
        g.drawText (title, r, juce::Justification::centredLeft, false);
    }

    //==========================================================================
    void tick (float dt)
    {
        updateState();
        atmosphere.tick (dt, proc.engine.monitor.activity.load() * 1.4f);
        core.tick (dt);
        env.tick();
        keys.tick();
    }

    void updateState()
    {
        const auto s = proc.getStatus();
        const bool controlsActive = proc.getUiSample() != nullptr || s == PrysmProcessor::SampleStatus::Missing;
        for (auto* p : std::initializer_list<Panel*> { &pitch, &env }) p->setDormant (! controlsActive);
    }

    void setAnimations (bool on)
    {
        proc.reduceMotion = ! on;
        core.setAnimationsEnabled (on);
        atmosphere.setAnimated (on);
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
                                 juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::NoIcon, "PRYSM  1.1.0",
                                     "Simple sample instrument (VST3).\n\n"
                                     "Drop a sample, shape it with the envelope, play it from your piano roll.\n\n"
                                     "Built with JUCE (AGPLv3).\n"
                                     "Fonts: Orbitron, Rajdhani and Share Tech Mono (SIL Open Font License).",
                                     "CLOSE", this);
                         });
    }

    void setDragHover (bool hover, bool supported) { core.setDragHover (hover, supported); }

    PrysmProcessor& proc;
    PrysmEditor& owner;
    PrysmLookAndFeel laf;
    juce::TooltipWindow tooltip;
    Atmosphere atmosphere;
    SampleCore core;
    PitchPanel pitch;
    EnvelopePanel env;
    KeyboardStrip keys;
    GearButton gear;
    std::unique_ptr<juce::FileChooser> chooser;
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
