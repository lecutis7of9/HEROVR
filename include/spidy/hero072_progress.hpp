#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
// Made by Agent Mulder.
//
// Version 0.0.72 (TEST): the reputation journey's PERK TREES and ULTIMATES (the user, 8:46, 8:47).
// Pure rules, no game calls. All the data is in the two tables below (perkTable, ultimateTable):
// change a perk or an ultimate there, nothing else.
// - PERKS: every reputation level (hero072::repLevel, a level each repTuning.levelEvery points) is
//   one perk point for THAT hero. Each hero has 3 branches of 4 perks (12), bought in order within a
//   branch (tier 0 first), tiers 0-1 cost 1 point, tiers 2-3 cost 2 (18 for a whole tree, of 20).
//   HERO PERKS (the setting) says where the next points go: AUTO (round the branches) or a branch;
//   points are spent as they come. A perk is a small upgrade to one stat of the hero's kit
//   (statBonus: +amount, 0.08 = +8 %; chainJumps whole jumps). FIRST PASS: the numbers are
//   placeholders to tune; game_xr reads the stats it already has a knob for (docs/HERO-0.0.72.md
//   lists which), the rest are kept for when their feature is wired.
// - ULTIMATE: the tree's capstone, only at the hero's MAX reputation (or TEST UNLOCKS). Fired by a
//   gesture (no trigger): both fists crossed on the chest with both grips held for ultimateHold
//   seconds; then a long cooldown. Its effect: `hits` blows (sm2_damage, CONFIRMED) on every
//   standing enemy within `radius` over `seconds`, plus its look.
// - Saved for good: "perks.<hero>=<mask>" lines (only ever OR-ed back in).
#include "spidy/hero072.hpp"
#include "spidy/holster.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace spidy::hero072 {

