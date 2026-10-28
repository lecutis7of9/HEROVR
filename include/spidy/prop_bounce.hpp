#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky. Made by Agent Mulder.
//
// Version 0.0.73 PROP BOUNCE (fix 7): the thrown props - CAPTAIN AMERICA's shield, THOR's hammer and
// DAREDEVIL's batons - bounce off buildings, the floor and cars instead of flying through them.
//
// How (no new address): as BOLT WALLS, each prop in flight asks the CONFIRMED ray bridge (native_rays)
// for one ray along its flight (ahead of it by its speed). The answer (world space, a frame or two
// later) is the surface's point and normal: a Plane. Each frame the prop that reaches the plane (within
// its radius, moving into it) is put back on its side and its velocity reflected (keep: the share of
// speed it keeps). Any body counts (a car is not a fixed surface), except within skipNear of an enemy
// (the prop hits him on its own path) or of the player (never a bounce off his own body).
// Fail safe: no answer (rays off, refused, a lost batch) means no plane - the prop flies as in 0.0.72.
// CANDIDATE: that native_rays::Hit::normal is the surface's world-space unit normal (the bridge fills
// it from the cast; checked here only for finite and unit length - anything else: no plane).
//
// Pure (tests): the caller converts between its tracking space and the bridge's world space.
#include "spidy/math.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace spidy::prop_bounce {

// Ray tags 1300 + slot * 16 + the throw's low 4 bits (1000-1295 are BOLT WALLS', 1200+ the beam's).
enum Slot : uint32_t { batonLeft = 0, batonRight = 1, shield = 2, hammer = 3, slots = 4 };
inline constexpr uint32_t tag0 = 1300;
inline constexpr float radius = .12f, keep = .6f, skipNear = 1.f;
inline uint32_t tag(uint32_t slot, uint32_t serial) { return tag0 + std::min(slot, slots - 1u) * 16u + (serial & 15u); }
inline bool ofTag(uint32_t t, uint32_t& slot, uint32_t& low) {
    if (t < tag0 || t >= tag0 + slots * 16u)
        return false;
    slot = (t - tag0) / 16u;
    low = (t - tag0) % 16u;
    return true;
}

struct Plane {
    bool known{};
    Vec3 point{}, normal{};
    uint32_t serial{}; // the throw it was cast for (low 4 bits; the caller's throw count)
};

// The ray to ask for a prop at `at` moving at `velocity` (world): ahead by a quarter second and half a
// metre (the answer comes a few frames later), 0.5..20 m. False: nothing to ask (too slow, not finite).
inline bool ask(Vec3 at, Vec3 velocity, Vec3& dir, float& metres) {
    if (!finite(at) || !finite(velocity))
        return false;
    const float speed = length(velocity);
    if (!(speed > .5f))
        return false;
    dir = velocity / speed;
    metres = std::clamp(speed * .25f + .5f, .5f, 20.f);
    return true;
}

// A cast's answer as a plane (world): something hit (count > 0, fraction 0..1) with a unit normal
// facing back along the ray. Not known otherwise.
inline Plane fromHit(Vec3 origin, Vec3 dir, float distance, uint32_t count, float fraction, Vec3 normal, uint32_t serial) {
    Plane p;
    if (!count || !finite(origin) || !finite(dir) || !finite(normal) || !std::isfinite(distance) || distance <= 0 ||
        !std::isfinite(fraction) || fraction < 0 || fraction >= 1)
        return p;
    const float n = length(normal);
    if (n < .9f || n > 1.1f)
        return p;
    Vec3 nn = normal / n;
    if (dot(nn, dir) > 0)
        nn = nn * -1.f; // facing the prop
    p.known = true;
    p.point = origin + dir * (fraction * distance);
    p.normal = nn;
    p.serial = serial & 15u;
    return p;
}

// Not a wall: within skipNear of an enemy's chest or of the player's body (head to feet).
inline bool nearBody(Vec3 at, const Vec3* chests, uint32_t count, Vec3 head, Vec3 feet) {
    for (uint32_t i = 0; chests && i < count; ++i)
        if (finite(chests[i]) && length(at - chests[i]) < skipNear + .6f)
            return true;
    if (finite(head) && finite(feet)) {
        const Vec3 ab = head - feet;
        const float l2 = dot(ab, ab);
        const float t = l2 > 1e-6f ? std::clamp(dot(at - feet, ab) / l2, 0.f, 1.f) : 0.f;
        if (length(at - (feet + ab * t)) < skipNear)
            return true;
    }
    return false;
}

// One plane in another space (to: that space from this one).
inline Plane moved(const Plane& p, const Pose& to) {
    if (!p.known)
        return p;
    Plane q = p;
    q.point = compose(to, Pose{p.point, {}}).position;
    q.normal = to.orientation.rotate(p.normal);
    if (!finite(q.point) || !finite(q.normal) || std::abs(length(q.normal) - 1) > .01f)
        q.known = false;
    return q;
}

// The bounce: the prop at `position` (its radius) moving at `velocity` that is on or through the plane,
// moving into it, goes back to the plane's side and leaves along the reflection with `keep` of its
// speed. True on the frame it bounces.
inline bool bounce(Vec3& position, Vec3& velocity, const Plane& p, float r = radius, float k = keep) {
    if (!p.known || !finite(position) || !finite(velocity))
        return false;
    const float into = dot(velocity, p.normal);
    const float gap = dot(position - p.point, p.normal);
    if (!(into < -1e-3f) || !(gap < r) || gap < -2.f) // through by more than 2 m: another surface's plane
        return false;
    position = position + p.normal * (r - gap);
    velocity = (velocity - p.normal * (2.f * into)) * k;
    return true;
}

} // namespace spidy::prop_bounce
