#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
// Made by Agent Mulder.
//
// Version 0.0.72 (TEST): the one composer of the 0.0.72 rules (hero072.hpp, hero072_progress.hpp)
// for game_xr, as hero007_play.hpp is 0.0.7's. Pure: no game calls. Each frame game_xr fills PlayIn
// (the head, the hands, the enemies, the health lost, the options) and gets PlayOut back: the blows
// to queue on sm2_damage (CONFIRMED), the haptics, the rage tint, whether to keep the health topped,
// whether a wall crawl is allowed, the grapple asks, the punch scales, the status lines and the Look.
// Every feature has its own Guard: a non-finite number trips it off for the session, never the mod.
// Gestures only, except the basic projectiles (GREEN LANTERN's beam and chain, as the user's 8:3x
// list asked): no trigger is ever a movement or a special.
#include "spidy/thor_grip.hpp"
#include "spidy/hero072.hpp"
#include "spidy/hero072_progress.hpp"
#include "spidy/hero072_visual.hpp"
#include "spidy/holster.hpp"
#include "spidy/punisher_rig.hpp"
#include "spidy/prop_bounce.hpp"

#include <cstdio>
#include <cstring>

namespace spidy::hero072 {

// The 0.0.72 options, read by game_xr from combat38 (c38) and turned into on/off here.
struct PlayOptions {
    bool thorLightning = true, thorStorm = true, thorChain = true;
    bool glFlight = true, glFist = true, glBeam = true, glBubble = true, glChain = true, glCharge = true;
    bool spiderClimbOnly = true, uppercut = true, smokeBlind = true, ddGrapple = true, ddParry = true;
    bool ironHud = true, punisherRage = true, reputation = true, survival = false;
    uint32_t punisherSuit{};
    bool naturalInvincible = true, hulkRage = true;
    uint32_t rageBlood{}, rageTint{};
    bool fireDouse = true, shieldSlam = true, punisherHook = true, clawClimb = false /* 0.0.73: off by default */, shove = true;
    uint32_t faction{}, testUnlocks{};
    bool ultimates = true;
    uint32_t perkBranch{};
    bool ownSuit{};
    bool punisherVan = false; // 10:41: the Punisher van (EXPERIMENTAL, off by default)
    uint32_t thorHammer{};    // Version 0.0.72 THOR HAMMER: 0 BOOOOP, 1 DEVILSEYE, 2 LOVE THUNDER, 3 FCCOOPER (the look only)
    bool perchTakedown = true, takedownLight = true; // 9:31, 9:32: BATMAN's PERCH TAKEDOWN and its light
    // From the combat38 values (ON first: 0 is ON; SURVIVAL and OWN SUIT: OFF first).
    static bool onFirst(uint32_t v) { return v == 0; }
    static bool offFirst(uint32_t v) { return v == 1; }
};
struct PlayHand {
    Vec3 position{}, aim{0, 0, -1};
    bool tracked{};
    float trigger{}, grip{};
    thor_grip::Held hammer{}; // test build 10: a hammer held in this fist, from the grip pose (ok false: none)
};
struct PlayIn {
    float dt{};
    bool play{};
    uint32_t hero{};
    Pose head{};
    PlayHand hands[2]{};
    // Version 0.0.73: the hand's trigger is the hologram's / phone's (no hook fire), the hand holds a gun (no
    // hook draw), and DAREDEVIL's 0.0.72 cast-up gesture (off: the held trigger grapples, game_xr).
    bool handBusy[2]{}, gunInHand[2]{};
    float eyeHeight{}; // 0.0.73 test 3: the calibrated eye height (m; 0: the standing average) - the rig's scale
    bool ddCastGesture{};
    const BatonEnemy* list{};
    uint32_t count{};
    float lostShare{};       // health lost this frame / the most health (0: none, NaN: unknown)
    bool hitSent{};          // another blow of the mod's was sent this frame (hero007)
    uint64_t lockId{};       // the aim assist's enemy (0: none)
    Vec3 lockAt{};
    bool flying{};
    float speed{}, altitude{}; // m/s, metres (NaN: unknown)
    float framePeriod = 1.f / 90.f;
    bool clawsOut{};         // WOLVERINE's claws (hero007)
    uint32_t shieldHand = 1; // CAPTAIN AMERICA's shield arm (0 left, 1 right)
    const Vec3* puffs{};     // BATMAN's smoke puffs (hero007) and their ages
    const float* puffAges{};
    uint32_t puffCount{};
    float puffLife{};
    // BATMAN's PERCH TAKEDOWN (9:31): the game side's state (each false / nullptr when unknown).
    bool perched{}, gliding{};
    bool faceButton{};         // the takedown's face button held (never a trigger)
    bool jumpBusy{};           // build 5: A held / a leap charging or going / a takeoff (no baton cast)
    bool takedownPrompt{};     // the game names that button in its takedown prompt (game_prompts)
    const Vec3* facings{};     // each enemy's facing, as `list` (nullptr: unknown)
    LineClear lineClear{};
    void* lineUser{};
    PlayOptions o{};
    // Version 0.0.73 PROP BOUNCE (fix 7): the surface ahead of THOR's thrown hammer (this frame's
    // tracking space; serial: the throw's low 4 bits). Unknown: it flies as in 0.0.72.
    prop_bounce::Plane hammerWall{};
};
enum BlowKind : uint32_t {
    kindUppercut = 0, kindShove, kindSmoke, kindParry, kindShieldSlam, kindFist, kindBeam, kindChain, kindHammer,
    kindStorm, kindBolt, kindLaunch, kindSlam, kindUltimate, blowKinds
};
inline constexpr const char* blowKindNames[blowKinds] = {
    "UPPERCUT", "SHOVE", "SMOKE BLIND", "BATON PARRY", "SHIELD SLAM", "CONSTRUCT FIST", "RING BEAM",
    "CHAIN CONSTRUCT", "HAMMER", "THOR STORM", "LIGHTNING", "HAMMER LAUNCH", "HAMMER SLAM", "ULTIMATE"};
struct PlayBlow {
    uint64_t id{};
    sm2_damage::Blow blow{};
    Vec3 at{};
    uint32_t kind{};
};
inline constexpr uint32_t blowsMost = 24;
struct PlayOut {
    PlayBlow blows[blowsMost]{};
    uint32_t blowCount{};
    float haptic[2]{};
    bool keepTopped{};         // NATURAL INVINCIBLE: refill the health this frame
    bool climbAllowed = true;  // SPIDER CLIMB ONLY
    bool grapple[2]{};         // the grapple without the cape (hook, baton cast): that hand's ask
    bool hookOut[2]{};         // 0.0.72: the PUNISHER's hook launcher drawn in this hand (grapple-gun model)
    float punchScale[2]{1, 1}; // SURVIVAL: an injured arm hits half
    float dazed{};             // SURVIVAL: a concussion's sway, 0-1
    RageTint tint{};
    uint32_t suit{};           // the suit in use (0: the game's own / the player's own mod)
    uint32_t faction{};        // the faction chosen (the status page)
    uint32_t factionWorn{};    // what the enemies wear: VANILLA until its models and the hook are in
    bool ultimateFired{};
    // Version 0.0.72: THOR's lightning strikes this frame, each one of the player's two sounds picked at
    // random (sfx::thorLightning1 / 2): the game plays lightning[k] times sound k (one playback a strike).
    uint8_t lightning[2]{};
    PerchOut perch{};          // BATMAN's PERCH TAKEDOWN: the light, the button to send, the rope, the alerted
    uint32_t events{};         // bit 1 << BlowKind: what fired this frame (the log)
    Look look{};
    void blow(uint64_t id, sm2_damage::Blow b, Vec3 at, uint32_t kind) {
        if (id && blowCount < blowsMost && std::isfinite(b.amount) && b.amount > 0) {
            blows[blowCount++] = {id, b, at, kind};
            events |= 1u << kind;
        }
    }
    void buzz(unsigned hand, float s) {
        if (hand < 2)
            haptic[hand] = std::max(haptic[hand], s);
    }
};

// Who a hand at `at` touches (a landed punch's stand-in: within touchRange of a standing chest).
inline constexpr float touchRange = .65f;
inline constexpr float hammerReach = .9f;
inline constexpr float chainJump = 8.f, hammerFlight = .35f;
// Build 7 THOR's throw: the release speed (the fastest of the last swingMemory s) times throwGain, kept
// between throwSpeedLow and throwSpeedHigh m/s; out at most throwSeconds or throwRange m, back at
// returnSpeed, caught within catchRange; it hits what passes within hammerHitRange.
inline constexpr float throwMinSpeed = 1.4f, swingMemory = .15f, throwGain = 6.f, throwSpeedLow = 14.f, throwSpeedHigh = 30.f;
inline constexpr float throwSeconds = 1.f, throwRange = 26.f, returnSpeed = 24.f, catchRange = .4f, hammerHitRange = 1.2f;
// LIGHTNING BLAST (the right trigger, hammer in hand) and every bolt's life on screen.
inline constexpr float blastRange = 30.f, blastEvery = .6f, boltSeconds = .3f;
// Version 0.0.72 THOR HAMMER: the grip 6 cm up the handle; SHEATHED, the handle's end 20 cm under the head,
// 15 cm behind and 12 cm right (inside the draw reach: below <= .25, ahead <= .05, side >= -.05).
inline constexpr float hammerGrip = .06f; // (only with no grip pose: thor_grip.hpp places a held hammer)
// Test build 10 THOR THROW ("throwing the hammer could be smoother and it's kind of hard to aim"):
//  - AIM: along the controller's pointing (aimShare of the way) blended with the swing's own direction;
//    an enemy within assistCone (12 degrees) of that line takes it (the nearest to the line, not the
//    nearest to Thor) - a gentle assist, nothing outside the cone is pulled in.
//  - FLIGHT: one cruise speed (throwSpeed, whatever the swing - a weak flick no longer flies slowly), a
//    homing hammer turns at most homingTurn rad/s toward its target (build 7-9 snapped onto it each step).
//  - RECALL: it turns back at returnTurn rad/s (no instant about-face), straight in once within
//    returnStraight m, slowing near the hand (returnNear m/s per metre, at least returnSlowest) to the catch.
//  - SPIN: spinPerMetre radians per metre flown (steady with the speed); the image extrapolates the
//    flight to its own time (no stutter at the game's step rate).
//  - HINT: winding up (the grip held, the hand over hintSpeed m/s) a faint line shows where it would go.
inline constexpr float aimShare = .7f, assistCone = .978f /* 12 deg */, throwSpeed = 22.f, homingTurn = 7.f;
inline constexpr float returnTurn = 9.f, returnStraight = 2.5f, returnNear = 8.f, returnSlowest = 6.f, spinPerMetre = .8f;
inline constexpr float hintSpeed = 1.f, hintLength = 12.f;
// The throw's line: the pointing (aim) blended with the swing (both unit or zero); the aim alone when the
// swing goes backwards from it.
inline Vec3 throwLine(Vec3 aim, Vec3 swing) {
    const bool a = length(aim) > .5f, w = length(swing) > .5f;
    if (!a)
        return w ? normalized(swing) : Vec3{0, 0, -1};
    const Vec3 n = normalized(aim);
    if (!w || dot(n, normalized(swing)) < 0.f)
        return n;
    const Vec3 b = n * aimShare + normalized(swing) * (1.f - aimShare);
    return length(b) > 1e-3f ? normalized(b) : n;
}
// The enemy closest to the line from `from` along `dir` within assistCone (and pullRange), or -1.
inline int assistTarget(Vec3 from, Vec3 dir, const BatonEnemy* list, uint32_t count) {
    int best = -1;
    float bestDot = assistCone;
    for (uint32_t i = 0; list && i < count; ++i) {
        if (!list[i].id || list[i].down || !ok(list[i].chest))
            continue;
        const Vec3 to = list[i].chest - from;
        const float d = length(to);
        if (d < .5f || d > pullRange)
            continue;
        const float c = dot(to / d, dir);
        if (c >= bestDot)
            bestDot = c, best = static_cast<int>(i);
    }
    return best;
}
// `now` (unit) turned toward `want` (unit) by at most `most` radians.
inline Vec3 steer(Vec3 now, Vec3 want, float most) {
    const float c = std::clamp(dot(now, want), -1.f, 1.f), a = std::acos(c);
    if (!(a > most) || !(most >= 0.f))
        return want;
    Vec3 side = want - now * c;
    if (!(length(side) > 1e-5f)) { // straight back: turn about any side
        side = cross(now, Vec3{0, 1, 0});
        if (!(length(side) > 1e-5f))
            side = cross(now, Vec3{1, 0, 0});
    }
    side = normalized(side);
    return normalized(now * std::cos(most) + side * std::sin(most));
}
inline Vec3 sheathAt(Vec3 head, Vec3 bodyForward, Vec3 bodyRight) {
    return head + Vec3{0, -.20f, 0} - bodyForward * .15f + bodyRight * .12f;
}
// A face for a hammer pointing along `along`: `look` without its part along the handle (world up when parallel).
inline Vec3 faceOf(Vec3 along, Vec3 look) {
    Vec3 f = look - along * dot(look, along);
    if (!(length(f) > 1e-3f))
        f = Vec3{0, 1, 0} - along * along.y;
    if (!(length(f) > 1e-3f))
        f = Vec3{0, 0, -1};
    return normalized(f);
}
inline constexpr float longRageSeconds = 4.f; // the PUNISHER's LONG RAGE: the rage kept after a fight

class Play {
  public:
    // One guard each (the status page lists the tripped ones).
    enum G : uint32_t { gUpper = 0, gShove, gSmoke, gDare, gIron, gRage, gRep, gSurvival, gDouse, gSlam, gHook, gLantern,
                        gThor, gUltimate, gChain, gPerch, guards };
    Guard guard[guards]{};
    HandSpeed speed;
    Fight fight;
    DownCounter downs;
    Rage rage;
    HulkRage hulk;
    HeartBeat heart;
    Blood blood;
    BloodPerf bloodPerf;
    Survival survival;
    Reputation rep;
    SuitCatalog suits;
    Perks perks;
    UltimateMove ultimate;
    SmokeBlind smoke;
    BatonCast cast;
    ParryTiming parry;
    IronHud iron;
    Charge charge;
    RingFlight ringFlight;
    FistGesture fist;
    BubbleGesture bubble;
    StormHold storm;
    HammerSwing swing;
    ShoveGesture shove;
    PerchTakedown perchTd;
    ShieldSlam slam;
    HookLauncher hooks[2]; // 0.0.73 test 3: either hand's (one launcher: a hand draws it while the other has not)
    Fires fires;
    SpinGesture spin;
    PalmSpray palm;
    HoseGesture hose;
    ClapGesture clap;
    holster::BodyYaw body;
    float upperRearm[2]{};
    float beamTick{}, chainShow{}, fistShow{}, shockShow{}, hookShow{}, stormLeft{}, stormNext{};
    uint32_t stormLeftStrikes{};
    Vec3 chainTo{}, fistAt{}, fistDir{0, 0, -1}, shockAt{}, hookFrom{}, hookTo{};
    float lastDt{};
    float shockRadius{};
    bool shockSubtle{};
    bool hammerHeld{}, gripWas{};
    uint32_t lightningSeed = 0x9e3779b9u; // THOR's strike sound pick (xorshift; seedLightning)
    float hammerOut{}; // THOR's thrown hammer: seconds left in the air
    Vec3 hammerTo{};
    uint32_t heroWas = hero_modes::heroes;
    float clock{};
    uint64_t frames{}, blowsOut{}, doused{};
    bool progressDirty{};

