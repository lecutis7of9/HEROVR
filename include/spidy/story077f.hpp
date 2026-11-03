#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
// HERO VR 0.0.77 HEAL ANYWHERE (story (9)). The second spider's crawl under the fallen ceiling (the
// walkthrough's 13:40): the game shows its Focus Bar card ("PRESS A TO CONTINUE") and wants its heal,
// the D-pad down. On that card the mod plays the game screen (Mapping::menus), where the right stick
// was only ever the game's right stick: no push of it healed there (game_pad::fromControllers). In VR
// play a heal push that leant aside on its way down was spent until the stick came back to rest
// (game_pad::StickPush), and nothing said why. Now:
//   - HealStick: the right stick held down (more down than aside, diagonals count) for heal_hold_ms
//     is the game's D-pad down for as long as it is held, at least 0.3 s a push - in VR play beside
//     StickPush, and on the game screen while the hero is hurt (a menu at full health is left alone).
//   - HealHint: hurt below 30% for 2 s with no heal, a line in the headset says how to heal (and
//     that a game card is closed with A), at most every 30 s.
// live.txt screen_heal = off keeps the game screen's right stick the game's own, as before.
// Pure and allocation-free; nothing here reads or writes the game. Made by Agent Mulder.
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace spidy::story077f {

// live.txt screen_heal (SpidyScreenHeal): 0 on (the default), 1 off.
enum Mode : uint32_t { screenHealOn = 0, screenHealOff = 1 };

class HealStick {
public:
    static constexpr float down = .6f, keep = .35f, minSend = .3f;
    // seconds: since the last update. live: the stick may heal now. x, y: the right stick, -1..1.
    // holdSeconds: heal_hold_ms. True while the game's D-pad down goes out.
    bool update(float seconds, bool live, float x, float y, float holdSeconds) {
        const float dt = std::isfinite(seconds) ? std::clamp(seconds, 0.f, .1f) : 0.f;
        if (!std::isfinite(x) || !std::isfinite(y))
            x = y = 0;
        const float hold = std::isfinite(holdSeconds) ? std::clamp(holdSeconds, .05f, 1.f) : .25f;
        const bool pushed = live && y <= -down && std::abs(x) <= -y;
        const bool kept = live && y <= -keep && std::abs(x) <= -y * 1.5f;
        ended_ = false;
        if (!sending_) {
            held_ = pushed ? held_ + dt : 0.f;
            if (held_ >= hold) {
                sending_ = true;
                sent_ = 0;
                ++sends;
            }
            return sending_;
        }
        sent_ += dt;
        if (!live || (!kept && sent_ >= minSend)) {
            sending_ = false;
            held_ = 0;
            ended_ = true;
            lastSeconds = sent_;
            longestSeconds = std::max(longestSeconds, sent_);
        }
        return sending_;
    }
    bool sending() const { return sending_; }
    // True on the update a send ended (lastSeconds is its length).
    bool ended() const { return ended_; }
    uint32_t sends{};
    float lastSeconds{}, longestSeconds{};

private:
    bool sending_{}, ended_{};
    float held_{}, sent_{};
};

class HealHint {
public:
    static constexpr float low = .3f, after = 2.f, every = 30.f;
    // fraction: health 0..1 (1 unknown). healing: the D-pad down goes out. True: show the hint now.
    bool update(float seconds, bool live, float fraction, bool healing) {
        const float dt = std::isfinite(seconds) ? std::clamp(seconds, 0.f, .1f) : 0.f;
        since_ += dt;
        const bool hurt = live && std::isfinite(fraction) && fraction < low && !healing;
        low_ = hurt ? low_ + dt : 0.f;
        if (low_ >= after && since_ >= every) {
            since_ = 0;
            ++shown;
            return true;
        }
        return false;
    }
    uint32_t shown{};

private:
    float low_{}, since_ = every;
};

inline constexpr const char* hint =
    "Heal: push the right stick down and hold it (the game's D-pad down). A game card on screen? Press A to "
    "close it, then heal";

} // namespace spidy::story077f
