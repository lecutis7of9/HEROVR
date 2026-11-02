#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
// Made by Agent Mulder.
//
// HERO VR 0.0.76 FLIGHT (the flight rework; the SM2 build and the SM1 add-on share this file). The players' and the
// tester grundy's asks, in one place:
// - Only a hero who really flies (IRON MAN, SUPERMAN, THOR, GREEN LANTERN; hero072::flyer) flies. Grip no longer starts
//   flight, so THOR's hammer grab (a grip behind the back) is his again.
// - Take off: both hands straight up over the head (held `armsUpSeconds`), or A held `aHoldSeconds`.
// - Flying is continuous: you cruise along a heading that turns smoothly toward where your extended arm(s) point
//   (from the chest, so an arm out in front flies forward, an arm up climbs, both arms down at your sides keep the
//   heading). The left stick's forward / back is the throttle (back to a hover).
// - SUPERSONIC (when on): the left stick full forward. It is speed only: the path stays the heading, never levelled,
//   never sunk, never grounded; let go and it coasts back down to the cruise along the same path.
// - A held again in flight: a controlled descent to the ground (R3 still does it too; L3 + R3 stays the ultimate).
// - B held: the air brake (slow to a hover in place). Y held past `yHoldSeconds`: the orientation brake (the flips'
//   spin stops, the view and the body roll come back upright); a Y tap is still FLY LOOK UP.
// - Right stick left / right: rolls the BODY about the flight's own axis (the heading), never turning it. Held, it
//   keeps rolling (a full turn and on); let go, the roll stays where it is (no snap back) - unless it is within
//   `BodyRoll::snapDegrees` of level, which is exactly level (0.0.76 roll fix). Landed: level at once.
// - 0.0.76 ROLL FIX (run flight-roll-1: "past about 90 degrees the camera gets weird, at 180 the chest fills the view,
//   laggy while rolling, and wrong until he lands"): RollFrame locks the roll's world axis when the roll starts (the
//   flight's way when fast, else the head's level look) instead of following the head or the 42 Hz velocity each
//   frame; the roll is one quaternion about it, applied the same frame (no smoothing); the body turns about the HEAD
//   with the view (RollFrame::aboutHead: the feet too, not only its up), and past `clipOnDegrees` the near plane
//   steps out to `clipMetres` so the chest never fills the view; at 0 and on landing everything is cleared.
// - The right stick is the roll OR the climb, whichever way it leans most (climbStick): a roll no longer climbs.
// - While flying, A, B and Y never reach the game and the realistic / legacy flips take no A, left stick or arm swing
//   from you: no control does two things.
// - HOVER FLIGHT (0.0.76 test, pt1616u: "you just keep flying... you shouldn't move unless you use the joystick or your
//   arm. you should still be able to hover and strafe, go up or down and rotate your body freely"): nothing held and no
//   arm pointed, the velocity comes to exactly zero and the hero hangs in place (no drift, no cruise). The left stick
//   moves him the way the head faces (forward / back) and strafes (left / right); the right stick's up / down climbs and
//   sinks; an arm pointed flies him along it. The left stick full forward is still SUPERSONIC (never lower, never
//   grounded). In.hover turns it on (both games send it); without it the 0.0.76 cruise above stays as it was.
// live.txt fly_classic 1 puts the 0.0.75 flight back (grip to enter, the poses, the triggers' thrust).
// Nothing here reads or writes game memory: the mod's own state and its existing movement command (CONFIRMED by
// construction; no CANDIDATE address).
#include "spidy/math.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace spidy::fly076 {

// live.txt fly_classic: 0 the 0.0.76 flight (the default), 1 the 0.0.75 flight. Export SpidyFlyClassic 5553.
inline constexpr const char* classicHelp =
    "FLY CLASSIC OFF (the default) flies the 0.0.76 way (both hands up or A held to take off, then you hover still: "
    "the left stick moves and strafes, the right stick up or down climbs, an arm reached out steers, its left or right "
    "rolls), while ON puts the 0.0.75 flight back (grip to take off, the poses and the triggers); live, "
    "no restart.";
