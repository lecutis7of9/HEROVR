#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
// Made by Agent Mulder.
//
// Version 0.0.72 (TEST): the reputation journey's PERK TREES and ULTIMATES (the user, 8:46, 8:47,
// 9:12; SPIDER-MAN left out, 9:13, 9:14: he keeps the game's own skill tree, only the REP meter).
// Pure rules, no game calls. All the data is in the two tables below (perkTable, ultimateTable):
// change a perk or an ultimate there, nothing else.
// - PERKS: every reputation level (hero072::repLevel: 20 levels, ROOKIE to LEGEND, 4 steps each) is
//   one perk point for THAT hero (rep stays a 10-15 hour grind a hero); the 20 points fill the tree
//   nearly at LEGEND. The tree (9:35, the approved mockup 02_skill_tree_thor) is 3 columns POWER /
//   DEFENSE / MOVE x 3 tiers T1 -> T2 -> T3 (1, 2 and 3 points a perk: 18 of the 20 points; the 2
//   left over are kept), then the ULTIMATE. A tier opens once the tier before is full. POWER holds
//   the CHAIN 4 / 5 / 6 perks. IDEA-TBD perks (Stat::idea) are placeholders: bought, they do nothing.
//   The hero's CHAIN move (9:16) hits up to 3 enemies from the start; the three CHAIN perks raise it
//   to 4, 5, 6 (Play::carry). NOT for SUPERMAN, IRON MAN or the PUNISHER (9:26: heat vision goes
//   where he looks, repulsors where he points, bullets never chain): their three slots are HEAT
//   VISION / REPULSOR damage +1 +2 +3 and RELOAD SPEED +1 +2 +3. HERO PERKS (the setting): AUTO,
//   CHAIN FIRST, HOLD POINTS (tap +1 in the SKILL TREE), PERKS OFF; RESET POINTS gives them back. Fail-safe: a perk whose hook is not wired (wiredStat) has
//   no effect, never a crash.
// - ULTIMATE: the tree's capstone, only at the hero's MAX reputation (or TEST UNLOCKS). Fired by a
//   gesture (no trigger): both fists crossed on the chest with both grips held for ultimateHold
//   seconds; then a long cooldown. Its effect: `hits` blows (sm2_damage, CONFIRMED) on every
//   standing enemy within `radius` over `seconds`, plus its look.
// - Saved: "tree.<hero>=<mask>" lines (9 bits; the 6-perk "perks." lines of the earlier test build
//   are ignored). RESET POINTS saves the cleared mask.
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
    uint32_t hero, tier, slot; // tier 0-2 (T1-T3; the ULTIMATE is the 4th step), slot = column 0 POWER, 1 DEFENSE, 2 MOVE
    const char* name;
    const char* sub; // its one-sentence subtext (the hologram, docs)
    Stat stat;
    float amount; // +amount (0.1 = +10 %); chainJumps: whole extra enemies
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
// Every stat a perk can raise. wiredStat says which this build reads (the others are kept, no effect).
enum class Stat : uint32_t { armorRegen, batarangs, bleed, blockSpeed, bounceForce, bubbleStrength, chainJumps, chargeDrain, comboTime, cooldown, fistSize, flySpeed, freezeDamage, glideTime, grappleSpeed, hammerReturn, healRate, heatSeconds, hookSpeed, jumpHeight, knockback, lightningSeconds, magSize, missileLock, parryWindow, rageBuild, rageSeconds, repulsorCharge, slamRadius, sonarRange, stealth, stormRadius, heatDamage, repulsorDamage, reloadSpeed, idea, stats };
inline constexpr const char* statNames[static_cast<uint32_t>(Stat::stats)] = {"armorRegen", "batarangs", "bleed", "blockSpeed", "bounceForce", "bubbleStrength", "chainJumps", "chargeDrain", "comboTime", "cooldown", "fistSize", "flySpeed", "freezeDamage", "glideTime", "grappleSpeed", "hammerReturn", "healRate", "heatSeconds", "hookSpeed", "jumpHeight", "knockback", "lightningSeconds", "magSize", "missileLock", "parryWindow", "rageBuild", "rageSeconds", "repulsorCharge", "slamRadius", "sonarRange", "stealth", "stormRadius", "heatDamage", "repulsorDamage", "reloadSpeed", "idea"};
// The user's trees (9:12), Spider-Man left out (9:13, 9:14: the game has his own skill tree).
// Per hero, T1 then T2 then T3, each POWER (the chain 4/5/6 or the damage / reload +1/+2/+3), DEFENSE, MOVE.
// Stat::idea = IDEA-TBD (9:35: placeholder perks are OK for now; no effect).
// SUPERMAN, IRON MAN, the PUNISHER (9:26): no chain; damage / reload +1 +2 +3 in those three slots.
inline constexpr Perk perkTable[] = {
    // IRON MAN
    {1, 0, 0, "REPULSOR DAMAGE +1", "Repulsor blasts hit harder.", Stat::repulsorDamage, 0.15f, 1},
    {1, 0, 1, "ARMOR REGEN", "The armor slowly mends between fights.", Stat::armorRegen, 0.15f, 1},
    {1, 0, 2, "HEAVY REPULSORS", "Repulsor blasts knock foes back further.", Stat::knockback, 0.15f, 1}, // 0.0.72: was FAST REPULSORS (no charge left)
    {1, 1, 0, "REPULSOR DAMAGE +2", "Repulsor blasts hit harder again.", Stat::repulsorDamage, 0.15f, 2},
    {1, 1, 1, "FLARE COUNTER", "Flares shake off a locked missile.", Stat::idea, 0.f, 2},
    {1, 1, 2, "MISSILE LOCK", "Locks missiles on the enemy you look at.", Stat::missileLock, 0.2f, 2},
    {1, 2, 0, "REPULSOR DAMAGE +3", "Repulsor blasts hit hardest.", Stat::repulsorDamage, 0.15f, 3},
    {1, 2, 1, "HEAVY PLATING", "The armor shrugs off one big hit a fight.", Stat::idea, 0.f, 3},
    {1, 2, 2, "AFTERBURNER", "Flight bursts to a far higher speed.", Stat::idea, 0.f, 3},
    // DAREDEVIL
    {2, 0, 0, "BATON BOUNCE 4", "A thrown baton bounces through up to 4 enemies.", Stat::chainJumps, 1.f, 1},
    {2, 0, 1, "WIDE RADAR", "The radar sense reaches further.", Stat::sonarRange, 0.2f, 1},
    {2, 0, 2, "FAST GRAPPLE", "The baton grapple pulls you faster.", Stat::grappleSpeed, 0.15f, 1},
    {2, 1, 0, "BATON BOUNCE 5", "A thrown baton bounces through up to 5 enemies.", Stat::chainJumps, 1.f, 2},
    {2, 1, 1, "LONG PARRY", "The parry window is longer.", Stat::parryWindow, 0.3f, 2},
    {2, 1, 2, "WALL FLIP", "A jump off a wall flips you over an enemy.", Stat::idea, 0.f, 2},
    {2, 2, 0, "BATON BOUNCE 6", "A thrown baton bounces through up to 6 enemies.", Stat::chainJumps, 1.f, 3},
    {2, 2, 1, "SIXTH SENSE", "The radar flashes just before a hit lands.", Stat::idea, 0.f, 3},
    {2, 2, 2, "ROOFTOP RUN", "Landing from a fall rolls you on at speed.", Stat::idea, 0.f, 3},
    // HULK
    {3, 0, 0, "SHOCKWAVE 4", "A shockwave carries through up to 4 enemies.", Stat::chainJumps, 1.f, 1},
    {3, 0, 1, "FAST RAGE", "Rage builds faster.", Stat::rageBuild, 0.25f, 1},
    {3, 0, 2, "BIG JUMPS", "Hulk jumps higher.", Stat::jumpHeight, 0.15f, 1},
    {3, 1, 0, "SHOCKWAVE 5", "A shockwave carries through up to 5 enemies.", Stat::chainJumps, 1.f, 2},
    {3, 1, 1, "THICK HIDE", "Rage takes the edge off every hit.", Stat::idea, 0.f, 2},
    {3, 1, 2, "WIDE POUND", "The ground pound reaches wider.", Stat::slamRadius, 0.25f, 2},
    {3, 2, 0, "SHOCKWAVE 6", "A shockwave carries through up to 6 enemies.", Stat::chainJumps, 1.f, 3},
    {3, 2, 1, "UNSTOPPABLE", "Raging, nothing knocks Hulk down.", Stat::idea, 0.f, 3},
    {3, 2, 2, "THUNDER CLAP", "A clap of both hands knocks enemies back.", Stat::idea, 0.f, 3},
    // BATMAN
    {4, 0, 0, "RICOCHET 4", "A batarang ricochets through up to 4 enemies.", Stat::chainJumps, 1.f, 1},
    {4, 0, 1, "QUIET TAKEDOWNS", "Takedowns make less noise.", Stat::stealth, 0.25f, 1},
    {4, 0, 2, "LONG GLIDE", "The cape glides for longer.", Stat::glideTime, 0.2f, 1},
    {4, 1, 0, "RICOCHET 5", "A batarang ricochets through up to 5 enemies.", Stat::chainJumps, 1.f, 2},
    {4, 1, 1, "ARMORED CAPE", "The cape turns aside one bullet a fight.", Stat::idea, 0.f, 2},
    {4, 1, 2, "MORE BATARANGS", "One more batarang in each throw.", Stat::batarangs, 1.f, 2},
    {4, 2, 0, "RICOCHET 6", "A batarang ricochets through up to 6 enemies.", Stat::chainJumps, 1.f, 3},
    {4, 2, 1, "SILENT PERCH", "A perch takedown never alerts a second enemy.", Stat::idea, 0.f, 3},
    {4, 2, 2, "DIVE BOMB", "A swoop from high knocks the target down.", Stat::idea, 0.f, 3},
    // WOLVERINE
    {5, 0, 0, "LUNGE 4", "A lunge carries through up to 4 enemies.", Stat::chainJumps, 1.f, 1},
    {5, 0, 1, "FAST HEALING", "The healing factor works faster.", Stat::healRate, 0.25f, 1},
    {5, 0, 2, "LONG COMBOS", "Combos stay alive a little longer.", Stat::comboTime, 0.25f, 1},
    {5, 1, 0, "LUNGE 5", "A lunge carries through up to 5 enemies.", Stat::chainJumps, 1.f, 2},
    {5, 1, 1, "ADAMANTIUM SKULL", "Head hits barely stagger Logan.", Stat::idea, 0.f, 2},
    {5, 1, 2, "CLAW BLEED", "Claw hits make enemies bleed.", Stat::bleed, 0.2f, 2},
    {5, 2, 0, "LUNGE 6", "A lunge carries through up to 6 enemies.", Stat::chainJumps, 1.f, 3},
    {5, 2, 1, "LAST STAND", "Near death, the healing factor surges.", Stat::idea, 0.f, 3},
    {5, 2, 2, "POUNCE", "A crouched jump leaps onto an enemy.", Stat::idea, 0.f, 3},
    // SUPERMAN
    {6, 0, 0, "HEAT VISION +1", "Heat vision burns hotter where you look.", Stat::heatDamage, 0.15f, 1},
    {6, 0, 1, "STRONG FREEZE", "The freeze breath is stronger.", Stat::freezeDamage, 0.2f, 1},
    {6, 0, 2, "FAST FLIGHT", "Superman flies faster.", Stat::flySpeed, 0.15f, 1},
    {6, 1, 0, "HEAT VISION +2", "Heat vision burns hotter again.", Stat::heatDamage, 0.15f, 2},
    {6, 1, 1, "MAN OF STEEL", "Heavy blows knock Superman back less.", Stat::idea, 0.f, 2},
    {6, 1, 2, "LONG HEAT VISION", "Heat vision lasts longer.", Stat::heatSeconds, 0.25f, 2},
    {6, 2, 0, "HEAT VISION +3", "Heat vision burns hottest.", Stat::heatDamage, 0.15f, 3},
    {6, 2, 1, "SOLAR FLARE", "A burst of stored sunlight stuns those near.", Stat::idea, 0.f, 3},
    {6, 2, 2, "SUPER DASH", "A punch from flight carries on through.", Stat::idea, 0.f, 3},
    // PUNISHER
    {7, 0, 0, "RELOAD SPEED +1", "The guns reload faster.", Stat::reloadSpeed, 0.15f, 1},
    {7, 0, 1, "BIG MAG", "Bigger magazines for the guns.", Stat::magSize, 0.25f, 1},
    {7, 0, 2, "FAST REEL", "The grappling hook reels you in faster.", Stat::hookSpeed, 0.2f, 1},
    {7, 1, 0, "RELOAD SPEED +2", "The guns reload faster again.", Stat::reloadSpeed, 0.15f, 2},
    {7, 1, 1, "LONG RAGE", "Rage lasts longer.", Stat::rageSeconds, 0.3f, 2},
    {7, 1, 2, "COMBAT ROLL", "A quick duck dodges a shot.", Stat::idea, 0.f, 2},
    {7, 2, 0, "RELOAD SPEED +3", "The guns reload fastest.", Stat::reloadSpeed, 0.15f, 3},
    {7, 2, 1, "KEVLAR", "The vest takes the first hit of a fight.", Stat::idea, 0.f, 3},
    {7, 2, 2, "DEATH FROM ABOVE", "Shots from the hook line hit harder.", Stat::idea, 0.f, 3},
    // CAPTAIN AMERICA
    {8, 0, 0, "SHIELD BOUNCE 4", "The shield bounces through up to 4 enemies.", Stat::chainJumps, 1.f, 1},
    {8, 0, 1, "FAST BLOCK", "The shield comes up faster.", Stat::blockSpeed, 0.2f, 1},
    {8, 0, 2, "BIG SLAM", "The shield slam knocks enemies further.", Stat::knockback, 0.25f, 1},
    {8, 1, 0, "SHIELD BOUNCE 5", "The shield bounces through up to 5 enemies.", Stat::chainJumps, 1.f, 2},
    {8, 1, 1, "HARD BOUNCE", "The shield bounces harder.", Stat::bounceForce, 0.2f, 2},
    {8, 1, 2, "SHIELD SURF", "Riding the shield downhill keeps your speed.", Stat::idea, 0.f, 2},
    {8, 2, 0, "SHIELD BOUNCE 6", "The shield bounces through up to 6 enemies.", Stat::chainJumps, 1.f, 3},
    {8, 2, 1, "SUPER SOLDIER", "Cap shrugs off a hit while blocking.", Stat::idea, 0.f, 3},
    {8, 2, 2, "SHIELD VAULT", "A jump off the raised shield goes higher.", Stat::idea, 0.f, 3},
    // THOR
    {9, 0, 0, "CHAIN LIGHTNING I", "Lightning chains to 4 enemies (was 3).", Stat::chainJumps, 1.f, 1},
    {9, 0, 1, "BIGGER STORM", "Storm radius +25%: more enemies knocked back.", Stat::stormRadius, 0.25f, 1},
    {9, 0, 2, "FASTER HAMMER RETURN", "The hammer flies back to your hand 40% faster.", Stat::hammerReturn, 0.4f, 1},
    {9, 1, 0, "CHAIN LIGHTNING II", "Lightning chains to 5 enemies.", Stat::chainJumps, 1.f, 2},
    {9, 1, 1, "LONGER LIGHTNING", "Lightning strikes last 50% longer.", Stat::lightningSeconds, 0.5f, 2},
    {9, 1, 2, "STORM DASH", "A thunder burst on take-off knocks enemies away.", Stat::idea, 0.f, 2},
    {9, 2, 0, "CHAIN LIGHTNING III", "Lightning chains to 6 enemies.", Stat::chainJumps, 1.f, 3},
    {9, 2, 1, "STORM SHIELD", "Raised hammer blocks a hit while the storm is up.", Stat::idea, 0.f, 3},
    {9, 2, 2, "WORTHY LANDING", "Landing from flight slams the ground with lightning.", Stat::idea, 0.f, 3},
    // GREEN LANTERN
    {10, 0, 0, "CHAIN GRAB 4", "The construct chain grabs up to 4 enemies.", Stat::chainJumps, 1.f, 1},
    {10, 0, 1, "STRONG SHIELD", "The bubble shield holds longer.", Stat::bubbleStrength, 0.25f, 1},
    {10, 0, 2, "SLOW DRAIN", "The ring's charge drains slower.", Stat::chargeDrain, 0.2f, 1},
    {10, 1, 0, "CHAIN GRAB 5", "The construct chain grabs up to 5 enemies.", Stat::chainJumps, 1.f, 2},
    {10, 1, 1, "WILLPOWER", "A full ring blocks one hit.", Stat::idea, 0.f, 2},
    {10, 1, 2, "BIG FIST", "The construct fist is bigger.", Stat::fistSize, 0.25f, 2},
    {10, 2, 0, "CHAIN GRAB 6", "The construct chain grabs up to 6 enemies.", Stat::chainJumps, 1.f, 3},
    {10, 2, 1, "LANTERN OATH", "A drained ring refills a little faster.", Stat::idea, 0.f, 3},
    {10, 2, 2, "WARP SPEED", "Flight bursts to a far higher speed.", Stat::idea, 0.f, 3},

};
// The ULTIMATE (the tree's 4th step, max reputation only). SPIDER-MAN has none (9:14): no name.
inline constexpr Ultimate ultimateTable[hero_modes::heroes] = {
    {"", 0.f, 0, {0.f, 5, 0.f}, 0.f, 1e9f, ""}, // SPIDER-MAN: vanilla (the game's own skill tree)
    {"UNIBEAM", 30.f, 8, {30.f, 5, 420.f}, 2.5f, 180.f, "the chest's full-power beam"}, // IRON MAN
    {"RADAR SLOW-MO", 25.f, 1, {20.f, 5, 260.f}, 4.0f, 200.f, "enemies outlined, a stun (slow-mo CANDIDATE)"}, // DAREDEVIL
    {"GROUND SMASH", 12.f, 1, {90.f, 5, 900.f}, 1.0f, 180.f, "a giant ground smash shockwave"}, // HULK
    {"FEAR TAKEDOWN", 8.f, 1, {80.f, 5, 500.f}, 1.5f, 200.f, "drops the group near"}, // BATMAN
    {"BERSERKER RAGE", 3.f, 20, {12.f, 5, 200.f}, 6.0f, 200.f, "nonstop claw slashes"}, // WOLVERINE
    {"HEAT VISION SWEEP", 20.f, 10, {30.f, 5, 300.f}, 3.0f, 180.f, "heat vision swept across them"}, // SUPERMAN
    {"ROCKET BARRAGE", 25.f, 6, {60.f, 5, 600.f}, 3.0f, 200.f, "a rocket launcher barrage"}, // PUNISHER
    {"SHIELD CHAIN", 25.f, 1, {45.f, 5, 420.f}, 3.0f, 180.f, "the shield bounces through every enemy"}, // CAPTAIN AMERICA
    {"BIFROST STRIKE", 12.f, 1, {120.f, 5, 800.f}, 1.5f, 220.f, "the Bifrost strikes the area"}, // THOR
    {"GIANT CONSTRUCT", 10.f, 1, {85.f, 5, 700.f}, 1.5f, 200.f, "a giant construct crushes the crowd"}, // GREEN LANTERN
};

