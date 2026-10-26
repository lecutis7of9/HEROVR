#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
//
// Version 0.0.6 (Update 39.0): the record (magic "V006") of its new parts, for
// tools/update37_records.py: the hero now, DAREDEVIL's batons and punches, HULK's punches,
// IRON MAN's blows and SUPERSONIC's reached speed.
#include <cstddef>
#include <cstdint>

namespace spidy::v006 {

inline constexpr uint32_t magicValue = 0x36303056; // "V006"

struct Data {
    uint32_t magic = magicValue, version = 1, bytes = 128, flags{};
    // combat38 45-47: DAREDEVIL, HULK, DAREDEVIL PUNCH; the hero now (hero_modes order)
    uint32_t daredevilValue{}, hulkValue{}, daredevilPunchValue{}, heroNow{};
    // DAREDEVIL: batons (both hands) thrown, enemies hit, caught, presses refused (out), blows sent
    uint64_t batonThrows{}, batonHits{}, batonCatches{}, batonRefused{}, batonBlows{};
    // landed punches' blows: DAREDEVIL's extra, HULK's; punches with no enemy within reach
    uint64_t ddPunchBlows{}, hulkPunchBlows{}, punchMisses{};
    // IRON MAN: blows sent (repulsor + beam)
    uint64_t ironBlows{};
    // DAREDEVIL SONAR: 1 on, 2 could not start (the eye pass failed)
    uint32_t sonarState{}, pad0{};
    // SUPERSONIC: the top speed reached in the last dash and in all (m/s)
    float sonicLast{}, sonicBest{};
    uint64_t sequence{}; // last: odd while written
};
static_assert(sizeof(Data) == 128 && offsetof(Data, batonThrows) == 32 && offsetof(Data, sonarState) == 104 &&
                  offsetof(Data, sequence) == 120, "Data.bytes and update37_records.RECORDS['v006']");
} // namespace spidy::v006