inline bool newFlight(uint32_t classicValue) { return classicValue != 1; }

inline constexpr float aHoldSeconds = .4f, armsUpSeconds = .25f, keepSeconds = .3f;
inline constexpr float yHoldSeconds = .5f; // fly_feel::tapSeconds: shorter is the LOOK UP tap, longer the brake
inline constexpr float armsUpAbove = .12f, armsUpWide = .40f; // metres over the head; at most this far out
inline constexpr float chestBelow = .30f, armOut = .45f, armDown = -.80f;
inline constexpr float stickDead = .20f, sonicOn = .85f, sonicOff = .60f;

inline float axis(float v) { return std::isfinite(v) ? std::clamp(v, -1.f, 1.f) : 0.f; }
inline float seconds(float dt) { return std::isfinite(dt) ? std::clamp(dt, 0.f, .1f) : 0.f; }
inline Vec3 unitOr(Vec3 v, Vec3 fallback) {
    return finite(v) && length(v) > 1e-3f ? normalized(v) : fallback;
}

// ---- The new flight's bits in the second flight word (game_swing::Command::reserved7[1], fly_feel's) ----
// fly_feel uses bits 0-2; an older swing module ignores these (and flies the 0.0.75 way).
// HOVER FLIGHT: bit 11 the hover flight, bits 3-7 the climb stick (-15..15 around 16; 0 from an older VR module: no
// climb). Bits 3-7, 11-15 and 24-31 were unused by fly_feel and by the 0.0.76 word.
enum : uint32_t { bitNew = 1u << 8, bitBrake = 1u << 9, bitHoldWay = 1u << 10, bitHover = 1u << 11, riseShift = 3,
                  throttleShift = 16 };
inline uint32_t pack(bool on, bool brake, bool holdWay, float throttle, bool hover = false, float rise = 0) {
    if (!on)
        return 0u;
    const uint32_t t = static_cast<uint32_t>(std::lround(axis(throttle) * 127.f) + 128) & 255u;
    const uint32_t r = static_cast<uint32_t>(std::lround(axis(rise) * 15.f) + 16) & 31u;
    return bitNew | (brake ? bitBrake : 0u) | (holdWay ? bitHoldWay : 0u) | (t << throttleShift) |
           (hover ? bitHover | (r << riseShift) : 0u);
}
struct Word {
    bool on{}, brake{}, holdWay{}, hover{};
    float throttle{}, rise{};
};
inline Word unpack(uint32_t w) {
    Word o;
    o.on = (w & bitNew) != 0;
    if (!o.on)
        return o;
    o.brake = (w & bitBrake) != 0;
    o.holdWay = (w & bitHoldWay) != 0;
    o.throttle = std::clamp((static_cast<float>((w >> throttleShift) & 255u) - 128.f) / 127.f, -1.f, 1.f);
    o.hover = (w & bitHover) != 0;
    const uint32_t r = (w >> riseShift) & 31u;
    o.rise = o.hover && r ? std::clamp((static_cast<float>(r) - 16.f) / 15.f, -1.f, 1.f) : 0.f;
    return o;
}

// ---- The arms ----
// Both hands straight up: each hand (its grip from the head, any frame whose up is `up`) over the head by
// armsUpAbove and within armsUpWide of the line up through it.
inline bool armsUp(const Vec3 (&offset)[2], const bool (&tracked)[2], Vec3 up) {
    up = unitOr(up, {0, 1, 0});
    for (int i = 0; i < 2; ++i) {
        if (!tracked[i] || !finite(offset[i]))
            return false;
        const float over = dot(offset[i], up);
        const Vec3 side = offset[i] - up * over;
        if (over < armsUpAbove || length(side) > armsUpWide)
            return false;
    }
    return true;
}
// Where an arm points from the chest (chestBelow under the head along `up`): a unit way when the hand is out at
// least armOut from it and not hanging down (its way's share of up above armDown), else zero.
inline Vec3 armWay(Vec3 offset, bool tracked, Vec3 up) {
    if (!tracked || !finite(offset))
        return {};
    up = unitOr(up, {0, 1, 0});
    const Vec3 d = offset + up * chestBelow;
    const float n = length(d);
    if (n < armOut)
        return {};
    const Vec3 w = d / n;
    return dot(w, up) < armDown ? Vec3{} : w;
}
// The steering way: the extended arms' ways averaged; zero when none is out (the heading is kept).
inline Vec3 steerWay(const Vec3 (&offset)[2], const bool (&tracked)[2], Vec3 up) {
    Vec3 sum{};
    for (int i = 0; i < 2; ++i)
        sum = sum + armWay(offset[i], tracked[i], up);
    return length(sum) > .3f ? normalized(sum) : Vec3{};
}

