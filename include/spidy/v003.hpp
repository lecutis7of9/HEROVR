#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
//
// Version 0.0.3 (Update 38.7): the record (magic "V003") of its new parts, for
// tools/update37_records.py: the new COMBAT values, AUTO PERFORMANCE, ROPE GRAB, WALL STICK,
// the MIRROR's suit id watch and view test, and the BOT STATE PROBE. Read-only research and
// counters; nothing in it is ever written to the game.
#include "auto_perf.hpp"
#include "bot_probe.hpp"

#include <cstddef>
#include <cstdint>

namespace spidy::v003 {

inline constexpr uint32_t magicValue = 0x33303056; // "V003"
inline constexpr uint32_t newOptions = 7, perfEvents = 16;

struct Data {
    uint32_t magic = magicValue, version = 1, bytes = 5200, flags{};
    uint32_t values[newOptions]{}; // ROPE GRAB .. SHOW STATUS (combat38 30-36)
    // AUTO PERFORMANCE
    uint32_t perfOn{}, perfRung{}, perfZone{}, perfEventCount{};
    float perfTarget{}, perfMeanFps{}, perfMissed{}, perfHeadsetHz{};
    uint32_t userFarPercent{}, userSharedLod{}, userFarShadows{}, nowFarPercent{};
    uint32_t nowSharedLod{}, nowFarShadows{}, applyRefused{}, userChangedWhileOn{};
    uint64_t perfDrops{}, perfRaises{}, perfZoneJumps{}, perfBans{}, perfRestores{};
    auto_perf::Event perfLog[perfEvents]{}; // the latest, oldest first
    // ROPE GRAB
    uint32_t tetherOn[2]{}, tetherWhy[2]{};
    uint64_t tetherId[2]{};
    float tetherLength[2]{}, tetherSpeed[2]{};
    uint64_t ropeAttaches{}, ropeYanks{}, ropeClaps{}, ropeNoTarget{}, ropeBothOnOne{};
    uint64_t ropeReleases[8]{}; // rope_grab::Release
    // WALL STICK
    uint64_t wallImpacts{}, wallShown{}, wallWalls{}, wallFloors{}, wallUnstuck{}, wallArms{};
    uint32_t wallNetsNow{}, wallPad{};
    // MIRROR: the suit id watch (hero + 0x8c, CANDIDATE) and the view test
    uint32_t suitId{}, suitHave{};
    uint64_t suitReads{}, suitFailed{}, suitChanges{}, suitColoursForgot{};
    // The MIRROR VIEW TEST (shipped in 0.0.4, OFF by default, needs a restart; 0 while never run).
    uint32_t testPhase{}, testWhy{}, testSlot{}, testRuns{};
    float testSeconds{}, testBaseMs{}, testMs{}, testPad{};
    uint64_t testFrames{};
    uint32_t testWidth{}, testHeight{}, testTexture{}, testFreeSlots{};
    uint64_t testHeroDraws{}, testOtherDraws{}; // the MIRROR PROBE's counts in its slot
    // BOT STATE PROBE
    uint32_t botOn{}, botPad{};
    bot_probe::State bot{};
    uint64_t sequence{}; // last: odd while written
};
static_assert(sizeof(Data) == 5200 && offsetof(Data, bot) == 864, "Data.bytes and update37_records.RECORDS['v003']");
} // namespace spidy::v003