inline constexpr uint32_t perkCount = sizeof perkTable / sizeof perkTable[0];
inline constexpr uint32_t tiers = 3, columns = 3, perksPerHero = tiers * columns, treeHeroes = hero_modes::heroes - 1;
inline constexpr uint32_t tierOf[perksPerHero] = {0, 0, 0, 1, 1, 1, 2, 2, 2};
inline constexpr uint32_t columnOf[perksPerHero] = {0, 1, 2, 0, 1, 2, 0, 1, 2};
inline constexpr const char* columnNames[columns] = {"POWER", "DEFENSE", "MOVE"};
inline constexpr uint32_t chainSlot[tiers] = {0, 3, 6}; // POWER: the CHAIN perk of each tier (or the damage / reload one)
static_assert(perkCount == treeHeroes * perksPerHero, "9 perks a hero, Spider-Man none");
inline constexpr bool perkTableInOrder() {
    for (uint32_t k = 0; k < perkCount; ++k)
        if (perkTable[k].hero != k / perksPerHero + 1 || perkTable[k].tier != tierOf[k % perksPerHero] ||
            perkTable[k].slot != columnOf[k % perksPerHero] || perkTable[k].cost != tierOf[k % perksPerHero] + 1)
            return false;
    return true;
}
static_assert(perkTableInOrder(), "the perk table: hero by hero, T1 to T3, POWER DEFENSE MOVE, 1/2/3 points");
// The CHAIN move hits up to chainBase enemies from the start (9:16); each CHAIN perk one more (6).
inline constexpr uint32_t chainBase = 3, chainMost = 6;
// SPIDER-MAN keeps the game's own skill tree: no perks, no ultimate (9:13, 9:14).
inline bool hasTree(uint32_t hero) { return hero >= 1 && hero < hero_modes::heroes; }
// The CHAIN move (9:16, 9:26): BATMAN, CAP, DAREDEVIL, HULK, WOLVERINE, THOR, GREEN LANTERN. Not
// SUPERMAN (heat vision where he looks), IRON MAN (repulsors where he points), the PUNISHER (no
// penetration), nor SPIDER-MAN (vanilla).
inline bool hasChain(uint32_t hero) {
    return hasTree(hero) && hero != hero_modes::superman && hero != hero_modes::ironMan && hero != hero_modes::punisher;
}
// The hero's perk i (0-8: tier * 3 + column); Spider-Man (none) gets Iron Man's
// row, which is never owned (callers check hasTree).
inline const Perk& perk(uint32_t hero, uint32_t i) {
    const uint32_t h = hasTree(hero) ? hero : 1u;
    return perkTable[(h - 1) * perksPerHero + std::min(i, perksPerHero - 1)];
}
inline uint32_t perkBit(uint32_t i) { return 1u << i; }
inline constexpr uint32_t perkMask = (1u << perksPerHero) - 1u;
inline constexpr uint32_t perkCost(uint32_t i) { return tierOf[i] + 1; } // T1 1, T2 2, T3 3: 18 for a tree
inline constexpr size_t treeLine = 160; // a PERK TREE line (T1 with 3 names)
inline bool isIdea(const Perk& p) { return p.stat == Stat::idea; }
inline constexpr uint32_t treeCost() {
    uint32_t c = 0;
    for (uint32_t i = 0; i < perksPerHero; ++i)
        c += perkCost(i);
    return c;
}
static_assert(treeCost() <= static_cast<uint32_t>(repLevels) && treeCost() + 3 > static_cast<uint32_t>(repLevels),
              "the 20 levels fill the tree by LEGEND (9:26, 9:35)");
