#pragma once
// Update 38.5 Y IN THE AIR (read-only telemetry): whether the game's own move out of the swing's
// flight - a web strike, a zip to a point, or (CANDIDATE) its Web Wings glide, which the game opens
// with Y held in the air - keeps the hero, and whether the swing took him back while the game's move
// still had him. No glide state is read: the game's HeroStateWingsuitGlide is only an RTTI name
// (CANDIDATE, docs/mirror.md and run384/DIAGNOSIS.md "for later"); the sign used is the swing's own
// AirYield (the game's air handler quiet after a Y or another ask, game_swing.hpp).
#include <cstdint>

namespace spidy::air_watch {
// The swing (ray) module's AirYield counts (SpidyAirYieldSample); odd `sequence` while it changes.
struct YieldData {
    uint32_t magic = 0x444c5941, version = 1, bytes = 64, yielding{}; // AYLD
    uint64_t yields{}, outlived{}, takenBack{}, capped{};
    uint32_t longestMs{}, lastMs{};
    uint64_t sequence{};
};
static_assert(sizeof(YieldData) == 64);
// The VR module's record (SpidyAirWatchData, tools/update37_records.py 'air_watch').
// ySent: Y presses sent to the game; yInAir: of them, with the swing carrying the hero in the air.
// yankInputs / yanksInAir: GRAB WEB yanks sent on the WEB YANK input (default Y HOLD), in the air.
// yields: the game's moves that took the hero out of the swing's flight; outlived: of them, held past
// 3 s (a glide's sign: a web strike or zip lands sooner); takenBack: ended past 3 s by a trigger (the
// player webbing again); capped: ended at the 60 s cap still in the game's move (the swing would
// have fought it - expected 0). longestMs / lastMs: the hand-overs' lengths.
struct Data {
    uint32_t magic = 0x57524941, version = 1, bytes = 104, yielding{}; // AIRW
    uint64_t ySent{}, yInAir{}, yankInputs{}, yanksInAir{}, yields{}, outlived{}, takenBack{}, capped{}, spare{};
    uint32_t longestMs{}, lastMs{};
    uint64_t sequence{};
};
static_assert(sizeof(Data) == 104);
} // namespace spidy::air_watch
