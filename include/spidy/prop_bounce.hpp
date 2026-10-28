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

// Ray tags 1400 + slot * 16 + the throw's low 4 bits (1000-1295 are BOLT WALLS', 1200+ the beam's).
// Test 3 WEAPON COLLISION: 1400, no longer 1300 - 1300-1363 are the PUNISHER VAN probe's tags and 1364
// its bumper's (game_xr vanTag0), and the van's answer branch came first and swallowed every prop's
// answer: in the game the props never had a plane, so fix 7 never bounced anything. + BATMAN's batarangs.
enum Slot : uint32_t { batonLeft = 0, batonRight = 1, shield = 2, hammer = 3, batarangLeft = 4, batarangRight = 5, slots = 6 };
inline constexpr uint32_t tag0 = 1400;
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
    float reach{};     // test 3 WEAPON COLLISION: the ray's length (m) - how deep a late answer may still catch it
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
    p.reach = distance;
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

// ---------------------------------------------------------------------------------------------------
// Version 0.0.73 test 3 WEAPON COLLISION (the user: "make sure weapons like shields and batons and the
// hammer and stuff have collision with cars and the ground buildings").
//
// What was wrong with fix 7 alone: BATMAN's batarangs were never asked a ray; a prop coming HOME never
// collided (it flew back through the wall); every hit within 1 m of the player's head-to-feet line was
// thrown away - the GROUND under him, and a wall or a car he stands at - and within 1.6 m of an enemy's
// chest (a wall behind him); and the one-plane test caught a prop at most 2 m through it, so a fast
// throw whose answer came a frame or three late flew on through (tunnelled), while a far plane could
// bounce a prop that was nowhere near the surface (no extent).
//
// Now: sweep() tests this frame's whole step (from -> to) against the plane, any depth up to the
// ray's own length, but only within `extent` of where the ray hit (the surface is there, not
// everywhere); each prop has a Feel (bounce or stop); slide() keeps a prop coming home on its side of
// a surface (along it, over it) for at most ghostAfter seconds of being blocked, then it passes through
// as in 0.0.72 - it ALWAYS comes home (the props' own recall timeouts are unchanged); and wallHit()
// keeps the ground under the player and the surfaces round enemies.
// Fail safe as before: no answer, no plane - the prop flies as it did.
inline constexpr float extent = 3.f;        // m along the surface from the ray's hit
inline constexpr float ghostAfter = .75f;   // s blocked on the way home, then through
inline constexpr float playerRadius = .45f; // m: the player's own body (never a wall)
inline constexpr float enemyRadius = .6f;   // m round an enemy's body (chest -1.2 m .. +0.5 m)

// How a prop meets a surface: keep, the share of its speed it leaves with (0: it stops dead there);
// rest, the seconds it stays put after a stop (then it comes home).
struct Feel {
    float keep = prop_bounce::keep;
    float rest = 0;
};
inline constexpr Feel shieldFeel{.6f, 0};    // CAPTAIN AMERICA's shield: a ricochet
inline constexpr Feel batonFeel{.45f, 0};    // DAREDEVIL's batons: a clatter off it
inline constexpr Feel batarangFeel{0, .35f}; // BATMAN's batarangs: they stick, then come home
inline constexpr Feel hammerFeel{.5f, 0};    // THOR's hammer going out: a heavy bounce

// The plane's surface reaches the point `at` (within extent of the ray's hit, along the plane).
inline bool within(const Plane& p, Vec3 at) {
    const Vec3 d = at - p.point;
    const Vec3 along = d - p.normal * dot(d, p.normal);
    return length(along) <= extent;
}

// The swept test: the prop went from `from` to `position` this frame (its radius r) moving at
// `velocity`. If that step reached the plane's surface (moving into it), it is put at the contact on
// its side and leaves along the reflection with k of its speed (k 0: stopped). Depth: up to the ray's
// length (at least 2 m) - a late answer still catches a fast prop that went through. True on a hit.
inline bool sweep(Vec3 from, Vec3& position, Vec3& velocity, const Plane& p, float r = radius, float k = keep) {
    if (!p.known || !finite(position) || !finite(velocity) || !finite(p.point) || !finite(p.normal))
        return false;
    if (!finite(from))
        from = position;
    const float into = dot(velocity, p.normal);
    const float g1 = dot(position - p.point, p.normal);
    if (!(into < -1e-3f) || !(g1 < r) || g1 < -std::max(2.f, p.reach))
        return false;
    const float g0 = dot(from - p.point, p.normal);
    Vec3 contact{};
    if (g0 >= r && g0 - g1 > 1e-6f) { // crossed this frame: where it touched
        const float t = std::clamp((g0 - r) / (g0 - g1), 0.f, 1.f);
        contact = from + (position - from) * t;
    } else { // already in it (a late answer): straight back out
        contact = position;
    }
    contact = contact + p.normal * (r - dot(contact - p.point, p.normal));
    if (!within(p, contact) || !finite(contact))
        return false;
    position = contact;
    velocity = k > 0 ? (velocity - p.normal * (2.f * into)) * k : Vec3{};
    return true;
}

