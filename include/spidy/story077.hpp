#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
// HERO VR 0.0.77 STORY INPUTS: the inputs the story missions need that VR play could not send.
// Audit of 792386b (hero-0.0.77-story):
//   - THE HERO SWITCH (Peter <-> the second spider). The game's own switch is its FNSM app: open it
//     (PC pad: hold View/Back, CANDIDATE from the PC control lists; the D-pad shortcuts can be set to
//     "Open FNSM App" in the game's controls) and hold X there (the PS5's Square, PUBLISHED). VR play
//     never held Back (VR X tapped is a 0.1 s Back, X held is the gadget wheel) and never sent the
//     game's X outside a prompt it names, which SM2 does not read (game_prompts: 9699, not ported).
//     SwapMacro sends the game's own presses in order; nothing of the game is called or read beyond
//     the existing hero probe (hero_switch.hpp, HSWP), which says whether the hero was replaced.
//   - THE WEB THROW (the Sandman opener: "LB + RB (Hold) Throw" at the debris he throws). LB + RB went
//     out only from the right stick's click while the swing did not carry the player, and from the web
//     pull gesture on the ground at a solid thing the aim hit. TwoHandYank is a yank of both hands back
//     from in front, anywhere (the game picks what is highlighted); the click works in the air too.
//   - The prompts the game names cannot be read in SM2 yet, so every press here is the game's own
//     pad button; QTEs on the game screen (cutscenes, menus) already get the VR buttons 1:1.
// Pure and allocation-free; nothing here reads or writes the game. Made by Agent Mulder.
#include "math.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace spidy::story077 {

// The XInput bits (game_pad::Button; checked with static_asserts in game_xr.cpp).
namespace pad {
constexpr uint16_t dpadLeft = 0x4, dpadRight = 0x8, back = 0x20, lb = 0x100, rb = 0x200, b = 0x2000, x = 0x4000;
}

// live.txt story_swap: how the switch opens the game's FNSM app. 0 hold View (the default), 1 the
// D-pad left shortcut, 2 the D-pad right shortcut (either set to "Open FNSM App" in the game's
// controls), 3 off.
enum Route : uint32_t { viewHold = 0, dpadLeft = 1, dpadRight = 2, routeOff = 3 };
inline const char* routeName(uint32_t r) {
    static constexpr const char* names[] = {"HOLD VIEW", "D-PAD LEFT", "D-PAD RIGHT", "OFF"};
    return r <= routeOff ? names[r] : "OFF";
}

// The game's own switch as presses: open the app, wait, hold X, then watch for the hero's actor to
// be replaced; no change within `settle` closes the app with B (the switch not allowed now).
class SwapMacro {
  public:
    // CANDIDATE timings (the game's hold lengths are not measured; generous on purpose).
    static constexpr float openHold = 1.0f, openTap = .15f, gap = .9f, switchHold = 2.2f, settle = 3.0f,
                           closeTap = .15f;
    enum Phase : uint8_t { idle, opening, waiting, switching, settling, closing };
    enum Result : uint8_t { none, switched, closedNoChange, cancelled };
    // Starts unless one runs or the route is off. `marker`: the hero probe's count of replacements
    // and gaps now (any change from it says the hero went).
    bool start(uint32_t route, uint64_t marker) {
        if (phase_ != idle || route >= routeOff)
            return false;
        route_ = route;
        marker_ = marker;
        phase_ = opening;
        t_ = 0;
        ++starts;
        return true;
    }
    // Once a frame: the buttons to send (they replace the pad's while it runs). `allowed` false (no
    // headset, no session) cancels it.
    uint16_t update(float seconds, bool allowed, uint64_t marker) {
        if (phase_ == idle)
            return 0;
        const float dt = std::isfinite(seconds) ? std::clamp(seconds, 0.f, .1f) : 0.f;
        if (!allowed) {
            finish(cancelled);
            return 0;
        }
        if (phase_ >= switching && marker != marker_) {
            finish(switched);
            return 0;
        }
        t_ += dt;
        switch (phase_) {
        case opening: {
            const float hold = route_ == viewHold ? openHold : openTap;
            if (t_ < hold)
                return route_ == viewHold ? pad::back : route_ == dpadLeft ? pad::dpadLeft : pad::dpadRight;
            next(waiting);
            return 0;
        }
        case waiting:
            if (t_ >= gap)
                next(switching);
            return 0;
        case switching:
            if (t_ < switchHold)
                return pad::x;
            next(settling);
            return 0;
        case settling:
            if (t_ >= settle)
                next(closing);
            return 0;
        case closing:
            if (t_ < closeTap)
                return pad::b;
            finish(closedNoChange);
            return 0;
        case idle:
            break;
        }
        return 0;
    }
    // The player's pause (Start) ends it at once.
    void cancel() {
        if (phase_ != idle)
            finish(cancelled);
    }
    bool running() const {
        return phase_ != idle;
    }
    Phase phase() const {
        return phase_;
    }
    Result last() const {
        return last_;
    }
    // Each phase's change since the last call (for the timeline): true once per change.
    bool changed(Phase& seen) const {
        const bool c = seen != phase_;
        seen = phase_;
        return c;
    }
    uint32_t starts{}, switchedCount{}, closedCount{}, cancelledCount{};

