#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "Theme.h"
#include "../PluginProcessor.h"
#include "../Envelope.h"

namespace prysm::ui
{
//==============================================================================
/** Glass panel with a thin neon edge and a plain title. Dimmed and non-interactive while no sample is loaded. */
class Panel : public juce::Component
{
public:
    explicit Panel (juce::String t) : title (std::move (t)) {}

    void setDormant (bool d)
    {
        if (dormant == d) return;
        dormant = d;
        setAlpha (d ? 0.38f : 1.0f);
        setInterceptsMouseClicks (! d, ! d);
    }
    bool isDormant() const noexcept { return dormant; }
    juce::Rectangle<int> contentArea() const { return getLocalBounds().withTrimmedTop (34).reduced (14, 6); }

    void paint (juce::Graphics& g) override { paintFrame (g); }

protected:
    void paintFrame (juce::Graphics& g)
    {
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        g.setGradientFill (juce::ColourGradient (col::bg2.withAlpha (0.78f), 0, 0, col::bg1.withAlpha (0.84f), 0, r.getBottom(), false));
        g.fillRoundedRectangle (r, 12.0f);
        g.setGradientFill (juce::ColourGradient (col::neon.withAlpha (0.55f), r.getX(), 0, col::violet.withAlpha (0.25f), r.getRight(), 0, false));
        g.drawRoundedRectangle (r, 12.0f, 1.0f);
        if (title.isNotEmpty())
        {
            g.setColour (col::text.withAlpha (0.92f));
            g.setFont (Fonts::title (15.0f, 0.3f));
            g.drawText (title, 18, 8, getWidth() / 2, 20, juce::Justification::centredLeft, false);
            g.setColour (col::neon);
            g.fillRoundedRectangle (18.0f, 30.0f, 22.0f, 2.0f, 1.0f);
        }
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
            g.setColour (sel ? col::deepBlue : col::bg2);
            g.fillRoundedRectangle (cell, 3.5f);
            g.setColour (sel ? col::neon.withAlpha (0.9f) : (i == hover ? col::edgeHi.brighter (0.3f) : col::edge));
            g.drawRoundedRectangle (cell, 3.5f, 1.0f);
            if (sel)
            {
                g.setColour (col::neon.withAlpha (0.18f));
                g.fillRoundedRectangle (cell.reduced (1.0f), 3.0f);
            }
            g.setColour (sel ? col::neon : col::grey);
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
/** FL-style envelope editor (Delay, Attack, Hold, Decay, Sustain, Release + curve tension on A/D/R).
    Square handles edit time/level, small round handles bend a segment. Double-click a handle to reset it. */
class EnvelopeGraph : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit EnvelopeGraph (juce::AudioProcessorValueTreeState& s)
        : pDel (s.getParameter (ids::delay)), pA (s.getParameter (ids::attack)), pH (s.getParameter (ids::hold)),
          pD (s.getParameter (ids::decay)), pS (s.getParameter (ids::sustain)), pR (s.getParameter (ids::release)),
          pTA (s.getParameter (ids::tensionA)), pTD (s.getParameter (ids::tensionD)), pTR (s.getParameter (ids::tensionR))
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        setTooltip ("Drag the square points to set the times and the sustain level. Drag the round points to bend a curve.");
        all = { pDel, pA, pH, pD, pS, pR, pTA, pTD, pTR };
    }

