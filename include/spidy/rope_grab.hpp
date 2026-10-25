#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
//
// Version 0.0.3 ROPE GRAB (rope-grab-spec.md; EXPERIMENTAL, OFF by default): SpidyVR's grip rope.
// A grip with the hand aimed at an enemy 3-9 m away tethers him to that hand on a rigid pole (no
// sag); each hand has its own tether (two enemies at once, or both hands on one). The pole's far
// end is where the enemy should be: the hand's aim ray at the length it caught him, plus the hand's
// own movement scaled by STRENGTH, so pulling back drags, raising lifts, a sideways swing flings
// and a circle swings him round. A hard fling, a slam down, both hands smashed together or letting
// go releases him with the pole end's momentum.
//
// What moves the enemy in SM2: NOTHING YET. SpidyVR (SM1) wrote the flight velocity into the bot's
// BotStateFlungLocal each step; SM2's BotStateFlung / BotStateFlungLocal classes exist (RTTI,
// CANDIDATE vtables 0x530f108 / 0x5359708) but no SM2 enemy write is confirmed, so 0.0.3 tracks the
// tether, draws the rigid rope to the real enemy, sends the game's own yank on a release (the yank
// input is CONFIRMED to send; its effect on the enemy is the game's) and probes the bot's state
// (bot_probe) to find the flight velocity for the next build. Pure; no game memory.
#include "spidy/math.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace spidy::rope_grab {

inline constexpr float minRange = 3.f, maxRange = 9.f, coneDegrees = 12.f, maxHold = 8.f;
// fling and slam: the HAND's own speed (m/s), not the pole end's (a wrist turn swings a 9 m end at 100 m/s).
inline constexpr float flingSpeed = 5.f, slamDown = 3.5f, clapSpeed = 2.5f, clapGap = .35f, dropSpeed = 3.f;
enum Strength : uint32_t { normal = 0, low = 1, strong = 2, strengths = 3 };
inline constexpr const char* strengthNames[strengths] = {"NORMAL", "LOW", "STRONG"};
inline constexpr float strengthGain[strengths] = {1.6f, 1.f, 2.5f};
// Version 0.0.4 drag: the hand pulled back toward the body (dragSpeed, under the fling's speed): the
// per-hand "drag him to you", sent as the game's yank like a fling (the mod moves no enemy).
enum Release : uint32_t { none = 0, fling, slam, clap, letGo, lost, timeout, drag, releases };
inline constexpr const char* releaseNames[releases] = {"", "fling", "slam", "clap slam", "let go", "lost", "timeout", "drag"};
inline constexpr float dragSpeed = 2.5f;

// The enemy the hand aims at (index into `chests`), or -1: 3-9 m, within 12 degrees, the smallest
// angle first.
inline int pick(Vec3 hand, Vec3 aim, const Vec3* chests, int count) {
    const Vec3 a = normalized(aim);
    if (!finite(hand) || !finite(a) || length(a) < .5f || !chests)
        return -1;
    const float cosCone = std::cos(coneDegrees * 3.14159265f / 180.f);
    int best = -1;
    float bestCos = cosCone;
    for (int i = 0; i < count; ++i) {
        const Vec3 d = chests[i] - hand;
        const float m = length(d);
        if (!finite(d) || m < minRange || m > maxRange)
            continue;
        const float c = dot(d, a) / m;
        if (c >= bestCos) {
            bestCos = c;
            best = i;
        }
    }
    return best;
}

struct Tether {
    bool on{};
    int enemy = -1;      // the enemy's index this frame
    uint32_t id{};       // and his stable id
    float length{}, age{}, gain = 1.6f;
    Vec3 hand0{}, target{}, velocity{}, chest0{};
    Vec3 handWas{}, handVelocity{}; // the hand's smoothed velocity: what fling and slam read
    uint32_t why{};      // the last release's Release
    Vec3 releaseVelocity{};
    uint64_t attaches{}, releasesBy[releases]{};

