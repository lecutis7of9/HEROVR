#pragma once
// Version 0.0.72 PUNISHER VAN (EXPERIMENTAL, the user 10:41 - 10:42; off by default).
//  - SUMMON: one quick press of the left controller's X while the PUNISHER plays (his X is free: he has
//    no gadget wheel, and an X the game names stays the game's). The van is placed on the street near
//    him: a spot ahead, beside or behind him with ground under all four wheels' corners and nothing in
//    its box (place()); no safe spot: no van (a buzz, a note), never one inside a wall.
//  - IN AND OUT: grip the DRIVER DOOR HANDLE (outside to get in, inside to get out - at a crawl only).
//    This is the van's only way in or out (no wrist or menu exit).
//  - DRIVING: a real steering wheel - each grip latches that hand to the rim and the wheel turns with
//    the hands round its hub; RT drives forward, LT reverses, both triggers brake (the van's pedals).
//  - Everything fails safe: a NaN, a lost hand or the setting off parks it; nothing here can crash.
// Game hooks: the walk's world raycast for the ground / clearance probes is the bridge's (CONFIRMED,
// the webs use it); carrying the player's body with the van is the bridge's velocity command
// (CONFIRMED for webs and flight) but NOT WIRED in this build: the van's motion is computed and
// tested (PREVIEW) and moves only the drawn van; the van stays MODEL MISSING until the user picks one.
#include "spidy/math.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace spidy::van072 {

inline constexpr const char* settingHelp = "EXPERIMENTAL: Punisher taps X to summon the van, no restart.";

struct Tune {
    float length = 5.4f, width = 2.1f, height = 2.5f; // the van's box (metres)
    float wheelBase = 3.4f;
    float topSpeed = 16.f, reverseTop = 5.f, accel = 5.5f, reverseAccel = 3.f, brake = 11.f, coast = 1.2f;
    float steerMost = .6f;         // the front wheels' most angle (rad)
    float wheelLock = 2.4f * 3.14159265f; // the steering wheel's most turn (rad) either way: 1.2 turns
    float handleReach = .2f;       // a grip this near the door handle opens it
    float rimNear = .09f;          // a grip this near the rim (and the wheel's plane) latches the hand
    float wheelRadius = .19f;
    float exitSpeed = 1.5f;        // out only at a crawl
    float pedal = .15f;            // a trigger past this is pressed
    float groundStep = .6f;        // the ground under the van within this of the player's feet
};

// The van's frame: x right, y up, -z forward, origin on the ground under the axles' middle.
// The left-hand-drive cab (a US van): the driver sits left.
inline constexpr Vec3 doorHandleOut{-1.08f, 1.12f, -.95f}, doorHandleIn{-.86f, 1.10f, -.92f};
inline constexpr Vec3 seatEye{-.45f, 1.62f, -.55f}, wheelHub{-.45f, 1.28f, -1.02f};
// The wheel faces the driver tilted back 25 degrees: its axis (towards the driver).
inline Vec3 wheelAxis() { return normalized(Vec3{0, .42f, .91f}); }

inline bool fine(Vec3 v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }
inline bool fine(float f) { return std::isfinite(f); }

struct Frame { // the van in the world
    Vec3 at{};
    float yaw{}; // about +y; 0: forward is -z
    Vec3 forward() const { return {-std::sin(yaw), 0, -std::cos(yaw)}; }
    Vec3 right() const { return {std::cos(yaw), 0, -std::sin(yaw)}; }
    Vec3 world(Vec3 local) const { return at + right() * local.x + Vec3{0, local.y, 0} + forward() * (-local.z); }
    Vec3 local(Vec3 w) const {
        const Vec3 d = w - at;
        return {dot(d, right()), d.y, -dot(d, forward())};
    }
};

