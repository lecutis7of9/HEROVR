#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
//
// Version 0.0.7 HERO PROFILES: one frame of BATMAN, WOLVERINE, SUPERMAN, PUNISHER (and DAREDEVIL's
// BATON HOLSTERS) from the hands, the head and the enemies near - pure, so core_tests drive it the
// way game_xr.cpp does. game_xr sends the blows it returns on sm2_damage's CONFIRMED path, plays
// the sounds, buzzes the hands, and zeroes the grips holsterStep claims before web, zip and climb.
//  - holsterStep (at the grip arbitration, before the phone's pocket draw): PUNISHER's rack (left
//    hip pistol, right hip pistol, the sawed-off on the back), DAREDEVIL's (a baton over each
//    shoulder) when BATON HOLSTERS is ON, or CAPTAIN AMERICA's (the shield on the back, drawn by
//    the SHIELD ARM's hand only); any other hero: nothing claimed, nothing changes.
//  - step (in the combat block): the hero's weapons (hero007.hpp has the rules).
//  - draw: the frame's looks (hero007_visual.hpp) in world space.
// Pure; no game memory, no address.
#include "spidy/cape_glide.hpp"
#include "spidy/captain.hpp"
#include "spidy/hero007.hpp"
#include "spidy/hero007_visual.hpp"
#include "spidy/gun_grip.hpp"
#include "spidy/hero_modes.hpp"
#include "spidy/holster.hpp"
#include "spidy/sfx.hpp"

#include <cstring>

