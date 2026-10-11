#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "BinaryData.h"

namespace prysm::ui
{
// Logical design size of the editor. The whole UI is drawn at this size and scaled uniformly.
constexpr int kDesignW = 1000;
constexpr int kDesignH = 660;

namespace col
{
    const juce::Colour bg0       { 0xff02050c }; // deep space navy / black
    const juce::Colour bg1       { 0xff060d1c }; // panels
    const juce::Colour bg2       { 0xff0b1730 }; // raised surfaces
    const juce::Colour edge      { 0xff14264a };
    const juce::Colour edgeHi    { 0xff24427f };
    const juce::Colour deepBlue  { 0xff081d47 }; // selected / active surfaces
    const juce::Colour midBlue   { 0xff1246a8 };
    const juce::Colour neon      { 0xff2fa8ff }; // main neon blue
    const juce::Colour glow      { 0xff7fe4ff }; // bright cyan accent
    const juce::Colour violet    { 0xff7a5cff }; // second vibe colour (smoke / gradients)
    const juce::Colour text      { 0xffeaf3ff };
    const juce::Colour grey      { 0xff8aa2c8 };
    const juce::Colour greyDim   { 0xff4a6089 };
    const juce::Colour warn      { 0xffffb020 };
    const juce::Colour error     { 0xffff5148 };
}

struct Fonts
{
    static juce::Typeface::Ptr label()
    {
        static juce::Typeface::Ptr t = juce::Typeface::createSystemTypefaceFor (BinaryData::RajdhaniSemiBold_ttf, BinaryData::RajdhaniSemiBold_ttfSize);
        return t;
    }
    static juce::Typeface::Ptr mono()
    {
        static juce::Typeface::Ptr t = juce::Typeface::createSystemTypefaceFor (BinaryData::ShareTechMonoRegular_ttf, BinaryData::ShareTechMonoRegular_ttfSize);
        return t;
    }
    static juce::Typeface::Ptr display()
    {
        static juce::Typeface::Ptr t = juce::Typeface::createSystemTypefaceFor (BinaryData::Orbitron_ttf, BinaryData::Orbitron_ttfSize);
        return t;
    }
    /** Wide futuristic display face (title). */
    static juce::Font headline (float h, float tracking = 0.2f) { return juce::Font (juce::FontOptions (display()).withHeight (h).withKerningFactor (tracking)); }
    /** Technical sans (tracked small caps style) for labels, titles and buttons. */
    static juce::Font title (float h, float tracking = 0.12f) { return juce::Font (juce::FontOptions (label()).withHeight (h).withKerningFactor (tracking)); }
    /** Monospace for numeric readouts. */
    static juce::Font value (float h) { return juce::Font (juce::FontOptions (mono()).withHeight (h)); }
}; // namespace Fonts

/** Title font that shrinks (never grows) so that `text` fits in `maxWidth`. */
inline juce::Font fitTitle (float height, float tracking, const juce::String& text, float maxWidth)
{
    auto f = Fonts::title (height, tracking);
    const float w = juce::GlyphArrangement::getStringWidth (f, text);
    if (w > maxWidth && w > 0.0f) f = Fonts::title (juce::jmax (9.0f, height * maxWidth / w), tracking);
    return f;
}

inline juce::Rectangle<float> centredSquare (juce::Rectangle<float> r, float size)
{
    return juce::Rectangle<float> (size, size).withCentre (r.getCentre());
}

inline void drawGlowLine (juce::Graphics& g, juce::Line<float> l, juce::Colour c, float width = 1.5f, float glow = 0.18f)
{
    g.setColour (c.withAlpha (glow));
    g.drawLine (l, width * 4.0f);
    g.setColour (c.withAlpha (glow * 1.8f));
    g.drawLine (l, width * 2.2f);
    g.setColour (c);
    g.drawLine (l, width);
}

/** Corner brackets used as a recurring "lab instrument" motif. */
inline void drawBrackets (juce::Graphics& g, juce::Rectangle<float> r, float len, juce::Colour c, float thick = 1.2f)
{
    g.setColour (c);
    auto l = [&] (float x1, float y1, float x2, float y2) { g.drawLine (x1, y1, x2, y2, thick); };
    l (r.getX(), r.getY() + len, r.getX(), r.getY());               l (r.getX(), r.getY(), r.getX() + len, r.getY());
    l (r.getRight() - len, r.getY(), r.getRight(), r.getY());       l (r.getRight(), r.getY(), r.getRight(), r.getY() + len);
    l (r.getX(), r.getBottom() - len, r.getX(), r.getBottom());     l (r.getX(), r.getBottom(), r.getX() + len, r.getBottom());
    l (r.getRight() - len, r.getBottom(), r.getRight(), r.getBottom()); l (r.getRight(), r.getBottom(), r.getRight(), r.getBottom() - len);
}

class PrysmLookAndFeel : public juce::LookAndFeel_V4
{
public:
    PrysmLookAndFeel()
    {
        setColour (juce::PopupMenu::backgroundColourId, col::bg1);
        setColour (juce::PopupMenu::textColourId, col::text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, col::midBlue);
        setColour (juce::PopupMenu::highlightedTextColourId, col::text);
        setColour (juce::TooltipWindow::backgroundColourId, col::bg2);
        setColour (juce::TooltipWindow::textColourId, col::text);
        setColour (juce::TooltipWindow::outlineColourId, col::edgeHi);
        setColour (juce::AlertWindow::backgroundColourId, col::bg1);
        setColour (juce::AlertWindow::textColourId, col::text);
        setColour (juce::AlertWindow::outlineColourId, col::edgeHi);
        setColour (juce::TextButton::buttonColourId, col::bg2);
        setColour (juce::TextButton::textColourOffId, col::text);
        setDefaultSansSerifTypeface (Fonts::label());
    }

