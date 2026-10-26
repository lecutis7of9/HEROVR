#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky. Made by Agent Mulder.
//
// Version 0.0.62 BOLT WALLS: the mod's own shots (Iron Man's repulsor bolts, the chest beam and
// Spider-Man's own web balls) stop at the first wall instead of flying on through it (the user,
// 0.0.6.6: "iron man's repulsor beams don't stop when they hit walls").
//
// How (no new address): each shot asks the CONFIRMED ray bridge (native_rays, the same casts the
// swing and the sight checks use) for one ray along its flight; the bridge answers a frame or two
// later in world space. Until the answer comes, the hand's own aim ray (tags 0/1, cast every
// frame) stands in when the shot left along it. A fixed surface (native_rays::fixedSurface: the
// city, not a body) on the ray is the wall; the shot dies where it crosses it, with its impact.
// Fail safe: no answer (rays off, refused, a lost batch) means no wall - the shot flies as in
// 0.0.6, never stopped early by a guess.
//
// Pure (tests): world-space positions; the caller converts its tracking-space shots.
#include "math.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace spidy::bolt_walls {

// Ray tags (native_rays::Ray::tag): the hands' 0/1, the head's 2-6, the roof's 7 and the sight
// rays' 100+ are taken; a shot's are 1000 + slot * 16 + its fire serial's low 4 bits, the beam's
// 1200 + its serial's low 4 bits.
inline constexpr uint32_t ballTag0 = 1000, beamTag0 = 1200, slots = 6;
inline uint32_t ballTag(int slot, uint32_t serial) {
    return ballTag0 + static_cast<uint32_t>(std::clamp(slot, 0, static_cast<int>(slots) - 1)) * 16u + (serial & 15u);
}
inline bool ballOfTag(uint32_t tag, int& slot, uint32_t& low) {
    if (tag < ballTag0 || tag >= ballTag0 + slots * 16u)
        return false;
    slot = static_cast<int>((tag - ballTag0) / 16u);
    low = (tag - ballTag0) % 16u;
    return true;
}
inline uint32_t beamTag(uint32_t serial) { return beamTag0 + (serial & 15u); }
inline bool isBeamTag(uint32_t tag) { return tag >= beamTag0 && tag < beamTag0 + 16u; }

// What one cast found for one shot.
struct Wall {
    bool known{};    // an answer came
    bool blocked{};  // a fixed surface on the ray (else clear for its whole length)
    Vec3 from{}, dir{};
    float metres{};  // along dir to the surface (blocked), or the ray's length (clear)
};
inline bool usable(Vec3 v) { return finite(v); }
// A cast's answer: the ray's origin, unit direction and length, whether the hit is a fixed surface
// and its fraction (0..1 of the length; anything else: no hit).
inline Wall fromHit(Vec3 origin, Vec3 dir, float distance, bool fixedSurface, float fraction) {
    Wall w;
    if (!usable(origin) || !usable(dir) || std::abs(length(dir) - 1) > .01f || !std::isfinite(distance) || distance <= 0)
        return w;
    w.known = true;
    w.from = origin;
    w.dir = dir;
    w.metres = distance;
    if (fixedSurface && std::isfinite(fraction) && fraction >= 0 && fraction < 1) {
        w.blocked = true;
        w.metres = fraction * distance;
    }
    return w;
}
// The hand's aim ray (cast every frame) stands for a shot that left from near its origin, along it:
// the shot's start within nearMetres of the ray's line (and ahead of it), its direction within
// alignDegrees. The wall is moved onto the shot's own line (same distance along).
inline constexpr float nearMetres = .25f, alignDegrees = 3.f;
inline Wall fromHandRay(const Wall& hand, Vec3 shotFrom, Vec3 shotDir) {
    if (!hand.known || !usable(shotFrom) || !usable(shotDir) || length(shotDir) < .5f)
        return {};
    const Vec3 d = normalized(shotDir);
    if (dot(d, hand.dir) < std::cos(alignDegrees * 3.14159265f / 180.f))
        return {};
    const Vec3 rel = shotFrom - hand.from;
    const float along = dot(rel, hand.dir);
    if (along < -nearMetres || length(rel - hand.dir * along) > nearMetres)
        return {};
    Wall w = hand;
    w.from = shotFrom;
    w.dir = d;
    w.metres = std::max(0.f, hand.metres - along);
    return w;
}
// A shot that has turned (homing) or strayed from its cast's line needs a new cast: more than
// strayMetres off the line, or more than turnDegrees off its direction.
inline constexpr float strayMetres = .6f, turnDegrees = 8.f;
inline bool stale(const Wall& w, Vec3 at, Vec3 dir) {
    if (!w.known || !usable(at) || !usable(dir) || length(dir) < .5f)
        return true;
    const Vec3 rel = at - w.from;
    const float along = dot(rel, w.dir);
    return length(rel - w.dir * along) > strayMetres ||
           dot(normalized(dir), w.dir) < std::cos(turnDegrees * 3.14159265f / 180.f);
}
// Did the step before -> at reach the wall? Then cut: the point on the wall where it stops.
// A step that was already past it (the answer came late) stops at once, at the wall.
inline bool crossed(const Wall& w, Vec3 before, Vec3 at, Vec3* cut) {
    if (!w.known || !w.blocked || !usable(before) || !usable(at))
        return false;
    const float a = dot(at - w.from, w.dir);
    if (a < w.metres)
        return false;
    if (cut)
        *cut = w.from + w.dir * w.metres;
    (void)before;
    return true;
}
// The beam's reach: to the wall when one is known (never less than minBeam), else range.
inline constexpr float minBeam = .3f;
inline float beamReach(const Wall& w, float range) {
    if (!std::isfinite(range) || range <= 0)
        return 0;
    if (!w.known || !w.blocked)
        return range;
    return std::clamp(w.metres, minBeam, range);
}

} // namespace spidy::bolt_walls
