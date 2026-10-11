#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "Theme.h"
#include "../PluginProcessor.h"

namespace prysm::ui
{
//==============================================================================
/** Framed instrument panel with a technical title bar. Can be put to "dormant" when no sample is loaded. */
class Panel : public juce::Component
{
public:
    explicit Panel (juce::String t) : title (std::move (t)) {}

    void setDormant (bool d)
    {
        if (dormant == d) return;
        dormant = d;
        setAlpha (d ? 0.34f : 1.0f);
        setInterceptsMouseClicks (! d, ! d);
    }
    bool isDormant() const noexcept { return dormant; }
    juce::Rectangle<int> contentArea() const { return getLocalBounds().withTrimmedTop (30).reduced (10, 4); }

    void paint (juce::Graphics& g) override { paintFrame (g); }

protected:
    void paintFrame (juce::Graphics& g)
    {
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        g.setGradientFill (juce::ColourGradient (col::bg2, 0, 0, col::bg1, 0, r.getBottom(), false));
        g.fillRoundedRectangle (r, 7.0f);
        g.setColour (col::edge);
        g.drawRoundedRectangle (r, 7.0f, 1.0f);

        // title bar
        g.setColour (col::acid);
        g.fillRect (juce::Rectangle<float> (14.0f, 11.5f, 5.0f, 5.0f));
        g.setColour (col::text.withAlpha (0.92f));
        g.setFont (Fonts::title (15.0f, 0.24f));
        g.drawText (title, 26, 4, getWidth() / 2, 20, juce::Justification::centredLeft, false);

        g.setColour (col::edgeHi.withAlpha (0.55f));
        g.drawHorizontalLine (28, 10.0f, (float) getWidth() - 10.0f);
        // measurement ticks (lab-instrument motif)
        const float x0 = (float) getWidth() * 0.62f;
        for (int i = 0; i < 24; ++i)
        {
            const float x = (float) getWidth() - 14.0f - (float) i * 5.0f;
            if (x < x0) break;
            g.setColour (col::greyDim.withAlpha (0.35f + 0.65f * (1.0f - (float) i / 24.0f)));
            g.drawVerticalLine ((int) x, 19.0f, i % 4 == 0 ? 27.0f : 24.0f);
        }
        drawBrackets (g, r.reduced (3.0f), 7.0f, col::acid.withAlpha (0.35f), 1.0f);
    }

    juce::String title;
    bool dormant = false;
};

//==============================================================================
class ParamKnob : public juce::Component
{
public:
    ParamKnob (juce::AudioProcessorValueTreeState& apvts, const char* paramId, const juce::String& labelText,
               const juce::String& tooltipText, bool bipolar = false)
        : label (labelText)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, true);
        slider.setMouseDragSensitivity (220);
        slider.getProperties().set ("bipolar", bipolar);
        slider.setTooltip (tooltipText);
        slider.onValueChange = [this] { repaint (valueArea()); };
        addAndMakeVisible (slider);
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, paramId, slider);
    }

    void resized() override
    {
        auto b = getLocalBounds();
        b.removeFromBottom (textBlockHeight);
        slider.setBounds (b);
    }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds();
        g.setColour (col::grey);
        g.setFont (fitTitle (compact ? 12.5f : 13.5f, 0.14f, label, (float) b.getWidth() - 2.0f));
        g.drawText (label, b.getX(), b.getBottom() - textBlockHeight, b.getWidth(), 14, juce::Justification::centred, false);
        g.setColour (slider.isEnabled() ? col::text : col::greyDim);
        g.setFont (Fonts::value (compact ? 12.0f : 13.0f));
        g.drawText (slider.getTextFromValue (slider.getValue()), valueArea(), juce::Justification::centred, false);
    }

    void setCompact (bool c) { compact = c; textBlockHeight = c ? 28 : 30; resized(); }

    juce::Slider slider;

private:
    juce::Rectangle<int> valueArea() const { return { 0, getHeight() - textBlockHeight + 14, getWidth(), 15 }; }
    juce::String label;
    bool compact = false;
    int textBlockHeight = 30;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