    //==========================================================================
    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float startA, float endA, juce::Slider& s) override
    {
        auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
        const float d = juce::jmin (b.getWidth(), b.getHeight());
        auto area = centredSquare (b, d).reduced (3.0f);
        const auto c = area.getCentre();
        const float r = area.getWidth() * 0.5f;
        const bool bipolar = (bool) s.getProperties().getWithDefault ("bipolar", false);
        const bool active = s.isEnabled();
        const float hot = s.isMouseOverOrDragging() ? 1.0f : 0.0f;
        const float ang = startA + pos * (endA - startA);

        // ticks
        g.setColour (col::greyDim.withAlpha (0.8f));
        for (int i = 0; i <= 10; ++i)
        {
            const float a = startA + (float) i / 10.0f * (endA - startA);
            const float c1 = std::cos (a - juce::MathConstants<float>::halfPi), s1 = std::sin (a - juce::MathConstants<float>::halfPi);
            g.drawLine (c.x + c1 * (r + 0.5f), c.y + s1 * (r + 0.5f), c.x + c1 * (r - 2.0f), c.y + s1 * (r - 2.0f), i % 5 == 0 ? 1.4f : 0.8f);
        }

        // body
        const float rb = r - 5.0f;
        g.setGradientFill (juce::ColourGradient (col::edgeHi.brighter (0.15f), c.x - rb * 0.4f, c.y - rb * 0.6f, col::bg0, c.x + rb * 0.6f, c.y + rb, true));
        g.fillEllipse (c.x - rb, c.y - rb, rb * 2, rb * 2);
        g.setColour (col::edge.brighter (0.2f + 0.2f * hot));
        g.drawEllipse (c.x - rb, c.y - rb, rb * 2, rb * 2, 1.0f);

        // track + value arc
        const float ra = r - 2.2f;
        juce::Path track;
        track.addCentredArc (c.x, c.y, ra, ra, 0.0f, startA, endA, true);
        g.setColour (col::edge);
        g.strokePath (track, juce::PathStrokeType (2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::butt));

        const float from = bipolar ? (startA + endA) * 0.5f : startA;
        juce::Path val;
        val.addCentredArc (c.x, c.y, ra, ra, 0.0f, juce::jmin (from, ang), juce::jmax (from, ang), true);
        const auto vc = active ? col::neon : col::greyDim;
        if (active)
        {
            g.setColour (vc.withAlpha (0.14f + 0.1f * hot));
            g.strokePath (val, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
        }
        g.setColour (vc);
        g.strokePath (val, juce::PathStrokeType (2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::butt));

        // pointer
        const float ca = std::cos (ang - juce::MathConstants<float>::halfPi), sa = std::sin (ang - juce::MathConstants<float>::halfPi);
        g.setColour (col::text);
        g.drawLine (c.x + ca * rb * 0.28f, c.y + sa * rb * 0.28f, c.x + ca * rb * 0.82f, c.y + sa * rb * 0.82f, 2.0f);
    }

    //==========================================================================
    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (1.0f);
        const bool on = b.getToggleState();
        g.setColour (on ? col::deepBlue : col::bg2);
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (on ? col::neon.withAlpha (0.9f) : (over ? col::edgeHi : col::edge));
        g.drawRoundedRectangle (r, 4.0f, 1.0f);

        auto led = juce::Rectangle<float> (10.0f, 10.0f).withCentre ({ r.getX() + 16.0f, r.getCentreY() });
        if (on)
        {
            g.setColour (col::neon.withAlpha (0.25f));
            g.fillEllipse (led.expanded (4.0f));
            g.setColour (col::neon);
        }
        else g.setColour (col::greyDim);
        g.fillEllipse (led);

        g.setColour (on ? col::text : col::grey);
        g.setFont (Fonts::title (15.0f, 0.14f));
        g.drawText (b.getButtonText(), r.withTrimmedLeft (30.0f).toNearestInt(), juce::Justification::centredLeft, false);
        juce::ignoreUnused (down);
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        const bool accent = (bool) b.getProperties().getWithDefault ("accent", false);
        auto base = accent ? col::deepBlue : col::bg2;
        if (down) base = base.brighter (0.15f);
        g.setColour (base);
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (accent ? col::neon.withAlpha (over ? 1.0f : 0.75f) : (over ? col::edgeHi.brighter (0.2f) : col::edgeHi));
        g.drawRoundedRectangle (r, 4.0f, 1.0f);
    }
    void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool) override
    {
        const bool accent = (bool) b.getProperties().getWithDefault ("accent", false);
        g.setColour (b.isEnabled() ? (accent ? col::neon : col::text) : col::greyDim);
        g.setFont (Fonts::title ((float) b.getHeight() * 0.56f, 0.14f));
        g.drawText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred, false);
    }

    //==========================================================================
    juce::Font getPopupMenuFont() override { return Fonts::title (16.0f, 0.1f); }
    void drawPopupMenuBackground (juce::Graphics& g, int w, int h) override
    {
        g.fillAll (col::bg1);
        g.setColour (col::edgeHi);
        g.drawRect (0, 0, w, h, 1);
    }
    juce::Font getTextButtonFont (juce::TextButton&, int h) override { return Fonts::title ((float) h * 0.56f); }
    juce::Font getAlertWindowMessageFont() override { return Fonts::title (16.0f, 0.04f); }
    juce::Font getAlertWindowTitleFont() override { return Fonts::title (20.0f, 0.12f); }
};
} // namespace prysm::ui
