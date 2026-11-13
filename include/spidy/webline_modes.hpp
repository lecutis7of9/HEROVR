#pragma once
// HERO VR 0.0.82 WEBLINE MODES (experimental test): other ways to ask for the game's own Web Line than the
// 0.0.81 grip-sweep-release gesture (webline_gesture.hpp, still mode GRIP and the default). Pure state machines:
// no game input is synthesized here and nothing is written to the game. The caller turns a `commit` into the
// same native request the grip gesture uses (webline_native.hpp: L2 held, then L2 + Y). The anchors / aim only
// GATE the request; the game decides where its own line goes from its own aim at that moment.
#include "spidy/math.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace spidy::webline_modes {
// live.txt webline_mode (SpidyWeblineMode): 0 grip (0.0.81's gesture), 1 wrists, 2 twohand.
enum Mode : uint32_t { modeGrip = 0, modeWrists = 1, modeTwoHand = 2 };
inline const char* modeName(uint32_t m) { return m == modeWrists ? "WRISTS" : m == modeTwoHand ? "TWO HAND" : "GRIP"; }

// ---- WRISTS: both wrists together for a short hold ----
constexpr float wristsRange = .12f;      // start: the two controllers within 12 cm
constexpr float wristsKeepRange = .17f;  // keep holding up to 17 cm (a hand's own shake never cancels)
constexpr float wristsWaitMax = 1.5f;    // a full ring with no valid surface waits this long, then cancels
constexpr uint32_t holdMsMin = 150, holdMsMax = 2000, holdMsDefault = 400; // live.txt webline_hold_ms
inline float holdSeconds(uint32_t ms) { return static_cast<float>(std::clamp(ms, holdMsMin, holdMsMax)) * .001f; }

struct WristsInput {
    float seconds{};
    bool enabled{};   // every gate (mode, master toggle, out of combat, not swinging, ...)
    bool tracked{};   // both hands tracked
    bool free{};      // both grips and both triggers released: a swing grip or a trigger gesture is never this
    Vec3 left{}, right{};
    bool valid{};     // the marker's validity: a hand's aim meets a surface a web holds on, in reach
    float holdSeconds{.4f};
};
enum class WristsWhy : uint8_t { none, separated, disabled, noSurface };
struct WristsOutput {
    bool together{}, active{}, commit{}, cancelled{}, ready{};
    float progress{}; // 0..1, the ring
    WristsWhy why{};
};
class Wrists {
  public:
    WristsOutput update(const WristsInput& in) {
        WristsOutput out{};
        const float dt = std::isfinite(in.seconds) ? std::clamp(in.seconds, 0.f, .1f) : 0.f;
        const bool finiteHands = finite(in.left) && finite(in.right);
        const float gap = finiteHands ? length(in.left - in.right) : 1e9f;
        const bool close = gap <= (active_ || done_ ? wristsKeepRange : wristsRange);
        const bool ok = in.enabled && in.tracked && in.free && finiteHands;
        out.together = ok && close;
        if (!ok || !close) {
            if (active_ && !done_) {
                out.cancelled = true;
                out.why = !in.enabled ? WristsWhy::disabled : WristsWhy::separated;
            }
            reset();
            return out;
        }
        if (done_) { // fired or cancelled for this meeting: the wrists must part before the next
            out.active = false;
            return out;
        }
        active_ = true;
        held_ += dt;
        const float hold = std::max(in.holdSeconds, .05f);
        out.active = true;
        out.progress = std::min(held_ / hold, 1.f);
        if (held_ >= hold) {
            out.ready = true;
            if (in.valid) {
                out.commit = true;
                done_ = true;
                out.active = false;
            } else if (held_ >= hold + wristsWaitMax) {
                out.cancelled = true;
                out.why = WristsWhy::noSurface;
                done_ = true;
                out.active = false;
            }
        }
        return out;
    }
    void reset() { active_ = false; done_ = false; held_ = 0; }
    bool active() const { return active_ && !done_; }
  private:
    bool active_{}, done_{};
    float held_{};
};

// ---- TWO HAND: one hand's shot places anchor A, the other hand's shot places B ----
constexpr float twoHandTimeout = 3.f;   // anchor A lapses after this long
constexpr float twoHandTapMax = .5f;    // a grip down and up within this long is a shot (longer is a swing grip)
constexpr float twoHandMinSpan = 2.f;   // A and B at least this far apart (the grip gesture's own minimum)

struct TwoHandInput {
    float seconds{};
    bool enabled{};
    bool shot[2]{};          // this frame: this hand's shot landed (a tap released on a surface a web holds on)
    Vec3 point[2]{};         // where that shot's aim met the surface
};
enum class TwoHandWhy : uint8_t { none, timeout, tooClose, disabled };
struct TwoHandOutput {
    bool anchored{}, placed{}, commit{}, cancelled{};
    int hand{-1};            // A's hand
    Vec3 a{}, b{};
    float remaining{};       // seconds until A lapses
    TwoHandWhy why{};
};
inline const char* twoHandWhyText(TwoHandWhy w) {
    switch (w) {
    case TwoHandWhy::timeout: return "the second shot did not come within 3 s";
    case TwoHandWhy::tooClose: return "the second spot was under 2 m from the first";
    case TwoHandWhy::disabled: return "WEBLINES were switched off or the hands were needed";
    default: return "ok";
    }
}
class TwoHand {
  public:
    TwoHandOutput update(const TwoHandInput& in) {
        TwoHandOutput out{};
        const float dt = std::isfinite(in.seconds) ? std::clamp(in.seconds, 0.f, .25f) : 0.f;
        if (!in.enabled) {
            if (anchored_) {
                out.cancelled = true;
                out.why = TwoHandWhy::disabled;
            }
            reset();
            return out;
        }
        if (anchored_) {
            age_ += dt;
            if (age_ > twoHandTimeout) {
                out.cancelled = true;
                out.why = TwoHandWhy::timeout;
                reset();
                return out;
            }
        }
        for (int h = 0; h < 2; ++h) {
            if (!in.shot[h] || !finite(in.point[h]))
                continue;
            if (!anchored_ || h == hand_) { // the first shot, or the same hand again: (re)place A
                anchored_ = true;
                hand_ = h;
                a_ = in.point[h];
                age_ = 0;
                out.placed = true;
            } else if (length(in.point[h] - a_) >= twoHandMinSpan) {
                out.commit = true;
                out.hand = hand_;
                out.a = a_;
                out.b = in.point[h];
                reset();
                return out;
            } else {
                out.cancelled = true;
                out.why = TwoHandWhy::tooClose;
                reset();
                return out;
            }
        }
        out.anchored = anchored_;
        out.hand = hand_;
        out.a = a_;
        out.remaining = anchored_ ? std::max(twoHandTimeout - age_, 0.f) : 0.f;
        return out;
    }
    void reset() { anchored_ = false; hand_ = -1; age_ = 0; a_ = {}; }
    bool anchored() const { return anchored_; }
  private:
    bool anchored_{};
    int hand_{-1};
    float age_{};
    Vec3 a_{};
};
} // namespace spidy::webline_modes
