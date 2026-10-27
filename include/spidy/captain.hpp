#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
//
// Version 0.0.7 CAPTAIN AMERICA (hero_modes::captainAmerica): the shield.
//  - WORN on the SHIELD ARM's forearm (default RIGHT, LEFT selectable): the forearm through the elbow strap, the hand
//    on the hand strap, the face out (the films' 24 in / 0.61 m disc).
//  - HOLSTERED flat on the back between the shoulder blades, the face out, the star upright; drawn
//    and put back with the shared holster state machine (holster.hpp: the grip squeeze over the
//    shoulder, a haptic tick, never a web-zip or climb clash), only by the SHIELD ARM's hand.
//  - THROWN backhand like a frisbee (the trigger): it flies flat, spinning about its face, at the
//    enemy the aim assist picks (or along the hand's sweep), ricochets to the next enemy like the
//    DAREDEVIL batons (hero_modes::Baton's chainTarget; SHIELD CHAIN), then comes back to the hand.
//    Every hit is a throwHit blow on sm2_damage's CONFIRMED path (SHIELD DAMAGE, default 35 HP);
//    a swing with the shield on the arm is a bashHit (BatonSwing on its rim).
//  - INVINCIBLE: the shield itself never breaks and takes nothing (a look and a counter only).
//  - SHIELD BLOCK EXPERIMENTAL (CANDIDATE, OFF by default): a hit taken while the shield is on the
//    arm and its face points at the nearest enemy is refunded through the CONFIRMED HeroHealth
//    record (extras385::invincible's rules: never above the game's cap, never a revive). The game
//    does not say what hit you or from where, so "the hit contacted the shield" is a guess from the
//    nearest enemy's direction: CANDIDATE, never on unless the player picks it.
// Real-world numbers (research, docs/HERO-0.0.7.md): 0.61 m across, ~25 m/s thrown, ~8-10 turns a
// second, a near-elastic bounce. Pure; no game memory, no address.
#include "spidy/extras385.hpp"
#include "spidy/hero_modes.hpp"
#include "spidy/math.hpp"
#include "spidy/sm2_damage.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace spidy::captain {

using hero_modes::BatonEnemy;

inline constexpr float diameter = .61f;      // m (24 in)
inline constexpr float spinRevs = 8.f;       // turns a second in flight
inline constexpr float sweepThrow = 2.5f;    // m/s: a hand moving this fast throws along its sweep
inline constexpr float blockDeg = 60.f;      // the face's cover (half-angle) for SHIELD BLOCK
inline constexpr float blockRange = 30.f;    // m: the nearest enemy past this covers nothing
inline constexpr float bashHalf = .30f;      // m: the bash's rim segment, each side of the centre

// SHIELD DAMAGE values (combat38::shieldDamage): 35 HP (0, the default), 20, 50, 70.
inline float throwHp(uint32_t value) {
    static constexpr float hp[4] = {35.f, 20.f, 50.f, 70.f};
    return value < 4 ? hp[value] : 35.f;
}
// The blows: the CONFIRMED knock kind 5 (as batarangHit / clawHit), amounts the mod's own choice.
inline sm2_damage::Blow throwHit(uint32_t value) { return {throwHp(value), 5, 120.f}; }
inline sm2_damage::Blow bashHit() { return {18.f, 5, 150.f}; }
// SHIELD ARM (0 RIGHT, the default - the user, 26 Oct: "it should be on the right arm... or toggable
// between arms"; 1 LEFT): the hand index wearing it (0 the left hand, 1 the right). Anything else: RIGHT.
inline constexpr uint32_t armRight = 0, armLeft = 1;
inline uint32_t armHand(uint32_t value) { return value == armLeft ? 0u : 1u; }
inline bool chainOn(uint32_t value) { return value != 1; }    // SHIELD CHAIN: ON (0), OFF (1)
inline bool blockOn(uint32_t value) { return value == 1; }    // SHIELD BLOCK: OFF (0), EXPERIMENTAL (1)
inline bool lookModel(uint32_t value) { return value == 1; }  // SHIELD LOOK: BUILT-IN (0), MODEL (1)

