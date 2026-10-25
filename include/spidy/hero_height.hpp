#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
//
// Version 0.0.5 HERO HEIGHT (BODY SCALE): for a big hero model (a Hulk skin mod). The VR body
// (native_body) sizes the model to the player: its eyes go to the player's eyes. A model taller
// than Spider-Man (its own rest pose taller, or its mesh bigger than its rig) is then pulled down
// to the player's eye height and its legs bend - "the legs look crouched". HERO HEIGHT tells the
// body the hero is k times the player: its eyes stand k times the player's eye height above the
// feet, its size and its arm reach mapping grow with it, so the big model stands up straight.
//
// Only the body's targets change (native_body::Command: eyes, height). The view stays at the
// player's own eyes, and the tracking, the swing, FLY MODE, the hands' places (the controllers),
// comfort and every game input are untouched. Live: read every frame. 1.00X (the default)
// changes nothing at all.
// Pure; no game memory.
#include "spidy/math.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace spidy::hero_height {

inline constexpr uint32_t steps = 5;
// The hologram's values (combat38 heroHeight): 1.00X (0, the default) .. 1.60X (4).
inline constexpr float factors[steps] = {1.f, 1.15f, 1.3f, 1.45f, 1.6f};
inline constexpr float most = 1.6f;
inline float factorOf(uint32_t value) { return value < steps ? factors[value] : 1.f; }
inline float clampFactor(float k) { return std::isfinite(k) ? std::clamp(k, 1.f, most) : 1.f; }

// The body's targets for the player's: eyes relative to the feet (world axes, `up` the body's up,
// a unit vector), the player's eye height (metres, 0 unknown) and arm (metres, 0 the hero's own).
struct Targets {
    Vec3 eyes{};
    float height{};
    float armLength{};
};
// The player's targets made the hero's at factor k: the eyes rise along `up` by (k - 1) times the
// player's standing eye height (a constant lift: the head's own bobbing is not scaled, so nothing
// moves more than the player does), the height is k times, the arm k times. The hands keep their
// places (the controllers).
inline Targets scaled(Targets t, Vec3 up, float k) {
    k = clampFactor(k);
    if (k <= 1.f || !(t.height > 0) || !std::isfinite(t.height))
        return t;
    const float upLength = length(up);
    const Vec3 u = upLength > .5f && std::isfinite(upLength) ? up / upLength : Vec3{0, 1, 0};
    t.eyes = t.eyes + u * ((k - 1) * t.height);
    t.height *= k;
    if (t.armLength > 0)
        t.armLength *= k;
    return t;
}
// native_body's size limits for a body at factor k: its usual ones (calibrated 0.7-1.3, headset
// height 0.85-1.2) with the upper end k times, so a k-times hero is never cut back to Spider-Man's.
inline float maxScale(float usual, float k) { return usual * clampFactor(k); }
// The Command's height check (native_body: 1 < height < 2.5 m) for a k-times hero.
inline float maxHeight(float k) { return 2.5f * clampFactor(k); }

} // namespace spidy::hero_height