//==============================================================================
/** Segmented selector bound to a choice parameter. */
class ChoiceSelector : public juce::Component, public juce::SettableTooltipClient
{
public:
    ChoiceSelector (juce::AudioProcessorValueTreeState& apvts, const char* paramId, juce::StringArray shortLabels,
                    int columnsIn, const juce::String& tooltipText)
        : labels (std::move (shortLabels)), columns (juce::jmax (1, columnsIn)),
          param (dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (paramId)))
    {
        jassert (param != nullptr);
        setTooltip (tooltipText);
        attach = std::make_unique<juce::ParameterAttachment> (*param, [this] (float v) { index = (int) v; repaint(); });
        attach->sendInitialUpdate();
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    void paint (juce::Graphics& g) override
    {
        const int n = labels.size();
        const int rows = (n + columns - 1) / columns;
        const float cw = (float) getWidth() / (float) columns, ch = (float) getHeight() / (float) rows;
        for (int i = 0; i < n; ++i)
        {
            auto cell = juce::Rectangle<float> (cw * (float) (i % columns), ch * (float) (i / columns), cw, ch).reduced (1.5f);
            const bool sel = i == index;
            g.setColour (sel ? col::deepGreen : col::bg2);
            g.fillRoundedRectangle (cell, 3.5f);
            g.setColour (sel ? col::acid.withAlpha (0.9f) : (i == hover ? col::edgeHi.brighter (0.3f) : col::edge));
            g.drawRoundedRectangle (cell, 3.5f, 1.0f);
            if (sel)
            {
                g.setColour (col::acid.withAlpha (0.18f));
                g.fillRoundedRectangle (cell.reduced (1.0f), 3.0f);
            }
            g.setColour (sel ? col::acid : col::grey);
            g.setFont (fitTitle (juce::jmin (16.0f, cell.getHeight() * 0.8f), 0.12f, labels[i], cell.getWidth() - 6.0f));
            g.drawText (labels[i], cell.toNearestInt(), juce::Justification::centred, false);
        }
    }

    void mouseMove (const juce::MouseEvent& e) override { const int h = indexAt (e.getPosition()); if (h != hover) { hover = h; repaint(); } }
    void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }
    void mouseDown (const juce::MouseEvent& e) override
    {
        const int i = indexAt (e.getPosition());
        if (i >= 0 && i < labels.size()) attach->setValueAsCompleteGesture ((float) i);
    }

private:
    int indexAt (juce::Point<int> p) const
    {
        const int rows = (labels.size() + columns - 1) / columns;
        const int c = juce::jlimit (0, columns - 1, p.x * columns / juce::jmax (1, getWidth()));
        const int r = juce::jlimit (0, rows - 1, p.y * rows / juce::jmax (1, getHeight()));
        return r * columns + c;
    }
    juce::StringArray labels;
    int columns, index = 0, hover = -1;
    juce::AudioParameterChoice* param;
    std::unique_ptr<juce::ParameterAttachment> attach;
};