  private:
    void next(Phase p) {
        phase_ = p;
        t_ = 0;
    }
    void finish(Result r) {
        last_ = r;
        switchedCount += r == switched;
        closedCount += r == closedNoChange;
        cancelledCount += r == cancelled;
        phase_ = idle;
        t_ = 0;
    }
    Phase phase_ = idle;
    Result last_ = none;
    uint32_t route_{};
    uint64_t marker_{};
    float t_{};
};
inline const char* phaseName(uint32_t p) {
    static constexpr const char* names[] = {"done", "opening the FNSM app", "waiting for the app",
                                            "holding X (switch)", "watching for the new hero", "closing the app (B)"};
    return p <= SwapMacro::closing ? names[p] : "";
}

// The switch's gesture: one hand laid flat on top of the head, still, for `hold` seconds, no trigger
// or grip (the other hand not up there too: both overhead is the FINISHER's start).
class HeadTouch {
  public:
    static constexpr float hold = 1.2f, cue = .5f, cooldown = 4.f, reach = .22f, low = .02f, high = .32f,
                           still = .45f, pressed = .3f;
    struct Out {
        bool fired{}, cue{};
        int hand = -1;
    };
    Out update(float seconds, bool play, Vec3 head, const Vec3 hands[2], const bool tracked[2],
               const float triggers[2], const float grips[2]) {
        Out out;
        const float dt = std::isfinite(seconds) ? std::clamp(seconds, 0.f, .1f) : 0.f;
        cool_ = std::max(0.f, cool_ - dt);
        bool on[2]{};
        for (int i = 0; i < 2; ++i) {
            float speed = 0;
            if (seen_[i] && dt > 1e-4f && tracked[i])
                speed = length(hands[i] - was_[i]) / dt;
            seen_[i] = tracked[i] && finite(hands[i]);
            if (seen_[i])
                was_[i] = hands[i];
            if (!play || !seen_[i] || !finite(head))
                continue;
            const Vec3 d = hands[i] - head;
            const float flat = std::hypot(d.x, d.z);
            on[i] = d.y >= low && d.y <= high && flat <= reach && triggers[i] < pressed && grips[i] < pressed &&
                    speed < still;
        }
        const int hand = on[0] && !on[1] ? 0 : on[1] && !on[0] ? 1 : -1;
        if (hand < 0 || hand != hand_ || cool_ > 0) {
            held_ = 0;
            cued_ = false;
            hand_ = hand;
            return out;
        }
        held_ += dt;
        if (held_ >= cue && !cued_) {
            cued_ = true;
            out.cue = true;
        }
        if (held_ >= hold) {
            out.fired = true;
            out.hand = hand;
            held_ = 0;
            cued_ = false;
            cool_ = cooldown;
            ++fires;
        }
        return out;
    }
    uint32_t fires{};