namespace spidy::hero007 {

struct PlayHand {
    Vec3 position{}, aim{0, 0, -1};
    bool tracked{};
    float trigger{};  // the hand's fire control (combat_input's button), 0-1
    float squeeze{};  // the grip, 0-1 (holsterStep)
    bool busy{};      // a web attached, climbing, the hologram's or the phone's (holsterStep)
    bool thrown{};    // DAREDEVIL: this hand's baton is in the air (no holstering it until it is back)
    // Version 0.0.7 PUNISHER GUN GRIP: a held gun from the controller's grip pose (gun_grip::held);
    // gunOk false: 0.0.62's placement along the aim, upright.
    Vec3 gunAt{}, gunForward{0, 0, -1}, gunUp{0, 1, 0};
    bool gunOk{};
    void holdGun(const gun_grip::Held& g) {
        gunOk = g.ok;
        gunAt = g.at;
        gunForward = g.forward;
        gunUp = g.up;
    }
};
struct PlayInput {
    float dt{};
    bool play{};
    uint32_t hero{};
    Pose head{};
    PlayHand hands[2]{};
    const BatonEnemy* list{};
    uint32_t count{};
    bool flying{};          // FLY MODE flying: SUPERMAN's triggers are flight's
    uint32_t chain{};       // BATARANG CHAIN value
    uint32_t reload{};      // EASY RELOAD value
    bool crossDraw{};       // HOLSTER HANDS EITHER HAND
    bool batonHolsters{};   // BATON HOLSTERS ON
    uint32_t shieldArm{};   // CAPTAIN AMERICA SHIELD ARM value (0 RIGHT, the default; 1 LEFT)
    bool shieldChain = true; // SHIELD CHAIN ON
    uint32_t shieldDamage{}; // SHIELD DAMAGE value (0: 35 HP)
};
enum BlowKind : uint32_t { kindBatarang = 0, kindClaw, kindHeat, kindFreeze, kindPistol, kindSawedOff, kindShield, kindShieldBash, blowKinds };
inline constexpr const char* blowKindNames[blowKinds] = {"BATARANG", "CLAWS",     "HEAT VISION", "COLD BREATH",
                                                         "PISTOL",   "SAWED-OFF", "SHIELD",      "SHIELD BASH"};
struct PlayBlow {
    uint64_t id{};
    sm2_damage::Blow blow{};
    Vec3 at{};
    uint32_t kind{};
};
inline constexpr uint32_t blowsMost = 16;
struct PlayOutput {
    PlayBlow blows[blowsMost]{};
    uint32_t blowCount{};
    float haptic[2]{};
    uint32_t sounds{};  // bit 1 << sfx::Id
    uint32_t claimed{}; // holsterStep: the hands whose grip is the rack's
    // Version 0.0.7 BATMAN GRAPPLE GUN: the hands whose trigger is held as a grapple (the press
    // began with no enemy aimed and the hand aimed above level); no batarang for that press.
    bool grapple[2]{};
    void blow(uint64_t id, sm2_damage::Blow b, Vec3 at, uint32_t kind) {
        if (id && blowCount < blowsMost)
            blows[blowCount++] = {id, b, at, kind};
    }
    void buzz(unsigned hand, float s) {
        if (hand < 2)
            haptic[hand] = std::max(haptic[hand], s);
    }
    void sound(sfx::Id id) { sounds |= 1u << id; }
};

// Where a holstered item sits on the body (tracking space): its grip, along it, its top.
inline void wornPose(uint32_t zone, Vec3 head, const holster::BodyYaw& body, Vec3& at, Vec3& forward, Vec3& up) {
    const Vec3 f = body.forward(), r = body.right(), down{0, -1, 0};
    switch (zone) {
    case holster::leftHip:
    case holster::rightHip: { // muzzle down, the grip up and back, the slide's top facing forward
        const float s = zone == holster::leftHip ? -1.f : 1.f;
        at = head + down * .78f + r * (s * .22f) - f * .02f;
        forward = normalized(down + f * .12f);
        up = f;
        break;
    }
    case holster::back: // the sawed-off across the back, the grip behind the right shoulder
        at = head + down * .12f + r * .14f - f * .17f;
        forward = normalized(down + r * -.65f);
        up = -f;
        break;
    default: { // a baton over each shoulder, crossed
        const float s = zone == holster::backLeft ? -1.f : 1.f;
        at = head + down * .10f + r * (s * .13f) - f * .16f;
        forward = normalized(down + r * (-s * .5f));
        up = -f;
        break;
    }
    }
}
// The enemies a ray of `radius` passes from .3 m to `range` (heatHits with another reach).
inline int rayHits(Vec3 from, Vec3 dir, float range, float radius, const BatonEnemy* list, uint32_t count, int* hit,
                   int most) {
    int n = 0;
    if (!finite(from) || !finite(dir) || length(dir) < 1e-4f || !list || !hit)
        return 0;
    const Vec3 d = normalized(dir);
    for (uint32_t i = 0; i < count && n < most; ++i) {
        const Vec3 c = list[i].chest;
        if (!list[i].id || list[i].down || !finite(c))
            continue;
        if (hero_modes::segmentGap(from + d * .3f, from + d * range, c - Vec3{0, 1.f, 0}, c + Vec3{0, .6f, 0}) <= radius)
            hit[n++] = static_cast<int>(i);
    }
    return n;
}
inline Vec3 gunUp(Vec3 aim) {
    const Vec3 a = length(aim) > 1e-4f ? normalized(aim) : Vec3{0, 0, -1};
    Vec3 u = Vec3{0, 1, 0} - a * a.y;
    return length(u) > 1e-3f ? normalized(u) : Vec3{0, 0, 1};
}

class Play {
  public:
    uint32_t hero = 0xffffffffu;
    hero_modes::Baton batarangs[2];
    uint32_t batarangThrowsSeen[2]{};
    bool batTrigWas[2]{}, grappling[2]{}; // Version 0.0.7 BATMAN GRAPPLE GUN: the press's kind
    Smoke smoke;
    Shells shells;
    HeatVision heat;
    bool freezing{}, heatWas{};
    Vec3 beamFrom{}, beamTo{};
    float beamFade{}, heatSoundIn{};
    hero_modes::BatonSwing claws[2];
    bool clawsOut[2]{};
    Healing healing;
    holster::Rack guns;   // PUNISHER: slots 0 left hip, 1 right hip (pistols), 2 back (the sawed-off)
    Gun armory[holster::maxSlots];
    holster::Rack batons; // DAREDEVIL BATON HOLSTERS: 0 back left, 1 back right
    // CAPTAIN AMERICA: the shield's rack (0 the back, only the SHIELD ARM's hand), the shield (a
    // Baton: throw, chain, return), its bash on the arm and SHIELD BLOCK's guess for game_xr.
    holster::Rack shieldRack;
    hero_modes::Baton shield;
    hero_modes::BatonSwing shieldBash;
    uint32_t shieldHand{1};      // the SHIELD ARM's hand index (1 the right hand: SHIELD ARM RIGHT)
    bool guarding{};             // on the arm, its face at the nearest enemy (captain::covers)
    captain::ShieldPose armPose; // this frame's pose on the arm (tracking space; ok false: not on it)
    captain::Block block;        // SHIELD BLOCK (EXPERIMENTAL, CANDIDATE): game_xr's health refund
    uint64_t shieldHits{}, shieldBashes{}, shieldCatches{};
    Vec3 lastHand[2]{}, handVelocity[2]{};
    bool haveLast[2]{};
    uint32_t gunStateWas[holster::maxSlots]{};
    uint64_t dryWas[holster::maxSlots]{};
    uint64_t blows{}, shots{}, shotHits{}, heatTicks{}, freezeTicks{}, clawHits{}, batarangHits{}, smokes{};