//==============================================================================
/** Interactive ADSR visualisation: drag the three handles to edit the envelope. */
class EnvelopeGraph : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit EnvelopeGraph (juce::AudioProcessorValueTreeState& s)
        : pA (s.getParameter (ids::attack)), pD (s.getParameter (ids::decay)), pS (s.getParameter (ids::sustain)), pR (s.getParameter (ids::release))
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        setTooltip ("Drag the handles to shape ATTACK, DECAY, SUSTAIN and RELEASE");
    }

    /** Called from the editor timer: repaints only when a value changed. */
    void tick()
    {
        const float v[4] = { pA->getValue(), pD->getValue(), pS->getValue(), pR->getValue() };
        if (! std::equal (std::begin (v), std::end (v), std::begin (last))) { std::copy (std::begin (v), std::end (v), std::begin (last)); repaint(); }
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        g.setColour (col::bg0);
        g.fillRoundedRectangle (bounds, 5.0f);
        g.setColour (col::edge);
        g.drawRoundedRectangle (bounds.reduced (0.5f), 5.0f, 1.0f);

        auto p = plot();
        // grid
        g.setColour (col::edge.withAlpha (0.8f));
        for (int i = 0; i <= 8; ++i) g.drawVerticalLine ((int) (p.getX() + p.getWidth() * (float) i / 8.0f), p.getY(), p.getBottom());
        for (int i = 0; i <= 4; ++i) g.drawHorizontalLine ((int) (p.getY() + p.getHeight() * (float) i / 4.0f), p.getX(), p.getRight());

        const auto pts = points();
        juce::Path line;
        line.startNewSubPath (p.getX(), p.getBottom());
        line.lineTo (pts.a); line.lineTo (pts.d); line.lineTo (pts.s); line.lineTo (pts.r);
        juce::Path fill (line);
        fill.lineTo (pts.r.x, p.getBottom());
        fill.closeSubPath();
        g.setGradientFill (juce::ColourGradient (col::acid.withAlpha (0.30f), 0, p.getY(), col::acid.withAlpha (0.02f), 0, p.getBottom(), false));
        g.fillPath (fill);
        g.setColour (col::acid.withAlpha (0.18f));
        g.strokePath (line, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved));
        g.setColour (col::acid);
        g.strokePath (line, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved));

        auto handle = [&] (juce::Point<float> pt, bool active)
        {
            g.setColour (col::bg0);
            g.fillEllipse (juce::Rectangle<float> (11.0f, 11.0f).withCentre (pt));
            g.setColour (active ? col::radio : col::acid);
            g.drawEllipse (juce::Rectangle<float> (11.0f, 11.0f).withCentre (pt), 1.8f);
            if (active) { g.setColour (col::radio.withAlpha (0.25f)); g.fillEllipse (juce::Rectangle<float> (20.0f, 20.0f).withCentre (pt)); }
        };
        handle (pts.a, drag == 1 || hover == 1);
        handle (pts.d, drag == 2 || hover == 2);
        handle (pts.r, drag == 3 || hover == 3);

        // segment captions
        g.setFont (Fonts::value (11.0f));
        g.setColour (col::grey);
        g.drawText ("A " + pA->getCurrentValueAsText(), 8, 4, 90, 12, juce::Justification::left, false);
        g.drawText ("D " + pD->getCurrentValueAsText(), 100, 4, 90, 12, juce::Justification::left, false);
        g.drawText ("S " + pS->getCurrentValueAsText(), 8, getHeight() - 15, 90, 12, juce::Justification::left, false);
        g.drawText ("R " + pR->getCurrentValueAsText(), 100, getHeight() - 15, 90, 12, juce::Justification::left, false);
    }

    void mouseMove (const juce::MouseEvent& e) override { const int h = hit (e.position); if (h != hover) { hover = h; repaint(); } }
    void mouseExit (const juce::MouseEvent&) override { hover = 0; repaint(); }
    void mouseDown (const juce::MouseEvent& e) override
    {
        drag = hit (e.position);
        if (drag == 1) pA->beginChangeGesture();
        if (drag == 2) { pD->beginChangeGesture(); pS->beginChangeGesture(); }
        if (drag == 3) pR->beginChangeGesture();
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (drag == 0) return;
        auto p = plot();
        const float segW = p.getWidth() * kSeg;
        if (drag == 1) pA->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, (e.position.x - p.getX()) / segW));
        if (drag == 2)
        {
            const auto pts = points();
            pD->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, (e.position.x - pts.a.x) / segW));
            pS->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, (p.getBottom() - e.position.y) / p.getHeight()));
        }
        if (drag == 3)
        {
            const auto pts = points();
            pR->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, (e.position.x - pts.s.x) / segW));
        }
        repaint();
    }
    void mouseUp (const juce::MouseEvent&) override
    {
        if (drag == 1) pA->endChangeGesture();
        if (drag == 2) { pD->endChangeGesture(); pS->endChangeGesture(); }
        if (drag == 3) pR->endChangeGesture();
        drag = 0;
        repaint();
    }
    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        const int h = hit (e.position);
        auto reset = [] (juce::RangedAudioParameter* x) { x->beginChangeGesture(); x->setValueNotifyingHost (x->getDefaultValue()); x->endChangeGesture(); };
        if (h == 1) reset (pA);
        if (h == 2) { reset (pD); reset (pS); }
        if (h == 3) reset (pR);
    }

