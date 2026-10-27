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
//   exactly at LEGEND. The tree is T1 (two perks and CHAIN 4, 2 points each) -> T2 (a perk and
//   CHAIN 5, 4 each) -> T3 (CHAIN 6, 6) -> the ULTIMATE. A tier opens once the tier before is full.
//   The hero's CHAIN move (9:16) hits up to 3 enemies from the start; the three CHAIN perks raise it
//   to 4, 5, 6 (Play::carry). NOT for SUPERMAN, IRON MAN or the PUNISHER (9:26: heat vision goes
//   where he looks, repulsors where he points, bullets never chain): their three slots are HEAT
//   VISION / REPULSOR damage +1 +2 +3 and RELOAD SPEED +1 +2 +3. HERO PERKS (the setting): AUTO,
//   CHAIN FIRST, HOLD POINTS, PERKS OFF. Fail-safe: a perk whose hook is not wired (wiredStat) has
//   no effect, never a crash.
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
    uint32_t hero, tier, slot; // tier 0-2 (T1-T3; the ULTIMATE is the 4th step), slot 0-1 in the tier
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
enum class Stat : uint32_t { armorRegen, batarangs, bleed, blockSpeed, bounceForce, bubbleStrength, chainJumps, chargeDrain, comboTime, cooldown, fistSize, flySpeed, freezeDamage, glideTime, grappleSpeed, hammerReturn, healRate, heatSeconds, hookSpeed, jumpHeight, knockback, lightningSeconds, magSize, missileLock, parryWindow, rageBuild, rageSeconds, repulsorCharge, slamRadius, sonarRange, stealth, stormRadius, heatDamage, repulsorDamage, reloadSpeed, stats };
inline constexpr const char* statNames[static_cast<uint32_t>(Stat::stats)] = {"armorRegen", "batarangs", "bleed", "blockSpeed", "bounceForce", "bubbleStrength", "chainJumps", "chargeDrain", "comboTime", "cooldown", "fistSize", "flySpeed", "freezeDamage", "glideTime", "grappleSpeed", "hammerReturn", "healRate", "heatSeconds", "hookSpeed", "jumpHeight", "knockback", "lightningSeconds", "magSize", "missileLock", "parryWindow", "rageBuild", "rageSeconds", "repulsorCharge", "slamRadius", "sonarRange", "stealth", "stormRadius", "heatDamage", "repulsorDamage", "reloadSpeed"};
// The user's trees (9:12), Spider-Man left out (9:13, 9:14: the game has his own skill tree).
// T1 two perks and the chain 4, T2 a perk and the chain 5, T3 the chain 6, then the ULTIMATE (LEGEND).
// SUPERMAN, IRON MAN, the PUNISHER (9:26): no chain; damage / reload +1 +2 +3 in those three slots.
inline constexpr Perk perkTable[] = {
    // IRON MAN
    {1, 0, 0, "FAST REPULSORS", "Repulsors charge faster.", Stat::repulsorCharge, .15f, 2},
    {1, 0, 1, "ARMOR REGEN", "The armor slowly mends between fights.", Stat::armorRegen, .15f, 2},
    {1, 0, 2, "REPULSOR DAMAGE +1", "Repulsor blasts hit harder.", Stat::repulsorDamage, .15f, 2},
    {1, 1, 0, "MISSILE LOCK", "Locks missiles on the enemy you look at.", Stat::missileLock, .2f, 4},
    {1, 1, 1, "REPULSOR DAMAGE +2", "Repulsor blasts hit harder again.", Stat::repulsorDamage, .15f, 4},
    {1, 2, 0, "REPULSOR DAMAGE +3", "Repulsor blasts hit hardest.", Stat::repulsorDamage, .15f, 6},
    // DAREDEVIL
    {2, 0, 0, "WIDE RADAR", "The radar sense reaches further.", Stat::sonarRange, .2f, 2},
    {2, 0, 1, "FAST GRAPPLE", "The baton grapple pulls you faster.", Stat::grappleSpeed, .15f, 2},
    {2, 0, 2, "BATON BOUNCE 4", "A thrown baton bounces through up to 4 enemies.", Stat::chainJumps, 1, 2},
    {2, 1, 0, "LONG PARRY", "The parry window is longer.", Stat::parryWindow, .3f, 4},
    {2, 1, 1, "BATON BOUNCE 5", "A thrown baton bounces through up to 5 enemies.", Stat::chainJumps, 1, 4},
    {2, 2, 0, "BATON BOUNCE 6", "A thrown baton bounces through up to 6 enemies.", Stat::chainJumps, 1, 6},
    // HULK
    {3, 0, 0, "BIG JUMPS", "Hulk jumps higher.", Stat::jumpHeight, .15f, 2},
    {3, 0, 1, "FAST RAGE", "Rage builds faster.", Stat::rageBuild, .25f, 2},
    {3, 0, 2, "SHOCKWAVE 4", "A shockwave carries through up to 4 enemies.", Stat::chainJumps, 1, 2},
    {3, 1, 0, "WIDE POUND", "The ground pound reaches wider.", Stat::slamRadius, .25f, 4},
    {3, 1, 1, "SHOCKWAVE 5", "A shockwave carries through up to 5 enemies.", Stat::chainJumps, 1, 4},
    {3, 2, 0, "SHOCKWAVE 6", "A shockwave carries through up to 6 enemies.", Stat::chainJumps, 1, 6},
    // BATMAN
    {4, 0, 0, "QUIET TAKEDOWNS", "Takedowns make less noise.", Stat::stealth, .25f, 2},
    {4, 0, 1, "LONG GLIDE", "The cape glides for longer.", Stat::glideTime, .2f, 2},
    {4, 0, 2, "RICOCHET 4", "A batarang ricochets through up to 4 enemies.", Stat::chainJumps, 1, 2},
    {4, 1, 0, "MORE BATARANGS", "One more batarang in each throw.", Stat::batarangs, 1, 4},
    {4, 1, 1, "RICOCHET 5", "A batarang ricochets through up to 5 enemies.", Stat::chainJumps, 1, 4},
    {4, 2, 0, "RICOCHET 6", "A batarang ricochets through up to 6 enemies.", Stat::chainJumps, 1, 6},
    // WOLVERINE
    {5, 0, 0, "FAST HEALING", "The healing factor works faster.", Stat::healRate, .25f, 2},
    {5, 0, 1, "LONG COMBOS", "Combos stay alive a little longer.", Stat::comboTime, .25f, 2},
    {5, 0, 2, "LUNGE 4", "A lunge carries through up to 4 enemies.", Stat::chainJumps, 1, 2},
    {5, 1, 0, "CLAW BLEED", "Claw hits make enemies bleed.", Stat::bleed, .2f, 4},
    {5, 1, 1, "LUNGE 5", "A lunge carries through up to 5 enemies.", Stat::chainJumps, 1, 4},
    {5, 2, 0, "LUNGE 6", "A lunge carries through up to 6 enemies.", Stat::chainJumps, 1, 6},
    // SUPERMAN
    {6, 0, 0, "FAST FLIGHT", "Superman flies faster.", Stat::flySpeed, .15f, 2},
    {6, 0, 1, "STRONG FREEZE", "The freeze breath is stronger.", Stat::freezeDamage, .2f, 2},
    {6, 0, 2, "HEAT VISION +1", "Heat vision burns hotter where you look.", Stat::heatDamage, .15f, 2},
    {6, 1, 0, "LONG HEAT VISION", "Heat vision lasts longer.", Stat::heatSeconds, .25f, 4},
    {6, 1, 1, "HEAT VISION +2", "Heat vision burns hotter again.", Stat::heatDamage, .15f, 4},
    {6, 2, 0, "HEAT VISION +3", "Heat vision burns hottest.", Stat::heatDamage, .15f, 6},
    // PUNISHER
    {7, 0, 0, "BIG MAG", "Bigger magazines for the guns.", Stat::magSize, .25f, 2},
    {7, 0, 1, "FAST REEL", "The grappling hook reels you in faster.", Stat::hookSpeed, .2f, 2},
    {7, 0, 2, "RELOAD SPEED +1", "The guns reload faster.", Stat::reloadSpeed, .15f, 2},
    {7, 1, 0, "LONG RAGE", "Rage lasts longer.", Stat::rageSeconds, .3f, 4},
    {7, 1, 1, "RELOAD SPEED +2", "The guns reload faster again.", Stat::reloadSpeed, .15f, 4},
    {7, 2, 0, "RELOAD SPEED +3", "The guns reload fastest.", Stat::reloadSpeed, .15f, 6},
    // CAPTAIN AMERICA
    {8, 0, 0, "FAST BLOCK", "The shield comes up faster.", Stat::blockSpeed, .2f, 2},
    {8, 0, 1, "BIG SLAM", "The shield slam knocks enemies further.", Stat::knockback, .25f, 2},
    {8, 0, 2, "SHIELD BOUNCE 4", "The shield bounces through up to 4 enemies.", Stat::chainJumps, 1, 2},
    {8, 1, 0, "HARD BOUNCE", "The shield bounces harder.", Stat::bounceForce, .2f, 4},
    {8, 1, 1, "SHIELD BOUNCE 5", "The shield bounces through up to 5 enemies.", Stat::chainJumps, 1, 4},
    {8, 2, 0, "SHIELD BOUNCE 6", "The shield bounces through up to 6 enemies.", Stat::chainJumps, 1, 6},
    // THOR
    {9, 0, 0, "FAST RETURN", "The hammer flies back faster.", Stat::hammerReturn, .2f, 2},
    {9, 0, 1, "BIG STORM", "The storm reaches wider.", Stat::stormRadius, .25f, 2},
    {9, 0, 2, "LIGHTNING CHAIN 4", "Lightning and the hammer throw chain up to 4 enemies.", Stat::chainJumps, 1, 2},
    {9, 1, 0, "LONG LIGHTNING", "The storm strikes for longer.", Stat::lightningSeconds, .3f, 4},
    {9, 1, 1, "LIGHTNING CHAIN 5", "Lightning and the hammer throw chain up to 5 enemies.", Stat::chainJumps, 1, 4},
    {9, 2, 0, "LIGHTNING CHAIN 6", "Lightning and the hammer throw chain up to 6 enemies.", Stat::chainJumps, 1, 6},
    // GREEN LANTERN
    {10, 0, 0, "SLOW DRAIN", "The ring's charge drains slower.", Stat::chargeDrain, .2f, 2},
    {10, 0, 1, "STRONG SHIELD", "The bubble shield holds longer.", Stat::bubbleStrength, .25f, 2},
    {10, 0, 2, "CHAIN GRAB 4", "The construct chain grabs up to 4 enemies.", Stat::chainJumps, 1, 2},
    {10, 1, 0, "BIG FIST", "The construct fist is bigger.", Stat::fistSize, .25f, 4},
    {10, 1, 1, "CHAIN GRAB 5", "The construct chain grabs up to 5 enemies.", Stat::chainJumps, 1, 4},
    {10, 2, 0, "CHAIN GRAB 6", "The construct chain grabs up to 6 enemies.", Stat::chainJumps, 1, 6},
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
inline constexpr uint32_t tiers = 3, perksPerHero = 6, treeHeroes = hero_modes::heroes - 1;
inline constexpr uint32_t tierOf[perksPerHero] = {0, 0, 0, 1, 1, 2};
inline constexpr uint32_t chainSlot[tiers] = {2, 4, 5}; // the CHAIN perk of each tier (or the damage / reload one)
static_assert(perkCount == treeHeroes * perksPerHero, "6 perks a hero, Spider-Man none");
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
// The hero's perk i (0-5: T1 a, T1 b, T1 chain 4, T2 a, T2 chain 5, T3 chain 6); Spider-Man (none) gets Iron Man's
// row, which is never owned (callers check hasTree).
inline const Perk& perk(uint32_t hero, uint32_t i) {
    const uint32_t h = hasTree(hero) ? hero : 1u;
    return perkTable[(h - 1) * perksPerHero + std::min(i, perksPerHero - 1)];
}
inline uint32_t perkBit(uint32_t i) { return 1u << i; }
inline constexpr uint32_t perkMask = (1u << perksPerHero) - 1u;
inline constexpr uint32_t perkCost(uint32_t i) { return 2 * (tierOf[i] + 1); } // T1 2, T2 4, T3 6: 20 for a tree
inline constexpr uint32_t treeCost() {
    uint32_t c = 0;
    for (uint32_t i = 0; i < perksPerHero; ++i)
        c += perkCost(i);
    return c;
}
static_assert(treeCost() == static_cast<uint32_t>(repLevels), "the 20 levels fill the tree exactly (9:26)");
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

// HERO PERKS (the setting): 0 AUTO (the tree's order), 1 CHAIN FIRST (the chain before the other
// T2 perk), 2 HOLD POINTS (nothing bought), 3 PERKS OFF (bought as AUTO, no effect: to compare).
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
        static constexpr uint32_t autoOrder[perksPerHero] = {0, 1, 2, 3, 4, 5}, chainOrder[perksPerHero] = {2, 0, 1, 4, 3, 5};
        const uint32_t* order = pref == perkChainFirst ? chainOrder : autoOrder;
        uint32_t bought = 0;
        for (uint32_t k = 0; k < perksPerHero; ++k) {
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
    float statBonus(uint32_t hero, Stat s) const {
        if (off || !hasTree(hero))
            return 0.f;
        float b = 0;
        for (uint32_t i = 0; i < perksPerHero; ++i)
            if (owned[hero] & perkBit(i)) {
                const Perk& p = perk(hero, i);
                if (p.stat == s && std::isfinite(p.amount))
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
    // The tree, one line a step (the settings' PERK TREE rows): "T1 [x] FAST RETURN  [ ] BIG STORM".
    // Returns the lines written (4: T1, T2, T3, ULT; 0 for Spider-Man).
    uint32_t tree(uint32_t hero, bool ultimateOpen, char lines[4][96]) const {
        if (!hasTree(hero))
            return 0;
        for (uint32_t t = 0; t < tiers; ++t) {
            size_t at = 0;
            at += static_cast<size_t>(std::max(0, std::snprintf(lines[t], 96, "T%u", t + 1)));
            for (uint32_t i = 0; i < perksPerHero && at < 96; ++i)
                if (tierOf[i] == t)
                    at += static_cast<size_t>(std::max(0, std::snprintf(lines[t] + at, 96 - at, " [%c] %s", owned[hero] & perkBit(i) ? 'x' : ' ', perk(hero, i).name)));
        }
        std::snprintf(lines[3], 96, "ULT [%c] %s", ultimateOpen ? 'x' : ' ', ultimateTable[hero].name);
        return 4;
    }
    void save(char* out, size_t n) const {
        size_t at = 0;
        for (uint32_t h = 1; h < hero_modes::heroes && at < n; ++h) {
            const int w = std::snprintf(out + at, n - at, "perks.%u=%u\n", h, owned[h] & perkMask);
            if (w < 0)
                break;
            at += static_cast<size_t>(w);
        }
    }
    void load(const char* text) {
        for (uint32_t h = 1; text && h < hero_modes::heroes; ++h) {
            char key[24];
            std::snprintf(key, sizeof key, "\nperks.%u=", h);
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
