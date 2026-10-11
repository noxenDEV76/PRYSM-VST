#pragma once
#include "Widgets.h"

namespace prysm::ui
{
/** Main sample view: drag-and-drop zone, waveform, start/end markers and playhead. */
class SampleCore : public Panel
{
public:
    explicit SampleCore (PrysmProcessor& p)
        : Panel (""), proc (p),
          pStart (p.apvts.getParameter (ids::start)), pEnd (p.apvts.getParameter (ids::end))
    {
        loadButton.setButtonText ("LOAD SAMPLE");
        loadButton.getProperties().set ("accent", true);
        loadButton.setTooltip ("Browse for a WAV, AIFF, FLAC or OGG file");
        loadButton.onClick = [this] { if (onLoadClicked) onLoadClicked(); };
        addAndMakeVisible (loadButton);

        clearButton.setButtonText ("CLEAR");
        clearButton.setTooltip ("Remove the current sample");
        clearButton.onClick = [this] { proc.clearSample(); };
        addAndMakeVisible (clearButton);

        setMouseCursor (juce::MouseCursor::NormalCursor);
    }

    std::function<void()> onLoadClicked;

    void setAnimationsEnabled (bool e) { animate = e; repaint(); }
    void setDragHover (bool hover, bool supported) { dragHover = hover; dragSupported = supported; repaint(); }

    /** Called from the editor timer. `dt` in seconds. */
    void tick (float dt)
    {
        auto sample = proc.getUiSample();
        const auto status = proc.getStatus();
        const auto text = proc.getStatusText();
        bool changed = false;
        if (sample != currentSample) { currentSample = sample; rebuildWaveform(); changed = true; }
        if (status != lastStatus || text != lastText) { lastStatus = status; lastText = text; changed = true; errorShownAt = text.isNotEmpty() ? juce::Time::getMillisecondCounter() : 0; }
        if (errorShownAt != 0 && juce::Time::getMillisecondCounter() - errorShownAt > 7000 && status == PrysmProcessor::SampleStatus::Loaded) { errorShownAt = 0; changed = true; }

        const float ps = pStart->getValue(), pe = pEnd->getValue();
        if (ps != startN || pe != endN) { startN = ps; endN = pe; changed = true; }

        const float ph = proc.engine.monitor.playhead.load();
        if (ph != playhead) { playhead = ph; changed = true; }
        const float act = proc.engine.monitor.activity.load();
        if (std::abs (act - activity) > 0.003f) { activity = act; if (animate) changed = true; }

        if (animate)
        {
            time += dt;
            changed = true;
        }
        if (changed) repaint (waveArea().toNearestInt().expanded (4));
    }

    void resized() override
    {
        auto b = getLocalBounds();
        clearButton.setBounds (b.getRight() - 70, 12, 58, 24);
        loadButton.setBounds (b.getRight() - 70 - 8 - 130, 12, 130, 24);
        rebuildWaveform();
    }

    void paint (juce::Graphics& g) override
    {
        paintFrame (g);
        const auto status = lastStatus;
        auto wa = waveArea();

        // ---- info row
        auto info = getLocalBounds().toFloat().withTrimmedTop (12.0f).withHeight (24.0f).reduced (18.0f, 0.0f);
        if (currentSample != nullptr)
        {
            g.setColour (col::text);
            g.setFont (Fonts::title (19.0f, 0.06f));
            g.drawText (currentSample->name, info.withWidth (info.getWidth() * 0.5f).toNearestInt(), juce::Justification::centredLeft, true);
            g.setFont (Fonts::value (13.0f));
            const auto ch = currentSample->numChannels() == 1 ? "MONO" : "STEREO";
            const juce::String stats = "DUR " + juce::String (currentSample->durationSeconds(), 3) + " s   |   "
                                     + juce::String (currentSample->sampleRate / 1000.0, 1) + " kHz   |   " + ch;
            g.setColour (col::grey);
            g.drawText (stats, info.withTrimmedRight (216.0f).toNearestInt(), juce::Justification::centredRight, false);
        }
        else
        {
            g.setColour (col::greyDim);
            g.setFont (Fonts::title (19.0f, 0.06f));
            g.drawText (status == PrysmProcessor::SampleStatus::Missing ? proc.getMissingFileName() : juce::String ("NO SAMPLE LOADED"),
                        info.toNearestInt(), juce::Justification::centredLeft, true);
        }

        // ---- wave area background + core
        g.saveState();
        juce::Path clip; clip.addRoundedRectangle (wa, 5.0f);
        g.reduceClipRegion (clip);
        g.setGradientFill (juce::ColourGradient (col::bg0.brighter (0.02f), wa.getCentreX(), wa.getCentreY(), col::bg0.darker (0.2f), wa.getX(), wa.getY(), true));
        g.fillRect (wa);
        paintGlow (g, wa);
        paintGrid (g, wa);
        if (currentSample != nullptr) paintWave (g, wa);
        g.restoreState();
        g.setColour (col::edge); g.drawRoundedRectangle (wa, 5.0f, 1.0f);

        // ---- state overlays
        if (currentSample == nullptr) paintEmpty (g, wa, status);
        if (status == PrysmProcessor::SampleStatus::Loading) paintLoading (g, wa);
        if (lastText.isNotEmpty() && status != PrysmProcessor::SampleStatus::Missing)
        {
            auto bar = wa.removeFromBottom (24.0f).reduced (1.0f);
            g.setColour (col::bg0.withAlpha (0.88f)); g.fillRect (bar);
            g.setColour (col::error); g.setFont (Fonts::title (14.5f, 0.2f));
            g.drawText ("! " + lastText, bar.toNearestInt().reduced (10, 0), juce::Justification::centredLeft, false);
        }
        if (dragHover) paintDropOverlay (g, waveArea());
    }

