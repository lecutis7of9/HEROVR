#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
//
// Version 0.0.4 HERO ABILITIES: power sets on whatever suit or model is on (the mod ships no other
// hero's model). SPIDER-MAN is the default (no ability toggle on). The hologram's toggles are
// "IRON MAN ABILITIES", "DAREDEVIL ABILITIES" and "HULK ABILITIES"; only one is on at a time
// (select: turning one on turns the others off). Version 0.0.6: all three are on the hologram
// (combat38's shooterType, daredevil and hulk, OFF/ON), each with real damage (sm2_damage).
//
// Every hero but SPIDER-MAN has NO web at all (webPaths): no web ball, no game web ball (RB), no
// swing (RT), no zip or point launch (LT + RT), no charged swing leap (RT + A), no web gadgets
// (RB held + a face button, the gadget wheel), no Sonic, no yank (LB + RB), no ROPE GRAB, no web
// glob. stripPad takes the game's web inputs out of the virtual pad as it is sent (the one
// choke point every game input goes through); a press a story prompt asks for is kept, so a
// prompt can never get stuck. FLY MODE is its own toggle and unaffected.
//
// DAREDEVIL's baton throw (Baton): a trigger press throws that hand's billy club at the aimed
// enemy (the shooters' strong aim assist picks him), it hits, ricochets and comes back to the
// hand; the hand is empty until it is caught (no throw while it is out: one throw per press).
// Damage: Version 0.0.6 sends each hit's blow on sm2_damage's path (CONFIRMED in rep0050).
// Version 0.0.5 BATON CHAIN (update with an enemy list): after a hit it chains to the nearest other
// enemy within batonChainRange of the one it hit, never one already hit on this throw, at most
// batonMaxChains times (batonMaxChains + 1 enemies hit); then it returns to the hand. With no
// enemy in range it returns at once; a miss still returns. Wired in game in 0.0.6.
// Pure; no game memory.
#include "spidy/math.hpp"
#include "spidy/sfx.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace spidy::hero_modes {

enum Hero : uint32_t { spiderMan = 0, ironMan, daredevil, hulk, heroes };
inline constexpr const char* heroNames[heroes] = {"SPIDER-MAN", "IRON MAN", "DAREDEVIL", "HULK"};
// Which heroes have a working hologram toggle in this build (Version 0.0.6: all of them).
inline constexpr bool shipped[heroes] = {true, true, true, true};
// Version 0.0.6: the hero from the three toggles (IRON MAN, DAREDEVIL, HULK); should two ever be on
// (a hand-edited live.txt), the first wins, so exactly one hero plays.
inline uint32_t fromToggles(bool ironMan_, bool daredevil_, bool hulk_) {
    return ironMan_ ? ironMan : daredevil_ ? daredevil : hulk_ ? hulk : spiderMan;
}

// The toggles: only one on. Turning `which` on makes it the hero; turning the current hero off
// goes back to SPIDER-MAN; turning off one that is not on changes nothing.
inline uint32_t select(uint32_t current, uint32_t which, bool on) {
    if (which == spiderMan || which >= heroes)
        return current < heroes ? current : spiderMan;
    if (on)
        return which;
    return current == which ? spiderMan : current;
}
inline bool toggleOn(uint32_t current, uint32_t which) { return current == which && which != spiderMan; }

// Every web path the mod or the game has. All true for SPIDER-MAN only.
struct WebPaths {
    bool webBall{}, gameWebBall{}, swing{}, zip{}, chargeLeap{}, gadgets{}, gadgetWheel{}, sonic{}, yank{},
        ropeGrab{}, glob{}, webRope{};
    bool any() const {
        return webBall || gameWebBall || swing || zip || chargeLeap || gadgets || gadgetWheel || sonic || yank ||
               ropeGrab || glob || webRope;
    }
};
inline WebPaths webPaths(uint32_t hero) {
    WebPaths w;
    if (hero != spiderMan)
        return w;
    w.webBall = w.gameWebBall = w.swing = w.zip = w.chargeLeap = w.gadgets = w.gadgetWheel = w.sonic = w.yank =
        w.ropeGrab = w.glob = w.webRope = true;
    return w;
}