// ---- Take off and come down: both hands up, or A held ----
class Start {
  public:
    // live: this hero may fly now; flying: the flight has him; armsUp: armsUp() this frame; a: A held.
    // Returns the ask to take off (held keepSeconds, so the swing module sees it, or until flying).
    bool update(float dt, bool live, bool flying, bool armsUpNow, bool a) {
        dt = seconds(dt);
        aHeld_ = a ? aHeld_ + dt : 0.f;
        if (!a)
            spent_ = false;
        descend_ = false;
        if (!live) {
            arms_ = keep_ = 0;
            asking_ = false;
            spent_ = a;
            return false;
        }
        if (flying) {
            arms_ = keep_ = 0;
            asking_ = false;
            if (!spent_ && aHeld_ >= aHoldSeconds) {
                descend_ = true;
                spent_ = true;
                ++descends_;
            }
            return false;
        }
        arms_ = armsUpNow ? arms_ + dt : 0.f;
        const bool byA = !spent_ && aHeld_ >= aHoldSeconds;
        if (byA || arms_ >= armsUpSeconds) {
            if (!asking_)
                ++asks_;
            asking_ = true;
            keep_ = keepSeconds;
            if (byA)
                spent_ = true; // the same press never also comes down
        } else if (asking_) {
            keep_ -= dt;
            if (keep_ <= 0)
                asking_ = false;
        }
        return asking_;
    }
    bool asking() const { return asking_; }
    bool descend() const { return descend_; } // the frame A held in flight asks the descent
    bool aClaimed() const { return aHeld_ > 0; } // A is down (in flight: never the game's)
    uint32_t asks() const { return asks_; }
    uint32_t descends() const { return descends_; }
    void reset() {
        aHeld_ = arms_ = keep_ = 0;
        asking_ = descend_ = false;
        spent_ = true;
    }

  private:
    float aHeld_{}, arms_{}, keep_{};
    bool asking_{}, descend_{}, spent_{};
    uint32_t asks_{}, descends_{};
};

// ---- Y: a tap is LOOK UP (fly_feel::YTap), held past yHoldSeconds the orientation brake ----
class YHold {
  public:
    bool update(float dt, bool active, bool y) {
        dt = seconds(dt);
        held_ = active && y ? held_ + dt : 0.f;
        return held_ > yHoldSeconds;
    }
    bool braking() const { return held_ > yHoldSeconds; }
    void reset() { held_ = 0; }

  private:
    float held_{};
};

// ---- SUPERSONIC by the left stick: full forward, with hysteresis ----
class Sonic {
  public:
    bool update(bool allowed, float stickY) {
        const float y = axis(stickY);
        on_ = allowed && (on_ ? y > sonicOff : y > sonicOn);
        return on_;
    }
    bool on() const { return on_; }
    void reset() { on_ = false; }

  private:
    bool on_{};
};

