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
        return mostForward * smooth(hoverSpeed, fullForward, along);
    return -mostBack * smooth(hoverSpeed, fullBack, -along);
}

// Version 0.0.73 test 3: when the lean is wanted - FLIGHT POSE LEAN (option 0), FLY MODE on, in flight, not on a
// surface, and the swing bridge's velocity live.
inline bool wanted(uint32_t flyPose, uint32_t flyMode, uint32_t flying, bool standing, bool liveSwing) {
    return flyPose == 0 && flyMode != 0 && flying != 0 && !standing && liveSwing;
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
