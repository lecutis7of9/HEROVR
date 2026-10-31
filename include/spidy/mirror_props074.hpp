#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
// Made by Agent Mulder.
//
// 0.0.74 test 10 MIRROR PROPS (owner pt1360u: "weapons and mirror ... they dont show up"): 0.0.73's fix 14
// put the hero007 props (guns, shield, claws, batarangs, grapple gun, holsters) and the DD batons in the
// mirror, never the 0.0.72 look. `heroLook` is the part of a hero072::Look the mirror shows: THOR's hammer
// (in the fist, flying or SHEATHED on the back - the mirror sees the back's edge; first person never draws
// it there), his lightning, and GREEN LANTERN's constructs (ring glow, beam, bubble, chain, fist). Left out:
// what only the player's own eyes see (IRON HUD, lock box, AIM HINT, the charge meter, the door handle
// marker), the van / Batmobile (a world object, not the hero), the blood, sprays, shock rings, hooks and
// the perch light. Pure (no models loaded, no drawing): tested in tests/mirror_props074_tests.cpp.
#include "hero072_visual.hpp"

#include <algorithm>

namespace spidy::mirror_props {

inline hero072::Look heroLook(const hero072::Look& l) {
    hero072::Look m{};
    if (!l.any)
        return m;
    if (l.hammer && l.hammerModel) {
        m.hammer = true;
        m.hammerAt = l.hammerAt;
        m.hammerForward = l.hammerForward;
        m.hammerUp = l.hammerUp;
        m.hammerSpin = l.hammerSpin;
        m.hammerSheathed = false; // drawn where it hangs (appendLook skips a sheathed one)
        m.hammerInHand = l.hammerInHand;
        m.hammerVel = l.hammerVel;
        m.hammerSpinRate = l.hammerSpinRate;
        m.hammerAheadMost = l.hammerAheadMost;
        m.hammerStamp = l.hammerStamp;
        m.hammerChoice = l.hammerChoice;
        m.hammerModel = l.hammerModel;
    }
    m.bolts = std::min(l.bolts, hero072::boltsMost);
    for (uint32_t k = 0; k < m.bolts; ++k) {
        m.boltFrom[k] = l.boltFrom[k];
        m.boltTo[k] = l.boltTo[k];
        m.boltSeed[k] = l.boltSeed[k];
    }
    for (int i = 0; i < 2; ++i) {
        m.ring[i] = l.ring[i];
        m.ringAt[i] = l.ringAt[i];
    }
    m.beam = l.beam, m.beamFrom = l.beamFrom, m.beamTo = l.beamTo;
    m.bubble = l.bubble, m.bubbleAt = l.bubbleAt, m.bubbleRadius = l.bubbleRadius;
    m.chain = l.chain, m.chainFrom = l.chainFrom, m.chainTo = l.chainTo;
    m.fist = l.fist, m.fistAt = l.fistAt, m.fistForward = l.fistForward, m.fistAlpha = l.fistAlpha;
    m.any = m.hammer || m.bolts > 0 || m.ring[0] || m.ring[1] || m.beam || m.bubble || m.chain || m.fist;
    return m;
}

// A rise in the model sink's misses worth a log line: a new miss, at most once per `gapMs` (the first at once).
struct MissLog {
    uint64_t seen{}, logged{}, lastMs{};
    bool started{};
    // `total`: misses so far; `nowMs`: a steady clock. True: log `total - logged` now (then remembered).
    bool due(uint64_t total, uint64_t nowMs, uint64_t gapMs = 10000) {
        seen = total;
        if (total <= logged)
            return false;
        if (started && nowMs - lastMs < gapMs)
            return false;
        started = true;
        lastMs = nowMs;
        return true;
    }
    uint64_t take() {
        const uint64_t n = seen - logged;
        logged = seen;
        return n;
    }
};

} // namespace spidy::mirror_props
