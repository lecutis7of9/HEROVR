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
// Version 0.0.62 BATON BOUNCE (the user): 3 enemies in all (2 bounces) within 5 m of the one it hit,
// never one already hit on the throw, never one down (BatonEnemy::down: dead or knocked out, his hit
// points read 0 or less); a target who goes down while it flies sends it home; batonMaxOut still
// brings it back whatever happens.
// Version 0.0.7 HERO PROFILES (hero007.hpp): BATMAN (two batarangs on this Baton, a chain roll per
// hit: chainChance, default 25%; his own grapple gun and cape glide, cape_glide.hpp), WOLVERINE (claws,
// rapid healing), SUPERMAN (heat vision) and PUNISHER (pistol, shotgun). Every hit is a blow on
// sm2_damage's CONFIRMED path. Knockdown and disarm are NOT here: no CONFIRMED game path (research).
// Pure; no game memory.
#include "spidy/math.hpp"
#include "spidy/sfx.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace spidy::hero_modes {

// Version 0.0.7: BATMAN, WOLVERINE, SUPERMAN, PUNISHER after HULK (0.0.6's numbers unchanged);
// CAPTAIN AMERICA (captain.hpp) after PUNISHER.
// Version 0.0.72: THOR and GREEN LANTERN after CAPTAIN AMERICA (hero072.hpp).
enum Hero : uint32_t { spiderMan = 0, ironMan, daredevil, hulk, batman, wolverine, superman, punisher, captainAmerica, thor, greenLantern, heroes };
inline constexpr const char* heroNames[heroes] = {"SPIDER-MAN", "IRON MAN", "DAREDEVIL", "HULK",
                                                  "BATMAN",     "WOLVERINE", "SUPERMAN", "PUNISHER", "CAPTAIN AMERICA",
                                                  "THOR",       "GREEN LANTERN"};
