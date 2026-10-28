#pragma once
// Version 0.0.73 UNIVERSAL GRAPPLE (the user's fix list: "every grappling hero draws or holds the device
// with grip, then trigger fires"; fixes 3, 5, 6, 17, 24). Pure; no game memory, no address.
//  - FireGate: per hand, the GRIP holds the device (ready) and a trigger PRESS that STARTS while ready
//    and not on the hologram / phone fires; it stays fired while the trigger is held (the bridge's
//    pull fires once, on the ask's rising edge). A press that began anywhere else (a holo click, the
//    device not out yet) never fires, not even when the grip comes after it - so the grapple never fires
//    on every trigger press and never on a holo click (fix 3).
//  - TapOrHold (DAREDEVIL, fix 5): his trigger throws the baton on a TAP (let go inside holdSeconds) and
//    fires the grapple along the baton on a HOLD (held past holdSeconds; the throw is then skipped).
//  - Enemy reel (fix 6): a grapple aimed at an enemy (enemyOnLine) closes to reelGap (about 2 ft) of him.
//    NO game write moves an enemy in SM2 (rope_grab.hpp: BotStateFlung is CANDIDATE, unconfirmed), so the
//    reel is the pull of the hero to 2 ft of the enemy along the line; the enemy is never touched.
//  - ArmPump (CAPTAIN AMERICA SUPER SPRINT, fix 17): pumping the arms like a sprinter (the hands swinging
//    up and down in turn) with the left stick pushed runs at sprintSpeed (the van's CONFIRMED carry
//    velocity path, game_swing Command v7). Stops the moment the pumping or the stick stops.
#include "spidy/math.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace spidy::grapple073 {

inline bool fineV(Vec3 v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }
inline constexpr float triggerPress = .6f, triggerRelease = .35f;
inline constexpr float gripPress = .6f, gripRelease = .35f;

// A control with hysteresis (pressed past `on`, let go under `off`).
inline bool held(float v, bool was, float on, float off) {
    const float x = std::isfinite(v) ? v : 0.f;
    return x >= on || (was && x > off);
}

class FireGate {
  public:
    // ready: the device is out in this hand (its grip held); blocked: the trigger is the hologram's,
    // the phone's or a menu's this frame. Returns true while this hand fires.
    bool step(unsigned hand, bool ready, float trigger, bool blocked) {
        if (hand > 1)
            return false;
        const bool down = held(trigger, down_[hand], triggerPress, triggerRelease);
        const bool press = down && !down_[hand];
        down_[hand] = down;
        if (!ready || blocked || !down) {
            firing_[hand] = false;
            return false;
        }
        if (press)
            firing_[hand] = true;
        if (firing_[hand] && press)
            ++fires_;
        return firing_[hand];
    }
    bool firing(unsigned hand) const { return hand < 2 && firing_[hand]; }
    uint64_t fires() const { return fires_; }
    void reset() { *this = FireGate{}; }

  private:
    bool down_[2]{}, firing_[2]{};
    uint64_t fires_{};
};

// DAREDEVIL's trigger: a tap throws, a hold grapples.
inline constexpr float holdSeconds = .3f;
class TapOrHold {
  public:
    struct Out {
        bool tap{};   // the one frame a tap let go (throw the baton now)
        bool hold{};  // held past holdSeconds: the grapple fires (true every frame while held)
    };
    // armed: the baton is in this hand (drawn); blocked: the trigger is the hologram's this frame.
    Out step(unsigned hand, float dt, bool armed, float trigger, bool blocked) {
        Out o;
        if (hand > 1)
            return o;
        const float t = std::isfinite(dt) ? std::clamp(dt, 0.f, .1f) : 0.f;
        const bool down = held(trigger, down_[hand], triggerPress, triggerRelease);
        const bool press = down && !down_[hand];
        down_[hand] = down;
        if (press)
            counting_[hand] = armed && !blocked; // a press that began on the holo or unarmed is nobody's
        if (!counting_[hand])
            return o;
        if (!armed || blocked) {
            counting_[hand] = false;
            held_[hand] = 0;
            return o;
        }
        if (down) {
            held_[hand] += t;
            o.hold = held_[hand] >= holdSeconds;
            return o;
        }
        o.tap = held_[hand] < holdSeconds; // let go
        counting_[hand] = false;
        held_[hand] = 0;
        return o;
    }
    void reset() { *this = TapOrHold{}; }

  private:
    bool down_[2]{}, counting_[2]{};
    float held_[2]{};
};

// The direction along a held baton (butt to tip), else the controller aim.
inline Vec3 batonAim(bool shaftOk, Vec3 butt, Vec3 tip, Vec3 aim) {
    if (shaftOk && fineV(butt) && fineV(tip) && length(tip - butt) > .05f)
        return normalized(tip - butt);
    return fineV(aim) && length(aim) > 1e-3f ? normalized(aim) : Vec3{0, 0, -1};
}