    /** Called from the editor timer: repaints only when a value changed. */
    void tick()
    {
        bool changed = false;
        for (size_t i = 0; i < all.size(); ++i)
        {
            const float v = all[i]->getValue();
            if (v != last[i]) { last[i] = v; changed = true; }
        }
        if (changed) repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        g.setColour (col::bg0.withAlpha (0.92f));
        g.fillRoundedRectangle (bounds, 8.0f);
        g.setColour (col::edge);
        g.drawRoundedRectangle (bounds.reduced (0.5f), 8.0f, 1.0f);

        auto p = plot();
        g.setColour (col::edge.withAlpha (0.55f));
        for (int i = 0; i <= 10; ++i) g.drawVerticalLine ((int) (p.getX() + p.getWidth() * (float) i / 10.0f), p.getY(), p.getBottom());
        for (int i = 0; i <= 4; ++i) g.drawHorizontalLine ((int) (p.getY() + p.getHeight() * (float) i / 4.0f), p.getX(), p.getRight());

        const auto q = layout();
        juce::Path line;
        line.startNewSubPath (q.p0);
        line.lineTo (q.delayEnd);
        addCurve (line, q.delayEnd, q.attackEnd, pTA->convertFrom0to1 (pTA->getValue()));
        line.lineTo (q.holdEnd);
        addCurve (line, q.holdEnd, q.decayEnd, pTD->convertFrom0to1 (pTD->getValue()));
        line.lineTo (q.sustainEnd);
        addCurve (line, q.sustainEnd, q.releaseEnd, pTR->convertFrom0to1 (pTR->getValue()));

        juce::Path fill (line);
        fill.lineTo (q.releaseEnd.x, p.getBottom());
        fill.lineTo (q.p0.x, p.getBottom());
        fill.closeSubPath();
        g.setGradientFill (juce::ColourGradient (col::neon.withAlpha (0.38f), 0, p.getY(), col::violet.withAlpha (0.03f), 0, p.getBottom(), false));
        g.fillPath (fill);
        g.setColour (col::neon.withAlpha (0.22f));
        g.strokePath (line, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved));
        g.setColour (col::glow);
        g.strokePath (line, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved));

        auto square = [&] (juce::Point<float> pt, bool active)
        {
            auto r = juce::Rectangle<float> (10.0f, 10.0f).withCentre (pt);
            if (active) { g.setColour (col::glow.withAlpha (0.28f)); g.fillEllipse (r.expanded (9.0f)); }
            g.setColour (col::bg0); g.fillRoundedRectangle (r, 2.0f);
            g.setColour (active ? col::text : col::glow); g.drawRoundedRectangle (r, 2.0f, 1.8f);
        };
        auto round = [&] (juce::Point<float> pt, bool active)
        {
            auto r = juce::Rectangle<float> (9.0f, 9.0f).withCentre (pt);
            if (active) { g.setColour (col::violet.withAlpha (0.35f)); g.fillEllipse (r.expanded (9.0f)); }
            g.setColour (col::bg0); g.fillEllipse (r);
            g.setColour (active ? col::text : col::violet.brighter (0.3f)); g.drawEllipse (r, 1.8f);
        };
        square (q.delayEnd, drag == hDelay || hover == hDelay);
        square (q.attackEnd, drag == hAttack || hover == hAttack);
        square (q.holdEnd, drag == hHold || hover == hHold);
        square (q.decayEnd, drag == hDecay || hover == hDecay);
        square (q.releaseEnd, drag == hRelease || hover == hRelease);
        round (tensionHandle (q.delayEnd, q.attackEnd, pTA), drag == hTA || hover == hTA);
        round (tensionHandle (q.holdEnd, q.decayEnd, pTD), drag == hTD || hover == hTD);
        round (tensionHandle (q.sustainEnd, q.releaseEnd, pTR), drag == hTR || hover == hTR);

