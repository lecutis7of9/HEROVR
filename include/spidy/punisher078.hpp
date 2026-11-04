#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
//
// HERO VR 0.0.78 PUNISHER ARSENAL (the owner pt1886u: "those go in 0.0.78"): four hand-worked guns beside
// the 0.0.7 pistols and sawed-off, chosen by live.txt punisher_arsenal (ON, the default, this loadout;
// OFF the 0.0.77 pair of pistols and the sawed-off, unchanged).
//  - PUMP SHOTGUN (Pump): two hands. Manual only, never an automatic reload: the off hand grips a shell
//    at the belt (shellPouch), carries it to the loading port under the receiver (loadingPort) and it
//    goes into the tube; then the off hand on the forend racks the pump - back (the chamber's shell,
//    spent or live, is thrown out of the ejection port) and forward (the next shell from the tube is
//    chambered). The trigger fires only with a live shell chambered and the pump fully forward.
//  - SUPPRESSED PISTOL (MagGun, semi): the eject button (live.txt mag_eject) drops the magazine; the off
//    hand grips a fresh one at the thigh pouch (magPouch: the PUNISHER model's own left-thigh pouch),
//    pushes it up into the mag well and racks the slide (pulls it back past rackTravel and lets it go
//    forward) before it fires. The last round locks the slide back; a new magazine still needs the rack.
//    Holstered in the PUNISHER model's own right-leg holster (legHolster: measured off
//    drive-dl/model/model.glb; no holster is ever drawn, the model's is the one).
//  - SNIPER RIFLE (MagGun, bolt): the same magazine, and the off hand cycles the bolt after every shot.
//    SCOPE (Scope): a magnified picture in the lens - the eye image's patch along the scope's axis,
//    magnified scope_zoom times into the lens's disk, with the eye box's shadow off axis (CANDIDATE: the
//    renderer can only magnify what the eye already renders, it has no second camera).
//  - ASSAULT RIFLE (MagGun, auto): 30 rounds, held trigger fires at autoInterval; the charging handle is
//    the rack. RED DOT (RedDot): the dot is collimated (at infinity): it is drawn where the eye's ray
//    parallel to the sight's axis crosses the lens, so it stays on the target as the head moves
//    (parallax-free) and is hidden once that ray misses the lens.
// Pure; no game memory, no address. The mechanics are checked in tests/punisher078_tests.cpp.
#include "spidy/hero007.hpp"
#include "spidy/holster.hpp"
#include "spidy/math.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace spidy::punisher078 {

enum Arm : uint32_t { pumpShotgun = 0, suppressedPistol, sniperRifle, assaultRifle, arms };
inline constexpr const char* armNames[arms] = {"PUMP SHOTGUN", "SUPPRESSED PISTOL", "SNIPER RIFLE", "ASSAULT RIFLE"};
// HERO VR 0.0.78 cut14: the guns that ship (their models are in Models\Punisher078): the pump and the pistol. The
// sniper and the rifle come later - not in the loadout, so no holster, draw or look of theirs.
inline constexpr bool shipped(uint32_t arm) { return arm == pumpShotgun || arm == suppressedPistol; }

// The shot itself (hero007::shoot's spec: rounds, cooldown, range, pellets, cone, HP a pellet, knock, unused).
// The pump: 8 pellets of 00 buck in a 4.5 deg cone (a full-length barrel: tighter than the sawed-off's 7).
inline constexpr hero007::GunSpec pumpShot{7, .30f, 30.f, 8, 4.5f, 11.f, 30.f, 0.f};
inline constexpr hero007::GunSpec pistolShot{15, .16f, 50.f, 1, 0.f, 18.f, 50.f, 0.f};
inline constexpr hero007::GunSpec sniperShot{5, .60f, 400.f, 1, 0.f, 120.f, 220.f, 0.f};
inline constexpr hero007::GunSpec rifleShot{30, .08f, 150.f, 1, 0.f, 16.f, 45.f, 0.f};
inline const hero007::GunSpec& shotOf(uint32_t arm) {
    return arm == pumpShotgun ? pumpShot : arm == sniperRifle ? sniperShot : arm == assaultRifle ? rifleShot : pistolShot;
}
inline constexpr float autoInterval = .08f; // the rifle: 750 rounds a minute