    Play() { suits.builtIn(); }

    void step(const PlayIn& in, PlayOut& out) {
        out = {};
        const float dt = clampDt(in.dt);
        lastDt = dt;
        clock += dt;
        ++frames;
        const uint32_t h = in.hero < hero_modes::heroes ? in.hero : 0u;
        if (h != heroWas) {
            heroWas = h;
            rep.select(h);
            rage.reset();
            hulk.reset();
            hammerHeld = false;
            hammerOut = 0;
            hammerFlying = hammerBack = false;
            for (auto& b : liveBolts)
                b.left = 0;
        }
        const PlayOptions& o = in.o;
        const Vec3 head = in.head.position;
        const Vec3 look = in.head.orientation.rotate(Vec3{0, 0, -1});
        Vec3 p[2]{in.hands[0].position, in.hands[1].position};
        bool tracked[2]{in.hands[0].tracked && ok(p[0]), in.hands[1].tracked && ok(p[1])};
        speed.update(dt, p, tracked);
        body.update(dt, look);
        const Vec3* v = speed.velocity;

        // ---- the fight, reputation, rage, survival (all heroes) ----
        const uint32_t near = standing(in.list, in.count, head, fightRange);
        fight.standingFar = standing(in.list, in.count, head, 1e6f) - near;
        const uint32_t newDowns = downs.step(in.list, in.count);
        const float lost = std::isfinite(in.lostShare) ? std::clamp(in.lostShare, 0.f, 1.f) : 0.f;
        fight.step(dt, near, lost, newDowns, in.hitSent);
        if (guard[gRep].live(o.reputation)) {
            RepEvents e;
            e.downs = newDowns;
            e.cleared = fight.justCleared;
            e.abandoned = fight.justAbandoned;
            const int32_t was = rep.points;
            rep.step(e);
            if (rep.points != was)
                progressDirty = true;
        }
        const int32_t points = rep.points;
        // SPIDER-MAN stays the vanilla game (9:14): the REP meter only, nothing else of 0.0.72.
        if (h == hero_modes::spiderMan) {
            survival.reset();
            out.climbAllowed = true;
            finishLook(out);
            return;
        }
        const bool allSuits = testSuits(o.testUnlocks);
        suits.unlockFor(h, points);
        out.suit = suits.inUse(h, allSuits, o.ownSuit);
        out.faction = factionFor(h, o.faction);
        out.factionWorn = factionWorn(out.faction);
        if (perks.spend(h, points, o.perkBranch, testPerks(o.testUnlocks)))
            progressDirty = true;
        out.keepTopped = keepTopped(h, o.naturalInvincible, false);
        if (guard[gSurvival].live(o.survival) && !out.keepTopped) {
            survival.step(dt, h, lost, fight.on, std::isfinite(in.speed) ? in.speed : 0.f, in.flying);
            for (unsigned i = 0; i < 2; ++i)
                out.punchScale[i] = survival.punchScale(i);
            out.dazed = survival.dazed(clock);
        } else if (!o.survival) {
            survival.reset();
        }
        // PUNISHER RAGE and HULK RAGE, the heartbeat tint.
        rage.step(dt, guard[gRage].live(o.punisherRage) && h == hero_modes::punisher, fight,
                  longRageSeconds * (perks.scale(h, Stat::rageSeconds) - 1.f) / .3f);
        hulk.step(dt, o.hulkRage && h == hero_modes::hulk, fight, lost, perks.scale(h, Stat::rageBuild));
        const float level = h == hero_modes::hulk ? hulk.level() : rage.level;
        const float pulse = heart.step(dt, level);
        out.tint = rageTint(h, level, pulse, rageTintScale(o.rageTint));
        blood.step(dt);
        if (bloodPerf.step(dt, in.framePeriod, blood.liveDrops() > 0))
            blood.clear();
        out.look.blood = blood.liveDrops() || blood.livePools() ? &blood : nullptr;

        // ---- SPIDER CLIMB ONLY / CLAW CLIMB ----
        out.climbAllowed = climbAllowed(h, o.spiderClimbOnly, in.clawsOut, o.clawClimb);

        if (!in.play) {
            finishLook(out);
            return;
        }
        // ---- UPPERCUT (every hero; the tier is his) ----
        for (unsigned i = 0; i < 2; ++i) {
            upperRearm[i] = std::max(0.f, upperRearm[i] - dt);
            if (!guard[gUpper].live(o.uppercut) || !tracked[i] || upperRearm[i] > 0 || !isUppercut(speed.upPeak[i]))
                continue;
            const int e = touched(p[i], in.list, in.count);
            if (e >= 0) {
                upperRearm[i] = .6f;
                out.blow(in.list[e].id, uppercutBlow(h), in.list[e].chest, kindUppercut);
                out.buzz(i, .5f);
            }
        }
        // ---- SHOVE (both open hands pushed fast at a close enemy) ----
        if (guard[gShove].live(o.shove) && tracked[0] && tracked[1] &&
            shove.step(dt, in.hands[0].grip, in.hands[1].grip, p[0], p[1], v[0], v[1], look)) {
            const int e = nearestEnemy((p[0] + p[1]) * .5f, in.list, in.count, shoveReach);
            if (e >= 0) {
                out.blow(in.list[e].id, shoveBlow(), in.list[e].chest, kindShove);
                out.buzz(0, .4f);
                out.buzz(1, .4f);
            }
        }
        // ---- BATMAN: PERCH TAKEDOWN (a quick face-button press: the game's own hang, by its prompt) ----
        if (h == hero_modes::batman && guard[gPerch].live(o.perchTakedown)) {
            PerchIn pi;
            pi.dt = dt;
            pi.on = true;
            pi.lightOn = o.takedownLight;
            pi.head = in.head;
            pi.perched = in.perched;
            pi.gliding = in.gliding;
            pi.button = in.faceButton;
            pi.promptNamed = in.takedownPrompt;
            pi.list = in.list;
            pi.facings = in.facings;
            pi.count = in.count;
            pi.clear = in.lineClear;
            pi.user = in.lineUser;
            perchTd.step(pi, out.perch);
            if (!guard[gPerch].sane(!out.perch.rope || (ok(out.perch.ropeFrom) && ok(out.perch.ropeTo)), "PERCH: a bad rope")) {
                perchTd.reset();
                out.perch = PerchOut{};
            } else if (out.perch.sendButton) {
                out.buzz(0, .5f);
                out.buzz(1, .5f);
            }
        } else {
            perchTd.reset();
        }
        // ---- BATMAN: SMOKE BLIND ----
        if (h == hero_modes::batman && guard[gSmoke].live(o.smokeBlind)) {
            BlindOut b;
            smoke.step(dt, in.puffs, in.puffAges, in.puffCount, in.puffLife, in.list, in.count, b);
            for (uint32_t k = 0; k < b.count; ++k)
                out.blow(b.ids[k], smokeStagger(), b.at[k], kindSmoke);
        }
        // ---- DAREDEVIL: BATON GRAPPLE (cast up fast) and PARRY (batons crossed at the face) ----
        if (h == hero_modes::daredevil && guard[gDare].live(true)) {
            // Version 0.0.73 (fix 5): DAREDEVIL's grapple is the held trigger along the baton (game_xr's
            // grapple073::TapOrHold); 0.0.72's cast-up gesture no longer fires it.
            if (o.ddGrapple && in.ddCastGesture) {
                const int i = cast.step(dt, head, p, v, tracked, in.jumpBusy);
                if (i >= 0) {
                    out.grapple[i] = true;
                    out.buzz(static_cast<unsigned>(i), .3f);
                }
            }
            if (o.ddParry) {
                const Vec3 t0 = p[0] + normalized(in.hands[0].aim) * .6f, t1 = p[1] + normalized(in.hands[1].aim) * .6f;
                const bool crossed = tracked[0] && tracked[1] && batonsCrossed(in.head, p[0], t0, p[1], t1);
                parry.step(dt, crossed);
                if (lost > 0 && parry.hit(parryWindow * perks.scale(h, Stat::parryWindow)) == ParryTiming::parry) {
                    const int e = nearestEnemy(head, in.list, in.count, parryCounterRange);
                    if (e >= 0)
                        out.blow(in.list[e].id, parryCounter(), in.list[e].chest, kindParry);
                    out.keepTopped = true; // the parried hit's health back (CANDIDATE: the game's own block is not ours)
                }
            }
        }
        // ---- CAP: SHIELD SLAM (the shield arm slammed down) ----
        if (h == hero_modes::captainAmerica && guard[gSlam].live(o.shieldSlam)) {
            const unsigned i = in.shieldHand < 2 ? in.shieldHand : 1u;
            if (tracked[i] && slam.step(dt, true, head, p[i], v[i])) {
                const Vec3 at{p[i].x, head.y - 1.6f, p[i].z};
                for (uint32_t e = 0; in.list && e < in.count; ++e)
                    if (!in.list[e].down && ok(in.list[e].chest) && length(in.list[e].chest - at) <= slamRadius * perks.scale(h, Stat::slamRadius))
                        out.blow(in.list[e].id, knocked(h, shieldSlamHit()), in.list[e].chest, kindShieldSlam);
                shock(at, slamRadius, true); // 0.0.72: a small faint ring, not the 3 m one
                if (o.fireDouse)
                    doused += fires.douse(Douse::shieldSlam, at, look, dt);
                out.buzz(i, 1.f);
                out.events |= 1u << kindShieldSlam;
            }
        }
        // ---- THE PUNISHER: the grapple hook from the chest rig ----
        if (h == hero_modes::punisher && guard[gHook].live(o.punisherHook)) {
            // Version 0.0.73 test 3 PUNISHER HOOK GRAB: either empty hand (test 2: the left only), at the rig's
            // generous chest zone anchored at the neck and scaled to the calibrated body (punisher_rig); one
            // launcher on the rig, so a hand draws it only while the other does not have it.
            holster::BodyYaw rigBody = body; // the rig's copy (the other heroes' body points stay unscaled)
            rigBody.scale = holster::scaleFor(in.eyeHeight);
            for (unsigned i = 0; i < 2; ++i) {
                const bool otherHas = hooks[1 - i].state != HookLauncher::stowed;
                const bool rig = tracked[i] && !otherHas && punisher_rig::at(p[i], in.head, rigBody);
                if (hooks[i].stepTrigger(dt, true, in.hands[i].grip, in.hands[i].trigger, rig, in.gunInHand[i], in.handBusy[i])) {
                    // Version 0.0.72: the line is the bridge's real one (game_xr, to the raycast's hit);
                    // 0.0.71's 12 m stand-in line to nowhere is gone.
                    out.grapple[i] = true;
                    out.buzz(i, .7f);
                }
                out.hookOut[i] = hooks[i].state != HookLauncher::stowed;
            }
        }
        // ---- FIRE DOUSE (the mod's own FireSpots; the game's fire hook is CANDIDATE) ----
        if (guard[gDouse].live(o.fireDouse))
            douseStep(dt, h, in, p, v, tracked, look, out);
        // ---- GREEN LANTERN ----
        if (h == hero_modes::greenLantern && guard[gLantern].live(true))
            lanternStep(dt, in, p, v, tracked, look, out);
        // ---- THOR ----
        if (h == hero_modes::thor && guard[gThor].live(true))
            thorStep(dt, in, p, v, tracked, look, out);
        // ---- HULK RAGE on the mod's own blows ----
        if (h == hero_modes::hulk)
            for (uint32_t k = 0; k < out.blowCount; ++k)
                out.blows[k].blow = hulk.scaled(out.blows[k].blow);
        // ---- ULTIMATE (max reputation or TEST UNLOCKS; both fists crossed on the chest) ----
        if (guard[gUltimate].live(o.ultimates)) {
            const bool crossed = tracked[0] && tracked[1] &&
                                 fistsCrossed(holster::bodyPoint(p[0], head, body), holster::bodyPoint(p[1], head, body));
            testUltimatesNow = testUltimates(o.testUnlocks);
            const bool unlocked = ultimateUnlocked(h, points, testUltimatesNow);
            if (ultimate.step(dt, h, unlocked, true, crossed, in.hands[0].grip, in.hands[1].grip, perks.scale(h, Stat::cooldown))) {
                out.ultimateFired = true;
                out.buzz(0, 1.f);
                out.buzz(1, 1.f);
                shock(head + Vec3{0, -1.6f, 0}, ultimateOf(h).radius);
            }
            while (ultimate.hit(h)) {
                const Ultimate& u = ultimateOf(h);
                if (h == hero_modes::thor) { // THOR's ultimate: each of its strikes
                    strike(out);
                    ++strikes;
                }
                for (uint32_t e = 0; in.list && e < in.count; ++e)
                    if (!in.list[e].down && ok(in.list[e].chest) && length(in.list[e].chest - head) <= u.radius)
                        out.blow(in.list[e].id, u.blow, in.list[e].chest, kindUltimate);
                if (u.hits <= 1)
                    break;
            }
        }
        // ---- IRON MAN HUD ----
        if (h == hero_modes::ironMan && guard[gIron].live(o.ironHud)) {
            iron.step(dt, fight, in.lockId, in.lockAt, near, in.flying, in.speed, in.altitude);
            out.look.hud = true;
            out.look.hudEye = head;
            out.look.hudForward = look;
            out.look.hudUp = in.head.orientation.rotate(Vec3{0, 1, 0});
            iron.lines(out.look.hudText, out.look.hudAlpha);
            out.look.lock = iron.lockedId != 0 && ok(iron.lockedAt);
            out.look.lockAt = iron.lockedAt;
        }
        // Every blow's numbers checked once more: an insane one trips its feature.
        for (uint32_t k = 0; k < out.blowCount; ++k) {
            const auto& b = out.blows[k].blow;
            if (!std::isfinite(b.amount) || !std::isfinite(b.knockAmount) || !ok(out.blows[k].at)) {
                guard[kindGuard(out.blows[k].kind)].trip("a blow came out insane");
                out.blowCount = 0;
                break;
            }
        }
        blowsOut += out.blowCount;
        finishLook(out);
    }