// The game pad's web inputs (XINPUT bits, game_pad.hpp): RB (web shooter, held: gadgets; with LB:
// yank) and RT (swing; with LT: zip; with A: the charged leap).
inline constexpr uint16_t padRb = 0x200;
// Takes the web inputs out of a pad on its way to the game. askedButtons / askedRt: what a story
// prompt asks for this frame (kept). Returns how many web inputs were taken out.
inline uint32_t stripPad(uint32_t hero, uint16_t& buttons, uint8_t& rightTrigger, uint16_t askedButtons,
                         bool askedRt) {
    if (hero == spiderMan)
        return 0;
    uint32_t taken = 0;
    if ((buttons & padRb) && !(askedButtons & padRb)) {
        buttons = static_cast<uint16_t>(buttons & ~padRb);
        ++taken;
    }
    if (rightTrigger && !askedRt) {
        rightTrigger = 0;
        ++taken;
    }
    return taken;
}

// DAREDEVIL's baton, one per hand.
inline constexpr float batonSpeed = 22.f;      // m/s out
inline constexpr float batonBackSpeed = 18.f;  // m/s back (it chases the moving hand)
inline constexpr float batonReach = 25.f;      // m: past this with no enemy it turns back
inline constexpr float batonHitRadius = .45f;  // m
inline constexpr float batonCatch = .3f;       // m from the hand: caught
inline constexpr float batonRicochet = .25f;   // s bouncing off him before it heads home
inline constexpr float batonMaxOut = 4.f;      // s: never lost; back in the hand after this
inline constexpr float batonChainRange = 6.f;  // m from the enemy it hit to the next (0.0.5 BATON CHAIN)
inline constexpr uint32_t batonMaxChains = 3;  // chains per throw: 4 enemies at most
// One enemy the aim assist sees this frame (the chain's list): an id that stays his while he lives.
struct BatonEnemy {
    uint64_t id{};
    Vec3 chest{};
};
enum BatonState : uint32_t { inHand = 0, flying, ricochet, returning };
inline constexpr const char* batonStateNames[] = {"IN HAND", "FLYING", "RICOCHET", "RETURNING"};

struct Baton {
    uint32_t state{inHand};
    Vec3 position{}, velocity{};
    Vec3 target{};
    bool hasTarget{}, wasDown{};
    float out{}, bounce{};
    uint32_t throws{}, hits{}, catches{}, refused{}, timeouts{};
    double damageCandidate{}; // blows' worth counted (CANDIDATE, not sent)
    // 0.0.5 BATON CHAIN: this throw's enemies hit (ids), the one it flies at, and the counts.
    uint64_t hitIds[batonMaxChains + 1]{};
    uint32_t hitCount{}, chainsNow{};
    uint64_t targetId{};
    uint32_t chains{}, chainEnds{}, chainNoneInRange{};

