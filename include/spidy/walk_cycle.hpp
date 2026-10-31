#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
//
// Version 0.0.74 test 11 WALK LEGS (the owner: "legs dont move when you walk its super goofy"). The worn suit's
// legs (suit_wear::estimate: the visible body and the mirror) and Player 2's (mp: the suit, the hero body and the
// stick figure) stood each foot under its rest place every frame, so walking - the stick or a real step in the
// room - slid a standing body along. This is a procedural step cycle driven by the ground speed:
//  - the ground speed is the eyes' level speed (world; the stick and room-scale alike), smoothed over 0.15 s; up or
//    down speed (a jump, a fall) and a one-frame jump (a teleport, a snap) never step;
//  - the cadence and the stride grow with the speed (0.75 - 1.9 cycles a second, two steps each: about 2 steps a
//    second at a walk; a half stride of speed / (4 cadence), at most 0.6 m), so a foot on the ground moves back
//    exactly as fast as the body goes forward: it stays planted (up to about 3.5 m/s, a jog; past the stride's cap
//    a run or a sprint slides a little);
//  - each foot: half the cycle on the ground (ahead to behind, planted), half in the air (lifted, swung ahead);
//    the feet half a cycle apart;
//  - eased in from 0.4 m/s to 1 m/s and back to the standing body (exactly the old one) when he stops, flies or
//    swings.
// The game body (native_body, GAME BODY on) keeps the game's own animated feet (body_ik: the feet from the game's
// animated pose while grounded - CONFIRMED path, its leg bones' addresses as before); this is only for the bodies
// the mod draws itself, which have no game animation to follow. Pure; no game memory.
#include "spidy/math.hpp"

#include <algorithm>
#include <cmath>

namespace spidy::walk_cycle {
inline constexpr float pi = 3.14159265f;
// Ground speeds (m/s): no steps under startSpeed, full steps from fullSpeed; faster than teleport in one frame: a jump.
inline constexpr float startSpeed = .4f, fullSpeed = 1.f, teleport = 15.f;
// The most up or down speed (m/s) that still walks (stairs, a slope, a bob; not a jump or a fall).
inline constexpr float mostClimb = 2.5f;
// The half stride's cap (m) and the most lift (m).
inline constexpr float mostHalfStride = .6f, mostLift = .22f;
// How long the speed is smoothed (s), and how quickly the steps ease in and out (a second).
inline constexpr float smoothing = .15f, ease = 6.f;

// Cycles a second (each foot one step) at a ground speed.
inline float cadence(float speed) { return std::clamp(.75f + .2f * speed, .75f, 1.9f); }
// The half stride (m): a planted foot moves back at the body's speed over half a cycle.
inline float halfStride(float speed) { return std::clamp(speed / (4 * cadence(speed)), 0.f, mostHalfStride); }
// The swinging foot's lift (m).
inline float lift(float speed) { return std::clamp(.06f + .03f * speed, .06f, mostLift); }
inline float smooth(float e0, float e1, float x) {
    const float t = std::clamp((x - e0) / (e1 - e0), 0.f, 1.f);
    return t * t * (3 - 2 * t);
}
// A foot at phase u (cycles; 0 - .5 on the ground, ahead to behind; .5 - 1 in the air, behind to ahead): how far
// ahead along the walk and how high, for half stride a and lift h.
inline void footAt(float u, float a, float h, float& ahead, float& high) {
    u -= std::floor(u);
    if (u < .5f) {
        ahead = a * (1 - 4 * u);
        high = 0;
    } else {
        const float w = (u - .5f) * 2;
        ahead = -a * std::cos(pi * w);
        high = h * std::sin(pi * w);
    }
}

struct Cycle {
    float phase{}, amount{}, speed{};
    Vec3 velocity{}, way{}, last{};
    bool have{};
    void reset() { *this = Cycle{}; }
    // This frame: the eyes (world), up (world), the seconds since the last, and whether he may walk (on the ground,
    // WALK LEGS on).
    void update(Vec3 eye, Vec3 up, float dt, bool walking) {
        if (!finite(eye) || !finite(up) || !(dt > 1e-4f) || dt > 1.f) {
            if (!finite(eye))
                reset();
            return;
        }
        if (!have) {
            last = eye, have = true;
            return;
        }
        const Vec3 v = (eye - last) * (1 / dt);
        last = eye;
        float climb = dot(v, up);
        Vec3 flat = v - up * climb;
        const bool jumped = !(length(v) < teleport);
        if (jumped)
            flat = {}, climb = 0;
        const float k = 1 - std::exp(-dt / smoothing);
        velocity = velocity + (flat - velocity) * k;
        speed = length(velocity);
        if (!std::isfinite(speed))
            return reset();
        if (speed > .05f)
            way = velocity * (1 / speed);
        const bool moving = walking && !jumped && std::fabs(climb) < mostClimb;
        const float want = moving ? smooth(startSpeed, fullSpeed, speed) : 0.f;
        amount += (want - amount) * (1 - std::exp(-dt * ease));
        if (want == 0 && amount < 1e-3f)
            amount = 0;
        if (amount > 0)
            phase = std::fmod(phase + cadence(speed) * dt, 1.f);
    }
    // Each foot's offset this frame (world; [0] the left, [1] the right; zero: the standing body), scaled by the
    // body's size against an adult's (1).
    void feet(Vec3 up, float scale, Vec3 out[2]) const {
        out[0] = out[1] = Vec3{};
        if (!(amount > 0) || !(length(way) > .5f) || !finite(up) || !(scale > 0))
            return;
        const float a = halfStride(speed) * scale * amount, h = lift(speed) * scale * amount;
        for (int i = 0; i < 2; ++i) {
            float ahead = 0, high = 0;
            footAt(phase + .5f * static_cast<float>(i), a, h, ahead, high);
            out[i] = way * ahead + up * high;
        }
    }
};
} // namespace spidy::walk_cycle