    void attach(int e, uint32_t enemyId, Vec3 hand, Vec3 chest, uint32_t strength) {
        on = true;
        enemy = e;
        id = enemyId;
        length = std::clamp(length_(chest - hand), minRange, maxRange);
        age = 0;
        gain = strengthGain[strength < strengths ? strength : 0];
        hand0 = hand;
        chest0 = target = chest;
        velocity = handVelocity = {};
        handWas = hand;
        why = none;
        ++attaches;
    }
    // The pole's far end for this hand pose (aim: the hand's pointing axis).
    Vec3 end(Vec3 hand, Vec3 aim) const {
        const Vec3 a = normalized(aim);
        Vec3 extra = (hand - hand0) * (gain - 1.f); // STRENGTH: the hand's travel, amplified (at most 3 m more)
        const float m = length_(extra);
        if (m > 3.f)
            extra = extra * (3.f / m);
        return hand + a * length + extra;
    }
    // One frame: returns the Release this frame (none while held).
    uint32_t update(float dt, bool gripHeld, bool enemyPresent, Vec3 hand, Vec3 aim) {
        if (!on)
            return none;
        age += dt;
        if (!enemyPresent)
            return stop(lost);
        if (!finite(hand) || !finite(aim) || dt <= 0)
            return none;
        const Vec3 next = end(hand, aim);
        if (!finite(next))
            return none;
        const Vec3 v = (next - target) / dt;
        velocity = velocity + (v - velocity) * std::min(1.f, dt / .05f); // ~50 ms smoothing
        target = next;
        const Vec3 hv = (hand - handWas) / dt;
        handWas = hand;
        if (finite(hv) && length_(hv) < 30.f) // a tracking jump is not a yank
            handVelocity = handVelocity + (hv - handVelocity) * std::min(1.f, dt / .06f);
        if (!gripHeld)
            return stop(letGo);
        if (age > .25f && -handVelocity.y > slamDown)
            return stop(slam);
        if (age > .25f && length_(handVelocity) > flingSpeed)
            return stop(fling);
        // Pulled back toward the body: the hand's speed along the rope, from the enemy to the hand.
        if (age > .25f) {
            const Vec3 back = hand - chest0;
            const float m = length_(back);
            if (m > 1e-3f && dot(handVelocity, back / m) > dragSpeed)
                return stop(drag);
        }
        if (age > maxHold)
            return stop(timeout);
        return none;
    }
    uint32_t stop(uint32_t reason) {
        if (!on)
            return none;
        on = false;
        why = reason;
        releaseVelocity = handVelocity; // the hand's: a wrist turn is not a throw
        if (reason < releases)
            ++releasesBy[reason];
        enemy = -1;
        return reason;
    }

private:
    static float length_(Vec3 v) {
        return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    }
};

// Both hands smashed together (each holding an enemy): closing faster than clapSpeed to within clapGap.
inline bool clapped(Vec3 left, Vec3 right, Vec3 leftVelocity, Vec3 rightVelocity) {
    const Vec3 d = right - left;
    const float m = length(d);
    if (!(m < clapGap) || m <= 1e-4f)
        return false;
    const float closing = -dot(rightVelocity - leftVelocity, d / m);
    return closing > clapSpeed;
}
// Where the enemy should be with both hands on him (or one): the ends averaged.
inline Vec3 heldTarget(const Tether& l, const Tether& r) {
    if (l.on && r.on && l.id == r.id)
        return (l.target + r.target) * .5f;
    return l.on ? l.target : r.target;
}
// A release worth the game's yank: fling, slam, clap or drag, or letting go of a moving pole.
inline bool yanks(uint32_t why, Vec3 releaseVelocity) {
    return why == fling || why == slam || why == clap || why == drag || (why == letGo && length(releaseVelocity) > dropSpeed);
}
} // namespace spidy::rope_grab
