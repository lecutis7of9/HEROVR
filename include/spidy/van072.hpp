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
//    Version 0.0.73 test 3 (WHEEL-HAND PEDALS, on): a trigger counts only while ITS hand grips the wheel
//    (wheelPedal() below): one hand on the wheel - its trigger drives, its trigger with ITS OWN stick
//    pulled back reverses at the trigger's strength; the free hand's trigger never moves the van; no
//    hand on the wheel - the triggers do nothing and the van coasts. Both hands on: the rule above.
//  - Everything fails safe: a NaN, a lost hand or the setting off parks it; nothing here can crash.
// Game hooks: the walk's world raycast for the ground / clearance probes is the bridge's (CONFIRMED,
// the webs use it); carrying the player's body with the van is the bridge's velocity command
// (CONFIRMED for webs and flight; CANDIDATE for a body on the street) - Version 0.0.73: WIRED (Driver,
// below); the 0.0.72 preview (stepPreview) stays as its fall-back.
// Version 0.0.72 THE PICK (owner pt850u "For the van do 8"): the GMC Vandura 1976 by snafuj (CC BY 4.0),
// painted black; its source is an empty shell, so the cab (seats, dash, gauges, a door card with its handle)
// and a separate steering wheel are BUILT (tools/models072/convert_thor_van_072.py). The numbers below are
// MEASURED on the shipped model (CONFIRMED on the model, CANDIDATE in game). ENGINE PREVIEW (stepPreview):
// the wheel turns with the hands and the pedals drive the van's speed, but the van and the player stay put
// until the body carry is wired - nothing in the world is moved, nothing can be driven into.
// SUMMON in game (Probe): the ground and clearance are the bridge's world rays, one a frame in the slot the
// roof ray lends (as BOLT WALLS' beam) - about half a second, then the van appears or a buzz says no spot.
#include "spidy/math.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace spidy::van072 {

inline constexpr const char* settingHelp = "EXPERIMENTAL, OFF by default: Punisher taps X to drive the van, no restart.";

// The van's frame: x right, y up, -z forward, origin on the ground under the axles' middle.
// The left-hand-drive cab (a US van): the driver sits left.
// Version 0.0.72: MEASURED on the shipped model (the door's own handle; the built cab's door card, seat, wheel).
inline constexpr Vec3 doorHandleOut{-.98f, 1.18f, -.66f}, doorHandleIn{-.855f, 1.08f, -.83f};
inline constexpr Vec3 seatEye{-.45f, 1.60f, -.55f}, wheelHub{-.45f, 1.18f, -1.08f};
// The wheel faces the driver tilted back 25 degrees: its axis (towards the driver).
inline Vec3 wheelAxis() { return normalized(Vec3{0, .42f, .91f}); }
inline constexpr Vec3 exitStep{-1.75f, 0.f, -.55f}; // on the street, 0.75 m out from the driver's side

struct Tune {
    // 0.0.74 test 1 BATMOBILE: the vehicle's own layout in its frame (the van's numbers by default; the
    // Batmobiles' come from their converter, tools/batmobile074). The van's constants above stay its own.
    Vec3 handleOut = doorHandleOut, handleIn = doorHandleIn, eye = seatEye, hub = wheelHub;
    Vec3 axis{0, .42f, .91f}; // the wheel's axis toward the driver (normalized on use)
    Vec3 exit = exitStep;
    // Version 0.0.72: MEASURED on the shipped Vandura (4.85 x 2.0 x 1.95 m, 2.96 m between the axles).
    float length = 4.85f, width = 2.0f, height = 1.95f; // the van's box (metres)
    float wheelBase = 2.96f;
    float topSpeed = 16.f, reverseTop = 5.f, accel = 5.5f, reverseAccel = 3.f, brake = 11.f, coast = 1.2f;
    float steerMost = .6f;         // the front wheels' most angle (rad)
    float wheelLock = 2.4f * 3.14159265f; // the steering wheel's most turn (rad) either way: 1.2 turns
    float handleReach = .2f;       // a grip this near the door handle opens it
    float rimNear = .09f;          // a grip this near the rim (and the wheel's plane) latches the hand
    float wheelRadius = .19f;
    float exitSpeed = 1.5f;        // out only at a crawl
    float pedal = .15f;            // a trigger past this is pressed
    // Version 0.0.73 test 3: WHEEL-HAND PEDALS (on; false: the 0.0.72 pedals - either trigger, wheel or not)
    bool wheelHandPedals = true;
    float stickBack = .5f;         // a stick pulled back past this (y < -stickBack) with the trigger: reverse
    // Version 0.0.73 test 3: BOARD THE PLAYER (on; false: 0.0.72 - the drawn van swings round him on entry).
    // On: getting in never moves the van; HE is turned to its forward and carried into the seat (Driver).
    bool boardPlayer = true;
    float groundStep = .6f;        // the ground under the van within this of the player's feet
};


