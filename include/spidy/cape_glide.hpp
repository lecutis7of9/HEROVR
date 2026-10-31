#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
//
// Version 0.0.7 BATMAN MOVEMENT: his grapple gun and his cape glide, with no web and no flight.
//  - GRAPPLE GUN: with no enemy aimed (the batarang's assist) and the hand aimed above level, a
//    trigger press fires the grapple at what the hand points at (the bridge's world ray). A hit on a
//    rooftop or a ledge within reach pulls him to it along a taut line, then pops him up and over
//    the edge; a miss buzzes. It drives the same safe velocity path FLY MODE does.
//  - CAPE GLIDE (Arkham Shadow style): while airborne, both arms held out to the sides spread the
//    cape: a slow fall with forward speed. Lean or dip one arm to turn that way; tuck the arms in
//    to dive and build speed; raise the spread arms to pull up, trading speed for a little height
//    (it stalls back to a glide when the speed runs out). Landing, dropping the arms to the sides
//    or a grapple ends it.
// The VR module reads the hands (tracking space, hero007_play's grapple press) and sends the asks
// bits below on game_swing::Command::asks; the bridge (game_swing.cpp) runs Glide and Grapple next
// to FLY MODE and publishes Data through SpidyCapeSample (optional: a missing export, no line).
// Pure; no game memory, no address.
#include "spidy/math.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace spidy::cape_glide {

// ---- The swing command's asks bits (bits 0-4 are the game move, SUPERSONIC and HULK LEAP's) ----
inline constexpr uint32_t askBatman = 1u << 5;               // BATMAN is on: glide allowed, no flight
inline constexpr uint32_t askGrapple[2] = {1u << 6, 1u << 7}; // that hand's grapple trigger is held
// Version 0.0.72 (TEST): the grapple without the cape (the PUNISHER's hook, DAREDEVIL's baton cast):
// askGrapple[i] then zips that hand along its aim, no glide.
inline constexpr uint32_t askGrappleOnly = 1u << 8;
inline constexpr uint32_t asksMost = (1u << 9) - 1u; // every bit in use (game_swing::valid)
inline uint32_t pack(bool batman, bool left, bool right) {
    return batman ? askBatman | (left ? askGrapple[0] : 0u) | (right ? askGrapple[1] : 0u) : 0u;
}
// 0.0.74 test 10 GLIDE TILT: the swing command's glide word (game_swing::Command::reserved7[0], Version 7;
// an older VR module sends 0 and an older bridge never reads it - the poses' pitch either way).
inline constexpr uint32_t glideTiltBit = 1u;
inline uint32_t glideWord(bool tilt) { return tilt ? glideTiltBit : 0u; }
inline bool glideTilt(uint32_t word) { return (word & glideTiltBit) != 0; }
inline uint32_t packGrappleOnly(bool left, bool right) {
    return left || right ? askGrappleOnly | (left ? askGrapple[0] : 0u) | (right ? askGrapple[1] : 0u) : 0u;
}

// One sentence each (the cheat sheet, TRAINING and the wrist HUD).
inline constexpr const char* glideHelp =
    "In the air, hold both arms out to glide; dip an arm to turn, tuck to dive, raise them to pull up, drop them to let go.";
inline constexpr const char* grappleHelp =
    "Aim a hand above a rooftop or ledge with no enemy in your sights and pull the trigger to grapple up to it.";

inline constexpr float pi = 3.14159265f;
inline constexpr float g = 9.81f;
inline float radians(float d) { return d * pi / 180.f; }