    // The PUNISHER's gun shot landed on an enemy at `at` from `from` (hero007's blows): blood while raging.
    void shot(Vec3 at, Vec3 from, float floorY, uint32_t bloodMode) {
        if (rage.raging && ok(at) && ok(from))
            blood.shot(bloodMode, at, at - from, floorY);
    }
    // The PUNISHER's shots while raging hit rageShotScale harder.
    float shotScale() const { return rage.raging ? rageShotScale : 1.f; }
    bool raging() const { return rage.raging; }

    // "REP 512 LOCAL HERO | PERKS 2/12 POINTS 0 | SUIT: ..." (the hologram's status line).
    void status(char* out, size_t n, bool allSuits, bool ownSuit) const {
        char r[48], pk[48], s[96];
        rep.text(r, sizeof r);
        perks.text(heroWas < hero_modes::heroes ? heroWas : 0u, rep.points, false, pk, sizeof pk);
        if (ownSuit)
            std::snprintf(s, sizeof s, "SUIT: YOUR OWN OR THE GAME'S");
        else
            suits.line(heroWas < hero_modes::heroes ? heroWas : 0u, suits.current(heroWas < hero_modes::heroes ? heroWas : 0u, allSuits), allSuits, s, sizeof s);
        std::snprintf(out, n, "%s | %s | %s", r, pk, s);
    }
    // The progress kept for good (reputation, suits, perks): one text, key=value lines.
    void save(char* out, size_t n) const {
        if (!out || n == 0)
            return;
        out[0] = 0;
        size_t at = 0;
        auto put = [&](auto&& writer) {
            if (at + 1 < n) {
                writer(out + at, n - at);
                at += std::strlen(out + at);
            }
        };
        put([&](char* o, size_t m) { rep.save(o, m); });
        put([&](char* o, size_t m) { suits.save(o, m); });
        put([&](char* o, size_t m) { perks.save(o, m); });
    }
    void load(const char* text) {
        if (!text)
            return;
        rep.load(text);
        suits.load(text);
        perks.load(text);
        progressDirty = false;
    }

