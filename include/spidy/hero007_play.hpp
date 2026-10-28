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
#include "spidy/grapple072.hpp"
#include "spidy/grapple073.hpp"
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
    bool holo{};      // Version 0.0.73: this hand's trigger is the hologram's / phone's / a menu's (never a grapple)
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
    float eyeHeight{};      // test build 12: the T-pose calibration's eye height (m; 0: the standing average)
    bool batonHolsters{};   // BATON HOLSTERS ON
    uint32_t shieldArm{};   // CAPTAIN AMERICA SHIELD ARM value (0 RIGHT, the default; 1 LEFT)
    // Version 0.0.72 GRAPPLE: this hand's grapple gun is out (the bridge pulling on this hand's line, or
    // the PUNISHER's hook launcher drawn and winding): drawn in the hand, the barrel along the aim.
    bool grappleHeld[2]{};
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
    // Version 0.0.72: how many times each sound was asked this frame (two pistols firing on one frame are
    // two shots: two playbacks). sound() sets both; the player plays plays[id] (at least 1 when the bit is set).
    uint8_t plays[sfx::sounds]{};
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
    void sound(sfx::Id id) {
        if (id >= sfx::sounds)
            return;
        sounds |= 1u << id;
        if (plays[id] < 255)
            ++plays[id];
    }
    uint32_t playsOf(uint32_t id) const { return id < sfx::sounds ? std::max<uint32_t>(plays[id], (sounds >> id) & 1u) : 0; }
    // Version 0.0.72 SOUND LOOPS: the looping sounds (sfx::loopable) to keep playing this frame (bit
    // 1 << sfx::Id); game_xr hands each to sfx::loop every frame, so a loop not asked for stops.
    uint32_t loops{};
    void loop(sfx::Id id) {
        if (id < sfx::sounds && sfx::loopable(id))
            loops |= 1u << id;
    }
    bool loopsOn(uint32_t id) const { return id < sfx::sounds && ((loops >> id) & 1u); }
};
static_assert(sfx::sounds <= 32, "PlayOutput::sounds is a 32-bit mask");

// Version 0.0.72: the reload sound a gun's state change asks for (sfx::sounds: none). The pistols play the
// player's own reload sound once a reload, as it begins (the magazine dropped, or AUTO's refill started
// from ready or empty; AUTO taking over a reload already begun is the same reload); the sawed-off keeps
// its 0.0.7 breech-open and seat sounds.
inline uint32_t reloadSoundOf(uint32_t weapon, uint32_t was, uint32_t now) {
    if (now == was)
        return sfx::sounds;
    const bool begins = (now == opened || now == autoLoading) && (was == ready || was == empty);
    if (weapon != sawedOff)
        return begins ? sfx::pistolReload : sfx::sounds;
    if (now == opened || (now == autoLoading && was != autoLoading))
        return now == autoLoading ? sfx::magSeat : sfx::magDrop;
    if (now == ready && (was == seating || was == autoLoading))
        return sfx::magSeat;
    return sfx::sounds;
}

