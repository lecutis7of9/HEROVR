#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
//
// Version 0.0.5 (Update 38.9): the record (magic "V005") of its new parts, for
// tools/update37_records.py: HERO HEIGHT, the DAMAGE PROBE (read-only: enemies' hit points
// beside X presses) and DAMAGE TEST (the candidate write, OFF by default).
#include "damage_probe.hpp"

#include <cstddef>
#include <cstdint>

namespace spidy::v005 {

inline constexpr uint32_t magicValue = 0x35303056; // "V005"

struct Data {
    uint32_t magic = magicValue, version = 2, bytes = 552, flags{};
    uint32_t heroHeightValue{}, heroDamageValue{}; // combat38 39 and 40
    float heroHeightK = 1.f, pad0{};
    // DAMAGE PROBE (damage_probe.hpp)
    uint32_t healthState{}, writeState{}, enemiesNow{}, enemiesWithHealth{};
    uint64_t healthComponents{}, samples{}, enemiesRead{}, falls{}, fallsMelee{}, fallsOurs{}, fallsUnexplained{},
        knockouts{}, rises{};
    float biggestFall{}, nearestHp{}, nearestMost{}, pad1{};
    uint64_t meleePresses{};
    damage_probe::Event recent[damage_probe::events]{}; // the latest last
    // DAMAGE TEST (sm2_damage.hpp)
    uint32_t writeStatus{}, writeCode{};
    uint64_t queued{}, sent{}, notSlot{}, full{}, faults{}, refusedOff{};
    uint64_t lastRecord{};
    uint32_t lastHandle{}, pad2{};
    // SOUND EFFECTS (sfx.hpp Stats)
    uint32_t sfxState{}, sfxCode{};
    uint64_t sfxAsked{}, sfxPlayed{}, sfxDropped{}, sfxMuted{};
    float sfxGain{}, pad3{};
    // combat38 41-44: PUNCH SENSITIVITY, SFX VOLUME, SUPERSONIC, SUPERSONIC SPEED
    uint32_t punchSenseValue{}, sfxVolumeValue{}, supersonicValue{}, supersonicSpeedValue{};
    // FLY: SUPERSONIC engages and the flight speed now (m/s)
    uint32_t supersonicEngages{};
    float flySpeedNow{};
    uint64_t sequence{}; // last: odd while written
};
static_assert(sizeof(damage_probe::Event) == 32, "update37_records.parse_v005");
static_assert(sizeof(Data) == 552 && offsetof(Data, recent) == 144 && offsetof(Data, sfxState) == 472 &&
                  offsetof(Data, sequence) == 544, "Data.bytes and update37_records.RECORDS['v005']");
} // namespace spidy::v005