        g.setFont (Fonts::value (11.0f));
        g.setColour (col::grey);
        if (hover != 0 || drag != 0)
        {
            const int h = drag != 0 ? drag : hover;
            juce::RangedAudioParameter* pr = h == hDelay ? pDel : h == hAttack ? pA : h == hHold ? pH : h == hDecay ? pD : h == hRelease ? pR
                                           : h == hTA ? pTA : h == hTD ? pTD : pTR;
            juce::String name = h == hDelay ? "DELAY" : h == hAttack ? "ATT" : h == hHold ? "HOLD" : h == hDecay ? "DEC" : h == hRelease ? "REL"
                              : h == hTA ? "ATT TENSION" : h == hTD ? "DEC TENSION" : "REL TENSION";
            juce::String txt = name + "  " + pr->getCurrentValueAsText();
            if (h == hDecay) txt += "   SUS " + pS->getCurrentValueAsText();
            g.drawText (txt, 12, 6, getWidth() - 24, 14, juce::Justification::left, false);
        }
    }

    void mouseMove (const juce::MouseEvent& e) override { const int h = hit (e.position); if (h != hover) { hover = h; repaint(); } }
    void mouseExit (const juce::MouseEvent&) override { hover = 0; repaint(); }
    void mouseDown (const juce::MouseEvent& e) override
    {
        drag = hit (e.position);
        for (auto* prm : paramsOf (drag)) prm->beginChangeGesture();
        if (drag != 0) dragStart = layout();
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (drag == 0) return;
        auto p = plot();
        const auto& q = dragStart;
        const float x = e.position.x;
        auto norm = [&] (float from) { return juce::jlimit (0.0f, 1.0f, (x - from - kMinW) / segMax()); };
        switch (drag)
        {
            case hDelay:   pDel->setValueNotifyingHost (norm (q.p0.x)); break;
            case hAttack:  pA->setValueNotifyingHost (norm (q.delayEnd.x)); break;
            case hHold:    pH->setValueNotifyingHost (norm (q.attackEnd.x)); break;
            case hDecay:
                pD->setValueNotifyingHost (norm (q.holdEnd.x));
                pS->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, (p.getBottom() - e.position.y) / p.getHeight()));
                break;
            case hRelease: pR->setValueNotifyingHost (norm (q.sustainEnd.x)); break;
            case hTA: setTension (pTA, q.delayEnd, q.attackEnd, e.position.y); break;
            case hTD: setTension (pTD, q.holdEnd, q.decayEnd, e.position.y); break;
            case hTR: setTension (pTR, q.sustainEnd, q.releaseEnd, e.position.y); break;
            default: break;
        }
        repaint();
    }
    void mouseUp (const juce::MouseEvent&) override
    {
        for (auto* prm : paramsOf (drag)) prm->endChangeGesture();
        drag = 0;
        repaint();
    }
    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        const int h = hit (e.position);
        for (auto* prm : paramsOf (h)) { prm->beginChangeGesture(); prm->setValueNotifyingHost (prm->getDefaultValue()); prm->endChangeGesture(); }
    }