// ---- The glide ----
enum Pose : uint32_t { none = 0, glide = 1, dive = 2, pullUp = 3 };
inline const char* poseName(uint32_t p) {
    static constexpr const char* names[4] = {"NONE", "GLIDE (ARMS OUT)", "DIVE (ARMS TUCKED)", "PULL UP (ARMS RAISED)"};
    return names[p < 4 ? p : 0];
}
struct Tune {
    // The pose (metres in the body's frame: x right, y up from the eyes, z forward; the shoulders
    // are about -0.25).
    float spanEnter = 1.0f, spanExit = .80f;    // arms out: the hands this far apart side to side
    float armLow = -.75f, armHigh = .45f;       // arms out: both hands between these heights
    float depthMost = .55f;                     // arms out: neither hand further than this in front or behind
    float raised = -.02f;                       // the hands' mean height over this: PULL UP
    float tuckSpan = .60f, tuckLow = -.72f;     // tucked: closer than this, above tuckLow
    float dropBelow = -.80f;                    // both hands under this: arms dropped, let go
    float enterHold = .10f, dropHold = .15f;    // seconds a pose is held to count
    float airborneHold = .12f;                  // seconds in the air before a glide can start
    // The flight (pitch is up from level, degrees).
    float pitchGlide = -10, pitchDive = -50, pitchPull = 25, pitchStall = -4;
    float pitchRate = 110;                      // degrees a second toward the pose's pitch
    float speedStart = 9, speedMin = 6, speedMax = 30, stallSpeed = 9;
    float drag = .0089f;                        // per metre: a glide settles near 14 m/s, a dive near 30
    float turnMost = 85;                        // degrees a second at full bank
    float bankFull = 35, bankDead = 6;          // degrees of arm tilt for full turn / for none
    float stuckSpeed = 2.f, stuckHold = .45f;   // the body barely moving this long while gliding: blocked, end
    // 0.0.74 test 10 GLIDE TILT (a tester: "dive pitch based on the roll of your controllers, like Shadow"):
    // the controllers' tilt about the body's side-to-side axis (see handTilt) sets the pitch directly -
    // pitchGlide less the tilt past tiltDead, times tiltGain, clamped to [pitchDive, pitchPull] - smoothed
    // over tiltSmooth seconds; with no hand's tilt it is held tiltHold seconds, then the poses' pitch again.
    float tiltDead = 5, tiltGain = 1, tiltSmooth = .10f, tiltHold = .15f;
    float tiltRate = 360; // degrees a second at most (a reacquire or a mode switch is no snap)
};
struct HandIn {
    bool tracked{};
    Vec3 offset{}; // the grip against the head, world-oriented (metres)
    Quat aim{};    // 0.0.74 test 10 GLIDE TILT: the controller's aim orientation (world, -Z forward, +Y up)
};
struct In {
    float seconds{};
    bool allowed{};    // BATMAN on, in play, nothing else driving (no grab, no game move)
    bool airborne{};   // the body is in the air (not grounded, not on a wall)
    Vec3 look{0, 0, -1};
    Vec3 velocity{};   // the body's velocity now (world)
    HandIn hands[2]{}; // 0 left, 1 right
    bool tilt{};       // 0.0.74 test 10 GLIDE TILT on (the swing command's glide word); off: the poses' pitch
};
struct Out {
    bool drive{};        // the glide is on: velocity is the body's
    Vec3 velocity{};
    uint32_t pose{};     // Pose
    float speed{}, pitch{}, bank{}; // m/s, degrees, degrees (+: turning left)
    bool started{}, ended{};
    bool tilting{}; // 0.0.74 test 10 GLIDE TILT: the pitch is the controllers' tilt's now
};
struct Reading {
    bool out{}, tucked{}, dropped{}, raised{};
    float span{}, bank{}; // metres, degrees (+: the right hand higher, a left turn)
};
// The arms against the head in the body's frame (flat look).
inline Reading read(const HandIn hands[2], Vec3 look, const Tune& t, bool engaged) {
    Reading r;
    if (!hands[0].tracked || !hands[1].tracked || !finite(hands[0].offset) || !finite(hands[1].offset))
        return r;
    Vec3 f{look.x, 0, look.z};
    f = finite(f) && length(f) > .1f ? normalized(f) : Vec3{0, 0, -1};
    const Vec3 right{-f.z, 0, f.x};
    Vec3 l[2];
    for (int i = 0; i < 2; ++i)
        l[i] = {dot(hands[i].offset, right), hands[i].offset.y, dot(hands[i].offset, f)};
    r.span = l[1].x - l[0].x; // right hand right of the left one
    const float sideSpan = std::max(std::fabs(r.span), 1e-3f);
    r.bank = std::atan2(l[1].y - l[0].y, sideSpan) * 180.f / pi;
    const bool heights = l[0].y > t.armLow && l[1].y > t.armLow && l[0].y < t.armHigh && l[1].y < t.armHigh;
    const bool depth = std::fabs(l[0].z) < t.depthMost && std::fabs(l[1].z) < t.depthMost;
    r.out = r.span > (engaged ? t.spanExit : t.spanEnter) && heights && depth;
    r.raised = r.out && (l[0].y + l[1].y) * .5f > t.raised;
    r.dropped = l[0].y < t.dropBelow && l[1].y < t.dropBelow;
    r.tucked = !r.out && !r.dropped && std::fabs(r.span) < t.tuckSpan && l[0].y > t.tuckLow && l[1].y > t.tuckLow;
    return r;
}