inline bool fine(Vec3 v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }
inline bool fine(float f) { return std::isfinite(f); }
// 0.0.74 test 1: the tune's wheel axis, unit (the van's when it is not a usable direction).
inline Vec3 axisOf(const Tune& t) {
    const float n = length(t.axis);
    return fine(t.axis) && std::isfinite(n) && n > .5f ? t.axis * (1 / n) : wheelAxis();
}

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

// Version 0.0.72: the candidate spots place() tries, in order (shared with Probe).
inline constexpr uint32_t spots = 7;
inline Vec3 spotOffset(uint32_t k, Vec3 f, Vec3 r) {
    switch (k) {
    case 0: return f * 6.f + r * 2.6f;
    case 1: return f * 7.f;
    case 2: return f * 6.f - r * 2.6f;
    case 3: return r * 3.6f;
    case 4: return -r * 3.6f;
    case 5: return -f * 6.f;
    default: return f * 10.f + r * 2.6f;
    }
}

// ---- Version 0.0.72 SUMMON IN GAME: place() one world ray at a time ----
// Per spot: four corner rays down (from 1.5 m over his feet, 3 m: the ground), then three across the van's
// box (its length at 1.0 m, both diagonals at 1.6 m) and one from his chest to the spot's middle - any hit
// is something in the way. The first spot that passes is the van's; none: no van.
struct Ray {
    Vec3 from{}, dir{};
    float distance{};
};
struct Probe {
    static constexpr uint32_t raysPerSpot = 8;
    static constexpr float downFrom = 1.5f, downLength = 3.f;
    bool active{}, done{};
    Spot result{};
    Vec3 feet{}, f{0, 0, -1}, r{1, 0, 0};
    float yaw{};
    uint32_t spot{}, ray{};
    float ground[4]{};
    Tune t{};
    void start(Vec3 atFeet, Vec3 look, const Tune& tune = {}) {
        *this = {};
        t = tune;
        if (!fine(atFeet) || !fine(look)) {
            done = true;
            return;
        }
        Vec3 h{look.x, 0, look.z};
        if (length(h) < 1e-3f)
            h = {0, 0, -1};
        f = normalized(h);
        r = {-f.z, 0, f.x};
        yaw = std::atan2(-f.x, -f.z);
        feet = atFeet;
        active = true;
    }
    Vec3 centre() const { return feet + spotOffset(spot, f, r); }
    float highest() const { return std::max(std::max(ground[0], ground[1]), std::max(ground[2], ground[3])); }
    Vec3 corner(int k, float scale) const {
        return centre() + f * ((k & 1 ? .5f : -.5f) * t.length * scale) + r * ((k & 2 ? .5f : -.5f) * t.width * scale);
    }
    // The ray to cast now (false: nothing to ask).
    bool next(Ray& out) const {
        if (!active || done || spot >= spots)
            return false;
        Vec3 a{}, b{};
        if (ray < 4) {
            const Vec3 c = corner(static_cast<int>(ray), .8f);
            out = {{c.x, feet.y + downFrom, c.z}, {0, -1, 0}, downLength};
            return true;
        }
        const float g = highest();
        if (ray == 4) {
            a = centre() - f * (t.length * .5f + .15f);
            b = centre() + f * (t.length * .5f + .15f);
            a.y = b.y = g + 1.f;
        } else if (ray == 5 || ray == 6) {
            const int k = ray == 5 ? 0 : 1;
            a = corner(k, 1.f);
            b = corner(3 - k, 1.f);
            a.y = b.y = g + 1.6f;
        } else {
            a = feet + Vec3{0, 1.2f, 0};
            b = centre();
            b.y = g + 1.f;
        }
        const float d = length(b - a);
        if (!fine(d) || d < .05f)
            return false;
        out = {a, (b - a) / d, d};
        return true;
    }
    // The answer to next()'s ray: hit (and where along it, 0-1).
    void answer(bool hit, float fraction) {
        if (!active || done)
            return;
        bool fail = false;
        if (ray < 4) {
            const float g = feet.y + downFrom - (fine(fraction) ? std::clamp(fraction, 0.f, 1.f) : 1.f) * downLength;
            ground[ray] = g;
            fail = !hit || !fine(fraction) || std::abs(g - feet.y) > t.groundStep;
            if (!fail && ray == 3) {
                const float lo = std::min(std::min(ground[0], ground[1]), std::min(ground[2], ground[3]));
                fail = highest() - lo > .35f; // not a street: a step, a kerb across it, a drop
            }
        } else
            fail = hit; // something in the box, or between him and it
        ++result.tried;
        if (fail) {
            ++spot;
            ray = 0;
            if (spot >= spots) {
                done = true;
                active = false;
            }
            return;
        }
        if (++ray < raysPerSpot)
            return;
        const Vec3 c = centre();
        result.frame = {{c.x, highest(), c.z}, yaw};
        result.ok = fine(result.frame.at) && fine(yaw);
        done = true;
        active = false;
    }
};

