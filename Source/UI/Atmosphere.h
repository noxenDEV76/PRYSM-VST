#pragma once
#include "Theme.h"

namespace prysm::ui
{
/** Full-window animated background: slow neon smoke (rendered small and upscaled so it stays soft and cheap)
    plus drifting particles that brighten with the audio level. With animations reduced it is drawn once, static. */
class Atmosphere : public juce::Component
{
public:
    Atmosphere()
    {
        setInterceptsMouseClicks (false, false);
        setOpaque (true);
        juce::Random rng (777);
        for (auto& p : particles)
        {
            p.x = rng.nextFloat();  p.y = rng.nextFloat();
            p.speed = 0.006f + rng.nextFloat() * 0.022f;
            p.sway = 0.004f + rng.nextFloat() * 0.012f;
            p.size = 0.8f + rng.nextFloat() * rng.nextFloat() * 3.2f;
            p.phase = rng.nextFloat() * juce::MathConstants<float>::twoPi;
            p.violet = rng.nextFloat() < 0.28f;
        }
        rebuildSmoke();
    }

    void setAnimated (bool a) { animated = a; rebuildSmoke(); repaint(); }

    /** `dt` seconds, `level` 0..1 audio activity. */
    void tick (float dt, float level)
    {
        if (! animated) return;
        time += dt;
        energy += (juce::jlimit (0.0f, 1.0f, level) - energy) * 0.12f;
        for (auto& p : particles)
        {
            p.y -= p.speed * dt * (1.0f + 3.0f * energy);
            if (p.y < -0.02f) { p.y = 1.02f; }
        }
        smokeTick += dt;
        if (smokeTick >= 1.0f / 14.0f) { smokeTick = 0.0f; rebuildSmoke(); }
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat();
        g.setGradientFill (juce::ColourGradient (col::bg0, 0, 0, juce::Colour (0xff040a1a), 0, b.getBottom(), false));
        g.fillRect (b);

        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.setOpacity (1.0f);
        g.drawImage (smoke, b);

        // fine perspective grid, very faint (reads as "digital" without noise)
        g.setColour (col::neon.withAlpha (0.025f));
        for (int x = 0; x < getWidth(); x += 40) g.drawVerticalLine (x, 0.0f, b.getBottom());
        for (int y = 0; y < getHeight(); y += 40) g.drawHorizontalLine (y, 0.0f, b.getRight());

        for (auto& p : particles)
        {
            const float sway = animated ? std::sin (time * 0.6f + p.phase) * p.sway : 0.0f;
            const float px = (p.x + sway) * b.getWidth(), py = p.y * b.getHeight();
            const float tw = animated ? 0.55f + 0.45f * std::sin (time * 1.4f + p.phase * 2.0f) : 0.8f;
            const float a = (0.22f + 0.5f * tw) * (0.7f + 0.6f * energy);
            const auto c = p.violet ? col::violet.brighter (0.4f) : col::glow;
            g.setColour (c.withAlpha (juce::jmin (0.5f, a * 0.28f)));
            g.fillEllipse (px - p.size * 2.2f, py - p.size * 2.2f, p.size * 4.4f, p.size * 4.4f);
            g.setColour (c.withAlpha (juce::jmin (0.95f, a)));
            g.fillEllipse (px - p.size * 0.5f, py - p.size * 0.5f, p.size, p.size);
        }
        // vignette
        g.setGradientFill (juce::ColourGradient (juce::Colours::transparentBlack, b.getCentreX(), b.getCentreY(), col::bg0.withAlpha (0.35f), 0.0f, 0.0f, true));
        g.fillRect (b);
    }

private:
    struct Particle { float x, y, speed, sway, size, phase; bool violet; };

    void rebuildSmoke()
    {
        constexpr int W = 250, H = 160;
        if (! smoke.isValid()) smoke = juce::Image (juce::Image::ARGB, W, H, true);
        smoke.clear (smoke.getBounds());
        juce::Graphics g (smoke);
        struct Blob { float x, y, rx, ry, fx, fy, ph, a; int kind; };
        static const Blob blobs[] = {
            { 0.04f, 0.55f, 0.34f, 0.62f, 0.07f, 0.05f, 0.0f, 0.34f, 0 }, { 0.10f, 0.95f, 0.40f, 0.40f, 0.05f, 0.08f, 1.3f, 0.34f, 1 },
            { 0.96f, 0.45f, 0.34f, 0.62f, 0.06f, 0.07f, 2.1f, 0.34f, 0 }, { 0.92f, 0.98f, 0.42f, 0.42f, 0.08f, 0.05f, 3.4f, 0.32f, 1 },
            { 0.50f, 1.10f, 0.60f, 0.38f, 0.04f, 0.06f, 4.2f, 0.30f, 2 }, { 0.20f, 0.02f, 0.40f, 0.30f, 0.05f, 0.04f, 5.0f, 0.22f, 2 },
            { 0.82f, 0.00f, 0.40f, 0.30f, 0.06f, 0.05f, 0.7f, 0.22f, 1 }, { 0.50f, 0.50f, 0.34f, 0.28f, 0.09f, 0.07f, 2.8f, 0.10f, 0 },
            { 0.30f, 0.70f, 0.22f, 0.26f, 0.11f, 0.09f, 1.9f, 0.14f, 2 }, { 0.70f, 0.30f, 0.22f, 0.26f, 0.10f, 0.12f, 3.9f, 0.14f, 0 },
        };
        const float t = animated ? time : 3.0f;
        const juce::Colour kinds[3] = { col::neon, col::violet, col::midBlue.brighter (0.5f) };
        for (const auto& b : blobs)
        {
            const float cx = (b.x + 0.07f * std::sin (t * b.fx * 3.0f + b.ph) + 0.03f * std::sin (t * b.fy * 7.0f + b.ph * 2.0f)) * (float) W;
            const float cy = (b.y + 0.06f * std::cos (t * b.fy * 3.0f + b.ph)) * (float) H;
            const float rot = t * 0.04f * (b.kind == 1 ? -1.0f : 1.0f) + b.ph;
            const float rx = b.rx * (float) W * (1.0f + 0.12f * std::sin (t * 0.2f + b.ph));
            const float ry = b.ry * (float) H * (1.0f + 0.12f * std::cos (t * 0.17f + b.ph));
            g.saveState();
            g.addTransform (juce::AffineTransform::rotation (rot * 0.5f, cx, cy));
            g.setGradientFill (juce::ColourGradient (kinds[b.kind].withAlpha (juce::jmin (0.9f, b.a * 2.1f)), cx, cy, kinds[b.kind].withAlpha (0.0f), cx + rx, cy, true));
            g.fillEllipse (cx - rx, cy - ry, rx * 2.0f, ry * 2.0f);
            g.restoreState();
        }
    }

    std::array<Particle, 84> particles;
    juce::Image smoke;
    bool animated = true;
    float time = 0.0f, energy = 0.0f, smokeTick = 0.0f;
};
} // namespace prysm::ui
