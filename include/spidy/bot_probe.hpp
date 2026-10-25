#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
//
// Version 0.0.3 BOT STATE PROBE (read-only; runs with ROPE GRAB or WALL STICK on): the step toward
// moving an enemy with the rope. SpidyVR (SM1) flew a thug by writing the flight velocity into
// his BotStateFlungLocal each step; SM2 has the same class names (RTTI: BotStateFlung 0x530f108,
// BotStateFlungLocal 0x5359708, BotStateWallStuck 0x530fad0, all CANDIDATE) but no confirmed path
// from an enemy to his state. Twice a second the probe looks two pointers deep from the watched
// enemy's record (the one on the rope, else the nearest) for objects whose class starts with
// "BotState", and logs where each was found and which states came and went. While a Flung state
// is seen it snapshots that object 10 times a second with the enemy's own measured velocity, so
// the reports can find the velocity field offline (next build). Nothing is written or called.
#include <cstdint>
#include <cstring>

namespace spidy::bot_probe {

inline constexpr uint32_t recordBytes = 0x400, objectBytes = 0x200, firstLevel = 48, found = 16, snaps = 12;
inline constexpr uint32_t nameChars = 40, snapFloats = 64;

inline bool isBotState(const char* name) {
    return name && std::strncmp(name, "BotState", 8) == 0;
}
inline bool isFlung(const char* name) {
    return isBotState(name) && std::strstr(name, "Flung") != nullptr;
}
inline bool isWallStuck(const char* name) {
    return isBotState(name) && std::strstr(name, "WallStuck") != nullptr;
}

struct Found {
    uint16_t first{}, second{}; // record + first -> object (+ second -> object when level 2)
    uint32_t level{}, vtableRva{}, seen{};
    char name[nameChars]{};
};
static_assert(sizeof(Found) == 56);
struct Snap {
    float seconds{}, vx{}, vy{}, vz{};
    uint32_t vtableRva{}, pad{};
    float words[snapFloats]{};
};
static_assert(sizeof(Snap) == 280);
struct State {
    uint64_t scans{}, objectsRead{}, botStates{}, flungSeen{}, wallStuckSeen{}, changes{}, faults{};
    uint64_t watched{};      // the record looked at last
    uint32_t foundCount{}, snapCount{};
    Found list[found]{};
    Snap snap[snaps]{};
};

// Adds (or counts again) one find; returns false when the list is full.
inline bool note(State& s, uint32_t level, uint32_t first, uint32_t second, uint32_t vtableRva, const char* name) {
    for (uint32_t i = 0; i < s.foundCount; ++i)
        if (s.list[i].vtableRva == vtableRva && s.list[i].first == first && s.list[i].second == second) {
            ++s.list[i].seen;
            return true;
        }
    if (s.foundCount >= found)
        return false;
    Found& f = s.list[s.foundCount++];
    f.level = level;
    f.first = static_cast<uint16_t>(first);
    f.second = static_cast<uint16_t>(second);
    f.vtableRva = vtableRva;
    f.seen = 1;
    const char* from = name ? name : "";
    size_t n = 0;
    while (n + 1 < nameChars && from[n])
        ++n;
    std::memcpy(f.name, from, n); // bounded, no CRT-deprecated call (MSVC C4996)
    f.name[n] = 0;
    return true;
}

// Offline helper (and the tests'): the float index in a snapshot whose run of three best follows
// the measured velocity across the snaps (-1: none within tolerance).
inline int velocityIndex(const Snap* s, uint32_t n, float tolerance = .75f) {
    if (n < 3)
        return -1;
    bool moving = false; // a still enemy matches any zeros: need a real flight
    for (uint32_t k = 0; k < n; ++k)
        moving = moving || s[k].vx * s[k].vx + s[k].vy * s[k].vy + s[k].vz * s[k].vz > 4.f;
    if (!moving)
        return -1;
    int best = -1;
    float bestErr = tolerance;
    for (uint32_t i = 0; i + 2 < snapFloats; ++i) {
        float err = 0;
        bool ok = true;
        for (uint32_t k = 0; k < n && ok; ++k) {
            const float dx = s[k].words[i] - s[k].vx, dy = s[k].words[i + 1] - s[k].vy, dz = s[k].words[i + 2] - s[k].vz;
            const float e = dx * dx + dy * dy + dz * dz;
            ok = e == e && e < 1e6f;
            err += e;
        }
        const float mean = err / static_cast<float>(n);
        if (ok && mean < bestErr) {
            bestErr = mean;
            best = static_cast<int>(i);
        }
    }
    return best;
}
} // namespace spidy::bot_probe
