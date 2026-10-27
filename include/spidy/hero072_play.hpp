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
#include "spidy/hero072.hpp"
#include "spidy/hero072_progress.hpp"
#include "spidy/hero072_visual.hpp"
#include "spidy/holster.hpp"

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
    bool fireDouse = true, shieldSlam = true, punisherHook = true, clawClimb = true, shove = true;
    uint32_t faction{}, testUnlocks{};
    bool ultimates = true;
    uint32_t perkBranch{};
    bool ownSuit{};
    // From the combat38 values (ON first: 0 is ON; SURVIVAL and OWN SUIT: OFF first).
    static bool onFirst(uint32_t v) { return v == 0; }
    static bool offFirst(uint32_t v) { return v == 1; }
};
struct PlayHand {
    Vec3 position{}, aim{0, 0, -1};
    bool tracked{};
    float trigger{}, grip{};
};
struct PlayIn {
    float dt{};
    bool play{};
    uint32_t hero{};
    Pose head{};
    PlayHand hands[2]{};
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
    PlayOptions o{};
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
    float punchScale[2]{1, 1}; // SURVIVAL: an injured arm hits half
    float dazed{};             // SURVIVAL: a concussion's sway, 0-1
    RageTint tint{};
    uint32_t suit{};           // the suit in use (0: the game's own / the player's own mod)
    uint32_t faction{};        // the faction chosen (the status page)
    uint32_t factionWorn{};    // what the enemies wear: VANILLA until its models and the hook are in
    bool ultimateFired{};
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
inline constexpr float longRageSeconds = 4.f; // the PUNISHER's LONG RAGE: the rage kept after a fight

class Play {
  public:
    // One guard each (the status page lists the tripped ones).
    enum G : uint32_t { gUpper = 0, gShove, gSmoke, gDare, gIron, gRage, gRep, gSurvival, gDouse, gSlam, gHook, gLantern,
                        gThor, gUltimate, gChain, guards };
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
    ShieldSlam slam;
    HookLauncher hook;
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
    bool hammerHeld{}, gripWas{};
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
        // ---- BATMAN: SMOKE BLIND ----
        if (h == hero_modes::batman && guard[gSmoke].live(o.smokeBlind)) {
            BlindOut b;
            smoke.step(dt, in.puffs, in.puffAges, in.puffCount, in.puffLife, in.list, in.count, b);
            for (uint32_t k = 0; k < b.count; ++k)
                out.blow(b.ids[k], smokeStagger(), b.at[k], kindSmoke);
        }
        // ---- DAREDEVIL: BATON GRAPPLE (cast up fast) and PARRY (batons crossed at the face) ----
        if (h == hero_modes::daredevil && guard[gDare].live(true)) {
            if (o.ddGrapple) {
                const int i = cast.step(dt, head, p, v, tracked);
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
                shock(at, slamRadius);
                if (o.fireDouse)
                    doused += fires.douse(Douse::shieldSlam, at, look, dt);
                out.buzz(i, 1.f);
                out.events |= 1u << kindShieldSlam;
            }
        }
        // ---- THE PUNISHER: the grapple hook from the chest rig ----
        if (h == hero_modes::punisher && guard[gHook].live(o.punisherHook)) {
            for (unsigned i = 0; i < 1; ++i) { // the left hand (the right holds the gun)
                const bool chest = tracked[i] && atChest(holster::bodyPoint(p[i], head, body));
                if (hook.step(dt, true, in.hands[i].grip, chest, v[i], look)) {
                    out.grapple[i] = true;
                    hookShow = .5f;
                    hookFrom = p[i];
                    hookTo = p[i] + look * 12.f;
                    out.buzz(i, .7f);
                }
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
    uint32_t tree(uint32_t h, char lines[4][96]) const {
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
    void shock(Vec3 at, float radius) {
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
            const int e = pullTarget(p[1], aim, in.list, in.count);
            const bool inReach = e >= 0 && length(in.list[e].chest - p[1]) <= beamRange;
            out.look.beam = true;
            out.look.beamFrom = p[1];
            out.look.beamTo = inReach ? in.list[e].chest : p[1] + aim * beamRange;
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
            const int e = pullTarget(p[0], aim, in.list, in.count);
            chainShow = chainSeconds;
            if (e >= 0 && length(in.list[e].chest - p[0]) <= chainRange) {
                chainTo = in.list[e].chest;
                out.blow(in.list[e].id, chainGrab(), chainTo, kindChain);
                carry(h, in.list[e].id, chainTo, chainGrab(), in.list, in.count, out, kindChain);
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
    void thorStep(float dt, const PlayIn& in, const Vec3* p, const Vec3* v, const bool* tracked, Vec3 look, PlayOut& out) {
        const PlayOptions& o = in.o;
        const uint32_t h = hero_modes::thor;
        const Vec3 hd = head(in);
        // The hammer on the back: the right grip behind the right shoulder draws it, held while gripped.
        const bool grip = in.hands[1].grip >= holster::pressed || (gripWas && in.hands[1].grip > holster::released);
        if (tracked[1] && grip && !gripWas && hammerOut <= 0) {
            const holster::BodyPoint bp = holster::bodyPoint(p[1], hd, body);
            if (bp.below <= .25f && bp.ahead <= .05f && bp.side >= -.05f)
                hammerHeld = true;
        }
        // THROW (HAMMER CHAIN): let go while swinging fast: it flies at the enemy aimed at and jumps on.
        if (hammerHeld && gripWas && !grip) {
            hammerHeld = false;
            if (length(v[1]) >= swingSpeed) {
                const Vec3 aim = ok(v[1]) ? normalized(v[1]) : look;
                int e = pullTarget(p[1], aim, in.list, in.count);
                hammerOut = hammerFlight;
                hammerTo = p[1] + aim * 10.f;
                uint64_t hitIds[chainMost]{};
                for (uint32_t jump = 0; e >= 0 && jump < (o.thorChain ? perks.chainCap(h) : 1u); ++jump) {
                    hitIds[jump] = in.list[e].id;
                    out.blow(in.list[e].id, hammerHit(), in.list[e].chest, kindHammer);
                    if (jump == 0)
                        hammerTo = in.list[e].chest;
                    if (o.thorLightning && out.look.bolts < boltsMost)
                        addBolt(out, p[1], in.list[e].chest);
                    e = nextChain(in.list[e].chest, in.list, in.count, hitIds);
                }
                hammerOut += hammerFlight; // and back
            }
        }
        gripWas = grip;
        hammerOut = std::max(0.f, hammerOut - dt);
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
                    if (act != ThorAct::launch)
                        addBolt(out, en.chest + Vec3{0, 12.f, 0}, en.chest);
                    if (act == ThorAct::bolt && o.thorChain) // LIGHTNING CHAIN: the bolt jumps on
                        carry(h, en.id, en.chest, thorBolt(), in.list, in.count, out, kindBolt);
                    out.buzz(1, 1.f);
                }
            }
        }
        // STORM: the hammer raised straight up for a second.
        if (o.thorStorm && hammerHeld && tracked[1] && storm.step(dt, hammerRaised(hd, p[1], in.hands[1].aim))) {
            stormLeftStrikes = static_cast<uint32_t>(stormStrikes * perks.scale(h, Stat::lightningSeconds) + .5f);
            stormNext = 0;
            out.buzz(1, 1.f);
        }
        stormNext -= dt;
        if (stormLeftStrikes > 0 && stormNext <= 0) {
            --stormLeftStrikes;
            stormNext = stormEvery;
            const float r = stormRadius * perks.scale(h, Stat::stormRadius);
            for (uint32_t e = 0; in.list && e < in.count; ++e)
                if (!in.list[e].down && ok(in.list[e].chest) && length(in.list[e].chest - hd) <= r) {
                    out.blow(in.list[e].id, stormHit(), in.list[e].chest, kindStorm);
                    addBolt(out, in.list[e].chest + Vec3{0, 15.f, 0}, in.list[e].chest);
                }
            shock(hd + Vec3{0, -1.6f, 0}, r);
        }
        // The look: the hammer in the hand (or on its way), lightning on the arms in a fight.
        if (hammerHeld && tracked[1]) {
            out.look.hammer = true;
            out.look.hammerAt = p[1];
            out.look.hammerForward = ok(in.hands[1].aim) && length(in.hands[1].aim) > 1e-3f ? normalized(in.hands[1].aim) : look;
            out.look.hammerSpin = 0;
        } else if (hammerOut > 0) {
            out.look.hammer = true;
            out.look.hammerAt = hammerTo;
            out.look.hammerSpin = clock * 20.f;
        }
        if (o.thorLightning && fight.on && tracked[1] && out.look.bolts < boltsMost)
            addBolt(out, p[1], p[1] + Vec3{std::sin(clock * 13.f) * .25f, .2f, std::cos(clock * 11.f) * .25f});
    }
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
            out.look.shockAge = 1.f - shockShow / .6f;
            shockShow = std::max(0.f, shockShow - dt);
        }
        if (hookShow > 0) {
            out.look.hook = true;
            out.look.hookFrom = hookFrom;
            out.look.hookTo = hookTo;
            hookShow = std::max(0.f, hookShow - dt);
        }
        out.look.any = out.look.hammer || out.look.bolts || out.look.ring[0] || out.look.ring[1] || out.look.beam ||
                       out.look.bubble || out.look.chain || out.look.fist || out.look.charge || out.look.hud ||
                       out.look.blood || out.look.spray || out.look.shock || out.look.hook;
    }
    bool chainWas{};
};

} // namespace spidy::hero072
