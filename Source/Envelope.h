#pragma once
#include <algorithm>
#include <cmath>

namespace prysm
{
/** Shape of one envelope segment. x = progress 0..1, tension in -1..1 (0 = straight line).
    Positive tension = slow start / fast finish, negative = fast start / slow finish.
    Shared by the audio engine and the editor graph so the picture is exactly what you hear. */
inline float envCurve (float x, float tension) noexcept
{
    x = std::min (1.0f, std::max (0.0f, x));
    if (std::abs (tension) < 1.0e-3f) return x;
    const float k = tension * 6.0f;
    return (std::exp (k * x) - 1.0f) / (std::exp (k) - 1.0f);
}

/** Delay - Attack - Hold - Decay - Sustain - Release envelope with a tension (curve) on A, D and R.
    Allocation-free, safe on the audio thread. Parameters may change while a note is playing. */
class Dahdsr
{
public:
    struct Params
    {
        float delay = 0.0f, attack = 0.001f, hold = 0.0f, decay = 0.4f, sustain = 1.0f, release = 0.08f; // seconds / level
        float tensionA = 0.0f, tensionD = 0.0f, tensionR = 0.0f;                                         // -1..1
    };

    void setSampleRate (double sr) noexcept { rate = (float) (sr > 0 ? sr : 44100.0); smooth = 1.0f - std::exp (-1.0f / (0.005f * rate)); }
    void setParameters (const Params& np) noexcept { p = np; }
    void reset() noexcept { stage = Stage::Idle; t = 0; level = 0; relFrom = 0; }

    void noteOn() noexcept { stage = Stage::Delay; t = 0; level = 0; }
    void noteOff() noexcept
    {
        if (stage == Stage::Idle || stage == Stage::Release) return;
        relFrom = level; stage = Stage::Release; t = 0;
    }
    bool isActive() const noexcept { return stage != Stage::Idle; }

    float getNextSample() noexcept
    {
        for (int guard = 0; guard < 8; ++guard)
        {
            switch (stage)
            {
                case Stage::Idle: return 0.0f;
                case Stage::Delay:
                    if (t < p.delay * rate) { t += 1.0f; level = 0.0f; return 0.0f; }
                    enter (Stage::Attack); break;
                case Stage::Attack:
                {
                    const float len = std::max (1.0f, p.attack * rate);
                    if (t < len) { level = envCurve (t / len, p.tensionA); t += 1.0f; return level; }
                    enter (Stage::Hold); break;
                }
                case Stage::Hold:
                    if (t < p.hold * rate) { t += 1.0f; level = 1.0f; return 1.0f; }
                    enter (Stage::Decay); break;
                case Stage::Decay:
                {
                    const float len = std::max (1.0f, p.decay * rate);
                    if (t < len) { level = 1.0f - (1.0f - p.sustain) * envCurve (t / len, p.tensionD); t += 1.0f; return level; }
                    enter (Stage::Sustain); break;
                }
                case Stage::Sustain:
                    level += (p.sustain - level) * smooth; // glide so moving SUS while playing never clicks
                    return level;
                case Stage::Release:
                {
                    const float len = std::max (1.0f, p.release * rate);
                    if (t < len) { level = relFrom * (1.0f - envCurve (t / len, p.tensionR)); t += 1.0f; return level; }
                    stage = Stage::Idle; level = 0.0f; return 0.0f;
                }
            }
        }
        return level;
    }

private:
    enum class Stage { Idle, Delay, Attack, Hold, Decay, Sustain, Release };
    void enter (Stage s) noexcept { stage = s; t = 0; }

    Params p;
    Stage stage = Stage::Idle;
    float rate = 44100.0f, smooth = 0.01f, t = 0.0f, level = 0.0f, relFrom = 0.0f;
};
} // namespace prysm