    // THE CHAIN MOVE (9:16): a chain move's first hit on firstId at firstAt carries on to the nearest
    // standing enemies (each within chainJump of the last) until the hero's cap is hit: 3 enemies in
    // all from the start, 4-6 with the CHAIN perks. game_xr calls it for the hero007 moves (batarang,
    // baton throw, shield throw, gun shot, lunge, heat vision, repulsor); Play for its own (THOR, GL).
    // SPIDER-MAN (vanilla), SUPERMAN, IRON MAN, the PUNISHER (9:26): nothing. Returns the extra blows queued.
    uint32_t carry(uint32_t h, uint64_t firstId, Vec3 firstAt, sm2_damage::Blow b, const BatonEnemy* list, uint32_t count,
                   PlayOut& out, uint32_t kind) {
        if (!hasChain(h) || !firstId || !ok(firstAt) || !std::isfinite(b.amount) || !guard[gChain].live(true))
            return 0;
        uint64_t hit[chainMost]{firstId};
        Vec3 from = firstAt;
        uint32_t extra = 0;
        const uint32_t cap = std::min(perks.chainCap(h), chainMost);
        for (uint32_t k = 1; k < cap; ++k) {
            const int e = nextChain(from, list, count, hit);
            if (e < 0)
                break;
            hit[k] = list[e].id;
            from = list[e].chest;
            out.blow(list[e].id, b, from, kind);
            ++extra;
        }
        chainHits += extra;
        return extra;
    }
    // How many enemies the hero's chain move hits in all (the hologram; 1: no chain).
    uint32_t chainCap(uint32_t h) const { return perks.chainCap(h); }
    // The PERK TREE lines for the settings page (4 lines; 0 for Spider-Man).
    uint32_t tree(uint32_t h, char lines[4][hero072::treeLine]) const {
        const uint32_t k = h < hero_modes::heroes ? h : 0u;
        return perks.tree(k, ultimateUnlocked(k, k == heroWas ? rep.points : rep.heroPoints[k], testUltimatesNow), lines);
    }
    uint64_t chainHits{};
    bool testUltimatesNow{};