// `a` turned toward `b` (both unit) by at most `most` radians; NaN-safe.
inline Vec3 toward(Vec3 a, Vec3 b, float most) {
    if (!finite(a) || length(a) < .5f)
        return unitOr(b, {0, 0, -1});
    if (!finite(b) || length(b) < .5f || !(most > 0))
        return normalized(a);
    a = normalized(a);
    b = normalized(b);
    const float c = std::clamp(dot(a, b), -1.f, 1.f);
    const float angle = std::acos(c);
    if (angle <= most)
        return b;
    Vec3 side = b - a * c;
    if (length(side) < 1e-4f) { // straight back: turn over the nearest side
        side = cross(a, std::fabs(a.y) < .9f ? Vec3{0, 1, 0} : Vec3{1, 0, 0});
        side = cross(side, a);
    }
    const Vec3 r = a * std::cos(most) + normalized(side) * std::sin(most);
    return finite(r) ? normalized(r) : a;
}

// ---- The heading: eased toward the arms' way (smooth, a little slower at speed) ----
class Heading {
  public:
    static constexpr float easeSeconds = .25f, mostDegrees = 150.f;
    Vec3 update(float dt, Vec3 want, bool hold, float speed) {
        dt = seconds(dt);
        if (!finite(way_) || length(way_) < .5f)
            way_ = unitOr(want, {0, 0, -1});
        if (hold || !finite(want) || length(want) < .5f)
            return way_;
        want = normalized(want);
        const float angle = std::acos(std::clamp(dot(way_, want), -1.f, 1.f));
        const float scale = std::clamp(40.f / std::max(1.f, std::isfinite(speed) ? speed : 1.f), .35f, 1.f);
        const float ease = angle * (1 - std::exp(-dt / easeSeconds));
        const float most = mostDegrees * (3.14159265f / 180.f) * scale * dt;
        way_ = toward(way_, want, std::min(ease, most));
        return way_;
    }
    void set(Vec3 way) { way_ = unitOr(way, {0, 0, -1}); }
    Vec3 way() const { return way_; }
    // Off a wall: the part of the heading into the wall (n, out of it) taken away.
    void slide(Vec3 n) {
        if (!finite(n) || length(n) < .5f)
            return;
        n = normalized(n);
        const float into = dot(way_, n);
        if (into < 0) {
            const Vec3 along = way_ - n * into;
            way_ = length(along) > .1f ? normalized(along + n * .05f) : n;
        }
    }

  private:
    Vec3 way_{};
};

// ---- The body roll about the flight's axis: a full turn and on, held where it is let go ----
class BodyRoll {
  public:
    static constexpr float levelDegrees = 180.f;
    static constexpr float snapDegrees = 8.f; // 0.0.76 roll fix: let go this near level, it IS level (exactly 0)
    // flying: the flight has him (off: level at once, 0.0.76 roll fix); stick: the right stick's x;
    // upright: the orientation brake (Y held) brings it back to 0.
    float update(float dt, bool flying, float stick, float degreesPerSecond, bool upright) {
        constexpr float toRad = 3.14159265f / 180.f, pi = 3.14159265f;
        dt = seconds(dt);
        const float s = axis(stick);
        const float x = std::fabs(s) > stickDead ? (s - std::copysign(stickDead, s)) / (1 - stickDead) : 0.f;
        const float rate = std::clamp(std::isfinite(degreesPerSecond) ? degreesPerSecond : 90.f, 30.f, 360.f) * toRad;
        rolling_ = flying && x != 0 && !upright;
        if (!flying) { // 0.0.76 roll fix: landed (or the flight ended): level at once, nothing left over
            angle_ = 0;
            return angle_;
        }
        if (upright) {
            const float step = levelDegrees * toRad * dt;
            angle_ = std::fabs(angle_) <= step ? 0.f : angle_ - std::copysign(step, angle_);
        } else if (x != 0) {
            angle_ += x * rate * dt;
        }
        if (angle_ > pi)
            angle_ -= 2 * pi;
        else if (angle_ < -pi)
            angle_ += 2 * pi;
        if (!std::isfinite(angle_))
            angle_ = 0;
        if (!rolling_ && std::fabs(angle_) < snapDegrees * toRad)
            angle_ = 0; // let go near level: exactly level (the view and the body clean, no leftover tilt)
        return angle_;
    }
    float angle() const { return angle_; }
    bool rolling() const { return rolling_; } // the stick rolls now (the heading holds meanwhile)
    void reset() { angle_ = 0, rolling_ = false; }