    bool inHandNow() const { return state == inHand; }
    bool hitThisThrow(uint64_t id) const {
        for (uint32_t i = 0; i < hitCount; ++i)
            if (hitIds[i] == id)
                return true;
        return false;
    }
    // The chain's next enemy from `from`: the nearest within batonChainRange not hit on this throw
    // (-1 none).
    int chainTarget(Vec3 from, const BatonEnemy* list, uint32_t count) const {
        int best = -1;
        float bestMetres = batonChainRange;
        for (uint32_t i = 0; list && i < count; ++i) {
            const float metres = length(list[i].chest - from);
            if (!list[i].id || hitThisThrow(list[i].id) || !(metres <= bestMetres))
                continue;
            best = static_cast<int>(i);
            bestMetres = metres;
        }
        return best;
    }
    // One frame with the chain: list/count, the enemies near this frame; aimed, the one the aim
    // assist picked (an index into list, -1 none). Returns true on each frame it hits one.
    bool update(float dt, bool trigger, Vec3 hand, Vec3 aim, const BatonEnemy* list, uint32_t count, int aimed) {
        if (state == inHand && trigger && !wasDown) {
            hitCount = chainsNow = 0;
            targetId = aimed >= 0 && static_cast<uint32_t>(aimed) < count ? list[aimed].id : 0;
        }
        // The enemy it flies at, where he is now (he moves); gone from the list: where he was.
        Vec3 enemy = target;
        bool have = false;
        if (state == inHand) {
            have = aimed >= 0 && static_cast<uint32_t>(aimed) < count;
            if (have)
                enemy = list[aimed].chest;
        } else if (targetId) {
            have = true;
            for (uint32_t i = 0; list && i < count; ++i)
                if (list[i].id == targetId)
                    enemy = list[i].chest;
        }
        if (state == flying && hasTarget)
            target = enemy;
        const bool hit = update(dt, trigger, hand, aim, enemy, have);
        if (!hit)
            return false;
        if (hitCount < batonMaxChains + 1)
            hitIds[hitCount++] = targetId;
        const int next = chainsNow < batonMaxChains ? chainTarget(position, list, count) : -1;
        if (next < 0) { // the chain is done (or never started): home at once
            if (chainsNow < batonMaxChains)
                ++chainNoneInRange;
            else
                ++chainEnds;
            state = returning;
            targetId = 0;
            return true;
        }
        ++chainsNow;
        ++chains;
        targetId = list[next].id;
        target = list[next].chest;
        hasTarget = true;
        state = flying;
        const Vec3 to = target - position;
        const float len = length(to);
        velocity = (len > 1e-4f ? to / len : Vec3{0, 0, -1}) * batonSpeed;
        return true;
    }
    // One frame. trigger: held now; aim: the assisted aim (a unit vector); enemy/haveEnemy: the
    // enemy the aim assist picked (its chest). Returns true on the frame it hits him.
    bool update(float dt, bool trigger, Vec3 hand, Vec3 aim, Vec3 enemy, bool haveEnemy) {
        const bool press = trigger && !wasDown;
        wasDown = trigger;
        bool hit = false;
        switch (state) {
        case inHand:
            position = hand;
            if (press) {
                state = flying;
                ++throws;
                out = 0;
                hasTarget = haveEnemy;
                target = enemy;
                const Vec3 to = haveEnemy ? enemy - hand : aim;
                const float len = length(to);
                velocity = (len > 1e-4f ? to / len : Vec3{0, 0, -1}) * batonSpeed;
            }
            return false;
        default:
            if (press)
                ++refused; // the hand is empty: nothing to throw
            break;
        }
        out += dt;
        if (out > batonMaxOut) { // never lost: back in the hand
            ++timeouts;
            state = inHand;
            position = hand;
            return false;
        }
        if (state == flying) {
            if (hasTarget) { // it homes on him (the assist), like the show's throws
                const Vec3 to = target - position;
                const float len = length(to);
                if (len <= batonHitRadius + batonSpeed * dt) {
                    position = target;
                    state = ricochet;
                    bounce = 0;
                    ++hits;
                    damageCandidate += 1.0;
                    hit = true;
                    velocity = Vec3{0, 4, 0} - (to / std::max(len, 1e-4f)) * 6.f; // off him and up
                    return hit;
                }
                velocity = to / len * batonSpeed;
            }
            position += velocity * dt;
            if (length(position - hand) > batonReach)
                state = returning;
        } else if (state == ricochet) {
            bounce += dt;
            position += velocity * dt;
            if (bounce >= batonRicochet)
                state = returning;
        }
        if (state == returning) {
            const Vec3 to = hand - position;
            const float len = length(to);
            if (len <= batonCatch + batonBackSpeed * dt) {
                state = inHand;
                position = hand;
                ++catches;
            } else {
                velocity = to / len * batonBackSpeed;
                position += velocity * dt;
            }
        }
        return hit;
    }
};

