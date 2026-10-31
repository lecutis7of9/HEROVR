#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
//
// 0.0.74 test 10 THROW BY SWING (a tester: "thrown objects go in the direction of release rather than
// direction of throwing - overhand goes down, under hand goes up"). Every hand throw (BATMAN's batarangs
// and smoke pellets, CAPTAIN AMERICA's shield, THOR's hammer, DAREDEVIL's baton) took its line from the
// controller's pointing at the release (or the one frame's hand speed there, often already slowing): an
// overhand throw lets go with the wrist flicked down, an underhand one with it turned up. Now the line is
// the hand's own motion: the mean velocity over a windowSeconds window round the fastest moment of the
// last memorySeconds (the throw's peak, the release usually after it). The old line is kept when the swing
// is slower than the throw's own least speed, goes back behind the view, or tracking blinked or jumped
// (NaN / over jumpSpeed) inside that window. Half of a downward part is taken off (an overhand peak still
// points a little down; the existing aim assists then pick an enemy on that line as before).
// Option THROW BY SWING (combat38::throwSwing, live.txt throw_swing): ON (0, the default) / OFF (1, the
// old aim). Pure; no game memory, no address.
#include "spidy/math.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace spidy::throw_swing {

inline constexpr float memorySeconds = .25f; // the samples looked at for the peak
inline constexpr float windowSeconds = .08f; // the peak window (60-100 ms)
inline constexpr float leastSpeed = 1.5f;    // m/s: a slower swing is no throw (the old line)
inline constexpr float jumpSpeed = 25.f;     // m/s: faster is a tracking jump, never a swing
inline constexpr float behindView = -.3f;    // a swing this far behind the view (cos): the old line
inline constexpr float downShare = .5f;      // the share of a downward part kept

struct Swing {
    Vec3 velocity{}; // the window's mean velocity (tracking space)
    float speed{};
    bool ok{};       // a clean window (no blink, no jump) with a speed
};

class Tracker {
  public:
    // One frame: the hand's position (tracking space) and whether it is tracked.
    void step(float dt, bool tracked, Vec3 position) {
        const float t = std::isfinite(dt) ? std::clamp(dt, 0.f, .1f) : 0.f;
        if (!(t > 1e-5f))
            return;
        const float before = now_;
        now_ = std::fmod(now_ + t, 1e3f); // kept small: float time stays fine to well under a millisecond
        if (now_ < before) // the clock wrapped: start over
            n_ = 0;
        Sample s{{}, now_, t, false};
        if (tracked && finite(position)) {
            if (have_) {
                const Vec3 v = (position - last_) / t;
                s.velocity = v;
                s.good = finite(v) && length(v) < jumpSpeed;
            }
            last_ = position;
            have_ = true;
        } else {
            have_ = false;
        }
        head_ = (head_ + 1) % ring;
        samples_[head_] = s;
        n_ = std::min(n_ + 1, ring);
    }
    Swing swing() const {
        Swing out;
        int peak = -1;
        float best = -1.f;
        for (int k = 0; k < n_; ++k) {
            const Sample& s = at(k);
            if (now_ - s.time > memorySeconds)
                break;
            const float sp = s.good ? length(s.velocity) : -1.f;
            if (sp > best)
                best = sp, peak = k;
        }
        if (peak < 0 || !(best > 0))
            return out;
        const float centre = at(peak).time - at(peak).dt * .5f;
        Vec3 sum{};
        float weight = 0;
        for (int k = 0; k < n_; ++k) {
            const Sample& s = at(k);
            const float mid = s.time - s.dt * .5f;
            if (centre - mid > windowSeconds * .5f)
                break;
            if (mid - centre > windowSeconds * .5f)
                continue;
            if (!s.good) // a blink or a jump inside the throw's window: not trusted
                return out;
            sum = sum + s.velocity * s.dt;
            weight += s.dt;
        }
        if (!(weight > 0))
            return out;
        out.velocity = sum / weight;
        out.speed = length(out.velocity);
        out.ok = finite(out.velocity) && std::isfinite(out.speed);
        return out;
    }
    void reset() { *this = Tracker{}; }

  private:
    static constexpr int ring = 64;
    struct Sample {
        Vec3 velocity{};
        float time{}, dt{};
        bool good{};
    };
    const Sample& at(int back) const { return samples_[(head_ - back + ring) % ring]; } // 0: the newest
    Sample samples_[ring]{};
    int n_ = 0, head_ = 0;
    float now_ = 0;
    Vec3 last_{};
    bool have_ = false;
};

inline Vec3 unitOr(Vec3 v, Vec3 fallback) {
    return finite(v) && length(v) > 1e-4f ? normalized(v) : fallback;
}

// The throw's line: ON, the swing's (`down` of a downward part kept: downShare for the straight fliers, 1 for
// a dropped smoke pellet) when it is fast enough (least m/s), clean and not behind the view; otherwise (and
// OFF) the old line, as a unit vector (NaN safe: straight ahead).
inline Vec3 direction(bool on, const Swing& s, Vec3 oldLine, Vec3 viewForward, float least = leastSpeed,
                      float down = downShare) {
    const Vec3 old = unitOr(oldLine, Vec3{0, 0, -1});
    if (!on || !s.ok || !finite(s.velocity) || !(s.speed >= least) || !(s.speed < jumpSpeed))
        return old;
    Vec3 d = s.velocity / s.speed;
    if (finite(viewForward) && length(viewForward) > 1e-4f && dot(d, normalized(viewForward)) < behindView)
        return old;
    if (d.y < 0)
        d.y *= std::isfinite(down) ? std::clamp(down, 0.f, 1.f) : downShare;
    return unitOr(d, old);
}
} // namespace spidy::throw_swing