private:
    static constexpr float kSeg = 0.27f, kHold = 0.19f;
    struct Pts { juce::Point<float> a, d, s, r; };
    juce::Rectangle<float> plot() const { return getLocalBounds().toFloat().reduced (14.0f, 22.0f); }
    Pts points() const
    {
        auto p = plot();
        const float segW = p.getWidth() * kSeg;
        Pts q;
        q.a = { p.getX() + pA->getValue() * segW, p.getY() };
        q.d = { q.a.x + pD->getValue() * segW, p.getBottom() - pS->getValue() * p.getHeight() };
        q.s = { q.d.x + p.getWidth() * kHold, q.d.y };
        q.r = { q.s.x + pR->getValue() * segW, p.getBottom() };
        return q;
    }
    int hit (juce::Point<float> pos) const
    {
        const auto q = points();
        const float r = 12.0f;
        if (pos.getDistanceFrom (q.d) < r) return 2;
        if (pos.getDistanceFrom (q.a) < r) return 1;
        if (pos.getDistanceFrom (q.r) < r) return 3;
        return 0;
    }
    juce::RangedAudioParameter *pA, *pD, *pS, *pR;
    float last[4] = { -1, -1, -1, -1 };
    int drag = 0, hover = 0;
};

//==============================================================================
/** Live preview of the MOTION LFO (shape, depth and target). */
class LfoScope : public juce::Component
{
public:
    explicit LfoScope (juce::AudioProcessorValueTreeState& s)
        : amount (s.getRawParameterValue (ids::motion)), rate (s.getRawParameterValue (ids::motionRate)), target (s.getRawParameterValue (ids::motionDest)) {}

    void tick (float dt, bool animate)
    {
        if (animate) phase = std::fmod (phase + dt * 0.35f, 1.0f);
        const float a = amount->load(), r = rate->load(); const int t = (int) target->load();
        if (animate || a != lastA || r != lastR || t != lastT) { lastA = a; lastR = r; lastT = t; repaint(); }
    }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        g.setColour (col::bg0); g.fillRoundedRectangle (b, 5.0f);
        g.setColour (col::edge); g.drawRoundedRectangle (b.reduced (0.5f), 5.0f, 1.0f);
        auto p = b.reduced (8.0f, 14.0f);
        g.setColour (col::edge);
        g.drawHorizontalLine ((int) p.getCentreY(), p.getX(), p.getRight());
        const float a = juce::jlimit (0.0f, 1.0f, amount->load());
        const float cycles = juce::jlimit (0.6f, 5.0f, 0.6f + std::log2 (1.0f + rate->load()) * 1.0f);
        juce::Path path;
        for (int i = 0; i <= 120; ++i)
        {
            const float x = p.getX() + p.getWidth() * (float) i / 120.0f;
            const float y = p.getCentreY() - std::sin (juce::MathConstants<float>::twoPi * (cycles * (float) i / 120.0f - phase)) * a * p.getHeight() * 0.5f;
            if (i == 0) path.startNewSubPath (x, y); else path.lineTo (x, y);
        }
        const auto c = a > 0.001f ? col::radio : col::greyDim;
        g.setColour (c.withAlpha (0.18f)); g.strokePath (path, juce::PathStrokeType (5.0f));
        g.setColour (c); g.strokePath (path, juce::PathStrokeType (1.6f));
        static const char* names[] = { "FILTER", "PITCH", "AMP", "PAN" };
        g.setColour (col::grey);
        g.setFont (Fonts::value (11.0f));
        g.drawText ("LFO > " + juce::String (names[juce::jlimit (0, 3, (int) target->load())]), 8, 2, getWidth() - 16, 12, juce::Justification::left, false);
    }

private:
    std::atomic<float> *amount, *rate, *target;
    float phase = 0.0f, lastA = -1, lastR = -1; int lastT = -1;
};