  private:
    sm2_damage::Blow knocked(uint32_t h, sm2_damage::Blow b) const {
        b.knockAmount *= perks.scale(h, Stat::knockback);
        return b;
    }
    static const Ultimate& ultimateOf(uint32_t h) { return spidy::hero072::ultimate(h); }
    sm2_damage::Blow scaled(uint32_t h, sm2_damage::Blow b, Stat s) const {
        b.amount *= perks.scale(h, s);
        return b;
    }
    static uint32_t kindGuard(uint32_t kind) {
        switch (kind) {
        case kindUppercut: return gUpper;
        case kindShove: return gShove;
        case kindSmoke: return gSmoke;
        case kindParry: return gDare;
        case kindShieldSlam: return gSlam;
        case kindFist:
        case kindBeam:
        case kindChain: return gLantern;
        case kindUltimate: return gUltimate;
        default: return gThor;
        }
    }
    static int touched(Vec3 at, const BatonEnemy* list, uint32_t count) { return nearestEnemy(at, list, count, touchRange); }
    void shock(Vec3 at, float radius, bool subtle = false) {
        shockSubtle = subtle; // Version 0.0.72 SUBTLE RING (CAP's slam)
        shockShow = .6f;
        shockAt = at;
        shockRadius = radius;
    }
    void douseStep(float dt, uint32_t h, const PlayIn& in, const Vec3* p, const Vec3* v, const bool* tracked, Vec3 look, PlayOut& out) {
        const Douse d = douseFor(h);
        bool sprayed = false;
        Vec3 from = head(in), dir = look;
        switch (d) {
        case Douse::foamPalm:
            sprayed = tracked[0] && palm.step(dt, in.hands[0].grip, in.hands[0].aim, look);
            from = p[0];
            dir = in.hands[0].aim;
            out.look.sprayKind = 0;
            break;
        case Douse::constructHose:
            sprayed = tracked[0] && tracked[1] && hose.step(dt, in.hands[0].grip, in.hands[1].grip, head(in), look, p[0], p[1]);
            from = (p[0] + p[1]) * .5f;
            out.look.sprayKind = 2;
            break;
        case Douse::hammerGust:
        case Douse::batonFan:
            sprayed = tracked[1] && spin.step(dt, v[1], d == Douse::batonFan || !hammerRaised(head(in), p[1], in.hands[1].aim));
            from = p[1];
            out.look.sprayKind = 3;
            break;
        case Douse::thunderClap:
            if (tracked[0] && tracked[1] && clap.step(dt, p[0], p[1])) {
                shock((p[0] + p[1]) * .5f, douseShape(d).reach);
                doused += fires.douse(d, (p[0] + p[1]) * .5f, look, dt);
                out.buzz(0, .8f);
                out.buzz(1, .8f);
            }
            return;
        default: return; // web balls, cold breath, the foam grenade, the slam: their own paths
        }
        if (sprayed) {
            out.look.spray = true;
            out.look.sprayFrom = from;
            out.look.sprayDir = ok(dir) && length(dir) > 1e-3f ? normalized(dir) : look;
            out.look.sprayReach = douseShape(d).reach;
            out.look.sprayTime = clock;
            doused += fires.douse(d, from, out.look.sprayDir, dt);
        }
    }
    static Vec3 head(const PlayIn& in) { return in.head.position; }
    void lanternStep(float dt, const PlayIn& in, const Vec3* p, const Vec3* v, const bool* tracked, Vec3 look, PlayOut& out) {
        const PlayOptions& o = in.o;
        const uint32_t h = hero_modes::greenLantern;
        bool used = false;
        // CONSTRUCT FIST: the right grip held, a fast punch along the look.
        const float drainScale = 1.f / perks.scale(h, Stat::chargeDrain);
        if (o.glFist && tracked[1] && fist.step(dt, in.hands[1].grip, v[1], look) && charge.spend(fistCost * drainScale, o.glCharge)) {
            used = true;
            fistShow = .45f;
            fistAt = p[1] + look * fistReach;
            fistDir = look;
            const int e = nearestEnemy(p[1] + look * (fistReach * .6f), in.list, in.count, fistReach * .6f * perks.scale(h, Stat::fistSize));
            if (e >= 0)
                out.blow(in.list[e].id, constructFist(), in.list[e].chest, kindFist);
            out.buzz(1, .8f);
        }
        // RING BEAM: the right trigger held (a basic projectile).
        beamTick = std::max(0.f, beamTick - dt);
        if (o.glBeam && tracked[1] && in.hands[1].trigger >= .5f && charge.can(beamDrain * drainScale * dt, o.glCharge)) {
            charge.spend(beamDrain * drainScale * dt, o.glCharge);
            used = true;
            const Vec3 aim = normalized(in.hands[1].aim);
            float along = 0; // test build 12: straight along the aim, stopping on the man it hits
            const int e = lineTarget(p[1], aim, in.list, in.count, beamRange, &along);
            const bool inReach = e >= 0;
            out.look.beam = true;
            out.look.beamFrom = p[1];
            out.look.beamTo = p[1] + aim * (inReach ? along : beamRange);
            if (inReach && beamTick <= 0) {
                beamTick = beamTickEvery;
                out.blow(in.list[e].id, ringBeamTick(), in.list[e].chest, kindBeam);
            }
            out.buzz(1, .15f);
        }
        // CHAIN CONSTRUCT: the left trigger pressed (a basic projectile that yanks).
        const bool press = in.hands[0].trigger >= .6f;
        chainShow = std::max(0.f, chainShow - dt);
        const float drain = 1.f / perks.scale(h, Stat::chargeDrain);
        if (o.glChain && tracked[0] && press && !chainWas && charge.spend(chainCost * drain, o.glCharge)) {
            used = true;
            const Vec3 aim = normalized(in.hands[0].aim);
            float along = 0; // test build 12: straight along the aim
            const int e = lineTarget(p[0], aim, in.list, in.count, chainRange, &along);
            chainShow = chainSeconds;
            if (e >= 0) {
                chainTo = p[0] + aim * along;
                out.blow(in.list[e].id, chainGrab(), in.list[e].chest, kindChain);
                carry(h, in.list[e].id, in.list[e].chest, chainGrab(), in.list, in.count, out, kindChain);
            } else {
                chainTo = p[0] + aim * chainRange;
            }
            out.buzz(0, .6f);
        }
        chainWas = press;
        if (chainShow > 0) {
            out.look.chain = true;
            out.look.chainFrom = p[0];
            out.look.chainTo = chainTo;
        }
        // SHIELD BUBBLE: both grips, the hands pulled apart; up while held and charged.
        if (o.glBubble && bubble.step(dt, in.hands[0].grip, in.hands[1].grip, p[0], p[1]) &&
            charge.can(bubbleDrain * drainScale / perks.scale(h, Stat::bubbleStrength) * dt, o.glCharge)) {
            charge.spend(bubbleDrain * drainScale / perks.scale(h, Stat::bubbleStrength) * dt, o.glCharge);
            used = true;
            out.look.bubble = true;
            out.look.bubbleAt = head(in) + Vec3{0, -.5f, 0};
            out.look.bubbleRadius = bubbleRadius;
            if (in.lostShare > 0)
                out.keepTopped = true; // the bubble takes the hit
        }
        charge.step(dt, used);
        for (unsigned i = 0; i < 2; ++i) {
            out.look.ring[i] = tracked[i] && i == 1;
            out.look.ringAt[i] = p[i];
        }
        if (o.glCharge && tracked[0]) {
            out.look.charge = true;
            out.look.chargeAt = p[0] + Vec3{0, .05f, 0};
            out.look.chargeLevel = charge.level / chargeMost;
            out.look.chargeEmpty = charge.empty;
        }
        if (fistShow > 0) {
            fistShow = std::max(0.f, fistShow - dt);
            out.look.fist = true;
            out.look.fistAt = fistAt;
            out.look.fistForward = fistDir;
            out.look.fistAlpha = std::clamp(fistShow / .45f, 0.f, 1.f);
        }
    }
    // ---- THOR (build 7: the user's "you cant actually throw thors hammer and theres no visual lightning") ----
    // Build 6's throw needed 2.6 m/s at the very frame the grip opened (a throw has slowed by then), and the
    // hammer then jumped 10 m away for 0.7 s and back to the back - nothing seen flying, nothing caught. Its
    // lightning was one frame's bolt (14 ms at 72 Hz) and only on an enemy hit: invisible. Now:
    //  - THROW: let go of the grip while swinging (the fastest hand speed of the last 0.15 s, at least
    //    throwMinSpeed): the hammer flies from the hand along the swing (straight at an enemy in the aim
    //    cone), spinning, lightning crackling on it; each enemy it passes is hit once and electrocuted.
    //  - RECALL: press the right grip while it is out - it turns and flies back to the hand; it also comes
    //    back on its own after throwSeconds / throwRange or its first hit. Caught with the grip held; not
    //    held, it goes back on the back.
    //  - LIGHTNING BLAST: the right trigger with the hammer in hand fires a bolt along the aim (a basic
    //    projectile: the user's rule for triggers) at the enemy in the aim cone or into the air.
    //  - STORM: the hammer held up overhead for 1 s: bolts rain from the sky around Thor (seen with no enemy).
    //  - MELEE: swings with the hammer in hand (down: a bolt, up: a launch, down on an airborne lock: a slam).
    // Every bolt stays boltSeconds, flickering: seen.
    void thorStep(float dt, const PlayIn& in, const Vec3* p, const Vec3* v, const bool* tracked, Vec3 look, PlayOut& out) {
        const PlayOptions& o = in.o;
        const uint32_t h = hero_modes::thor;
        const Vec3 hd = head(in);
        const bool grip = in.hands[1].grip >= holster::pressed || (gripWas && in.hands[1].grip > holster::released);
        const bool gripPressed = grip && !gripWas;
        // The swing's fastest velocity over the last moment (the release is often after the peak).
        swingAge += dt;
        // (A tracking jump over 25 m/s is no swing; the grip's press - the draw's reach back - starts it over.)
        if (tracked[1] && ok(v[1]) && length(v[1]) < 25.f && (length(v[1]) >= length(swingPeak) || swingAge > swingMemory)) {
            swingPeak = v[1];
            swingAge = 0;
        }
        if (gripPressed)
            swingPeak = {}, swingAge = 0;
        // DRAW: the right grip behind the right shoulder (the hammer on the back).
        if (tracked[1] && gripPressed && !hammerFlying) {
            const holster::BodyPoint bp = holster::bodyPoint(p[1], hd, body);
            if (bp.below <= .25f && bp.ahead <= .05f && bp.side >= -.05f) {
                hammerHeld = true;
                thorEvents |= thorDrawn;
            }
        }
        // RECALL: the grip pressed again while the hammer is out.
        if (hammerFlying && gripPressed && !hammerBack) {
            hammerBack = true;
            thorEvents |= thorRecalled;
        }
        // THROW: the grip opened while swinging.
        if (hammerHeld && gripWas && !grip) {
            hammerHeld = false;
            const float sp = length(swingPeak);
            if (tracked[1] && ok(swingPeak) && sp >= throwMinSpeed) {
                // Test build 10: along the pointing blended with the swing, a 12-degree assist.
                const Vec3 aim = ok(in.hands[1].aim) && length(in.hands[1].aim) > 1e-3f ? normalized(in.hands[1].aim) : Vec3{};
                Vec3 dir = throwLine(aim, swingPeak * (1.f / sp));
                hammerAim = 0;
                const int e = assistTarget(p[1], dir, in.list, in.count);
                if (e >= 0) {
                    dir = normalized(in.list[e].chest - p[1]);
                    hammerAim = in.list[e].id;
                }
                hammerFlying = true;
                hammerBack = false;
                hammerAge = 0;
                hammerPos = p[1];
                hammerFrom = p[1];
                hammerVel = dir * throwSpeed;
                for (auto& id : hammerHitIds)
                    id = 0;
                lastThrowSpeed = length(hammerVel);
                thorEvents |= thorThrown;
                ++throws;
                if (o.thorLightning) // the throw's crack of thunder
                    strike(out), ++strikes;
                out.buzz(1, .9f);
            }
        }
        gripWas = grip;
        // The hammer in the air.
        if (hammerFlying) {
            hammerAge += dt;
            const Vec3 was = hammerPos;
            hammerTurn = std::fmod(hammerTurn + length(hammerVel) * dt * spinPerMetre, 6.2831853f * 64.f);
            if (!hammerBack) {
                for (uint32_t k = 0; in.list && k < in.count && hammerAim; ++k) // turning toward its target
                    if (in.list[k].id == hammerAim && !in.list[k].down && ok(in.list[k].chest)) {
                        const Vec3 to = in.list[k].chest - hammerPos;
                        if (length(to) > .05f && length(hammerVel) > 1e-3f)
                            hammerVel = steer(normalized(hammerVel), normalized(to), homingTurn * dt) * throwSpeed;
                    }
                hammerPos = hammerPos + hammerVel * dt;
                // Version 0.0.73 PROP BOUNCE: off a wall, the floor or a car (and no more homing into it).
                if (in.hammerWall.known && in.hammerWall.serial == (throws & 15u) &&
                    prop_bounce::bounce(hammerPos, hammerVel, in.hammerWall)) {
                    hammerAim = 0;
                    ++hammerBounces;
                }
                if (hammerAge >= throwSeconds || length(hammerPos - hammerFrom) >= throwRange)
                    hammerBack = true;
            } else {
                const Vec3 to = (tracked[1] ? p[1] : hd) - hammerPos;
                const float d = length(to), stepLen = returnSpeed * dt;
                if (!(d > std::max(stepLen, catchRange)) || hammerAge > throwSeconds + 4.f) {
                    hammerFlying = false; // home: in the hand when the grip is held, else back on the back
                    hammerHeld = grip && tracked[1];
                    thorEvents |= hammerHeld ? thorCaught : thorStowed;
                    if (hammerHeld)
                        out.buzz(1, 1.f);
                } else {
                    // Test build 10: turning back smoothly, straight in near the hand, slowing to the catch.
                    const Vec3 want = to * (1.f / d);
                    const Vec3 now = length(hammerVel) > 1e-3f ? normalized(hammerVel) : want;
                    const Vec3 way = d <= returnStraight ? want : steer(now, want, returnTurn * dt);
                    const float speed = std::clamp(d * returnNear, returnSlowest, returnSpeed);
                    hammerVel = way * speed;
                    hammerPos = hammerPos + hammerVel * dt;
                }
            }
            // Each enemy it passes, once a throw: the hammer's hit and its electrocution (and the chain).
            for (uint32_t k = 0; hammerFlying && in.list && k < in.count; ++k) {
                const BatonEnemy& en = in.list[k];
                if (!en.id || en.down || !ok(en.chest) || segmentDistance(en.chest, was, hammerPos) > hammerHitRange)
                    continue;
                bool seen = false;
                for (uint64_t id : hammerHitIds)
                    seen = seen || id == en.id;
                if (seen)
                    continue;
                for (auto& id : hammerHitIds)
                    if (!id) {
                        id = en.id;
                        break;
                    }
                out.blow(en.id, hammerHit(), en.chest, kindHammer);
                thorEvents |= thorHammerHit;
                if (o.thorLightning) {
                    strike(out), ++strikes;
                    liveBolt(hammerPos + Vec3{0, 6.f, 0}, en.chest, boltSeconds);
                    if (o.thorChain) {
                        uint64_t chainIds[chainMost]{en.id};
                        Vec3 from = en.chest;
                        const uint32_t cap = std::min(perks.chainCap(h), chainMost);
                        for (uint32_t j = 1; j < cap; ++j) {
                            const int n = nextChain(from, in.list, in.count, chainIds);
                            if (n < 0)
                                break;
                            chainIds[j] = in.list[n].id;
                            out.blow(in.list[n].id, thorBolt(), in.list[n].chest, kindBolt);
                            liveBolt(from, in.list[n].chest, boltSeconds);
                            from = in.list[n].chest;
                        }
                    }
                }
                if (en.id == hammerAim)
                    hammerBack = true; // Mjolnir comes back after its target
            }
        }
        hammerOut = hammerFlying ? 1.f : 0.f;
        // LIGHTNING BLAST: the right trigger with the hammer in hand.
        blastCool = std::max(0.f, blastCool - dt);
        const bool trig = in.hands[1].trigger >= .5f;
        if (o.thorLightning && hammerHeld && tracked[1] && trig && !blastWas && blastCool <= 0) {
            blastCool = blastEvery;
            const Vec3 aim = ok(in.hands[1].aim) && length(in.hands[1].aim) > 1e-3f ? normalized(in.hands[1].aim) : look;
            const Vec3 from = p[1] + aim * .35f; // the hammer's head
            float along = 0; // test build 12: straight along the aim
            const int e = lineTarget(p[1], aim, in.list, in.count, blastRange, &along);
            Vec3 to = p[1] + aim * blastRange;
            if (e >= 0) {
                to = p[1] + aim * along;
                out.blow(in.list[e].id, thorBolt(), in.list[e].chest, kindBolt);
                if (o.thorChain)
                    carry(h, in.list[e].id, in.list[e].chest, thorBolt(), in.list, in.count, out, kindBolt);
            }
            liveBolt(from, to, boltSeconds);
            liveBolt(from, to, boltSeconds * .7f);
            strike(out), ++strikes;
            thorEvents |= thorBlast;
            out.buzz(1, .8f);
        }
        blastWas = trig;
        // MELEE: swings with the hammer in hand.
        if (hammerHeld && tracked[1]) {
            const Swing s = swing.step(dt, true, v[1]);
            if (s != Swing::none) {
                const int e = nearestEnemy(p[1], in.list, in.count, hammerReach);
                int locked = -1;
                for (uint32_t k = 0; in.list && k < in.count && in.lockId; ++k)
                    if (in.list[k].id == in.lockId && !in.list[k].down)
                        locked = static_cast<int>(k);
                const bool airborne = locked >= 0 && inAir(in.list[locked].chest.y - 1.2f, hd.y - 1.7f);
                const ThorAct act = thorAct(s, e >= 0, airborne);
                const int t = act == ThorAct::slam ? locked : e;
                if (t >= 0 && act != ThorAct::none) {
                    const BatonEnemy& en = in.list[t];
                    out.blow(en.id, hammerHit(), en.chest, kindHammer);
                    const sm2_damage::Blow extra = act == ThorAct::bolt ? thorBolt() : act == ThorAct::launch ? thorLaunch() : thorSlam();
                    out.blow(en.id, extra, en.chest,
                             act == ThorAct::bolt ? kindBolt : act == ThorAct::launch ? kindLaunch : kindSlam);
                    if (act != ThorAct::launch) {
                        liveBolt(en.chest + Vec3{0, 12.f, 0}, en.chest, boltSeconds);
                        strike(out); // the bolt (and its LIGHTNING CHAIN) / the slam: one strike
                        ++strikes;
                    }
                    if (act == ThorAct::bolt && o.thorChain) // LIGHTNING CHAIN: the bolt jumps on
                        carry(h, en.id, en.chest, thorBolt(), in.list, in.count, out, kindBolt);
                    thorEvents |= thorMelee;
                    out.buzz(1, 1.f);
                }
            }
        }
        // STORM: the hammer raised up overhead for a second.
        if (o.thorStorm && hammerHeld && tracked[1] && storm.step(dt, hammerRaised(hd, p[1], in.hands[1].aim))) {
            stormLeftStrikes = static_cast<uint32_t>(stormStrikes * perks.scale(h, Stat::lightningSeconds) + .5f);
            stormNext = 0;
            thorEvents |= thorStorm;
            out.buzz(1, 1.f);
        }
        stormNext -= dt;
        if (stormLeftStrikes > 0 && stormNext <= 0) {
            --stormLeftStrikes;
            stormNext = stormEvery;
            strike(out); // each storm strike, one sound
            ++strikes;
            const float r = stormRadius * perks.scale(h, Stat::stormRadius);
            uint32_t hits = 0;
            for (uint32_t e = 0; in.list && e < in.count; ++e)
                if (!in.list[e].down && ok(in.list[e].chest) && length(in.list[e].chest - hd) <= r) {
                    out.blow(in.list[e].id, stormHit(), in.list[e].chest, kindStorm);
                    liveBolt(in.list[e].chest + Vec3{0, 15.f, 0}, in.list[e].chest, boltSeconds);
                    ++hits;
                }
            // Seen with no enemy too: bolts from the sky onto the ground around Thor (2-5 m out).
            for (uint32_t k = hits; k < 3; ++k) {
                const float ang = rand01() * 6.2831853f, dist = 2.f + rand01() * 3.f;
                const Vec3 ground{hd.x + std::cos(ang) * dist, hd.y - 1.6f, hd.z + std::sin(ang) * dist};
                liveBolt(ground + Vec3{(rand01() - .5f) * 3.f, 16.f, (rand01() - .5f) * 3.f}, ground, boltSeconds);
            }
            if (tracked[1]) // and one onto the raised hammer
                liveBolt(p[1] + Vec3{0, 14.f, 0}, p[1] + Vec3{0, .3f, 0}, boltSeconds);
            shock(hd + Vec3{0, -1.6f, 0}, r);
        }
        // The look: the hammer in the hand, flying, or SHEATHED on his back (head down, the handle's end at
        // the right shoulder blade - exactly where the right grip reaches to draw it; build 7: not drawn
        // there, it is behind the head and out of view, and it cost 150,000 vertices a frame).
        out.look.hammerChoice = o.thorHammer;
        if (hammerHeld && tracked[1] && in.hands[1].hammer.ok) { // test build 10: in the fist (thor_grip.hpp)
            out.look.hammer = true;
            out.look.hammerInHand = true;
            out.look.hammerAt = in.hands[1].hammer.at;
            out.look.hammerForward = in.hands[1].hammer.handle;
            out.look.hammerUp = in.hands[1].hammer.face;
            out.look.hammerSpin = 0;
        } else if (hammerHeld && tracked[1]) { // no grip pose: along the aim, as before
            const Vec3 aim = ok(in.hands[1].aim) && length(in.hands[1].aim) > 1e-3f ? normalized(in.hands[1].aim) : look;
            out.look.hammer = true;
            out.look.hammerAt = p[1] - aim * hammerGrip;
            out.look.hammerForward = aim;
            out.look.hammerUp = faceOf(aim, look);
            out.look.hammerSpin = 0;
        } else if (hammerFlying && ok(hammerPos)) {
            const Vec3 way = ok(hammerVel) && length(hammerVel) > 1e-3f ? normalized(hammerVel) : look;
            out.look.hammer = true;
            out.look.hammerAt = hammerPos;
            out.look.hammerForward = way;
            out.look.hammerUp = faceOf(way, look);
            out.look.hammerSpin = hammerTurn;
            // Test build 10: the image carries the flight on to its own time (no farther than the hand).
            out.look.hammerVel = hammerVel;
            out.look.hammerSpinRate = length(hammerVel) * spinPerMetre;
            out.look.hammerAheadMost = hammerBack ? std::max(0.f, length((tracked[1] ? p[1] : hd) - hammerPos) - catchRange)
                                                  : std::max(0.f, throwRange - length(hammerPos - hammerFrom));
        } else if (ok(hd)) {
            const Vec3 f = body.forward(), r = body.right();
            out.look.hammer = true;
            out.look.hammerAt = sheathAt(hd, f, r);
            out.look.hammerForward = normalized(Vec3{0, -1, 0} - f * .15f);
            out.look.hammerUp = r;
            out.look.hammerSpin = 0;
            out.look.hammerSheathed = true;
        }
        // Test build 10 AIM HINT: winding up with the hammer in hand, a faint line along where it would fly.
        if (hammerHeld && tracked[1] && grip && ok(v[1]) && length(v[1]) > hintSpeed && length(v[1]) < 25.f) {
            const Vec3 aim = ok(in.hands[1].aim) && length(in.hands[1].aim) > 1e-3f ? normalized(in.hands[1].aim) : Vec3{};
            const Vec3 dir = throwLine(aim, normalized(v[1]));
            const int e = assistTarget(p[1], dir, in.list, in.count);
            out.look.aimHint = true;
            out.look.aimHintFrom = p[1] + dir * .35f;
            out.look.aimHintTo = e >= 0 ? in.list[e].chest : p[1] + dir * hintLength;
            out.look.aimHintLocked = e >= 0;
            out.look.aimHintAlpha = std::clamp((length(v[1]) - hintSpeed) / 2.f, 0.f, 1.f);
        }
        // The bolts alive (each flickers: a new jag each frame), then the crackle on the hammer.
        for (uint32_t k = 0; k < liveBoltsMost; ++k) {
            LiveBolt& b = liveBolts[k];
            if (b.left <= 0)
                continue;
            b.left -= dt;
            addBolt(out, b.from, b.to);
        }
        if (o.thorLightning && hammerFlying && ok(hammerPos)) // crackling on the flying hammer
            for (int k = 0; k < 2; ++k)
                addBolt(out, hammerPos, hammerPos + Vec3{rand01() - .5f, rand01() - .5f, rand01() - .5f} * .9f);
        else if (o.thorLightning && hammerHeld && tracked[1] && (fight.on || in.hands[1].trigger > .15f))
            addBolt(out, p[1], p[1] + Vec3{std::sin(clock * 13.f) * .25f, .2f, std::cos(clock * 11.f) * .25f});
    }
  public:
    static constexpr float boltsSeenSeconds() { return boltSeconds; }
    bool hammerInHandOrOut() const { return hammerHeld || hammerFlying; }
    bool hammerInAir() const { return hammerFlying; }
    Vec3 hammerAirAt() const { return hammerPos; }
    Vec3 hammerAirVelocity() const { return hammerVel; }
    bool hammerGoingOut() const { return hammerFlying && !hammerBack; }
    uint32_t hammerBounces{}; // Version 0.0.73 PROP BOUNCE
    // THOR's events since the last take (game_xr logs them): bits of ThorEvent.
    enum ThorEvent : uint32_t { thorDrawn = 1, thorThrown = 2, thorRecalled = 4, thorCaught = 8, thorStowed = 16,
                                thorHammerHit = 32, thorBlast = 64, thorStorm = 128, thorMelee = 256 };
    uint32_t takeThorEvents() {
        const uint32_t e = thorEvents;
        thorEvents = 0;
        return e;
    }
    uint32_t throws{};
    float lastThrowSpeed{};