    // A hero change (or the first frame): everything back in its place. Returns true when it changed.
    bool setHero(uint32_t h) {
        if (h == hero)
            return false;
        hero = h;
        for (unsigned i = 0; i < 2; ++i) {
            batarangs[i] = {};
            batarangs[i].roll.state = 0x2545f491u + 977u * i;
            batarangThrowsSeen[i] = 0;
            batTrigWas[i] = grappling[i] = false;
            claws[i].reset();
            clawsOut[i] = false;
            haveLast[i] = false;
        }
        heat = {};
        healing = {};
        heatWas = freezing = false;
        beamFade = 0;
        smoke = {};
        shells = {};
        guns = {};
        guns.add(holster::leftHip, pistol);
        guns.add(holster::rightHip, pistol);
        guns.add(holster::back, sawedOff);
        for (uint32_t s = 0; s < guns.count; ++s) {
            armory[s].arm(guns.slots[s].item);
            gunStateWas[s] = ready;
            dryWas[s] = 0;
        }
        batons = {};
        batons.add(holster::backLeft, 0);
        batons.add(holster::backRight, 0);
        resetShield();
        return true;
    }
    // The shield back on the back, its rack for the SHIELD ARM's hand (a hero or arm change).
    void resetShield() {
        shieldRack = {};
        shieldRack.add(holster::back, 0, 1u << shieldHand);
        shield = {};
        shield.roll.state = 0x9e3779b9u;
        shieldBash.reset();
        guarding = false;
        armPose = {};
    }
    bool captainAmerica() const { return hero == hero_modes::captainAmerica; }
    // CAPTAIN AMERICA: is the shield in the SHIELD ARM's hand (drawn, not thrown)?
    bool shieldOnArm() const { return captainAmerica() && shield.inHandNow() && shieldRack.heldIn(shieldHand) >= 0; }
    bool punisher() const { return hero == hero_modes::punisher; }
    // DAREDEVIL with BATON HOLSTERS: may hand i use its baton (drawn)? Without holsters: always.
    bool batonDrawn(unsigned hand, bool holstersOn) const {
        return !holstersOn || hero != hero_modes::daredevil || batons.heldIn(hand) >= 0;
    }
    // game_xr draws this hand's baton (in the hand or in the air) when this is true; a worn one is
    // in draw()'s d.worn. Together never more than TWO batons (core_tests: every state).
    bool batonShown(unsigned hand, bool inHand, bool holstersOn) const {
        return !inHand || batonDrawn(hand, holstersOn);
    }

    // The racks, at the grip arbitration. Fills out.claimed, the draw / holster ticks and sounds.
    void holsterStep(const PlayInput& in, PlayOutput& out) {
        setHero(in.hero);
        if (captain::armHand(in.shieldArm) != shieldHand) { // SHIELD ARM changed: the shield to the back
            shieldHand = captain::armHand(in.shieldArm);
            resetShield();
        }
        holster::Rack* rack = punisher() ? &guns
                              : (hero == hero_modes::daredevil && in.batonHolsters) ? &batons
                              : captainAmerica()                                    ? &shieldRack
                                                                                    : nullptr;
        if (!rack) {
            guns.stowAll();
            batons.stowAll();
            shieldRack.stowAll();
            return;
        }
        holster::Hand hands[2];
        for (unsigned i = 0; i < 2; ++i)
            hands[i] = {in.hands[i].position, in.hands[i].tracked, in.hands[i].squeeze, in.hands[i].busy,
                        (rack == &batons && in.hands[i].thrown) ||
                            (rack == &shieldRack && i == shieldHand && !shield.inHandNow())};
        rack->crossDraw = in.crossDraw;
        rack->update(in.dt, in.play, in.head, hands);
        out.claimed = rack->claimed();
        for (unsigned i = 0; i < 2; ++i) {
            const uint32_t e = rack->events(i);
            if (e & (holster::drew | holster::holstered)) {
                out.buzz(i, holster::tickFor(e));
                out.sound(sfx::holsterTick);
            }
        }
    }