// Coming home: a prop pressed into the plane stays on its side and goes along it (over it, when it
// is head-on), at its speed. True when it was held back this frame (the caller counts the seconds).
inline bool slide(Vec3& position, Vec3& velocity, const Plane& p, float r = radius) {
    if (!p.known || !finite(position) || !finite(velocity))
        return false;
    const float into = dot(velocity, p.normal);
    const float gap = dot(position - p.point, p.normal);
    if (!(into < -1e-3f) || !(gap < r) || gap < -std::max(2.f, p.reach) || !within(p, position))
        return false;
    const float speed = length(velocity);
    position = position + p.normal * (r - gap);
    Vec3 along = velocity - p.normal * into;
    if (length(along) < .3f * speed) { // head-on: up and over (or off the side of a floor/ceiling)
        Vec3 up = Vec3{0, 1, 0} - p.normal * p.normal.y;
        if (length(up) < .1f)
            up = Vec3{1, 0, 0} - p.normal * p.normal.x;
        along = along + normalized(up) * (.6f * speed);
    }
    velocity = length(along) > 1e-4f ? normalized(along) * speed : Vec3{};
    return finite(velocity);
}

// Is the ray's hit a surface a prop should meet? Not the player's own body (within playerRadius of
// his head-to-feet line, unless it is ground under his feet), not an enemy's body (within enemyRadius
// of chest -1.2 m .. +0.5 m, unless it is ground under him); the ground (normal up), walls, cars yes.
inline bool wallHit(Vec3 at, Vec3 normal, const Vec3* chests, uint32_t count, Vec3 head, Vec3 feet) {
    if (!finite(at))
        return false;
    const bool ground = finite(normal) && normal.y > .7f;
    auto segGap = [](Vec3 p, Vec3 a, Vec3 b) {
        const Vec3 ab = b - a;
        const float l2 = dot(ab, ab);
        const float t = l2 > 1e-6f ? std::clamp(dot(p - a, ab) / l2, 0.f, 1.f) : 0.f;
        return length(p - (a + ab * t));
    };
    for (uint32_t i = 0; chests && i < count; ++i) {
        if (!finite(chests[i]))
            continue;
        if (ground && at.y < chests[i].y - 1.f)
            continue; // the street under him
        if (segGap(at, chests[i] - Vec3{0, 1.2f, 0}, chests[i] + Vec3{0, .5f, 0}) < enemyRadius)
            return false;
    }
    if (finite(head) && finite(feet)) {
        if (ground && at.y < feet.y + .3f)
            return true; // what he stands on
        if (segGap(at, feet, head) < playerRadius)
            return false;
    }
    return true;
}

// ---------------------------------------------------------------------------------------------------
// Version 0.0.73 test 3 WEAPON COLLISION, HELD: a prop in the hand never shows sunk into a wall, a car
// or the ground. The hand's aim ray (the CONFIRMED bridge's tags 0/1, any body) says how far the
// surface is ahead of the hand; a prop reaching `reach` ahead of the hand that would poke into it is
// DRAWN pulled back along the aim (at most maxPush) - only the drawing: the hand, the aim, the swing's
// hits never move. Eased in and out (rate per second) so it never pops.
inline constexpr float maxPush = .6f, pushRate = 14.f;
// The push this prop wants (m along the aim, back toward the hand) with the surface `hit` m ahead of
// the hand (negative / not finite: nothing ahead).
inline float heldWant(float reach, float hit) {
    if (!std::isfinite(hit) || hit < 0 || !std::isfinite(reach) || reach <= 0)
        return 0;
    return std::clamp(reach + radius * .5f - hit, 0.f, std::min(maxPush, reach));
}
// One frame of the eased push toward `want`.
inline float heldEase(float now, float want, float dt) {
    if (!std::isfinite(now))
        now = 0;
    if (!std::isfinite(want))
        want = 0;
    const float a = 1.f - std::exp(-pushRate * std::clamp(std::isfinite(dt) ? dt : 0.f, 0.f, .1f));
    return std::clamp(now + (want - now) * a, 0.f, maxPush);
}

} // namespace spidy::prop_bounce