// ---- the gun's own frame --------------------------------------------------------------------------
// Each gun's points in its frame (metres from the grip point: forward along the barrel, up toward the top,
// right). Measured on the real guns at real scale (an 870 with an 18.5" barrel, a full-size 9 mm with a
// 6" can, a bolt rifle with a 24" barrel, a 14.5" carbine); the converted models put their grip at the
// origin with the same axes (tools/models078/README.md), so these are where the parts are drawn too.
struct Local {
    float forward{}, up{}, right{};
};
struct GunPoints {
    Local muzzle, port, magWell, forend, rack, sightEye, lens;
    float forendRadius, rackRadius, wellRadius, portRadius;
    float rackTravel;  // how far the pump / slide / bolt / charging handle goes back (m)
    float lensRadius;  // the scope's or red dot's lens (m); 0 none
};
inline constexpr GunPoints pointsOf(uint32_t arm) {
    switch (arm) {
    case pumpShotgun: // measured on the shipped Remington 870 (convert_guns_078.py, conversion-report078.json): the muzzle
        // 46 cm ahead of the grip, the forend's middle at 35 cm with 6.9 cm to the receiver, the loading port under it
        return {{.459f, .092f, 0}, {.147f, .052f, 0}, {}, {.346f, .067f, 0}, {.346f, .067f, 0}, {-.02f, .114f, 0}, {}, .09f, .09f, 0,
                .07f, .069f, 0};
    case sniperRifle: // the bolt handle on the right behind the scope; a 50 mm objective, 4 cm ocular
        return {{1.02f, .06f, 0}, {.07f, .06f, .02f}, {.06f, -.03f, 0}, {.36f, .02f, 0}, {.04f, .07f, .05f}, {-.10f, .115f, 0},
                {-.02f, .115f, 0}, .10f, .07f, .07f, 0, .09f, .019f};
    case assaultRifle: // the charging handle at the back of the upper; a 25 mm red dot window
        return {{.56f, .055f, 0}, {.06f, .06f, .02f}, {.07f, -.02f, 0}, {.25f, .03f, 0}, {-.06f, .085f, 0}, {-.18f, .12f, 0},
                {.10f, .12f, 0}, .10f, .07f, .07f, 0, .065f, .0125f};
    default: // measured on the shipped Beretta M9 with its suppressor (convert_guns_078.py): the can's end 36 cm ahead of
        // the grip, the magazine into the grip's base, the slide's serrations at the back, 35 mm of slide travel
        return {{.360f, .074f, 0}, {.05f, .075f, .01f}, {0, -.054f, 0}, {}, {-.012f, .066f, 0}, {-.12f, .089f, 0}, {}, 0, .06f, .07f,
                0, .035f, 0};
    }
}
// A point of the gun's frame in tracking space (the held gun: grip point, barrel forward, top up).
inline Vec3 toWorld(const Vec3& at, const Vec3& forward, const Vec3& up, Local p) {
    const Vec3 r = normalized(cross(forward, up));
    return at + forward * p.forward + up * p.up + r * p.right;
}
// A tracking-space point in the gun's frame.
inline Local toLocal(const Vec3& at, const Vec3& forward, const Vec3& up, Vec3 p) {
    const Vec3 r = normalized(cross(forward, up)), d = p - at;
    return {dot(d, forward), dot(d, up), dot(d, r)};
}
inline float gap(Local a, Local b) {
    const float f = a.forward - b.forward, u = a.up - b.up, r = a.right - b.right;
    return std::sqrt(f * f + u * u + r * r);
}

// ---- RACK: the pump, slide, bolt or charging handle --------------------------------------------------
// The off hand grips within rackRadius of the handle and pulls it back along the barrel; back reaches at
// rackTravel * backShare, forward again under rackTravel * homeShare. One full stroke (back, then home) is
// one cycle; back() is reported on the frame the stroke reaches the back, home() on the frame it returns.
inline constexpr float backShare = .85f, homeShare = .20f;
struct Rack {
    bool holding{}, wasBack{};
    float grabAlong{}, travel{}; // where the hand gripped (along the barrel), the handle's travel now (m)
    bool backNow{}, homeNow{};   // this frame
    void update(const GunPoints& g, bool grip, Local offHand, bool offTracked) {
        backNow = homeNow = false;
        if (!grip || !offTracked) {
            if (holding && wasBack) // let go at the back: a spring sends it home
                homeNow = true, wasBack = false;
            holding = false;
            travel = 0;
            return;
        }
        if (!holding) {
            if (gap(offHand, g.rack) > g.rackRadius)
                return;
            holding = true;
            grabAlong = offHand.forward;
        }
        travel = std::clamp(grabAlong - offHand.forward, 0.f, g.rackTravel);
        if (!wasBack && travel >= g.rackTravel * backShare)
            backNow = wasBack = true;
        else if (wasBack && travel <= g.rackTravel * homeShare)
            homeNow = true, wasBack = false;
    }
    float share(const GunPoints& g) const { return g.rackTravel > 0 ? travel / g.rackTravel : 0.f; }
};