  private:
    float angle_{};
    bool rolling_{};
};

// ---- 0.0.76 ROLL FIX: the roll's frame (pure) ----
// The world axis the roll turns about, locked when it starts, and that turn as one quaternion. The head stays put and
// everything of the body turns about it (aboutHead). Angle 0 clears it all (axis, turn, near plane): a clean upright.
class RollFrame {
  public:
    static constexpr float fastAxis = 4.f;                           // m/s: faster, the flight's way is the axis
    static constexpr float clipOnDegrees = 75.f, clipOffDegrees = 60.f; // past this the near plane steps out
    static constexpr float clipMetres = .25f;
    // angle: BodyRoll's (radians); velocity: the flight's (world, m/s); look: the head's look (world).
    Quat update(float angle, Vec3 velocity, Vec3 look) {
        if (!std::isfinite(angle) || angle == 0) {
            reset();
            return turn_;
        }
        if (finite(velocity) && length(velocity) > fastAxis)
            axis_ = normalized(velocity);
        else if (!(length(axis_) > .5f)) { // slow (a hover): the level look when the roll started, then kept
            const Vec3 level{finite(look) ? look.x : 0.f, 0, finite(look) ? look.z : -1.f};
            axis_ = length(level) > .1f ? normalized(level) : Vec3{0, 0, -1};
        }
        constexpr float toDeg = 180.f / 3.14159265f;
        const float deg = std::fabs(angle) * toDeg;
        clip_ = clip_ ? deg > clipOffDegrees : deg > clipOnDegrees;
        turn_ = Quat::around(axis_, angle);
        return turn_;
    }
    Vec3 axis() const { return axis_; }
    Quat turn() const { return turn_; }
    bool active() const { return length(axis_) > .5f; }
    float nearClip() const { return clip_ ? clipMetres : 0.f; } // 0: NEAR CLIP's own
    void reset() { axis_ = {}, turn_ = Quat{}, clip_ = false; }
    // A point of the body turned with the view about the head (the head itself stays where it is).
    static Vec3 aboutHead(Vec3 head, Vec3 point, Quat turn) { return head + turn.rotate(point - head); }

  private:
    Vec3 axis_{};
    Quat turn_{};
    bool clip_{};
};

// The right stick (x rolls, y climbs): only the way it leans most counts, so a roll never climbs (run flight-roll-1:
// "STICK 0.6 m/s (climb +0.6)" with the left stick at rest while he rolled) and a climb never rolls.
inline float climbStick(float x, float y) {
    x = axis(x), y = axis(y);
    return std::fabs(y) > std::fabs(x) ? y : 0.f;
}
inline float rollStick(float x, float y) {
    x = axis(x), y = axis(y);
    return std::fabs(x) >= std::fabs(y) ? x : 0.f;
}