private:
    enum { hDelay = 1, hAttack, hHold, hDecay, hRelease, hTA, hTD, hTR };
    static constexpr float kMinW = 12.0f, kSustainW = 0.10f;
    struct Layout { juce::Point<float> p0, delayEnd, attackEnd, holdEnd, decayEnd, sustainEnd, releaseEnd; };

    juce::Rectangle<float> plot() const { return getLocalBounds().toFloat().reduced (16.0f, 26.0f); }
    float segMax() const { return (plot().getWidth() * (1.0f - kSustainW) - 5.0f * kMinW) / 5.0f; }
    static float val (juce::RangedAudioParameter* p) { return p->getValue(); }

    Layout layout() const
    {
        auto p = plot();
        Layout q;
        const float sm = segMax();
        q.p0 = { p.getX(), p.getBottom() };
        q.delayEnd  = { q.p0.x + kMinW + val (pDel) * sm, p.getBottom() };
        q.attackEnd = { q.delayEnd.x + kMinW + val (pA) * sm, p.getY() };
        q.holdEnd   = { q.attackEnd.x + kMinW + val (pH) * sm, p.getY() };
        q.decayEnd  = { q.holdEnd.x + kMinW + val (pD) * sm, p.getBottom() - val (pS) * p.getHeight() };
        q.sustainEnd = { q.decayEnd.x + p.getWidth() * kSustainW, q.decayEnd.y };
        q.releaseEnd = { q.sustainEnd.x + kMinW + val (pR) * sm, p.getBottom() };
        return q;
    }

    // Draws the segment a->b with the same shape function the audio engine uses.
    static void addCurve (juce::Path& path, juce::Point<float> a, juce::Point<float> b, float tension)
    {
        constexpr int steps = 28;
        for (int i = 1; i <= steps; ++i)
        {
            const float x = (float) i / (float) steps;
            const float f = envCurve (x, tension);
            path.lineTo (a.x + (b.x - a.x) * x, a.y + (b.y - a.y) * f);
        }
    }
    juce::Point<float> tensionHandle (juce::Point<float> a, juce::Point<float> b, juce::RangedAudioParameter* t) const
    {
        const float f = envCurve (0.5f, t->convertFrom0to1 (t->getValue()));
        return { a.x + (b.x - a.x) * 0.5f, a.y + (b.y - a.y) * f };
    }
    // Finds the tension for which the curve passes through the mouse height at mid-segment.
    void setTension (juce::RangedAudioParameter* t, juce::Point<float> a, juce::Point<float> b, float mouseY)
    {
        if (std::abs (b.y - a.y) < 4.0f) return;
        const float want = juce::jlimit (0.02f, 0.98f, (mouseY - a.y) / (b.y - a.y));
        float lo = -1.0f, hi = 1.0f;
        for (int i = 0; i < 24; ++i)
        {
            const float mid = 0.5f * (lo + hi);
            if (envCurve (0.5f, mid) > want) lo = mid; else hi = mid; // envCurve(0.5, t) decreases with t
        }
        t->setValueNotifyingHost (t->convertTo0to1 (0.5f * (lo + hi)));
    }
    std::vector<juce::RangedAudioParameter*> paramsOf (int h) const
    {
        switch (h)
        {
            case hDelay: return { pDel };    case hAttack: return { pA };   case hHold: return { pH };
            case hDecay: return { pD, pS };  case hRelease: return { pR };
            case hTA: return { pTA };        case hTD: return { pTD };      case hTR: return { pTR };
            default: return {};
        }
    }
    int hit (juce::Point<float> pos) const
    {
        const auto q = layout();
        const float r = 11.0f;
        struct C { int id; juce::Point<float> pt; };
        const C c[] = { { hTA, tensionHandle (q.delayEnd, q.attackEnd, pTA) }, { hTD, tensionHandle (q.holdEnd, q.decayEnd, pTD) },
                        { hTR, tensionHandle (q.sustainEnd, q.releaseEnd, pTR) },
                        { hDecay, q.decayEnd }, { hRelease, q.releaseEnd }, { hHold, q.holdEnd }, { hAttack, q.attackEnd }, { hDelay, q.delayEnd } };
        int best = 0; float bd = r;
        for (auto& k : c) { const float d = pos.getDistanceFrom (k.pt); if (d < bd) { bd = d; best = k.id; } }
        return best;
    }

    juce::RangedAudioParameter *pDel, *pA, *pH, *pD, *pS, *pR, *pTA, *pTD, *pTR;
    std::array<juce::RangedAudioParameter*, 9> all;
    float last[9] = { -9, -9, -9, -9, -9, -9, -9, -9, -9 };
    Layout dragStart;
    int drag = 0, hover = 0;
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
                g.setGradientFill (juce::ColourGradient (col::neon, 0, r.getY(), col::neon.darker (0.7f), 0, r.getBottom(), false));
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
            g.setGradientFill (juce::ColourGradient (down ? col::glow : col::bg0.brighter (0.1f), 0, r.getY(), down ? col::glow.darker (0.8f) : col::bg0, 0, r.getBottom(), false));
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
            g.setColour (col::neon.withAlpha (0.25f)); g.fillEllipse (juce::Rectangle<float> (16.0f, 16.0f).withCentre (c));
            g.setColour (isBlack (rootNote) ? col::neon : col::bg0); g.fillPath (d);
            g.setColour (col::neon); g.strokePath (d, juce::PathStrokeType (1.0f));
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


} // namespace prysm::ui