// The throw's direction: a fast forward sweep of the hand (the backhand's follow-through) when
// there is one, else the hand's aim.
inline Vec3 throwAim(Vec3 handVelocity, Vec3 aim, Vec3 headForward) {
    const Vec3 a = finite(aim) && length(aim) > 1e-4f ? normalized(aim) : Vec3{0, 0, -1};
    if (!finite(handVelocity) || !finite(headForward))
        return a;
    const float speed = length(handVelocity);
    if (!(speed > sweepThrow))
        return a;
    const Vec3 sweep = handVelocity / speed;
    const Vec3 f = length(headForward) > 1e-4f ? normalized(headForward) : a;
    return dot(sweep, f) > .2f ? sweep : a;
}

// The shield model's frame (tools/models007/convert_shield.py): the hand strap at the origin, y
// along the forearm (elbow -> hand), x across, -z the face. The RIGHT arm's is the LEFT's turned
// half a turn about the straps' midpoint (pivot = the elbow strap): x' = px - x, y' = py - y.
inline constexpr Vec3 pivotLeft{0.f, -.21763f, 0.f};
inline constexpr Vec3 centreLeft{-.002f, -.113f, -.004f}; // the disc's middle (the bounding box's)
inline Vec3 forSide(Vec3 c, uint32_t side, Vec3 pivot = pivotLeft) {
    return side == 1 ? Vec3{pivot.x - c.x, pivot.y - c.y, c.z} : c;
}
// An item frame (hero007::Item: at, forward = the face, up = the forearm): right = forward x up.
struct ShieldPose {
    Vec3 at{}, face{0, 0, -1}, up{0, 1, 0};
    bool ok{};
};
inline Vec3 place(const ShieldPose& p, Vec3 c) {
    const Vec3 r = cross(p.face, p.up);
    return p.at + r * c.x + p.up * c.y - p.face * c.z;
}
inline Vec3 centreOf(const ShieldPose& p, uint32_t side) { return place(p, forSide(centreLeft, side)); }