// ---- The flight (the swing module flies it through its movement command; pure) ----
struct In {
    float seconds{};
    uint32_t speed{};      // FLY MODE's speed, 0 none (extras385::fly SLOW 1, NORMAL 2, FAST 3)
    float top{};           // that speed's m/s (extras385::fly::speeds)
    bool suspend{};        // a game move, a grab, a strike or no collision: hands off now, flight kept
    bool grounded{}, airborne{};
    bool enter{};          // Start: take off
    bool drop{};           // A held in flight, or R3: come down
    bool brake{};          // B held: the air brake
    bool holdWay{};        // the roll stick or the orientation brake: the heading holds
    float throttle{};      // left stick forward / back
    float boost{};         // SUPERSONIC: times FAST (<= 1: none)
    float sonicTop{120};
    Vec3 look{};           // the head's look, world
    Vec3 want{};           // steerWay, world (zero: none)
    Vec3 velocity{};       // the body's now
    Vec3 wall{};           // a wall just ahead (its normal out of it; zero: none)
    // HOVER FLIGHT (above): on, no cruise - only the sticks and a pointed arm move him; nothing held, he hangs still.
    bool hover{};
    Vec3 move{};           // the left stick, world, level, at most 1 long: forward / back the head's way, strafe across
    float rise{};          // the climb stick, -1..1 (up +)
};
// The stick's share past its dead zone, 0..1 on each side (NaN: 0).
inline float pastDead(float v) {
    v = axis(v);
    return std::fabs(v) > stickDead ? std::copysign((std::fabs(v) - stickDead) / (1 - stickDead), v) : 0.f;
}
inline Vec3 pastDead(Vec3 v) {
    if (!finite(v))
        return {};
    const float n = length(v);
    if (!(n > stickDead))
        return {};
    return v * (std::min(1.f, (n - stickDead) / (1 - stickDead)) / n);
}
struct Out {
    bool drive{}, flying{}, moving{}, supersonic{}, coasting{}, resumed{}, tookOff{}, landed{};
    Vec3 velocity{};
    float vignette{}, top{};
    const char* how = ""; // extras385::fly::hows
};
class Flight {
  public:
    static constexpr float accelPerSpeed = 1.6f, brakeFactor = 2.5f, coastDecel = 10.f;
    static constexpr float liftSpeed = 6.f, liftSeconds = .6f, landAfter = .5f, suspendMost = 10.f;
    static float dropSpeed(float top) { return std::max(4.f, top * .4f); }
    static constexpr float climbShare = .6f, stillBelow = .05f;
    static float share(float throttle) {
        const float t = axis(throttle);
        if (std::fabs(t) < stickDead)
            return 1.f;
        return t > 0 ? 1.f + .5f * std::min(1.f, (t - stickDead) / (sonicOn - stickDead))
                     : std::max(0.f, 1.f + (t + stickDead) / (1 - stickDead));
    }
    Out update(const In& in) {
        Out o;
        const float dt = seconds(in.seconds);
        const float top = std::isfinite(in.top) && in.top > 0 ? in.top : 16.f;
        if (!in.speed) {
            if (flying_)
                o.landed = true, ++landings_;
            land();
            return o;
        }
        if (!flying_) {
            if (!in.enter || in.suspend)
                return o;
            flying_ = true;
            ++takeoffs_;
            o.tookOff = true;
            descending_ = false;
            air_ = suspended_ = 0;
            v_ = finite(in.velocity) ? limited(in.velocity, top) : Vec3{};
            lift_ = in.grounded || !in.airborne ? liftSeconds : 0.f;
            const Vec3 flat = unitOr({in.look.x, 0, in.look.z}, {0, 0, -1});
            heading_.set(length(v_) > 3.f ? v_ : length(in.want) > .5f && !lift_ ? in.want : flat);
            speed_ = std::max(0.f, dot(v_, heading_.way()));
        }
        o.flying = true;
        if (in.suspend) {
            // Not a landing: the game has him for its move; the flight goes on from it when it is done.
            suspended_ += dt;
            if (suspended_ > suspendMost) {
                land();
                ++landings_;
                o.landed = true;
                o.flying = false;
            }
            return o;
        }
        if (suspended_ > 0) {
            suspended_ = 0;
            o.resumed = true;
            if (finite(in.velocity)) {
                v_ = limited(in.velocity, std::max(top, length(v_)));
                speed_ = std::max(0.f, dot(v_, heading_.way()));
            }
        }
        air_ += dt;
        if (in.drop)
            descending_ = true;
        if (descending_ && finite(in.want) && dot(in.want, Vec3{0, 1, 0}) > .7f)
            descending_ = false; // an arm up climbs again
        if (descending_ && in.hover && pastDead(in.rise) > .5f)
            descending_ = false; // HOVER FLIGHT: the climb stick up climbs again
        const bool boosting = std::isfinite(in.boost) && in.boost > 1.f;
        const bool sonic = boosting && !descending_ && !in.brake && lift_ <= 0;
        const float sonicTop = std::min(top * (boosting ? in.boost : 1.f), std::max(top, in.sonicTop));
        const float goal = sonic ? sonicTop : top * share(in.throttle);
        // The heading: the arms steer it (slower at speed); the roll stick and the orientation brake hold it.
        heading_.update(dt, in.want, in.holdWay, length(v_));
        if (finite(in.wall) && length(in.wall) > .5f) {
            heading_.slide(in.wall); // never into a wall: along it (no wall to stick to)
            const Vec3 n = normalized(in.wall);
            if (dot(v_, n) < 0)
                v_ = v_ - n * dot(v_, n);
        }
        const Vec3 way = heading_.way();
        Vec3 before = v_;
        float accel = top * accelPerSpeed;
        if (lift_ > 0) {
            lift_ = std::max(0.f, lift_ - dt);
            // HOVER FLIGHT: straight up (no drift ahead); it then comes to rest where it rose to.
            const Vec3 target = Vec3{0, liftSpeed, 0} + (in.hover ? Vec3{} : way * (goal * .25f));
            v_ = v_ + limited(target - v_, std::max(accel, liftSpeed * 4) * dt);
            speed_ = std::max(0.f, dot(v_, way));
            o.how = "RISE";
        } else if (descending_) {
            const Vec3 target{0, -dropSpeed(top), 0};
            v_ = v_ + limited(target - v_, accel * dt);
            speed_ = 0;
            o.how = "DROP";
        } else if (in.brake) {
            v_ = v_ + limited(Vec3{} - v_, accel * brakeFactor * dt);
            speed_ = std::max(0.f, dot(v_, way));
            o.how = "STOP";
        } else if (in.hover) {
            // HOVER FLIGHT: the sticks and a pointed arm say where; nothing held, he comes to rest and hangs still.
            const Vec3 up{0, 1, 0};
            const Vec3 move = pastDead(Vec3{in.move.x, 0, in.move.z});
            const float rise = pastDead(in.rise);
            const Vec3 arm = finite(in.want) && length(in.want) > .5f ? normalized(in.want) : Vec3{};
            Vec3 target{};
            if (sonic) {
                // SUPERSONIC: along the pointed arm, else the stick's / head's level way; never lower (a way down
                // is levelled), only the climb stick's up adds height.
                Vec3 d = length(arm) > .5f ? arm : length(move) > .1f ? move : Vec3{in.look.x, 0, in.look.z};
                d = unitOr(d, unitOr(Vec3{in.look.x, 0, in.look.z}, {0, 0, -1}));
                if (d.y < 0)
                    d = unitOr(Vec3{d.x, 0, d.z}, unitOr(Vec3{in.look.x, 0, in.look.z}, {0, 0, -1}));
                target = d * sonicTop + up * (std::max(0.f, rise) * top * climbShare);
            } else {
                target = limited(move * top + arm * top + up * (rise * top * climbShare), top);
            }
            if (finite(in.wall) && length(in.wall) > .5f) {
                const Vec3 n = normalized(in.wall);
                if (dot(target, n) < 0)
                    target = target - n * dot(target, n);
            }
            const bool still = length(target) < .01f;
            const float rate = still ? accel * brakeFactor : sonic ? std::max(accel, top * accelPerSpeed * 2) : accel;
            v_ = v_ + limited(target - v_, rate * dt);
            if (still && length(v_) < stillBelow)
                v_ = {}; // exactly still: no drift
            if (sonic && v_.y < 0)
                v_.y = 0; // SUPERSONIC never lowers you
            if (length(v_) > 1.f)
                heading_.set(v_);
            speed_ = length(v_);
            hoverStill_ = still;
            hoverDown_ = rise < 0;
            o.coasting = !sonic && speed_ > top + .01f;
            o.how = sonic ? "SUPERSONIC" : still ? (length(v_) > .01f ? "STOP" : "HOVER") : "STICK";
        } else {
            // Speed along the heading; SUPERSONIC and its coast change only the speed, never the way.
            if (speed_ > goal + .01f && !sonic) {
                speed_ = std::max(goal, speed_ - std::max(coastDecel, accel) * dt);
                o.coasting = speed_ > top + .01f;
            } else {
                const float up = sonic ? std::max(accel, top * accelPerSpeed * 2) : accel;
                speed_ = speed_ < goal ? std::min(goal, speed_ + up * dt) : std::max(goal, speed_ - accel * dt);
            }
            const Vec3 target = way * speed_;
            // What is left of the old velocity across the way fades out quickly (no snap at a takeoff or a resume).
            const Vec3 across = v_ - way * dot(v_, way);
            const Vec3 keep = limited(across, std::max(0.f, length(across) - accel * 2 * dt));
            v_ = sonic ? target : target + keep;
            o.how = sonic ? "SUPERSONIC" : speed_ > .5f ? "FORWARD" : "HOVER";
        }
        if (!finite(v_))
            v_ = {};
        o.supersonic = sonic;
        o.top = sonic ? sonicTop : std::max(top, goal);
        // Down onto the ground: a descent always lands; a cruise only when it heads down or has stopped. The dash
        // never lands you.
        // HOVER FLIGHT: a descent, the climb stick down, or resting on the ground lands; skimming along it does not.
        const bool downNow = in.hover && !descending_ && !in.brake && lift_ <= 0
                                 ? hoverDown_ || hoverStill_
                                 : descending_ || way.y < .2f || speed_ < 2.f;
        if (in.grounded && air_ >= landAfter && lift_ <= 0 && !sonic && (descending_ || downNow) && v_.y <= .5f) {
            land();
            ++landings_;
            o = {};
            o.landed = true;
            o.how = "LANDED";
            return o;
        }
        o.drive = true;
        o.moving = !in.brake && length(v_) > .5f;
        o.velocity = v_;
        const float a = dt > 0 ? length(v_ - before) / dt : 0;
        const float vTop = std::max(o.top, length(before));
        o.vignette = std::clamp(length(v_) / vTop * .3f + a / (vTop * accelPerSpeed) * .3f, 0.f, .6f);
        return o;
    }
    bool flying() const { return flying_; }
    bool descending() const { return descending_; }
    bool suspended() const { return suspended_ > 0; }
    Vec3 heading() const { return heading_.way(); }
    uint32_t takeoffs() const { return takeoffs_; }
    uint32_t landings() const { return landings_; }
    void land() {
        flying_ = descending_ = false;
        v_ = {};
        speed_ = lift_ = air_ = suspended_ = 0;
        hoverStill_ = hoverDown_ = false;
        heading_ = {};
    }