// ---- CARRY: a shell or a magazine in the off hand ---------------------------------------------------
// The grip pressed at the pouch takes one; carried to within the port's / well's radius it goes in (the
// gun says whether it has room); let go anywhere else it falls (dropped: a look and a sound only).
enum CarryEvent : uint32_t { carryNone = 0, carryTook = 1, carryIn = 2, carryDropped = 4 };
struct Carry {
    bool carrying{}, gripWas{};
    uint64_t took{}, inserted{}, dropped{};
    uint32_t update(bool grip, bool atPouch, bool atPort, bool room) {
        uint32_t e = carryNone;
        const bool press = grip && !gripWas;
        gripWas = grip;
        if (!carrying) {
            if (press && atPouch) {
                carrying = true;
                ++took;
                e |= carryTook;
            }
            return e;
        }
        if (!grip) {
            carrying = false;
            ++dropped;
            return e | carryDropped;
        }
        if (atPort && room) {
            carrying = false;
            ++inserted;
            e |= carryIn;
        }
        return e;
    }
};

// ---- the off hand's places on the body ------------------------------------------------------------
// From holster::bodyPoint (below the eyes in standingEye metres, side + right, ahead + front). Measured on the
// drawn suit (holster.hpp legHolster*, the same frame; cut14 the shipped 23.7k suit): the right-leg holster's
// mouth .80 under the eyes, .285 right, .07 behind; the left thigh's pouch panel y .62-.86 of the suit (.80-1.03 under), .16-.26 left; the belt's
// front pouches y .96-1.08 (.59-.70 under), within .16 of the centre line, ahead.
using holster::legHolsterAhead;
using holster::legHolsterBelow;
using holster::legHolsterSide;
inline bool atMagPouch(holster::BodyPoint p) {
    return std::isfinite(p.below) && p.below >= .70f && p.below <= 1.04f && -p.side >= .08f && -p.side <= .34f &&
           std::abs(p.ahead) <= .22f;
}
inline bool atShellPouch(holster::BodyPoint p) {
    return std::isfinite(p.below) && p.below >= .48f && p.below <= .74f && std::abs(p.side) <= .24f && p.ahead >= -.05f &&
           p.ahead <= .35f;
}
// Where the holstered pistol is drawn (the model's own holster: the grip above the mouth, the muzzle and
// can down through it, the slide's top facing forward). No holster mesh: the PUNISHER's suit has it.
inline void legHolsterPose(Vec3 head, const holster::BodyYaw& body, Vec3& at, Vec3& forward, Vec3& up) {
    const float k = std::isfinite(body.scale) && body.scale > .5f ? body.scale : 1.f;
    const Vec3 f = body.forward(), r = body.right();
    at = head + Vec3{0, -(legHolsterBelow - .05f) * k, 0} + r * (legHolsterSide * k) + f * (legHolsterAhead * k);
    forward = normalized(Vec3{0, -1, 0} + f * .04f);
    up = f;
}

