#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
// HERO VR 0.0.77 CUTFIX: why a scene on the game screen went (or did not go) 3d or immersive (cutscene077.hpp).
//
// The cut1 report (November 3): cutscenes = immersive, an in-engine opening scene (the game's own camera moving,
// its lens animating 29-113 degrees) stayed on the flat screen for 80 s. CONFIRMED: SceneClock
// (presentation_gate.hpp) only ran from game_time's clock, and game_time is not hooked on Spider-Man 2
// (game_time.cpp: SM2TODO, timeNotPorted), so the clock never ran: no 3d pair was ever asked, and ImmersiveHold,
// which starts only on a running clock, never started.
//
// The fix: without the game-time hook, a scene's clock runs while the game's own render camera (its pose and its
// lens, read in stereo_probe.cpp each render frame) moves, after the screen has been up for motionSettleMs. A
// pause menu or a hint card freezes the game's camera, so they keep the screen exactly as before (CANDIDATE: SM2
// freezes its camera in its pause menu, as the cut1 report's still stretches suggest). A still camera inside a
// running scene is ImmersiveHold's frozen clock (cutscene077.hpp): held as a QTE, the Menu button still the screen.
//
// A PRE-RENDERED VIDEO (a movie file the game plays, not its world) has no 3D camera at all: it can never be 3d or
// immersive, only shown on the flat screen. Its camera does not move, so it stays there (reason camera_still).
//
// Pure and allocation-free; nothing here reads or writes the game. Made by Agent Mulder.
#include <cmath>
#include <cstdint>
#include <cstdio>

namespace spidy::cutscene077 {

// ---- The scene clock without the game-time hook ----
// The game's render camera moved between two render frames: its pose (16 floats, engine rows) or its lens
// (left, right, top, bottom tangents). Non-finite values never count as a move.
constexpr float cameraMoveEpsilon = 1e-5f;
inline bool cameraMoved(const float before[16], const float beforeLens[4], const float now[16],
                        const float nowLens[4]) {
    bool moved = false;
    for (int i = 0; i < 16; ++i) {
        if (!std::isfinite(now[i]) || !std::isfinite(before[i]))
            return false;
        moved = moved || std::abs(now[i] - before[i]) > cameraMoveEpsilon;
    }
    for (int i = 0; i < 4; ++i) {
        if (!std::isfinite(nowLens[i]) || !std::isfinite(beforeLens[i]))
            return false;
        moved = moved || std::abs(nowLens[i] - beforeLens[i]) > cameraMoveEpsilon;
    }
    return moved;
}
// The screen is up this long before camera motion counts: the switch from the VR eyes' camera to the game's
// own (the 250 ms screen delay, the last eye command's lease) is not a scene.
constexpr uint64_t motionSettleMs = 400;
// Which clock a scene ran on.
enum ClockSource : uint32_t { clockGameTime = 0, clockCameraMotion = 1 };
inline const char* clockName(uint32_t source) {
    return source == clockCameraMotion ? "camera motion (SM2: no game-time hook)" : "game time";
}
class MotionClock {
  public:
    // Once per headset frame. `timeHooked`, `timeAdvances`: game_time's clock is installed, and its last frame
    // took game time. `moves`: the render frames whose camera moved so far (monotonic, stereo_probe.cpp).
    // `screen`: the game screen is up. True: the scene's time ran this frame (SceneClock's `advancing`).
    bool advancing(bool timeHooked, bool timeAdvances, uint64_t moves, bool screen, uint64_t nowMs) {
        source_ = timeHooked ? clockGameTime : clockCameraMotion;
        const bool movedNow = moves != lastMoves_;
        lastMoves_ = moves;
        if (timeHooked)
            return timeAdvances;
        if (!screen) {
            upMs_ = 0;
            return false;
        }
        if (!upMs_ || nowMs < upMs_)
            upMs_ = nowMs ? nowMs : 1;
        return movedNow && nowMs - upMs_ >= motionSettleMs;
    }
    uint32_t source() const {
        return source_;
    }
    void reset() {
        *this = {};
    }