// Version 0.0.5 SOUND EFFECTS: a baton's sounds from its counts: baton_whoosh on each throw,
// baton_hit on each hit (every chain bounce is a hit: update returns true on each). throwsSeen:
// the caller's copy of Baton::throws.
inline void batonSounds(uint32_t& throwsSeen, const Baton& b, bool hit) {
    if (b.throws != throwsSeen) {
        throwsSeen = b.throws;
        sfx::play(sfx::batonWhoosh);
    }
    if (hit)
        sfx::play(sfx::batonHit);
}

// Version 0.0.6 DAREDEVIL BATON AIM (strong aim assist, as the shooters'): the enemy whose chest is
// nearest the hand's aim within batonAssistDeg and batonReach (-1 none; ties: the nearer one).
inline constexpr float batonAssistDeg = 35.f;
inline int batonAim(Vec3 hand, Vec3 aim, const BatonEnemy* list, uint32_t count) {
    const float aimLen = length(aim);
    if (!list || !(aimLen > 1e-4f))
        return -1;
    const Vec3 a = aim / aimLen;
    const float minCos = std::cos(batonAssistDeg * 3.14159265f / 180.f);
    int best = -1;
    float bestCos = minCos, bestMetres = batonReach;
    for (uint32_t i = 0; i < count; ++i) {
        const Vec3 to = list[i].chest - hand;
        const float metres = length(to);
        if (!list[i].id || !(metres > 1e-3f) || !(metres <= batonReach))
            continue;
        const float c = dot(to / metres, a);
        if (c > bestCos + 1e-4f || (c >= bestCos - 1e-4f && metres < bestMetres)) {
            best = static_cast<int>(i);
            bestCos = std::max(c, minCos);
            bestMetres = metres;
        }
    }
    return best;
}

// Version 0.0.6 punches. A punch that lands (the gesture sent as the game's punch with an enemy's
// chest within punchReach of the fist) gets one more blow on sm2_damage's path (CONFIRMED):
//  - DAREDEVIL PUNCH k (1x-5x): (k - 1) x punchUnitHp more, so each punch hits like k of them.
//    punchUnitHp is the game's own punch estimated (no read of it: CANDIDATE).
//  - HULK: a normal enemy (punchNormalHp or less at most) gets all his health (one punch KO); a
//    brute gets most / 2.5 (2-3 punches with the game's own); the knock is big (the fling,
//    CANDIDATE: the knockback strength's top is not confirmed).
inline constexpr float punchReach = 2.2f;    // m, fist to chest
inline constexpr float punchUnitHp = 10.f;   // CANDIDATE
inline constexpr float punchNormalHp = 25.f; // rep0050: normal enemies 25, others 50
inline constexpr float hulkFlingKnock = 900.f;
inline float daredevilExtraHp(float k) {
    return std::isfinite(k) ? std::clamp(k - 1.f, 0.f, 4.f) * punchUnitHp : 0.f;
}
inline float hulkPunchHp(float most) {
    if (!std::isfinite(most) || !(most > 0))
        return punchNormalHp; // health unknown: a normal enemy's
    return most <= punchNormalHp + .5f ? most : most / 2.5f;
}
// The landed punch's enemy: the nearest chest (feet + 1.1 m) within punchReach of the fist (-1 none).
inline int punchVictim(Vec3 fist, const Vec3* feet, uint32_t count) {
    int best = -1;
    float bestMetres = punchReach;
    for (uint32_t i = 0; feet && i < count; ++i) {
        const float metres = length(feet[i] + Vec3{0, 1.1f, 0} - fist);
        if (metres <= bestMetres) {
            best = static_cast<int>(i);
            bestMetres = metres;
        }
    }
    return best;
}

// Version 0.0.6 HULK LEAP: the charged jump (A held on the ground) asks the swing for hulkLeapScale
// times the leap speed (4x the height and distance: height and range go with the square). The ask
// rides the swing command's asks bit hulkLeapAsk (game_swing.hpp; CANDIDATE: the speed write is the
// mod's own swing step, its landing the game's).
inline constexpr uint32_t hulkLeapAsk = 16u;
inline constexpr float hulkLeapScale = 2.f;
inline uint32_t leapAsks(uint32_t hero) { return hero == hulk ? hulkLeapAsk : 0u; }

} // namespace spidy::hero_modes