  private:
    struct LiveBolt {
        Vec3 from{}, to{};
        float left{};
    };
    static constexpr uint32_t liveBoltsMost = 8;
    LiveBolt liveBolts[liveBoltsMost]{};
    void liveBolt(Vec3 from, Vec3 to, float seconds) {
        if (!ok(from) || !ok(to))
            return;
        uint32_t best = 0;
        for (uint32_t k = 1; k < liveBoltsMost; ++k)
            if (liveBolts[k].left < liveBolts[best].left)
                best = k;
        liveBolts[best] = {from, to, seconds};
    }
    uint32_t boltRng = 0x2545f491u;
    float rand01() {
        boltRng ^= boltRng << 13;
        boltRng ^= boltRng >> 17;
        boltRng ^= boltRng << 5;
        return static_cast<float>(boltRng >> 8) * (1.f / 16777216.f);
    }
    static float segmentDistance(Vec3 q, Vec3 a, Vec3 b) {
        const Vec3 ab = b - a;
        const float l2 = dot(ab, ab);
        const float t = l2 > 1e-8f ? std::clamp(dot(q - a, ab) / l2, 0.f, 1.f) : 0.f;
        return length(q - (a + ab * t));
    }
    bool hammerFlying{}, hammerBack{}, blastWas{};
    Vec3 hammerPos{}, hammerVel{}, hammerFrom{}, swingPeak{};
    float hammerTurn{}; // test build 10: the flying hammer's tumble (radians, by distance flown)
    float hammerAge{}, swingAge{}, blastCool{};
    uint64_t hammerAim{};
    uint64_t hammerHitIds[chainMost]{};
    uint32_t thorEvents{};

