#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
//
// Version 0.0.5 (Update 38.9) DAMAGE PROBE: read-only. The nearest enemies' hit points (each
// one's BotHealth component, found in the component registry by its vtable 0x531b0a0 with its
// owner actor at +0x10 - the very way the CONFIRMED HeroHealth is found; hit points +0xd0,
// most +0xa0) watched at 10 Hz, and every fall put beside the player's last X press (the
// game's own punch) and the mod's last DAMAGE TEST blow.
//   health state: 0 none read, 1 read, 2 CONFIRMED (a fall within 1 s of an X press)
//   write state:  0 none sent, 1 sent, 2 CONFIRMED (a fall within 1 s of our blow, no X press)
// A knockout: hit points fall to 0 or below. Pure (tests): the caller gives the samples and times.
#include <cstdint>

namespace spidy::damage_probe {

inline constexpr uint32_t slots = 16, events = 8;
inline constexpr uint64_t windowMs = 1000;
enum Cause : uint32_t { unexplained = 0, melee = 1, ours = 2 };

struct Sample {
    uint64_t id{};  // the enemy's record (world_trace id)
    float hp{}, most{};
    bool have{};    // its health component found and read
};
struct Event {
    uint64_t id{};
    float before{}, after{}, most{};
    uint32_t cause{}, delayMs{}; // the fall's cause; ms since that press or blow
    uint32_t knockedOut{};
};
struct Tracker {
    struct Slot {
        uint64_t id{};
        float hp{}, most{};
        uint32_t age{};
    } slot[slots]{};
    uint32_t healthState{}, writeState{};
    uint64_t samples{}, enemiesRead{}, falls{}, fallsMelee{}, fallsOurs{}, fallsUnexplained{}, knockouts{},
        rises{};
    float biggestFall{};
    Event recent[events]{};
    uint32_t nextEvent{};
    // One sample round: nowMs, the last X press and the last mod blow (0: never).
    void update(uint64_t nowMs, const Sample* s, uint32_t n, uint64_t meleeMs, uint64_t oursMs, bool oursEverSent) {
        if (oursEverSent && writeState == 0)
            writeState = 1;
        for (auto& sl : slot)
            if (sl.id)
                ++sl.age;
        for (uint32_t i = 0; i < n; ++i) {
            if (!s[i].have || !s[i].id || !(s[i].hp == s[i].hp) || !(s[i].most > 0))
                continue;
            ++samples;
            if (healthState == 0)
                healthState = 1;
            Slot* found = nullptr;
            for (auto& sl : slot)
                if (sl.id == s[i].id)
                    found = &sl;
            if (!found) {
                ++enemiesRead;
                Slot* oldest = &slot[0];
                for (auto& sl : slot) {
                    if (!sl.id) {
                        oldest = &sl;
                        break;
                    }
                    if (sl.age > oldest->age)
                        oldest = &sl;
                }
                *oldest = {s[i].id, s[i].hp, s[i].most, 0};
                continue;
            }
            found->age = 0;
            const float before = found->hp, after = s[i].hp;
            found->hp = after;
            found->most = s[i].most;
            if (after > before + .01f) {
                ++rises;
                continue;
            }
            if (!(after < before - .01f))
                continue;
            ++falls;
            if (before - after > biggestFall)
                biggestFall = before - after;
            const bool byMelee = meleeMs && nowMs >= meleeMs && nowMs - meleeMs <= windowMs;
            const bool byOurs = oursMs && nowMs >= oursMs && nowMs - oursMs <= windowMs;
            Event e{s[i].id, before, after, s[i].most, unexplained, 0, 0};
            if (byOurs && !byMelee) {
                e.cause = ours;
                e.delayMs = static_cast<uint32_t>(nowMs - oursMs);
                ++fallsOurs;
                writeState = 2;
            } else if (byMelee) {
                e.cause = melee;
                e.delayMs = static_cast<uint32_t>(nowMs - meleeMs);
                ++fallsMelee;
                healthState = 2;
            } else {
                ++fallsUnexplained;
            }
            if (before > 0 && after <= 0) {
                e.knockedOut = 1;
                ++knockouts;
            }
            recent[nextEvent % events] = e;
            ++nextEvent;
        }
    }
};
inline const char* stateName(uint32_t s) {
    return s >= 2 ? "CONFIRMED" : s == 1 ? "read" : "none";
}

} // namespace spidy::damage_probe
