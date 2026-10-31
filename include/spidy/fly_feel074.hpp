#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
// Made by Agent Mulder.
//
// 0.0.74 test 10 FLY FEEL (owner pt1424u, a player's comment: "the way it automatically starts flight instead of
// having a held button makes it so you maneuver kind of stiffly and if you hold your hand up when youre upside
// down or sideways you just go up"). Four live.txt switches (the wrist HUD is full):
// - FLY HOLD (fly_hold, ON): flight no longer starts by itself the moment a flyer is in the air. Hold a grip
//   `holdSeconds` in the air (on the ground: with a hand up, the RISE pose) to enter flight; once flying you
//   stay flying after letting go, and R3 / landing end it as before. OFF: the old auto start.
// - FLY STEER (fly_steer, ON): RISE and the right stick's climb go along the body's up (the tracking space's
//   up: FLY ROLL, the flips' tilt and LOOK UP included), not the world's, so a hand up while upside down or
//   sideways goes the way it looks; the hands' way is smoothed and the flight turns its velocity round
//   (keeping speed) instead of braking into each new way. OFF: world up, as before.
// - FLY FLIPS (fly_flips, ON): while FLY flies, REALISTIC FLIPS (only if the player has it on) respond more
//   readily: more strength and spin, a quicker response, lower speed and arm thresholds. The van / Batmobile
//   override (vehicle_flips) still turns them off while driving. OFF: the player's own rows.
// - FLY LOOK UP (fly_look_up, OFF): while flying, the view is pitched up `lookUpDegrees` (a mod-side tracking
//   space tilt like FLY ROLL, eased in and out), so a belly-down Superman pose sees ahead without craning.
// No game address is read or written: everything here is the mod's own state (CONFIRMED by construction; no
// CANDIDATE). The flight word's extra bits ride in game_swing::Command::reserved7[1] (unused until now): an
// older bridge ignores them, an older VR module sends 0 (the old flight exactly).
#include "spidy/math.hpp"
#include "spidy/realistic_flip.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace spidy::fly_feel {

// live.txt values: fly_hold / fly_steer / fly_flips 0 ON (the default), 1 OFF; fly_look_up 0 OFF (the default), 1 ON.
// Exports SpidyFlyHold 5548, SpidyFlySteer 5549, SpidyFlyFlips 5550, SpidyFlyLookUp 5551.
inline constexpr const char* holdHelp =
    "FLY HOLD ON: hold a grip for half a second in the air (or with a hand up on the ground) to start flying, "
    "then you stay flying when you let go; OFF starts flight by itself as before; live, no restart.";
inline constexpr const char* steerHelp =
    "FLY STEER ON: a raised hand and the right stick climb toward your body's up even upside down or sideways, "
    "and turns are smoother; OFF uses the world's up as before; live, no restart.";
inline constexpr const char* flipsHelp =
    "FLY FLIPS ON: your REALISTIC FLIPS react more readily while you fly (driving still turns them off); "
    "OFF keeps your own flip rows; live, no restart.";
inline constexpr const char* lookUpHelp =
    "FLY LOOK UP ON: while you fly your view tilts up as if you raised your head, so a belly-down pose sees "
    "ahead; OFF (the default) keeps your view as it is; live, no restart.";
inline bool onByDefault(uint32_t value) { return value != 1; } // fly_hold, fly_steer, fly_flips
inline bool offByDefault(uint32_t value) { return value == 1; } // fly_look_up

// ---- The flight's second word (game_swing::Command::reserved7[1]) ----
enum : uint32_t { bitHold = 1u, bitEnter = 2u, bitSteer = 4u };
inline uint32_t pack(bool holdOn, bool enter, bool steerOn) {
    return (holdOn ? bitHold : 0u) | (holdOn && enter ? bitEnter : 0u) | (steerOn ? bitSteer : 0u);
}
struct Word2 {
    bool hold{}, enter{}, steer{};
};
inline Word2 unpack(uint32_t w) {
    Word2 o;
    o.hold = (w & bitHold) != 0;
    o.enter = o.hold && (w & bitEnter) != 0;
    o.steer = (w & bitSteer) != 0;
    return o;
}

// ---- FLY HOLD: the grip held to enter flight ----
// A grip counts down past `press`, up again under `release` (hysteresis). Held `holdSeconds` while it may
// enter (in the air, or on the ground with the RISE pose), it asks to enter; the ask stays while the grip is
// still held and `keepSeconds` after, so the bridge sees it, and ends once flight has begun.
class EnterHold {
  public:
    static constexpr float holdSeconds = .4f, keepSeconds = .3f, press = .6f, release = .35f;
    // live: FLY MODE flies this hero now (flyLive); flying: the bridge's flight has him; airborne: in the air;
    // rise: the RISE pose (a hand up); grips: each controller's grip 0..1; counts: whether each hand's grip
    // may count (THOR's hammer hand: no).
    bool update(float seconds, bool live, bool flying, bool airborne, bool rise, const float (&grips)[2],
                const bool (&counts)[2]) {
        const float dt = std::isfinite(seconds) ? std::clamp(seconds, 0.f, .1f) : 0.f;
        bool down = false;
        for (int i = 0; i < 2; ++i) {
            const float g = std::isfinite(grips[i]) ? grips[i] : 0.f;
            down_[i] = counts[i] && g > (down_[i] ? release : press);
            down = down || down_[i];
        }
        if (!live || flying) {
            held_ = keep_ = 0;
            asking_ = false;
            return false;
        }
        const bool may = airborne || rise;
        held_ = down && may ? held_ + dt : 0.f;
        if (held_ >= holdSeconds) {
            if (!asking_)
                ++asks_;
            asking_ = true;
            keep_ = keepSeconds;
        } else if (asking_) {
            keep_ -= dt;
            if (keep_ <= 0 || !may)
                asking_ = false;
        }
        return asking_;
    }
    bool asking() const { return asking_; }
    float progress() const { return std::clamp(held_ / holdSeconds, 0.f, 1.f); }
    uint32_t asks() const { return asks_; }
    void reset() {
        held_ = keep_ = 0;
        asking_ = down_[0] = down_[1] = false;
    }