// ---- PUMP SHOTGUN --------------------------------------------------------------------------------
enum Chamber : uint32_t { chamberEmpty = 0, chamberLive, chamberSpent };
struct Pump {
    uint32_t tube = pumpShot.rounds; // shells in the magazine tube
    uint32_t chamber = chamberLive;
    Rack pump;
    Carry shell;
    float cool{};
    bool trigWas{};
    uint64_t shots{}, dryPulls{}, racks{}, ejected{}, loaded{};
    // Events this frame
    bool fired{}, dry{}, rackedBack{}, rackedHome{}, ejectedLive{}, shellIn{};
    uint32_t carry{};
    // trigger: the gun hand's; offGrip / offHand (the gun's frame) / offTracked: the off hand's;
    // atPouch: the off hand at the belt's shells (atShellPouch).
    void update(float dt, bool trigger, bool offGrip, Local offHand, bool offTracked, bool atPouch) {
        const GunPoints g = pointsOf(pumpShotgun);
        fired = dry = rackedBack = rackedHome = ejectedLive = shellIn = false;
        const float t = std::isfinite(dt) ? std::clamp(dt, 0.f, .1f) : 0.f;
        cool = std::max(0.f, cool - t);
        // A shell in the off hand is never the pump's grip; the pump's hand never takes a shell.
        const bool nearPort = offTracked && gap(offHand, g.port) <= g.portRadius;
        const bool nearForend = offTracked && gap(offHand, g.rack) <= g.rackRadius;
        carry = shell.update(offGrip && offTracked, atPouch && !pump.holding && !nearForend, nearPort, tube < pumpShot.rounds);
        if (carry & carryIn) {
            ++tube;
            ++loaded;
            shellIn = true;
        }
        pump.update(g, offGrip && !shell.carrying && !(carry & carryIn), offHand, offTracked);
        if (pump.backNow) { // the chamber's shell out of the ejection port
            rackedBack = true;
            if (chamber != chamberEmpty) {
                ejectedLive = chamber == chamberLive;
                ++ejected;
            }
            chamber = chamberEmpty;
        }
        if (pump.homeNow) {
            rackedHome = true;
            ++racks;
            if (tube > 0) {
                --tube;
                chamber = chamberLive;
            }
        }
        const bool press = trigger && !trigWas;
        trigWas = trigger;
        if (!press || cool > 0)
            return;
        if (chamber == chamberLive && pump.travel <= g.rackTravel * homeShare) {
            chamber = chamberSpent; // stays in until the pump is racked: no automatic anything
            cool = pumpShot.cooldown;
            ++shots;
            fired = true;
        } else {
            ++dryPulls;
            dry = true;
        }
    }
    bool ready() const { return chamber == chamberLive && !pump.holding; }
};

// ---- MAGAZINE GUNS: the pistol, the sniper rifle, the assault rifle -------------------------------
enum Cycle : uint32_t { semi = 0, bolt, automatic };
inline constexpr uint32_t cycleOf(uint32_t arm) {
    return arm == sniperRifle ? bolt : arm == assaultRifle ? automatic : semi;
}
struct MagGun {
    uint32_t arm = suppressedPistol;
    bool magIn = true;
    uint32_t magRounds{};
    bool chambered = true;  // a live round in the chamber
    bool locked{};          // the slide / bolt held back (empty after the last round)
    bool needCycle{};       // the bolt rifle: a spent case in the chamber until the bolt is worked
    Rack rack;
    Carry mag;
    float cool{};
    bool trigWas{}, ejectWas{};
    uint64_t shots{}, dryPulls{}, drops{}, seats{}, racks{};
    bool fired{}, dry{}, magDropped{}, magSeated{}, rackedBack{}, rackedHome{}, ejectedLive{};
    uint32_t carry{};
    void arm_(uint32_t a) {
        *this = MagGun{};
        arm = a == sniperRifle || a == assaultRifle ? a : suppressedPistol;
        magRounds = shotOf(arm).rounds - 1; // a full magazine and one up the spout
    }
    uint32_t capacity() const { return shotOf(arm).rounds; }
    uint32_t rounds() const { return (magIn ? magRounds : 0) + (chambered ? 1u : 0u); }
    // eject: the eject button held (live.txt mag_eject's button on the gun hand); atPouch: the off hand at
    // the thigh's magazine pouch (atMagPouch).
    void update(float dt, bool trigger, bool eject, bool offGrip, Local offHand, bool offTracked, bool atPouch) {
        const GunPoints g = pointsOf(arm);
        const hero007::GunSpec& s = shotOf(arm);
        fired = dry = magDropped = magSeated = rackedBack = rackedHome = ejectedLive = false;
        const float t = std::isfinite(dt) ? std::clamp(dt, 0.f, .1f) : 0.f;
        cool = std::max(0.f, cool - t);
        if (eject && !ejectWas && magIn) { // the magazine falls free (its rounds with it)
            magIn = false;
            magRounds = 0;
            ++drops;
            magDropped = true;
        }
        ejectWas = eject;
        const bool nearWell = offTracked && gap(offHand, g.magWell) <= g.wellRadius;
        const bool nearRack = offTracked && gap(offHand, g.rack) <= g.rackRadius;
        carry = mag.update(offGrip && offTracked, atPouch && !rack.holding && !nearRack, nearWell, !magIn);
        if (carry & carryIn) {
            magIn = true;
            magRounds = s.rounds;
            ++seats;
            magSeated = true;
        }
        rack.update(g, offGrip && !mag.carrying && !(carry & carryIn), offHand, offTracked);
        if (rack.backNow) {
            rackedBack = true;
            ejectedLive = chambered;
            chambered = false;
            needCycle = false;
            locked = false;
        }
        if (rack.homeNow) {
            rackedHome = true;
            ++racks;
            if (magIn && magRounds > 0) {
                --magRounds;
                chambered = true;
            }
        }
        const bool held = trigger, press = trigger && !trigWas;
        trigWas = trigger;
        const bool wants = cycleOf(arm) == automatic ? held : press;
        if (!wants || cool > 0)
            return;
        if (!chambered || locked || needCycle || rack.travel > g.rackTravel * homeShare) {
            if (press) {
                ++dryPulls;
                dry = true;
            }
            return;
        }
        ++shots;
        fired = true;
        cool = cycleOf(arm) == automatic ? autoInterval : s.cooldown;
        if (cycleOf(arm) == bolt) { // the spent case stays until the bolt is worked
            needCycle = true;
            chambered = false; // a spent case, not a round
            return;
        }
        if (magIn && magRounds > 0) { // the action cycles itself
            --magRounds;
        } else {
            chambered = false;
            locked = true; // slide / bolt lock: empty, held open
        }
    }
};