//==============================================================================
/** Voices, output level, MIDI activity and overload indicator. Informative, not intrusive. */
class VoiceMonitor : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit VoiceMonitor (PrysmProcessor& p) : proc (p)
    {
        setTooltip ("Voice monitor. Click to reset the overload indicator.");
    }

    void tick()
    {
        auto& m = proc.engine.monitor;
        const int v = m.activeVoices.load();
        const float pl = m.peakL.load(), pr = m.peakR.load();
        const auto midi = m.midiCounter.load();
        bool changed = v != voices || std::abs (pl - peakL) > 0.004f || std::abs (pr - peakR) > 0.004f || midi != lastMidi || m.overload.load() != overload;
        if (midi != lastMidi) { lastMidi = midi; midiGlow = 1.0f; changed = true; }
        if (midiGlow > 0.0f) { midiGlow = juce::jmax (0.0f, midiGlow - 0.12f); changed = true; }
        voices = v; peakL = pl; peakR = pr; overload = m.overload.load();
        maxVoices = (int) proc.apvts.getRawParameterValue (ids::voices)->load();
        if (changed) repaint();
    }

    void mouseDown (const juce::MouseEvent&) override { proc.engine.monitor.overload = false; }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        g.setColour (col::bg1); g.fillRoundedRectangle (b, 6.0f);
        g.setColour (col::edge); g.drawRoundedRectangle (b.reduced (0.5f), 6.0f, 1.0f);

        float x = 14.0f;
        const float cy = b.getCentreY();
        g.setFont (Fonts::title (13.0f, 0.22f));
        g.setColour (col::grey);
        g.drawText ("VOICES", (int) x, 0, 60, getHeight(), juce::Justification::centredLeft, false);
        x += 56.0f;
        g.setColour (col::text);
        g.setFont (Fonts::value (14.0f));
        g.drawText (juce::String (voices).paddedLeft ('0', 2) + " / " + juce::String (maxVoices).paddedLeft ('0', 2), (int) x, 0, 70, getHeight(), juce::Justification::centredLeft, false);
        x += 80.0f;
        for (int i = 0; i < 16; ++i) // voice LEDs
        {
            auto led = juce::Rectangle<float> (7.0f, 12.0f).withCentre ({ x + (float) i * 10.0f + 3.5f, cy });
            const bool avail = i < maxVoices, used = i < voices;
            g.setColour (used ? col::acid : (avail ? col::edgeHi : col::edge.darker (0.3f)));
            g.fillRoundedRectangle (led, 1.5f);
            if (used) { g.setColour (col::acid.withAlpha (0.2f)); g.fillRoundedRectangle (led.expanded (2.5f), 3.0f); }
        }
        x += 16 * 10.0f + 26.0f;

        // output meters
        g.setColour (col::grey);
        g.setFont (Fonts::title (13.0f, 0.22f));
        g.drawText ("OUTPUT", (int) x, 0, 60, getHeight(), juce::Justification::centredLeft, false);
        x += 60.0f;
        drawMeter (g, { x, cy - 8.0f, 190.0f, 6.0f }, peakL);
        drawMeter (g, { x, cy + 2.0f, 190.0f, 6.0f }, peakR);
        x += 200.0f;
        const float pk = juce::jmax (peakL, peakR);
        g.setColour (pk > 1.0f ? col::error : col::text);
        g.setFont (Fonts::value (13.0f));
        g.drawText (pk > 0.0001f ? juce::String (juce::Decibels::gainToDecibels (pk), 1) + " dB" : "-inf dB", (int) x, 0, 74, getHeight(), juce::Justification::centredLeft, false);
        x += 90.0f;

        // MIDI + overload
        auto led = [&] (const juce::String& txt, juce::Colour c, float glow)
        {
            auto d = juce::Rectangle<float> (9.0f, 9.0f).withCentre ({ x + 5.0f, cy });
            g.setColour (c.withAlpha (0.18f * glow)); g.fillEllipse (d.expanded (4.0f));
            g.setColour (glow > 0.01f ? c.withAlpha (0.35f + 0.65f * glow) : col::greyDim); g.fillEllipse (d);
            g.setColour (col::grey); g.setFont (Fonts::title (13.0f, 0.22f));
            g.drawText (txt, (int) x + 16, 0, 90, getHeight(), juce::Justification::centredLeft, false);
            x += 16.0f + juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), txt) + 22.0f;
        };
        led ("MIDI", col::radio, midiGlow);
        led ("OVERLOAD", col::error, overload ? 1.0f : 0.0f);
    }

private:
    static void drawMeter (juce::Graphics& g, juce::Rectangle<float> r, float level)
    {
        g.setColour (col::bg0); g.fillRoundedRectangle (r, 2.0f);
        const float db = juce::Decibels::gainToDecibels (level, -60.0f);
        const float n = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 66.0f);
        auto f = r.withWidth (r.getWidth() * n);
        g.setColour (db > 0.0f ? col::error : (db > -6.0f ? col::warn : col::acid));
        g.fillRoundedRectangle (f, 2.0f);
    }
    PrysmProcessor& proc;
    int voices = 0, maxVoices = 8;
    float peakL = 0, peakR = 0, midiGlow = 0;
    juce::uint32 lastMidi = 0;
    bool overload = false;
};

//==============================================================================
/** On-screen keyboard: auditions notes with the mouse and shows host MIDI + the ROOT key. */
class KeyboardStrip : public juce::Component, public juce::SettableTooltipClient
{
public:
    static constexpr int kLow = 24, kHigh = 96; // C2 .. C8 (MIDI 60 = C5, FL Studio convention)

