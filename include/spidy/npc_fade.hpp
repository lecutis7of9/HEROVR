#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
// Update 37.2: NPC NEAR FADE - people who come close to the player vanish in VR.
//
// What hides them (Spider-Man2.exe 2.810.0.0, static disassembly; docs/npc-near-fade.md).
// Every address below was read from the exe; the bytes are checked at run time and
// nothing is written where they differ. CANDIDATE = static only, not yet seen live.
// - CONFIRMED (static, RTTI): Camera2::CameraPenetrationTest (vtable 0x85b2e68, set by
//   the constructor at 0x33610ec) and the registry text "All actors that the camera can
//   hide when clipping" (0x5b71d90).
// - CANDIDATE: its update 0x3361e10 hides actors near the GAME's camera. Two ways in:
//   (a) the hits of its sphere query around the camera (0x3361a80, radius from the
//       camera's +0x7c): each hit goes through 0x3360ff0 (called only from 0x3361e75
//       and 0x3361eba), which adds the actor to a list of at most 32 and calls
//       0x3707c30 (a per-reason hide counter on the actor; 0x3707f30 undoes it);
//   (b) the registry of hideable actors (0x3360760) tested against a sphere round the
//       camera, skipped when the byte 0xc417360 is not 0 (0x3361ec5:
//       cmp byte [rip+0x90b5494], r15b / jne).
//   Hidden actors come back on their own when their time in the list runs out
//   (0x3361250). In VR the eyes are far from the game's camera, so people next to you
//   can be inside its clipping sphere and get hidden.
// - CANDIDATE: 0xc417360 is the game's own "camera penetration tests disabled" switch:
//   SetCameraPenetrationTestsEnabledAction (vtable 0x54425d0) writes it (0x117fe12,
//   1 for "disable") and clears it when the action ends (0x117fe54).
// The modes:
//   GAME   the game's own behaviour.
//   OFF    (default) the switch set (b) and 0x3360ff0 returns at once (a): one byte, c3
//          over its first byte 48, put back on GAME / SWITCH and when VR stops.
//   SWITCH plan B: only the game's own switch (b), no code changed.
// NPC PUSH (a soft push of people away from the player) is NOT in this build: no game
// path that moves a street NPC was confirmed (docs/npc-near-fade.md lists the candidates).
#include <cstdint>
#include <cstdio>

namespace spidy::npc_fade {
enum Mode : uint32_t { game = 0, off = 1, switchOnly = 2, modes = 3 };
inline constexpr uint32_t defaultMode = off, nextMode = 3;
inline uint32_t next(uint32_t m) { return m == game ? off : m == off ? switchOnly : game; }
inline const char* name(uint32_t m) {
    switch (m) {
    case off: return "OFF";
    case switchOnly: return "SWITCH";
    default: return "GAME";
    }
}
enum Why : uint32_t { fine = 0, noBytes = 1, writeFailed = 2 };

// The switch: read by the update (cmp byte [rip+disp32], r15b / jne rel32).
inline constexpr uint32_t gateRva = 0x3361ec5;
inline constexpr unsigned char gateBytes[] = {0x44, 0x38, 0x3d, 0x94, 0x54, 0x0b, 0x09, 0x0f, 0x85};
// ... and written by the game's own action (mov [rip+disp32], al).
inline constexpr uint32_t writerRva = 0x117fe12;
inline constexpr unsigned char writerBytes[] = {0x88, 0x05, 0x48, 0x75, 0x29, 0x0b};
inline constexpr uint32_t disp32(const unsigned char* b) {
    return static_cast<uint32_t>(b[0]) | static_cast<uint32_t>(b[1]) << 8 | static_cast<uint32_t>(b[2]) << 16 |
           static_cast<uint32_t>(b[3]) << 24;
}
// rip-relative target: the next instruction + the signed displacement.
inline constexpr uint32_t ripTarget(uint32_t next, uint32_t disp) {
    return static_cast<uint32_t>(static_cast<int64_t>(next) + static_cast<int32_t>(disp));
}
inline constexpr uint32_t switchRva = 0xc417360;
static_assert(ripTarget(gateRva + 7, disp32(gateBytes + 3)) == switchRva);
static_assert(ripTarget(writerRva + 6, disp32(writerBytes + 2)) == switchRva);
// The hit-hide: its first bytes (test rdx,rdx / je), and its only two callers.
inline constexpr uint32_t hitHideRva = 0x3360ff0;
inline constexpr unsigned char hitHideBytes[] = {0x48, 0x85, 0xd2, 0x0f, 0x84, 0xc5, 0x00, 0x00, 0x00};
inline constexpr unsigned char retByte = 0xc3;
inline constexpr uint32_t callerRvas[] = {0x3361e75, 0x3361eba};
inline constexpr unsigned char callerBytes[2][5] = {{0xe8, 0x76, 0xf1, 0xff, 0xff}, {0xe8, 0x31, 0xf1, 0xff, 0xff}};
static_assert(ripTarget(callerRvas[0] + 5, disp32(callerBytes[0] + 1)) == hitHideRva);
static_assert(ripTarget(callerRvas[1] + 5, disp32(callerBytes[1] + 1)) == hitHideRva);

// What each mode wants: the switch byte, and whether the hit-hide returns at once.
inline bool wantsSwitch(uint32_t m) { return m == off || m == switchOnly; }
inline bool wantsRet(uint32_t m) { return m == off; }

inline constexpr const char* help = "OFF stops the game hiding people who come close to you. No restart.";
inline const char* helpText(uint32_t mode, uint32_t why) {
    if (why == noBytes)
        return "N/A: this game build differs, so people are left to the game.";
    if (why == writeFailed)
        return "Back to GAME: the change could not be made.";
    if (mode == switchOnly)
        return "Plan B: only the game's own switch. Still vanishing? Set GAME, send the log.";
    return help;
}
inline void valueText(uint32_t mode, uint32_t why, char* out, unsigned size) {
    std::snprintf(out, size, "%s", why == noBytes ? "GAME (N/A)" : why ? "GAME (REVERTED)" : name(mode));
}

// Near-NPC telemetry: a person within `nearMetres` of the player starts an episode.
inline constexpr float nearMetres = 2.f, clearMetres = 3.f;
struct Episodes {
    bool near{};
    uint64_t count{};
    // One step: the nearest person's metres (1e9 none / unknown). True when one began.
    bool step(float metres) {
        if (!near && metres <= nearMetres) {
            near = true;
            ++count;
            return true;
        }
        if (near && metres > clearMetres)
            near = false;
        return false;
    }
};

// telemetry: SpidyNpcFadeData (tools/update37_records.py)
inline constexpr uint32_t dataMagic = 0x4643504e; // NPCF
struct Data {
    uint32_t magic = dataMagic, version = 1, bytes = 96, mode = defaultMode;
    uint32_t why{}, switchNow{}, retNow{}, ready{};
    uint64_t switchWrites{}, retWrites{}, switchCleared{}; // cleared: found 0 while OFF (the game's action ended)
    uint64_t nearEpisodes{}, nearWhileGame{}, nearWhileFixed{};
    float nearestMetres = 1e9f, pad{};
    uint64_t sequence{};
};
static_assert(sizeof(Data) == 96);
} // namespace spidy::npc_fade


// npc_fade.cpp (Windows): the thread, from VR start to VR stop.
namespace spidy::npc_fade {
void start(uintptr_t imageBase);
void stop();
uint32_t now(); // the mode in force | why << 8
void set(uint32_t mode);
} // namespace spidy::npc_fade