    void step(const PlayInput& in, PlayOutput& out) {
        setHero(in.hero);
        const float dt = std::isfinite(in.dt) ? std::clamp(in.dt, 0.f, .1f) : 0.f;
        for (unsigned i = 0; i < 2; ++i) { // the hands' speed (REALISTIC's flick)
            const Vec3 p = in.hands[i].position;
            if (in.hands[i].tracked && finite(p)) {
                if (haveLast[i] && dt > 1e-4f)
                    handVelocity[i] = handVelocity[i] * .4f + ((p - lastHand[i]) / dt) * .6f;
                lastHand[i] = p;
                haveLast[i] = true;
            } else {
                haveLast[i] = false;
                handVelocity[i] = {};
            }
        }
        smoke.update(dt);
        shells.update(dt);
        beamFade = std::max(0.f, beamFade - dt * 6.f);
        if (!in.play)
            return;
        switch (hero) {
        case hero_modes::batman: stepBatman(in, dt, out); break;
        case hero_modes::wolverine: stepWolverine(in, dt, out); break;
        case hero_modes::superman: stepSuperman(in, dt, out); break;
        case hero_modes::punisher: stepPunisher(in, dt, out); break;
        case hero_modes::captainAmerica: stepCaptain(in, dt, out); break;
        default: break;
        }
        blows += out.blowCount;
    }

