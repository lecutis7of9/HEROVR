#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
// HERO VR 0.0.77 X IS THE GAME'S X (story (8)). 05e70a2 sent a 0.1 s View (back) for a VR X tap, so
// a story prompt's "tap X" / "rapidly press X" (the second spider hitting Sandman with debris) also
// opened the game menu View opens. Now, for the spiders:
//   - VR X tapped is the game's X (Square): XPulse, one short press per tap, mashing queued so each
//     tap is its own press with a release between. X held alone (no chord) is the game's X held, or
//     the mod's gadget wheel where the wheel is offered, as before.
//   - That menu moved to the menu (pause) button HELD about half a second: MenuHold sends the same
//     0.1 s View the X tap sent, with a short buzz, and that hold never pauses. A tap of the menu
//     button still pauses (on release now: a hold has to be told from a tap).
// live.txt x_button = view brings 05e70a2's X back (X tap View, menu pauses on press).
// Pure and allocation-free; nothing here reads or writes the game. Made by Agent Mulder.
#include <algorithm>
#include <cstdint>

namespace spidy::story077e {

// live.txt x_button (SpidyXButton): 0 game (the default), 1 view (05e70a2's X = View).
enum Mode : uint32_t { xGame = 0, xView = 1 };

// The game's X as short presses, one per VR X tap; taps during a press are queued (at most 4).
class XPulse {
public:
    static constexpr float press = .07f, gap = .04f;
    static constexpr uint32_t queueMax = 4;
    void tap() {
        ++taps;
        if (pending_ < queueMax) ++pending_;
    }
    // True while the game's X is down this frame.
    bool update(float seconds) {
        const float dt = std::clamp(seconds, 0.f, .1f);
        if (left_ > 0) {
            left_ -= dt;
            if (left_ <= 0) gap_ = gap;
            return true;
        }
        if (gap_ > 0) {
            gap_ -= dt;
            return false;
        }
        if (pending_) {
            --pending_;
            ++sent;
            left_ = press - dt;
            if (left_ <= 0) gap_ = gap;
            return true;
        }
        return false;
    }
    void reset() { left_ = gap_ = 0, pending_ = 0; }
    bool busy() const { return left_ > 0 || gap_ > 0 || pending_; }
    uint64_t taps{}, sent{};

private:
    float left_{}, gap_{};
    uint32_t pending_{};
};

// The menu button in VR play: a tap pauses (start, 0.1 s on release), a hold of holdSeconds sends
// the View tap X used to send (0.1 s) once, with a buzz, and never pauses. A press already down when
// it turns on waits for its release.
class MenuHold {
public:
    static constexpr float holdSeconds = .5f, startSeconds = .1f, viewSeconds = .1f;
    struct Out {
        bool start{}, view{}, buzz{};
    };
    Out update(float seconds, bool on, bool menu) {
        const float dt = std::clamp(seconds, 0.f, .1f);
        Out o;
        startLeft_ = std::max(0.f, startLeft_ - dt);
        viewLeft_ = std::max(0.f, viewLeft_ - dt);
        if (!on) {
            down_ = fired_ = false;
            held_ = 0;
            spent_ = menu;
            startLeft_ = viewLeft_ = 0;
            return o;
        }
        if (menu && !spent_) {
            if (!down_) down_ = true, held_ = 0, fired_ = false;
            held_ += dt;
            if (!fired_ && held_ >= holdSeconds) {
                fired_ = true;
                viewLeft_ = viewSeconds;
                o.buzz = true;
                ++views;
            }
        } else if (!menu) {
            if (down_ && !fired_) startLeft_ = startSeconds, ++pauses;
            down_ = fired_ = spent_ = false;
            held_ = 0;
        }
        o.start = startLeft_ > 0;
        o.view = viewLeft_ > 0;
        return o;
    }
    bool holding() const { return down_ && !fired_; }
    uint64_t pauses{}, views{};

private:
    float held_{}, startLeft_{}, viewLeft_{};
    bool down_{}, fired_{}, spent_{};
};

}  // namespace spidy::story077e
