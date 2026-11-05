#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
// HERO VR 0.0.78 cut16 SANDMAN (pure and allocation-free; nothing here reads or writes the game).
//
// The fight (the player, 0.0.78 cut15 correction): debris thrown back at Sandman (the game's LB + RB) until he is
// low, then Y zips to him when the game shows its Y on his head. Public 0.0.77 and cut8: both stop at 233.3/250
// (250 x 14/15) after the throws, the last hit cut short (cut8 236.5 -> 233.3, public 235.0 -> 233.3) and Thor's and
// Iron Man's 445 HP of mod blows after it moved nothing either: the game's own gate, the web strike's turn. In cut8 the
// next Y took him to 1/250 and the next phase's records came.
//
// BossPick: which boss record the mod tracks. Several SandmanBot records live at once (public 0.0.77: 0x265176412c0
// at 250/250, never hit, and 0x265179cdcc0, the one the throws took to 233.3); the nearest one flipped between them
// every scan, so THROW WATCH read "2 s 250.0" and a hero's blow could go to the record nobody fights. A record whose
// HP fell in the last hitHoldSeconds goes first (the latest hit), then a living one, the nearest; the dead last.
//
// BossGate: the throws that no longer move his HP (gateThrows in a row, below his most and above 0) - the game's
// web strike turn - and each Y press in a boss fight with his HP before it and the lowest over yWatchSeconds after,
// so one test log says whether the Y lands. Made by Agent Mulder.
#include <cmath>
#include <cstdint>
namespace spidy::cut16 {
inline constexpr float bossFightMetres = 200.f; // a living boss read this close: the boss fight (record: his feet)
inline constexpr float hitHoldSeconds = 20.f;
inline constexpr uint32_t gateThrows = 2;
inline constexpr float yWatchSeconds = 2.f;
inline constexpr float hpStep = .05f; // smaller HP changes are no hit
// live.txt boss_gate: 0 on (the default), 1 off (0.0.77's Y: InteractStep's turn and walk in a boss fight too, no
// gate card).
enum BossGateMode : uint32_t { bossGateOn = 0, bossGateOff = 1 };

struct BossPick {
    struct In {
        uint64_t id{};
        float metres{};
        bool have{};
        float hp{-1.f}, most{};
    };
    struct Slot {
        uint64_t id{};
        float hp{-1.f};
        double hitAt{-1e9}, seenAt{-1e9};
    };
    static constexpr uint32_t slots = 16;
    Slot s[slots]{};
    Slot* find(uint64_t id) {
        for (Slot& x : s)
            if (x.id == id)
                return &x;
        return nullptr;
    }
    // One scan's boss records (n of them) at `now` seconds: the index to track, -1 none.
    int pick(const In* in, uint32_t n, double now) {
        int best = -1;
        int bestRank = -1;
        double bestHit = -1e18;
        float bestMetres = 1e30f;
        for (uint32_t i = 0; i < n; ++i) {
            const In& b = in[i];
            if (!b.id || !std::isfinite(b.metres))
                continue;
            Slot* x = find(b.id);
            if (!x) { // the slot seen longest ago
                x = &s[0];
                for (Slot& y : s)
                    if (y.seenAt < x->seenAt)
                        x = &y;
                *x = Slot{b.id};
            }
            const bool hpOk = b.have && std::isfinite(b.hp);
            if (hpOk && x->hp >= 0 && b.hp < x->hp - hpStep)
                x->hitAt = now;
            if (hpOk)
                x->hp = b.hp;
            x->seenAt = now;
            const bool dead = hpOk && b.hp <= 0.f;
            const bool hit = !dead && now - x->hitAt <= hitHoldSeconds;
            const int rank = dead ? 0 : hit ? 2 : 1;
            const bool better = rank > bestRank ||
                                (rank == bestRank && (rank == 2 ? (x->hitAt > bestHit ||
                                                                   (x->hitAt == bestHit && b.metres < bestMetres))
                                                                : b.metres < bestMetres));
            if (better) {
                best = static_cast<int>(i);
                bestRank = rank;
                bestHit = x->hitAt;
                bestMetres = b.metres;
            }
        }
        return best;
    }
};

struct BossGate {
    // Throws.
    uint32_t still{};      // throws in a row that moved nothing
    float heldHp{-1.f};    // the HP the gate holds at (-1 none)
    uint32_t gates{};      // gates found this session
    // A throw's report (cut8::ThrowWatch): his HP as it started and the lowest over its watch; full: watched the whole
    // 2 s (a throw cut short by the next one neither counts nor resets - its debris may land in the next one's watch).
    // True once per gate: the throw that makes gateThrows full ones in a row with no hit while he is hurt and alive.
    bool thrown(bool have, float hp0, float lowest, float most, bool full = true) {
        if (!have || !std::isfinite(hp0) || !std::isfinite(most) || most <= 0)
            return false;
        if (!full && !(std::isfinite(lowest) && lowest >= 0 && lowest < hp0 - hpStep))
            return false;
        const bool hurt = hp0 > 0 && hp0 < most - hpStep;
        const bool moved = std::isfinite(lowest) && lowest >= 0 && lowest < hp0 - hpStep;
        if (moved || !hurt) {
            still = 0;
            if (heldHp >= 0 && (!hurt || (moved && lowest < heldHp - hpStep)))
                heldHp = -1.f;
            return false;
        }
        if (heldHp >= 0 && std::fabs(hp0 - heldHp) > hpStep)
            heldHp = -1.f;
        if (++still < gateThrows || heldHp >= 0)
            return false;
        heldHp = hp0;
        ++gates;
        return true;
    }
    // His HP read now (any frame): a fall below the held HP ends the gate.
    void hp(bool have, float now) {
        if (have && std::isfinite(now) && heldHp >= 0 && now < heldHp - hpStep) {
            heldHp = -1.f;
            still = 0;
        }
    }
    void lost() { // no boss read (he is gone, or the fight is)
        heldHp = -1.f;
        still = 0;
    }
    bool holding() const { return heldHp >= 0; }

    // Y presses in a boss fight.
    struct YReport {
        uint32_t no{};
        bool have0{}, gated{}, plain{};
        float hp0{-1.f}, lowest{-1.f}, metres{-1.f};
    };
    bool yOn{};
    float yT{};
    YReport y{};
    uint32_t ys{};
    // A Y went out to the game (its rising edge). True with out filled if a watched one is cut short.
    bool yPressed(bool have, float hpNow, float metres, bool plain, YReport& out) {
        const bool cut = yOn;
        if (cut)
            out = y;
        y = {};
        y.no = ++ys;
        y.have0 = have && std::isfinite(hpNow);
        y.hp0 = y.lowest = y.have0 ? hpNow : -1.f;
        y.metres = std::isfinite(metres) ? metres : -1.f;
        y.gated = holding();
        y.plain = plain;
        yOn = true;
        yT = 0;
        return cut;
    }
    // Each frame: true at yWatchSeconds with out filled.
    bool yUpdate(float dt, bool have, float hpNow, YReport& out) {
        if (!yOn)
            return false;
        dt = std::isfinite(dt) ? (dt < 0 ? 0.f : dt > .1f ? .1f : dt) : 0.f;
        yT += dt;
        if (have && std::isfinite(hpNow) && (y.lowest < 0 || hpNow < y.lowest))
            y.lowest = hpNow;
        if (yT >= yWatchSeconds) {
            yOn = false;
            out = y;
            return true;
        }
        return false;
    }
    static bool landed(const YReport& r) { return r.have0 && r.lowest >= 0 && r.lowest < r.hp0 - hpStep; }
};
} // namespace spidy::cut16