enum class Stat : uint32_t;
struct Perk {
    uint32_t hero, branch, tier;
    const char* branchName;
    const char* name;
    Stat stat;
    float amount;
    uint32_t cost;
};
struct Ultimate {
    const char* name;
    float radius;
    uint32_t hits;
    sm2_damage::Blow blow;
    float seconds, cooldown;
    const char* look;
};
enum class Stat : uint32_t { armor, beamDamage, blockStrength, bubbleSeconds, chainJumps, chargeMost, clawDamage, cooldown, fistSize, flySpeed, freezeDamage, grappleSpeed, healRate, heatDamage, hookSpeed, jumpHeight, lightningDamage, punchDamage, rageDamage, reloadSpeed, repulsorDamage, shotDamage, slamRadius, smokeSeconds, sonarRange, sprintSpeed, stormRadius, throwDamage, uppercutLaunch, stats };
inline constexpr const char* statNames[static_cast<uint32_t>(Stat::stats)] = {"armor", "beamDamage", "blockStrength", "bubbleSeconds", "chainJumps", "chargeMost", "clawDamage", "cooldown", "fistSize", "flySpeed", "freezeDamage", "grappleSpeed", "healRate", "heatDamage", "hookSpeed", "jumpHeight", "lightningDamage", "punchDamage", "rageDamage", "reloadSpeed", "repulsorDamage", "shotDamage", "slamRadius", "smokeSeconds", "sonarRange", "sprintSpeed", "stormRadius", "throwDamage", "uppercutLaunch"};
inline constexpr Perk perkTable[] = {
    // SPIDER-MAN
    {0, 0, 0, "COMBAT", "WEB FISTS", Stat::punchDamage, 0.08f, 1},
    {0, 0, 1, "COMBAT", "SPIDER-SENSE", Stat::armor, 0.06f, 1},
    {0, 0, 2, "COMBAT", "HEAVY HITTER", Stat::punchDamage, 0.12f, 2},
    {0, 0, 3, "COMBAT", "LAUNCHER", Stat::uppercutLaunch, 0.15f, 2},
    {0, 1, 0, "MOBILITY", "QUICK ZIP", Stat::grappleSpeed, 0.06f, 1},
    {0, 1, 1, "MOBILITY", "WALL RUNNER", Stat::sprintSpeed, 0.06f, 1},
    {0, 1, 2, "MOBILITY", "SLINGSHOT", Stat::grappleSpeed, 0.1f, 2},
    {0, 1, 3, "MOBILITY", "AIR TRICKS", Stat::jumpHeight, 0.1f, 2},
    {0, 2, 0, "GADGETS", "WEB BOMB", Stat::throwDamage, 0.1f, 1},
    {0, 2, 1, "GADGETS", "SPIDER DRONE", Stat::cooldown, 0.06f, 1},
    {0, 2, 2, "GADGETS", "IMPACT WEB", Stat::throwDamage, 0.15f, 2},
    {0, 2, 3, "GADGETS", "WEB OVERLOAD", Stat::cooldown, 0.1f, 2},
    // IRON MAN
    {1, 0, 0, "WEAPONS", "REPULSOR TUNE", Stat::repulsorDamage, 0.08f, 1},
    {1, 0, 1, "WEAPONS", "TARGETING", Stat::repulsorDamage, 0.08f, 1},
    {1, 0, 2, "WEAPONS", "UNIBEAM COILS", Stat::beamDamage, 0.12f, 2},
    {1, 0, 3, "WEAPONS", "MICRO MISSILES", Stat::shotDamage, 0.12f, 2},
    {1, 1, 0, "FLIGHT", "THRUSTER TUNE", Stat::flySpeed, 0.06f, 1},
    {1, 1, 1, "FLIGHT", "STABILISERS", Stat::armor, 0.05f, 1},
    {1, 1, 2, "FLIGHT", "AFTERBURNER", Stat::flySpeed, 0.1f, 2},
    {1, 1, 3, "FLIGHT", "HOVER JETS", Stat::cooldown, 0.06f, 2},
    {1, 2, 0, "ARMOR", "PLATING", Stat::armor, 0.06f, 1},
    {1, 2, 1, "ARMOR", "ARC REACTOR", Stat::chargeMost, 0.1f, 1},
    {1, 2, 2, "ARMOR", "REINFORCED", Stat::armor, 0.08f, 2},
    {1, 2, 3, "ARMOR", "NANO REPAIR", Stat::healRate, 0.15f, 2},
    // DAREDEVIL
    {2, 0, 0, "COMBAT", "BATON STRIKES", Stat::punchDamage, 0.08f, 1},
    {2, 0, 1, "COMBAT", "COUNTER", Stat::blockStrength, 0.1f, 1},
    {2, 0, 2, "COMBAT", "RICOCHET", Stat::throwDamage, 0.12f, 2},
    {2, 0, 3, "COMBAT", "DEVIL'S FURY", Stat::punchDamage, 0.12f, 2},
    {2, 1, 0, "MOBILITY", "QUICK CAST", Stat::grappleSpeed, 0.06f, 1},
    {2, 1, 1, "MOBILITY", "ROOFTOP RUN", Stat::sprintSpeed, 0.06f, 1},
    {2, 1, 2, "MOBILITY", "LONG LINE", Stat::grappleSpeed, 0.1f, 2},
    {2, 1, 3, "MOBILITY", "ACROBAT", Stat::jumpHeight, 0.1f, 2},
    {2, 2, 0, "SENSES", "RADAR RANGE", Stat::sonarRange, 0.1f, 1},
    {2, 2, 1, "SENSES", "HEARTBEATS", Stat::sonarRange, 0.1f, 1},
    {2, 2, 2, "SENSES", "DANGER SENSE", Stat::armor, 0.06f, 2},
    {2, 2, 3, "SENSES", "WORLD ON FIRE", Stat::sonarRange, 0.15f, 2},
    // HULK
    {3, 0, 0, "RAGE", "ANGRIER", Stat::rageDamage, 0.08f, 1},
    {3, 0, 1, "RAGE", "SLOW BURN", Stat::rageDamage, 0.08f, 1},
    {3, 0, 2, "RAGE", "HULK SMASH", Stat::punchDamage, 0.12f, 2},
    {3, 0, 3, "RAGE", "WORLD BREAKER", Stat::rageDamage, 0.15f, 2},
    {3, 1, 0, "MOBILITY", "BIG LEGS", Stat::jumpHeight, 0.08f, 1},
    {3, 1, 1, "MOBILITY", "STAMPEDE", Stat::sprintSpeed, 0.06f, 1},
    {3, 1, 2, "MOBILITY", "GIANT LEAP", Stat::jumpHeight, 0.12f, 2},
    {3, 1, 3, "MOBILITY", "LANDING", Stat::slamRadius, 0.12f, 2},
    {3, 2, 0, "TOUGHNESS", "THICK SKIN", Stat::armor, 0.06f, 1},
    {3, 2, 1, "TOUGHNESS", "REGROWTH", Stat::healRate, 0.1f, 1},
    {3, 2, 2, "TOUGHNESS", "UNSTOPPABLE", Stat::armor, 0.08f, 2},
    {3, 2, 3, "TOUGHNESS", "THUNDER HANDS", Stat::slamRadius, 0.15f, 2},
    // BATMAN
    {4, 0, 0, "COMBAT", "FREEFLOW", Stat::punchDamage, 0.08f, 1},
    {4, 0, 1, "COMBAT", "COUNTER", Stat::blockStrength, 0.1f, 1},
    {4, 0, 2, "COMBAT", "BATARANG EDGE", Stat::throwDamage, 0.12f, 2},
    {4, 0, 3, "COMBAT", "TAKEDOWN", Stat::punchDamage, 0.12f, 2},
    {4, 1, 0, "MOBILITY", "GRAPPLE MOTOR", Stat::grappleSpeed, 0.08f, 1},
    {4, 1, 1, "MOBILITY", "CAPE GLIDE", Stat::flySpeed, 0.06f, 1},
    {4, 1, 2, "MOBILITY", "GRAPPLE BOOST", Stat::grappleSpeed, 0.12f, 2},
    {4, 1, 3, "MOBILITY", "DIVE BOMB", Stat::jumpHeight, 0.1f, 2},
    {4, 2, 0, "GADGETS", "THICK SMOKE", Stat::smokeSeconds, 0.15f, 1},
    {4, 2, 1, "GADGETS", "SPARE PELLETS", Stat::cooldown, 0.06f, 1},
    {4, 2, 2, "GADGETS", "LONG SMOKE", Stat::smokeSeconds, 0.2f, 2},
    {4, 2, 3, "GADGETS", "UTILITY BELT", Stat::cooldown, 0.1f, 2},
    // WOLVERINE
    {5, 0, 0, "CLAWS", "SHARP", Stat::clawDamage, 0.08f, 1},
    {5, 0, 1, "CLAWS", "FERAL", Stat::clawDamage, 0.08f, 1},
    {5, 0, 2, "CLAWS", "LUNGE", Stat::punchDamage, 0.12f, 2},
    {5, 0, 3, "CLAWS", "BERSERKER", Stat::clawDamage, 0.15f, 2},
    {5, 1, 0, "HEALING", "FAST HEALER", Stat::healRate, 0.1f, 1},
    {5, 1, 1, "HEALING", "TOUGH", Stat::armor, 0.05f, 1},
    {5, 1, 2, "HEALING", "REGENERATE", Stat::healRate, 0.15f, 2},
    {5, 1, 3, "HEALING", "ADAMANTIUM", Stat::armor, 0.08f, 2},
    {5, 2, 0, "HUNTER", "SCENT", Stat::sonarRange, 0.1f, 1},
    {5, 2, 1, "HUNTER", "CLIMBER", Stat::sprintSpeed, 0.06f, 1},
    {5, 2, 2, "HUNTER", "POUNCE", Stat::jumpHeight, 0.1f, 2},
    {5, 2, 3, "HUNTER", "PREDATOR", Stat::punchDamage, 0.1f, 2},
    // SUPERMAN
    {6, 0, 0, "POWERS", "HEAT FOCUS", Stat::heatDamage, 0.08f, 1},
    {6, 0, 1, "POWERS", "ARCTIC BREATH", Stat::freezeDamage, 0.1f, 1},
    {6, 0, 2, "POWERS", "HEAT BEAM", Stat::heatDamage, 0.12f, 2},
    {6, 0, 3, "POWERS", "SOLAR", Stat::cooldown, 0.1f, 2},
    {6, 1, 0, "FLIGHT", "FASTER", Stat::flySpeed, 0.06f, 1},
    {6, 1, 1, "FLIGHT", "SONIC", Stat::flySpeed, 0.08f, 1},
    {6, 1, 2, "FLIGHT", "MACH", Stat::flySpeed, 0.12f, 2},
    {6, 1, 3, "FLIGHT", "ORBITAL", Stat::flySpeed, 0.15f, 2},
    {6, 2, 0, "STRENGTH", "STEEL FISTS", Stat::punchDamage, 0.08f, 1},
    {6, 2, 1, "STRENGTH", "UPPERCUT", Stat::uppercutLaunch, 0.12f, 1},
    {6, 2, 2, "STRENGTH", "MAN OF STEEL", Stat::punchDamage, 0.12f, 2},
    {6, 2, 3, "STRENGTH", "KRYPTONIAN", Stat::punchDamage, 0.15f, 2},
    // PUNISHER
    {7, 0, 0, "GUNS", "STEADY AIM", Stat::shotDamage, 0.06f, 1},
    {7, 0, 1, "GUNS", "FAST HANDS", Stat::reloadSpeed, 0.1f, 1},
    {7, 0, 2, "GUNS", "HOLLOW POINTS", Stat::shotDamage, 0.1f, 2},
    {7, 0, 3, "GUNS", "SLUGS", Stat::shotDamage, 0.12f, 2},
    {7, 1, 0, "RAGE", "SHORT FUSE", Stat::rageDamage, 0.06f, 1},
    {7, 1, 1, "RAGE", "PAYBACK", Stat::rageDamage, 0.1f, 1},
    {7, 1, 2, "RAGE", "NO MERCY", Stat::punchDamage, 0.1f, 2},
    {7, 1, 3, "RAGE", "WAR JOURNAL", Stat::rageDamage, 0.15f, 2},
    {7, 2, 0, "GEAR", "HOOK MOTOR", Stat::hookSpeed, 0.1f, 1},
    {7, 2, 1, "GEAR", "FOAM CANS", Stat::cooldown, 0.06f, 1},
    {7, 2, 2, "GEAR", "KEVLAR", Stat::armor, 0.08f, 2},
    {7, 2, 3, "GEAR", "FAST HOOK", Stat::hookSpeed, 0.15f, 2},
    // CAPTAIN AMERICA
    {8, 0, 0, "SHIELD", "BALANCED THROW", Stat::throwDamage, 0.08f, 1},
    {8, 0, 1, "SHIELD", "RICOCHET", Stat::chainJumps, 1.0f, 1},
    {8, 0, 2, "SHIELD", "VIBRANIUM EDGE", Stat::throwDamage, 0.12f, 2},
    {8, 0, 3, "SHIELD", "BOUNCE MASTER", Stat::chainJumps, 1.0f, 2},
    {8, 1, 0, "DEFENSE", "BRACE", Stat::blockStrength, 0.1f, 1},
    {8, 1, 1, "DEFENSE", "SLAM", Stat::slamRadius, 0.1f, 1},
    {8, 1, 2, "DEFENSE", "SHIELD WALL", Stat::blockStrength, 0.15f, 2},
    {8, 1, 3, "DEFENSE", "SHOCKWAVE", Stat::slamRadius, 0.15f, 2},
    {8, 2, 0, "SOLDIER", "SUPER SOLDIER", Stat::punchDamage, 0.08f, 1},
    {8, 2, 1, "SOLDIER", "ENDURANCE", Stat::healRate, 0.1f, 1},
    {8, 2, 2, "SOLDIER", "BIG LEAP", Stat::jumpHeight, 0.1f, 2},
    {8, 2, 3, "SOLDIER", "SENTINEL", Stat::armor, 0.08f, 2},
    // THOR
    {9, 0, 0, "HAMMER", "BALANCED", Stat::throwDamage, 0.08f, 1},
    {9, 0, 1, "HAMMER", "LONG CHAIN", Stat::chainJumps, 1.0f, 1},
    {9, 0, 2, "HAMMER", "WORTHY", Stat::throwDamage, 0.12f, 2},
    {9, 0, 3, "HAMMER", "CHAIN MASTER", Stat::chainJumps, 1.0f, 2},
    {9, 1, 0, "LIGHTNING", "SPARK", Stat::lightningDamage, 0.08f, 1},
    {9, 1, 1, "LIGHTNING", "THUNDER", Stat::lightningDamage, 0.1f, 1},
    {9, 1, 2, "LIGHTNING", "STORM CALLER", Stat::stormRadius, 0.12f, 2},
    {9, 1, 3, "LIGHTNING", "GOD OF THUNDER", Stat::lightningDamage, 0.15f, 2},
    {9, 2, 0, "ASGARDIAN", "WINDS", Stat::flySpeed, 0.06f, 1},
    {9, 2, 1, "ASGARDIAN", "WARRIOR", Stat::punchDamage, 0.08f, 1},
    {9, 2, 2, "ASGARDIAN", "GALE", Stat::flySpeed, 0.1f, 2},
    {9, 2, 3, "ASGARDIAN", "ODINSON", Stat::punchDamage, 0.12f, 2},
    // GREEN LANTERN
    {10, 0, 0, "CONSTRUCTS", "SOLID LIGHT", Stat::punchDamage, 0.08f, 1},
    {10, 0, 1, "CONSTRUCTS", "BIG FIST", Stat::fistSize, 0.12f, 1},
    {10, 0, 2, "CONSTRUCTS", "LONG CHAIN", Stat::chainJumps, 1.0f, 2},
    {10, 0, 3, "CONSTRUCTS", "BIGGER FIST", Stat::fistSize, 0.15f, 2},
    {10, 1, 0, "RING", "BATTERY", Stat::chargeMost, 0.1f, 1},
    {10, 1, 1, "RING", "RECHARGE", Stat::cooldown, 0.08f, 1},
    {10, 1, 2, "RING", "POWER CELL", Stat::chargeMost, 0.15f, 2},
    {10, 1, 3, "RING", "WILLPOWER", Stat::beamDamage, 0.12f, 2},
    {10, 2, 0, "FLIGHT", "FASTER", Stat::flySpeed, 0.06f, 1},
    {10, 2, 1, "FLIGHT", "BUBBLE", Stat::bubbleSeconds, 0.12f, 1},
    {10, 2, 2, "FLIGHT", "STREAK", Stat::flySpeed, 0.1f, 2},
    {10, 2, 3, "FLIGHT", "FORTRESS", Stat::bubbleSeconds, 0.2f, 2},
};
inline constexpr Ultimate ultimateTable[hero_modes::heroes] = {
    {"WEB STORM", 10.f, 1, {25.f, 5, 300.f}, 2.0f, 180.f, "wraps every enemy near in webs"}, // SPIDER-MAN
    {"UNIBEAM", 30.f, 8, {30.f, 5, 420.f}, 2.5f, 180.f, "the chest's full-power beam"}, // IRON MAN
    {"RADAR BURST", 25.f, 1, {20.f, 5, 260.f}, 4.0f, 200.f, "enemies outlined through walls (slow-mo CANDIDATE)"}, // DAREDEVIL
    {"WORLD SMASH", 12.f, 1, {90.f, 5, 900.f}, 1.0f, 180.f, "a giant ground smash shockwave"}, // HULK
    {"FEAR TAKEDOWN", 8.f, 1, {80.f, 5, 500.f}, 1.5f, 200.f, "drops the group near"}, // BATMAN
    {"BERSERKER", 3.f, 20, {12.f, 5, 200.f}, 6.0f, 200.f, "nonstop claw slashes"}, // WOLVERINE
    {"HEAT SWEEP", 20.f, 10, {30.f, 5, 300.f}, 3.0f, 180.f, "heat vision swept across them"}, // SUPERMAN
    {"ROCKET BARRAGE", 25.f, 6, {60.f, 5, 600.f}, 3.0f, 200.f, "a rocket launcher barrage"}, // PUNISHER
    {"SHIELD STORM", 25.f, 1, {45.f, 5, 420.f}, 3.0f, 180.f, "the shield bounces through every enemy"}, // CAPTAIN AMERICA
    {"BIFROST", 12.f, 1, {120.f, 5, 800.f}, 1.5f, 220.f, "the Bifrost strikes the area"}, // THOR
    {"CRUSHER", 10.f, 1, {85.f, 5, 700.f}, 1.5f, 200.f, "a giant construct crushes the crowd"}, // GREEN LANTERN
};