  private:
    Vec3 was_[2]{};
    bool seen_[2]{}, cued_{};
    int hand_ = -1;
    float held_{}, cool_{};
};

// The web throw's gesture: both hands held out in front (a moment: a punch's reach is no hold), then
// yanked back toward the chest together.
// It holds the game's LB + RB for `hold` seconds (its "(Hold) Throw"); the game picks the object.
class TwoHandYank {
  public:
    static constexpr float out = .30f, in = .24f, speed = 1.3f, window = .3f, remember = .5f, dwell = .3f,
                           hold = 1.0f, cooldown = 1.0f, pressed = .3f, smoothing = .35f;
    // `forward`: the head's look (levelled here). Returns whether LB + RB are held this frame.
    bool update(float seconds, bool play, Vec3 head, Vec3 forward, const Vec3 hands[2], const bool tracked[2],
                const float triggers[2], bool webAttached) {
        const float dt = std::isfinite(seconds) ? std::clamp(seconds, 0.f, .1f) : 0.f;
        now_ += dt;
        cool_ = std::max(0.f, cool_ - dt);
        if (held_ > 0) {
            held_ = std::max(0.f, held_ - dt);
            if (held_ > 0)
                return true;
        }
        Vec3 f{forward.x, 0, forward.z};
        const bool level = finite(f) && length(f) > 1e-3f;
        if (level)
            f = normalized(f);
        bool yanked[2]{};
        for (int i = 0; i < 2; ++i) {
            if (!tracked[i] || !finite(hands[i]) || !finite(head) || !level) {
                seen_[i] = false;
                outFor_[i] = 0;
                continue;
            }
            const float ahead = dot(hands[i] - head, f);
            if (seen_[i] && dt > 1e-4f) {
                const float v = -(ahead - ahead_[i]) / dt; // toward the body
                rate_[i] += (v - rate_[i]) * std::clamp(dt / (smoothing * .1f + dt), 0.f, 1.f);
            } else {
                rate_[i] = 0;
            }
            ahead_[i] = ahead;
            seen_[i] = true;
            outFor_[i] = ahead >= out ? outFor_[i] + dt : 0.f;
            if (outFor_[i] >= dwell)
                outAt_[i] = now_;
            const bool wasOut = now_ - outAt_[i] <= remember;
            if (wasOut && rate_[i] >= speed && ahead <= in && triggers[i] < pressed)
                yankAt_[i] = now_;
            yanked[i] = now_ - yankAt_[i] <= window;
        }
        if (play && !webAttached && cool_ <= 0 && yanked[0] && yanked[1]) {
            held_ = hold;
            cool_ = hold + cooldown;
            yankAt_[0] = yankAt_[1] = -1e9f;
            outAt_[0] = outAt_[1] = -1e9f;
            ++yanks;
            return true;
        }
        return false;
    }
    bool holding() const {
        return held_ > 0;
    }
    uint32_t yanks{};

  private:
    float ahead_[2]{}, rate_[2]{}, outFor_[2]{}, outAt_[2]{-1e9f, -1e9f}, yankAt_[2]{-1e9f, -1e9f};
    bool seen_[2]{};
    float now_{}, held_{}, cool_{};
};

#pragma pack(push, 1)
// The STRY record (update37.json story): one headset run says which story input reached the game.
struct Data {
    uint32_t magic = 0x59525453, version = 1, bytes = 96; // "STRY"
    uint32_t route{}, throwOn{};
    uint32_t swapStarts{}, swapFromPhone{}, swapFromGesture{}, swapSwitched{}, swapClosed{}, swapCancelled{};
    uint32_t swapPhase{}, swapLast{};
    uint32_t yanks{}, yanksInAir{}, clickPulls{}, clickPullsInAir{};
    uint32_t pullFrames{}, pullFramesInAir{};
    uint32_t headCues{}, refusedBusy{}, refusedOff{};
    uint64_t sequence{};
};
static_assert(sizeof(Data) == 96);
#pragma pack(pop)

} // namespace spidy::story077