// 0.0.74 test 10 GLIDE TILT: one controller's tilt about the body's side-to-side axis, degrees, + the
// controller's top tipped forward (nose down: dive), - back (pull up). The glide holds the arms out to
// the sides, so the wing-like twist is a forearm roll about that axis: with the controller pointing forward
// (the usual hold) it is the controller's own nose pitch, with it pointing out along the arm it is the
// controller's own roll - the same number either way. Read from the controller's up axis in the plane of
// the flat look and world up; the forward axis if the up axis lies along the arm. False: no reading.
inline bool handTilt(Quat q, Vec3 look, float& degrees) {
    const float n = q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w;
    if (!std::isfinite(n) || n < .9f || n > 1.1f)
        return false;
    Vec3 f{look.x, 0, look.z};
    if (!finite(f) || length(f) < .1f)
        return false;
    f = normalized(f);
    const Vec3 up = q.rotate({0, 1, 0}), fwd = q.rotate({0, 0, -1});
    if (!finite(up) || !finite(fwd))
        return false;
    float a{};
    if (const float uf = dot(up, f), uu = up.y; uf * uf + uu * uu > .25f)
        a = std::atan2(uf, uu);
    else if (const float ff = dot(fwd, f), fu = fwd.y; ff * ff + fu * fu > .25f)
        a = -std::atan2(fu, ff);
    else
        return false;
    degrees = a * 180.f / pi;
    return std::isfinite(degrees);
}
// The tilt (degrees, + nose down) to the glide's pitch (degrees, + up): direct, angle in = pitch out.
inline float tiltPitch(float tilt, const Tune& t) {
    if (!std::isfinite(tilt))
        return t.pitchGlide;
    const float a = std::fabs(tilt) <= t.tiltDead ? 0.f : tilt - std::copysign(t.tiltDead, tilt);
    return std::clamp(t.pitchGlide - a * t.tiltGain, t.pitchDive, t.pitchPull);
}

