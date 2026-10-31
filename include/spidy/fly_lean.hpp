#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
//
// Version 0.0.73 test 2 FLIGHT POSE (fix 27): a flying hero's body matches how he moves.
//  - Hovering or strafing (little speed along the body's facing): upright.
//  - Flying forward: leaning forward, to horizontal (Superman-style) at full speed.
//  - Flying backward: leaning back, a little.
//  - Blended smoothly by the speed along the facing (smoothstep), and eased over time (no snap).
// The lean is a turn of the body about the eyes (native_body::Command::tilt, the turn the flips already
// use: CONFIRMED path), about the body's level right axis, so the view never tilts. Up or down speed and
// sideways speed never lean it. FLIGHT POSE UPRIGHT: none (the 0.0.72 body). Pure; no game memory.
// Version 0.0.74 (fix 27's third try): every body the player can see leans - the worn suit hangs from the neck under
// the upright head (suit_wear::Tracked::lean; test 3 swung it about the eyes, level with the camera, so looking down
// found nothing) and Player 2 carries it (mp::PoseState::leanUp). Every input is the mod's own tracked pose and the
// swing bridge's velocity (CONFIRMED: the mod's own data); no new game address.
#include "spidy/math.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace spidy::fly_lean {
inline constexpr float pi = 3.14159265f;
// Speeds along the facing (m/s): upright under hoverSpeed, fully leaned from fullForward / fullBack.
inline constexpr float hoverSpeed = 1.5f, fullForward = 14.f, fullBack = 7.f;
// The most lean: forward to horizontal (90 degrees), back 25 degrees.
inline constexpr float mostForward = pi / 2, mostBack = 25 * pi / 180;
// Version 0.0.74 test 11 FLIGHT POSE (fix 27, fourth pass): "more with speed, fully horizontal at supersonic" - a
// cruise (FLY NORMAL, about 14 m/s) leans cruiseForward (70 degrees, Superman's), then the lean keeps growing with
// the speed to horizontal at sonicSpeed (FAST is 40 m/s, the SUPERSONIC dash 80 by default).
inline constexpr float cruiseForward = 70 * pi / 180, sonicSpeed = 60.f;
// How quickly the lean eases toward its target (a second).
inline constexpr float ease = 3.5f;

inline float smooth(float e0, float e1, float x) {
    const float t = std::clamp((x - e0) / (e1 - e0), 0.f, 1.f);
    return t * t * (3 - 2 * t);
}
// The target lean (radians, + forward, - back) for a world velocity and a body facing (world, any length).
inline float target(Vec3 velocity, Vec3 facing) {
    if (!finite(velocity) || !finite(facing))
        return 0;
    Vec3 f{facing.x, 0, facing.z};
    if (length(f) < 1e-3f)
        return 0;
    f = normalized(f);
    const float along = dot(Vec3{velocity.x, 0, velocity.z}, f);
    if (along >= 0)
        return cruiseForward * smooth(hoverSpeed, fullForward, along) +
               (mostForward - cruiseForward) * smooth(fullForward, sonicSpeed, along);
    return -mostBack * smooth(hoverSpeed, fullBack, -along);
}

// Version 0.0.73 test 3: when the lean is wanted - FLIGHT POSE LEAN (option 0), FLY MODE on, in flight, not on a
// surface, and the swing bridge's velocity live.
// Version 0.0.74 test 11 (fix 27, fourth pass): FLY's own "flying" is the word on the air - the tracking's SurfaceHold
// "standing" (CANDIDATE game surface read) only keeps the lean off when FLY says he is not flying, so a stale or
// wrong surface flag can no longer hold a flying body upright.
inline bool wanted(uint32_t flyPose, uint32_t flyMode, uint32_t flying, bool standing, bool liveSwing) {
    (void)standing;
    return flyPose == 0 && flyMode != 0 && flying != 0 && liveSwing;
}
// Version 0.0.73 test 3 (fix 27): the root cause of "the bodies still fly like they are standing straight up" -
// a worn suit (IRON MAN, SUPERMAN, ...) stands in for the game's body (suit_wear::gameBodyDrawn), which turns
// native_body off, so test 2's lean (native_body's tilt) never reached the body the player sees. This leans the
// suit's estimate the same way: its up and its feet turned about the eyes. The identity (upright): no change;
// anything not finite: no change.
inline void leanSuit(Quat lean, Vec3 eye, Vec3& up, Vec3& feet) {
    if (std::abs(lean.x) < 1e-6f && std::abs(lean.y) < 1e-6f && std::abs(lean.z) < 1e-6f)
        return;
    const float n = lean.x * lean.x + lean.y * lean.y + lean.z * lean.z + lean.w * lean.w;
    if (!std::isfinite(n) || std::abs(n - 1) > .01f || !finite(eye) || !finite(up) || !finite(feet))
        return;
    up = lean.rotate(up);
    feet = eye + lean.rotate(feet - eye);
}

// Version 0.0.74 (fix 27, why test 3 still looked upright): the lean as the body's up (world; +y upright), and back
// to the turn from upright (the shortest arc, which is the lean itself: it only ever turns about a level axis).
// Player 2 carries it this way (mp::PoseState::leanUp). Not finite, or too small: upright.
inline Vec3 upOf(Quat lean) {
    const Vec3 u = lean.rotate(Vec3{0, 1, 0});
    return finite(u) && length(u) > .5f ? normalized(u) : Vec3{0, 1, 0};
}
inline Quat fromUp(Vec3 upright, Vec3 leaned) {
    if (!finite(upright) || !finite(leaned) || !(length(upright) > .5f) || !(length(leaned) > .5f))
        return {};
    const Vec3 a = normalized(upright), b = normalized(leaned);
    if (dot(a, b) > .99999f || dot(a, b) < -.5f) // none, or more than 120 degrees: not a flight lean
        return {};
    return arc(a, b);
}
// A real lean: finite, unit, not the identity.
inline bool leaning(Quat q) {
    const float n = q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w;
    if (!std::isfinite(n) || std::abs(n - 1) > .01f)
        return false;
    return std::abs(q.x) >= 1e-6f || std::abs(q.y) >= 1e-6f || std::abs(q.z) >= 1e-6f;
}

class Lean {
public:
    // Each frame: flying (the hero is in flight and FLIGHT POSE is LEAN), his world velocity and facing.
    // Not flying eases back to upright. Returns the lean now (radians).
    float update(float seconds, bool flying, Vec3 velocity, Vec3 facing) {
        const float dt = std::isfinite(seconds) ? std::clamp(seconds, 0.f, .1f) : 0.f;
        const float want = flying ? target(velocity, facing) : 0.f;
        angle_ += (want - angle_) * std::min(1.f, ease * dt);
        if (!std::isfinite(angle_))
            angle_ = 0;
        angle_ = std::clamp(angle_, -mostBack, mostForward);
        if (finite(facing) && std::hypot(facing.x, facing.z) > 1e-3f)
            facing_ = normalized(Vec3{facing.x, 0, facing.z});
        return angle_;
    }
    float angle() const { return angle_; }
    // The body's turn (world axes, about the eyes): forward lean tips the head toward the facing and the
    // feet back. The identity while upright.
    Quat tilt() const {
        if (std::abs(angle_) < 1e-4f)
            return {};
        const Vec3 right{-facing_.z, 0, facing_.x}; // the facing's right (y up)
        return Quat::around(right, -angle_);
    }
    void reset() { angle_ = 0; }

private:
    float angle_{};
    Vec3 facing_{0, 0, -1};
};
} // namespace spidy::fly_lean