    explicit KeyboardStrip (PrysmProcessor& p) : proc (p) { setTooltip ("Click to audition notes. The marked key plays the sample at its original pitch."); }

    void tick()
    {
        const auto lo = proc.engine.monitor.heldLo.load(), hi = proc.engine.monitor.heldHi.load();
        const int root = (int) proc.apvts.getRawParameterValue (ids::root)->load();
        if (lo != heldLo || hi != heldHi || root != rootNote) { heldLo = lo; heldHi = hi; rootNote = root; repaint(); }
    }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        g.setColour (col::bg1); g.fillRoundedRectangle (b, 6.0f);
        g.setColour (col::edge); g.drawRoundedRectangle (b.reduced (0.5f), 6.0f, 1.0f);

        // white keys
        for (int n = kLow; n <= kHigh; ++n)
        {
            if (isBlack (n)) continue;
            auto r = keyRect (n);
            const bool down = isHeld (n) || n == mouseNote;
            if (down)
            {
                g.setGradientFill (juce::ColourGradient (col::acid, 0, r.getY(), col::acid.darker (0.7f), 0, r.getBottom(), false));
            }
            else g.setGradientFill (juce::ColourGradient (col::bg2.brighter (0.12f), 0, r.getY(), col::bg2.darker (0.3f), 0, r.getBottom(), false));
            g.fillRoundedRectangle (r.reduced (0.8f, 0.0f), 2.0f);
            g.setColour (col::bg0);
            g.drawVerticalLine ((int) r.getRight(), r.getY(), r.getBottom());
            if (n % 12 == 0)
            {
                g.setColour (down ? col::bg0 : col::greyDim);
                g.setFont (Fonts::value (10.0f));
                g.drawText (noteName (n), r.toNearestInt().withTrimmedTop ((int) r.getHeight() - 14), juce::Justification::centred, false);
            }
        }
        // black keys
        for (int n = kLow; n <= kHigh; ++n)
        {
            if (! isBlack (n)) continue;
            auto r = keyRect (n);
            const bool down = isHeld (n) || n == mouseNote;
            g.setGradientFill (juce::ColourGradient (down ? col::radio : col::bg0.brighter (0.1f), 0, r.getY(), down ? col::radio.darker (0.8f) : col::bg0, 0, r.getBottom(), false));
            g.fillRoundedRectangle (r, 2.0f);
            g.setColour (col::edgeHi.withAlpha (0.7f));
            g.drawRoundedRectangle (r, 2.0f, 0.8f);
        }
        // root key marker
        if (rootNote >= kLow && rootNote <= kHigh)
        {
            auto r = keyRect (rootNote);
            const auto c = juce::Point<float> (r.getCentreX(), isBlack (rootNote) ? r.getBottom() - 8.0f : r.getBottom() - 24.0f);
            juce::Path d; d.addTriangle (c.x, c.y - 5.0f, c.x + 5.0f, c.y, c.x, c.y + 5.0f); d.addTriangle (c.x, c.y - 5.0f, c.x - 5.0f, c.y, c.x, c.y + 5.0f);
            g.setColour (col::acid.withAlpha (0.25f)); g.fillEllipse (juce::Rectangle<float> (16.0f, 16.0f).withCentre (c));
            g.setColour (isBlack (rootNote) ? col::acid : col::bg0); g.fillPath (d);
            g.setColour (col::acid); g.strokePath (d, juce::PathStrokeType (1.0f));
        }
    }

    void mouseDown (const juce::MouseEvent& e) override { press (e.position); }
    void mouseDrag (const juce::MouseEvent& e) override { press (e.position); }
    void mouseUp (const juce::MouseEvent&) override { release(); }