class Glide {
  public:
    Tune tune{};
    Out update(const In& in) {
        Out o;
        const float dt = std::isfinite(in.seconds) ? std::clamp(in.seconds, 0.f, .1f) : 0.f;
        air_ = in.allowed && in.airborne ? air_ + dt : 0.f;
        if (!in.allowed || !in.airborne) {
            if (on_)
                o.ended = true;
            reset();
            o.pose = none;
            return o;
        }
        const Reading r = read(in.hands, in.look, tune, on_);
        if (!on_) {
            outHeld_ = r.out ? outHeld_ + dt : 0.f;
            if (outHeld_ >= tune.enterHold && air_ >= tune.airborneHold)
                start(in);
            if (!on_)
                return o;
            o.started = true;
        } else {
            // Let go: the arms dropped to the sides (held a moment), or the hands lost.
            dropHeld_ = r.dropped || (!in.hands[0].tracked || !in.hands[1].tracked) ? dropHeld_ + dt : 0.f;
            // Blocked: the body is barely moving although the glide drives it (a wall, a roof's lip).
            const float moved = finite(in.velocity) ? length(in.velocity) : 0.f;
            stuck_ = since_ > .3f && moved < tune.stuckSpeed ? stuck_ + dt : 0.f;
            if (dropHeld_ >= tune.dropHold || stuck_ >= tune.stuckHold) {
                reset();
                o.ended = true;
                return o;
            }
        }
        since_ += dt;
        // The pose: tucked dives, raised pulls up (while there is speed to trade), out glides; any
        // in-between keeps the last pose (a hand passing through is no change).
        if (r.tucked)
            pose_ = dive;
        else if (r.raised)
            pose_ = pullUp;
        else if (r.out)
            pose_ = glide;
        float target = tune.pitchGlide;
        if (pose_ == dive)
            target = tune.pitchDive;
        else if (pose_ == pullUp)
            target = speed_ > tune.stallSpeed ? tune.pitchPull : tune.pitchStall;
        float rate = tune.pitchRate;
        // 0.0.74 test 10 GLIDE TILT: the arms out (not tucked: a tuck still dives), the controllers' tilt is
        // the pitch - the mean of both hands' (one hand's while the other blinks), smoothed; none read: the
        // last held tiltHold seconds, then the poses' pitch above. A climb past the stall stalls as before.
        o.tilting = in.tilt && tiltTarget(in, r.tucked, dt, target);
        if (o.tilting)
            rate = tune.tiltRate;
        const float step = rate * dt;
        pitch_ += std::clamp(target - pitch_, -step, step);
        if (!std::isfinite(pitch_))
            pitch_ = tune.pitchGlide;
        // Energy: gravity along the path, less drag.
        const float th = radians(pitch_);
        speed_ += (-g * std::sin(th) - tune.drag * speed_ * speed_) * dt;
        speed_ = std::clamp(speed_, tune.speedMin, tune.speedMax);
        // Steering: the arms' tilt (only with the arms out; a tuck dives straight).
        float bank = 0;
        if (r.out) {
            const float a = std::fabs(r.bank);
            bank = a <= tune.bankDead ? 0.f : std::copysign(std::min(1.f, (a - tune.bankDead) / (tune.bankFull - tune.bankDead)), r.bank);
        }
        bank_ = bank * tune.bankFull;
        const float yaw = radians(bank * tune.turnMost) * dt; // +: left
        const float c = std::cos(yaw), s = std::sin(yaw);
        heading_ = normalized(Vec3{heading_.x * c + heading_.z * s, 0, -heading_.x * s + heading_.z * c});
        o.drive = true;
        o.velocity = heading_ * (speed_ * std::cos(th)) + Vec3{0, speed_ * std::sin(th), 0};
        o.pose = pose_;
        o.speed = speed_;
        o.pitch = pitch_;
        o.bank = bank_;
        return o;
    }
    bool on() const { return on_; }
    Vec3 heading() const { return heading_; }
    void reset() {
        on_ = false;
        outHeld_ = dropHeld_ = stuck_ = since_ = 0;
        pose_ = none;
        bank_ = 0;
        tiltHas_ = false;
        tiltLost_ = 0;
    }

  private:
    // GLIDE TILT: true and target set while the tilt drives the pitch.
    bool tiltTarget(const In& in, bool tucked, float dt, float& target) {
        float sum = 0;
        int count = 0;
        for (const auto& h : in.hands) {
            float d{};
            if (h.tracked && handTilt(h.aim, in.look, d)) {
                sum += d;
                ++count;
            }
        }
        if (count) {
            const float raw = sum / float(count);
            const float k = tune.tiltSmooth > 1e-4f ? 1.f - std::exp(-dt / tune.tiltSmooth) : 1.f;
            tilt_ = tiltHas_ && std::isfinite(tilt_) ? tilt_ + (raw - tilt_) * k : raw;
            tiltHas_ = true;
            tiltLost_ = 0;
        } else if (tiltHas_) {
            tiltLost_ += dt;
            if (tiltLost_ > tune.tiltHold)
                tiltHas_ = false; // the poses' pitch again; the next reading starts afresh
        }
        if (!tiltHas_ || tucked)
            return false;
        target = tiltPitch(tilt_, tune);
        if (target > tune.pitchStall && speed_ <= tune.stallSpeed)
            target = tune.pitchStall; // no climb without the speed to trade (the PULL UP's stall)
        return true;
    }
    bool tiltHas_{};
    float tilt_{}, tiltLost_{};
    void start(const In& in) {
        on_ = true;
        since_ = 0;
        pose_ = glide;
        const Vec3 v = finite(in.velocity) ? in.velocity : Vec3{};
        const Vec3 flat{v.x, 0, v.z};
        Vec3 look{in.look.x, 0, in.look.z};
        look = finite(look) && length(look) > .1f ? normalized(look) : Vec3{0, 0, -1};
        heading_ = length(flat) > 1.5f ? normalized(flat) : look;
        speed_ = std::clamp(std::max(length(flat), tune.speedStart), tune.speedMin, tune.speedMax);
        const float path = std::atan2(v.y, std::max(length(flat), 1.f)) * 180.f / pi;
        pitch_ = std::clamp(path, tune.pitchDive, tune.pitchGlide);
    }
    bool on_{};
    float air_{}, outHeld_{}, dropHeld_{}, stuck_{}, since_{};
    uint32_t pose_{};
    Vec3 heading_{0, 0, -1};
    float speed_{}, pitch_{}, bank_{};
};