  public:
    void seedLightning(uint32_t s) { lightningSeed = s ? s : 0x9e3779b9u; }
    // One lightning strike: one of the two sounds, at random.
    void strike(PlayOut& out) {
        lightningSeed ^= lightningSeed << 13;
        lightningSeed ^= lightningSeed >> 17;
        lightningSeed ^= lightningSeed << 5;
        uint8_t& n = out.lightning[(lightningSeed >> 7) & 1u];
        if (n < 255)
            ++n;
    }
    uint32_t strikes{}; // every THOR strike so far (the status page, the tests)

  private:
    static int nextChain(Vec3 from, const BatonEnemy* list, uint32_t count, const uint64_t* hit) {
        int best = -1;
        float bd = chainJump;
        for (uint32_t i = 0; list && i < count; ++i) {
            bool seen = false;
            for (uint32_t k = 0; k < chainMost; ++k)
                seen = seen || (hit[k] && hit[k] == list[i].id);
            if (list[i].down || !ok(list[i].chest) || seen)
                continue;
            const float d = length(list[i].chest - from);
            if (d < bd) {
                bd = d;
                best = static_cast<int>(i);
            }
        }
        return best;
    }
    void addBolt(PlayOut& out, Vec3 from, Vec3 to) {
        if (out.look.bolts >= boltsMost || !ok(from) || !ok(to))
            return;
        const uint32_t b = out.look.bolts++;
        out.look.boltFrom[b] = from;
        out.look.boltTo[b] = to;
        out.look.boltSeed[b] = clock * 7.f + static_cast<float>(b);
    }
    void finishLook(PlayOut& out) {
        const float dt = lastDt;
        if (shockShow > 0) {
            out.look.shock = true;
            out.look.shockAt = shockAt;
            out.look.shockRadius = shockRadius;
            out.look.shockSubtle = shockSubtle;
            out.look.shockAge = 1.f - shockShow / .6f;
            shockShow = std::max(0.f, shockShow - dt);
        }
        if (out.perch.light != lightNone && ok(out.perch.targetAt)) { // PERCH TAKEDOWN: over the target's head
            out.look.perchLight = true;
            out.look.perchRed = out.perch.light == lightRed;
            out.look.perchAt = out.perch.targetAt + Vec3{0, .75f, 0};
        }
        if (out.perch.rope) {
            out.look.rope = true;
            out.look.ropeFrom = out.perch.ropeFrom;
            out.look.ropeTo = out.perch.ropeTo;
        }
        out.look.any = out.look.perchLight || out.look.rope || out.look.hammer || out.look.bolts || out.look.ring[0] || out.look.ring[1] || out.look.beam ||
                       out.look.bubble || out.look.chain || out.look.fist || out.look.charge || out.look.hud ||
                       out.look.blood || out.look.spray || out.look.shock || out.look.hook;
    }
    bool chainWas{};
};

} // namespace spidy::hero072