// ---- THE WHEEL: a hand latched to the rim turns it round the hub ----
struct Wheel {
    bool latched[2]{};
    float handAngle[2]{}; // each latched hand's angle round the hub (rad, in the wheel's plane)
    float angle{};        // the wheel's turn (rad, + clockwise as the driver sees it: a right turn)
    bool gripWas[2]{};
    // Angle of a point round the hub in the wheel's plane (as the driver sees it); false: off the rim.
    static bool onRim(Vec3 localHand, float& a, float& off, const Tune& t) {
        const Vec3 axis = axisOf(t);
        const Vec3 d = localHand - t.hub;
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

// Version 0.0.73 test 3: WHEEL-HAND PEDALS. Only a hand latched to the wheel works a pedal.
//  - no hand on the wheel: none (the van coasts; the triggers are the free hands').
//  - one hand on (either - left or right is specced the same): its trigger drives forward; its trigger with
//    ITS OWN controller's stick pulled back (y < -stickBack) reverses, at the trigger's strength. The other
//    hand's trigger and stick are ignored (a gun or the screen there never moves the van).
//  - both hands on: the 0.0.72 pedals (RT forward, LT reverse, both brake) - and either trigger with its own
//    stick back reverses too.
// Reverse while rolling forward brakes, as before. strength 0..1: the reverse's share of reverseTop and
// reverseAccel (1 for the pedals' LT reverse, as in 0.0.72; forward stays full, as before).
struct Drive {
    Pedal pedal{Pedal::none};
    float strength{};
};
inline Drive wheelPedal(const bool onWheel[2], const float trigger[2], const float stickY[2], const Tune& t = {}) {
    bool press[2], back[2];
    for (int i = 0; i < 2; ++i) {
        press[i] = onWheel[i] && fine(trigger[i]) && trigger[i] > t.pedal;
        back[i] = press[i] && fine(stickY[i]) && stickY[i] < -t.stickBack;
    }
    auto pull = [&](int i) { return std::clamp((trigger[i] - t.pedal) / (1 - t.pedal), .05f, 1.f); };
    if (press[0] && press[1])
        return {Pedal::brake, 1};
    for (int i = 0; i < 2; ++i)
        if (press[i]) {
            if (back[i])
                return {Pedal::reverse, pull(i)};
            if (onWheel[0] && onWheel[1] && i == 0)
                return {Pedal::reverse, 1}; // both hands on: LT reverses (0.0.72)
            return {Pedal::drive, 1};
        }
    return {};
}

// Version 0.0.73 test 3 FREE-HAND GUNS: may hand i's trigger reach the hero's own weapons (the PUNISHER's
// drawn pistol) this frame? Off the seat: always. In the seat: never with the 0.0.72 pedals (either trigger
// is a pedal there), never the wheel's hand (latched, or its grip held on the rim - about to latch: its
// trigger is the throttle), never a hand pointed at the dash screen (that trigger pages it). Everything
// else - the free hand - fires. Fails safe: anything unsure is "no fire", never a throttle.
inline bool freeTrigger(bool driving, bool wheelHandPedals, bool latched, bool gripOnRim, bool onScreen) {
    if (!driving)
        return true;
    return wheelHandPedals && !latched && !gripOnRim && !onScreen;
}

// ---- THE VAN ----
enum State : uint32_t { none = 0, parked, driving };
// 0.0.73 test 3 SEATED STICK: the rig's stick lock - asked (boarding, alighting, the carry) or in the seat.
inline bool seatStickLock(State s, bool asked) { return asked || s == driving; }
inline constexpr const char* stateNames[] = {"NONE", "PARKED", "DRIVING"};
struct In {
    float dt{};
    bool enabled{}, punisher{}, play{};
    bool summonPress{}; // X's press edge (game_xr: only when the game does not name X)
    bool summonSpot{};  // Version 0.0.72: a Probe's finished answer in `spot` (game_xr), instead of place()
    Spot spot{};
    Vec3 feet{}, look{0, 0, -1};
    Vec3 head{};          // Version 0.0.72: his eyes (world; stepPreview)
    Vec3 hands[2]{};      // world
    bool tracked[2]{};
    float grip[2]{}, trigger[2]{}; // trigger[0] LT, trigger[1] RT
    float stickY[2]{};    // Version 0.0.73 test 3: each controller's own thumbstick y (+ forward, - pulled back)
};
struct Out {
    bool summoned{}, noSpot{}, entered{}, exited{}, bumped{};
    bool seat{};     // the player belongs at seatEye (world: seatAt)
    Vec3 seatAt{}, velocity{};
    float wheelAngle{}, steer{};
    Pedal pedal{Pedal::none};
    float pedalStrength{}; // Version 0.0.73 test 3: the reverse's strength (wheelPedal)
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
        if ((in.summonPress || in.summonSpot) && state != driving) {
            const Spot s = in.summonSpot ? in.spot : place(in.feet, in.look, groundAt, clear, t);
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
            if (state == parked && length(in.hands[i] - frame.world(t.handleOut)) <= t.handleReach) {
                state = driving;
                wheel = {};
                ++entries;
                o.entered = true;
                o.haptic[i] = .5f;
            } else if (state == driving && length(in.hands[i] - frame.world(t.handleIn)) <= t.handleReach &&
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
        Drive dr{pedal(in.trigger[0], in.trigger[1], t), 1};
        if (t.wheelHandPedals)
            dr = wheelPedal(wheel.latched, in.trigger, in.stickY, t);
        o.pedal = dr.pedal;
        o.pedalStrength = dr.strength;
        switch (o.pedal) {
        case Pedal::drive:
            speed = speed < 0 ? std::min(0.f, speed + t.brake * dt) : std::min(t.topSpeed, speed + t.accel * dt);
            break;
        case Pedal::reverse:
            if (speed > 0)
                speed = std::max(0.f, speed - t.brake * dt);
            else if (speed > -t.reverseTop * dr.strength)
                speed = std::max(-t.reverseTop * dr.strength, speed - t.reverseAccel * dr.strength * dt);
            else
                speed = std::min(-t.reverseTop * dr.strength, speed + t.coast * dt); // a lighter pull: ease off
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
        o.seatAt = frame.world(t.eye);
        if (!fine(o.seatAt) || !fine(frame.yaw)) {
            state = none;
            o = {};
        }
        return o;
    }
    // Version 0.0.72 ENGINE PREVIEW: step() whole (door, wheel, pedals, speed) but nothing in the world moves
    // (the body carry is NOT WIRED): `speed` is what it would do. Getting in brings the DRAWN van round him (its
    // seat at his eyes, facing his look) so the cab and the wheel are in reach; getting out puts it back where
    // it was summoned. Only the drawn van moves - never the player, never anything solid.
    Frame home{};
    template <class Ground, class Clear, class Ahead>
    Out stepPreview(const In& in, Ground&& groundAt, Clear&& clear, Ahead&& ahead, const Tune& t = {}) {
        const Frame kept = frame;
        Out o = step(in, groundAt, clear, ahead, t);
        if (state != none && !o.summoned)
            frame = kept;
        if (o.summoned)
            home = frame;
        if (o.entered)
            home = kept;
        if (o.entered && !t.boardPlayer && fine(in.head) && fine(in.look)) { // 0.0.72: the van comes round him
            Vec3 h{in.look.x, 0, in.look.z};
            if (length(h) < 1e-3f)
                h = kept.forward();
            h = normalized(h);
            Frame k{{}, std::atan2(-h.x, -h.z)};
            const Vec3 off = k.world(t.eye); // the seat from an origin at 0
            k.at = in.head - off;
            if (fine(k.at) && fine(k.yaw))
                frame = k;
        }
        if (o.exited)
            frame = home;
        if (!fine(frame.at) || !fine(frame.yaw)) {
            state = none;
            frame = {};
        }
        o.velocity = {};
        o.seat = false;
        return o;
    }
};

// ---- Version 0.0.73 DRIVING: THE BODY CARRY (owner t995u "do the van first") ----
// The van rides in the TRACKING space while he drives: the cab is fixed to his room, so it can never judder
// against his head or hands. The van's motion moves the room instead: its seat's velocity is the player's
// body's (the bridge's velocity command - game_swing Command v7 `carry`, CONFIRMED for webs and flight,
// CANDIDATE for a body on the street), and its yaw rate turns the tracking space about his head
// (GameTrackingRig::vehicleTurn), both one frame on, together. The game's own collision has the body: a
// wall stops it, and a body far slower than the van asked (stuckFrames) or the bumper ray's hit stops the
// van. Getting out (the inside handle, at a crawl): the van parks where it is in the WORLD and the body is
// walked out beside the driver door (exitStep) over at most alightSeconds. Fail-safe: a bridge that does
// not carry (refusedFrames), a bad pose or a NaN parks the van where it is (the preview's parked van), the
// player standing - never a crash, never the flatscreen.
struct CarryTune {
    float bodyMost = 18.f;        // m/s: a carry asked faster than this is a fault (topSpeed 16 + the swing)
    float stuckRatio = .35f;      // the body under this share of the van's speed ...
    uint32_t stuckFrames = 8;     // ... this many frames: something holds it (a wall, a car) - the van stops
    uint32_t refusedFrames = 24;  // frames the bridge may not carry before the van parks (about 0.25 s)
    float alightSeconds = 1.2f, alightSpeed = 2.5f, alightNear = .15f;
    float rpmSeconds = .25f;      // the engine's sound follows the van this fast
    // Version 0.0.73 test 3 BOARDING (Tune::boardPlayer): his room turned to the van's forward in steps of
    // at most boardTurnStep a frame (a 180 is 4 frames, a snap under a blink), his body carried to the seat
    // (boardSpeed at most, slowing in the last metre) until within boardNear or boardSeconds; then a
    // boardFade-second fade of the comfort blink.
    float boardTurnStep = .9f, boardSpeed = 6.f, boardNear = .06f, boardSeconds = .6f, boardFade = .15f;
};
struct CarryIn {
    bool ready{};        // the bridge runs, its state fresh, gameplay: a carry can be asked
    bool carried{};      // the bridge drove the body with the last carry (its Data's status)
    bool bodyKnown{};
    Vec3 body{}, bodyVelocity{}; // the player's body (world; the bridge's)
    bool bumper{};       // the bumper ray hit within its reach (its answer this frame)
    Pose roomToWorld{};  // the tracking space in the world, this frame
};
struct CarryOut {
    bool carry{};       // ask the bridge to drive the body at `velocity`
    Vec3 velocity{};    // world, level (the bridge adds the fall / the street)
    float turn{};       // the tracking space's turn this frame, radians (+: to the left)
    bool lockStick{};   // no stick turning (it would swing the van with the room)
    bool fellBack{};    // this frame the carry was given up: the van parked where it is
    bool alighting{};
    bool boarding{};    // Version 0.0.73 test 3: getting him into the seat (the van stays put)
    float shade{};      // Version 0.0.73 test 3: the comfort blink 0..1 (game_xr: the vignette ring)
    float rpm{};        // the engine's sound, 0 idle .. 1 flat out
};
inline bool fine(Quat q) {
    const float n = q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w;
    return std::isfinite(n) && std::abs(n - 1) < .01f;
}
inline Frame frameIn(const Pose& to, const Frame& f) { // a frame carried by `to` (its yaw only)
    Frame w;
    w.at = compose(to, Pose{f.at, {}}).position;
    const Vec3 a = to.orientation.rotate(f.forward());
    w.yaw = std::atan2(-a.x, -a.z);
    return w;
}
struct Driver {
    Van van;
    Frame room{};              // the van in the tracking space while carried
    bool carried{};            // driving with the body carry (else the preview: nothing moves)
    uint32_t stuck{}, refused{}, sinceEntry{};
    float alightLeft{}, rpm{};
    Vec3 alightTo{};
    // Version 0.0.73 test 3 BOARDING: the room's turn still to give, the time so far, the blink's fade
    bool boarding{}, boarded{};
    float boardTurn{}, boardTime{}, shadeLeft{};
    uint64_t fallbacks{}, bumps{}, alights{};
    template <class Ground, class Clear, class Ahead>
    Out step(const In& in, const CarryIn& ci, CarryOut& co, Ground&& groundAt, Clear&& clear, Ahead&& ahead,
             const Tune& t = {}, const CarryTune& ct = {}) {
        co = {};
        const float dt = fine(in.dt) ? std::clamp(in.dt, 0.f, .1f) : 0.f;
        const bool poseOk = fine(ci.roomToWorld.position) && fine(ci.roomToWorld.orientation);
        const auto fallBack = [&] {
            if (carried && van.state == driving) {
                van.park(); // where it is in the world
                ++fallbacks;
                co.fellBack = true;
            }
            carried = false;
        };
        if (van.state == driving && carried) {
            if (!poseOk || !ci.ready)
                fallBack();
            else {
                const Frame w = frameIn(ci.roomToWorld, room);
                if (fine(w.at) && fine(w.yaw))
                    van.frame = w;
                else
                    fallBack();
            }
        }
        const auto bumperAhead = [&](Vec3 from, Vec3 dir, float d) { return !ci.bumper && ahead(from, dir, d); };
        Out o;
        if (carried && van.state == driving) {
            const Frame before = van.frame;
            const float speedWas = van.speed;
            o = van.step(in, groundAt, clear, bumperAhead, t);
            if (o.exited) {
                van.frame = before; // parked where it is
                carried = false;
                alightTo = before.world(t.exit);
                alightLeft = ct.alightSeconds;
                ++alights;
                boarded = false;
            } else if (van.state == driving) {
                Vec3 v = dt > 0 ? (van.frame.world(t.eye) - before.world(t.eye)) * (1 / dt) : Vec3{};
                v.y = 0;
                float turn = van.frame.yaw - before.yaw;
                if (fine(turn)) // the yaw's wrap at +-pi is no turn
                    turn = std::remainder(turn, 6.2831853f);
                van.frame = before; // the room carries it
                // the body far slower than the van: held by something
                const float s = std::abs(van.speed);
                if (ci.bodyKnown && fine(ci.bodyVelocity) && s > 2.f && sinceEntry > 6) {
                    const Vec3 dir = van.speed > 0 ? before.forward() : before.forward() * -1.f;
                    const float along = dot(Vec3{ci.bodyVelocity.x, 0, ci.bodyVelocity.z}, dir);
                    stuck = along < ct.stuckRatio * s ? stuck + 1 : 0;
                } else
                    stuck = 0;
                if (stuck >= ct.stuckFrames) {
                    o.bumped = std::abs(speedWas) > 3.f;
                    o.haptic[0] = o.haptic[1] = o.bumped ? 1.f : .3f;
                    van.speed = 0;
                    v = {};
                    turn = 0;
                    stuck = 0;
                    ++bumps;
                }
                refused = ci.carried || sinceEntry < 4 ? 0 : refused + 1;
                ++sinceEntry;
                if (!fine(v) || length(v) > ct.bodyMost || !fine(turn) || std::abs(turn) > 1.f || refused >= ct.refusedFrames)
                    fallBack();
                else {
                    co.carry = true;
                    co.velocity = v;
                    co.turn = turn;
                    co.lockStick = true;
                    o.seat = false;
                }
            } else
                carried = false; // parked by step() (the setting, the play gate, a NaN): no carry
        } else {
            const bool wasDriving = van.state == driving;
            o = van.stepPreview(in, groundAt, clear, bumperAhead, t);
            if (o.entered && t.boardPlayer) {
                // Version 0.0.73 test 3: the van stays exactly where it is parked; he turns to its forward
                // (the room about his head) and is carried to the seat (below).
                Vec3 h{in.look.x, 0, in.look.z};
                const float headYaw = fine(h) && length(h) > 1e-3f ? std::atan2(-h.x, -h.z) : van.frame.yaw;
                boardTurn = std::remainder(van.frame.yaw - headYaw, 6.2831853f);
                if (!fine(boardTurn))
                    boardTurn = 0;
                boarding = true;
                boarded = false;
                boardTime = 0;
            } else if (o.entered && ci.ready && poseOk) {
                // the preview brought the drawn van round him; from here the room carries it
                const Pose back = inverse(ci.roomToWorld);
                const Frame r = frameIn(back, van.frame);
                if (fine(r.at) && fine(r.yaw)) {
                    room = r;
                    carried = true;
                    stuck = refused = sinceEntry = 0;
                    alightLeft = 0;
                }
            }
            (void)wasDriving;
            if (van.state == driving && !carried)
                co.lockStick = true; // the preview's wheel: no turning away from it
            if (o.exited && boarded) { // carried in, not driven (the carry gave up): walked out beside the door
                alightTo = van.frame.world(t.exit);
                alightLeft = ct.alightSeconds;
                ++alights;
                boarded = false;
            }
            if (boarding && van.state != driving)
                boarding = false;
            if (boarding) {
                boardTime += dt;
                Vec3 d = van.frame.world(t.eye) - in.head;
                d.y = 0;
                const float n = fine(d) ? length(d) : 0.f;
                const bool canCarry = ci.ready && poseOk && fine(in.head);
                const bool there = !canCarry || n <= ct.boardNear || boardTime >= ct.boardSeconds;
                if (boardTurn == 0 && there && !o.entered) {
                    // In the seat, facing the van's forward: from here the room carries the van (this frame's
                    // room has every turn and carry asked before - nothing is asked now).
                    boarding = false;
                    shadeLeft = ct.boardFade;
                    if (canCarry) {
                        const Frame r = frameIn(inverse(ci.roomToWorld), van.frame);
                        if (fine(r.at) && fine(r.yaw)) {
                            room = r;
                            carried = true;
                            stuck = refused = sinceEntry = 0;
                            alightLeft = 0;
                        }
                    }
                } else {
                    if (boardTurn != 0) {
                        const float step = std::clamp(boardTurn, -ct.boardTurnStep, ct.boardTurnStep);
                        co.turn = step;
                        boardTurn = std::abs(boardTurn - step) < 1e-5f ? 0.f : boardTurn - step;
                    }
                    if (canCarry && n > ct.boardNear && boardTime < ct.boardSeconds) {
                        co.carry = true;
                        co.velocity = d * (std::min(ct.boardSpeed, n * 10.f) / n);
                        boarded = true;
                    }
                    co.boarding = co.lockStick = true;
                    co.shade = 1;
                }
            }
        }
        if (shadeLeft > 0 && !co.boarding) {
            co.shade = std::max(co.shade, std::clamp(shadeLeft / ct.boardFade, 0.f, 1.f));
            shadeLeft -= dt;
        }
        // Getting out: walked to the street beside the driver door.
        if (alightLeft > 0) {
            alightLeft -= dt;
            Vec3 d = alightTo - ci.body;
            d.y = 0;
            const float n = length(d);
            if (!ci.ready || !ci.bodyKnown || !fine(d) || n < ct.alightNear || alightLeft <= 0 || van.state == driving)
                alightLeft = 0;
            else {
                co.carry = true;
                co.velocity = d * (std::min(ct.alightSpeed, n * 4.f) / n);
                co.alighting = co.lockStick = true;
            }
        }
        // The engine's sound: the speed and the pedal.
        const bool on = van.state == driving;
        const float target = on ? std::clamp(.65f * std::abs(van.speed) / t.topSpeed +
                                                 ((o.pedal == Pedal::drive || o.pedal == Pedal::reverse) ? .35f : 0.f), 0.f, 1.f)
                                : 0.f;
        rpm += (target - rpm) * (dt > 0 ? 1 - std::exp(-dt / ct.rpmSeconds) : 0.f);
        if (!fine(rpm))
            rpm = 0;
        co.rpm = rpm;
        // 0.0.73 test 3 SEATED STICK: in the seat the right stick never turns the view (smooth or snap) - every
        // driving frame, carried or not (the preview, a carry given up), so a stick pulled back to reverse
        // cannot drift it. The van's own turn (co.turn) still turns the room.
        co.lockStick = seatStickLock(van.state, co.lockStick);
        return o;
    }
};

// Version 0.0.72: the steering wheel's model axes in the van's frame, turned `angle` (+ clockwise as the
// driver sees it): `up` the model's +y (12 o'clock), `toward` its +z (to the driver).
inline void wheelAxes(float angle, Vec3& up, Vec3& toward, const Tune& t = {}) {
    const Vec3 a = axisOf(t);
    const Vec3 up0 = normalized(Vec3{0, 1, 0} - a * a.y), right{1, 0, 0};
    const float c = fine(angle) ? std::cos(angle) : 1.f, s = fine(angle) ? std::sin(angle) : 0.f;
    up = up0 * c + right * s;
    toward = a;
}
// A direction in the van's frame to the world.
inline Vec3 worldDir(const Frame& k, Vec3 v) { return k.right() * v.x + Vec3{0, v.y, 0} + k.forward() * (-v.z); }

} // namespace spidy::van072