// ---- The grapple gun ----
// The VR side: a press counts as a grapple when no enemy is aimed and the hand points this far
// above level (the rest of the presses throw a batarang, as before).
inline constexpr float grappleAimUp = .17f; // sine of about 10 degrees
inline bool grappleAim(Vec3 aim, bool enemyAimed) {
    const float n = length(aim);
    return !enemyAimed && n > 1e-4f && finite(aim) && aim.y / n >= grappleAimUp;
}
// Version 0.0.73 test 3 GRAPPLE REACH (the user, test 2: "you cant reach a building like across a street"):
// 45 m (0.0.7 to 0.0.73 test 2) to 80 m - a building across a wide avenue, aimed up at its roof line,
// is 50-70 m from the hand. This was THE cap: the bridge's ray is cast this far and a hit past it is
// refused (hookable). Still inside the ray bridge's own 150 m limit (native_rays::valid) and under
// SPIDER-MAN's 100 m web reach (SwingConfig::maxRange). The pull's clock already grows with the trip
// (GrappleTune: the trip at full speed plus timeoutSpare), so an 80 m pull completes. That the game
// has collision for a building 80 m off is CANDIDATE (the webs' 100 m rays are the same bridge).
inline constexpr float reachMost = 80, reachLeast = 2.5f;
inline constexpr float reachMostWas = 45; // test 2's
// A hit the bridge's ray found: a fixed surface within reach (never the air, never a moving body).
inline bool hookable(Vec3 from, Vec3 point, bool airAnchor, bool fixedSurface) {
    if (airAnchor || !fixedSurface || !finite(from) || !finite(point))
        return false;
    const float d = length(point - from);
    return d >= reachLeast && d <= reachMost;
}
enum Phase : uint32_t { idle = 0, pull = 1, vault = 2 };
struct GrappleTune {
    float speed = 24, accel = 55, near = 1.0f, slowFrom = 3.5f;
    // Version 0.0.72 SUSTAINED PULL (the user, 11:09: "no brief tug then drop"): the time allowed grows
    // with the distance (timeout at least, else the trip at full speed plus timeoutSpare); progress is
    // the distance to the target coming down (not any movement), checked only after stuckGrace.
    float timeout = 3.0f, timeoutSpare = 1.5f, stuckGrace = .6f, stuckWindow = .5f, stuckMetres = .45f;
    float vaultUp = 7.5f, vaultOver = 4.f, vaultSeconds = .32f;
    float roofStand = 1.1f, ledgeAbove = 1.6f, ledgeOut = .6f;
    // 0.0.74 test 10 DD QUICK GRAPPLE (dd_grapple074.hpp): rampSeconds eases the pull's acceleration in from 0
    // (a smoothstep, no jerk) and blendSeconds eases a redirected pull's velocity from the carried one (carry())
    // onto the new line. Both 0 (every other hero): the pull exactly as before.
    float rampSeconds = 0, blendSeconds = 0;
};
struct GrappleOut {
    bool drive{};
    Vec3 velocity{};
    uint32_t phase{};
    Vec3 anchor{};
    bool started{}, arrived{}, ended{};
};
inline constexpr float reelNear = .25f; // 0.0.73: a reel's stop is reached within this
class Grapple {
  public:
    GrappleTune tune{};
    // Fire at a hook point (its surface normal); the body at from. False: no start.
    bool start(Vec3 body, Vec3 anchor, Vec3 normal, uint32_t hand) {
        if (!finite(body) || !finite(anchor))
            return false;
        Vec3 n = finite(normal) && length(normal) > .1f ? normalized(normal) : Vec3{0, 1, 0};
        anchor_ = anchor;
        hand_ = hand;
        roof_ = n.y > .6f;
        Vec3 out{n.x, 0, n.z};
        out = length(out) > .1f ? normalized(out) : Vec3{};
        over_ = out * -1.f;
        // A rooftop: stand on it; a wall face: just in front of it and over its edge height.
        target_ = roof_ ? anchor + Vec3{0, tune.roofStand, 0} : anchor + out * tune.ledgeOut + Vec3{0, tune.ledgeAbove, 0};
        if (roof_) {
            // Roof: the edge comes in from where we are.
            Vec3 toward{anchor.x - body.x, 0, anchor.z - body.z};
            over_ = length(toward) > .1f ? normalized(toward) : Vec3{};
        }
        reel_ = false;
        phase_ = pull;
        t_ = 0;
        speed_ = 0;
        carryT_ = 0;
        carry_ = {};
        carrying_ = false;
        progressAt_ = body;
        progressT_ = 0;
        first_ = normalized(target_ - body);
        progressD_ = length(target_ - body);
        allowed_ = std::max(tune.timeout, progressD_ / std::max(tune.speed, 1.f) + tune.timeoutSpare);
        return true;
    }
    GrappleOut update(float seconds, Vec3 body, Vec3 velocity, bool allowed) {
        GrappleOut o;
        const float dt = std::isfinite(seconds) ? std::clamp(seconds, 0.f, .1f) : 0.f;
        if (phase_ == idle)
            return o;
        if (!allowed || !finite(body)) {
            stop();
            o.ended = true;
            return o;
        }
        t_ += dt;
        o.anchor = anchor_;
        if (phase_ == pull) {
            if (t_ <= dt)
                o.started = true;
            const Vec3 to = target_ - body;
            const float d = length(to);
            // Arrived (or passed it), or out of time, or stuck (a wall in the way): the vault, or off.
            progressT_ += dt;
            bool stuck = false;
            if (t_ < tune.stuckGrace) {
                progressT_ = 0;
                progressD_ = d;
            } else if (progressT_ >= tune.stuckWindow) {
                stuck = progressD_ - d < tune.stuckMetres;
                progressAt_ = body;
                progressD_ = d;
                progressT_ = 0;
            }
            if (reel_ && (d <= reelNear || dot(to, first_) < 0 || stuck)) { // 0.0.73: a reel ends at the enemy
                stop();
                o.arrived = !stuck;
                o.ended = true;
                return o;
            }
            if (!reel_ && (d <= tune.near || dot(to, first_) < 0 || stuck)) { // a reel never vaults
                phase_ = vault;
                t_ = 0;
                o.arrived = !stuck;
            } else if (t_ >= allowed_) {
                stop();
                o.ended = true;
                return o;
            } else {
                const float now = finite(velocity) ? std::max(0.f, dot(velocity, to / d)) : 0.f;
                speed_ = std::min(tune.speed, std::max(speed_, now) + tune.accel * rampShare(t_, tune.rampSeconds) * dt);
                const float s = std::min(speed_, tune.slowFrom + d * 5.f);
                o.drive = true;
                o.velocity = to / d * s;
                if (carrying_) { // 0.0.74 test 10 DD QUICK GRAPPLE: a redirect eases from the carried velocity
                    carryT_ += dt;
                    const float k = rampShare(carryT_, tune.blendSeconds);
                    o.velocity = carry_ * (1.f - k) + o.velocity * k;
                    carrying_ = k < 1.f && finite(o.velocity);
                    if (!finite(o.velocity))
                        o.velocity = to / d * s;
                }
                o.phase = pull;
                return o;
            }
        }
        if (phase_ == vault) {
            if (t_ >= tune.vaultSeconds) {
                stop();
                o.ended = true;
                return o;
            }
            o.drive = true;
            o.velocity = Vec3{0, roof_ ? tune.vaultUp * .5f : tune.vaultUp, 0} + over_ * tune.vaultOver;
            o.phase = vault;
        }
        return o;
    }
    // Version 0.0.73 ENEMY REEL (grapple073.hpp, fix 6): the line on an enemy (anchor: his chest) closes to
    // `stop` (reelGap short of him) and ends there - no vault over him. False: no start.
    bool startReel(Vec3 body, Vec3 stop, Vec3 enemyChest, uint32_t hand) {
        if (!start(body, enemyChest, Vec3{0, 1, 0}, hand) || !finite(stop))
            return false;
        reel_ = true;
        roof_ = false;
        over_ = {};
        target_ = stop;
        first_ = normalized(target_ - body);
        progressD_ = length(target_ - body);
        allowed_ = std::max(tune.timeout, progressD_ / std::max(tune.speed, 1.f) + tune.timeoutSpare);
        if (!finite(first_)) {
            phase_ = idle;
            return false;
        }
        return true;
    }
    // 0.0.74 test 10 DD QUICK GRAPPLE: a redirect - the pull just started eases over blendSeconds from velocity
    // (the cancelled pull's, kept: no stop) onto its own line. Ignored with blendSeconds 0 or a bad velocity.
    void carry(Vec3 velocity) {
        if (phase_ != pull || !(tune.blendSeconds > 0) || !finite(velocity))
            return;
        carry_ = velocity;
        carryT_ = 0;
        carrying_ = true;
        speed_ = std::max(0.f, dot(velocity, first_)); // the speed already along the new line is kept
    }
    bool carrying() const { return carrying_; }
    // The share of the full acceleration (or of the blend) at t of `over` seconds: a smoothstep; over 0: 1.
    static float rampShare(float t, float over) {
        if (!(over > 0) || !std::isfinite(t))
            return 1.f;
        const float k = std::clamp(t / over, 0.f, 1.f);
        return k * k * (3.f - 2.f * k);
    }
    // The enemy moved: the line follows him (a reel only).
    void retarget(Vec3 stop, Vec3 enemyChest) {
        if (!reel_ || phase_ != pull || !finite(stop) || !finite(enemyChest))
            return;
        target_ = stop;
        anchor_ = enemyChest;
    }
    bool reeling() const { return reel_ && phase_ != idle; }
    void stop() { phase_ = idle; }
    uint32_t phase() const { return phase_; }
    uint32_t hand() const { return hand_; }
    Vec3 anchor() const { return anchor_; }
    Vec3 target() const { return target_; }
    float allowed() const { return allowed_; }

