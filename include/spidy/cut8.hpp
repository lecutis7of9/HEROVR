#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
// HERO VR 0.0.77 CUT8 (diagnostics; nothing here reads or writes the game; pure and allocation-free).
// ThrowWatch (live.txt throw_watch, 5598): each LB + RB throw the boss's HP as the throw starts and at 0.5 s, 1 s and
// 2 s after (and the lowest over the 2 s), the left stick sent the game that frame, the throw aim and the boss's
// distance - one log line at 2 s (the player, t1846u: "when you throw the debris at sandman if it hits him it is
// supposed to DAMAGE him"). Handle: the game registry's 32-bit handle (generation << 20 | index), the form every
// registry entry's own handle has (world_trace.cpp checks head.handle == generation << 20 | i), used by the cut8
// POI TARGET diagnostic on the GUI2 POI components' words. Made by Agent Mulder.
#include <cmath>
#include <cstdint>
namespace spidy::cut8 {
enum ThrowWatchMode : uint32_t { throwWatchOn = 0, throwWatchOff = 1 };
// A 32-bit word that has the registry handle's form: an index below count (and not the all-ones "none"), a
// generation 1..0xfff.
inline bool handleParts(uint32_t h, uint32_t count, uint32_t& index, uint32_t& generation) {
    index = h & 0xfffffu;
    generation = h >> 20;
    return index != 0xfffffu && index < count && generation >= 1 && generation <= 0xfffu;
}
struct ThrowWatch {
    static constexpr float span = 2.f;
    static constexpr float marks[3] = {.5f, 1.f, 2.f};
    struct Report {
        uint32_t throwNo{};
        bool have0{};
        float hp0{-1.f}, most0{}, hpAt[3]{-1.f, -1.f, -1.f}, lowest{-1.f};
        float metres0{-1.f}, metresEnd{-1.f};
        int16_t stickX{}, stickY{};
        float aimDegrees{};
        bool aimActive{};
        uint32_t bossSeen{}; // frames with a boss read
    };
    bool active{};
    float t{};
    Report r{};
    uint32_t throws{};
    // A throw starts (the rising LB + RB edge): the boss's state (have: its HP read), the stick sent, the aim.
    // A throw still watched is reported cut short (returns true with out filled).
    bool start(bool bossKnown, bool have, float hp, float most, float metres, int16_t sx, int16_t sy, bool aimOn,
               float degrees, Report& out) {
        const bool cut = active;
        if (cut)
            out = r;
        r = {};
        r.throwNo = ++throws;
        r.have0 = bossKnown && have && std::isfinite(hp);
        r.hp0 = r.have0 ? hp : -1.f;
        r.most0 = r.have0 ? most : 0.f;
        r.lowest = r.hp0;
        r.metres0 = bossKnown && std::isfinite(metres) ? metres : -1.f;
        r.stickX = sx, r.stickY = sy;
        r.aimActive = aimOn;
        r.aimDegrees = std::isfinite(degrees) ? degrees : 0.f;
        active = true;
        t = 0;
        return cut;
    }
    // Each frame while active: true at 2 s (out: the report).
    bool update(float dt, bool bossKnown, bool have, float hp, float metres, Report& out) {
        if (!active)
            return false;
        dt = std::isfinite(dt) ? (dt < 0 ? 0.f : dt > .1f ? .1f : dt) : 0.f;
        const float before = t;
        t += dt;
        if (bossKnown) {
            ++r.bossSeen;
            if (std::isfinite(metres))
                r.metresEnd = metres;
            if (have && std::isfinite(hp)) {
                if (r.lowest < 0 || hp < r.lowest)
                    r.lowest = hp;
                for (int i = 0; i < 3; ++i)
                    if (before < marks[i] && t >= marks[i])
                        r.hpAt[i] = hp;
            }
        }
        if (t >= span) {
            active = false;
            out = r;
            return true;
        }
        return false;
    }
};
} // namespace spidy::cut8