// ---- Enemy reel (fix 6) ----
inline constexpr float reelGap = .6f;          // about 2 ft
inline constexpr float reelCone = .9962f;      // cos 5 degrees (the line on the enemy)
inline constexpr float reelLeast = 1.5f, reelMost = 30.f;
inline constexpr float enemyRadius = .45f;
// The enemy (feet positions; their chest about chestUp over them) the line from `from` along `dir`
// is on: within reelCone of his chest or passing within enemyRadius of his body; nearest first.
// limit: nothing at or past this distance (a wall the ray hit first). -1: none.
inline constexpr float chestUp = 1.2f;
inline int enemyOnLine(Vec3 from, Vec3 dir, const Vec3* feet, uint32_t count, float limit) {
    if (!fineV(from) || !fineV(dir) || length(dir) < .5f || !feet)
        return -1;
    const Vec3 d = normalized(dir);
    int best = -1;
    float bestT = std::min(std::isfinite(limit) ? limit : reelMost, reelMost);
    for (uint32_t i = 0; i < count; ++i) {
        if (!fineV(feet[i]))
            continue;
        const Vec3 chest = feet[i] + Vec3{0, chestUp, 0};
        const Vec3 r = chest - from;
        const float t = dot(r, d);
        const float m = length(r);
        if (t < reelLeast || t >= bestT || m < 1e-3f)
            continue;
        // the body as a vertical segment feet..head: the line's closest approach to it
        const Vec3 p = from + d * t;
        const float yOff = std::max(0.f, std::max(feet[i].y - p.y, p.y - (feet[i].y + 1.9f)));
        const float flat = std::hypot(p.x - chest.x, p.z - chest.z);
        if (dot(r, d) / m >= reelCone || (flat <= enemyRadius && yOff <= .1f)) {
            best = static_cast<int>(i);
            bestT = t;
        }
    }
    return best;
}
// Where the hero stops: reelGap short of the enemy, on the level line from him to the hero.
inline Vec3 reelStop(Vec3 body, Vec3 enemyFeet) {
    if (!fineV(body) || !fineV(enemyFeet))
        return body;
    Vec3 flat{body.x - enemyFeet.x, 0, body.z - enemyFeet.z};
    const float m = length(flat);
    flat = m > 1e-3f ? flat / m : Vec3{0, 0, 1};
    return enemyFeet + flat * reelGap;
}

// ---- CAPTAIN AMERICA SUPER SPRINT (fix 17) ----
inline constexpr float pumpSpeed = 1.1f;     // m/s a hand swings up or down to count
inline constexpr float pumpWindow = .7f;     // s: the swings must alternate within this
inline constexpr float pumpKeep = .45f;      // s: the sprint keeps on this long after the last swing
inline constexpr int pumpsToStart = 3;
inline constexpr float sprintSpeed = 13.f;   // m/s (game_swing::carryMost is 20)
inline constexpr float sprintStick = .5f;
class ArmPump {
  public:
    // y[2]: each hand's height (tracking space; up is +y); stick: the left stick's push 0..1.
    bool step(float dt, const float* y, const bool* tracked, float stick, bool allowed) {
        const float t = std::isfinite(dt) ? std::clamp(dt, 0.f, .1f) : 0.f;
        since_ = std::min(since_ + t, 10.f);
        bool swung = false;
        for (unsigned i = 0; i < 2; ++i) {
            if (!tracked[i] || !std::isfinite(y[i])) {
                have_[i] = false;
                continue;
            }
            if (have_[i] && t > 1e-4f) {
                const float v = (y[i] - lastY_[i]) / t;
                vel_[i] = vel_[i] * .5f + v * .5f;
                const int dir = vel_[i] > pumpSpeed ? 1 : vel_[i] < -pumpSpeed ? -1 : 0;
                if (dir != 0 && dir != lastDir_[i]) {
                    lastDir_[i] = dir;
                    // a swing of this hand while the other swings the other way: a pump
                    if (have_[1 - i] && vel_[1 - i] * static_cast<float>(dir) < -.5f * pumpSpeed) {
                        swung = true;
                    }
                }
            }
            lastY_[i] = y[i];
            have_[i] = true;
        }
        if (swung) {
            pumps_ = since_ <= pumpWindow ? pumps_ + 1 : 1;
            since_ = 0;
        }
        const bool stickOn = std::isfinite(stick) && stick >= sprintStick;
        if (!allowed || !stickOn || since_ > (on_ ? pumpKeep : pumpWindow)) {
            if (since_ > pumpWindow || !allowed)
                pumps_ = 0;
            on_ = false;
            return false;
        }
        if (pumps_ >= pumpsToStart)
            on_ = true;
        return on_;
    }
    bool on() const { return on_; }
    void reset() { *this = ArmPump{}; }

  private:
    float lastY_[2]{}, vel_[2]{};
    bool have_[2]{};
    int lastDir_[2]{};
    int pumps_{};
    float since_{10.f};
    bool on_{};
};
// The sprint's world velocity: the left stick's push (x right, y forward) on the look's level heading.
inline Vec3 sprintVelocity(Vec3 look, float stickX, float stickY) {
    Vec3 f{look.x, 0, look.z};
    if (!fineV(f) || length(f) < 1e-3f || !std::isfinite(stickX) || !std::isfinite(stickY))
        return {};
    f = normalized(f);
    const Vec3 r{-f.z, 0, f.x};
    Vec3 w = f * stickY + r * stickX;
    const float m = length(w);
    if (m < 1e-3f)
        return {};
    return w / m * sprintSpeed;
}

} // namespace spidy::grapple073