  private:
    bool flying_{}, descending_{}, hoverStill_{}, hoverDown_{};
    Vec3 v_{};
    float speed_{}, lift_{}, air_{}, suspended_{};
    Heading heading_;
    uint32_t takeoffs_{}, landings_{};
};

// ---- Walls are SPIDER-MAN's: after the game's crawl is left, the stick pushes away from the wall ----
// For `awaySeconds` after a kick off the crawl (hero072::WallRefusal), the walk stick is the way out of the wall
// (its normal across the camera's level ahead), so the player's own stick never walks him back onto it.
class PushOff {
  public:
    static constexpr float awaySeconds = .5f, strength = .8f;
    void kicked(Vec3 normal) {
        if (finite(normal) && length(Vec3{normal.x, 0, normal.z}) > .3f) {
            normal_ = normalized(Vec3{normal.x, 0, normal.z});
            left_ = awaySeconds;
        }
    }
    // The stick (right, forward) while pushing away; false when not.
    bool update(float dt, Vec3 ahead, float& right, float& forward) {
        left_ = std::max(0.f, left_ - seconds(dt));
        ahead = Vec3{ahead.x, 0, ahead.z};
        if (left_ <= 0 || length(ahead) < .3f)
            return false;
        ahead = normalized(ahead);
        forward = strength * dot(normal_, ahead);
        right = strength * dot(normal_, cross(ahead, Vec3{0, 1, 0}));
        return true;
    }
    bool pushing() const { return left_ > 0; }

  private:
    Vec3 normal_{};
    float left_{};
};
} // namespace spidy::fly076
