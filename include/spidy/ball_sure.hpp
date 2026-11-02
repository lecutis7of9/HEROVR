#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
// Made by Agent Mulder.
//
// HERO VR 0.0.76 BALL SURE (test 801ce57: "webballs only fire out like a quarter of the time and even then only in
// combat"). In that run all 98 presses went to the game as RB taps (GAME WEBS ONLY plays OURS as WRIST); the game fires
// its R1 web ball only in combat and not on every tap. This port knows no CONFIRMED SM2 function that fires the game's
// ball, so none is called. Instead, pure and per press:
// - OUT OF COMBAT (no enemy within `combatMetres`): no RB at all (the game would fire nothing); the mod's own ball
//   leaves that wrist at once along the aim (BALL LOOK OURS' ball).
// - IN COMBAT: the RB goes to the game (its own ball, look and auto-aim). The wrist muzzle hook (wrist_muzzle.hpp,
//   CANDIDATE) counts the web shooter's muzzle asks; when it is hooked and no ask comes within `waitSeconds` of the
//   press, the game dropped it and the mod's own ball leaves that wrist instead. Unhooked (nothing to tell), the mod's
//   own ball goes with every press at once, so a press is never lost (at worst two balls).
// Nothing here reads or writes game memory.
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace spidy::ball_sure {

inline constexpr float combatMetres = 30.f, waitSeconds = .30f;

// HERO VR 0.0.76 BALL NOW (test 32bd648: "webballs are still not super reactive and dont fire unless in combat"). Why
// they did not fire out of combat: the press there WAS the mod's own ball (outOfCombat, above), but under GAME WEBS ONLY
// the mod's balls were not drawn at all (the 0.0.76 draw left them to the game, which draws only its own), so every one
// flew unseen; in combat the RB waited on the game (the burst's grace, the gadget wait, the RB queue and the game's own
// gate), then up to waitSeconds more for a backup. Now SPIDER-MAN under GAME WEBS ONLY fires the mod's own ball from
// the pulled wrist on the press frame, in combat or not, drawn; no RB, so the game never adds a second ball.
inline bool oursEveryPress(bool spiderMan, bool gameWebsOnly) { return spiderMan && gameWebsOnly; }

// The nearest enemy (metres, 1e9 / NaN unknown) and how many the enemy list has: out of combat (no RB).
inline bool outOfCombat(uint32_t enemyCount, float nearestMetres) {
    return enemyCount == 0 || !std::isfinite(nearestMetres) || nearestMetres > combatMetres;
}

struct In {
    float seconds{};
    bool start[2]{};      // the combat machine's ballStart: the game's RB for this hand went down this frame
    bool hooked{};        // wrist_muzzle hooked (status().hooked != 0)
    uint64_t webCalls{};  // wrist_muzzle status().webCalls (the web shooter's muzzle asks so far)
};
struct Out {
    bool backup[2]{}; // fire the mod's own ball from this wrist now
};

class Sure {
  public:
    Out update(const In& in) {
        Out o;
        const float dt = std::isfinite(in.seconds) ? std::clamp(in.seconds, 0.f, .1f) : 0.f;
        if (!primed_) { // the count so far is not this press's
            primed_ = true;
            lastCalls_ = in.webCalls;
        }
        const bool seen = in.webCalls != lastCalls_;
        lastCalls_ = in.webCalls;
        for (int i = 0; i < 2; ++i) {
            if (in.start[i]) {
                ++presses_;
                if (!in.hooked) { // nothing tells whether the game fired: ours goes with it
                    o.backup[i] = true;
                    ++unhookedBalls_;
                    waiting_[i] = -1;
                    continue;
                }
                waiting_[i] = 0;
                continue;
            }
            if (waiting_[i] < 0)
                continue;
            if (seen) { // the game's web shooter asked its muzzle: its ball went
                waiting_[i] = -1;
                ++gameShots_;
                continue;
            }
            waiting_[i] += dt;
            if (waiting_[i] >= waitSeconds) {
                waiting_[i] = -1;
                o.backup[i] = true;
                ++backups_;
            }
        }
        return o;
    }
    void outOfCombatPress() { ++outBalls_; }
    uint32_t presses() const { return presses_; }
    uint32_t gameShots() const { return gameShots_; }
    uint32_t backups() const { return backups_; }
    uint32_t unhookedBalls() const { return unhookedBalls_; }
    uint32_t outBalls() const { return outBalls_; }
    void reset() { *this = Sure{}; }

  private:
    float waiting_[2]{-1, -1};
    uint64_t lastCalls_{};
    bool primed_{};
    uint32_t presses_{}, gameShots_{}, backups_{}, unhookedBalls_{}, outBalls_{};
};
} // namespace spidy::ball_sure