inline constexpr uint32_t perkCount = sizeof perkTable / sizeof perkTable[0];
inline constexpr uint32_t branches = 3, tiers = 4, perksPerHero = branches * tiers;
static_assert(perkCount == hero_modes::heroes * perksPerHero, "12 perks a hero");
// The hero's perk (branch, tier) in the table, its index (0..11 in his mask).
inline const Perk& perk(uint32_t hero, uint32_t branch, uint32_t tier) {
    const uint32_t i = std::min(hero, hero_modes::heroes - 1) * perksPerHero + std::min(branch, branches - 1) * tiers +
                       std::min(tier, tiers - 1);
    return perkTable[i];
}
inline uint32_t perkBit(uint32_t branch, uint32_t tier) { return 1u << (branch * tiers + tier); }
inline constexpr uint32_t treeCost() {
    uint32_t c = 0;
    for (uint32_t t = 0; t < tiers; ++t)
        c += t < 2 ? 1 : 2;
    return c * branches;
}
static_assert(treeCost() <= static_cast<uint32_t>(repLevels), "a tree costs more than the levels give");

// HERO PERKS: 0 AUTO, 1-3 the branch.
class Perks {
  public:
    uint32_t owned[hero_modes::heroes]{};
    uint32_t nextAuto[hero_modes::heroes]{};
    bool dirty{};
    static uint32_t spentOf(uint32_t mask) {
        uint32_t s = 0;
        for (uint32_t b = 0; b < branches; ++b)
            for (uint32_t t = 0; t < tiers; ++t)
                if (mask & perkBit(b, t))
                    s += t < 2 ? 1 : 2;
        return s;
    }
    uint32_t points(uint32_t hero, int32_t rep, bool all) const {
        if (hero >= hero_modes::heroes)
            return 0;
        const uint32_t have = all ? treeCost() : static_cast<uint32_t>(repLevel(rep));
        const uint32_t spent = spentOf(owned[hero]);
        return have > spent ? have - spent : 0u;
    }
    // The next perk of a branch not owned (tiers in order), tiers when the branch is full.
    uint32_t nextTier(uint32_t hero, uint32_t branch) const {
        for (uint32_t t = 0; t < tiers; ++t)
            if (!(owned[hero] & perkBit(branch, t)))
                return t;
        return tiers;
    }
    // Spend what there is (pref 0 AUTO round the branches, 1-3 that branch, then the others). Returns
    // the perks bought.
    uint32_t spend(uint32_t hero, int32_t rep, uint32_t pref, bool all) {
        if (hero >= hero_modes::heroes)
            return 0;
        uint32_t bought = 0;
        for (int guard = 0; guard < 16; ++guard) {
            const uint32_t pts = points(hero, rep, all);
            bool any = false;
            for (uint32_t k = 0; k < branches && !any; ++k) {
                const uint32_t b = pref >= 1 && pref <= branches ? (pref - 1 + k) % branches : (nextAuto[hero] + k) % branches;
                const uint32_t t = nextTier(hero, b);
                if (t >= tiers || perk(hero, b, t).cost > pts)
                    continue;
                owned[hero] |= perkBit(b, t);
                if (pref == 0)
                    nextAuto[hero] = (b + 1) % branches;
                any = true;
                ++bought;
            }
            if (!any)
                break;
        }
        dirty = dirty || bought > 0;
        return bought;
    }
    // The bonus on a stat from the perks owned (0: none).
    float statBonus(uint32_t hero, Stat s) const {
        if (hero >= hero_modes::heroes)
            return 0.f;
        float b = 0;
        for (uint32_t br = 0; br < branches; ++br)
            for (uint32_t t = 0; t < tiers; ++t)
                if (owned[hero] & perkBit(br, t)) {
                    const Perk& p = perk(hero, br, t);
                    if (p.stat == s)
                        b += p.amount;
                }
        return b;
    }
    // A multiplier on that stat (1 + bonus; for cooldown and armor: 1 - bonus, never under .5).
    float scale(uint32_t hero, Stat s) const {
        const float b = statBonus(hero, s);
        if (s == Stat::cooldown || s == Stat::armor)
            return std::max(.5f, 1.f - b);
        return 1.f + b;
    }
    uint32_t count(uint32_t hero) const {
        uint32_t n = 0;
        for (uint32_t m = hero < hero_modes::heroes ? owned[hero] : 0u; m; m &= m - 1)
            ++n;
        return n;
    }
    // "PERKS 5/12  POINTS 2" (the hologram's line), the branch line "COMBAT: WEB FISTS > SPIDER-SENSE".
    void text(uint32_t hero, int32_t rep, bool all, char* out, size_t n) const {
        std::snprintf(out, n, "PERKS %u/%u  POINTS %u", count(hero), perksPerHero, points(hero, rep, all));
    }
    void save(char* out, size_t n) const {
        size_t at = 0;
        for (uint32_t h = 0; h < hero_modes::heroes && at < n; ++h) {
            const int w = std::snprintf(out + at, n - at, "perks.%u=%u\n", h, owned[h]);
            if (w < 0)
                break;
            at += static_cast<size_t>(w);
        }
    }
    void load(const char* text) {
        for (uint32_t h = 0; text && h < hero_modes::heroes; ++h) {
            char key[24];
            std::snprintf(key, sizeof key, "perks.%u=", h);
            if (const char* v = std::strstr(text, key))
                owned[h] |= static_cast<uint32_t>(std::strtoul(v + std::strlen(key), nullptr, 10)) & ((1u << perksPerHero) - 1u);
        }
    }
};