// On the arm: the hand on the strap, up along the forearm (the controller's aim), the face toward
// where the player looks (square to the forearm), leaning out to the arm's side - Cap's guard.
inline ShieldPose onArm(Vec3 hand, Vec3 aim, Vec3 headForward, Vec3 bodyRight, uint32_t side) {
    ShieldPose p;
    if (!finite(hand) || !finite(aim) || !(length(aim) > 1e-4f))
        return p;
    p.up = normalized(aim);
    const Vec3 outSide = (side == 1 ? bodyRight : -bodyRight);
    Vec3 f = (finite(headForward) ? headForward : Vec3{0, 0, -1}) + (finite(outSide) ? outSide * .35f : Vec3{});
    f = f - p.up * dot(f, p.up);
    if (length(f) < 1e-3f) { // the forearm points where he looks: the face to the arm's side
        f = (finite(outSide) ? outSide : Vec3{1, 0, 0});
        f = f - p.up * dot(f, p.up);
    }
    if (length(f) < 1e-3f)
        return p;
    p.face = normalized(f);
    p.at = hand;
    p.ok = true;
    return p;
}
// Version 0.0.72 SHIELD CATCH (0.0.71 videos: the thrown shield "snapped" onto the arm): for
// catchBlend seconds after a catch the drawn shield eases from where it was caught (flat, its face
// up as it flew) onto the arm pose: smoothstep in place, the face turned over the same time.
// Version 0.0.72 SUBTLE RETICLE (0.0.71 videos: a "huge 3x3 blue reticle"): the right hand's target
// marker round an enemy is sized by his radius (about 3 m across, wider while it locks on). For
// CAPTAIN AMERICA it is held to reticleRadius and drawn at reticleOpacity; aiming is unchanged.
inline constexpr float reticleRadius = .15f, reticleOpacity = .5f;
inline float subtleRadius(float radius) {
    return std::isfinite(radius) && radius > 0 ? std::min(radius, reticleRadius) : reticleRadius;
}
inline constexpr float catchBlend = .12f;
inline ShieldPose catchPose(const ShieldPose& arm, Vec3 caughtAt, float since) {
    if (!arm.ok || !finite(caughtAt) || !(since >= 0) || since >= catchBlend)
        return arm;
    const float x = since / catchBlend, k = x * x * (3 - 2 * x);
    ShieldPose p = arm;
    p.at = caughtAt + (arm.at - caughtAt) * k;
    Vec3 f = Vec3{0, 1, 0} * (1 - k) + arm.face * k;
    if (length(f) < 1e-3f)
        f = arm.face;
    p.face = normalized(f);
    Vec3 u = arm.up - p.face * dot(arm.up, p.face);
    if (length(u) < 1e-3f) // the forearm along the flat shield's face (an arm held up): up toward the arm's face
        u = arm.face - p.face * dot(arm.face, p.face);
    if (length(u) < 1e-3f)
        return arm;
    p.up = normalized(u);
    return p;
}
// On the back: flat between the shoulder blades, the face backward, the star upright (the item's
// up is the body's left for the LEFT arm's model, its right for the RIGHT's), the disc's middle
// 0.40 m below and 0.22 m behind the eyes.
inline ShieldPose onBack(Vec3 head, Vec3 bodyForward, Vec3 bodyRight, uint32_t side) {
    ShieldPose p;
    if (!finite(head) || !finite(bodyForward) || !finite(bodyRight) || length(bodyForward) < 1e-3f || length(bodyRight) < 1e-3f)
        return p;
    const Vec3 f = normalized(bodyForward), r = normalized(bodyRight);
    p.face = -f;
    p.up = side == 1 ? r : -r;
    const Vec3 middle = head + Vec3{0, -.40f, 0} - f * .22f;
    const Vec3 c = forSide(centreLeft, side);
    p.at = middle - p.up * c.y - cross(p.face, p.up) * c.x + p.face * c.z; // place(p, c) == middle
    p.ok = true;
    return p;
}
// SHIELD BLOCK's guess that a hit met the shield: the nearest standing enemy within blockRange is
// inside blockDeg of the face (from the disc's middle).
inline bool covers(Vec3 centre, Vec3 face, const BatonEnemy* list, uint32_t count) {
    if (!list || !finite(centre) || !finite(face) || !(length(face) > 1e-4f))
        return false;
    int best = -1;
    float bestMetres = blockRange;
    for (uint32_t i = 0; i < count; ++i) {
        const float m = length(list[i].chest - centre);
        if (!list[i].id || list[i].down || !(m <= bestMetres) || !(m > 1e-3f))
            continue;
        best = static_cast<int>(i);
        bestMetres = m;
    }
    if (best < 0)
        return false;
    const Vec3 to = (list[best].chest - centre) / bestMetres;
    return dot(to, normalized(face)) >= std::cos(blockDeg * 3.14159265f / 180.f);
}
// SHIELD BLOCK EXPERIMENTAL (CANDIDATE): what to write into the CONFIRMED health record this frame
// (a negative number: nothing). A fall in health while `covered` is given back, up to the health
// before the hit and never above extras385::invincible::refill's cap; never a revive (0 stays 0);
// nothing when it is off or INVINCIBLE already refills.
struct Block {
    float hpWas = -1;
    uint64_t refunds{}, uncovered{};
    float step(float hp, float most, float reduce, bool on, bool covered, bool invincible) {
        if (!std::isfinite(hp)) {
            hpWas = -1;
            return -1;
        }
        float to = -1;
        if (on && !invincible && hpWas > 0 && hp > 0 && hp < hpWas - .01f) {
            if (covered) {
                const float cap = extras385::invincible::refill(hp, most, reduce);
                if (cap >= 0) {
                    const float back = std::min(cap, hpWas);
                    if (back > hp) {
                        to = back;
                        ++refunds;
                    }
                }
            } else {
                ++uncovered;
            }
        }
        hpWas = to >= 0 ? to : hp;
        return to;
    }
};

} // namespace spidy::captain
