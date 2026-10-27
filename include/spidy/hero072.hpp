#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
// Made by Agent Mulder.
//
// Version 0.0.72 (TEST): the rules of the 0.0.72 list, pure (no game memory, no address), so
// core_tests drive them the way game_xr.cpp does. Every blow they return goes out on sm2_damage's
// CONFIRMED path (game_xr's queue); everything else is the mod's own (looks, the HUD, the colour
// pass, the pad). Each feature has its own toggle (combat38) and turns itself off (Guard) when its
// numbers come out insane, so nothing here can crash the game or drop VR to the flat screen.
//  - SPIDER CLIMB ONLY: only SPIDER-MAN crawls walls (the mod's climb, wall stick, crawl asks).
//  - UPPERCUT: a punch that lands while the fist moves up fast launches (a big upward knock).
//  - SMOKE BLIND: BATMAN's smoke cloud staggers the enemies inside it (CONFIRMED blows; true AI
//    blindness is CANDIDATE - the game's perception is not found).
//  - DAREDEVIL GRAPPLE: a fast upward cast of a baton arm (a gesture, never a trigger: the user's
//    rule, triggers are only for projectiles) with no enemy aimed grapples (no glide).
//  - DAREDEVIL PARRY: both batons crossed in front of the face blocks (a health refund, CANDIDATE
//    like SHIELD BLOCK) and staggers the nearest enemy.
//  - IRON MAN HUD: a light helmet frame at the edges, a lock box on the target, HOSTILES DETECTED
//    on the first hit, TARGET LOCKED, ALL HOSTILES NEUTRALIZED, the hostiles left, and speed and
//    altitude while flying.
//  - PUNISHER RAGE: after taking enough damage or downing enough enemies in a fight the view
//    goes red and his punches hit harder; it fades after the fight.
//  - REPUTATION: points for enemies downed and fights cleared, lost on being downed.
//  - SURVIVAL: limb damage rolls on each hit taken (per hero), healing over time; a broken leg
//    slows the walk and drifts the strafe, an injured arm weakens the mod's punches, a concussion
//    pulses the view.
//  - THOR: a hammer on the back (grip over a shoulder), trigger throws it (a projectile: it chains
//    and comes back, each hit electrocutes), held straight up overhead for 1 s it calls a radial
//    lightning strike (a gesture, no trigger), lightning crawls on his arms in a fight; a down swing
//    that lands calls a bolt, an up swing launches, a down swing on a locked enemy in the air slams
//    him down with lightning (THOR MELEE, gestures).
//  - GREEN LANTERN (the user, 8:18): FLIGHT without triggers - both arms straight up overhead take
//    off, then the hands steer (FLY MODE's poses); RING BEAM on the right trigger and the CHAIN
//    CONSTRUCT on the left (both projectiles); CONSTRUCT FIST = right grip + a fast punch; SHIELD
//    BUBBLE = both grips held and the hands pulled apart; RING CHARGE on the left wrist (flight
//    costs none).
// The user's rule for every hero: triggers only fire basic projectiles, never movement.
//  - ROSTER: each hero's tile and bio (the wrist's HERO page).
#include "spidy/captain.hpp"
#include "spidy/hero_modes.hpp"
#include "spidy/holster.hpp"
#include "spidy/math.hpp"
#include "spidy/sm2_damage.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

namespace spidy::hero072 {
using hero_modes::BatonEnemy;

inline bool ok(Vec3 v) { return finite(v); }
inline constexpr float halfTurn = 3.14159265f; // pi
inline float clampDt(float dt) { return std::isfinite(dt) ? std::clamp(dt, 0.f, .1f) : 0.f; }

// ---------------------------------------------------------------------------------------------
// Self-disabling toggle guard: a feature that trips (insane numbers) turns itself off for the
// rest of the session and says why once.
struct Guard {
    bool tripped{};
    uint32_t trips{};
    const char* why = "";
    bool live(bool on) const { return on && !tripped; }
    void trip(const char* reason) {
        if (!tripped) {
            tripped = true;
            why = reason ? reason : "";
            ++trips;
        }
    }
    // A finite check: false trips it.
    bool sane(bool good, const char* reason) {
        if (!good)
            trip(reason);
        return good && !tripped;
    }
};

// ---------------------------------------------------------------------------------------------
// SPIDER CLIMB ONLY.
// The user (8:35): ONLY SPIDER-MAN wall-crawls. The ways up for the rest (wayUp): BATMAN and DAREDEVIL
// grapple, SUPERMAN, IRON MAN, THOR and GREEN LANTERN fly, HULK jumps big, CAP leaps big, the
// PUNISHER fires a tactical grappling hook (HookLauncher) and WOLVERINE claws up: with his claws out
// (CLAW CLIMB) the game's own crawl is allowed and moved by the hands' pull (hand_crawl, grip only).
inline bool climbAllowed(uint32_t hero, bool spiderOnly, bool clawsOut = false, bool clawClimb = false) {
    return !spiderOnly || hero == hero_modes::spiderMan || (hero == hero_modes::wolverine && clawsOut && clawClimb);
}
enum class WayUp : uint32_t { wallCrawl, grapple, fly, bigJump, bigLeap, clawClimb, hookLauncher };
inline WayUp wayUp(uint32_t hero) {
    switch (hero) {
    case hero_modes::spiderMan: return WayUp::wallCrawl;
    case hero_modes::batman:
    case hero_modes::daredevil: return WayUp::grapple;
    case hero_modes::hulk: return WayUp::bigJump;
    case hero_modes::captainAmerica: return WayUp::bigLeap;
    case hero_modes::wolverine: return WayUp::clawClimb;
    case hero_modes::punisher: return WayUp::hookLauncher;
    default: return WayUp::fly; // SUPERMAN, IRON MAN, THOR, GREEN LANTERN
    }
}

// ---------------------------------------------------------------------------------------------
// The hands' velocity (combat_input's Hand has none).
struct HandSpeed {
    Vec3 last[2]{}, velocity[2]{};
    bool have[2]{};
    // The fastest upward speed in the last `memory` seconds, per hand (an uppercut lands at the top
    // of the swing, when the fist has slowed).
    float upPeak[2]{}, upPeakAge[2]{};
    static constexpr float memory = .25f;
    void update(float dt, const Vec3 p[2], const bool tracked[2]) {
        dt = clampDt(dt);
        for (unsigned i = 0; i < 2; ++i) {
            if (!tracked[i] || !ok(p[i])) {
                have[i] = false;
                velocity[i] = {};
                upPeak[i] = 0;
                continue;
            }
            if (have[i] && dt > 1e-4f) {
                const Vec3 v = (p[i] - last[i]) / dt;
                if (ok(v) && length(v) < 40.f)
                    velocity[i] = velocity[i] * .4f + v * .6f;
            }
            last[i] = p[i];
            have[i] = true;
            upPeakAge[i] += dt;
            if (upPeakAge[i] > memory)
                upPeak[i] = 0;
            const float sp = length(velocity[i]);
            if (sp > 1e-3f && velocity[i].y > upPeak[i] && velocity[i].y >= .6f * sp) {
                upPeak[i] = velocity[i].y;
                upPeakAge[i] = 0;
            }
        }
    }
};

// ---------------------------------------------------------------------------------------------
// UPPERCUT: the punch landed while the fist went up at least this fast (m/s, recent peak).
inline constexpr float uppercutUp = 2.2f;
inline bool isUppercut(float upPeak) { return std::isfinite(upPeak) && upPeak >= uppercutUp; }
// The launch: little damage, a big knock (kind 5: the knock the mod's punches use; the launch's
// direction is the game's - CANDIDATE that it goes up).
inline sm2_damage::Blow uppercutBlow() { return {10.f, 5, 520.f}; }
// Per hero (the user, 8:38): super strength launches far higher (SUPERMAN, HULK, THOR), IRON MAN's
// suit high, SPIDER-MAN, CAP, WOLVERINE and GREEN LANTERN moderately, BATMAN and DAREDEVIL a small
// pop at most, and the PUNISHER (no super strength) never: a normal stagger hit.
enum class UpperTier : uint32_t { stagger = 0, pop, moderate, high, huge };
inline UpperTier upperTier(uint32_t hero) {
    switch (hero) {
    case hero_modes::superman:
    case hero_modes::hulk:
    case hero_modes::thor: return UpperTier::huge;
    case hero_modes::ironMan: return UpperTier::high;
    case hero_modes::batman:
    case hero_modes::daredevil: return UpperTier::pop;
    case hero_modes::punisher: return UpperTier::stagger;
    default: return UpperTier::moderate; // SPIDER-MAN, CAP, WOLVERINE, GREEN LANTERN
    }
}
inline bool uppercutLaunches(uint32_t hero) { return upperTier(hero) >= UpperTier::moderate; }
inline sm2_damage::Blow uppercutBlow(uint32_t hero) {
    switch (upperTier(hero)) {
    case UpperTier::huge: return {18.f, 5, 980.f};
    case UpperTier::high: return {14.f, 5, 720.f};
    case UpperTier::moderate: return uppercutBlow();
    case UpperTier::pop: return {10.f, 5, 240.f};
    default: return {10.f, 5, 130.f}; // a stagger, no launch
    }
}

// GRAPPLE PULL (the user, 8:38): BATMAN's grapple gun and the PUNISHER's hook can also pick an
// ENEMY (the nearest standing one within pullCone of the aim and pullRange) and pull him in, open
// for a hit. What is CONFIRMED is the stagger (sm2_damage, pullStagger: he is open); moving him to
// the player needs a CONFIRMED position or yank write, which there is not: that part is CANDIDATE
// (pullTarget only picks; the game_xr side staggers him).
inline constexpr float pullRange = 22.f, pullCone = .97f /* ~14 deg */;
inline sm2_damage::Blow pullStagger() { return {6.f, 5, 140.f}; }
inline int pullTarget(Vec3 from, Vec3 aim, const BatonEnemy* list, uint32_t count) {
    if (!ok(from) || !ok(aim) || length(aim) < 1e-3f)
        return -1;
    const Vec3 n = normalized(aim);
    int best = -1;
    float bestD = pullRange;
    for (uint32_t i = 0; list && i < count; ++i) {
        if (!list[i].id || list[i].down || !ok(list[i].chest))
            continue;
        const Vec3 to = list[i].chest - from;
        const float d = length(to);
        if (d < .5f || d > bestD || dot(to / d, n) < pullCone)
            continue;
        bestD = d;
        best = static_cast<int>(i);
    }
    return best;
}

// ---------------------------------------------------------------------------------------------
// SMOKE BLIND: an enemy whose chest is within smokeRadius of a live puff is blinded for
// blindSeconds; going in staggers him, and every staggerEvery while he stays blind.
inline constexpr float smokeRadius = 2.6f, blindSeconds = 3.5f, staggerEvery = 1.4f;
inline sm2_damage::Blow smokeStagger() { return {4.f, 5, 160.f}; }
struct BlindOut {
    uint64_t ids[16]{};
    Vec3 at[16]{};
    uint32_t count{};
};
class SmokeBlind {
  public:
    struct Foe {
        uint64_t id{};
        float left{}, next{};
    };
    static constexpr uint32_t most = 16;
    Foe foes[most]{};
    uint64_t staggers{}, blinded{};
    void reset() { *this = SmokeBlind{}; }
    // puffs: the live smoke puffs' centres (tracking space), age < life.
    void step(float dt, const Vec3* puffs, const float* ages, uint32_t puffCount, float life, const BatonEnemy* list,
              uint32_t count, BlindOut& out) {
        dt = clampDt(dt);
        out.count = 0;
        for (auto& f : foes)
            if (f.id) {
                f.left -= dt;
                f.next -= dt;
                if (f.left <= 0)
                    f = {};
            }
        for (uint32_t e = 0; list && e < count; ++e) {
            const BatonEnemy& en = list[e];
            if (!en.id || en.down || !ok(en.chest))
                continue;
            bool in = false;
            for (uint32_t p = 0; puffs && p < puffCount && !in; ++p)
                if (ok(puffs[p]) && (!ages || (std::isfinite(ages[p]) && ages[p] < life)) &&
                    length(en.chest - puffs[p]) <= smokeRadius)
                    in = true;
            if (!in)
                continue;
            Foe* f = find(en.id);
            bool hit = false;
            if (!f) {
                f = free();
                if (!f)
                    continue;
                *f = {en.id, blindSeconds, staggerEvery};
                ++blinded;
                hit = true;
            } else {
                f->left = blindSeconds;
                if (f->next <= 0) {
                    f->next = staggerEvery;
                    hit = true;
                }
            }
            if (hit && out.count < 16) {
                out.ids[out.count] = en.id;
                out.at[out.count++] = en.chest;
                ++staggers;
            }
        }
    }
    bool isBlind(uint64_t id) const {
        for (const auto& f : foes)
            if (id && f.id == id)
                return true;
        return false;
    }

