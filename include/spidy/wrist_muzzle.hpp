// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
// Update 38.5 BALL LOOK WRIST: the game's own web ball (its look, its auto-aim) leaving the
// wrist of the hand whose trigger was pulled.
//
// What is known (static disassembly of Spider-Man2.exe 2.810.0.0, docs/mirror.md has the method):
//   CANDIDATE 0x53c1d98  WeaponWebShooter's vtable (RTTI ".?AVWeaponWebShooter@@"). Its slots
//                        +0x160..+0x178 are WeaponBase's (0x532f968) own functions.
//   CANDIDATE 0x373e1b0  vtable +0x168, Muzzle(weapon, out 4x4, index): the emitter at weapon
//                        +0x1bc + 0x38 * index (index < 16), else the weapon's own transform;
//                        the 4x4's row 3 (+0x30) is the position. +0x170 (0x373e300) calls
//                        it through the vtable and returns that position.
//   CANDIDATE 0x373e260  vtable +0x160, the same with a parent matrix (r8) as the fallback.
//   Spider-Man Remastered's analogue (docs/REFERENCE.md "Muzzle"): index 0 the right wrist,
//   1 the left; overriding it started the shot at the given point and it still flew to the
//   game's own aim point.
// NOT KNOWN: that SM2's web ball reads either function when it is spawned. Hence the
// guards: only the web shooter's calls, only in the window after this port's own ball
// press, only a shift under maxShift, and the counters below say what happened
// (`overrides` 0 after balls were fired = the candidate is wrong; the ball is then
// exactly the game's, as GAME ONLY).
#pragma once
#include "spidy/math.hpp"
#include <cstdint>

namespace spidy::wrist_muzzle {
inline constexpr uintptr_t webShooterVtable = 0x53c1d98, muzzleRva = 0x373e1b0, parentMuzzleRva = 0x373e260;
inline constexpr unsigned char muzzleBytes[] = {0x48, 0x89, 0x5c, 0x24, 0x08, 0x57, 0x48, 0x83,
                                                0xec, 0x60, 0x48, 0x8b, 0xda, 0x48, 0x8b, 0xf9};
inline constexpr unsigned char parentMuzzleBytes[] = {0x48, 0x89, 0x5c, 0x24, 0x08, 0x57, 0x48, 0x83,
                                                      0xec, 0x60, 0x48, 0x8b, 0xf9, 0x48, 0x8b, 0xda};
// The window after a ball's RB goes down in which the web shooter's muzzle is the wrist's,
// and the farthest the wrist may be from where the game put it (further: not this ball).
inline constexpr float window = .30f, maxShift = 2.5f, staleSeconds = .25f;

// One hand's wrist in game world metres, as published each frame.
struct Wrist {
    Vec3 position{};
    float at = -1e9f; // seconds (the publisher's clock)
    bool valid{};
};
// The ball in flight: whose, since when.
struct Shot {
    int hand = -1;
    float at = -1e9f;
};
enum Verdict : uint32_t { kept = 0, moved, noShot, stale, tooFar, notFinite };
// The pure rule (tested): where the muzzle goes for a web-shooter call at `now`.
inline Verdict decide(const Shot& shot, const Wrist (&wrists)[2], float now, Vec3 game, Vec3& out) {
    out = game;
    if (shot.hand < 0 || shot.hand > 1 || !(now - shot.at >= 0) || now - shot.at > window)
        return noShot;
    const Wrist& w = wrists[shot.hand];
    if (!w.valid || !(now - w.at <= staleSeconds))
        return stale;
    if (!finite(w.position) || !finite(game))
        return notFinite;
    if (length(w.position - game) > maxShift)
        return tooFar;
    out = w.position;
    return moved;
}

#pragma pack(push, 8)
// The exported record (SpidyWristMuzzleData; tools/update37_records.py 'wrist_muzzle'): the
// header and the odd-while-writing sequence last, as every record.
struct Status {
    uint32_t magic = 0x5a554d57, version = 1, bytes = 144; // "WMUZ"
    uint32_t hooked{};     // 1 the muzzle, 2 the parent muzzle, 3 both
    uint32_t startError{}; // 0, or why it stayed off (11001 bytes / slot differ, 11xxx MinHook)
    uint32_t enabled{};    // BALL LOOK WRIST or GLOB
    uint64_t calls{}, webCalls{}, moved{}, noShot{}, stale{}, tooFar{}, notFinite{};
    uint64_t shots[2]{};     // ball presses handed over, by hand
    uint64_t indexSeen[4]{}; // the web shooter's emitter index asked (0, 1, 2, other)
    float lastShift{};       // metres the last move took the muzzle
    float lastAge{};         // seconds after the press of the last move
    uint64_t sequence{};
};
static_assert(sizeof(Status) == 144);
#pragma pack(pop)

// Windows only (wrist_muzzle.cpp, in the game DLL): start / stop the hooks, the
// wrists each frame, a ball press, BALL LOOK.
uint32_t start(uintptr_t base);
void stop();
// (Times are the module's own clock, taken at the call.)
void publish(unsigned hand, Vec3 world, bool valid);
void fire(unsigned hand);
void setEnabled(bool on);
Status status();
} // namespace spidy::wrist_muzzle