private:
    static bool isBlack (int n) { const int k = n % 12; return k == 1 || k == 3 || k == 6 || k == 8 || k == 10; }
    int whiteIndex (int n) const { int c = 0; for (int i = kLow; i < n; ++i) if (! isBlack (i)) ++c; return c; }
    int numWhite() const { int c = 0; for (int i = kLow; i <= kHigh; ++i) if (! isBlack (i)) ++c; return c; }
    bool isHeld (int n) const { return n < 64 ? ((heldLo >> n) & 1u) != 0 : ((heldHi >> (n - 64)) & 1u) != 0; }

    juce::Rectangle<float> keyRect (int n) const
    {
        auto b = getLocalBounds().toFloat().reduced (3.0f);
        const float ww = b.getWidth() / (float) numWhite();
        if (! isBlack (n)) return { b.getX() + (float) whiteIndex (n) * ww, b.getY(), ww, b.getHeight() };
        const float x = b.getX() + (float) whiteIndex (n) * ww; // black keys sit on the boundary after the previous white key
        return { x - ww * 0.3f, b.getY(), ww * 0.6f, b.getHeight() * 0.6f };
    }
    int noteAt (juce::Point<float> p) const
    {
        for (int n = kLow; n <= kHigh; ++n) if (isBlack (n) && keyRect (n).contains (p)) return n;
        for (int n = kLow; n <= kHigh; ++n) if (! isBlack (n) && keyRect (n).contains (p)) return n;
        return -1;
    }
    void press (juce::Point<float> p)
    {
        const int n = noteAt (p);
        if (n == mouseNote) return;
        release();
        if (n >= 0)
        {
            const float vel = juce::jlimit (0.3f, 1.0f, 0.35f + 0.65f * (p.y / (float) getHeight()));
            proc.pushUiNote (n, vel, true);
            mouseNote = n;
            repaint();
        }
    }
    void release()
    {
        if (mouseNote >= 0) { proc.pushUiNote (mouseNote, 0.0f, false); mouseNote = -1; repaint(); }
    }
    PrysmProcessor& proc;
    juce::uint64 heldLo = 0, heldHi = 0;
    int rootNote = 60, mouseNote = -1;
};

//==============================================================================
/** PRYSM mark: a prism splitting a ray, with an alien eye at its core. */
inline void drawLogo (juce::Graphics& g, juce::Rectangle<float> area)
{
    const auto c = area.getCentre();
    const float R = juce::jmin (area.getWidth(), area.getHeight()) * 0.46f;
    juce::Path hex;
    for (int i = 0; i < 6; ++i)
    {
        const float a = juce::MathConstants<float>::twoPi * (float) i / 6.0f - juce::MathConstants<float>::halfPi;
        const juce::Point<float> p (c.x + std::cos (a) * R, c.y + std::sin (a) * R);
        if (i == 0) hex.startNewSubPath (p); else hex.lineTo (p);
    }
    hex.closeSubPath();
    g.setColour (col::acid.withAlpha (0.12f));
    g.strokePath (hex, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved));
    g.setColour (col::acid);
    g.strokePath (hex, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved));

    // prism facets
    g.setColour (col::acid.withAlpha (0.35f));
    for (int i = 0; i < 6; i += 2)
    {
        const float a = juce::MathConstants<float>::twoPi * (float) i / 6.0f - juce::MathConstants<float>::halfPi;
        g.drawLine (c.x, c.y, c.x + std::cos (a) * R, c.y + std::sin (a) * R, 0.9f);
    }
    // dispersing rays
    const float rx = c.x + R * 0.87f;
    g.setColour (col::text.withAlpha (0.8f));
    g.drawLine (area.getX() - 7.0f, c.y + 2.0f, c.x - R * 0.6f, c.y + 2.0f, 1.2f);
    const juce::Colour rays[3] = { col::acid, col::radio, col::midGreen.brighter (0.6f) };
    for (int i = 0; i < 3; ++i)
        g.setColour (rays[i].withAlpha (0.9f)), g.drawLine (rx, c.y + 3.0f, rx + 9.0f, c.y + 3.0f + ((float) i - 1.0f) * 6.0f, 1.3f);

    // alien eye
    juce::Path eye;
    const float ew = R * 0.62f, eh = R * 0.30f;
    eye.startNewSubPath (c.x - ew, c.y);
    eye.quadraticTo (c.x, c.y - eh * 2.0f, c.x + ew, c.y);
    eye.quadraticTo (c.x, c.y + eh * 2.0f, c.x - ew, c.y);
    eye.closeSubPath();
    g.setColour (col::radio.withAlpha (0.25f)); g.fillPath (eye);
    g.setColour (col::radio); g.strokePath (eye, juce::PathStrokeType (1.3f));
    g.setColour (col::bg0);
    g.fillEllipse (juce::Rectangle<float> (R * 0.16f, R * 0.52f).withCentre (c));
}
} // namespace prysm::ui
