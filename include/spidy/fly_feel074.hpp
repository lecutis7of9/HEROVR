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
// - FLY LOOK UP (fly_look_up, ON = the feature is there; the tester's final spec): a toggled view. A tap of Y while
//   flying (FLY took Y from the game's Web Wings) turns it on or off, with a haptic tick and a line; it starts OFF
//   every session. On, while flying, the view pitches up by as much as the flying body has pitched it down - full
//   when the body is parallel to the ground (he looks ahead, not at the street), none when upright - about the
//   view's level right axis (no roll, the heading kept), the head tracked on top. Each toggle is a smooth
//   cinematic pan (`panSeconds`, eased in and out), never a snap; landing pans back. live.txt OFF: no Y, no pan.
// No game address is read or written: everything here is the mod's own state (CONFIRMED by construction; no
// CANDIDATE). The flight word's extra bits ride in game_swing::Command::reserved7[1] (unused until now): an
// older bridge ignores them, an older VR module sends 0 (the old flight exactly).
#include "spidy/math.hpp"
#include "spidy/realistic_flip.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace spidy::fly_feel {

// live.txt values: fly_hold / fly_steer / fly_flips / fly_look_up 0 ON (the default), 1 OFF.
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
    "FLY LOOK UP ON (the default) lets a tap of Y while flying smoothly pan your view up to look ahead instead of "
    "down when your body lies flat (it starts off each session, another tap pans back), while OFF removes the Y "
    "switch; live, no restart.";
inline bool onByDefault(uint32_t value) { return value != 1; } // fly_hold, fly_steer, fly_flips
inline bool levelOn(uint32_t value) { return value == 0; } // fly_look_up enabled: 0 ON; 1 or any other value OFF

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

// ---- FLY LOOK UP (the toggled view) ----
inline constexpr float panSeconds = .6f, tapSeconds = .5f;
class Pan {
  public:
    // How far the view is panned up this frame, 0 .. 1: the progress runs toward `want` at 1 / panSeconds a
    // second and is eased (smoothstep), so it starts and ends gently; turned round half way, it goes back from
    // where it is. Exactly 0 and 1 at the ends; NaN-safe.
    float update(float seconds, bool want) {
        const float dt = std::isfinite(seconds) ? std::clamp(seconds, 0.f, .1f) : 0.f;
        progress_ += (want ? dt : -dt) / panSeconds;
        progress_ = std::isfinite(progress_) ? std::clamp(progress_, 0.f, 1.f) : 0.f;
        return weight();
    }
    float weight() const { return progress_ * progress_ * (3 - 2 * progress_); }
    float progress() const { return progress_; }
    void reset() { progress_ = 0; }

  private:
    float progress_{};
};
// The tracking space's tilt (in the rig's yaw frame) with the view turned `weight` of the way from where the
// tilt points `ahead` (the head's level ahead in the tracking space) to the horizon, worked out in the body's own
// frame so any heading, roll, upside down or on the back works: the target is the level heading of the view's
// ahead plus, as the view nears straight down or up, the level heading of the body's up (where the head's top
// points, so flat on the front or the back it looks the way the head leads), which is smooth through vertical
// with no gimbal flip. The angle is the view's lift to that level line - pi/2 lying flat, 0 upright (flatness),
// the view's roll is left alone. NaN-safe: anything unreadable returns the tilt.
inline Quat pitchUpTilt(Quat tilt, Vec3 ahead, float weight) {
    if (!std::isfinite(weight) || weight <= 0)
        return tilt;
    weight = std::min(weight, 1.f);
    const float n = std::sqrt(tilt.x * tilt.x + tilt.y * tilt.y + tilt.z * tilt.z + tilt.w * tilt.w);
    if (!std::isfinite(n) || n < 1e-6f)
        return tilt;
    const Quat q{tilt.x / n, tilt.y / n, tilt.z / n, tilt.w / n};
    ahead = {ahead.x, 0, ahead.z};
    const float al = length(ahead);
    if (!finite(ahead) || al < .1f)
        return tilt;
    const Vec3 w = q.rotate(ahead / al);       // the view's ahead in the yaw frame
    const Vec3 u = q.rotate(Vec3{0, 1, 0});    // the body's up (the head's top)
    if (!finite(w) || !finite(u))
        return tilt;
    // Level target: w's own heading, with the head's-top heading blended in by how far w points up or down (the
    // sign takes the head-first side both face down and face up); |h| >= ~0.7 for unit orthogonal w, u.
    Vec3 h{w.x - w.y * u.x, 0, w.z - w.y * u.z};
    const float hl = length(h);
    if (!finite(h) || hl < 1e-4f)
        return tilt;
    h = h / hl;
    const Vec3 axis = cross(w, h);
    const float s = length(axis), c = std::clamp(dot(w, h), -1.f, 1.f);
    if (!finite(axis) || s < 1e-6f)
        return tilt; // level already (h can't oppose w: a level w gives h = w)
    const float angle = std::atan2(s, c);
    if (!std::isfinite(angle) || angle <= 1e-5f)
        return tilt;
    const Quat lift = Quat::around(axis / s, angle * weight);
    const Quat r = lift * q;
    const float m = std::sqrt(r.x * r.x + r.y * r.y + r.z * r.z + r.w * r.w);
    if (!std::isfinite(m) || m < 1e-6f)
        return tilt;
    return {r.x / m, r.y / m, r.z / m, r.w / m};
}
// The in-game switch: a tap of Y (pressed and let go within `tapSeconds`) while `active` (FLY flying, the
// game not naming Y). A press that began while inactive, or held longer, never counts.
class YTap {
  public:
    bool update(float seconds, bool active, bool y) {
        const float dt = std::isfinite(seconds) ? std::clamp(seconds, 0.f, .1f) : 0.f;
        bool tap = false;
        if (!active) {
            spent_ = y;
            down_ = false;
        } else if (spent_) {
            spent_ = y;
        } else if (y) {
            held_ = down_ ? held_ + dt : 0.f;
            down_ = true;
            if (held_ > tapSeconds)
                spent_ = true, down_ = false;
        } else if (down_) {
            tap = held_ <= tapSeconds;
            down_ = false;
        }
        return tap;
    }
    void reset() { down_ = spent_ = false; held_ = 0; }

  private:
    float held_{};
    bool down_{}, spent_{};
};

} // namespace spidy::fly_feel