// Which heroes have a working hologram toggle in this build (Version 0.0.6: all of them; 0.0.7: all).
inline constexpr bool shipped[heroes] = {true, true, true, true, true, true, true, true, true, true, true};
// Version 0.0.6: the hero from the three toggles (IRON MAN, DAREDEVIL, HULK); should two ever be on
// (a hand-edited live.txt), the first wins, so exactly one hero plays.
inline uint32_t fromToggles(bool ironMan_, bool daredevil_, bool hulk_) {
    return ironMan_ ? ironMan : daredevil_ ? daredevil : hulk_ ? hulk : spiderMan;
}
// Version 0.0.7: the seven toggles in combat38::heroToggles' order; the first on wins.
inline uint32_t fromToggles(bool ironMan_, bool daredevil_, bool hulk_, bool batman_, bool wolverine_,
                            bool superman_, bool punisher_) {
    const uint32_t old = fromToggles(ironMan_, daredevil_, hulk_);
    if (old != spiderMan)
        return old;
    return batman_ ? batman : wolverine_ ? wolverine : superman_ ? superman : punisher_ ? punisher : spiderMan;
}
// Version 0.0.7 CAPTAIN AMERICA: the eight toggles (combat38::heroToggles' order); the first on wins.
inline uint32_t fromToggles(bool ironMan_, bool daredevil_, bool hulk_, bool batman_, bool wolverine_,
                            bool superman_, bool punisher_, bool captain_) {
    const uint32_t seven = fromToggles(ironMan_, daredevil_, hulk_, batman_, wolverine_, superman_, punisher_);
    return seven != spiderMan ? seven : captain_ ? captainAmerica : spiderMan;
}
// Version 0.0.72: THOR and GREEN LANTERN after them (the ten toggles; the first on wins).
inline uint32_t fromToggles(bool ironMan_, bool daredevil_, bool hulk_, bool batman_, bool wolverine_,
                            bool superman_, bool punisher_, bool captain_, bool thor_, bool lantern_) {
    const uint32_t eight =
        fromToggles(ironMan_, daredevil_, hulk_, batman_, wolverine_, superman_, punisher_, captain_);
    return eight != spiderMan ? eight : thor_ ? thor : lantern_ ? greenLantern : spiderMan;
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
    // Version 0.0.7 BATMAN: no web path at all (his grapple gun and cape glide are cape_glide.hpp's).
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
// Version 0.0.7: every hero but SPIDER-MAN loses RT whatever LT does (BATMAN included: the game's
// zip is a web; his grapple gun is cape_glide.hpp's). leftTrigger is kept for the callers.
inline uint32_t stripPad(uint32_t hero, uint16_t& buttons, uint8_t& rightTrigger, uint16_t askedButtons,
                         bool askedRt, uint8_t leftTrigger) {
    (void)leftTrigger;
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
inline uint32_t stripPad(uint32_t hero, uint16_t& buttons, uint8_t& rightTrigger, uint16_t askedButtons,
                         bool askedRt) {
    return stripPad(hero, buttons, rightTrigger, askedButtons, askedRt, 0);
}

// Version 0.0.7 FLY MODE: the heroes whose trigger fires a weapon - IRON MAN's palm repulsor,
// DAREDEVIL's baton throw, BATMAN's batarangs, WOLVERINE's claws, PUNISHER's guns, CAPTAIN AMERICA's
// shield throw (the SHIELD ARM hand's trigger, RIGHT by default). Their triggers
// never thrust FLY MODE (the user, 26 Oct: "if youre iron man and you shoot your hand palm you
// actually fly around"); they fly by the poses (a hand up rises, both hands forward fly, a T-pose
// stops), the sticks and R3, which every hero already has. SPIDER-MAN (no webs while flying),
// SUPERMAN (his triggers are flight's: no heat vision while flying) and HULK (no trigger weapon)
// keep the trigger thrust. A hero added later with a trigger weapon belongs here too.
inline bool triggerWeapon(uint32_t hero) {
    return hero == ironMan || hero == daredevil || hero == batman || hero == wolverine || hero == punisher ||
           hero == captainAmerica || hero == thor || hero == greenLantern; // Version 0.0.72: THOR's hammer, GREEN LANTERN's ring beam and chain (he takes off with both arms up)
}
// The trigger value FLY MODE's thrust gets for this hero (0 for a trigger-weapon hero; NaN is 0).
inline float flyThrust(uint32_t hero, float trigger) {
    if (triggerWeapon(hero) || !std::isfinite(trigger))
        return 0.f;
    return std::clamp(trigger, 0.f, 1.f);
}

// Version 0.0.7: a small seeded random number source (xorshift32) for the per-hit chance rolls;
// tests give it a seed. chance(p): true with probability p (p >= 1 always, p <= 0 never; neither
// draws a number, so DAREDEVIL's sure chain draws nothing).
struct Roll {
    uint32_t state = 0x9e3779b9u;
    float next01() {
        uint32_t x = state ? state : 0x9e3779b9u;
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        state = x;
        return static_cast<float>(x >> 8) / 16777216.f; // [0, 1)
    }
    bool chance(float p) {
        if (!(p > 0))
            return false;
        if (p >= 1)
            return true;
        return next01() < p;
    }
};

// DAREDEVIL's baton, one per hand.
inline constexpr float batonSpeed = 22.f;      // m/s out
inline constexpr float batonBackSpeed = 18.f;  // m/s back (it chases the moving hand)
inline constexpr float batonReach = 25.f;      // m: past this with no enemy it turns back
inline constexpr float batonHitRadius = .45f;  // m
inline constexpr float batonCatch = .3f;       // m from the hand: caught
inline constexpr float batonRicochet = .25f;   // s bouncing off him before it heads home
inline constexpr float batonMaxOut = 4.f;      // s: never lost; back in the hand after this
inline constexpr float batonChainRange = 5.f;  // m from the enemy it hit to the next (0.0.62; 0.0.5-0.0.6: 6)
inline constexpr uint32_t batonMaxChains = 2;  // bounces per throw: 3 enemies at most (0.0.62; 0.0.5-0.0.6: 3)
inline constexpr float curveAim = 12.f;      // m: BATARANG CURVE with no target arcs onto the aim this far out
// Version 0.0.62: the closest distance between segment a-b and segment c-d (metres).
inline float segmentGap(Vec3 a, Vec3 b, Vec3 c, Vec3 d) {
    const Vec3 u = b - a, v = d - c, w = a - c;
    const float A = dot(u, u), B = dot(u, v), C = dot(v, v), D = dot(u, w), E = dot(v, w);
    float s = 0, t = 0;
    if (A < 1e-9f && C < 1e-9f)
        return length(a - c);
    if (A < 1e-9f) {
        t = std::clamp(E / C, 0.f, 1.f);
    } else if (C < 1e-9f) {
        s = std::clamp(-D / A, 0.f, 1.f);
    } else {
        const float den = A * C - B * B;
        s = den > 1e-9f ? std::clamp((B * E - C * D) / den, 0.f, 1.f) : 0.f;
        t = std::clamp((B * s + E) / C, 0.f, 1.f);
        s = std::clamp((B * t - D) / A, 0.f, 1.f);
    }
    return length((a + u * s) - (c + v * t));
}
// One enemy the aim assist sees this frame (the chain's list): an id that stays his while he lives.
struct BatonEnemy {
    uint64_t id{};
    Vec3 chest{};
    bool down{}; // Version 0.0.62: dead or knocked out (hit points read 0 or less): never a target
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
    uint32_t straightLocks{}; // 0.0.62: straight throws that hit someone on the way
    uint32_t targetDowns{};   // 0.0.62: its target went down while it flew (home, no hit)
    // Version 0.0.7 BATMAN CHAIN ROLL: each hit rolls this chance to chain on (DAREDEVIL: 1, always);
    // a failed roll sends it home. batonMaxChains still caps it (3 enemies in all).
    float chainChance = 1.f;
    Roll roll{};
    uint32_t chainRollsWon{}, chainRollsLost{};
    // Version 0.0.7 BATMAN BATARANG CURVE: the flight leans `curve` radians off the straight line to
    // where it is going (0, the default: straight - DAREDEVIL's batons, the shield). Held at that angle
    // the path is a gentle arc that still ENDS on the target (it closes in at cos(curve) of its speed).
    // With no target it arcs onto aimPoint (curveAim metres along the aim), then flies on straight.
    float curve = 0;
    float curveSign = 1; // +1 bends one way, -1 the other (the hand's side)
    Vec3 aimPoint{};
    bool toAimPoint{};
    Vec3 curvedVelocity(Vec3 to) const {
        const float len = length(to);
        const Vec3 d = len > 1e-4f ? to / len : Vec3{0, 0, -1};
        Vec3 side = cross(d, Vec3{0, 1, 0});
        if (length(side) < 1e-3f)
            side = cross(d, Vec3{1, 0, 0});
        side = normalized(side);
        return (d * std::cos(curve) + side * (curveSign * std::sin(curve))) * batonSpeed;
    }

    // Version 0.0.62: the first enemy (index, -1 none) whose body (chest -1.0 m .. +0.6 m) the
    // segment a-b passes within batonHitRadius of; ties: the one nearest a.
    static int sweepHit(Vec3 a, Vec3 b, const BatonEnemy* list, uint32_t count) {
        if (!finite(a) || !finite(b))
            return -1;
        int best = -1;
        float bestMetres = 1e9f;
        for (uint32_t i = 0; list && i < count; ++i) {
            const Vec3 c = list[i].chest;
            if (!list[i].id || list[i].down || !finite(c))
                continue;
            if (segmentGap(a, b, c - Vec3{0, 1.f, 0}, c + Vec3{0, .6f, 0}) > batonHitRadius)
                continue;
            const float m = length(c - a);
            if (m < bestMetres) {
                best = static_cast<int>(i);
                bestMetres = m;
            }
        }
        return best;
    }

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
            if (!list[i].id || list[i].down || hitThisThrow(list[i].id) || !(metres <= bestMetres))
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
                if (list[i].id == targetId) {
                    enemy = list[i].chest;
                    // Version 0.0.62: he went down while it flew at him: home, no hit.
                    if (list[i].down && state == flying) {
                        state = returning;
                        targetId = 0;
                        hasTarget = have = false;
                        ++targetDowns;
                    }
                }
        }
        if (state == flying && hasTarget)
            target = enemy;
        // Version 0.0.62 BATON STRAIGHT THROW: a throw with no enemy picked (none in the 35 deg cone)
        // had no hit test at all and flew through everyone (B3ARVR, 0.0.6.6). Now this frame's flight
        // is swept against each enemy's body; the first one it passes becomes its target (it homes
        // the last few centimetres and hits him on the usual path, chain and blow included).
        if (state == flying && !hasTarget && list && count) {
            const int k = sweepHit(position, position + velocity * std::clamp(dt, 0.f, .1f), list, count);
            if (k >= 0) {
                targetId = list[k].id;
                target = enemy = list[k].chest;
                hasTarget = have = true;
                ++straightLocks;
            }
        }
        const bool hit = update(dt, trigger, hand, aim, enemy, have);
        if (!hit)
            return false;
        if (hitCount < batonMaxChains + 1)
            hitIds[hitCount++] = targetId;
        int next = chainsNow < batonMaxChains ? chainTarget(position, list, count) : -1;
        bool lostRoll = false;
        if (next >= 0) { // Version 0.0.7: the roll only when there is someone to chain to
            if (roll.chance(chainChance)) {
                if (chainChance < 1)
                    ++chainRollsWon;
            } else {
                ++chainRollsLost;
                lostRoll = true;
                next = -1;
            }
        }
        if (next < 0) { // the chain is done (or never started): home at once
            if (!lostRoll && chainsNow < batonMaxChains) // a lost roll is counted in chainRollsLost
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
        velocity = curve > 0 ? curvedVelocity(to) : (len > 1e-4f ? to / len : Vec3{0, 0, -1}) * batonSpeed;
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
                toAimPoint = false;
                if (curve > 0) { // BATARANG CURVE: leave on the arc
                    toAimPoint = !haveEnemy;
                    aimPoint = hand + (len > 1e-4f ? to / len : Vec3{0, 0, -1}) * curveAim;
                    velocity = curvedVelocity(haveEnemy ? to : aimPoint - hand);
                }
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
                velocity = curve > 0 ? curvedVelocity(to) : to / len * batonSpeed;
            } else if (toAimPoint) { // BATARANG CURVE with no target: arc onto the aim, then straight on
                const Vec3 to = aimPoint - position;
                if (length(to) < 1.f)
                    toAimPoint = false;
                else
                    velocity = curvedVelocity(to);
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

// Version 0.0.62 BATON SWING (the fix for "batons do no damage", the user and B3ARVR on 0.0.6.6): in
// 0.0.6 a baton only hurt when thrown at an enemy the aim assist picked; swinging it in the hand
// hit nothing. Now a baton in the hand hits an enemy when its shaft (grip to tip, tracking space)
// or its tip's path this frame passes within swingRadius of his body (chest -1.0 m .. +0.6 m) while
// the tip moves at least swingSpeed - a swing, not a baton resting against him - at most once per
// enemy every swingCooldown seconds. The blow is batonHit on sm2_damage's path (CONFIRMED).
inline constexpr float swingSpeed = 2.f;      // m/s at the tip
inline constexpr float swingRadius = .35f;    // m from the body line
inline constexpr float swingCooldown = .35f;  // s per enemy
struct BatonSwing {
    static constexpr int remembered = 8;
    uint64_t ids[remembered]{};
    float since[remembered]{};
    Vec3 tipWas{};
    bool haveTip{};
    uint32_t hits{}, cooled{};
    float lastSpeed{};
    void reset() { *this = BatonSwing{}; }
    bool cooling(uint64_t id) const {
        for (int k = 0; k < remembered; ++k)
            if (ids[k] == id && since[k] < swingCooldown)
                return true;
        return false;
    }
    // One frame with the baton in the hand (inHand false: forgets the tip, hits nothing). Returns
    // the index into list of the enemy hit this frame (-1 none).
    int update(float dt, bool inHand, Vec3 grip, Vec3 tip, const BatonEnemy* list, uint32_t count) {
        const float step = std::isfinite(dt) ? std::clamp(dt, 0.f, .1f) : 0.f;
        for (int k = 0; k < remembered; ++k)
            since[k] = std::min(since[k] + step, 10.f);
        if (!inHand || !finite(grip) || !finite(tip)) {
            haveTip = false;
            return -1;
        }
        const Vec3 was = haveTip ? tipWas : tip;
        tipWas = tip;
        haveTip = true;
        lastSpeed = step > 1e-4f ? length(tip - was) / step : 0.f;
        if (!(lastSpeed >= swingSpeed) || !list)
            return -1;
        int best = -1;
        float bestGap = swingRadius;
        for (uint32_t i = 0; i < count; ++i) {
            const Vec3 c = list[i].chest;
            if (!list[i].id || !finite(c))
                continue;
            const Vec3 lo = c - Vec3{0, 1.f, 0}, hi = c + Vec3{0, .6f, 0};
            const float gap = std::min(segmentGap(grip, tip, lo, hi), segmentGap(was, tip, lo, hi));
            if (gap > bestGap)
                continue;
            if (cooling(list[i].id)) {
                ++cooled;
                continue;
            }
            best = static_cast<int>(i);
            bestGap = gap;
        }
        if (best < 0)
            return -1;
        int slot = 0;
        for (int k = 1; k < remembered; ++k)
            if (since[k] > since[slot])
                slot = k;
        ids[slot] = list[best].id;
        since[slot] = 0;
        ++hits;
        return best;
    }
};
// The in-hand baton's shaft from the hand and its aim (as it is drawn): grip 0.12 m behind, tip
// 0.45 m ahead.
inline void batonShaft(Vec3 hand, Vec3 aim, Vec3& grip, Vec3& tip) {
    const Vec3 a = length(aim) > 1e-4f ? normalized(aim) : Vec3{0, 0, -1};
    grip = hand - a * .12f;
    tip = hand + a * .45f;
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