// ---- SUMMON: a safe spot on the street ----
// groundAt(x,z, aroundY): the ground's height there (NaN: none found); clear(centre, halfExtents, yaw):
// nothing solid in that box. Both are the bridge's raycasts in game (CONFIRMED path).
struct Spot {
    Frame frame{};
    bool ok{};
    uint32_t tried{};
};
template <class Ground, class Clear>
Spot place(Vec3 feet, Vec3 look, Ground&& groundAt, Clear&& clear, const Tune& t = {}) {
    Spot s;
    if (!fine(feet) || !fine(look))
        return s;
    Vec3 f{look.x, 0, look.z};
    if (length(f) < 1e-3f)
        f = {0, 0, -1};
    f = normalized(f);
    const Vec3 r{-f.z, 0, f.x};
    const float yaw = std::atan2(-f.x, -f.z);
    // Ahead-right first (the kerb side), then ahead, ahead-left, beside, behind; never under him.
    const Vec3 offsets[] = {f * 6.f + r * 2.6f, f * 7.f, f * 6.f - r * 2.6f, r * 3.6f, -r * 3.6f, -f * 6.f, f * 10.f + r * 2.6f};
    for (const Vec3& o : offsets) {
        ++s.tried;
        const Vec3 c = feet + o;
        float lowest = INFINITY, highest = -INFINITY;
        bool grounded = true;
        for (int k = 0; k < 4 && grounded; ++k) {
            const Vec3 corner = c + f * ((k & 1 ? .5f : -.5f) * t.length * .8f) + r * ((k & 2 ? .5f : -.5f) * t.width * .8f);
            const float g = groundAt(corner.x, corner.z, feet.y);
            if (!fine(g) || std::abs(g - feet.y) > t.groundStep) {
                grounded = false;
                break;
            }
            lowest = std::min(lowest, g);
            highest = std::max(highest, g);
        }
        if (!grounded || highest - lowest > .35f) // not a street: a step, a kerb across it, a drop
            continue;
        const Vec3 centre{c.x, highest + t.height * .5f + .05f, c.z};
        if (!clear(centre, Vec3{t.width * .5f + .15f, t.height * .5f, t.length * .5f + .15f}, yaw))
            continue;
        s.frame = {{c.x, highest, c.z}, yaw};
        s.ok = true;
        return s;
    }
    return s;
}

// ---- THE WHEEL: a hand latched to the rim turns it round the hub ----
struct Wheel {
    bool latched[2]{};
    float handAngle[2]{}; // each latched hand's angle round the hub (rad, in the wheel's plane)
    float angle{};        // the wheel's turn (rad, + clockwise as the driver sees it: a right turn)
    bool gripWas[2]{};
    // Angle of a point round the hub in the wheel's plane (as the driver sees it); false: off the rim.
    static bool onRim(Vec3 localHand, float& a, float& off, const Tune& t) {
        const Vec3 axis = wheelAxis();
        const Vec3 d = localHand - wheelHub;
        const float out = dot(d, axis);
        const Vec3 inPlane = d - axis * out;
        const Vec3 up = normalized(Vec3{0, 1, 0} - axis * axis.y), rightV{1, 0, 0};
        a = std::atan2(dot(inPlane, rightV), dot(inPlane, up)); // 0 at 12 o'clock, + clockwise
        off = std::max(std::abs(length(inPlane) - t.wheelRadius), std::abs(out));
        return fine(a) && off <= t.rimNear;
    }
    void release() {
        latched[0] = latched[1] = false;
    }
    // hands in the van's frame; grip 0-1. Returns the wheel's angle.
    float step(const Vec3 localHand[2], const float grip[2], const bool tracked[2], const Tune& t = {}) {
        float turn = 0;
        int turning = 0;
        for (int i = 0; i < 2; ++i) {
            const bool g = tracked[i] && fine(grip[i]) && grip[i] >= .6f;
            const bool pressed = g && !gripWas[i];
            gripWas[i] = g;
            float a = 0, off = 0;
            const bool rim = tracked[i] && fine(localHand[i]) && onRim(localHand[i], a, off, t);
            if (!g) {
                latched[i] = false;
                continue;
            }
            if (pressed && rim) {
                latched[i] = true;
                handAngle[i] = a;
                continue;
            }
            if (latched[i] && fine(a) && fine(localHand[i])) {
                float da = a - handAngle[i];
                while (da > 3.14159265f)
                    da -= 2 * 3.14159265f;
                while (da < -3.14159265f)
                    da += 2 * 3.14159265f;
                if (std::abs(da) < 1.2f) { // a jump this big is a lost hand, not a turn
                    turn += da;
                    ++turning;
                }
                handAngle[i] = a;
            }
        }
        if (turning)
            angle = std::clamp(angle + turn / static_cast<float>(turning), -t.wheelLock, t.wheelLock);
        if (!fine(angle))
            angle = 0;
        return angle;
    }
};

// ---- THE PEDALS: RT forward, LT reverse, both brake ----
enum class Pedal : uint32_t { none = 0, drive, reverse, brake };
inline Pedal pedal(float lt, float rt, const Tune& t = {}) {
    const bool l = fine(lt) && lt > t.pedal, r = fine(rt) && rt > t.pedal;
    return l && r ? Pedal::brake : r ? Pedal::drive : l ? Pedal::reverse : Pedal::none;
}