    // This frame's looks, world space (worldFromTracking), into d (d.any false: nothing).
    // batonLook: BATON LOOK's value (0 CLASSIC, 1 CLUB, 2 MODEL: lookBatonModel, CLUB without the model).
    void draw(Draw& d, const Pose& wft, const PlayInput& in, uint32_t batonLook) const {
        d = Draw{};
        const auto W = [&](Vec3 p) { return compose(wft, Pose{p, {0, 0, 0, 1}}).position; };
        const auto R = [&](Vec3 v) { return wft.orientation.rotate(v); };
        const auto item = [&](uint32_t look, Vec3 at, Vec3 fwd, Vec3 up, uint32_t holsterKind = 0) {
            Item it;
            it.look = look;
            it.at = W(at);
            it.forward = R(fwd);
            it.up = R(up);
            it.holster = holsterKind;
            return it;
        };
        const auto handOk = [&](unsigned i) {
            return in.hands[i].tracked && finite(in.hands[i].position) && finite(in.hands[i].aim) &&
                   length(in.hands[i].aim) > .5f;
        };
        bool any = false;
        if (hero == hero_modes::batman) {
            for (unsigned i = 0; i < 2; ++i) {
                const auto& b = batarangs[i];
                if (b.inHandNow()) {
                    if (handOk(i)) {
                        const Vec3 a = normalized(in.hands[i].aim);
                        d.held[i] = item(lookBatarang, in.hands[i].position + a * .05f, a, gunUp(a));
                        any = true;
                    }
                } else if (finite(b.position)) {
                    const Vec3 way = length(b.velocity) > .1f ? normalized(b.velocity) : Vec3{0, 0, -1};
                    d.flying[i] = item(lookBatarang, b.position, way, Vec3{0, 1, 0});
                    d.flying[i].spin = b.out * 20.f;
                    any = true;
                }
            }
        } else if (hero == hero_modes::wolverine) {
            for (unsigned i = 0; i < 2; ++i)
                if (clawsOut[i] && handOk(i)) {
                    const Vec3 a = normalized(in.hands[i].aim);
                    d.held[i] = item(lookClaws, in.hands[i].position, a, gunUp(a));
                    any = true;
                }
        } else if (hero == hero_modes::punisher) {
            for (uint32_t s = 0; s < guns.count; ++s) {
                const auto& slot = guns.slots[s];
                const Gun& g = armory[s];
                const bool open = g.weapon == sawedOff && (g.state == opened || g.state == seating || g.state == autoLoading);
                const uint32_t look = g.weapon == sawedOff ? (open ? lookSawedOffOpen : lookSawedOff) : lookPistol;
                Vec3 at{}, fwd{}, up{};
                wornPose(slot.zone, in.head.position, guns.body, at, fwd, up);
                const uint32_t kind = slot.zone == holster::back ? 2u : 1u;
                if (slot.heldBy < 0) {
                    d.worn[s] = item(look, at, fwd, up, kind);
                } else {
                    d.worn[s] = item(lookNone, at, fwd, up, kind); // the empty holster stays on the body
                    const unsigned i = static_cast<unsigned>(slot.heldBy);
                    if (i < 2 && in.hands[i].tracked && in.hands[i].gunOk && finite(in.hands[i].gunAt)) {
                        // Version 0.0.7 GUN GRIP: in the palm from the grip pose, the barrel along the forearm.
                        d.held[i] = item(look, in.hands[i].gunAt, in.hands[i].gunForward, in.hands[i].gunUp);
                        d.held[i].palm = true;
                        ammoText(g, d.ammo[i], sizeof d.ammo[i]);
                    } else if (i < 2 && handOk(i)) {
                        const Vec3 a = normalized(in.hands[i].aim);
                        d.held[i] = item(look, in.hands[i].position, a, gunUp(a));
                        ammoText(g, d.ammo[i], sizeof d.ammo[i]);
                    }
                }
                any = true;
            }
        } else if (captainAmerica()) {
            const unsigned i = shieldHand;
            const Vec3 headF = in.head.orientation.rotate(Vec3{0, 0, -1});
            const Vec3 bodyF = shieldRack.body.valid ? shieldRack.body.forward() : headF;
            const Vec3 bodyR = shieldRack.body.valid ? shieldRack.body.right() : cross(headF, Vec3{0, 1, 0});
            if (!shield.inHandNow()) { // in the air: flat, spinning about its face
                if (finite(shield.position)) {
                    const Vec3 way = length(shield.velocity) > .1f ? normalized(shield.velocity) : bodyF;
                    d.flying[i] = item(lookShieldSpin, shield.position, Vec3{0, 1, 0}, way);
                    d.flying[i].spin = shield.out * 2 * 3.14159265f * captain::spinRevs;
                    d.flying[i].side = i;
                    any = true;
                }
            } else if (shieldRack.heldIn(i) >= 0) { // on the arm
                if (handOk(i)) {
                    const captain::ShieldPose p = captain::onArm(in.hands[i].position, in.hands[i].aim, headF, bodyR, i);
                    if (p.ok) {
                        d.held[i] = item(lookShield, p.at, p.face, p.up);
                        d.held[i].side = i;
                        any = true;
                    }
                }
            } else { // on the back
                const captain::ShieldPose p = captain::onBack(in.head.position, bodyF, bodyR, i);
                if (p.ok) {
                    d.worn[0] = item(lookShield, p.at, p.face, p.up);
                    d.worn[0].side = i;
                    any = true;
                }
            }
        } else if (hero == hero_modes::daredevil && in.batonHolsters) {
            for (uint32_t s = 0; s < batons.count; ++s) {
                Vec3 at{}, fwd{}, up{};
                wornPose(batons.slots[s].zone, in.head.position, batons.body, at, fwd, up);
                // The baton on the back when not drawn (game_xr draws a drawn one in the hand).
                const uint32_t look = batonLook == 2 ? lookBatonModel : batonLook == 1 ? lookClub : lookBaton062;
                d.worn[s] = item(batons.slots[s].heldBy < 0 ? look : lookNone, at, fwd, up, 3);
                d.worn[s].side = s < 2 ? s : 1;
                any = true;
            }
        }
        if (beamFade > 0 && finite(beamTo)) {
            const Vec3 f = in.head.orientation.rotate(Vec3{0, 0, -1});
            const Vec3 to = beamTo;
            if (freezing) {
                const Vec3 mouth = in.head.position + in.head.orientation.rotate(Vec3{0, -.09f, -.08f});
                d.beamFrom[0] = d.beamFrom[1] = W(mouth + normalized(to - mouth) * .2f);
            } else {
                for (int e = 0; e < 2; ++e) {
                    const Vec3 eye = in.head.position + in.head.orientation.rotate(Vec3{e ? .032f : -.032f, 0, -.06f});
                    d.beamFrom[e] = W(eye + f * .25f);
                }
            }
            d.beamTo = W(to);
            d.beamFade = beamFade;
            d.beamFreeze = freezing;
            any = true;
        }
        for (uint32_t k = 0; k < smokeMost && k < drawSmokes; ++k) {
            d.smokeAt[k] = W(smoke.puffs[k].at);
            d.smokeAge[k] = smoke.puffs[k].age;
            any = any || smoke.puffs[k].age < smokeSeconds;
        }
        for (uint32_t k = 0; k < shellsMost && d.shells < drawShells; ++k) {
            const auto& s = shells.list[k];
            if (s.age >= shellSeconds)
                continue;
            d.shellAt[d.shells] = W(s.at);
            d.shellWay[d.shells] = R(s.velocity);
            d.shellAge[d.shells] = s.age;
            d.shellShotgun[d.shells] = s.shotgun;
            ++d.shells;
            any = true;
        }
        d.any = any;
    }