  private:
    uint32_t phase_{}, hand_{};
    Vec3 anchor_{}, target_{}, over_{}, first_{}, progressAt_{};
    bool roof_{}, reel_{}, carrying_{};
    Vec3 carry_{};
    float t_{}, speed_{}, progressT_{}, progressD_{}, allowed_{3.f}, carryT_{};
};

// Version 0.0.72: a pull from the ground takes off by the game's jump first (game_swing's takeoff, as
// a web's): the bridge asks for it while this is true and holds the pull until he is off.
inline bool liftFirst(uint32_t phase, bool grounded, bool collidable) { return phase == pull && grounded && collidable; }

// ---- The bridge's sample (SpidyCapeSample) ----
struct Data {
    uint32_t magic = 'C' | 'A' << 8 | 'P' << 16 | uint32_t('E') << 24; // "CAPE" (FourCC, not an address)
    uint32_t version = 1;
    uint32_t bytes = sizeof(Data);
    uint32_t gliding{}, pose{}, grappling{}, hand{};
    float speed{}, pitch{}, bank{};
    Vec3 anchor{};
    uint32_t glides{}, grapples{}, misses{}, arrivals{};
    // 0.0.74 test 10 GRAPPLE YANK: each ENEMY REEL's start - the count, the enemy (world_trace id), his feet
    // and the hero's body then (world).
    uint32_t reels{}, reserved{};
    uint64_t reelId{};
    Vec3 reelFeet{}, reelFrom{};
};

} // namespace spidy::cape_glide
