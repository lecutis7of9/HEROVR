#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
//
// 0.0.74 test 10 grapple fixes (owner pt1362u-pt1364u: "punishers grapple takes so long to fire", "half
// the time i press it multiple times to get it to shoot", "grapples still dont grab enemies to reel them in").
//
// SURE GRAPPLE (AskLatch): the VR loop runs at the headset's rate (~90 Hz) and the swing bridge reads only
// the newest command once a game tick (~40 Hz), so a grapple ask that is true for one VR frame is often
// overwritten before the bridge sees it. The latch holds a press's ask until the bridge answers it - its
// grapple or miss count (cape_glide::Data) rises - or holdFor seconds pass, for every grappler hero.
//
// GRAPPLE YANK (CANDIDATE): a grapple that reels onto an enemy (the bridge's ENEMY REEL) also asks the game
// for a knockback on him whose direction runs from him to the hero (sm2_damage::Blow::toward, DirectDamage
// RVA 0x3412ec0, CONFIRMED path; the pull itself is CANDIDATE until a run shows the enemy move). The hero's
// own reel to him stays (the fallback). YankWatch reports, per yank, whether the enemy really moved.
// Never the game's web yank (no web is ever shown for a grapple).
#include <cmath>
#include <cstdint>

#include "spidy/math.hpp"
#include "spidy/sm2_damage.hpp"

namespace spidy::grapple074 {

struct AskLatch {
    static constexpr double holdFor = .15; // seconds a press is held at most with no answer
    bool rawWas[2]{}, pending[2]{};
    double at[2]{};
    uint32_t grapplesSeen{}, missesSeen{};
    bool seen{};
    uint64_t armed{}, answered{}, expired{};
    // This frame's ask for hand i: the raw ask, or a press of the last holdFor seconds not yet answered.
    // on false (SURE GRAPPLE OFF, or a lock): the raw ask only, nothing kept.
    bool ask(unsigned i, bool raw, double now, bool on) {
        i = i < 2 ? i : 1u;
        if (!on || !std::isfinite(now)) {
            pending[i] = false;
            rawWas[i] = raw;
            return raw;
        }
        if (raw && !rawWas[i]) {
            pending[i] = true;
            at[i] = now;
            ++armed;
        }
        rawWas[i] = raw;
        if (pending[i] && !(now - at[i] < holdFor)) {
            pending[i] = false;
            ++expired;
        }
        return raw || pending[i];
    }
    // The bridge's counts (cape_glide::Data grapples, misses) after a good sample: a rise answers the asks.
    void answer(uint32_t grapples, uint32_t misses) {
        if (seen && (grapples != grapplesSeen || misses != missesSeen)) {
            for (bool& p : pending)
                if (p) {
                    p = false;
                    ++answered;
                }
        }
        grapplesSeen = grapples;
        missesSeen = misses;
        seen = true;
    }
    bool holding(unsigned i) const { return pending[i < 2 ? i : 1u]; }
};

// The yank's blow: 1 HP (the request needs an amount), the melee knockback kind 5, pulled toward the hero.
inline sm2_damage::Blow yankBlow() {
    sm2_damage::Blow b{1.f, 5, 160.f};
    b.toward = true;
    return b;
}

// One yank's check: where the enemy and the hero were, then, after watchFor seconds, where the enemy is.
struct YankWatch {
    static constexpr double watchFor = .5;
    static constexpr float movedLeast = .4f; // metres toward the hero that count as "moved"
    uint64_t id{};
    Vec3 enemyFrom{}, heroFrom{};
    double at{};
    bool live{};
    void start(uint64_t enemy, Vec3 enemyAt, Vec3 heroAt, double now) {
        id = enemy;
        enemyFrom = enemyAt;
        heroFrom = heroAt;
        at = now;
        live = enemy && finite(enemyAt) && finite(heroAt) && std::isfinite(now);
    }
    bool due(double now) const { return live && !(now - at < watchFor); }
    // How far he came toward where the hero was (metres; + toward), and the whole move.
    struct Verdict {
        bool moved{};
        float toward{}, distance{};
    };
    Verdict judge(Vec3 enemyNow) const {
        Verdict v;
        if (!finite(enemyNow))
            return v;
        v.distance = length(enemyNow - enemyFrom);
        v.toward = length(heroFrom - enemyFrom) - length(heroFrom - enemyNow);
        v.moved = std::isfinite(v.toward) && v.toward >= movedLeast;
        return v;
    }
};

} // namespace spidy::grapple074