  private:
    uint64_t lastMoves_{}, upMs_{};
    uint32_t source_{};
};

// ---- Why a scene on the screen was not 3d or immersive ----
enum Refusal : uint32_t {
    refuseFault = 1u << 0,       // the mode is off this session after a fault (the flat screen)
    refuseNoPlayer = 1u << 1,    // no player (the title screen, a load)
    refuseNoBridge = 1u << 2,    // the gameplay bridge is not running
    refuseGameplay = 1u << 3,    // the player's own camera is back (the screen is going)
    refuseViewScreen = 1u << 4,  // VIEW: GAME SCREEN (the game played on its screen)
    refuseNoHead = 1u << 5,      // the headset has no tracked head pose
    refuseEyesLost = 1u << 6,    // the eyes were lost to a graphics setting (the notice screen)
    refuseClock = 1u << 7,       // the scene's clock does not run (a pause, a hint card)
    refuseStill = 1u << 8,       // no game-time hook and the game's camera is still: a pause, a hint card, or a
                                 // pre-rendered video (no 3D camera: flat screen only)
    refuseMenu = 1u << 9,        // the Menu button ended the stretch (a pause)
    refuseDepthOff = 1u << 10,   // 3d: GAME SCREEN 3D's depth is off
    refuseNoPanel = 1u << 11,    // 3d: no subtitle strip landed for 1 s (presentation_gate.hpp ScenePanel)
};
constexpr unsigned refusalCount = 12;
inline const char* refusalName(unsigned bit) {
    static const char* const names[refusalCount] = {
        "fault",        "no_player", "no_bridge", "gameplay",  "view_game_screen", "no_head_pose", "eyes_lost",
        "clock_stopped", "camera_still", "menu",     "depth_off", "no_panel"};
    return bit < refusalCount ? names[bit] : "unknown";
}
struct GateInput {
    uint32_t mode{};  // cutscene077::Mode
    bool screen{}, hero{}, bridge{}, gameplay{}, viewScreen{}, head{}, eyesLost{}, fault{};
    bool clockRuns{}, cameraClock{}, menu{}, depthOn{};
};
// The reasons the frame's scene is refused, for the mode asked; 0: nothing in the way. Only meaningful while
// the screen is up (gameplay itself is never a scene).
inline uint32_t refusals(const GateInput& g) {
    uint32_t r = 0;
    if (g.fault)
        r |= refuseFault;
    if (!g.hero)
        r |= refuseNoPlayer;
    if (!g.bridge)
        r |= refuseNoBridge;
    if (g.gameplay)
        r |= refuseGameplay;
    if (g.viewScreen)
        r |= refuseViewScreen;
    if (!g.clockRuns)
        r |= g.cameraClock ? refuseStill : refuseClock;
    if (g.mode == 2) {
        if (!g.head)
            r |= refuseNoHead;
        if (g.eyesLost)
            r |= refuseEyesLost;
        if (g.menu)
            r |= refuseMenu;
    } else if (g.mode == 1 && !g.depthOn) {
        r |= refuseDepthOff;
    }
    return r;
}
// Frames refused per reason: a 10 s window (the diagnostics line) and one screen stretch (its summary line).
class RefusalLog {
  public:
    void add(uint32_t mask) {
        ++frames_;
        for (unsigned b = 0; b < refusalCount; ++b)
            counts_[b] += (mask >> b) & 1u;
        refused_ += mask != 0;
    }
    uint64_t frames() const {
        return frames_;
    }
    uint64_t refused() const {
        return refused_;
    }
    uint64_t count(unsigned bit) const {
        return bit < refusalCount ? counts_[bit] : 0;
    }
    // "reason N, reason N" (only the reasons seen), or "none"; always terminated.
    void reasons(char* out, size_t size) const {
        if (!size)
            return;
        out[0] = 0;
        size_t at = 0;
        for (unsigned b = 0; b < refusalCount; ++b) {
            if (!counts_[b])
                continue;
            const int n = std::snprintf(out + at, size - at, "%s%s %llu", at ? ", " : "", refusalName(b),
                                        static_cast<unsigned long long>(counts_[b]));
            if (n < 0 || static_cast<size_t>(n) >= size - at)
                break;
            at += static_cast<size_t>(n);
        }
        if (!at)
            std::snprintf(out, size, "none");
    }
    void reset() {
        *this = {};
    }

  private:
    uint64_t counts_[refusalCount]{}, frames_{}, refused_{};
};
// Said in the log once a session: what can never be immersive.
constexpr const char* videoNote =
    "a pre-rendered video has no 3D camera: it can only be shown on the flat screen, never 3d or immersive";
} // namespace spidy::cutscene077
