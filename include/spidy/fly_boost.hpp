#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
//
// Version 0.0.5 SUPERSONIC (COMBAT page, FLY MODE's): flying with both hands out (FLY's FORWARD
// pose), push the left stick forward and you dash at SUPERSONIC SPEED times FLY's FAST speed. The
// moment it kicks in: a shockwave (a ring sweeping past you and a vapour cone) and the sonic boom.
// The VR side reads the stick and the pose (Boost) and asks the swing module through two spare
// bits of the command's `asks` (asksOf); the swing module's flight (extras385::fly::Flight) flies
// it, never past supersonicTop, the movement module's own ceiling, nor 2 m a game step.
// Nothing else in FLY MODE changes: let go of the stick (or drop the pose) and FLY is as it was.
#include "spidy/combat38.hpp"
#include "spidy/extras385.hpp"
#include "spidy/native_movement.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace spidy::fly_boost {
static_assert(extras385::fly::supersonicTop <= native_movement::motionCeiling, "the movement module refuses it");
inline constexpr float stickOn = .6f, stickOff = .4f; // the left stick's forward, with hysteresis
inline constexpr float rearmSeconds = 1.f;            // off this long before another boom
inline constexpr float shockSeconds = .9f;            // the shockwave's life
// 0.0.74 test 11: the dash holds this long while the hands-out pose blinks (a hand's tracking drops
// out, a hand dips) with the stick still forward - it used to end at once and the flight braked.
inline constexpr float graceSeconds = extras385::fly::Flight::dashGraceSeconds;
// Command::asks: bit 0 the game's own move (as before), bit 1 SUPERSONIC now, bits 2-3 its speed.
inline constexpr uint32_t asksBit = 2u, asksMask = 15u;
inline uint32_t asksOf(bool engaged, uint32_t speedValue) {
    return engaged ? asksBit | (std::min(speedValue, 3u) << 2) : 0u;
}
// What the swing module's flight multiplies FAST by (0: no dash).
inline float multiplierOf(uint32_t asks) {
    return (asks & asksBit) ? combat38::supersonicK((asks >> 2) & 3u) : 0.f;
}
inline float speedOf(uint32_t speedValue) {
    return std::min(extras385::fly::speeds[extras385::fly::fast] * combat38::supersonicK(speedValue),
                    extras385::fly::supersonicTop);
}
struct In {
    float seconds{};
    bool on{};       // SUPERSONIC ON, FLY MODE on and live (no menu, pad layer or phone)
    bool flying{};   // the swing module flies the hero now
    bool handsOut{}; // FLY's FORWARD pose (both hands out in front)
    float stickY{};  // the left stick, + forward
};
class Boost {
  public:
    // True while the dash is asked for; boomed() the frame it kicks in.
    bool update(const In& in) {
        const float dt = std::isfinite(in.seconds) ? std::clamp(in.seconds, 0.f, .1f) : 0.f;
        const float y = std::isfinite(in.stickY) ? in.stickY : 0.f;
        const bool can = in.on && in.flying && in.handsOut;
        bool want = can && (engaged_ ? y > stickOff : y > stickOn);
        const bool hold = !want && engaged_ && in.on && in.flying && !in.handsOut && y > stickOff &&
                          grace_ + dt <= graceSeconds + 1e-5f;
        grace_ = hold ? grace_ + dt : 0.f;
        want = want || hold;
        holding_ = hold;
        boomed_ = false;
        if (want && !engaged_) {
            if (offFor_ >= rearmSeconds) {
                boomed_ = true;
                ++engages_;
            }
        }
        engaged_ = want;
        offFor_ = engaged_ ? 0.f : std::min(offFor_ + dt, 60.f);
        return engaged_;
    }
    bool engaged() const { return engaged_; }
    bool boomed() const { return boomed_; }
    bool holding() const { return holding_; } // engaged on the grace (the pose blinked)
    uint32_t engages() const { return engages_; }
    void reset() {
        engaged_ = boomed_ = holding_ = false;
        grace_ = 0;
        offFor_ = rearmSeconds;
    }

  private:
    bool engaged_{}, boomed_{}, holding_{};
    float grace_{};
    float offFor_ = rearmSeconds;
    uint32_t engages_{};
};
// The shockwave's strength, 1 at the boom to 0 at shockSeconds (0 outside).
inline float shock(float age) {
    if (!std::isfinite(age) || age < 0 || age >= shockSeconds)
        return 0.f;
    return 1.f - age / shockSeconds;
}
} // namespace spidy::fly_boost