// ---- THE VAN ----
enum State : uint32_t { none = 0, parked, driving };
inline constexpr const char* stateNames[] = {"NONE", "PARKED", "DRIVING"};
struct In {
    float dt{};
    bool enabled{}, punisher{}, play{};
    bool summonPress{}; // X's press edge (game_xr: only when the game does not name X)
    Vec3 feet{}, look{0, 0, -1};
    Vec3 hands[2]{};      // world
    bool tracked[2]{};
    float grip[2]{}, trigger[2]{}; // trigger[0] LT, trigger[1] RT
};
struct Out {
    bool summoned{}, noSpot{}, entered{}, exited{}, bumped{};
    bool seat{};     // the player belongs at seatEye (world: seatAt)
    Vec3 seatAt{}, velocity{};
    float wheelAngle{}, steer{};
    Pedal pedal{Pedal::none};
    float haptic[2]{};
};
struct Van {
    State state{none};
    Frame frame{};
    float speed{}; // + forward
    Wheel wheel;
    bool gripWas[2]{};
    uint64_t summons{}, entries{}, exits{};
    void park() {
        if (state == driving)
            state = parked;
        speed = 0;
        wheel.release();
    }
    // ahead(distance): nothing solid ahead of the bumper for that far (true when unknown: the game's
    // own collision still holds the player's body).
    template <class Ground, class Clear, class Ahead>
    Out step(const In& in, Ground&& groundAt, Clear&& clear, Ahead&& ahead, const Tune& t = {}) {
        Out o;
        const float dt = fine(in.dt) ? std::clamp(in.dt, 0.f, .1f) : 0.f;
        if (!in.enabled || !in.punisher) { // off, or another hero: the van goes
            state = none;
            speed = 0;
            wheel.release();
            return o;
        }
        if (!in.play || !fine(frame.at) || !fine(frame.yaw) || !fine(speed)) {
            park();
            if (!fine(frame.at) || !fine(frame.yaw))
                state = none;
            speed = 0;
            return o;
        }
        if (in.summonPress && state != driving) {
            const Spot s = place(in.feet, in.look, groundAt, clear, t);
            if (s.ok) {
                frame = s.frame;
                state = parked;
                speed = 0;
                ++summons;
                o.summoned = true;
                o.haptic[0] = .4f;
            } else {
                o.noSpot = true;
                o.haptic[0] = .15f;
            }
        }
        // The door handle (a grip's press within reach).
        for (int i = 0; i < 2; ++i) {
            const bool g = in.tracked[i] && fine(in.grip[i]) && in.grip[i] >= .6f;
            const bool pressed = g && !gripWas[i];
            gripWas[i] = g;
            if (!pressed || state == none || !fine(in.hands[i]))
                continue;
            if (state == parked && length(in.hands[i] - frame.world(doorHandleOut)) <= t.handleReach) {
                state = driving;
                wheel = {};
                ++entries;
                o.entered = true;
                o.haptic[i] = .5f;
            } else if (state == driving && length(in.hands[i] - frame.world(doorHandleIn)) <= t.handleReach &&
                       std::abs(speed) <= t.exitSpeed) {
                park();
                ++exits;
                o.exited = true;
                o.haptic[i] = .5f;
            }
        }
        if (state != driving)
            return o;
        Vec3 local[2];
        for (int i = 0; i < 2; ++i)
            local[i] = frame.local(in.hands[i]);
        const float a = wheel.step(local, in.grip, in.tracked, t);
        o.wheelAngle = a;
        o.steer = a / t.wheelLock * t.steerMost;
        o.pedal = pedal(in.trigger[0], in.trigger[1], t);
        switch (o.pedal) {
        case Pedal::drive:
            speed = speed < 0 ? std::min(0.f, speed + t.brake * dt) : std::min(t.topSpeed, speed + t.accel * dt);
            break;
        case Pedal::reverse:
            speed = speed > 0 ? std::max(0.f, speed - t.brake * dt) : std::max(-t.reverseTop, speed - t.reverseAccel * dt);
            break;
        case Pedal::brake:
            speed = speed > 0 ? std::max(0.f, speed - t.brake * dt) : std::min(0.f, speed + t.brake * dt);
            break;
        default:
            speed = speed > 0 ? std::max(0.f, speed - t.coast * dt) : std::min(0.f, speed + t.coast * dt);
        }
        const float look = std::abs(speed) * dt + t.length * .5f + .3f;
        if (std::abs(speed) > 1e-3f && !ahead(frame.at + (speed > 0 ? frame.forward() : -frame.forward()) * (t.length * .5f), speed > 0 ? frame.forward() : -frame.forward(), look - t.length * .5f)) {
            o.bumped = speed > 3.f;
            o.haptic[0] = o.haptic[1] = o.bumped ? 1.f : .3f;
            speed = 0; // into a wall: stop, never through it
        }
        frame.yaw -= speed * std::tan(o.steer) / t.wheelBase * dt; // a right turn: yaw goes clockwise
        frame.at = frame.at + frame.forward() * (speed * dt);
        const float g = groundAt(frame.at.x, frame.at.z, frame.at.y);
        if (fine(g) && std::abs(g - frame.at.y) < .5f)
            frame.at.y = g; // follow the street (a drop or a wall's top: keep the height)
        o.velocity = frame.forward() * speed;
        o.seat = true;
        o.seatAt = frame.world(seatEye);
        if (!fine(o.seatAt) || !fine(frame.yaw)) {
            state = none;
            o = {};
        }
        return o;
    }
};

} // namespace spidy::van072