// ---- OPTICS -----------------------------------------------------------------------------------------
// The lens's picture for one eye. The eye image is the game's frame for that eye (its tangent field of
// view fov: left/right/down/up, OpenXR's angles). The scope's axis points at `axis` (unit, tracking
// space); its lens is a disk of radius lensRadius at lensCentre, facing the eye along -axis.
struct EyeView {
    Pose eye{};
    float tanLeft{-1}, tanRight{1}, tanDown{-1}, tanUp{1}; // tan of the eye's half angles (left/down negative)
};
// A tracking-space point's place in the eye image (0-1 across, 0-1 down), and whether it is ahead.
inline bool project(const EyeView& v, Vec3 p, float& u, float& w) {
    const Vec3 d = v.eye.orientation.conjugate().rotate(p - v.eye.position);
    if (!(d.z < -1e-4f))
        return false;
    const float x = d.x / -d.z, y = d.y / -d.z;
    u = (x - v.tanLeft) / (v.tanRight - v.tanLeft);
    w = (v.tanUp - y) / (v.tanUp - v.tanDown);
    return std::isfinite(u) && std::isfinite(w);
}
struct ScopeView {
    bool ok{};
    float lensU{}, lensW{}, lensRadiusU{}, lensRadiusW{};  // the lens's disk in the eye image (where it is drawn)
    float srcU{}, srcW{}, srcRadiusU{}, srcRadiusW{};      // the patch magnified into it (along the axis, far away)
    float shade{};  // 0 clear to 1 black: the eye box's shadow (the eye off the scope's axis)
    float zoom{};
};
// The eye box: the eye within eyeBox of the axis line sees the full picture, the shadow closes by eyeBoxEdge.
inline constexpr float eyeBox = .006f, eyeBoxEdge = .018f;
inline constexpr float zoomChoices[4] = {4.f, 6.f, 8.f, 2.5f}; // live.txt scope_zoom: 4X (default), 6X, 8X, 2.5X
inline float zoomOf(uint32_t choice) { return zoomChoices[choice < 4 ? choice : 0]; }
inline ScopeView scopeView(const EyeView& v, Vec3 lensCentre, Vec3 axis, float lensRadius, float zoom) {
    ScopeView s{};
    if (!finite(lensCentre) || !finite(axis) || length(axis) < .5f || !(lensRadius > 0) || !(zoom >= 1.f))
        return s;
    const Vec3 a = normalized(axis);
    float u, w;
    if (!project(v, lensCentre, u, w))
        return s;
    const float dist = length(lensCentre - v.eye.position);
    if (!(dist > 1e-3f))
        return s;
    const float spanU = v.tanRight - v.tanLeft, spanW = v.tanUp - v.tanDown;
    // The lens's disk: its angular radius from the eye (a lens facing the eye; a tilted one is drawn round).
    const float lensTan = lensRadius / dist;
    s.lensU = u, s.lensW = w;
    s.lensRadiusU = 2.f * lensTan / spanU;
    s.lensRadiusW = 2.f * lensTan / spanW;
    // The patch: far along the axis, the lens's angle divided by the zoom.
    float su, sw;
    if (!project(v, lensCentre + a * 1000.f, su, sw))
        return s;
    const float srcTan = std::tan(std::atan(lensTan) / zoom);
    s.srcU = su, s.srcW = sw;
    s.srcRadiusU = 2.f * srcTan / spanU;
    s.srcRadiusW = 2.f * srcTan / spanW;
    // The eye's distance from the axis line through the lens.
    const Vec3 rel = v.eye.position - lensCentre;
    const float off = length(rel - a * dot(rel, a));
    s.shade = std::clamp((off - eyeBox) / (eyeBoxEdge - eyeBox), 0.f, 1.f);
    if (dot(v.eye.position - lensCentre, a) > 0) // the eye in front of the lens: the scope faces away
        s.shade = 1.f;
    s.zoom = zoom;
    s.ok = true;
    return s;
}
// The red dot: collimated, so it lies where the eye's ray parallel to the axis crosses the lens's plane.
// ok false: that ray misses the lens's window (the dot is not seen, as through a real sight).
struct DotOnLens {
    bool ok{};
    Vec3 at{};       // on the lens (tracking space)
    float offset{};  // from the lens centre (m)
};
inline DotOnLens redDot(Vec3 eye, Vec3 lensCentre, Vec3 axis, float lensRadius) {
    DotOnLens d{};
    if (!finite(eye) || !finite(lensCentre) || !finite(axis) || length(axis) < .5f || !(lensRadius > 0))
        return d;
    const Vec3 a = normalized(axis);
    const float along = dot(lensCentre - eye, a); // the lens's plane is normal to the axis
    if (!(along > 1e-3f))
        return d;
    d.at = eye + a * along;
    d.offset = length(d.at - lensCentre);
    d.ok = d.offset <= lensRadius;
    return d;
}