    //==========================================================================
    void mouseMove (const juce::MouseEvent& e) override
    {
        setMouseCursor (markerAt (e.position) != 0 ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::NormalCursor);
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        dragging = currentSample != nullptr ? markerAt (e.position) : 0;
        if (dragging == 1) pStart->beginChangeGesture();
        if (dragging == 2) pEnd->beginChangeGesture();
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragging == 0) return;
        auto wa = waveArea();
        const float n = juce::jlimit (0.0f, 1.0f, (e.position.x - wa.getX()) / wa.getWidth());
        if (dragging == 1) pStart->setValueNotifyingHost (juce::jmin (n, pEnd->getValue() - 0.002f));
        if (dragging == 2) pEnd->setValueNotifyingHost (juce::jmax (n, pStart->getValue() + 0.002f));
    }
    void mouseUp (const juce::MouseEvent&) override
    {
        if (dragging == 1) pStart->endChangeGesture();
        if (dragging == 2) pEnd->endChangeGesture();
        dragging = 0;
    }
    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        const int m = markerAt (e.position);
        auto reset = [] (juce::RangedAudioParameter* x) { x->beginChangeGesture(); x->setValueNotifyingHost (x->getDefaultValue()); x->endChangeGesture(); };
        if (m == 1) reset (pStart);
        if (m == 2) reset (pEnd);
    }

    juce::TextButton loadButton, clearButton;

