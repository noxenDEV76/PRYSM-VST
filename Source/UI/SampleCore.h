#pragma once
#include "Widgets.h"

namespace prysm::ui
{
/** Main sample view: drag-and-drop zone, waveform, start/end markers, playhead and the animated alien "core". */
class SampleCore : public Panel
{
public:
    explicit SampleCore (PrysmProcessor& p)
        : Panel ("SAMPLE CORE"), proc (p),
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

        juce::Random rng (20241011);
        for (auto& pt : particles)
        {
            pt.ang = rng.nextFloat() * juce::MathConstants<float>::twoPi;
            pt.rad = 0.18f + 0.82f * std::sqrt (rng.nextFloat());
            pt.speed = (rng.nextFloat() - 0.5f) * 0.35f;
            pt.size = 0.9f + rng.nextFloat() * 1.9f;
            pt.phase = rng.nextFloat() * juce::MathConstants<float>::twoPi;
        }
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
        clearButton.setBounds (b.getRight() - 62, 5, 52, 20);
        loadButton.setBounds (b.getRight() - 62 - 8 - 110, 5, 110, 20);
        rebuildWaveform();
    }

    void paint (juce::Graphics& g) override
    {
        paintFrame (g);
        const auto status = lastStatus;
        auto wa = waveArea();

        // ---- info row
        auto info = getLocalBounds().toFloat().withTrimmedTop (34.0f).withHeight (24.0f).reduced (14.0f, 0.0f);
        if (currentSample != nullptr)
        {
            g.setColour (col::text);
            g.setFont (Fonts::title (19.0f, 0.06f));
            g.drawText (currentSample->name, info.withWidth (info.getWidth() * 0.55f).toNearestInt(), juce::Justification::centredLeft, true);
            g.setFont (Fonts::value (13.0f));
            const auto ch = currentSample->numChannels() == 1 ? "MONO" : "STEREO";
            const juce::String stats = "DUR " + juce::String (currentSample->durationSeconds(), 3) + " s   |   "
                                     + juce::String (currentSample->sampleRate / 1000.0, 1) + " kHz   |   " + ch;
            g.setColour (col::grey);
            g.drawText (stats, info.toNearestInt(), juce::Justification::centredRight, false);
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
        paintCore (g, wa);
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
    struct Particle { float ang, rad, speed, size, phase; };

    juce::Rectangle<float> waveArea() const
    {
        return getLocalBounds().toFloat().withTrimmedTop (62.0f).reduced (12.0f, 0.0f).withTrimmedBottom (12.0f);
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

    void paintCore (juce::Graphics& g, juce::Rectangle<float> wa)
    {
        const auto c = wa.getCentre().translated (0.0f, -6.0f);
        const float act = animate ? juce::jlimit (0.0f, 1.0f, activity) : 0.0f;
        const float R = wa.getHeight() * 0.62f;

        // nucleus glow
        g.setGradientFill (juce::ColourGradient (col::radio.withAlpha (0.10f + 0.16f * act), c.x, c.y, juce::Colours::transparentBlack, c.x + R * 1.6f, c.y, true));
        g.fillEllipse (c.x - R * 1.6f, c.y - R, R * 3.2f, R * 2.0f);
        g.setGradientFill (juce::ColourGradient (col::acid.withAlpha (0.10f + 0.22f * act), c.x, c.y, juce::Colours::transparentBlack, c.x + R * 0.5f, c.y, true));
        g.fillEllipse (c.x - R * 0.5f, c.y - R * 0.5f, R, R);

        // organic membranes
        for (int k = 0; k < 3; ++k)
        {
            juce::Path ring;
            const float base = R * (0.42f + 0.2f * (float) k);
            const int N = 96;
            for (int i = 0; i <= N; ++i)
            {
                const float th = juce::MathConstants<float>::twoPi * (float) i / (float) N;
                const float wob = 1.0f + 0.05f * std::sin (3.0f * th + time * 0.6f + (float) k)
                                       + 0.035f * std::sin (5.0f * th - time * 0.9f + 1.7f * (float) k)
                                       + act * 0.07f * std::sin (7.0f * th + time * 3.0f);
                const juce::Point<float> p (c.x + std::cos (th) * base * wob * 1.9f, c.y + std::sin (th) * base * wob * 0.95f);
                if (i == 0) ring.startNewSubPath (p); else ring.lineTo (p);
            }
            ring.closeSubPath();
            g.setColour ((k == 1 ? col::acid : col::radio).withAlpha (0.07f + 0.05f * (float) (2 - k) + 0.10f * act));
            g.strokePath (ring, juce::PathStrokeType (1.1f));
        }

        // particles
        if (animate)
        {
            for (auto& pt : particles)
            {
                const float a = pt.ang + time * pt.speed * (1.0f + act * 2.5f);
                const float r = pt.rad * (1.0f + 0.05f * std::sin (time * 0.5f + pt.phase)) + act * 0.05f * std::sin (pt.phase + time * 6.0f);
                const float px = c.x + std::cos (a) * r * wa.getWidth() * 0.5f * 0.94f;
                const float py = c.y + std::sin (a) * r * wa.getHeight() * 0.5f * 0.8f;
                const float tw = 0.5f + 0.5f * std::sin (time * 1.3f + pt.phase);
                const float alpha = 0.10f + 0.22f * tw + 0.40f * act * (1.0f - pt.rad * 0.5f);
                g.setColour (col::radio.withAlpha (alpha * 0.35f));
                g.fillEllipse (px - pt.size * 1.8f, py - pt.size * 1.8f, pt.size * 3.6f, pt.size * 3.6f);
                g.setColour ((pt.size > 2.2f ? col::acid : col::radio).withAlpha (juce::jmin (0.95f, alpha)));
                g.fillEllipse (px - pt.size * 0.5f, py - pt.size * 0.5f, pt.size, pt.size);
            }
        }
        else // static core when animations are reduced
        {
            g.setColour (col::radio.withAlpha (0.5f));
            g.fillEllipse (c.x - 2.5f, c.y - 2.5f, 5.0f, 5.0f);
        }
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
        g.setGradientFill (juce::ColourGradient (col::acid.withAlpha (0.55f), 0, wa.getY(), col::radio.withAlpha (0.35f), 0, wa.getBottom(), false));
        g.fillPath (wavePath);
        g.setColour (col::acid.withAlpha (0.9f));
        g.strokePath (wavePath, juce::PathStrokeType (0.8f));

        const float xs = xFor (startN), xe = xFor (endN);
        g.setColour (col::bg0.withAlpha (0.62f));
        g.fillRect (juce::Rectangle<float> (wa.getX(), wa.getY(), juce::jmax (0.0f, xs - wa.getX()), wa.getHeight()));
        g.fillRect (juce::Rectangle<float> (xe, wa.getY(), juce::jmax (0.0f, wa.getRight() - xe), wa.getHeight()));

        auto marker = [&] (float x, bool isStart)
        {
            drawGlowLine (g, { x, wa.getY(), x, wa.getBottom() }, col::acid, 1.2f, 0.10f);
            juce::Path tri;
            if (isStart) tri.addTriangle (x, wa.getY(), x + 11.0f, wa.getY(), x, wa.getY() + 11.0f);
            else         tri.addTriangle (x, wa.getY(), x - 11.0f, wa.getY(), x, wa.getY() + 11.0f);
            g.setColour (col::acid); g.fillPath (tri);
            g.setColour (col::text); g.setFont (Fonts::value (10.0f));
            const juce::String t = isStart ? "START" : "END";
            g.drawText (t, (int) (isStart ? x + 13.0f : x - 40.0f), (int) wa.getY() + 1, 30, 12, isStart ? juce::Justification::left : juce::Justification::right, false);
        };
        marker (xs, true);
        marker (xe, false);

        if (playhead >= 0.0f)
        {
            const float x = xFor (playhead);
            drawGlowLine (g, { x, wa.getY(), x, wa.getBottom() - 16.0f }, col::radio, 1.4f, 0.22f);
            g.setColour (col::radio);
            g.fillEllipse (x - 3.5f, waveCentreY - 3.5f, 7.0f, 7.0f);
        }
    }

    void paintEmpty (juce::Graphics& g, juce::Rectangle<float> wa, PrysmProcessor::SampleStatus status)
    {
        if (status == PrysmProcessor::SampleStatus::Loading) return;
        const bool missing = status == PrysmProcessor::SampleStatus::Missing;
        auto zone = wa.reduced (28.0f, 22.0f);
        const float pulse = animate ? 0.55f + 0.45f * std::sin (time * 2.0f) : 0.8f;
        const auto accent = missing ? col::warn : col::acid;

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
        g.setColour (col::acid); g.strokePath (arc, juce::PathStrokeType (3.0f));
        g.setColour (col::text); g.setFont (Fonts::title (17.0f, 0.3f));
        g.drawText ("LOADING", wa.withTrimmedTop (wa.getHeight() * 0.5f + 20.0f).withHeight (24.0f).toNearestInt(), juce::Justification::centred, false);
    }

    void paintDropOverlay (juce::Graphics& g, juce::Rectangle<float> wa)
    {
        const auto c = dragSupported ? col::radio : col::error;
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
    std::array<Particle, 64> particles;
};
} // namespace prysm::ui