  private:
    // CAPTAIN AMERICA: the SHIELD ARM's trigger throws the drawn shield (backhand: along a fast
    // forward sweep, else the aim, the aim assist's enemy first); it chains like the batons (SHIELD
    // CHAIN) and comes back to the hand. On the arm a swing of its rim is a bash.
    void stepCaptain(const PlayInput& in, float dt, PlayOutput& out) {
        const unsigned i = shieldHand;
        const PlayHand& h = in.hands[i];
        guarding = false;
        armPose = {};
        shield.chainChance = in.shieldChain ? 1.f : 0.f;
        const bool trig = h.tracked && h.trigger > .5f;
        const bool drawn = shieldRack.heldIn(i) >= 0;
        if (!finite(h.position) || !finite(h.aim)) {
            shieldBash.update(dt, false, {}, {}, in.list, in.count);
            return;
        }
        if (!drawn && shield.inHandNow()) { // on the back: the trigger throws nothing
            shield.wasDown = trig;
            shield.position = h.position;
            shieldBash.update(dt, false, {}, {}, in.list, in.count);
            return;
        }
        const Vec3 headF = in.head.orientation.rotate(Vec3{0, 0, -1});
        const Vec3 aim = captain::throwAim(handVelocity[i], h.aim, headF);
        const int aimed = shield.inHandNow() ? hero_modes::batonAim(h.position, aim, in.list, in.count) : -1;
        const uint32_t throwsWas = shield.throws, catchesWas = shield.catches + shield.timeouts;
        const bool hit = shield.update(dt, trig, h.position, aim, in.list, in.count, aimed);
        if (shield.throws != throwsWas) {
            out.buzz(i, .4f);
            out.sound(sfx::shieldThrow);
        }
        if (hit && shield.hitCount > 0) {
            const uint64_t who = shield.hitIds[shield.hitCount - 1];
            Vec3 at = shield.position;
            for (uint32_t e = 0; e < in.count; ++e)
                if (in.list[e].id == who)
                    at = in.list[e].chest;
            out.blow(who, captain::throwHit(in.shieldDamage), at, kindShield);
            out.buzz(i, .5f);
            out.sound(sfx::shieldRicochet);
            ++shieldHits;
        }
        if (shield.catches + shield.timeouts != catchesWas) {
            out.buzz(i, .6f);
            out.sound(sfx::shieldCatch);
            ++shieldCatches;
        }
        if (!shield.inHandNow() || !h.tracked) {
            shieldBash.update(dt, false, {}, {}, in.list, in.count);
            return;
        }
        const Vec3 bodyR = shieldRack.body.valid ? shieldRack.body.right() : cross(headF, Vec3{0, 1, 0});
        armPose = captain::onArm(h.position, h.aim, headF, bodyR, i);
        if (!armPose.ok) {
            shieldBash.update(dt, false, {}, {}, in.list, in.count);
            return;
        }
        const Vec3 centre = captain::centreOf(armPose, i);
        guarding = captain::covers(centre, armPose.face, in.list, in.count);
        const int bash = shieldBash.update(dt, true, centre - armPose.up * captain::bashHalf,
                                           centre + armPose.up * captain::bashHalf, in.list, in.count);
        if (bash >= 0) {
            out.blow(in.list[bash].id, captain::bashHit(), in.list[bash].chest, kindShieldBash);
            out.buzz(i, .7f);
            out.sound(sfx::shieldClang);
            ++shieldBashes;
        }
    }
    void stepBatman(const PlayInput& in, float dt, PlayOutput& out) {
        for (unsigned i = 0; i < 2; ++i) {
            auto& b = batarangs[i];
            b.chainChance = batarangChance(in.chain);
            const PlayHand& h = in.hands[i];
            const bool trig = h.tracked && h.trigger > .5f;
            if (!finite(h.position) || !finite(h.aim))
                continue;
            const int aimed = b.inHandNow() ? hero_modes::batonAim(h.position, h.aim, in.list, in.count) : -1;
            // Version 0.0.7 BATMAN GRAPPLE GUN: a press that starts with no enemy in the batarang's
            // sights and the hand aimed above level is the grapple's, held until the trigger lets go.
            if (trig && !batTrigWas[i])
                grappling[i] = cape_glide::grappleAim(h.aim, hero_modes::batonAim(h.position, h.aim, in.list, in.count) >= 0);
            if (!trig)
                grappling[i] = false;
            batTrigWas[i] = trig;
            if (grappling[i])
                out.grapple[i] = true;
            const uint32_t throwsWas = b.throws;
            const bool hit = b.update(dt, trig && !grappling[i], h.position, h.aim, in.list, in.count, aimed);
            if (b.throws != throwsWas) {
                out.buzz(i, .3f);
                out.sound(sfx::batonWhoosh);
            }
            batarangThrowsSeen[i] = b.throws;
            if (hit && b.hitCount > 0) {
                const uint64_t who = b.hitIds[b.hitCount - 1];
                Vec3 at = b.position;
                for (uint32_t e = 0; e < in.count; ++e)
                    if (in.list[e].id == who)
                        at = in.list[e].chest;
                out.blow(who, batarangHit(), at, kindBatarang);
                out.buzz(i, .5f);
                out.sound(sfx::batonHit);
                smoke.add(at);
                ++smokes;
                ++batarangHits;
            }
        }
    }
    void stepWolverine(const PlayInput& in, float dt, PlayOutput& out) {
        for (unsigned i = 0; i < 2; ++i) {
            const PlayHand& h = in.hands[i];
            const bool outNow = h.tracked && h.trigger > .5f && finite(h.position) && finite(h.aim);
            if (outNow && !clawsOut[i]) {
                out.sound(sfx::clawSnikt);
                out.buzz(i, .25f);
            }
            clawsOut[i] = outNow;
            Vec3 grip{}, tip{};
            clawShaft(h.position, h.aim, grip, tip);
            const int hit = claws[i].update(dt, outNow, grip, tip, in.list, in.count);
            if (hit >= 0) {
                out.blow(in.list[hit].id, clawHit(), in.list[hit].chest, kindClaw);
                out.buzz(i, .6f);
                out.sound(sfx::batonHit);
                ++clawHits;
            }
        }
    }
    void stepSuperman(const PlayInput& in, float dt, PlayOutput& out) {
        const bool both = !in.flying && in.hands[0].tracked && in.hands[1].tracked && in.hands[0].trigger > .5f &&
                          in.hands[1].trigger > .5f;
        const bool mouth = handAtMouth(in.hands[0].position, in.head) || handAtMouth(in.hands[1].position, in.head);
        if (!heat.firing)
            freezing = mouth; // chosen as it starts: a hand moving off the mouth does not flip it
        const bool tick = heat.update(dt, both);
        if (!heat.firing) {
            heatWas = false;
            return;
        }
        Vec3 from{}, dir{};
        if (!heatRay(in.head.position, in.head.orientation.rotate(Vec3{0, 0, -1}), from, dir))
            return;
        const int aimed = heatAim(from, dir, in.list, in.count);
        const float range = freezing ? freezeRange : heatRange;
        beamTo = aimed >= 0 ? in.list[aimed].chest : from + dir * range;
        if (aimed >= 0 && length(beamTo - from) > range)
            beamTo = from + dir * range;
        beamFade = 1;
        heatSoundIn -= dt;
        if (!heatWas || heatSoundIn <= 0) {
            out.sound(freezing ? sfx::freezeBreath : sfx::heatVision);
            heatSoundIn = freezing ? .65f : .55f;
        }
        heatWas = true;
        if (!tick)
            return;
        int hit[8];
        const int n = rayHits(from, dir, range, freezing ? freezeRadius : heatRadius, in.list, in.count, hit, 8);
        for (int k = 0; k < n; ++k)
            out.blow(in.list[hit[k]].id, freezing ? freezeTick() : heatTick(), in.list[hit[k]].chest,
                     freezing ? kindFreeze : kindHeat);
        (freezing ? freezeTicks : heatTicks) += 1;
        out.buzz(0, .15f);
        out.buzz(1, .15f);
    }
    void stepPunisher(const PlayInput& in, float dt, PlayOutput& out) {
        const Vec3 headFwd = in.head.orientation.rotate(Vec3{0, 0, -1});
        for (uint32_t s = 0; s < guns.count; ++s) {
            Gun& g = armory[s];
            const int heldBy = guns.slots[s].heldBy;
            const bool inHand = heldBy == 0 || heldBy == 1;
            const unsigned i = inHand ? static_cast<unsigned>(heldBy) : 0u;
            const PlayHand& h = in.hands[i];
            const bool trig = inHand && h.tracked && h.trigger > .5f;
            const bool flick = inHand && flickDown(handVelocity[i]);
            const bool hip = inHand && atHip(h.position, in.head.position, headFwd);
            const uint32_t was = g.state;
            const bool fired = g.update(dt, trig, in.reload, flick, hip);
            // Version 0.0.7 GUN GRIP: from the gun as it is drawn (the muzzle, along the barrel), so the
            // sights are on the shot; 0.0.62's along the aim without a usable grip pose.
            const gun_grip::Weapon gw = g.weapon == sawedOff ? gun_grip::sawedOff : gun_grip::pistol;
            const gun_grip::Held gh{h.gunAt, h.gunForward, h.gunUp, h.gunOk && finite(h.gunAt) && finite(h.gunForward) && finite(h.gunUp)};
            if (fired && gh.ok) {
                const Vec3 a = normalized(gh.forward);
                const Vec3 muzzle = gun_grip::from(gh, gun_grip::muzzleFromPalm(gw));
                float amount[our_ball_max]{};
                const uint32_t n = std::min(in.count, our_ball_max);
                const int hits = shoot(g.spec(), muzzle, a, in.list, n, amount);
                for (uint32_t e = 0; e < n; ++e)
                    if (amount[e] > 0)
                        out.blow(in.list[e].id, shotBlow(g.spec(), amount[e]), in.list[e].chest,
                                 g.weapon == sawedOff ? kindSawedOff : kindPistol);
                ++shots;
                shotHits += static_cast<uint64_t>(hits);
                out.sound(g.weapon == sawedOff ? sfx::shotgunBlast : sfx::pistolShot);
                out.buzz(i, g.weapon == sawedOff ? .95f : .7f);
                if (g.weapon == pistol)
                    shells.eject(gun_grip::from(gh, gun_grip::breechFromPalm(gw)), normalized(cross(a, gh.up)), false);
            } else if (fired && finite(h.position) && finite(h.aim) && length(h.aim) > .5f) {
                const Vec3 a = normalized(h.aim);
                const Vec3 muzzle = h.position + a * (g.weapon == sawedOff ? .38f : .16f) + gunUp(a) * .06f;
                float amount[our_ball_max]{};
                const uint32_t n = std::min(in.count, our_ball_max);
                const int hits = shoot(g.spec(), muzzle, a, in.list, n, amount);
                for (uint32_t e = 0; e < n; ++e)
                    if (amount[e] > 0)
                        out.blow(in.list[e].id, shotBlow(g.spec(), amount[e]), in.list[e].chest,
                                 g.weapon == sawedOff ? kindSawedOff : kindPistol);
                ++shots;
                shotHits += static_cast<uint64_t>(hits);
                out.sound(g.weapon == sawedOff ? sfx::shotgunBlast : sfx::pistolShot);
                out.buzz(i, g.weapon == sawedOff ? .95f : .7f);
                if (g.weapon == pistol) {
                    const Vec3 right = normalized(cross(a, gunUp(a)));
                    shells.eject(h.position + gunUp(a) * .06f, right, false);
                }
            }
            if (g.state != was) {
                const bool opening = g.state == opened || (g.state == autoLoading && was != autoLoading);
                if (opening) {
                    out.sound(g.weapon == sawedOff && g.state == autoLoading ? sfx::magSeat : sfx::magDrop);
                    if (g.weapon == sawedOff && gh.ok) { // Version 0.0.7 GUN GRIP: out of the drawn breech
                        const Vec3 breech = gun_grip::from(gh, gun_grip::breechFromPalm(gw)), f = normalized(gh.forward);
                        shells.eject(breech, -f + gh.up * .3f, true);
                        shells.eject(breech, -f + gh.up * .3f + cross(f, gh.up) * .1f, true);
                    } else if (g.weapon == sawedOff && finite(h.position)) { // both spent shells out of the breech
                        const Vec3 a = length(h.aim) > .5f ? normalized(h.aim) : Vec3{0, 0, -1};
                        shells.eject(h.position + gunUp(a) * .05f + a * .07f, -a + Vec3{0, .3f, 0}, true);
                        shells.eject(h.position + gunUp(a) * .05f + a * .07f, -a + Vec3{.1f, .3f, 0}, true);
                    }
                }
                if (g.state == ready && (was == seating || was == autoLoading)) {
                    out.sound(sfx::magSeat);
                    if (inHand)
                        out.buzz(i, .35f);
                }
            }
            if (g.dryPulls != dryWas[s]) {
                dryWas[s] = g.dryPulls;
                out.sound(sfx::dryClick);
            }
            gunStateWas[s] = g.state;
        }
    }
    static constexpr uint32_t our_ball_max = 32;
};

} // namespace spidy::hero007