// Where a holstered item sits on the body (tracking space): its grip, along it, its top.
// Version 0.0.72 HOLSTER PLACES (the user's 0.0.71 videos): hipThigh, the PUNISHER's pistols in drop-leg
// holsters on the outside of each thigh (the grip at the top of the thigh, the barrel down along it, a
// little inside the 0.0.7 hip spot so it reads as strapped to the leg); hipBelt, BATMAN's grapple gun
// clipped high on the utility belt, tipped forward (0.0.7's hip spot hung it near the knee: "floating by
// the foot"). Both stay inside holster::inZone's hip zone, so the draw grab is where it is drawn.
enum HipStyle : uint32_t { hipClassic = 0, hipThigh, hipBelt };
// Test build 12 HOLSTERS HIGHER: thighBelow .86 hung the PUNISHER's grips by the knee ("too low"); now .66
// under the eyes - the hip bone of a standingEye player - and every height scales with body.scale (the
// calibrated eye height), so a tall or short player finds them at his own hip.
inline constexpr float thighBelow = .66f, thighOut = .19f, beltHang = .60f, beltOut = .18f, classicBelow = .68f;
inline void wornPose(uint32_t zone, Vec3 head, const holster::BodyYaw& body, Vec3& at, Vec3& forward, Vec3& up,
                     uint32_t hipStyle = hipClassic) {
    const Vec3 f = body.forward(), r = body.right();
    const float k = std::isfinite(body.scale) && body.scale > .5f ? body.scale : 1.f;
    const Vec3 down{0, -k, 0}; // the heights below scale with the player (directions use dn)
    const Vec3 dn{0, -1, 0};
    switch (zone) {
    case holster::leftHip:
    case holster::rightHip: { // muzzle down, the grip up and back, the slide's top facing forward
        const float s = zone == holster::leftHip ? -1.f : 1.f;
        if (hipStyle == hipThigh) {
            at = head + down * thighBelow + r * (s * thighOut) + f * .03f;
            forward = normalized(dn + f * .08f);
        } else if (hipStyle == hipBelt) {
            at = head + down * beltHang + r * (s * beltOut) - f * .03f;
            forward = normalized(dn + f * .30f);
        } else {
            at = head + down * classicBelow + r * (s * .22f) - f * .02f;
            forward = normalized(dn + f * .12f);
        }
        const Vec3 u = f - forward * dot(f, forward);
        up = length(u) > 1e-3f ? normalized(u) : f;
        break;
    }
    case holster::back: // the sawed-off across the back, the grip behind the right shoulder
        at = head + down * .12f + r * .14f - f * .17f;
        forward = normalized(dn + r * -.65f);
        up = -f;
        break;
    default: { // a baton over each shoulder, crossed
        const float s = zone == holster::backLeft ? -1.f : 1.f;
        at = head + down * .10f + r * (s * .13f) - f * .16f;
        forward = normalized(dn + r * (-s * .5f));
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

// Version 0.0.72 BATARANG IN THE FINGERS (0.0.71 videos: it lay "flat on the back of the hand"): held
// pinched between the thumb and the index finger, ahead of the knuckles, standing on edge (its face to
// the side, its wings up and down through the pinch) - from the grip pose (at: the palm, forward: out of
// the fist, up: the thumb's side) or the aim pose without one. Item forward/up for appendBatarang (its
// up is the face's normal).
inline constexpr float pinchAhead = .095f, pinchUp = .03f;
inline bool batarangPinch(Vec3 palm, Vec3 forward, Vec3 thumbUp, Vec3& at, Vec3& itemForward, Vec3& itemUp) {
    if (!finite(palm) || !finite(forward) || !finite(thumbUp) || length(forward) < 1e-4f)
        return false;
    const Vec3 f = normalized(forward);
    Vec3 u = thumbUp - f * dot(thumbUp, f);
    if (length(u) < 1e-3f)
        return false;
    u = normalized(u);
    at = palm + f * pinchAhead + u * pinchUp;
    itemForward = f;
    itemUp = normalized(cross(f, u)); // the face's normal: square to the palm's plane
    return true;
}
// Version 0.0.72 HEAT VISION HEAD LOCK: where the beams leave the eyes (or the frost the mouth) for a
// head pose; headLockBeam redraws a Draw's beam from the head pose the frame is rendered with, along
// where it looks now, as far as the beam reached (beamReach) - so it never trails the head.
inline Vec3 beamEyeMid(const Pose& head) { return head.position + head.orientation.rotate(Vec3{0, 0, -.06f}); }
inline void beamOrigins(const Pose& head, bool freeze, Vec3 to, Vec3 from[2]) {
    if (freeze) {
        const Vec3 mouth = head.position + head.orientation.rotate(Vec3{0, -.09f, -.08f});
        const Vec3 d = to - mouth;
        from[0] = from[1] = mouth + (length(d) > 1e-3f ? normalized(d) : head.orientation.rotate(Vec3{0, 0, -1})) * .2f;
        return;
    }
    const Vec3 f = head.orientation.rotate(Vec3{0, 0, -1});
    for (int e = 0; e < 2; ++e)
        from[e] = head.position + head.orientation.rotate(Vec3{e ? .032f : -.032f, 0, -.06f}) + f * .25f;
}
inline bool headLockBeam(Draw& d, const Pose& head) {
    if (!(d.beamFade > 0) || !(d.beamReach > .3f) || !std::isfinite(d.beamReach) || !finite(head.position) ||
        !finite(Vec3{head.orientation.x, head.orientation.y, head.orientation.z}) || !std::isfinite(head.orientation.w))
        return false;
    const Vec3 f = head.orientation.rotate(Vec3{0, 0, -1});
    if (!finite(f) || length(f) < .5f)
        return false;
    const Vec3 to = beamEyeMid(head) + normalized(f) * d.beamReach;
    Vec3 from[2];
    beamOrigins(head, d.beamFreeze, to, from);
    if (!finite(from[0]) || !finite(from[1]) || !finite(to))
        return false;
    d.beamFrom[0] = from[0];
    d.beamFrom[1] = from[1];
    d.beamTo = to;
    return true;
}
// Version 0.0.72 BATMAN GRAPPLE: the line shoots out of the muzzle over grappleFlight seconds (0..1 of
// the way to the hook) before it pulls taut, instead of appearing whole.
inline constexpr float grappleFlight = .12f;
inline float grappleOut(float seconds) {
    if (!std::isfinite(seconds) || seconds <= 0)
        return 0;
    const float x = std::min(1.f, seconds / grappleFlight);
    return 1 - (1 - x) * (1 - x);
}
// Version 0.0.72 PUNISHER NPC REACTIONS (0.0.71 videos: the thugs barely reacted to being shot): the
// knockback kind stays 5 (FlyBack, punch.hpp's names decoded from the game's reflection: CONFIRMED),
// as every 0.0.72 hero's blow; the size of the reaction rides on knockAmount, as there (uppercutBlow's
// 130 "a stagger, no launch"; shoveBlow's 380 a shove). How big it looks in the game is CANDIDATE.
// A pistol hit staggers him (140); a sawed-off blast with 3+ pellets in him shoves him off his feet
// (380), fewer staggers him (160). The damage is shotBlow's, unchanged.
inline constexpr float pistolStagger = 140.f, buckStagger = 160.f, buckShove = 380.f;
inline sm2_damage::Blow shotReaction(const GunSpec& g, float amount) {
    sm2_damage::Blow b = shotBlow(g, amount);
    const float pellets = g.amount > 0 ? amount / g.amount : 1.f;
    b.knockback = 5;
    b.knockAmount = g.pellets > 1 ? (pellets >= 2.5f ? buckShove : buckStagger) : pistolStagger;
    return b;
}

class Play {
  public:
    uint32_t hero = 0xffffffffu;
    hero_modes::Baton batarangs[2];
    uint32_t batarangThrowsSeen[2]{};
    bool batTrigWas[2]{}, grappling[2]{}; // Version 0.0.7 BATMAN GRAPPLE GUN: the press's kind
    // Version 0.0.73 UNIVERSAL GRAPPLE (grapple073.hpp): BATMAN's and CAPTAIN AMERICA's grapple gun is in the
    // hand while a grip that did not take a batarang, a pellet or the shield is held; the trigger fires it
    // only then (FireGate: a press begun on the hologram or before the gun was out never fires).
    bool gunDrawn[2]{}, gunGripWas[2]{};
    grapple073::FireGate grappleGate;
    unsigned grappleHand{1};              // the hand that fired it last (its hip holsters the gun; right at first)
    holster::BodyYaw batBody;             // BATMAN: the body's heading for the gun's hip holster
    // Version 0.0.7 BATARANG THROW (test build 12: drawn from a belt pouch by the grip): the batarang is in
    // the hand only while that grip is held; letting go throws it. Never in an idle hand.
    bool batReady[2]{};
    // Test build 12 BATARANG DRAW: the belt pouches' grips (hero007.hpp BatarangDraw); the trigger is the grapple's.
    BatarangDraw batDraw;
    // Version 0.0.7 SMOKE PELLET: the belt buckle's pellets (grip there, let go to throw).
    SmokePellets pellets;
    holster::BodyYaw pelletBody; // the body's heading at the grip arbitration (the belt's front)
    Smoke smoke;
    Shells shells;
    HeatVision heat;
    bool freezing{}, heatWas{};
    Vec3 beamFrom{}, beamTo{};
    float beamFade{}, heatSoundIn{};
    // Version 0.0.72 HEAT VISION: the beam's end on an enemy (the sparks), the sparks' clock, and the scorch
    // marks on the enemies burned (each kept on its enemy by id while he is listed; age s, 1e9 none).
    bool beamOnEnemy{};
    float beamClock{};
    uint64_t scorchId[drawScorches]{};
    Vec3 scorchAt[drawScorches]{}, scorchOff[drawScorches]{};
    float scorchAge[drawScorches]{1e9f, 1e9f, 1e9f, 1e9f};
    uint32_t scorchNext{};
    // Version 0.0.72 PUNISHER: each hand's last muzzle flash and the last hits (tracking space).
    Vec3 flashAt[2]{}, flashWay[2]{};
    float flashAge[2]{1e9f, 1e9f};
    bool flashShotgun[2]{};
    Vec3 impactAt[drawImpacts]{}, impactWay[drawImpacts]{};
    float impactAge[drawImpacts]{1e9f, 1e9f, 1e9f, 1e9f, 1e9f, 1e9f};
    uint32_t impactNext{};
    hero_modes::BatonSwing claws[2];
    bool clawsOut[2]{};
    float clawsOutFor[2]{}; // Version 0.0.72 SNIKT: seconds the claws have been out (their extension)
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
    float shieldCaughtFor = 1e3f;  // Version 0.0.72 SHIELD CATCH: seconds since the last catch
    Vec3 shieldCaughtAt{};         // where it was caught (captain::catchPose eases from here)
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
            batTrigWas[i] = grappling[i] = batReady[i] = false;
            gunDrawn[i] = gunGripWas[i] = false; // Version 0.0.73
            batDraw.reset();
            batarangs[i].curve = batarangCurve;
            batarangs[i].curveSign = i == 0 ? -1.f : 1.f;
            grappleHand = 1;
            batBody = {};
            claws[i].reset();
            clawsOut[i] = false;
            clawsOutFor[i] = 0;
            haveLast[i] = false;
        }
        heat = {};
        healing = {};
        heatWas = freezing = false;
        beamFade = 0;
        beamOnEnemy = false;
        beamClock = 0;
        for (uint32_t k = 0; k < drawScorches; ++k)
            scorchAge[k] = 1e9f, scorchId[k] = 0;
        for (int k = 0; k < 2; ++k)
            flashAge[k] = 1e9f;
        for (uint32_t k = 0; k < drawImpacts; ++k)
            impactAge[k] = 1e9f;
        smoke = {};
        pellets = {};
        pelletBody = {};
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
        shieldRack.add(holster::back, 0, 1u << shieldHand, true); // 0.0.73 fix 2: sticky (stays on the arm)
        shield = {};
        shield.flyHome = true; // Version 0.0.72: it flies home after batonMaxOut, never snaps
        shield.roll.state = 0x9e3779b9u;
        shieldCaughtFor = 1e3f;
        shieldCaughtAt = {};
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

    // Version 0.0.73 GRAPPLE GUN DRAW: a grip pressed in hand i with `free` (nothing else took it) draws the
    // gun; it stays out while the grip is held and goes back to the hip when it lets go.
    void gunGrip(const PlayInput& in, unsigned i, bool free, PlayOutput& out) {
        const PlayHand& h = in.hands[i];
        const bool sq = h.tracked && grapple073::held(h.squeeze, gunGripWas[i], holster::pressed, holster::released);
        const bool press = sq && !gunGripWas[i];
        gunGripWas[i] = sq;
        if (!in.play || !sq) {
            gunDrawn[i] = false;
            return;
        }
        if (press && free && !h.busy && !h.holo && !gunDrawn[i]) {
            gunDrawn[i] = true;
            out.buzz(i, .25f);
            out.sound(sfx::holsterTick);
        }
        if (gunDrawn[i])
            out.claimed |= 1u << i;
    }
    // The racks, at the grip arbitration. Fills out.claimed, the draw / holster ticks and sounds.
    void holsterStep(const PlayInput& in, PlayOutput& out) {
        setHero(in.hero);
        { // test build 12 BODY SCALE: every holster, belt and pouch height from the player's own eye height
            const float k = holster::scaleFor(in.eyeHeight);
            guns.body.scale = batons.body.scale = shieldRack.body.scale = batBody.scale = pelletBody.scale = k;
        }
        if (captain::armHand(in.shieldArm) != shieldHand) { // SHIELD ARM changed: the shield to the back
            shieldHand = captain::armHand(in.shieldArm);
            resetShield();
        }
        holster::Rack* rack = punisher() ? &guns
                              : (hero == hero_modes::daredevil && in.batonHolsters) ? &batons
                              : captainAmerica()                                    ? &shieldRack
                                                                                    : nullptr;
        if (hero == hero_modes::batman) { // SMOKE PELLET: the belt's grips
            pelletBody.update(in.dt, in.head.orientation.rotate(Vec3{0, 0, -1}));
            Vec3 at[2];
            bool tracked[2], busy[2];
            float squeeze[2];
            for (unsigned i = 0; i < 2; ++i) {
                at[i] = in.hands[i].position;
                tracked[i] = in.hands[i].tracked;
                busy[i] = in.hands[i].busy;
                squeeze[i] = in.hands[i].squeeze;
            }
            uint32_t took = 0;
            const uint32_t pelletHands = pellets.grip(in.dt, in.play, in.head.position, pelletBody, at, tracked, squeeze, busy, &took);
            out.claimed |= pelletHands;
            for (unsigned i = 0; i < 2; ++i)
                if (took & (1u << i)) {
                    out.buzz(i, .2f);
                    out.sound(sfx::holsterTick);
                }
            // Test build 12 BATARANG DRAW: a grip at a belt pouch (beside the buckle) takes a batarang.
            bool drawBusy[2], ready[2];
            for (unsigned i = 0; i < 2; ++i) {
                drawBusy[i] = busy[i] || (pelletHands & (1u << i)) || pellets.hands[i].state == SmokePellets::held ||
                              grappling[i] || in.grappleHeld[i] || gunDrawn[i];
                ready[i] = batarangs[i].inHandNow();
            }
            out.claimed |= batDraw.update(in.play, in.head.position, pelletBody, at, tracked, squeeze, drawBusy, ready);
            for (unsigned i = 0; i < 2; ++i) {
                if (batDraw.events[i] & BatarangDraw::took) {
                    out.buzz(i, .35f);
                    out.sound(sfx::holsterTick);
                } else if (batDraw.events[i] & BatarangDraw::nearPouch) {
                    out.buzz(i, .15f); // in reach of a pouch
                }
            }
            // Version 0.0.73 (fix 3): any other grip draws the grapple gun (a pouch's or the buckle's took it first).
            for (unsigned i = 0; i < 2; ++i) {
                const bool other = (out.claimed & (1u << i)) && !gunDrawn[i];
                gunGrip(in, i, !other && !batDraw.drawn[i] && !batReady[i] && pellets.hands[i].state != SmokePellets::held, out);
            }
        } else if (hero != hero_modes::captainAmerica) {
            gunDrawn[0] = gunDrawn[1] = gunGripWas[0] = gunGripWas[1] = false;
        }
        if (hero == hero_modes::wolverine) // Version 0.0.72: a fist's grip is the claws' (no web or zip from it)
            for (unsigned i = 0; i < 2; ++i)
                if (in.hands[i].tracked && !in.hands[i].busy &&
                    (in.hands[i].squeeze >= holster::pressed || (clawsOut[i] && in.hands[i].squeeze > holster::released)))
                    out.claimed |= 1u << i;
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
            } else if (e & holster::near) {
                out.buzz(i, holster::tickFor(e)); // in reach of a holster: a soft pulse, no sound
            }
        }
        // Version 0.0.73 CAPTAIN AMERICA GRAPPLE (fix 17): the hand that is not the SHIELD ARM's draws the
        // grapple gun with its grip (never a grip the shield rack took); the shield arm's grip stays the shield's.
        if (captainAmerica()) {
            for (unsigned i = 0; i < 2; ++i) {
                if (i == shieldHand) {
                    gunDrawn[i] = false;
                    gunGripWas[i] = in.hands[i].squeeze >= holster::pressed;
                    continue;
                }
                const bool rackHas = (out.claimed & (1u << i)) && !gunDrawn[i];
                // never a grip reaching for the back (the shield's place: the wrong hand takes nothing there)
                bool atBack = false;
                if (shieldRack.body.valid && finite(in.hands[i].position) && finite(in.head.position)) {
                    const holster::BodyPoint bp = holster::bodyPoint(in.hands[i].position, in.head.position, shieldRack.body);
                    atBack = holster::inZone(holster::back, bp) || holster::inZone(holster::backLeft, bp) ||
                             holster::inZone(holster::backRight, bp);
                }
                gunGrip(in, i, !rackHas && !atBack, out);
            }
        }
    }

    void step(const PlayInput& in, PlayOutput& out) {
        setHero(in.hero);
        { // test build 12 BODY SCALE: every holster, belt and pouch height from the player's own eye height
            const float k = holster::scaleFor(in.eyeHeight);
            guns.body.scale = batons.body.scale = shieldRack.body.scale = batBody.scale = pelletBody.scale = k;
        }
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
        {   // Version 0.0.72: the scorches, flashes and hits age (and a scorch follows its enemy)
            const float t = std::isfinite(dt) ? std::clamp(dt, 0.f, 1.f) : 0.f;
            beamClock = std::fmod(beamClock + t, 1000.f);
            for (uint32_t k = 0; k < drawScorches; ++k) {
                scorchAge[k] = std::min(scorchAge[k] + t, 1e9f);
                for (uint32_t e = 0; scorchId[k] && e < in.count && in.list; ++e)
                    if (in.list[e].id == scorchId[k] && finite(in.list[e].chest))
                        scorchAt[k] = in.list[e].chest + scorchOff[k];
            }
            for (int k = 0; k < 2; ++k)
                flashAge[k] = std::min(flashAge[k] + t, 1e9f);
            for (uint32_t k = 0; k < drawImpacts; ++k)
                impactAge[k] = std::min(impactAge[k] + t, 1e9f);
        }
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
            // The GRAPPLE GUN (drawn only with its model): in the firing hand while the press is the
            // grapple's (the grip pose, like PUNISHER's guns), else on that hand's hip.
            bool gunHeld = false;
            for (unsigned i = 0; i < 2; ++i) {
                if (!grappling[i] && !in.grappleHeld[i] && !gunDrawn[i]) // 0.0.73: out while its grip holds it
                    continue;
                // Version 0.0.72: the barrel ALONG THE AIM (the ray the line fires on), not the forearm.
                const auto g = in.hands[i].tracked && in.hands[i].gunOk && finite(in.hands[i].gunAt) && handOk(i)
                                   ? grapple072::alongAim(in.hands[i].gunAt, in.hands[i].aim, in.hands[i].gunUp)
                                   : grapple072::Held{};
                if (g.ok) {
                    d.held[i] = item(lookGrappleGun, g.at, g.forward, g.up);
                    d.held[i].palm = true;
                    gunHeld = any = true;
                } else if (handOk(i)) {
                    const Vec3 a = normalized(in.hands[i].aim);
                    d.held[i] = item(lookGrappleGun, in.hands[i].position, a, gunUp(a));
                    gunHeld = any = true;
                }
            }
            if (!gunHeld && batBody.valid && finite(in.head.position)) {
                Vec3 at{}, fwd{}, up{};
                wornPose(grappleHand == 0 ? holster::leftHip : holster::rightHip, in.head.position, batBody, at, fwd, up, hipBelt);
                d.worn[0] = item(lookGrappleGun, at, fwd, up, 1);
                any = true;
            }
            for (unsigned i = 0; i < 2; ++i) {
                const auto& b = batarangs[i];
                if (b.inHandNow()) {
                    // BATARANG THROW: only while its pouch grip is held (never idle), never with the gun drawn.
                    if (batReady[i] && !grappling[i] && d.held[i].look != lookGrappleGun && handOk(i)) {
                        // Version 0.0.72 BATARANG IN THE FINGERS: pinched ahead of the knuckles, on edge.
                        const PlayHand& h = in.hands[i];
                        Vec3 at{}, f{}, u{};
                        const bool pinched = h.gunOk && finite(h.gunAt) &&
                                             batarangPinch(h.gunAt, h.gunForward, h.gunUp, at, f, u);
                        if (!pinched) {
                            const Vec3 a = normalized(h.aim);
                            batarangPinch(h.position, a, gunUp(a), at, f, u);
                        }
                        d.held[i] = item(lookBatarang, at, f, u);
                        any = true;
                    }
                } else if (finite(b.position)) {
                    const Vec3 way = length(b.velocity) > .1f ? normalized(b.velocity) : Vec3{0, 0, -1};
                    d.flying[i] = item(lookBatarang, b.position, way, Vec3{0, 1, 0});
                    d.flying[i].spin = b.out * 20.f;
                    any = true;
                }
            }
            for (unsigned i = 0; i < 2; ++i) { // SMOKE PELLET: in the hand or in the air
                const auto& p = pellets.hands[i];
                if (p.state != SmokePellets::none && finite(p.at)) {
                    d.pellet[i] = true;
                    d.pelletAt[i] = W(p.at);
                    any = true;
                }
            }
        } else if (hero == hero_modes::wolverine) {
            for (unsigned i = 0; i < 2; ++i)
                if (clawsOut[i] && handOk(i)) {
                    // Version 0.0.72 CLAW SOCKET: from the fist's knuckles along the forearm (the grip
                    // pose, as PUNISHER's guns), its top the back of the hand; the aim pose when the
                    // grip pose is missing. spin carries the SNIKT extension (0-1).
                    if (in.hands[i].gunOk && finite(in.hands[i].gunAt) && finite(in.hands[i].gunForward) &&
                        finite(in.hands[i].gunUp))
                        d.held[i] = item(lookClaws, in.hands[i].gunAt, in.hands[i].gunForward, in.hands[i].gunUp);
                    else {
                        const Vec3 a = normalized(in.hands[i].aim);
                        d.held[i] = item(lookClaws, in.hands[i].position, a, gunUp(a));
                    }
                    d.held[i].spin = clawExtend(clawsOutFor[i]);
                    any = true;
                }
        } else if (hero == hero_modes::punisher) {
            for (uint32_t s = 0; s < guns.count; ++s) {
                const auto& slot = guns.slots[s];
                const Gun& g = armory[s];
                const bool open = g.weapon == sawedOff && (g.state == opened || g.state == seating || g.state == autoLoading);
                const uint32_t look = g.weapon == sawedOff ? (open ? lookSawedOffOpen : lookSawedOff) : lookPistol;
                Vec3 at{}, fwd{}, up{};
                wornPose(slot.zone, in.head.position, guns.body, at, fwd, up, hipThigh);
                const uint32_t kind = slot.zone == holster::back ? 2u : 4u; // Version 0.0.72: 4 the drop-leg holster
                if (slot.heldBy < 0) {
                    d.worn[s] = item(look, at, fwd, up, kind);
                } else {
                    d.worn[s] = item(lookNone, at, fwd, up, kind); // the empty holster stays on the body
                }
                d.worn[s].side = slot.zone == holster::leftHip ? 0u : 1u;
                if (slot.heldBy >= 0) {
                    const unsigned i = static_cast<unsigned>(slot.heldBy);
                    const gun_grip::Held gh = i < 2 && in.hands[i].tracked ? aimedGrip(in.hands[i]) : gun_grip::Held{};
                    if (i < 2 && gh.ok) {
                        // Version 0.0.72 AIMED GUNS: in the palm from the grip pose, the barrel along the aim (the shot's line).
                        d.held[i] = item(look, gh.at, gh.forward, gh.up);
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
            // Version 0.0.72 GRAPPLE: the hook launcher (the grapple-gun model) in the hand that fires it,
            // the barrel along the aim (a hand already holding a gun keeps its gun).
            for (unsigned i = 0; i < 2; ++i) {
                if (!in.grappleHeld[i] || d.held[i].look != lookNone || !handOk(i))
                    continue;
                const auto g = in.hands[i].gunOk ? grapple072::alongAim(in.hands[i].gunAt, in.hands[i].aim, in.hands[i].gunUp)
                                                 : grapple072::alongAim(in.hands[i].position, in.hands[i].aim, Vec3{0, 1, 0});
                if (g.ok) {
                    d.held[i] = item(lookGrappleGun, g.at, g.forward, g.up);
                    d.held[i].palm = in.hands[i].gunOk;
                    any = true;
                }
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
                    const captain::ShieldPose p = captain::catchPose(armOf(in.hands[i], headF, bodyR, i), shieldCaughtAt, shieldCaughtFor);
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
            // Version 0.0.73 CAPTAIN AMERICA GRAPPLE (fix 17): the grapple gun in the free hand while its grip
            // holds it (barrel along the aim), else on that hand's hip.
            const unsigned g = 1 - i;
            if ((gunDrawn[g] || grappling[g] || in.grappleHeld[g]) && handOk(g)) {
                const auto gh = in.hands[g].gunOk && finite(in.hands[g].gunAt)
                                    ? grapple072::alongAim(in.hands[g].gunAt, in.hands[g].aim, in.hands[g].gunUp)
                                    : grapple072::Held{};
                const Vec3 a = normalized(in.hands[g].aim);
                d.held[g] = gh.ok ? item(lookGrappleGun, gh.at, gh.forward, gh.up) : item(lookGrappleGun, in.hands[g].position, a, gunUp(a));
                d.held[g].palm = gh.ok;
                any = true;
            } else if (shieldRack.body.valid && finite(in.head.position)) {
                Vec3 at{}, fwd{}, up{};
                wornPose(g == 0 ? holster::leftHip : holster::rightHip, in.head.position, shieldRack.body, at, fwd, up, hipBelt);
                d.worn[1] = item(lookGrappleGun, at, fwd, up, 1);
                any = true;
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
            // Version 0.0.72 HEAD LOCK: the reach game_xr redraws it with from the rendered head (headLockBeam).
            d.beamReach = length(to - beamEyeMid(in.head));
            d.beamHit = beamOnEnemy;
            d.beamClock = beamClock;
            any = true;
        }
        for (uint32_t k = 0; k < drawScorches; ++k)
            if (scorchAge[k] < scorchLife && finite(scorchAt[k])) {
                d.scorchAt[k] = W(scorchAt[k]);
                d.scorchAge[k] = scorchAge[k];
                any = true;
            }
        for (int k = 0; k < 2; ++k)
            if (flashAge[k] < flashLife && finite(flashAt[k])) {
                d.flashAt[k] = W(flashAt[k]);
                d.flashWay[k] = R(flashWay[k]);
                d.flashAge[k] = flashAge[k];
                d.flashShotgun[k] = flashShotgun[k];
                any = true;
            }
        for (uint32_t k = 0; k < drawImpacts; ++k)
            if (impactAge[k] < impactLife && finite(impactAt[k])) {
                d.impactAt[k] = W(impactAt[k]);
                d.impactWay[k] = R(impactWay[k]);
                d.impactAge[k] = impactAge[k];
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
    // Test build 12: the shield on the forearm from the grip frame (captain::onForearm); onArm's
    // aim-and-look placement only with no usable grip pose.
    static captain::ShieldPose armOf(const PlayHand& h, Vec3 headF, Vec3 bodyR, unsigned i) {
        if (h.gunOk && finite(h.gunAt)) {
            const captain::ShieldPose p = captain::onForearm(h.gunAt, h.gunForward, h.gunUp, i);
            if (p.ok)
                return p;
        }
        return captain::onArm(h.position, h.aim, headF, bodyR, i);
    }
    // CAPTAIN AMERICA: the SHIELD ARM's trigger throws the drawn shield (backhand: along a fast
    // forward sweep, else the aim, the aim assist's enemy first); it chains like the batons (SHIELD
    // CHAIN) and comes back to the hand. On the arm a swing of its rim is a bash.
    void stepCaptain(const PlayInput& in, float dt, PlayOutput& out) {
        // Version 0.0.73 CAPTAIN AMERICA GRAPPLE (fix 17): the free hand's drawn gun fires on its trigger.
        for (unsigned k = 0; k < 2; ++k) {
            const bool fire = k != shieldHand && grappleGate.step(k, gunDrawn[k] && in.hands[k].tracked, in.hands[k].trigger, in.hands[k].holo);
            grappling[k] = fire;
            if (fire) {
                out.grapple[k] = true;
                grappleHand = k;
            }
        }
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
        const Vec3 flewAt = shield.position;
        shieldCaughtFor = std::min(shieldCaughtFor + dt, 1e3f);
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
            if (finite(flewAt)) { // 0.0.72 SHIELD CATCH: drawn easing onto the arm from here
                shieldCaughtFor = 0;
                shieldCaughtAt = flewAt;
            }
            out.buzz(i, .6f);
            out.sound(sfx::shieldCatch);
            ++shieldCatches;
        }
        if (!shield.inHandNow() || !h.tracked) {
            shieldBash.update(dt, false, {}, {}, in.list, in.count);
            return;
        }
        const Vec3 bodyR = shieldRack.body.valid ? shieldRack.body.right() : cross(headF, Vec3{0, 1, 0});
        armPose = armOf(h, headF, bodyR, i);
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
        batBody.update(dt, in.head.orientation.rotate(Vec3{0, 0, -1}));
        for (unsigned i = 0; i < 2; ++i) {
            auto& b = batarangs[i];
            b.chainChance = batarangChance(in.chain);
            const PlayHand& h = in.hands[i];
            const bool trig = h.tracked && h.trigger > .5f;
            if (!finite(h.position) || !finite(h.aim))
                continue;
            // Test build 12 BATMAN BUTTONS (the user: batarangs and the grapple shared the trigger): the
            // TRIGGER is always the grapple gun (a press grapples wherever it aims, held until let go) - except
            // in a hand holding a batarang, whose trigger waits; the GRIP at a belt pouch draws a batarang
            // (BatarangDraw) and letting the grip go throws it; the grip at the buckle is the SMOKE PELLET.
            // Version 0.0.73 (fix 3, the user: "grip draws it, then trigger fires; never on every trigger press
            // or on holo clicks"): the trigger grapples only with the gun drawn by the grip (FireGate).
            grappling[i] = grappleGate.step(i, gunDrawn[i] && h.tracked && !batDraw.drawn[i] && !batReady[i], h.trigger, h.holo);
            batTrigWas[i] = trig;
            if (grappling[i]) {
                out.grapple[i] = true;
                grappleHand = i;
            }
            // BATARANG THROW: in the hand while its pouch grip is held, thrown on the frame the grip lets go -
            // along the hand's sweep when it is a throwing motion (captain::throwAim), else straight along the
            // aim - at the enemy the aim assist picks on that line THEN.
            const bool readyWas = batReady[i];
            batReady[i] = batDraw.drawn[i] && b.inHandNow();
            const bool release = readyWas && (batDraw.events[i] & BatarangDraw::letGo) && b.inHandNow();
            const Vec3 throwWay = release ? captain::throwAim(pellets.handVelocity[i], h.aim,
                                                               in.head.orientation.rotate(Vec3{0, 0, -1}))
                                          : h.aim;
            const int aimed = b.inHandNow() ? hero_modes::batonAim(h.position, throwWay, in.list, in.count) : -1;
            const uint32_t throwsWas = b.throws;
            const bool hit = b.update(dt, release, h.position, throwWay, in.list, in.count, aimed);
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
                ++batarangHits; // no smoke: that is the SMOKE PELLET's now
            }
        }
        Vec3 burst[2];
        const uint32_t bursts = pellets.fly(dt, in.head.position, in.list, in.count, burst);
        for (uint32_t k = 0; k < bursts; ++k) {
            smoke.add(burst[k]);
            ++smokes;
        }
    }
    void stepWolverine(const PlayInput& in, float dt, PlayOutput& out) {
        for (unsigned i = 0; i < 2; ++i) {
            const PlayHand& h = in.hands[i];
            // Version 0.0.72 CLAWS ON A FIST (the user: off the trigger): a GRIP squeeze pops the
            // claws (SNIKT) and they stay out while the grip is held past holster::released; the
            // trigger is free (basic projectiles only, none for WOLVERINE).
            const bool squeezing = h.squeeze >= holster::pressed || (clawsOut[i] && h.squeeze > holster::released);
            const bool outNow = h.tracked && squeezing && finite(h.position) && finite(h.aim);
            if (outNow && !clawsOut[i]) {
                out.sound(sfx::clawSnikt);
                out.buzz(i, .25f);
            }
            clawsOut[i] = outNow;
            clawsOutFor[i] = outNow ? std::min(clawsOutFor[i] + (std::isfinite(dt) ? std::max(dt, 0.f) : 0.f), 1.f) : 0.f;
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
            beamOnEnemy = false;
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
        beamOnEnemy = aimed >= 0 && length(in.list[aimed].chest - from) <= range;
        // Version 0.0.72 SOUND LOOPS (0.0.71 videos: the beam's sound stuttered, a one-shot fired again every
        // .55 s): one seamless loop for as long as the beam is on, stopped the frame it is not asked for.
        out.loop(freezing ? sfx::freezeBreath : sfx::heatVision);
        heatWas = true;
        if (!tick)
            return;
        int hit[8];
        const int n = rayHits(from, dir, range, freezing ? freezeRadius : heatRadius, in.list, in.count, hit, 8);
        for (int k = 0; k < n; ++k) {
            out.blow(in.list[hit[k]].id, freezing ? freezeTick() : heatTick(), in.list[hit[k]].chest,
                     freezing ? kindFreeze : kindHeat);
            if (!freezing) { // Version 0.0.72 SCORCH: a burn on his front where the beam hit, kept on him
                const uint32_t m = scorchNext++ % drawScorches;
                const float j = static_cast<float>(heatTicks % 5u) - 2.f;
                scorchId[m] = in.list[hit[k]].id;
                scorchOff[m] = -dir * .14f + Vec3{.03f * j, .04f * j, 0};
                scorchAt[m] = in.list[hit[k]].chest + scorchOff[m];
                scorchAge[m] = 0;
            }
        }
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
            // Version 0.0.72 AIMED GUNS (0.0.71 videos: the barrel pointed off the aim): in the palm, the barrel
            // along the controller's aim ray (as BATMAN's grapple gun), so where you point is where it shoots.
            const gun_grip::Held gh = aimedGrip(h);
            if (fired && gh.ok) {
                const Vec3 a = normalized(gh.forward);
                const Vec3 muzzle = gun_grip::from(gh, gun_grip::muzzleFromPalm(gw));
                float amount[our_ball_max]{};
                const uint32_t n = std::min(in.count, our_ball_max);
                const int hits = shoot(g.spec(), muzzle, a, in.list, n, amount);
                for (uint32_t e = 0; e < n; ++e)
                    if (amount[e] > 0)
                        out.blow(in.list[e].id, shotReaction(g.spec(), amount[e]), in.list[e].chest,
                                 g.weapon == sawedOff ? kindSawedOff : kindPistol);
                ++shots;
                shotHits += static_cast<uint64_t>(hits);
                shotLook(i, g.weapon == sawedOff, muzzle, a, in, n, amount);
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
                        out.blow(in.list[e].id, shotReaction(g.spec(), amount[e]), in.list[e].chest,
                                 g.weapon == sawedOff ? kindSawedOff : kindPistol);
                ++shots;
                shotHits += static_cast<uint64_t>(hits);
                shotLook(i, g.weapon == sawedOff, muzzle, a, in, n, amount);
                out.sound(g.weapon == sawedOff ? sfx::shotgunBlast : sfx::pistolShot);
                out.buzz(i, g.weapon == sawedOff ? .95f : .7f);
                if (g.weapon == pistol) {
                    const Vec3 right = normalized(cross(a, gunUp(a)));
                    shells.eject(h.position + gunUp(a) * .06f, right, false);
                }
            }
            if (g.state != was) {
                const bool opening = g.state == opened || (g.state == autoLoading && was != autoLoading);
                const uint32_t reloadSound = reloadSoundOf(g.weapon, was, g.state);
                if (reloadSound < sfx::sounds)
                    out.sound(static_cast<sfx::Id>(reloadSound));
                if (opening) {
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
    // Version 0.0.72 AIMED GUNS: a hand's gun pose: the grip point, the barrel along the aim, its top
    // toward the thumb (not ok: no usable grip pose - the aim pose instead).
    static gun_grip::Held aimedGrip(const PlayHand& h) {
        if (!h.gunOk || !finite(h.gunAt) || !finite(h.gunUp))
            return {};
        const auto g = grapple072::alongAim(h.gunAt, h.aim, h.gunUp);
        return {g.at, g.forward, g.up, g.ok};
    }
    // Version 0.0.72 MUZZLE FLASH / HIT EFFECTS: a shot's flash at the muzzle along the barrel, and a hit
    // puff on each enemy it hit (on his front, facing back along the shot).
    void shotLook(unsigned hand, bool shotgun, Vec3 muzzle, Vec3 way, const PlayInput& in, uint32_t n, const float* amount) {
        if (hand < 2 && finite(muzzle) && finite(way)) {
            flashAt[hand] = muzzle;
            flashWay[hand] = way;
            flashAge[hand] = 0;
            flashShotgun[hand] = shotgun;
        }
        for (uint32_t e = 0; e < n && in.list; ++e)
            if (amount[e] > 0 && finite(in.list[e].chest)) {
                const uint32_t m = impactNext++ % drawImpacts;
                const Vec3 c = in.list[e].chest, toward = normalized(c - muzzle);
                impactAt[m] = finite(toward) ? c - toward * .15f : c;
                impactWay[m] = finite(toward) ? toward : way;
                impactAge[m] = 0;
            }
    }
};

} // namespace spidy::hero007