  private:
    float held_{}, keep_{};
    bool asking_{}, down_[2]{};
    uint32_t asks_{};
};

// ---- FLY STEER: the body's up and the smoothed way ----
// The body's up from the tracking space's turn (swing's trackingTurn: yaw * tilt); world up if unknown.
inline Vec3 bodyUp(Quat trackingTurn) {
    const Vec3 u = trackingTurn.rotate(Vec3{0, 1, 0});
    return finite(u) && length(u) > .5f ? normalized(u) : Vec3{0, 1, 0};
}
inline constexpr float steerSmoothSeconds = .10f; // the hands' way eased over this
inline constexpr float turnDegreesPerSecond = 140.f;
// The way `was` eased toward `want` (both unit, or zero: unknown) over steerSmoothSeconds.
inline Vec3 eased(Vec3 was, Vec3 want, float seconds) {
    if (!finite(want) || length(want) < .5f)
        return was;
    want = normalized(want);
    if (!finite(was) || length(was) < .5f)
        return want;
    const float dt = std::isfinite(seconds) ? std::clamp(seconds, 0.f, .1f) : 0.f;
    const Vec3 v = was + (want - was) * (1 - std::exp(-dt / steerSmoothSeconds));
    return length(v) > 1e-3f ? normalized(v) : want;
}
// The velocity `v` turned toward the way of `target` by at most turnDegreesPerSecond * dt (fewer at
// supersonic speed), its speed kept; straight back (no clear turn) or too slow: unchanged.
inline Vec3 turned(Vec3 v, Vec3 target, float seconds) {
    const float dt = std::isfinite(seconds) ? std::clamp(seconds, 0.f, .1f) : 0.f;
    const float speed = length(v);
    if (!finite(v) || !finite(target) || speed < 1.f || length(target) < 1.f || dt <= 0)
        return v;
    const Vec3 a = v / speed, b = normalized(target);
    const float c = std::clamp(dot(a, b), -1.f, 1.f);
    const float angle = std::acos(c);
    const Vec3 side = b - a * c;
    if (angle < 1e-4f || c < -.95f || length(side) < 1e-4f)
        return v;
    const float rate = turnDegreesPerSecond * (3.14159265f / 180.f) * std::min(1.f, 40.f / speed);
    const float step = std::min(angle, rate * dt);
    const Vec3 r = a * std::cos(step) + normalized(side) * std::sin(step);
    return finite(r) ? r * speed : v;
}

// ---- FLY FLIPS: REALISTIC FLIPS more ready while flying ----
// Kept inside vr_settings' ranges (strength 25-100, speed 90-360, minimum 3-16, response 100-750,
// hands 25-200, threshold 50-300). Off, or not flying, or REALISTIC FLIPS off: the rows as they are.
inline RealisticFlipSettings flying(RealisticFlipSettings s, bool flyingNow, bool on) {
    if (!on || !flyingNow || !s.enabled)
        return s;
    s.strength = std::clamp(s.strength * 3 / 2, 25, 100);
    s.speed = std::clamp(s.speed * 5 / 4, 90, 360);
    s.minimumSpeed = 3;
    s.responseMs = std::clamp(s.responseMs * 3 / 5, 100, 750);
    s.handStrength = std::clamp(s.handStrength * 3 / 2, 25, 200);
    s.handThreshold = std::clamp(s.handThreshold * 3 / 5, 50, 300);
    return s;
}

// ---- FLY LOOK UP ----
inline constexpr float lookUpDegrees = 30.f, lookUpEaseSeconds = .35f;
class LookUp {
  public:
    // Radians to pitch the view up this frame: eased toward lookUpDegrees while `want`, back to 0 otherwise.
    float update(float seconds, bool want) {
        const float dt = std::isfinite(seconds) ? std::clamp(seconds, 0.f, .1f) : 0.f;
        const float goal = want ? lookUpDegrees * (3.14159265f / 180.f) : 0.f;
        angle_ += (goal - angle_) * (1 - std::exp(-dt / lookUpEaseSeconds));
        if (!want && std::fabs(angle_) < .002f)
            angle_ = 0;
        if (!std::isfinite(angle_))
            angle_ = 0;
        return angle_;
    }
    float angle() const { return angle_; }
    void reset() { angle_ = 0; }

  private:
    float angle_{};
};
// The turn that pitches a view looking along level `ahead` up by `radians`: about its right (cross(ahead, up)),
// that right first rolled `roll` about `ahead` (FLY ROLL), so a rolled view tilts up its own way.
inline Quat pitchUp(Vec3 ahead, float radians, float roll = 0) {
    const Vec3 h{ahead.x, 0, ahead.z};
    if (!finite(h) || length(h) < .1f || !std::isfinite(radians) || radians == 0)
        return {};
    const Vec3 a = normalized(h);
    Vec3 right = cross(a, Vec3{0, 1, 0});
    if (std::isfinite(roll) && roll != 0)
        right = Quat::around(a, roll).rotate(right);
    return Quat::around(right, radians);
}

} // namespace spidy::fly_feel