// The eye pass's 12 constants for one eye (native_eye_gpu setEyeOptics): [0-3] the lens's disk (u, w, radius u, w),
// [4-7] the patch's centre (u, w), the zoom, the mode (0 none, 1 scope, 2 red dot), [8-11] the exit pupil's shadow,
// the dot's radius (u, w), 0. The scope: scopeView; the red dot: redDot (on the lens where the eye's ray parallel to the
// axis crosses it, so it stays on the target as the head moves - no parallax), the dot hidden when that ray misses.
inline constexpr float dotTan = .0012f; // the dot's angular radius (about 4 MOA: a few pixels in a headset)
inline std::array<float, 12> opticConstants(uint32_t mode, const EyeView& v, Vec3 lensCentre, Vec3 axis, float lensRadius,
                                            float zoom) {
    std::array<float, 12> c{};
    if (mode != 1 && mode != 2)
        return c;
    const ScopeView s = scopeView(v, lensCentre, axis, lensRadius, mode == 1 ? zoom : 1.f);
    if (!s.ok)
        return c;
    const float spanU = v.tanRight - v.tanLeft, spanW = v.tanUp - v.tanDown;
    c[0] = s.lensU, c[1] = s.lensW, c[2] = s.lensRadiusU, c[3] = s.lensRadiusW;
    c[4] = s.srcU, c[5] = s.srcW, c[6] = mode == 1 ? s.zoom : 1.f, c[7] = static_cast<float>(mode);
    c[8] = mode == 1 ? s.shade : 0.f;
    if (mode == 2) {
        const DotOnLens d = redDot(v.eye.position, lensCentre, axis, lensRadius);
        float u, w;
        if (d.ok && project(v, d.at, u, w)) {
            c[4] = u, c[5] = w;
            c[9] = 2.f * dotTan / spanU, c[10] = 2.f * dotTan / spanW;
        } else {
            c[4] = -10.f, c[5] = -10.f; // off the lens: the shader draws only the window's coating
        }
    }
    for (const float x : c)
        if (!std::isfinite(x))
            return std::array<float, 12>{};
    return c;
}

} // namespace spidy::punisher078