// Which stats this build reads (a hook in hero072_play / game_xr); the others have no effect yet.
inline bool wiredStat(Stat s) {
    switch (s) {
    case Stat::chainJumps:
    case Stat::fistSize:
    case Stat::stormRadius:
    case Stat::lightningSeconds:
    case Stat::knockback:
    case Stat::slamRadius:
    case Stat::chargeDrain:
    case Stat::bubbleStrength:
    case Stat::parryWindow:
    case Stat::rageBuild:
    case Stat::rageSeconds:
        return true;
    default:
        return false;
    }
}

// HERO PERKS (the setting): 0 AUTO (the tree's order: DEFENSE, MOVE, then POWER in each tier),
// 1 CHAIN FIRST (POWER first in each tier), 2 HOLD POINTS (bought by hand: +1 in the SKILL TREE),
// 3 PERKS OFF (bought as AUTO, no effect: to compare). Every mode can buy by hand too (buy).
inline constexpr uint32_t perkAuto = 0, perkChainFirst = 1, perkHold = 2, perkOff = 3;
class Perks {
  public:
    uint32_t owned[hero_modes::heroes]{};
    bool off{}; // PERKS OFF: every scale 1, no chain
    bool dirty{};
    static uint32_t spentOf(uint32_t mask) {
        uint32_t s = 0;
        for (uint32_t i = 0; i < perksPerHero; ++i)
            if (mask & perkBit(i))
                s += perkCost(i);
        return s;
    }
    uint32_t points(uint32_t hero, int32_t rep, bool all) const {
        if (!hasTree(hero))
            return 0;
        const uint32_t have = all ? treeCost() : static_cast<uint32_t>(repLevel(rep));
        const uint32_t spent = spentOf(owned[hero]);
        return have > spent ? have - spent : 0u;
    }
    // A perk can be bought once every perk of the tier before it is owned.
    bool open(uint32_t hero, uint32_t i) const {
        if (!hasTree(hero) || i >= perksPerHero || (owned[hero] & perkBit(i)))
            return false;
        for (uint32_t j = 0; j < perksPerHero; ++j)
            if (tierOf[j] < tierOf[i] && !(owned[hero] & perkBit(j)))
                return false;
        return true;
    }
    // Spend what there is (see HERO PERKS). Returns the perks bought.
    uint32_t spend(uint32_t hero, int32_t rep, uint32_t pref, bool all) {
        off = pref == perkOff;
        if (!hasTree(hero) || (pref == perkHold && !all))
            return 0;
        static constexpr uint32_t autoOrder[perksPerHero] = {1, 2, 0, 4, 5, 3, 7, 8, 6}, chainOrder[perksPerHero] = {0, 1, 2, 3, 4, 5, 6, 7, 8};
        const uint32_t* base = pref == perkChainFirst ? chainOrder : autoOrder;
        // In each tier the IDEA-TBD placeholders go last (they do nothing yet).
        uint32_t order[perksPerHero], n = 0;
        for (uint32_t t = 0; t < tiers; ++t)
            for (uint32_t pass = 0; pass < 2; ++pass)
                for (uint32_t k = t * columns; k < (t + 1) * columns; ++k)
                    if (isIdea(perk(hero, base[k])) == (pass == 1))
                        order[n++] = base[k];
        uint32_t bought = 0;
        for (uint32_t k = 0; k < n; ++k) {
            const uint32_t i = order[k];
            if (!open(hero, i))
                continue;
            if (perkCost(i) > points(hero, rep, all))
                break; // in order: never skip ahead to a cheaper perk
            owned[hero] |= perkBit(i);
            ++bought;
        }
        dirty = dirty || bought > 0;
        return bought;
    }
    // +1 in the SKILL TREE: buy perk i by hand (open, and the points are there).
    bool buy(uint32_t hero, uint32_t i, int32_t rep, bool all) {
        if (!open(hero, i) || perkCost(i) > points(hero, rep, all))
            return false;
        owned[hero] |= perkBit(i);
        dirty = true;
        return true;
    }
    // RESET POINTS: every perk of the hero back to points (saved).
    uint32_t reset(uint32_t hero) {
        if (!hasTree(hero))
            return 0;
        const uint32_t n = count(hero);
        owned[hero] = 0;
        dirty = dirty || n > 0;
        return n;
    }
    uint32_t spent(uint32_t hero) const { return hasTree(hero) ? spentOf(owned[hero]) : 0u; }
    float statBonus(uint32_t hero, Stat s) const {
        if (off || !hasTree(hero))
            return 0.f;
        float b = 0;
        for (uint32_t i = 0; i < perksPerHero; ++i)
            if (owned[hero] & perkBit(i)) {
                const Perk& p = perk(hero, i);
                if (p.stat == s && !isIdea(p) && std::isfinite(p.amount))
                    b += p.amount;
            }
        return std::clamp(b, 0.f, 3.f);
    }
    // A multiplier on that stat (1 + bonus; cooldown: 1 - bonus, never under .5).
    float scale(uint32_t hero, Stat s) const {
        const float b = statBonus(hero, s);
        if (s == Stat::cooldown)
            return std::max(.5f, 1.f - b);
        return 1.f + b;
    }
    // The CHAIN perks: how many enemies the hero's chain move hits in all (3 from the start, up to 6;
    // 1 for SPIDER-MAN, SUPERMAN, IRON MAN and the PUNISHER: no chain).
    uint32_t chainCap(uint32_t hero) const {
        if (!hasChain(hero))
            return 1;
        return chainBase + static_cast<uint32_t>(std::min(float(chainMost - chainBase), statBonus(hero, Stat::chainJumps)) + .5f);
    }
    uint32_t count(uint32_t hero) const {
        uint32_t n = 0;
        for (uint32_t m = hasTree(hero) ? owned[hero] & perkMask : 0u; m; m &= m - 1)
            ++n;
        return n;
    }
    bool tierDone(uint32_t hero, uint32_t tier) const {
        for (uint32_t i = 0; i < perksPerHero; ++i)
            if (tierOf[i] == tier && !(owned[hero] & perkBit(i)))
                return false;
        return hasTree(hero);
    }
    // "PERKS 3/5  POINTS 2" (the hologram's line); Spider-Man: "" (vanilla).
    void text(uint32_t hero, int32_t rep, bool all, char* out, size_t n) const {
        if (!out || n == 0)
            return;
        if (!hasTree(hero)) {
            out[0] = 0;
            return;
        }
        std::snprintf(out, n, "PERKS %u/%u  POINTS %u%s", count(hero), perksPerHero, points(hero, rep, all), off ? "  OFF" : "");
    }
    // The tree, one line a step (the settings' PERK TREE rows): "T1 [x] CHAIN LIGHTNING I [ ] BIGGER STORM ...".
    // Returns the lines written (4: T1, T2, T3, ULT; 0 for Spider-Man).
    uint32_t tree(uint32_t hero, bool ultimateOpen, char lines[4][treeLine]) const {
        if (!hasTree(hero))
            return 0;
        for (uint32_t t = 0; t < tiers; ++t) {
            size_t at = 0;
            at += static_cast<size_t>(std::max(0, std::snprintf(lines[t], treeLine, "T%u", t + 1)));
            for (uint32_t i = 0; i < perksPerHero && at < treeLine; ++i)
                if (tierOf[i] == t)
                    at += static_cast<size_t>(std::max(0, std::snprintf(lines[t] + at, treeLine - at, " [%c] %s", owned[hero] & perkBit(i) ? 'x' : ' ', perk(hero, i).name)));
        }
        std::snprintf(lines[3], treeLine, "ULT [%c] %s", ultimateOpen ? 'x' : ' ', ultimateTable[hero].name);
        return 4;
    }
    void save(char* out, size_t n) const {
        size_t at = 0;
        for (uint32_t h = 1; h < hero_modes::heroes && at < n; ++h) {
            const int w = std::snprintf(out + at, n - at, "tree.%u=%u\n", h, owned[h] & perkMask);
            if (w < 0)
                break;
            at += static_cast<size_t>(w);
        }
    }
    void load(const char* text) {
        for (uint32_t h = 1; text && h < hero_modes::heroes; ++h) {
            char key[24];
            std::snprintf(key, sizeof key, "\ntree.%u=", h);
            const char* v = std::strstr(text, key);
            const size_t skip = v ? 1 : 0;
            if (!v && std::strncmp(text, key + 1, std::strlen(key + 1)) == 0)
                v = text;
            if (v)
                owned[h] |= static_cast<uint32_t>(std::strtoul(v + skip + std::strlen(key + 1), nullptr, 10)) & perkMask;
        }
    }
};

// ---- ULTIMATE ----
inline const Ultimate& ultimate(uint32_t hero) { return ultimateTable[std::min(hero, hero_modes::heroes - 1)]; }
inline bool ultimateUnlocked(uint32_t hero, int32_t rep, bool testUnlock) {
    return hasTree(hero) && (testUnlock || rep >= repMost);
}
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
        const bool hold = on && unlocked && hasTree(hero) && crossed && std::isfinite(gripL) && std::isfinite(gripR) && gripL >= .6f && gripR >= .6f;
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