// ---- ULTIMATE ----
inline const Ultimate& ultimate(uint32_t hero) { return ultimateTable[std::min(hero, hero_modes::heroes - 1)]; }
inline bool ultimateUnlocked(int32_t rep, bool testUnlock) { return testUnlock || rep >= repMost; }
// The gesture: both fists crossed on the chest (each hand on the OTHER side of the body), both grips
// held, for ultimateHold seconds.
inline constexpr float ultimateHold = 1.2f, ultBelow = .2f, ultLow = .6f, ultOver = .04f, ultAhead = .35f;
inline bool fistsCrossed(holster::BodyPoint left, holster::BodyPoint right) {
    const auto okp = [](holster::BodyPoint p) {
        return std::isfinite(p.below) && std::isfinite(p.side) && std::isfinite(p.ahead) && p.below >= ultBelow &&
               p.below <= ultLow && p.ahead >= -.05f && p.ahead <= ultAhead;
    };
    return okp(left) && okp(right) && left.side >= ultOver && right.side <= -ultOver;
}
class UltimateMove {
  public:
    float held{}, cooldown{}, active{}, nextHit{};
    uint32_t hitsLeft{};
    uint64_t fired{};
    // Returns true the frame it fires (start its look; then `hit()` says when each blow lands).
    bool step(float dt, uint32_t hero, bool unlocked, bool on, bool crossed, float gripL, float gripR, float cooldownScale = 1.f) {
        dt = clampDt(dt);
        cooldown = std::max(0.f, cooldown - dt);
        active = std::max(0.f, active - dt);
        nextHit -= dt;
        const bool hold = on && unlocked && crossed && std::isfinite(gripL) && std::isfinite(gripR) && gripL >= .6f && gripR >= .6f;
        held = hold && cooldown <= 0 && active <= 0 ? held + dt : 0.f;
        if (held < ultimateHold)
            return false;
        const Ultimate& u = ultimate(hero);
        held = 0;
        active = u.seconds;
        hitsLeft = std::max(1u, u.hits);
        nextHit = 0;
        const float cs = std::isfinite(cooldownScale) ? std::clamp(cooldownScale, .5f, 1.f) : 1.f;
        cooldown = u.cooldown * cs;
        ++fired;
        return true;
    }
    // A blow due now (each of `hits`, spread over `seconds`).
    bool hit(uint32_t hero) {
        if (hitsLeft == 0 || nextHit > 0)
            return false;
        --hitsLeft;
        const Ultimate& u = ultimate(hero);
        nextHit = u.hits > 1 ? u.seconds / static_cast<float>(u.hits) : 1e9f;
        return true;
    }
};

// TEST UNLOCKS (the setting): 0 OFF, 1 SUITS, 2 ULTIMATES (and suits), 3 ALL (and perks).
inline bool testSuits(uint32_t v) { return v >= 1 && v <= 3; }
inline bool testUltimates(uint32_t v) { return v == 2 || v == 3; }
inline bool testPerks(uint32_t v) { return v == 3; }

} // namespace spidy::hero072