private:
    juce::Rectangle<float> waveArea() const
    {
        return getLocalBounds().toFloat().withTrimmedTop (46.0f).reduced (14.0f, 0.0f).withTrimmedBottom (14.0f);
    }
    float xFor (float n) const { auto wa = waveArea(); return wa.getX() + n * wa.getWidth(); }
    int markerAt (juce::Point<float> p) const
    {
        auto wa = waveArea();
        if (! wa.expanded (6.0f).contains (p)) return 0;
        const float ds = std::abs (p.x - xFor (startN)), de = std::abs (p.x - xFor (endN));
        if (ds < 9.0f && ds <= de) return 1;
        if (de < 9.0f) return 2;
        return 0;
    }

    void rebuildWaveform()
    {
        wavePath.clear();
        if (currentSample == nullptr) return;
        auto wa = waveArea();
        const auto& mn = currentSample->peakMin; const auto& mx = currentSample->peakMax;
        const int bins = (int) mx.size();
        if (bins < 2) return;
        // normalise for readability (view only; audio is untouched)
        float peak = 0.0001f;
        for (int i = 0; i < bins; ++i) peak = juce::jmax (peak, std::abs (mx[(size_t) i]), std::abs (mn[(size_t) i]));
        const float scale = 0.92f / peak;
        const float cy = wa.getCentreY() - 6.0f, half = (wa.getHeight() - 28.0f) * 0.5f;
        wavePath.startNewSubPath (wa.getX(), cy);
        for (int i = 0; i < bins; ++i)
            wavePath.lineTo (wa.getX() + wa.getWidth() * (float) i / (float) (bins - 1), cy - mx[(size_t) i] * scale * half);
        for (int i = bins - 1; i >= 0; --i)
            wavePath.lineTo (wa.getX() + wa.getWidth() * (float) i / (float) (bins - 1), cy - mn[(size_t) i] * scale * half);
        wavePath.closeSubPath();
        waveCentreY = cy;
    }

    void paintGlow (juce::Graphics& g, juce::Rectangle<float> wa)
    {
        const auto c = wa.getCentre();
        const float act = animate ? juce::jlimit (0.0f, 1.0f, activity) : 0.0f;
        g.setGradientFill (juce::ColourGradient (col::neon.withAlpha (0.10f + 0.20f * act), c.x, c.y, juce::Colours::transparentBlack, wa.getX(), c.y, true));
        g.fillRect (wa);
    }

    void paintGrid (juce::Graphics& g, juce::Rectangle<float> wa)
    {
        g.setColour (col::edge.withAlpha (0.55f));
        const float cy = currentSample ? waveCentreY : wa.getCentreY() - 6.0f;
        g.drawHorizontalLine ((int) cy, wa.getX(), wa.getRight());
        for (int i = 1; i < 8; ++i) g.drawVerticalLine ((int) (wa.getX() + wa.getWidth() * (float) i / 8.0f), wa.getY(), wa.getBottom() - 16.0f);

        // time ruler
        if (currentSample != nullptr)
        {
            const double dur = currentSample->durationSeconds();
            static const double steps[] = { 0.005, 0.01, 0.02, 0.05, 0.1, 0.2, 0.5, 1.0, 2.0, 5.0, 10.0, 30.0, 60.0 };
            double step = steps[12];
            for (double s : steps) if (dur / s <= 10.0) { step = s; break; }
            g.setFont (Fonts::value (10.0f));
            for (double t = 0.0; t <= dur; t += step)
            {
                const float x = wa.getX() + (float) (t / dur) * wa.getWidth();
                g.setColour (col::greyDim);
                g.drawVerticalLine ((int) x, wa.getBottom() - 16.0f, wa.getBottom() - 11.0f);
                g.setColour (col::grey.withAlpha (0.8f));
                g.drawText (juce::String (t, step < 0.1 ? 3 : (step < 1 ? 2 : 1)) + "s", (int) x + 3, (int) wa.getBottom() - 14, 44, 12, juce::Justification::left, false);
            }
        }
    }

    void paintWave (juce::Graphics& g, juce::Rectangle<float> wa)
    {
        g.setGradientFill (juce::ColourGradient (col::glow.withAlpha (0.70f), 0, wa.getY(), col::violet.withAlpha (0.40f), 0, wa.getBottom(), false));
        g.fillPath (wavePath);
        g.setColour (col::neon.withAlpha (0.9f));
        g.strokePath (wavePath, juce::PathStrokeType (0.8f));

        const float xs = xFor (startN), xe = xFor (endN);
        g.setColour (col::bg0.withAlpha (0.66f));
        g.fillRect (juce::Rectangle<float> (wa.getX(), wa.getY(), juce::jmax (0.0f, xs - wa.getX()), wa.getHeight()));
        g.fillRect (juce::Rectangle<float> (xe, wa.getY(), juce::jmax (0.0f, wa.getRight() - xe), wa.getHeight()));

        auto marker = [&] (float x, bool isStart)
        {
            drawGlowLine (g, { x, wa.getY(), x, wa.getBottom() }, col::neon, 1.2f, 0.10f);
            juce::Path tri;
            if (isStart) tri.addTriangle (x, wa.getY(), x + 11.0f, wa.getY(), x, wa.getY() + 11.0f);
            else         tri.addTriangle (x, wa.getY(), x - 11.0f, wa.getY(), x, wa.getY() + 11.0f);
            g.setColour (col::neon); g.fillPath (tri);
            g.setColour (col::text); g.setFont (Fonts::value (10.0f));
            const juce::String t = isStart ? "START" : "END";
            g.drawText (t, (int) (isStart ? x + 13.0f : x - 40.0f), (int) wa.getY() + 1, 30, 12, isStart ? juce::Justification::left : juce::Justification::right, false);
        };
        marker (xs, true);
        marker (xe, false);

        if (playhead >= 0.0f)
        {
            const float x = xFor (playhead);
            drawGlowLine (g, { x, wa.getY(), x, wa.getBottom() - 16.0f }, col::glow, 1.4f, 0.22f);
            g.setColour (col::glow);
            g.fillEllipse (x - 3.5f, waveCentreY - 3.5f, 7.0f, 7.0f);
        }
    }

    void paintEmpty (juce::Graphics& g, juce::Rectangle<float> wa, PrysmProcessor::SampleStatus status)
    {
        if (status == PrysmProcessor::SampleStatus::Loading) return;
        const bool missing = status == PrysmProcessor::SampleStatus::Missing;
        auto zone = wa.reduced (28.0f, 22.0f);
        const float pulse = animate ? 0.55f + 0.45f * std::sin (time * 2.0f) : 0.8f;
        const auto accent = missing ? col::warn : col::neon;

        juce::Path dash; dash.addRoundedRectangle (zone, 8.0f);
        juce::Path stroked;
        const float dashes[] = { 7.0f, 6.0f };
        juce::PathStrokeType (1.4f).createDashedStroke (stroked, dash, dashes, 2);
        g.setColour (accent.withAlpha (0.25f + 0.35f * pulse));
        g.fillPath (stroked);
        drawBrackets (g, zone.expanded (3.0f), 12.0f, accent.withAlpha (0.5f + 0.4f * pulse), 1.6f);

        auto textArea = zone.reduced (10.0f);
        if (missing)
        {
            g.setColour (col::warn);
            g.setFont (Fonts::title (30.0f, 0.3f));
            g.drawText ("SAMPLE NOT FOUND", textArea.withTrimmedBottom (textArea.getHeight() * 0.35f).toNearestInt(), juce::Justification::centredBottom, false);
            g.setColour (col::text); g.setFont (Fonts::value (14.0f));
            g.drawText (proc.getMissingFileName(), textArea.withTrimmedTop (textArea.getHeight() * 0.62f).withHeight (20.0f).toNearestInt(), juce::Justification::centred, true);
            g.setColour (col::grey); g.setFont (Fonts::title (15.0f, 0.18f));
            g.drawText ("DROP THE FILE HERE OR PRESS LOCATE SAMPLE", textArea.withTrimmedTop (textArea.getHeight() * 0.62f + 24.0f).withHeight (20.0f).toNearestInt(), juce::Justification::centred, false);
        }
        else
        {
            g.setColour (col::text);
            g.setFont (Fonts::title (32.0f, 0.34f));
            g.drawText ("DROP A SAMPLE TO BEGIN", textArea.withTrimmedBottom (textArea.getHeight() * 0.40f).toNearestInt(), juce::Justification::centredBottom, false);
            g.setColour (col::grey); g.setFont (Fonts::title (15.0f, 0.3f));
            g.drawText ("WAV   .   AIFF   .   FLAC   .   OGG", textArea.withTrimmedTop (textArea.getHeight() * 0.66f).withHeight (22.0f).toNearestInt(), juce::Justification::centred, false);
        }
    }

    void paintLoading (juce::Graphics& g, juce::Rectangle<float> wa)
    {
        g.setColour (col::bg0.withAlpha (0.5f)); g.fillRoundedRectangle (wa, 5.0f);
        const auto c = wa.getCentre();
        juce::Path arc; arc.addCentredArc (c.x, c.y - 8.0f, 22.0f, 22.0f, 0.0f, time * 4.0f, time * 4.0f + 4.2f, true);
        g.setColour (col::neon); g.strokePath (arc, juce::PathStrokeType (3.0f));
        g.setColour (col::text); g.setFont (Fonts::title (17.0f, 0.3f));
        g.drawText ("LOADING", wa.withTrimmedTop (wa.getHeight() * 0.5f + 20.0f).withHeight (24.0f).toNearestInt(), juce::Justification::centred, false);
    }

    void paintDropOverlay (juce::Graphics& g, juce::Rectangle<float> wa)
    {
        const auto c = dragSupported ? col::glow : col::error;
        g.setColour (c.withAlpha (0.12f)); g.fillRoundedRectangle (wa, 5.0f);
        g.setColour (c); g.drawRoundedRectangle (wa.reduced (1.0f), 5.0f, 2.0f);
        drawBrackets (g, wa.reduced (8.0f), 18.0f, c, 2.0f);
        g.setFont (Fonts::title (30.0f, 0.34f));
        g.drawText (dragSupported ? "RELEASE TO LOAD" : "UNSUPPORTED FILE TYPE", wa.toNearestInt(), juce::Justification::centred, false);
    }

    PrysmProcessor& proc;
    juce::RangedAudioParameter *pStart, *pEnd;
    prysm::SamplePtr currentSample;
    juce::Path wavePath;
    float waveCentreY = 0.0f, startN = 0.0f, endN = 1.0f, playhead = -1.0f, activity = 0.0f, time = 0.0f;
    bool animate = true, dragHover = false, dragSupported = true;
    int dragging = 0;
    PrysmProcessor::SampleStatus lastStatus = PrysmProcessor::SampleStatus::Empty;
    juce::String lastText;
    juce::uint32 errorShownAt = 0;
};
} // namespace prysm::ui
