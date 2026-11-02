#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
// Made by Agent Mulder.
//
// HERO VR 0.0.76 BALL GAME (the owner after 8046cd4: "Do not revert back to ours they are awful"). SPIDER-MAN's web
// ball is only ever the GAME's: the mod never fires or draws its own ball or cocoon for him. Every press goes to the
// game as an RB on the press frame (combat_input.hpp Config::ballInstant), in combat or not; the game decides.
//
// Why the game fires nothing out of combat (test 801ce57: every one of 98 presses reached the game as an RB tap, yet
// "only in combat"): the RB arrives, so it is not this port's routing. The game's own web shooter is picked by its
// state (the way its traversal selector refuses a move outside the states it allows); out of combat it does not pick
// it. This port knows no CONFIRMED SM2 function that fires the game's ball or lifts that gate, so none is called and
// nothing is written. NOT YET POSSIBLE out of combat: there, a press does nothing visible.
//
// What this file does: a READ-ONLY watch. Per press it notes in or out of combat, then whether the wrist muzzle hook
// (wrist_muzzle.hpp, CANDIDATE, already in place) saw the web shooter ask for its muzzle within `waitSeconds` - i.e.
// whether the game started a shot at all. The counts go to the log so the next test says, from the game itself,
// whether an out-of-combat RB ever starts a shot. Nothing here reads or writes game memory or fires anything.
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace spidy::ball_sure {

inline constexpr float combatMetres = 30.f, waitSeconds = .30f;

// The nearest enemy (metres, 1e9 / NaN unknown) and how many the enemy list has: out of combat.
inline bool outOfCombat(uint32_t enemyCount, float nearestMetres) {
    return enemyCount == 0 || !std::isfinite(nearestMetres) || nearestMetres > combatMetres;
}

struct In {
    float seconds{};
    bool start[2]{};      // the combat machine's ballStart: the game's RB for this hand went down this frame
    bool outOfCombat{};   // outOfCombat(...) this frame
    bool hooked{};        // wrist_muzzle hooked (status().hooked != 0)
    uint64_t webCalls{};  // wrist_muzzle status().webCalls (the web shooter's muzzle asks so far)
};
// Per side: [0] in combat, [1] out of combat.
struct Counts {
    uint32_t presses{}, shots{}, unseen{}, unhooked{};
};

class Sure {
  public:
    void update(const In& in) {
        const float dt = std::isfinite(in.seconds) ? std::clamp(in.seconds, 0.f, .1f) : 0.f;
        if (!primed_) { // the count so far is not this press's
            primed_ = true;
            lastCalls_ = in.webCalls;
        }
        const bool seen = in.webCalls != lastCalls_;
        lastCalls_ = in.webCalls;
        for (int i = 0; i < 2; ++i) {
            if (in.start[i]) {
                Counts& c = counts_[in.outOfCombat ? 1 : 0];
                ++c.presses;
                if (!in.hooked) { // nothing tells whether the game fired
                    ++c.unhooked;
                    waiting_[i] = -1;
                    continue;
                }
                waiting_[i] = 0;
                side_[i] = in.outOfCombat ? 1 : 0;
                continue;
            }
            if (waiting_[i] < 0)
                continue;
            if (seen) { // the game's web shooter asked its muzzle: its ball went
                waiting_[i] = -1;
                ++counts_[side_[i]].shots;
                continue;
            }
            waiting_[i] += dt;
            if (waiting_[i] >= waitSeconds) {
                waiting_[i] = -1;
                ++counts_[side_[i]].unseen;
            }
        }
    }
    const Counts& inCombat() const { return counts_[0]; }
    const Counts& outCombat() const { return counts_[1]; }
    uint32_t presses() const { return counts_[0].presses + counts_[1].presses; }
    void reset() { *this = Sure{}; }

  private:
    float waiting_[2]{-1, -1};
    int side_[2]{};
    uint64_t lastCalls_{};
    bool primed_{};
    Counts counts_[2]{};
};
} // namespace spidy::ball_sure