  private:
    Foe* find(uint64_t id) {
        for (auto& f : foes)
            if (f.id == id)
                return &f;
        return nullptr;
    }
    Foe* free() {
        for (auto& f : foes)
            if (!f.id)
                return &f;
        return nullptr;
    }
};

// ---------------------------------------------------------------------------------------------
// DAREDEVIL GRAPPLE (no glide), a gesture: a baton arm cast up fast (at least castSpeed m/s, mostly
// upward) ending at least castAbove over the eyes; the grapple goes along the hand's aim (the swing
// bridge's asks bit askGrappleOnly grapples without the cape). Once per castRearm seconds a hand.
inline constexpr float castSpeed = 3.0f, castAbove = .05f, castRearm = .8f;
struct BatonCast {
    float since[2]{castRearm, castRearm};
    // -> the hand that cast this frame (0 / 1), or -1.
    int step(float dt, Vec3 head, const Vec3 hand[2], const Vec3 velocity[2], const bool tracked[2]) {
        dt = clampDt(dt);
        int out = -1;
        for (int i = 0; i < 2; ++i) {
            since[i] = std::min(since[i] + dt, 10.f);
            if (!tracked[i] || !ok(head) || !ok(hand[i]) || !ok(velocity[i]))
                continue;
            const float sp = length(velocity[i]);
            if (out < 0 && since[i] >= castRearm && sp >= castSpeed && velocity[i].y >= .6f * sp &&
                hand[i].y >= head.y + castAbove) {
                since[i] = 0;
                out = i;
            }
        }
        return out;
    }
};

// DAREDEVIL PARRY: both batons in the hands, crossed (their segments within crossGap of each
// other, at least crossAngle apart) in front of the face (crossAhead metres ahead of the eyes,
// from chest height to over the head).
inline constexpr float crossGap = .09f, crossAngleCos = .82f /* ~35 deg */, crossAheadLeast = .12f,
                       crossAheadMost = .75f;
inline bool batonsCrossed(const Pose& head, Vec3 grip0, Vec3 tip0, Vec3 grip1, Vec3 tip1) {
    if (!ok(head.position) || !ok(grip0) || !ok(tip0) || !ok(grip1) || !ok(tip1))
        return false;
    const Vec3 a = tip0 - grip0, b = tip1 - grip1;
    if (length(a) < .05f || length(b) < .05f)
        return false;
    if (std::abs(dot(normalized(a), normalized(b))) > crossAngleCos)
        return false; // nearly parallel: not a cross
    if (hero_modes::segmentGap(grip0, tip0, grip1, tip1) > crossGap)
        return false;
    const Vec3 mid = (grip0 + tip0 + grip1 + tip1) * .25f;
    const Vec3 f = head.orientation.rotate(Vec3{0, 0, -1});
    const Vec3 rel = mid - head.position;
    const float ahead = dot(rel, f);
    return ahead >= crossAheadLeast && ahead <= crossAheadMost && rel.y >= -.6f && rel.y <= .35f;
}
inline constexpr float parryCounterRange = 2.8f;
// The user (8:35): batons crossed in front BLOCK; crossed right as a hit lands (within parryWindow
// of crossing) PARRY - the blow is blocked and the attacker staggered (parryCounter).
inline constexpr float parryWindow = .25f;
struct ParryTiming {
    float crossedFor = -1.f; // seconds the batons have been crossed (-1: not)
    enum Result : uint32_t { none = 0, block, parry };
    void step(float dt, bool crossed) {
        dt = clampDt(dt);
        crossedFor = crossed ? (crossedFor < 0 ? 0.f : crossedFor + dt) : -1.f;
    }
    // window: parryWindow, longer with DAREDEVIL's LONG PARRY perk (a bad number: the default).
    Result hit(float window = parryWindow) const {
        const float w = std::isfinite(window) ? std::clamp(window, .05f, 1.f) : parryWindow;
        return crossedFor < 0 ? none : crossedFor <= w ? parry : block;
    }
};
inline sm2_damage::Blow parryCounter() { return {8.f, 5, 260.f}; }
// The nearest standing enemy within range of p (-1 none).
inline int nearestEnemy(Vec3 p, const BatonEnemy* list, uint32_t count, float range) {
    int best = -1;
    float bestM = range;
    for (uint32_t i = 0; list && i < count; ++i) {
        if (!list[i].id || list[i].down || !ok(list[i].chest))
            continue;
        const float m = length(list[i].chest - p);
        if (m <= bestM) {
            bestM = m;
            best = static_cast<int>(i);
        }
    }
    return best;
}

// ---------------------------------------------------------------------------------------------
// The fight: enemies standing near, and how it went (IRON MAN HUD, PUNISHER RAGE, REPUTATION).
inline constexpr float fightRange = 30.f, fightEndSeconds = 3.f;
inline uint32_t standing(const BatonEnemy* list, uint32_t count, Vec3 at, float range) {
    uint32_t n = 0;
    for (uint32_t i = 0; list && i < count; ++i)
        if (list[i].id && !list[i].down && ok(list[i].chest) && (!ok(at) || length(list[i].chest - at) <= range))
            ++n;
    return n;
}
struct Fight {
    bool on{};
    float quiet{};       // seconds with no one standing
    uint32_t downs{};    // enemies downed this fight
    float damage{};      // share of most health lost this fight
    uint32_t cleared{};  // fights that ended with everyone down
    uint64_t downSeen[24]{};
    bool justStarted{}, justCleared{}, justAbandoned{};
    uint32_t abandoned{}; // fights walked away from with enemies still up (REPUTATION: a crime left)
    // standingFar (setAbandon before step): enemies still up but out of fightRange.
    uint32_t standingFar{};
    // standingNow: enemies standing near; lostShare: health lost this frame / most; newDowns this frame.
    void step(float dt, uint32_t standingNow, float lostShare, uint32_t newDowns, bool hitSent) {
        dt = clampDt(dt);
        justStarted = justCleared = justAbandoned = false;
        if (!on && standingNow > 0 && (hitSent || lostShare > 0)) {
            on = true;
            justStarted = true;
            quiet = 0;
            downs = 0;
            damage = 0;
        }
        if (!on)
            return;
        downs += newDowns;
        if (std::isfinite(lostShare) && lostShare > 0)
            damage += lostShare;
        if (standingNow == 0) {
            quiet += dt;
            if (quiet >= fightEndSeconds) {
                on = false;
                if (standingFar > 0) { // they are still up, the player left
                    justAbandoned = true;
                    ++abandoned;
                } else if (downs > 0) {
                    justCleared = true;
                    ++cleared;
                }
            }
        } else {
            quiet = 0;
        }
    }
};
// The enemies that went down since the last call (ids remembered, up to 24).
struct DownCounter {
    uint64_t seen[32]{};
    uint32_t next{};
    uint32_t step(const BatonEnemy* list, uint32_t count) {
        uint32_t n = 0;
        for (uint32_t i = 0; list && i < count; ++i) {
            if (!list[i].id || !list[i].down)
                continue;
            bool known = false;
            for (uint64_t s : seen)
                known = known || s == list[i].id;
            if (!known) {
                seen[next++ % 32] = list[i].id;
                ++n;
            }
        }
        return n;
    }
};

// ---------------------------------------------------------------------------------------------
// IRON MAN HUD: three lines in the helmet.
class IronHud {
  public:
    static constexpr float detectedSeconds = 2.5f, neutralizedSeconds = 3.f;
    float detected{}, neutralized{};
    uint64_t lockedId{};
    Vec3 lockedAt{};
    uint32_t standingNow{};
    bool fighting{}, flying{};
    float speed{}, altitude{}; // m/s and metres (altitude: the game's world height, CANDIDATE as "above the street")
    uint64_t shows{};
    void reset() { *this = IronHud{}; }
    // lockId/at: the enemy the aim assist has (0 none); standingCount: hostiles left near;
    // flyingNow/speedNow/altitudeNow: FLY MODE flying, the player's speed and height (NaN: unknown).
    void step(float dt, const Fight& fight, uint64_t lockId, Vec3 lockAt, uint32_t standingCount, bool flyingNow = false,
              float speedNow = 0, float altitudeNow = 0) {
        dt = clampDt(dt);
        detected = std::max(0.f, detected - dt);
        neutralized = std::max(0.f, neutralized - dt);
        if (fight.justStarted) {
            detected = detectedSeconds;
            neutralized = 0;
            ++shows;
        }
        if (fight.justCleared) {
            neutralized = neutralizedSeconds;
            detected = 0;
            ++shows;
        }
        fighting = fight.on;
        lockedId = fight.on ? lockId : 0;
        lockedAt = lockAt;
        standingNow = standingCount;
        flying = flyingNow;
        speed = std::isfinite(speedNow) ? std::clamp(speedNow, 0.f, 999.f) : 0.f;
        altitude = std::isfinite(altitudeNow) ? std::clamp(altitudeNow, -999.f, 9999.f) : 0.f;
    }
    static constexpr uint32_t lineCount = 5;
    // The lines this frame (empty: none) and each one's alpha: 0 HOSTILES DETECTED, 1 TARGET LOCKED,
    // 2 ALL HOSTILES NEUTRALIZED, 3 the enemy count (in a fight), 4 speed and altitude (flying).
    void lines(char text[lineCount][40], float a[lineCount]) const {
        for (uint32_t i = 0; i < lineCount; ++i) {
            text[i][0] = 0;
            a[i] = 0;
        }
        if (detected > 0) {
            std::snprintf(text[0], 40, "HOSTILES DETECTED");
            a[0] = std::min(1.f, detected * 2.f) * (std::fmod(detected, .5f) > .12f ? 1.f : .45f);
        }
        if (lockedId) {
            std::snprintf(text[1], 40, "TARGET LOCKED");
            a[1] = 1;
        }
        if (neutralized > 0) {
            std::snprintf(text[2], 40, "ALL HOSTILES NEUTRALIZED");
            a[2] = std::min(1.f, neutralized * 2.f);
        }
        if (fighting && neutralized <= 0) {
            std::snprintf(text[3], 40, "HOSTILES: %u", standingNow);
            a[3] = .9f;
        }
        if (flying) {
            std::snprintf(text[4], 40, "SPD %.0f M/S  ALT %.0f M", speed, altitude);
            a[4] = .9f;
        }
    }
};

// ---------------------------------------------------------------------------------------------
// PUNISHER RAGE.
// PUNISHER RAGE MODE (the user, 8:28): heavy damage OR 5 enemies downed in one fight (whichever
// first), only in a fight; red vision (rageTint, a heartbeat at the edges) fading after the fight;
// he hits harder (rageBlow on punches, rageShotScale on the mod's shot blows) while it is on.
inline constexpr float rageDamage = .30f; // share of most health lost in one fight
inline constexpr uint32_t rageDowns = 5;  // enemies downed in one fight
inline constexpr float rageShotScale = 1.5f;
inline constexpr float rageRise = 1.5f, rageFade = .35f; // level per second
class Rage {
  public:
    float level{};
    bool raging{};
    uint64_t rages{};
    float after{}; // seconds raging since the fight ended
    void reset() { *this = Rage{}; }
    // keepSeconds: the PUNISHER's LONG RAGE perk keeps the rage that long after the fight (0: none).
    void step(float dt, bool on, const Fight& fight, float keepSeconds = 0.f) {
        dt = clampDt(dt);
        if (!on) {
            level = 0;
            raging = false;
            return;
        }
        if (fight.on && !raging && (fight.damage >= rageDamage || fight.downs >= rageDowns)) {
            raging = true;
            ++rages;
        }
        const float keep = std::isfinite(keepSeconds) ? std::clamp(keepSeconds, 0.f, 30.f) : 0.f;
        after = fight.on ? 0.f : after + dt;
        if (!fight.on && after > keep)
            raging = false;
        level = raging ? std::min(1.f, level + dt * rageRise) : std::max(0.f, level - dt * rageFade);
    }
};
// RAGE punch: one extra punch's worth on each landed punch while raging.
inline sm2_damage::Blow rageBlow() { return {hero_modes::punchUnitHp, 5, 140.f}; }

// ---------------------------------------------------------------------------------------------
// REPUTATION (per hero). THE GRIND (the user, 8:46): the top takes real play, about 10-15 hours per
// hero - slow to rise, faster to fall for mistakes - with suit unlocks and perk levels spaced along
// the way and the ULTIMATE at the top. Every number is in this one table (repTuning) to tune.
// The estimate: a street fight (~2 min with the travel) downs ~5 and is cleared: 5 x down + cleared
// = 22 points, ~33 fights an hour = ~730 an hour: 10000 in ~13.7 hours.
struct RepTuning {
    int32_t start, most;                 // a new hero's points, the top (the ULTIMATE)
    int32_t down, cleared, fire, saved;  // the gains
    int32_t abandoned, defeated, car;    // the losses (heavier)
    int32_t levelEvery;                  // a level (a perk point) every this many points
    int32_t antiHero;                    // rep lost to under this: the ANTI-HERO tag (9:26)
    int32_t suitTier[3];                 // the 2nd, 3rd and 4th suit of a hero unlock here
};
inline constexpr RepTuning repTuning{0, 10000, 2, 12, 15, 20, -60, -120, -40, 500,
                                     250, {2500, 5000, 7500}};
inline constexpr int32_t repStart = repTuning.start, repMost = repTuning.most;
inline constexpr int32_t repDown = repTuning.down, repCleared = repTuning.cleared, repDefeated = repTuning.defeated;
// A hero's level (0 .. repMost / levelEvery = 20): one perk point each. Losing rep can drop a level.
inline int32_t repLevel(int32_t points) { return std::clamp(points, 0, repMost) / repTuning.levelEvery; }
inline constexpr int32_t repLevels = repMost / repTuning.levelEvery;
// THE TITLES (9:26): 5 titles of 4 steps each = the 20 levels. Level 1-4 ROOKIE 1-4, 5-8 VIGILANTE,
// 9-12 HERO, 13-16 SUPERHERO, 17-20 LEGEND; LEGEND 4 (level 20) is the max: the ULTIMATE and the
// FINAL suit. Level 0 (a new hero): ROOKIE, no step yet.
inline constexpr uint32_t repTitles = 5, repSteps = 4;
inline constexpr const char* repTitleNames[repTitles] = {"ROOKIE", "VIGILANTE", "HERO", "SUPERHERO", "LEGEND"};
static_assert(repTitles * repSteps == static_cast<uint32_t>(repLevels), "5 titles x 4 steps = the 20 levels");
inline uint32_t repTitleIndex(int32_t points) {
    const int32_t l = repLevel(points);
    return l <= 0 ? 0u : std::min(repTitles - 1, static_cast<uint32_t>(l - 1) / repSteps);
}
inline uint32_t repStep(int32_t points) {
    const int32_t l = repLevel(points);
    return l <= 0 ? 0u : static_cast<uint32_t>(l - 1) % repSteps + 1;
}
inline const char* repTitle(int32_t points) { return repTitleNames[repTitleIndex(points)]; }
inline bool repLegend(int32_t points) { return points >= repMost; }
// The user (8:32): per hero. Up: enemies downed (villains beaten), a fight cleared (a crime stopped),
// a fire put out (FIRE DOUSE - its game hook is CANDIDATE, so 0 until it is CONFIRMED), a person
// saved (no CONFIRMED signal yet: an event kept for when there is one). Down: a fight walked away
// from (a crime left unfinished), being defeated, a car wrecked by the player (no CONFIRMED car
// damage / explosion / wreck state found: CANDIDATE, unused) and civilians hurt (no reliable signal:
// left out). The civilians' cheer / flee needs NPC behaviour hooks that are not CONFIRMED: the
// meter only (the wrist hologram).
inline constexpr int32_t repFire = repTuning.fire, repSaved = repTuning.saved, repAbandoned = repTuning.abandoned,
                         repCar = repTuning.car;
struct RepEvents {
    uint32_t downs{}, fires{}, saved{}, cars{};
    bool cleared{}, abandoned{}, defeated{};
};
struct Reputation {
    int32_t points = repStart;
    int32_t heroPoints[hero_modes::heroes]{repStart, repStart, repStart, repStart, repStart, repStart,
                                           repStart, repStart, repStart, repStart, repStart};
    uint32_t hero{};
    bool dirty{};
    bool antiHero[hero_modes::heroes]{}; // rep LOST to under repTuning.antiHero (a new hero is not one)
    void select(uint32_t h) {
        if (h < hero_modes::heroes && h != hero) {
            heroPoints[hero] = points;
            hero = h;
            points = heroPoints[h];
        }
    }
    void add(int32_t d) {
        const int32_t was = points;
        points = std::clamp(points + d, 0, repMost);
        if (hero < hero_modes::heroes) {
            heroPoints[hero] = points;
            if (d < 0 && points < repTuning.antiHero)
                antiHero[hero] = true;
            else if (points >= repTuning.antiHero)
                antiHero[hero] = false;
        }
        dirty = dirty || points != was;
    }
    void step(uint32_t newDowns, bool cleared, bool defeated) {
        RepEvents e;
        e.downs = newDowns;
        e.cleared = cleared;
        e.defeated = defeated;
        step(e);
    }
    void step(const RepEvents& e) {
        if (e.downs)
            add(static_cast<int32_t>(std::min(e.downs, 8u)) * repDown);
        if (e.cleared)
            add(repCleared);
        if (e.fires)
            add(static_cast<int32_t>(std::min(e.fires, 4u)) * repFire);
        if (e.saved)
            add(static_cast<int32_t>(std::min(e.saved, 4u)) * repSaved);
        if (e.abandoned)
            add(repAbandoned);
        if (e.cars)
            add(static_cast<int32_t>(std::min(e.cars, 4u)) * repCar);
        if (e.defeated)
            add(repDefeated);
    }
    bool isAntiHero() const { return hero < hero_modes::heroes && antiHero[hero] && points < repTuning.antiHero; }
    // "REP 5120 LV 10 HERO 2" ("... ROOKIE  ANTI-HERO" when rep was lost to the bottom).
    void text(char* out, size_t n) const {
        const uint32_t step = repStep(points);
        char st[8] = "";
        if (step)
            std::snprintf(st, sizeof st, " %u", step);
        std::snprintf(out, n, "REP %d LV %d %s%s%s", points, repLevel(points), repTitle(points), st, isAntiHero() ? "  ANTI-HERO" : "");
    }
    // The meter, 0-1 (the wrist hologram's bar).
    float meter() const { return std::clamp(points / static_cast<float>(repMost), 0.f, 1.f); }
    // The saved line ("reputation=512") read back; anything else: the start.
    static int32_t parse(const char* s) {
        if (!s)
            return repStart;
        const char* v = std::strstr(s, "reputation=");
        if (!v)
            return repStart;
        const long p = std::strtol(v + 11, nullptr, 10);
        return static_cast<int32_t>(std::clamp<long>(p, 0, repMost));
    }
    // Per hero: "reputation.<hero>=N" lines (heroes 0-10); a missing one: the old shared line, else the start.
    void load(const char* s) {
        const int32_t shared = parse(s);
        for (uint32_t h = 0; h < hero_modes::heroes; ++h) {
            heroPoints[h] = shared;
            char key[24];
            std::snprintf(key, sizeof key, "reputation.%u=", h);
            if (const char* v = s ? std::strstr(s, key) : nullptr)
                heroPoints[h] = static_cast<int32_t>(std::clamp<long>(std::strtol(v + std::strlen(key), nullptr, 10), 0, repMost));
            std::snprintf(key, sizeof key, "antihero.%u=1", h);
            antiHero[h] = s && std::strstr(s, key) && heroPoints[h] < repTuning.antiHero;
        }
        points = heroPoints[hero < hero_modes::heroes ? hero : 0];
    }
    // All of them, one line each (the save file).
    void save(char* out, size_t n) const {
        size_t at = 0;
        for (uint32_t h = 0; h < hero_modes::heroes && at < n; ++h) {
            const int32_t pts = h == hero ? points : heroPoints[h];
            const int w = antiHero[h] ? std::snprintf(out + at, n - at, "reputation.%u=%d\nantihero.%u=1\n", h, pts, h)
                                      : std::snprintf(out + at, n - at, "reputation.%u=%d\n", h, pts);
            if (w < 0)
                break;
            at += static_cast<size_t>(w);
        }
    }
};

// ---------------------------------------------------------------------------------------------
// NATURAL INVINCIBLE (the user, 8:23): THOR and SUPERMAN take no damage. Done through INVINCIBLE's
// CONFIRMED refill (extras385::invincible: the hero health record topped back up every frame, every
// source - hits, falls, slams - never above the game's cap and NEVER a revive, so a one-shot or a
// scripted death still happens and nothing the game expects can softlock). SURVIVAL already leaves
// them alone (toughness 0).
inline bool naturallyInvincible(uint32_t hero) { return hero == hero_modes::thor || hero == hero_modes::superman; }
inline bool keepTopped(uint32_t hero, bool optionOn, bool invincibleExtra) {
    return invincibleExtra || (optionOn && naturallyInvincible(hero));
}

// ---------------------------------------------------------------------------------------------
// HULK RAGE (the user, 8:24): health lost in a fight builds his punch multiplier (1x .. hulkRageMost),
// hulkRagePerShare per whole bar lost; after the fight it fades back at hulkRageFade per second.
inline constexpr float hulkRageMost = 2.f, hulkRagePerShare = 2.f, hulkRageFade = .2f;
struct HulkRage {
    float mult = 1.f;
    uint64_t builds{};
    void reset() { *this = HulkRage{}; }
    // build: HULK's FAST RAGE perk (1: as 0.0.72 shipped; a bad number: 1).
    void step(float dt, bool on, const Fight& fight, float lostShare, float build = 1.f) {
        dt = clampDt(dt);
        if (!on) {
            mult = 1.f;
            return;
        }
        const float b = std::isfinite(build) ? std::clamp(build, 1.f, 3.f) : 1.f;
        if (fight.on && std::isfinite(lostShare) && lostShare > 0) {
            mult = std::min(hulkRageMost, mult + std::min(lostShare, 1.f) * hulkRagePerShare * b);
            ++builds;
        }
        if (!fight.on)
            mult = std::max(1.f, mult - hulkRageFade * dt);
        if (!std::isfinite(mult))
            mult = 1.f;
    }
    // 0 (calm) .. 1 (the cap): the tint's strength.
    float level() const { return std::clamp((mult - 1.f) / (hulkRageMost - 1.f), 0.f, 1.f); }
    sm2_damage::Blow scaled(sm2_damage::Blow b) const {
        const float m = std::clamp(std::isfinite(mult) ? mult : 1.f, 1.f, hulkRageMost);
        b.amount *= m;
        b.knockAmount *= 1.f + .25f * (m - 1.f);
        return b;
    }
};

// The rage tints (the colour pass, eye_color: subtle, a tint and a vignette only): HULK green, the
// PUNISHER red. level 0-1, quantised to tenths so the pass is set again only when it shows.
// PUNISHER RAGE MODE's (the user, 8:28): a red that PULSES like a heartbeat (lub-dub, 90-120 bpm,
// faster as the rage builds) at the EDGES only - the pulse moves the vignette, never the whole view
// (no full-screen flash), by at most heartVignette (x the RAGE TINT intensity). It fades in with
// the rage and out after the fight (Rage::level).
inline constexpr float heartBpmLow = 90.f, heartBpmHigh = 120.f, heartDub = .30f, heartWidth = .14f,
                       heartDubShare = .6f;
inline constexpr uint32_t heartVignette = 14;
inline float heartBpm(float level) {
    return heartBpmLow + (heartBpmHigh - heartBpmLow) * (std::isfinite(level) ? std::clamp(level, 0.f, 1.f) : 0.f);
}
// The lub (at 0) and the softer dub (heartDub of a beat later), each a smooth bump; 0-1.
inline float heartPulse(float phase) {
    if (!std::isfinite(phase))
        return 0.f;
    phase -= std::floor(phase);
    const auto bump = [](float x) {
        if (x < 0 || x >= heartWidth)
            return 0.f;
        const float s = std::sin(halfTurn * x / heartWidth);
        return s * s;
    };
    return std::max(bump(phase), heartDubShare * bump(phase - heartDub));
}
struct HeartBeat {
    float phase{};
    // The pulse now (0-1); the phase runs at heartBpm(level) so a bpm change never jumps.
    float step(float dt, float level) {
        dt = clampDt(dt);
        phase += dt * heartBpm(level) / 60.f;
        phase -= std::floor(phase);
        return heartPulse(phase);
    }
};
// RAGE TINT (the intensity setting): NORMAL, STRONG, SUBTLE, OFF.
inline float rageTintScale(uint32_t v) { return v == 1 ? 1.4f : v == 2 ? .6f : v == 3 ? 0.f : 1.f; }
struct RageTint {
    int32_t tint{}, warmth{};
    uint32_t vignette{}, step{};
    bool operator==(const RageTint& o) const {
        return tint == o.tint && warmth == o.warmth && vignette == o.vignette && step == o.step;
    }
    bool operator!=(const RageTint& o) const { return !(*this == o); }
};
inline RageTint rageTint(uint32_t hero, float level, float pulse = 0.f, float intensity = 1.f) {
    RageTint t;
    if (!std::isfinite(level) || level <= 0 || !std::isfinite(intensity) || intensity <= 0)
        return t;
    intensity = std::min(intensity, 1.5f);
    pulse = std::isfinite(pulse) ? std::clamp(pulse, 0.f, 1.f) : 0.f;
    t.step = static_cast<uint32_t>(std::lround(std::clamp(level, 0.f, 1.f) * 10.f));
    const float q = t.step * .1f;
    if (hero == hero_modes::hulk) {
        t.tint = -static_cast<int32_t>(std::lround(35 * q * intensity)); // green
        t.vignette = static_cast<uint32_t>(std::lround(30 * q * intensity));
    } else if (hero == hero_modes::punisher) {
        t.tint = static_cast<int32_t>(std::lround(25 * q * intensity));   // magenta ...
        t.warmth = static_cast<int32_t>(std::lround(45 * q * intensity)); // ... and amber: red
        t.vignette = static_cast<uint32_t>(std::lround((40.f + heartVignette * pulse) * q * intensity));
    } else {
        t.step = 0;
    }
    if (t.step == 0)
        t = {};
    return t;
}

// ---------------------------------------------------------------------------------------------
// RAGE BLOOD (the user, 8:28): enemies shot while the PUNISHER rages bleed - drops (mod-drawn, the
// game's own blood effects are not CONFIRMED) that fall and leave pools on the floor. EXTREME (the
// default) a LOT, NORMAL some, OFF none. Capped (bloodDropsMost, bloodPoolsMost: the oldest go first)
// and BloodPerf turns it off for the session when the frames drop while it draws.
enum BloodMode : uint32_t { bloodExtreme = 0, bloodNormal, bloodOff };
inline constexpr uint32_t bloodDropsMost = 360, bloodPoolsMost = 40;
inline constexpr uint32_t bloodDropsPerShot[2] = {64, 16}, bloodPoolsPerShot[2] = {3, 1};
inline constexpr float bloodDropSeconds = 2.5f, bloodPoolSeconds[2] = {14.f, 8.f}, bloodGravity = 9.8f;
inline constexpr float bloodPoolMost = .55f;
struct BloodDrop {
    Vec3 at{}, velocity{};
    float age{}, floor{}, size{};
    bool live{};
};
struct BloodPool {
    Vec3 at{};
    float age{}, radius{}, most{};
    bool live{};
};
class Blood {
  public:
    BloodDrop drops[bloodDropsMost]{};
    BloodPool pools[bloodPoolsMost]{};
    uint32_t nextDrop{}, nextPool{}, mode{bloodOff};
    uint32_t seed = 0x2545f491u;
    uint64_t shots{}, spawned{};
    float rnd() {
        seed ^= seed << 13;
        seed ^= seed >> 17;
        seed ^= seed << 5;
        return (seed & 0xffffffu) / 16777216.f;
    }
    void clear() {
        for (auto& d : drops)
            d.live = false;
        for (auto& p : pools)
            p.live = false;
    }
    uint32_t liveDrops() const {
        uint32_t n = 0;
        for (const auto& d : drops)
            n += d.live;
        return n;
    }
    uint32_t livePools() const {
        uint32_t n = 0;
        for (const auto& p : pools)
            n += p.live;
        return n;
    }
    void pool(Vec3 at, float most) {
        BloodPool& p = pools[nextPool++ % bloodPoolsMost];
        p = {at, 0.f, .05f, std::clamp(most, .1f, bloodPoolMost), true};
    }
    // A shot landed at `at` (from the muzzle along `dir`); `floor` the enemy's feet height.
    void shot(uint32_t m, Vec3 at, Vec3 dir, float floor) {
        if (m >= bloodOff || !ok(at) || !ok(dir) || !std::isfinite(floor))
            return;
        mode = m;
        ++shots;
        const Vec3 d = length(dir) > 1e-4f ? normalized(dir) : Vec3{0, 0, -1};
        for (uint32_t i = 0; i < bloodDropsPerShot[m]; ++i) {
            BloodDrop& b = drops[nextDrop++ % bloodDropsMost];
            const Vec3 spray{rnd() * 2 - 1, rnd() * 2 - .4f, rnd() * 2 - 1};
            const float sp = 1.5f + rnd() * (m == bloodExtreme ? 5.5f : 3.f);
            b = {at, d * sp + spray * (m == bloodExtreme ? 2.2f : 1.4f), 0.f, floor, .008f + rnd() * .02f, true};
            ++spawned;
        }
        for (uint32_t i = 0; i < bloodPoolsPerShot[m]; ++i)
            pool({at.x + d.x * (.3f + rnd()), floor + .01f, at.z + d.z * (.3f + rnd())}, .2f + rnd() * .35f);
    }
    void step(float dt) {
        dt = clampDt(dt);
        for (auto& b : drops) {
            if (!b.live)
                continue;
            b.age += dt;
            b.velocity.y -= bloodGravity * dt;
            b.at = b.at + b.velocity * dt;
            if (!ok(b.at) || b.age >= bloodDropSeconds) {
                b.live = false;
            } else if (b.at.y <= b.floor) {
                b.live = false;
                if (mode == bloodExtreme && rnd() < .08f) // a splat where it lands
                    pool({b.at.x, b.floor + .01f, b.at.z}, .08f + rnd() * .12f);
            }
        }
        const float last = bloodPoolSeconds[mode == bloodExtreme ? 0 : 1];
        for (auto& p : pools) {
            if (!p.live)
                continue;
            p.age += dt;
            p.radius = std::min(p.most, p.radius + dt * .6f);
            if (p.age >= last)
                p.live = false;
        }
    }
};
// Frames dropping while the blood draws (slower than 1.4x the headset's period, for a total of
// bloodSlowTrip seconds, recovering at half speed) turn RAGE BLOOD off for the session.
inline constexpr float bloodSlowFactor = 1.4f, bloodSlowTrip = .75f;
struct BloodPerf {
    float slow{};
    bool off{};
    // Returns true the frame it turns off.
    bool step(float dt, float period, bool drawing) {
        if (off || !std::isfinite(dt) || !std::isfinite(period) || period <= 0)
            return false;
        if (drawing && dt > period * bloodSlowFactor)
            slow += std::min(dt, .1f);
        else
            slow = std::max(0.f, slow - .5f * std::min(dt, .1f));
        if (slow >= bloodSlowTrip) {
            off = true;
            return true;
        }
        return false;
    }
};

// ---------------------------------------------------------------------------------------------
// SURVIVAL.
enum Limb : uint32_t { head = 0, torso, leftArm, rightArm, leftLeg, rightLeg, limbs };
inline constexpr const char* limbNames[limbs] = {"HEAD", "TORSO", "L ARM", "R ARM", "L LEG", "R LEG"};
inline constexpr float limbWeight[limbs] = {.10f, .34f, .14f, .14f, .14f, .14f};
// How much a hero gets hurt (0: SURVIVAL never touches him) and how fast he heals (points/s).
struct Toughness {
    float hurt, heal;
};
inline Toughness toughness(uint32_t hero) {
    switch (hero) {
    case hero_modes::spiderMan: return {.6f, 1.2f};
    case hero_modes::batman: return {.85f, .6f};
    case hero_modes::punisher: return {1.f, .45f};
    case hero_modes::daredevil: return {.8f, .7f};
    case hero_modes::captainAmerica: return {.5f, 1.f};
    case hero_modes::wolverine: return {.7f, 4.f}; // the healing factor: fast, and even mid-fight
    default: return {0.f, 0.f}; // SUPERMAN, THOR, HULK (the user, 8:33: immune), GREEN LANTERN, IRON MAN (the suit)
    }
}
inline bool survivalHero(uint32_t hero) { return toughness(hero).hurt > 0; }
inline constexpr float limbMost = 100.f, injuredBelow = 50.f, healDelay = 6.f;
// HEALING (the user, 8:33): only while RESTING - out of a fight, not swinging, slower than restSpeed
// (standing or walking) for restSeconds. Nothing heals in a fight. WOLVERINE (healsInFight) heals
// always, after wolverineDelay, at his faster rate.
inline constexpr float restSpeed = 2.5f /* m/s */, restSeconds = 4.f, wolverineDelay = 1.f;
inline bool healsInFight(uint32_t hero) { return hero == hero_modes::wolverine; }
class Survival {
  public:
    float limb[limbs]{limbMost, limbMost, limbMost, limbMost, limbMost, limbMost};
    float sinceHit = 99.f, rested{};
    hero_modes::Roll roll{0x51f15e1u};
    uint64_t rolls{}, injuries{};
    uint32_t lastLimb = limbs;
    float lastChance{};
    void reset() {
        for (float& l : limb)
            l = limbMost;
        sinceHit = 99.f;
        rested = 0;
        lastLimb = limbs;
    }
    // The chance a hit of `lostShare` of most health hurts a limb (scaled by the hero).
    static float chance(float lostShare, float hurt) {
        if (!std::isfinite(lostShare) || lostShare <= 0 || !(hurt > 0))
            return 0.f;
        return std::clamp(lostShare * 5.f * hurt, 0.f, .9f);
    }
    // inFight: Fight::on; speed: the player's m/s (NaN: not resting); swinging: on a web or a zip.
    void step(float dt, uint32_t hero, float lostShare, bool inFight = false, float speed = 0.f, bool swinging = false) {
        dt = clampDt(dt);
        const Toughness t = toughness(hero);
        if (t.hurt <= 0) {
            reset();
            return;
        }
        sinceHit += dt;
        if (std::isfinite(lostShare) && lostShare > .002f) {
            sinceHit = 0;
            lastChance = chance(lostShare, t.hurt);
            ++rolls;
            if (roll.chance(lastChance)) {
                float pick = roll.next01(), acc = 0;
                uint32_t which = torso;
                for (uint32_t l = 0; l < limbs; ++l) {
                    acc += limbWeight[l];
                    if (pick <= acc) {
                        which = l;
                        break;
                    }
                }
                const float hurtBy = std::clamp(lostShare * 260.f * t.hurt, 8.f, 70.f);
                limb[which] = std::max(0.f, limb[which] - hurtBy);
                lastLimb = which;
                ++injuries;
            }
        }
        const bool resting = !inFight && !swinging && std::isfinite(speed) && speed < restSpeed;
        rested = resting ? rested + dt : 0.f;
        if (healing(hero))
            for (float& l : limb)
                l = std::min(limbMost, l + t.heal * dt);
    }
    // Healing now (after step): WOLVERINE a second after a hit, the rest only resting.
    bool healing(uint32_t hero) const {
        if (!survivalHero(hero))
            return false;
        if (healsInFight(hero))
            return sinceHit >= wolverineDelay;
        return rested >= restSeconds && sinceHit >= healDelay;
    }
    bool brokenLeg() const { return limb[leftLeg] < injuredBelow || limb[rightLeg] < injuredBelow; }
    bool injuredArm(unsigned hand) const { return limb[hand == 0 ? leftArm : rightArm] < injuredBelow; }
    bool concussed() const { return limb[head] < injuredBelow; }
    // An injured arm's punches: half (the mod's blows only - the game's own punch is the game's).
    float punchScale(unsigned hand) const { return injuredArm(hand) ? .5f : 1.f; }
    // A broken leg: the walk stick scaled down and a sideways drift (the pad's left stick).
    void legStick(float& x, float& y, float seconds) const {
        if (!brokenLeg() || !std::isfinite(x) || !std::isfinite(y))
            return;
        const float worst = std::min(limb[leftLeg], limb[rightLeg]);
        const float s = .55f + .25f * std::clamp(worst / injuredBelow, 0.f, 1.f);
        const float mag = std::sqrt(x * x + y * y);
        const float drift = mag > .15f ? .18f * std::sin(std::isfinite(seconds) ? seconds * 1.7f : 0.f) : 0.f;
        x = std::clamp(x * s + drift, -1.f, 1.f);
        y = std::clamp(y * s, -1.f, 1.f);
    }
    // CONCUSSION: the view's pulse 0-1 (a slow throb; the colour pass's vignette and blur-ish dim).
    float dazed(float seconds) const {
        if (!concussed() || !std::isfinite(seconds))
            return 0.f;
        const float depth = std::clamp((injuredBelow - limb[head]) / injuredBelow, .25f, 1.f);
        const float wave = .5f + .5f * std::sin(seconds * 2.3f);
        return depth * wave;
    }
    // "HEAD 100  TORSO 82 ..." (the hologram's first page).
    void text(char* out, size_t n) const {
        std::snprintf(out, n, "HEAD %.0f TORSO %.0f  L ARM %.0f R ARM %.0f  L LEG %.0f R LEG %.0f", limb[head],
                      limb[torso], limb[leftArm], limb[rightArm], limb[leftLeg], limb[rightLeg]);
    }
};

// ---------------------------------------------------------------------------------------------
// FIRE DOUSE (the user, 8:30, 8:31): each hero's own way to put a fire out. SPIDER-MAN the game's
// own web balls (nothing of ours); SUPERMAN his COLD BREATH (hero007); IRON MAN foam from the palm
// (PalmSpray); BATMAN and the PUNISHER a foam grenade from the belt (the SMOKE PELLET's grab and
// throw, a thrown projectile); THOR spins the hammer fast (SpinGesture, NOT raised overhead - that is
// the storm) for a gust; GREEN LANTERN a construct hose (HoseGesture); HULK a thunder clap
// (ClapGesture); CAP a SHIELD SLAM (ShieldSlam, also a combat move); DAREDEVIL twirls the batons fast
// like a fan (SpinGesture). No triggers: all gestures or a belt grab.
// The game's fires: NO CONFIRMED address for a fire or its extinguish logic was found - the hook is
// CANDIDATE (fireHookConfirmed false), so the douse is drawn and tested on the mod's own FireSpots
// and puts out nothing in the game until a fire hook is CONFIRMED (the toggle says so once).
inline constexpr bool fireHookConfirmed = false;
enum class Douse : uint32_t { none = 0, webBalls, coldBreath, foamPalm, foamGrenade, hammerGust, constructHose, thunderClap, shieldSlam, batonFan, douses };
inline Douse douseFor(uint32_t hero) {
    switch (hero) {
    case hero_modes::spiderMan: return Douse::webBalls;
    case hero_modes::superman: return Douse::coldBreath;
    case hero_modes::ironMan: return Douse::foamPalm;
    case hero_modes::batman:
    case hero_modes::punisher: return Douse::foamGrenade;
    case hero_modes::thor: return Douse::hammerGust;
    case hero_modes::greenLantern: return Douse::constructHose;
    case hero_modes::hulk: return Douse::thunderClap;
    case hero_modes::captainAmerica: return Douse::shieldSlam;
    case hero_modes::daredevil: return Douse::batonFan;
    default: return Douse::none; // WOLVERINE
    }
}
inline const char* douseName(Douse d) {
    constexpr const char* n[] = {"NONE", "WEB BALLS", "COLD BREATH", "PALM FOAM", "FOAM GRENADE", "HAMMER GUST",
                                 "CONSTRUCT HOSE", "THUNDER CLAP", "SHIELD SLAM", "BATON FAN"};
    const auto i = static_cast<uint32_t>(d);
    return i < static_cast<uint32_t>(Douse::douses) ? n[i] : "?";
}
// A spray (a cone ahead, heat off per second) or a burst (a sphere, heat off at once).
struct DouseShape {
    float reach{}, cosHalf{}, perSecond{}, burst{};
};
inline DouseShape douseShape(Douse d) {
    switch (d) {
    case Douse::coldBreath: return {9.f, .94f, 1.2f, 0.f};
    case Douse::foamPalm: return {7.f, .93f, 1.4f, 0.f};
    case Douse::constructHose: return {10.f, .95f, 1.6f, 0.f};
    case Douse::hammerGust: return {5.f, .5f, 1.5f, 0.f};
    case Douse::batonFan: return {3.f, .6f, 1.1f, 0.f};
    case Douse::foamGrenade: return {3.f, -1.f, 0.f, 1.f};
    case Douse::thunderClap: return {8.f, .35f, 0.f, 1.f};
    case Douse::shieldSlam: return {4.f, -1.f, 0.f, 1.f};
    default: return {};
    }
}
struct FireSpot {
    Vec3 at{};
    float heat{}; // 0 out .. 1
    bool live{};
};
class Fires {
  public:
    static constexpr uint32_t most = 16;
    FireSpot spots[most]{};
    uint64_t putOut{};
    void add(Vec3 at, float heat = 1.f) {
        for (auto& f : spots)
            if (!f.live) {
                f = {at, std::clamp(heat, .05f, 1.f), true};
                return;
            }
    }
    uint32_t live() const {
        uint32_t n = 0;
        for (const auto& f : spots)
            n += f.live;
        return n;
    }
    // Douse from `from` along `dir` (a spray: dt of it; a burst: at once, dir unused when cosHalf < -.99).
    // Returns the fires that went out.
    uint32_t douse(Douse d, Vec3 from, Vec3 dir, float dt) {
        const DouseShape sh = douseShape(d);
        if (sh.reach <= 0 || !ok(from))
            return 0;
        dt = clampDt(dt);
        const Vec3 n = ok(dir) && length(dir) > 1e-4f ? normalized(dir) : Vec3{0, 0, -1};
        uint32_t out = 0;
        for (auto& f : spots) {
            if (!f.live)
                continue;
            const Vec3 to = f.at - from;
            const float dist = length(to);
            if (!(dist <= sh.reach))
                continue;
            if (sh.cosHalf > -.99f && dist > .3f && dot(to / dist, n) < sh.cosHalf)
                continue;
            f.heat -= sh.burst > 0 ? sh.burst : sh.perSecond * dt;
            if (f.heat <= 0) {
                f.live = false;
                ++out;
                ++putOut;
            }
        }
        return out;
    }
};
// THOR's hammer and DAREDEVIL's batons spun fast: the hand's velocity turning at spinSpeed or more;
// spinTurns of a full turn within about spinWindow seconds is spinning. THOR's only below the head
// (hammerRaised is the storm): `allowed` false stops it.
inline constexpr float spinSpeed = 2.f, spinTurns = 1.5f, spinWindow = 1.f;
struct SpinGesture {
    Vec3 lastDir{};
    bool have{};
    float turned{}; // radians, decaying
    bool step(float dt, Vec3 velocity, bool allowed) {
        dt = clampDt(dt);
        turned = std::max(0.f, turned - dt * (2 * halfTurn * spinTurns / spinWindow) * .5f);
        const float sp = ok(velocity) ? length(velocity) : 0.f;
        if (!allowed || sp < spinSpeed) {
            have = false;
            if (!allowed)
                turned = 0;
            return turned >= 2 * halfTurn * spinTurns && allowed;
        }
        const Vec3 d = velocity / sp;
        if (have)
            turned = std::min(turned + std::acos(std::clamp(dot(d, lastDir), -1.f, 1.f)), 4 * halfTurn * spinTurns);
        lastDir = d;
        have = true;
        return turned >= 2 * halfTurn * spinTurns;
    }
};
// IRON MAN: a grip held with the open palm (the hand's aim) pointed ahead, for palmHold seconds.
inline constexpr float palmHold = .2f, palmAhead = .7f;
struct PalmSpray {
    float held{};
    bool step(float dt, float grip, Vec3 aim, Vec3 look) {
        dt = clampDt(dt);
        const bool on = std::isfinite(grip) && grip >= .6f && ok(aim) && ok(look) && length(aim) > 1e-3f &&
                        length(look) > 1e-3f && dot(normalized(aim), normalized(look)) >= palmAhead;
        held = on ? held + dt : 0.f;
        return held >= palmHold;
    }
};
// GREEN LANTERN: both grips held, the hands together (hoseGap) out in front (hoseAhead past the head).
inline constexpr float hoseGap = .22f, hoseAhead = .25f, hoseHold = .2f;
struct HoseGesture {
    float held{};
    bool step(float dt, float gripL, float gripR, Vec3 head, Vec3 look, Vec3 l, Vec3 r) {
        dt = clampDt(dt);
        bool on = std::isfinite(gripL) && std::isfinite(gripR) && gripL >= .6f && gripR >= .6f && ok(head) && ok(look) &&
                  ok(l) && ok(r) && length(look) > 1e-3f && length(l - r) <= hoseGap;
        if (on) {
            const Vec3 mid = (l + r) * .5f;
            on = dot(mid - head, normalized(look)) >= hoseAhead;
        }
        held = on ? held + dt : 0.f;
        return held >= hoseHold;
    }
};
// HULK: the hands slammed together - closing at clapSpeed or more and meeting within clapGap.
inline constexpr float clapSpeed = 2.5f, clapGap = .16f, clapRearm = .8f;
struct ClapGesture {
    float lastGap = -1.f, rearm{};
    bool step(float dt, Vec3 l, Vec3 r) {
        dt = clampDt(dt);
        rearm = std::max(0.f, rearm - dt);
        if (!ok(l) || !ok(r) || dt <= 0) {
            lastGap = -1.f;
            return false;
        }
        const float gap = length(l - r);
        const float closing = lastGap >= 0 ? (lastGap - gap) / dt : 0.f;
        lastGap = gap;
        if (rearm <= 0 && gap <= clapGap && closing >= clapSpeed) {
            rearm = clapRearm;
            return true;
        }
        return false;
    }
};
// CAP's SHIELD SLAM (the user, 8:31): the shield arm driven down fast (slamSpeed) to low
// (slamBelow under the head) - a shockwave that puts out fires near and knocks back every enemy in
// slamRadius (shieldSlamHit: some damage, a big knock), through sm2_damage blows.
inline constexpr float slamSpeed = 3.f, slamBelow = 1.f, slamRearm = 1.2f, slamRadius = 4.f;
inline sm2_damage::Blow shieldSlamHit() { return {18.f, 5, 460.f}; }
struct ShieldSlam {
    float rearm{};
    bool step(float dt, bool shieldOn, Vec3 head, Vec3 hand, Vec3 velocity) {
        dt = clampDt(dt);
        rearm = std::max(0.f, rearm - dt);
        if (!shieldOn || rearm > 0 || !ok(head) || !ok(hand) || !ok(velocity))
            return false;
        const float sp = length(velocity);
        if (velocity.y > -slamSpeed || -velocity.y < .7f * sp || head.y - hand.y < slamBelow)
            return false;
        rearm = slamRearm;
        return true;
    }
};
// THE PUNISHER's TACTICAL GRAPPLING HOOK (the user, 8:35): real gear, slower than BATMAN's. A grip
// press with the hand on the chest rig (chestZone) draws the launcher; a fast push ahead along the
// look (hookPush) fires it (a gesture, no trigger: the triggers stay the guns'); the line takes
// hookWindup seconds to bite before the pull (the slower part) - the grapple itself is the game's
// zip (askGrappleOnly). Letting the grip go stows it.
inline constexpr float chestBelow = .2f, chestLow = .5f, chestSide = .2f, chestNear = .0f, chestFar = .35f;
inline bool atChest(holster::BodyPoint p) {
    return std::isfinite(p.below) && std::isfinite(p.side) && std::isfinite(p.ahead) && p.below >= chestBelow &&
           p.below <= chestLow && std::abs(p.side) <= chestSide && p.ahead >= chestNear && p.ahead <= chestFar;
}
inline constexpr float hookPush = 2.2f, hookWindup = .6f, hookReach = 35.f, hookRearm = 1.5f;
struct HookLauncher {
    enum State : uint32_t { stowed = 0, held, winding };
    uint32_t state{stowed};
    float timer{}, rearm{};
    bool down{};
    uint64_t draws{}, fires{};
    // Returns true the frame the line bites (ask the grapple then).
    bool step(float dt, bool play, float grip, bool atChestNow, Vec3 velocity, Vec3 look) {
        dt = clampDt(dt);
        rearm = std::max(0.f, rearm - dt);
        const float g = std::isfinite(grip) ? grip : 0.f;
        const bool was = down;
        down = g >= holster::pressed || (was && g > holster::released);
        if (!play) {
            state = stowed;
            return false;
        }
        switch (state) {
        case stowed:
            if (down && !was && atChestNow) {
                state = held;
                ++draws;
            }
            return false;
        case held:
            if (!down) {
                state = stowed;
                return false;
            }
            if (rearm <= 0 && ok(velocity) && ok(look) && length(look) > 1e-3f &&
                dot(velocity, normalized(look)) >= hookPush) {
                state = winding;
                timer = hookWindup;
                ++fires;
            }
            return false;
        default: // winding
            timer -= dt;
            if (timer > 0)
                return false;
            state = down ? held : stowed;
            rearm = hookRearm;
            return true;
        }
    }
};

// ---------------------------------------------------------------------------------------------
// GREEN LANTERN.
inline constexpr float chargeMost = 100.f;
inline constexpr float beamDrain = 14.f /* per s */, bubbleDrain = 11.f /* per s */, fistCost = 7.f, chainCost = 12.f;
inline constexpr float rechargeRate = 9.f /* per s */, rechargeDelay = 1.5f, emptyUntil = 25.f;
struct Charge {
    float level = chargeMost;
    float idle{};
    bool empty{}; // ran dry: no power until it is back to emptyUntil
    bool spend(float amount, bool meterOn) {
        if (!meterOn)
            return true;
        if (empty || !(amount >= 0) || level < amount)
            return false;
        level -= amount;
        idle = 0;
        if (level <= .01f) {
            level = 0;
            empty = true;
        }
        return true;
    }
    bool can(float amount, bool meterOn) const { return !meterOn || (!empty && level >= amount); }
    void step(float dt, bool used) {
        dt = clampDt(dt);
        if (used) {
            idle = 0;
            return;
        }
        idle += dt;
        if (idle >= rechargeDelay)
            level = std::min(chargeMost, level + rechargeRate * dt);
        if (empty && level >= emptyUntil)
            empty = false;
    }
};
inline sm2_damage::Blow constructFist() { return {2.5f * hero_modes::punchUnitHp, 5, 480.f}; }
inline sm2_damage::Blow ringBeamTick() { return {14.f, 5, 30.f}; }
inline sm2_damage::Blow chainGrab() { return {10.f, 5, 220.f}; }
inline constexpr float beamRange = 40.f, beamRadius = .45f, beamTickEvery = .12f;
inline constexpr float chainRange = 18.f, chainSeconds = .9f;
// RING FLIGHT TAKE-OFF (no trigger): both arms fully straight up overhead, each hand at least
// takeoffAbove over the eyes and within takeoffSide of the head across, held takeoffHold seconds.
inline constexpr float takeoffAbove = .30f, takeoffSide = .45f, takeoffHold = .25f;
inline bool armsStraightUp(Vec3 head, Vec3 l, Vec3 r) {
    if (!ok(head) || !ok(l) || !ok(r))
        return false;
    const auto up = [&](Vec3 h) {
        const Vec3 d = h - head;
        return d.y >= takeoffAbove && std::sqrt(d.x * d.x + d.z * d.z) <= takeoffSide;
    };
    return up(l) && up(r);
}
// What FLY MODE's poses (extras385::fly::Pose, 0 hover 1 rise 2 forward 3 brake) may do for GREEN
// LANTERN: on the ground only the held both-arms-up gesture takes off (as RISE at full lift); every
// other pose hovers (stays on the ground). In the air the poses steer as for SUPERMAN (both hands
// forward fly the way they point, one hand up rises, a T-pose brakes) - his triggers never thrust.
struct RingFlight {
    float held{};
    bool taking{};
    // -> the pose to pass on; `lift` is set to the RISE lift to pass on.
    uint32_t gate(float dt, bool flying, uint32_t pose, float poseLift, Vec3 head, Vec3 l, Vec3 r, float& lift) {
        dt = clampDt(dt);
        lift = poseLift;
        if (flying) {
            held = 0;
            taking = false;
            return pose;
        }
        held = armsStraightUp(head, l, r) ? held + dt : 0.f;
        taking = held >= takeoffHold;
        if (taking) {
            lift = 1.f;
            return 1u; // RISE
        }
        lift = 0;
        return 0u; // HOVER: nothing else takes off
    }
};
// CONSTRUCT FIST (no trigger): the right grip held and that hand punching fast (fistSpeed m/s, mostly
// along the look), once per fistRearm seconds; the fist flies fistReach metres ahead.
inline constexpr float fistSpeed = 3.0f, fistRearm = .6f, fistReach = 3.5f;
struct FistGesture {
    float since{fistRearm};
    bool step(float dt, float grip, Vec3 velocity, Vec3 look) {
        since = std::min(since + clampDt(dt), 10.f);
        if (!std::isfinite(grip) || grip < .6f || !ok(velocity) || !ok(look) || length(look) < 1e-3f)
            return false;
        const float sp = length(velocity);
        if (since < fistRearm || sp < fistSpeed || dot(velocity, normalized(look)) < .7f * sp)
            return false;
        since = 0;
        return true;
    }
};
// SHIELD BUBBLE (no trigger): both grips held and the hands pulled apart - the gap grows by at least
// bubbleSpread within bubbleWindow seconds and ends at least bubbleApart; it stays up while both
// grips stay held (and the ring has charge).
inline constexpr float bubbleSpread = .25f, bubbleWindow = .6f, bubbleApart = .55f;
struct BubbleGesture {
    bool up{};
    float startGap{-1}, age{};
    bool step(float dt, float gripL, float gripR, Vec3 l, Vec3 r) {
        dt = clampDt(dt);
        const bool both = std::isfinite(gripL) && std::isfinite(gripR) && gripL >= .6f && gripR >= .6f && ok(l) && ok(r);
        if (!both) {
            up = false;
            startGap = -1;
            age = 0;
            return false;
        }
        const float gap = length(l - r);
        if (up)
            return true;
        if (startGap < 0 || age > bubbleWindow || gap < startGap) {
            startGap = gap; // the gap's low point in the window
            age = 0;
        }
        age += dt;
        if (gap - startGap >= bubbleSpread && gap >= bubbleApart)
            up = true;
        return up;
    }
};
// The old pose test (both grips held, the hands apart in front of the chest), kept for reference.
inline bool bubblePose(const Pose& head, Vec3 l, Vec3 r, float squeezeL, float squeezeR) {
    if (!ok(head.position) || !ok(l) || !ok(r) || squeezeL < .6f || squeezeR < .6f)
        return false;
    const Vec3 f = head.orientation.rotate(Vec3{0, 0, -1});
    const float apart = length(l - r);
    const Vec3 mid = (l + r) * .5f - head.position;
    return apart >= .30f && apart <= 1.1f && dot(mid, f) >= .15f && mid.y > -.75f && mid.y < .2f;
}
inline constexpr float bubbleRadius = 1.6f;

// ---------------------------------------------------------------------------------------------
// THOR.
inline sm2_damage::Blow hammerHit() { return {45.f, 5, 260.f}; } // the hammer and its electrocution
inline sm2_damage::Blow stormHit() { return {60.f, 5, 380.f}; }  // the radial strike
inline constexpr float stormRadius = 9.f, stormHold = 1.0f, stormCooldown = 7.f; // the user, 8:18: about 1 s
inline constexpr uint32_t stormStrikes = 3;
inline constexpr float stormEvery = .35f;
// Raised (a gesture, no trigger): the hammer hand straight up overhead (at least .25 m over the eyes,
// within .45 m across) and the hammer pointing up.
inline bool hammerRaised(Vec3 head, Vec3 hand, Vec3 aim) {
    if (!ok(head) || !ok(hand) || !ok(aim) || length(aim) < 1e-3f)
        return false;
    const Vec3 d = hand - head;
    return d.y >= .25f && std::sqrt(d.x * d.x + d.z * d.z) <= .45f && normalized(aim).y >= .85f;
}
// THOR STORM's hold: the raise held stormHold seconds, then the cooldown.
struct StormHold {
    float held{}, cool{};
    bool step(float dt, bool raised) {
        dt = clampDt(dt);
        cool = std::max(0.f, cool - dt);
        held = raised && cool <= 0 ? held + dt : 0.f;
        if (held >= stormHold) {
            held = 0;
            cool = stormCooldown;
            return true;
        }
        return false;
    }
};
// THOR MELEE (the user, 8:22; gestures, no triggers): the hammer hand's swing, read from HandSpeed.
//  - DOWN: a swing down (at least swingSpeed m/s, mostly downward) that lands on an enemy calls one
//    lightning bolt onto him (thorBolt, extra damage on top of the punch).
//  - UP: a swing up that lands launches him (thorLaunch, the uppercut's big upward knock; that it
//    goes up is the game's - CANDIDATE, like UPPERCUT).
//  - SLAM: a swing down while locked onto an enemy in the air (his feet at least airAbove over the
//    player's) strikes him with lightning even untouched and slams him down (thorSlam, bonus slam
//    damage; the downward knock is CANDIDATE - the blow is CONFIRMED sm2_damage).
inline constexpr float swingSpeed = 2.6f, swingRearm = .45f, airAbove = 1.2f;
inline sm2_damage::Blow thorBolt() { return {35.f, 5, 120.f}; }
inline sm2_damage::Blow thorLaunch() { return {12.f, 5, 560.f}; }
inline sm2_damage::Blow thorSlam() { return {55.f, 5, 480.f}; }
enum class Swing : uint32_t { none = 0, down = 1, up = 2 };
struct HammerSwing {
    float since{swingRearm};
    Swing step(float dt, bool hammerInHand, Vec3 velocity) {
        since = std::min(since + clampDt(dt), 10.f);
        if (!hammerInHand || !ok(velocity) || since < swingRearm)
            return Swing::none;
        const float sp = length(velocity);
        if (sp < swingSpeed)
            return Swing::none;
        Swing out = Swing::none;
        if (velocity.y <= -.6f * sp)
            out = Swing::down;
        else if (velocity.y >= .6f * sp)
            out = Swing::up;
        if (out != Swing::none)
            since = 0;
        return out;
    }
};
enum class ThorAct : uint32_t { none = 0, bolt = 1, launch = 2, slam = 3 };
// The swing's outcome: `landed` is a hammer hit on an enemy this swing; `lockedAirborne` the locked
// enemy is in the air (inAir). A down swing on an airborne lock slams him even when it also landed.
inline ThorAct thorAct(Swing s, bool landed, bool lockedAirborne) {
    if (s == Swing::down)
        return lockedAirborne ? ThorAct::slam : landed ? ThorAct::bolt : ThorAct::none;
    if (s == Swing::up)
        return landed ? ThorAct::launch : ThorAct::none;
    return ThorAct::none;
}
inline bool inAir(float enemyFeetY, float playerFeetY) {
    return std::isfinite(enemyFeetY) && std::isfinite(playerFeetY) && enemyFeetY - playerFeetY >= airAbove;
}
// PUNISHER SUIT (combat38::punisherSuit): the kit's name and credit for the roster popup.
inline const char* punisherSuitLine(uint32_t v) {
    return v == 1 ? "SUIT: WARZONE BY SPYBG (CC BY)" : "SUIT: VEST BY FLOH (CC BY-NC)";
}

// ---------------------------------------------------------------------------------------------
// ROSTER bios (the wrist's HERO page tiles). Plain-language, no credits other than ours.
struct Bio {
    const char* name;
    const char* tag;      // the tile's short line
    const char* bio;      // the popup's text
    float color[3];       // the tile's colour
    const char* identity; // the popup's "REAL NAME"
    const char* abilities; // the popup's powers, one line
};
inline constexpr uint32_t rosterCount = 11;
inline const Bio& bio(uint32_t hero) {
    static constexpr Bio bios[rosterCount] = {
        {"SPIDER-MAN", "WEBS, SWINGS, WALLS", "Swing on webs, crawl any wall, fight with the game's moves.", {.85f, .1f, .12f},
         "Peter Parker", "Webs, wall crawling, spider-sense"},
        {"IRON MAN", "REPULSORS, BEAM", "Palm repulsors on the triggers, the chest beam, a helmet HUD.", {.8f, .55f, .1f},
         "Tony Stark", "Repulsors, unibeam, flight, palm foam"},
        {"DAREDEVIL", "BATONS, SONAR", "A baton in each hand: throw, grapple, cross them to parry.", {.6f, .05f, .08f},
         "Matt Murdock", "Batons, radar sense, grapple, parry"},
        {"HULK", "SMASH", "One-punch knockouts and a giant charged leap.", {.25f, .7f, .2f}, "Bruce Banner",
         "Strength, rage, big jumps, thunder clap"},
        {"BATMAN", "BATARANGS, GRAPPLE", "Batarangs, the grapple gun, the cape glide and smoke pellets.", {.25f, .25f, .3f},
         "Bruce Wayne", "Batarangs, grapple pull, glide, smoke"},
        {"WOLVERINE", "CLAWS, HEALING", "Claws on the triggers and a healing factor.", {.9f, .8f, .1f}, "Logan",
         "Claws, healing factor, claw climbing"},
        {"SUPERMAN", "FLIGHT, HEAT VISION", "Flight, heat vision, cold breath and super strength.", {.15f, .3f, .9f},
         "Clark Kent", "Flight, heat vision, cold breath, invulnerable"},
        {"PUNISHER", "GUNS, RAGE", "Pistols on the hips, a sawed-off on the back, and rage.", {.15f, .15f, .15f},
         "Frank Castle", "Pistols, shotgun, rage, grappling hook"},
        {"CAPTAIN AMERICA", "SHIELD", "Throw the shield, block with it, bash with it.", {.2f, .35f, .85f}, "Steve Rogers",
         "Shield throw, block, shield slam, leaps"},
        {"THOR", "HAMMER, LIGHTNING", "Draw the hammer from your back, throw it, raise it for lightning.", {.55f, .65f, .95f},
         "Thor Odinson", "Hammer, lightning, storm, flight, invulnerable"},
        {"GREEN LANTERN", "RING CONSTRUCTS", "Arms up to fly, ring beam, chain, a construct fist and a bubble.", {.15f, .95f, .3f},
         "Hal Jordan", "Ring beam, chain, fist, bubble, flight"},
    };
    return bios[hero < rosterCount ? hero : 0];
}
// THE ROSTER (the user, 8:39): a grid of tiles (rosterColumns across), each the hero's portrait
// (rendered in Blender from the hero's own model: tools/roster/render_portraits.py; a tile without
// one shows the hero's colour and name), a tap opens the popup (bio, real name, abilities, CONFIRM),
// CONFIRM switches to that hero (the hero toggles), BACK closes it.
inline constexpr uint32_t rosterColumns = 4;
struct RosterUi {
    enum State : uint32_t { grid = 0, popup };
    uint32_t state{grid}, picked{};
    // A tile tapped (0..rosterCount-1): its popup.
    void tap(uint32_t tile) {
        if (tile < rosterCount) {
            picked = tile;
            state = popup;
        }
    }
    void back() { state = grid; }
    // CONFIRM: the hero to switch to (-1 nothing open).
    int confirm() {
        if (state != popup)
            return -1;
        state = grid;
        return static_cast<int>(picked);
    }
    static void tileAt(uint32_t tile, uint32_t& column, uint32_t& row) {
        column = tile % rosterColumns;
        row = tile / rosterColumns;
    }
};

// ---------------------------------------------------------------------------------------------
// GRAB AND SHOVE (the user, 8:40). NPCs no longer vanish near you: that is NPC NEAR FADE (Update
// 37.2, npc_fade.hpp, default OFF = they stay). SHOVE: both open hands (no grips) pushed fast ahead
// at a close enemy knocks him back (sm2_damage, CONFIRMED). GRAB to move someone and civilians
// stumbling need a CONFIRMED NPC position / reaction path, which there is not (npc_fade.hpp: "no game
// path that moves a street NPC was confirmed"): CANDIDATE, not in this build.
inline constexpr float shoveSpeed = 2.f, shoveReach = 1.6f, shoveRearm = .7f, shoveGapLow = .15f, shoveGapHigh = .7f;
inline sm2_damage::Blow shoveBlow() { return {3.f, 5, 380.f}; }
struct ShoveGesture {
    float rearm{};
    bool step(float dt, float gripL, float gripR, Vec3 l, Vec3 r, Vec3 vl, Vec3 vr, Vec3 look) {
        dt = clampDt(dt);
        rearm = std::max(0.f, rearm - dt);
        if (rearm > 0 || !ok(l) || !ok(r) || !ok(vl) || !ok(vr) || !ok(look) || length(look) < 1e-3f)
            return false;
        if (!(gripL < .3f && gripR < .3f))
            return false;
        const float gap = length(l - r);
        const Vec3 n = normalized(look);
        if (gap < shoveGapLow || gap > shoveGapHigh || dot(vl, n) < shoveSpeed || dot(vr, n) < shoveSpeed)
            return false;
        rearm = shoveRearm;
        return true;
    }
};

// ---------------------------------------------------------------------------------------------
// ENEMY FACTIONS (the user, 8:40): by default the enemies match the hero (AUTO): DAREDEVIL, BATMAN
// and WOLVERINE THE HAND; CAP, IRON MAN and HULK HYDRA; SPIDER-MAN (Kraven's hunters) and the rest
// VANILLA. BATMAN (9:28): the JOKER GANG (Joker's henchmen). FACTION overrides it: AUTO, VANILLA,
// THE HAND, HYDRA, JOKER GANG. The JOKER GANG has no model picked yet (factionModelReady): it is
// chosen and shown, and worn as VANILLA (factionWorn), never a crash or a missing model. Dressing the game's enemies as
// another faction needs a CONFIRMED gear / spawn swap, which there is not: the hook is CANDIDATE, so
// the faction is chosen and shown (the status page) and the game's enemies stay vanilla until then.
inline constexpr bool factionHookConfirmed = false;
enum Faction : uint32_t { vanilla = 0, theHand, hydra, jokerGang, factions };
inline constexpr const char* factionNames[factions] = {"VANILLA", "THE HAND", "HYDRA", "JOKER GANG"};
// A faction's crowd models are in the kit (tools/factions): the JOKER GANG's are still being picked.
inline constexpr bool factionModelReady[factions] = {true, true, true, false};
// What the enemies actually wear: a faction without models (or an odd number) falls back to VANILLA.
inline Faction factionWorn(uint32_t f) {
    return f < factions && factionModelReady[f] && factionHookConfirmed ? static_cast<Faction>(f) : vanilla;
}
inline Faction autoFaction(uint32_t hero) {
    switch (hero) {
    case hero_modes::batman: return jokerGang;
    case hero_modes::daredevil:
    case hero_modes::wolverine: return theHand;
    case hero_modes::captainAmerica:
    case hero_modes::ironMan:
    case hero_modes::hulk: return hydra;
    default: return vanilla;
    }
}
// The setting: 0 AUTO, 1 VANILLA, 2 THE HAND, 3 HYDRA, 4 JOKER GANG.
inline Faction factionFor(uint32_t hero, uint32_t setting) {
    switch (setting) {
    case 1: return vanilla;
    case 2: return theHand;
    case 3: return hydra;
    case 4: return jokerGang;
    default: return autoFaction(hero);
    }
}

// ---------------------------------------------------------------------------------------------
// SUITS (the user, 8:43): each hero can have several outfits - his installed suit models, each with
// its artist's credit. Generic: the built-in list (builtInSuits) plus any dropped in later as lines
// of suits.txt next to the mod ("hero|NAME|artist|licence|rep|folder", hero 0-10; '#' a comment),
// so a new suit needs no new code. HERO SUIT (the setting, and SUIT in the roster popup next to
// CONFIRM) steps through the current hero's UNLOCKED suits.
// UNLOCKS (the user, 8:43): suit 0 of each hero is his default and always unlocked; the others unlock
// when THAT hero's reputation reaches the suit's rep (tiers). Once unlocked a suit stays unlocked
// (saved: "suits.<hero>=<mask>" lines, only ever OR-ed). UNLOCK ALL SUITS shows them all.
inline constexpr uint32_t suitsMost = 8, suitsListMost = 64;
inline constexpr int32_t suitTier[4] = {0, repTuning.suitTier[0], repTuning.suitTier[1], repTuning.suitTier[2]};
struct Suit {
    uint32_t hero{hero_modes::heroes};
    char name[32]{}, artist[48]{}, licence[16]{}, folder[48]{};
    int32_t rep{}; // the hero's reputation that unlocks it (0: from the start)
    bool final{};  // the hero's FINAL suit (the user, 8:59: unlocks at that hero's max reputation)
    bool soon{};   // COMING SOON: its model is not converted yet - listed, never picked, never loaded
};
inline Suit makeSuit(uint32_t hero, const char* name, const char* artist, const char* licence, int32_t rep, const char* folder,
                     bool final = false, bool soon = false) {
    Suit x;
    x.final = final;
    x.soon = soon;
    x.hero = hero;
    std::snprintf(x.name, sizeof x.name, "%s", name ? name : "");
    std::snprintf(x.artist, sizeof x.artist, "%s", artist ? artist : "");
    std::snprintf(x.licence, sizeof x.licence, "%s", licence ? licence : "");
    std::snprintf(x.folder, sizeof x.folder, "%s", folder ? folder : "");
    x.rep = std::clamp(rep, 0, repMost);
    return x;
}
class SuitCatalog {
  public:
    Suit list[suitsListMost]{};
    uint32_t count{};
    uint32_t unlocked[hero_modes::heroes]{}; // bit i: that hero's suit i (bit 0 always)
    uint32_t chosen[hero_modes::heroes]{};
    bool dirty{};
    SuitCatalog() { builtIn(); }
    bool add(const Suit& x) {
        if (x.hero >= hero_modes::heroes || !x.name[0])
            return false;
        for (uint32_t i = 0; i < count; ++i)
            if (list[i].hero == x.hero && std::strcmp(list[i].name, x.name) == 0) {
                if (!list[i].soon || x.soon)
                    return false; // already there
                const bool final = list[i].final;
                list[i] = x; // a converted model for a COMING SOON pick: it keeps its FINAL rule
                list[i].final = final;
                list[i].rep = final ? repMost : x.rep;
                return true;
            }
        if (count >= suitsListMost || suits(x.hero) >= suitsMost)
            return false;
        list[count++] = x;
        return true;
    }
    // Suit 0 of each hero: the game's own (or whatever suit another mod applies) - always there. Then
    // the user's picks (8:59, tools/alt_suits/alt_suits.py makes these lines; the numbers on the sheets
    // are in the folder names). Every pick is COMING SOON in this build: Sketchfab refused the
    // downloads (401), so none is fetched, rigged or decimated yet. A suits.txt line with the same
    // hero and NAME (a converted model dropped in) replaces the COMING SOON entry.
    void builtIn() {
        count = 0;
        const char* game = "THE GAME'S OWN";
        for (uint32_t h = 0; h < hero_modes::heroes; ++h)
            if (h != hero_modes::captainAmerica)
                add(makeSuit(h, "DEFAULT", game, "", 0, ""));
        add(makeSuit(hero_modes::captainAmerica, "CAPTAIN AMERICA", "iqbalmulyawan", "CC BY", 0, "CapSuit"));
        // The PUNISHER's (the user, 8:59: 1 and 2, no final) and WARZONE (8:35), all from the start.
        add(makeSuit(hero_modes::batman, "BEYOND", "Light.k", "CC BY", repMost, "Suit_batman_1", true, true));
        add(makeSuit(hero_modes::batman, "1989", "Homeless_Models (hl_models)", "CC BY", 0, "Suit_batman_2", false, true));
        add(makeSuit(hero_modes::batman, "YEAR ONE", "Tigerar1 (allanromanreyes)", "SF STANDARD", 0, "Suit_batman_3", false, true));
        add(makeSuit(hero_modes::batman, "ORIGINS", "Light.k", "CC BY", 0, "Suit_batman_5", false, true));
        add(makeSuit(hero_modes::ironMan, "MARK 85", "9A Films / Nihar Arora", "CC BY", 0, "Suit_ironMan_1", false, true));
        add(makeSuit(hero_modes::ironMan, "MARK 1", "Nathang30", "CC BY-NC-SA", repMost, "Suit_ironMan_2", true, true));
        add(makeSuit(hero_modes::ironMan, "MARK 7", "benjamin.borromeo", "CC BY", 0, "Suit_ironMan_3", false, true));
        add(makeSuit(hero_modes::ironMan, "MARK 6", "Mittergen", "CC BY", 0, "Suit_ironMan_4", false, true));
        add(makeSuit(hero_modes::captainAmerica, "WINTER SOLDIER", "Zeztz777", "CC BY", repMost, "Suit_captainAmerica_1", true, true));
        add(makeSuit(hero_modes::captainAmerica, "WW2", "Marco.41", "SF STANDARD", 0, "Suit_captainAmerica_2", false, true));
        add(makeSuit(hero_modes::captainAmerica, "CLASSIC", "alexseagle2004", "CC BY", 0, "Suit_captainAmerica_4", false, true));
        add(makeSuit(hero_modes::daredevil, "BORN AGAIN S2", "YE YE", "CC BY", 0, "Suit_daredevil_1", false, true));
        add(makeSuit(hero_modes::daredevil, "NETFLIX", "VLTXD", "CC BY", 0, "Suit_daredevil_2", false, true));
        add(makeSuit(hero_modes::daredevil, "YELLOW AND RED", "AtoMichael Editz", "CC BY", repMost, "Suit_daredevil_3", true, true));
        add(makeSuit(hero_modes::hulk, "GREY", "3D MODELS", "CC BY", 0, "Suit_hulk_1", false, true));
        add(makeSuit(hero_modes::hulk, "PROFESSOR", "Shivam.Yadav", "CC BY", 0, "Suit_hulk_2", false, true));
        add(makeSuit(hero_modes::hulk, "GLADIATOR", "aredjune", "CC BY", repMost, "Suit_hulk_3", true, true));
        add(makeSuit(hero_modes::hulk, "MAESTRO", "YE YE", "CC BY", 0, "Suit_hulk_4", false, true));
        add(makeSuit(hero_modes::wolverine, "CLASSIC", "stokissout", "CC BY", 0, "Suit_wolverine_1", false, true));
        add(makeSuit(hero_modes::wolverine, "LEGENDARY", "LombaxLover2K", "CC BY", 0, "Suit_wolverine_2", false, true));
        add(makeSuit(hero_modes::wolverine, "JACKET AND JEANS", "Light.k", "CC BY", 0, "Suit_wolverine_3", false, true));
        add(makeSuit(hero_modes::wolverine, "WEAPON X", "AssetGoat", "CC BY", repMost, "Suit_wolverine_4", true, true));
        add(makeSuit(hero_modes::wolverine, "D AND W", "BlendAtlas", "UNKNOWN", 0, "Suit_wolverine_5", false, true));
        add(makeSuit(hero_modes::punisher, "VEST", "floh", "CC BY-NC", 0, "Suit_punisher_1", false, true));
        add(makeSuit(hero_modes::punisher, "CLASSIC", "floh", "CC BY-NC", 0, "Suit_punisher_2", false, true));
        add(makeSuit(hero_modes::thor, "RIGGED", "3D MODELS", "CC BY", 0, "Suit_thor_1", false, true));
        add(makeSuit(hero_modes::thor, "CLASSIC", "3D MODELS", "CC BY", repMost, "Suit_thor_2", true, true));
        add(makeSuit(hero_modes::thor, "RAGNAROK", "YE YE", "CC BY", 0, "Suit_thor_3", false, true));
        add(makeSuit(hero_modes::greenLantern, "FILM", "DavidA3D", "CC BY-NC", 0, "Suit_greenLantern_1", false, true));
        add(makeSuit(hero_modes::greenLantern, "INJUSTICE 2", "Neut2000", "CC BY", 0, "Suit_greenLantern_2", false, true));
        add(makeSuit(hero_modes::greenLantern, "RENEGADE", "3D MODELS", "CC BY", repMost, "Suit_greenLantern_3", true, true));
        add(makeSuit(hero_modes::greenLantern, "RIGGED", "3D MODELS", "CC BY", 0, "Suit_greenLantern_4", false, true));
        add(makeSuit(hero_modes::superman, "SSKTJL", "20062020year", "CC BY", 0, "Suit_superman_1", false, true));
        add(makeSuit(hero_modes::superman, "CLASSIC", "Allen Tseng", "CC BY", 0, "Suit_superman_2", false, true));
        add(makeSuit(hero_modes::superman, "CAVILL", "DavidA3D", "CC BY-NC", 0, "Suit_superman_3", false, true));
        add(makeSuit(hero_modes::superman, "DC", "Chechorams16", "CC BY", repMost, "Suit_superman_4", true, true));
        add(makeSuit(hero_modes::punisher, "WARZONE", "spybg (Valentin Yovchev)", "CC BY", 0, "PunisherWarzone", false, true));
    }
    uint32_t suits(uint32_t hero) const {
        uint32_t n = 0;
        for (uint32_t i = 0; i < count; ++i)
            n += list[i].hero == hero;
        return n;
    }
    // The hero's i-th suit (nullptr none).
    const Suit* suit(uint32_t hero, uint32_t i) const {
        for (uint32_t k = 0; k < count; ++k)
            if (list[k].hero == hero && i-- == 0)
                return &list[k];
        return nullptr;
    }
    // Reputation reached: unlock (never lock again).
    void unlockFor(uint32_t hero, int32_t rep) {
        if (hero >= hero_modes::heroes)
            return;
        uint32_t m = unlocked[hero] | 1u;
        for (uint32_t i = 0; i < suits(hero); ++i)
            if (const Suit* x = suit(hero, i); x && rep >= x->rep)
                m |= 1u << i;
        dirty = dirty || m != unlocked[hero];
        unlocked[hero] = m;
    }
    // UNLOCK ALL SUITS (`all`) opens the finals too; a COMING SOON suit is never unlocked (no model).
    bool isUnlocked(uint32_t hero, uint32_t i, bool all) const {
        const Suit* x = suit(hero, i);
        return hero < hero_modes::heroes && x && !x->soon && (all || i == 0 || x->rep <= 0 || (unlocked[hero] >> i & 1u));
    }
    bool comingSoon(uint32_t hero, uint32_t i) const {
        const Suit* x = suit(hero, i);
        return x && x->soon;
    }
    // USE MY OWN OR GAME SUIT (the user, 8:59): the fallback - suit 0 (the game's, or another mod's).
    uint32_t inUse(uint32_t hero, bool all, bool ownSuit) const { return ownSuit ? 0u : current(hero, all); }
    // HERO SUIT / the popup's SUIT: the next unlocked suit of the hero (wrapping).
    uint32_t next(uint32_t hero, bool all) {
        if (hero >= hero_modes::heroes)
            return 0;
        const uint32_t n = std::max(1u, suits(hero));
        uint32_t c = chosen[hero] % n;
        for (uint32_t k = 1; k <= n; ++k)
            if (isUnlocked(hero, (c + k) % n, all)) {
                c = (c + k) % n;
                break;
            }
        dirty = dirty || c != chosen[hero];
        chosen[hero] = c;
        return c;
    }
    // The suit in use: the chosen one if unlocked, else the default.
    uint32_t current(uint32_t hero, bool all) const {
        if (hero >= hero_modes::heroes)
            return 0;
        return isUnlocked(hero, chosen[hero], all) ? chosen[hero] : 0u;
    }
    // "SUIT: WARZONE BY SPYBG (VALENTIN YOVCHEV) (CC BY)", or "LOCKED: REP 650" for a locked one.
    void line(uint32_t hero, uint32_t i, bool all, char* out, size_t n) const {
        const Suit* x = suit(hero, i);
        if (!x) {
            std::snprintf(out, n, "SUIT: DEFAULT");
            return;
        }
        if (x->soon)
            std::snprintf(out, n, "%s: COMING SOON", x->name);
        else if (!isUnlocked(hero, i, all) && x->final)
            std::snprintf(out, n, "%s LOCKED: MAX REP (FINAL)", x->name);
        else if (!isUnlocked(hero, i, all))
            std::snprintf(out, n, "%s LOCKED: REP %d", x->name, x->rep);
        else if (x->licence[0])
            std::snprintf(out, n, "SUIT: %s BY %s (%s)", x->name, x->artist, x->licence);
        else
            std::snprintf(out, n, "SUIT: %s (%s)", x->name, x->artist);
    }
    // suits.txt (dropped-in suits). Returns how many were added; bad lines are skipped.
    uint32_t parseList(const char* text) {
        uint32_t added = 0;
        for (const char* l = text; l && *l;) {
            const char* e = std::strchr(l, '\n');
            std::string line(l, e ? static_cast<size_t>(e - l) : std::strlen(l));
            l = e ? e + 1 : nullptr;
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            if (line.empty() || line[0] == '#')
                continue;
            std::string f[6];
            size_t k = 0, at = 0;
            for (; k < 6; ++k) {
                const size_t bar = line.find('|', at);
                f[k] = line.substr(at, bar == std::string::npos ? std::string::npos : bar - at);
                if (bar == std::string::npos)
                    break;
                at = bar + 1;
            }
            if (k < 5)
                continue;
            char* end = nullptr;
            const long h = std::strtol(f[0].c_str(), &end, 10);
            if (end == f[0].c_str() || h < 0 || h >= static_cast<long>(hero_modes::heroes))
                continue;
            const long rep = std::strtol(f[4].c_str(), nullptr, 10);
            added += add(makeSuit(static_cast<uint32_t>(h), f[1].c_str(), f[2].c_str(), f[3].c_str(),
                                  static_cast<int32_t>(std::clamp<long>(rep, 0, repMost)), f[5].c_str()));
        }
        return added;
    }
    // The save: "suits.<hero>=<mask>" and "suit.<hero>=<chosen>" lines.
    void save(char* out, size_t n) const {
        size_t at = 0;
        for (uint32_t h = 0; h < hero_modes::heroes && at < n; ++h) {
            const int w = std::snprintf(out + at, n - at, "suits.%u=%u\nsuit.%u=%u\n", h, unlocked[h] | 1u, h, chosen[h]);
            if (w < 0)
                break;
            at += static_cast<size_t>(w);
        }
    }
    // Read back: unlocks only ever added to (an old or broken save never locks one again).
    void load(const char* text) {
        for (uint32_t h = 0; text && h < hero_modes::heroes; ++h) {
            char key[24];
            std::snprintf(key, sizeof key, "suits.%u=", h);
            if (const char* v = std::strstr(text, key))
                unlocked[h] |= static_cast<uint32_t>(std::strtoul(v + std::strlen(key), nullptr, 10)) & ((1u << suitsMost) - 1u);
            std::snprintf(key, sizeof key, "suit.%u=", h);
            if (const char* v = std::strstr(text, key))
                chosen[h] = static_cast<uint32_t>(std::strtoul(v + std::strlen(key), nullptr, 10)) % suitsMost;
            unlocked[h] |= 1u;
        }
    }
};

} // namespace spidy::hero072
