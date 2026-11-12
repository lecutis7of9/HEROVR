#pragma once
// HERO VR 0.0.79.1 web-mechanics test: turn the webline gesture
// (webline_gesture.hpp) into Spider-Man 2's OWN Web Line input, the route the
// branch already sends for a manual Web Line (commit e8761e8: L2 aim + Y, see
// game_xr.cpp nativeWebLineAim and game_body_tests "Web Line must pass native
// L2 + Triangle"). Nothing here places a line or writes game memory: the game
// decides, from its own aim, whether and where its stealth Web Line goes. The
// gesture's two anchors (validated with the swing bridge's world rays) gate
// the request; they are NOT handed to the game.
//
// Timeline: while a gesture is held, L2 is held (the game's aim primes, as a
// player holding the left trigger would). A commit keeps L2 for `settle` (0.04 s), then
// holds Y with L2 for `pulse` (0.10 s), then L2 alone for `tail` (0.04 s), then releases. A
// cancel, or the gesture being switched off, releases L2 at once with no Y.
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace spidy::webline_native {
// live.txt webline_gesture (SpidyWeblineGesture): 0 on (perched, standing or on
// a wall: never while airborne, so mid-air grip swings are untouched), 1 off,
// 2 anywhere (airborne too).
enum Mode : uint32_t { modeOn = 0, modeOff = 1, modeAnywhere = 2 };
inline bool allowed(uint32_t mode, bool airborne) {
    return mode == modeAnywhere || (mode == modeOn && !airborne);
}
struct Pad {
    bool aim{}, y{};
};
class Driver {
  public:
    // 79.5: trimmed from .08 / .12 / .06 (0.26 s) - L2 is already held for the whole gesture, so the
    // release only needs a short settle before the Y pulse.
    static constexpr float settle = .04f, pulse = .10f, tail = .04f;
    // active: a gesture holds a grip now. commit / cancel: this frame's
    // gesture ending. enabled false drops everything at once.
    Pad update(float seconds, bool enabled, bool active, bool commit, bool cancel) {
        const float dt = std::isfinite(seconds) ? std::clamp(seconds, 0.f, .1f) : 0.f;
        if (!enabled) {
            reset();
            return {};
        }
        if (commit && (phase_ == Phase::idle || phase_ == Phase::priming)) {
            phase_ = Phase::settling;
            left_ = settle;
            ++sends_;
        } else if (cancel && (phase_ == Phase::priming || phase_ == Phase::idle)) {
            phase_ = Phase::idle;
        }
        if (phase_ == Phase::idle && active)
            phase_ = Phase::priming;
        if (phase_ == Phase::priming && !active && !commit)
            phase_ = Phase::idle;
        Pad pad{};
        switch (phase_) {
        case Phase::idle:
            break;
        case Phase::priming:
            pad.aim = true;
            break;
        case Phase::settling:
            pad.aim = true;
            if ((left_ -= dt) <= 0) {
                phase_ = Phase::firing;
                left_ = pulse;
            }
            break;
        case Phase::firing:
            pad.aim = pad.y = true;
            if ((left_ -= dt) <= 0) {
                phase_ = Phase::tailing;
                left_ = tail;
            }
            break;
        case Phase::tailing:
            pad.aim = true;
            if ((left_ -= dt) <= 0)
                phase_ = Phase::idle;
            break;
        }
        return pad;
    }
    void reset() {
        phase_ = Phase::idle;
        left_ = 0;
    }
    bool busy() const { return phase_ != Phase::idle; }
    uint64_t sends() const { return sends_; }

  private:
    enum class Phase { idle, priming, settling, firing, tailing };
    Phase phase_ = Phase::idle;
    float left_{};
    uint64_t sends_{};
};
} // namespace spidy::webline_native
