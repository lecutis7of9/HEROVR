#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
//
// Update 36: EYE BLOOM (live, GRAPHICS page, default OFF, never kept).
// Exe-confirmed (Spider-Man2.exe 2.810.0.0, by disassembly; docs/main-view-gates-audit.md):
// - SubmitScene takes the active view's settings ([0xc2cae80]) for the active
//   view (0x2d01cf9..0x2d01d09) and builds any other view's with 0x2c27ec0 (0x2d01d1f).
// - 0x2c27ec0 copies source +0x5b0.. to settings +0xd8.. (0x2c284c1/0x2c284ca), then
//   zeroes the dword +0xd8 (0x2c29107) when the -no_bloom flag byte 0xa16461b is set
//   (0x2c290f4; the command line parser 0x29dc890 sets it at 0x29dce95 on "-no_bloom"),
//   or its flag sil, or the view is not the active one (0x2c29102 test r14b, r14b; r14b
//   is IsActiveView at 0x2c29085). So +0xd8 is the view's bloom, and the eyes have none.
// - The settings go to the render job through copyJob 0x2d03240 (r8, also job +0x658).
// COPY (A): before copyJob runs for an eye, the eye's settings +0xd8 takes the active
// view's ([0xc2cae80] +0xd8). GATE (plan B): the not-active branch at 0x2c29102 is
// jumped over (2 bytes, 45 84 -> eb 0d), so every view keeps its copied value; the
// -no_bloom flag still zeroes it. Candidates (not confirmed): the type of +0xd8 (a
// float scale is likely; it is copied as raw bits); the size of the visible effect.
#include <cstdint>
#include <cstdio>

namespace spidy::eye_bloom {
inline constexpr uint32_t copyJobRva = 0x2d03240, activeSettingsRva = 0xc2cae80;
inline constexpr uint32_t bloomOffset = 0xd8;
// The gate: test r14b, r14b; jne 0x2c29111, then xor ecx, ecx; mov [rdi+0xd8], ecx.
inline constexpr uint32_t gateRva = 0x2c29102;
inline constexpr unsigned char gateBytes[] = {0x45, 0x84, 0xf6, 0x75, 0x0a, 0x33, 0xc9,
                                              0x89, 0x8f, 0xd8, 0x00, 0x00, 0x00};
// jmp 0x2c29111 over the test (written as one aligned 16-bit store; the rest unchanged).
inline constexpr unsigned char gatePatch[] = {0xeb, 0x0d};

enum Mode : uint32_t { off = 0, copy = 1, gate = 2, modes = 3 };
// 0.0.73 lights (test 4's video: streetlight / traffic-light halos and flare streaks strobing, worse
// panning; headlights ghosting): OFF again, as the game itself builds non-active views (0x2c29102,
// exe-confirmed above) and as SpidyVR (no bloom code at all). COPY runs the game's bloom / lens pass
// in both eyes as well as the game view each frame; its frame-to-frame state being one per pass (not
// per view) is the CANDIDATE cause of the strobing. COPY stays one wrist press away.
inline constexpr uint32_t defaultMode = off; // was COPY (Update 37)
inline constexpr uint32_t nextMode = 3;
inline uint32_t next(uint32_t mode) { return (mode + 1) % modes; }
inline const char* modeName(uint32_t mode) {
    switch (mode) {
    case copy: return "COPY";
    case gate: return "GATE";
    default: return "OFF";
    }
}
enum Reason : uint32_t {
    none = 0,
    noHook = 1,    // the gate's bytes differ in this game build (GATE only)
    unseen = 2,    // no eye settings came through copyJob
    headNone = 3,  // the game view has no bloom either
    stillZero = 4, // GATE: the eyes' bloom stayed 0 while the game view's was not
    writeFailed = 5,
    reasons = 6
};
inline constexpr uint32_t pack(uint32_t mode, uint32_t reason) { return mode | reason << 8; }
inline uint32_t modeOf(uint32_t packed) { return packed & 0xff; }
inline uint32_t reasonOf(uint32_t packed) { return packed >> 8 & 0xff; }
// The subtexts: one sentence each, <= 80 characters, exactly one '.'.
inline constexpr const char* help = "OFF: no eye bloom, as SpidyVR; COPY adds the game's glow, no restart.";
inline const char* helpText(uint32_t packed) {
    const uint32_t m = modeOf(packed), r = reasonOf(packed);
    if (m == gate && r == unseen)
        return "Plan B on: the eyes' settings never came through COPY.";
    switch (m ? none : r) {
    case noHook: return "N/A: this game build differs, so bloom is left as it is.";
    case unseen: return "Back to OFF: no eye settings came through.";
    case headNone: return "Back to OFF: the game view has no bloom here either.";
    case stillZero: return "Back to OFF: the eyes' bloom stayed off with GATE.";
    case writeFailed: return "Back to OFF: the eyes' settings could not be written.";
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
// The auto-revert, judged over the first eye jobs after a mode went on.
inline constexpr uint64_t judgeJobs = 240;
inline constexpr uint64_t unseenCalls = 4 * judgeJobs; // copyJob calls with no eye in them
// eyeJobs: eye settings seen; headOn: of those, with the game view's bloom non-zero;
// eyeOwnOn: of those, the eye's own value (before COPY writes) non-zero.
inline uint32_t verdict(uint32_t mode, uint64_t calls, uint64_t eyeJobs, uint64_t headOn, uint64_t eyeOwnOn) {
    if (!eyeJobs)
        return calls >= unseenCalls ? unseen : none;
    if (eyeJobs < judgeJobs)
        return none;
    if (!headOn)
        return headNone;
    return mode == gate && !eyeOwnOn ? stillZero : none;
}
inline uint32_t afterFailure(uint32_t mode, uint32_t reason) {
    return mode == copy && reason == unseen ? gate : off;
}
// Shared with tools (SpidyEyeBloomData).
inline constexpr uint32_t dataMagic = 0x4d4f4c42; // BLOM
struct Data {
    uint32_t magic = dataMagic, version = 1, bytes = 144, packed = pack(defaultMode, none);
    uint32_t gateKnown{}, gatePatched{}, modeChanges{}, reverts{};
    uint32_t lastRevert{}, pad{};
    uint64_t calls{}, eyeJobs{}, headOn{}, eyeOwnOn{}, written{};
    uint32_t headValue{}, eyeOwn[2]{}, eyeNow[2]{}, pad2{};      // raw bits, the newest
    uint64_t judgedCalls{}, judgedEyes{}, judgedHeadOn{}, judgedOwnOn{};
    uint64_t sequence{};
};
static_assert(sizeof(Data) == 144);
} // namespace spidy::eye_bloom
