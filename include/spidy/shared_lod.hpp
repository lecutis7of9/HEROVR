#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
//
// Update 36: SHARED LOD (live, GRAPHICS page, default OFF, never kept).
// Exe-confirmed (Spider-Man2.exe 2.810.0.0, by disassembly; docs/main-view-gates-audit.md):
// - SubmitScene calls 0x2c9a000 (rcx the view) at 0x2d01c52 for every view it
//   submits. 0x2c9a000 writes view +0x1b24 = K / tangent at 0x2c9a0c3 (K by the aspect
//   +0x41c, the tangent +0x420 or +0x424; 1.0 when +0x438 bit 1).
// - The level-of-detail and screen-size tests multiply it by +0x1b28 (0x2b4f920 in
//   0x2b4f8e0, 0x2c97ee8, 0x2c980e8). +0x1b28 is 1.0 from the view's init (0x2c988cf)
//   and nothing else in the render code writes it.
// So an eye's lens (its own tangent) gives it its own detail distances, unlike the game
// view's. GAME VIEW (A): after 0x2c9a000, an eye's +0x1b28 becomes the game view's
// (active view [0xc2cae70]) +0x1b24 x +0x1b28 over the eye's +0x1b24, so all views
// share one scale. HIGHER (plan B): the larger of that and the eye's own (never less
// detail than now). OFF puts +0x1b28 back to 1.0. EYE LOD (Update 28) stands aside
// while it is on. Candidates (not confirmed): that the game view's scale is the better
// one while it rests (it then keeps its last submit's value).
#include <cmath>
#include <cstdint>
#include <cstdio>

namespace spidy::shared_lod {
inline constexpr uint32_t scaleRva = 0x2c9a000, activeViewRva = 0xc2cae70;
inline constexpr uint32_t scaleOffset = 0x1b24, multiplierOffset = 0x1b28;
// 0x2c9a000's entry as the exe has it.
inline constexpr unsigned char scaleBytes[] = {0x48, 0x8b, 0xc4, 0x48, 0x89, 0x58, 0x20, 0x57,
                                               0x48, 0x81, 0xec, 0x00, 0x01, 0x00, 0x00};

enum Mode : uint32_t { off = 0, game = 1, higher = 2, modes = 3 };
inline constexpr uint32_t defaultMode = higher; // Update 37: the run36 closing setting
inline constexpr uint32_t nextMode = 3;
inline uint32_t next(uint32_t mode) { return (mode + 1) % modes; }
inline const char* modeName(uint32_t mode) {
    switch (mode) {
    case game: return "GAME VIEW";
    case higher: return "HIGHER";
    default: return "OFF";
    }
}
enum Reason : uint32_t {
    none = 0,
    noHook = 1,      // 0x2c9a000 differs in this game build
    gameBad = 2,     // the game view's scale unreadable or out of range
    lowered = 3,     // GAME VIEW would have halved the eyes' detail distance
    writeFailed = 4,
    unseen = 5,      // no eye came through 0x2c9a000
    reasons = 6
};
inline constexpr uint32_t pack(uint32_t mode, uint32_t reason) { return mode | reason << 8; }
inline uint32_t modeOf(uint32_t packed) { return packed & 0xff; }
inline uint32_t reasonOf(uint32_t packed) { return packed >> 8 & 0xff; }
inline constexpr const char* help = "Eyes use the game view's detail distance; HIGHER is the plan B.";
inline const char* helpText(uint32_t packed) {
    const uint32_t m = modeOf(packed), r = reasonOf(packed);
    if (m == higher && r == lowered)
        return "Plan B on: GAME VIEW would have halved the eyes' detail distance.";
    switch (m ? none : r) {
    case noHook: return "N/A: this game build differs, so detail is left as it is.";
    case gameBad: return "Back to OFF: the game view's detail scale was not usable.";
    case writeFailed: return "Back to OFF: the eyes' detail scale could not be written.";
    case unseen: return "Back to OFF: no eye detail scale came through.";
    default: return help;
    }
}
inline constexpr const char* widestValue() { return "OFF (REVERTED)"; }
inline void valueText(uint32_t packed, char* out, unsigned size) {
    const uint32_t m = modeOf(packed), r = reasonOf(packed);
    if (!m && r)
        std::snprintf(out, size, r == noHook ? "OFF (N/A)" : "OFF (REVERTED)");
    else
        std::snprintf(out, size, "%s", modeName(m));
}
inline constexpr float minMultiplier = .25f, maxMultiplier = 4.f; // never more than 4x either way
inline bool sane(float scale) { return std::isfinite(scale) && scale > .05f && scale < 50.f; }
// The eye's +0x1b28 for a mode: the game view's scale over the eye's own (+0x1b24).
inline float multiplier(uint32_t mode, float eyeScale, float gameScale) {
    if (!sane(eyeScale) || !sane(gameScale))
        return 1.f;
    const float target = mode == higher ? std::fmax(eyeScale, gameScale) : gameScale;
    return std::fmin(std::fmax(target / eyeScale, minMultiplier), maxMultiplier);
}
// The auto-revert, judged over the first eye calls after a mode went on.
inline constexpr uint64_t judgeCalls = 240;
inline constexpr uint64_t unseenCalls = 8 * judgeCalls;
inline uint32_t verdict(uint32_t mode, uint64_t calls, uint64_t eyeCalls, uint64_t gameBadCalls,
                        uint64_t halvedCalls) {
    if (!eyeCalls)
        return calls >= unseenCalls ? unseen : none;
    if (eyeCalls < judgeCalls)
        return none;
    if (gameBadCalls * 2 > eyeCalls)
        return gameBad;
    return mode == game && halvedCalls * 2 > eyeCalls ? lowered : none;
}
inline uint32_t afterFailure(uint32_t mode, uint32_t reason) {
    return mode == game && reason == lowered ? higher : off;
}
inline constexpr uint32_t dataMagic = 0x444f4c53; // SLOD
struct Data {
    uint32_t magic = dataMagic, version = 1, bytes = 144, packed = pack(defaultMode, none);
    uint32_t hooked{}, modeChanges{}, reverts{}, lastRevert{};
    uint64_t calls{}, eyeCalls{}, gameBadCalls{}, halvedCalls{}, written{}, restored{};
    float gameScale{}, eyeScale[2]{}, eyeMultiplier[2]{}, pad{};
    uint64_t judgedCalls{}, judgedEyes{}, judgedBad{}, judgedHalved{};
    uint64_t sequence{};
};
static_assert(sizeof(Data) == 144);
} // namespace spidy::shared_lod
