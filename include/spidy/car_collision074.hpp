#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
// Made by Agent Mulder.
//
// 0.0.74 test 10 CAR COLLISION (owner pt1387u: "make it where you cant just walk into a vehicle it needs collision").
//
// The mod's cars (the PUNISHER's van and every BATMOBILE) are drawn by the mod: the game has no collider for them,
// so on foot he walked straight through them. Now, while he is on foot, the car is a solid box: the model's
// measured box (van072::Tune length x width x height, centred on the car's frame like the summon's clearance box),
// with his body as a .3 m column round the swing bridge's body point.
//  - BODY: the stick's walk into a face is taken off (he slides along the side), and a body that got in anyway is
//    carried out through the nearest face (or round the corner) at a walk - the bridge's level carry (Command v7
//    carry / carryVelocity), the same CONFIRMED path the van's boarding, getting out and CAP SPRINT already use.
//    No game address is read or written here: everything is the mod's own state (CONFIRMED by construction).
//  - HEAD: leaning the head into the car at room scale is held off its side by HeadClearance (the eyes' existing
//    wall push: held off at once, let go over .15 s, never a jump), fed the nearest face as a surface.
//    A head already deep inside (more than .3 m) is never pushed - the body carry brings it out smoothly.
//  - DOOR: by the driver door the side is taken in to the outer handle (the Batmobiles' handle sits in from their
//    widest fender), so the handle is always in reach; a hand gripping at the handle, the boarding, the drive (and
//    the hideout), and getting out (the exit carry) are never blocked. Getting out ends .75 m out from the side, so
//    outside the box; standing inside the car after that is pushed out like anywhere else.
//  - Off: while he drives or rides, in the air (a swing, a fall, a jump: the swing owns the body then), when the
//    car is under him (feet over the roof's top - he can't stand on it: the carry is level, so a landing on the roof
//    slides off the side) or far below it, and with CAR COLLISION OFF (live.txt car_collision 1, live).
//  - NOT here: the car pushing the player when it drives into him (in single player only he drives it).
#include "spidy/van072.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace spidy::car_collision {

// live.txt car_collision: 0 ON (the default), 1 OFF (SpidyCarCollision 5547).
inline constexpr const char* help =
    "CAR COLLISION ON: you can't walk through the Batmobile or the van; OFF lets you pass through like before; "
    "live, no restart.";
inline bool on(uint32_t value) {
    return value != 1;
}

struct Tune {
    float radius = .3f;      // the body's column (the game's capsule is about .25)
    float skin = .12f;       // the stick's walk into a face is taken off this near it
    float deadband = .02f;   // deeper than this inside: carried out
    float gain = 6.f;        // carry speed per metre inside
    float least = .6f, most = 3.f; // the carry's speed (m/s)
    float headMargin = .2f;  // HeadClearance's margin: the head's face is fed this near and nearer
    float headNear = .35f;   // fed while the head is this near the box's face (outside)
    float headDeep = .3f;    // a head deeper inside than this is not pushed (the body carry takes it out)
    float roofStep = .35f;   // feet higher than the roof less this: over the car, no side
    float below = 1.2f;      // feet lower than the car's ground by more: under it (another level), no side
    float eyeHeight = 1.6f;  // the feet from the head when the body's height is unknown
    float doorHalf = .7f;    // the door's notch: this either side of the outer handle along the car
    float handleExempt = .35f; // a hand gripping this near the outer handle: no collision at all
    float grip = .5f;        // a grip past this is held
};

// The car's box in its own frame (x right, y up, z back; van072::Frame).
struct Box {
    van072::Frame frame{};
    float xMin{}, xMax{}, zMin{}, zMax{}, height{};
    bool ok{};
};
// The model's box, with the driver door's notch when `at` (local) is beside the outer handle.
inline Box boxAt(const van072::Frame& f, const van072::Tune& t, Vec3 localAt, const Tune& c = {}) {
    Box b;
    if (!van072::fine(f.at) || !std::isfinite(f.yaw) || !std::isfinite(t.width) || !std::isfinite(t.length) ||
        !std::isfinite(t.height) || t.width <= .1f || t.length <= .1f || t.height <= .1f || t.width > 20 ||
        t.length > 40 || t.height > 20)
        return b;
    b.frame = f;
    b.xMin = -t.width * .5f;
    b.xMax = t.width * .5f;
    b.zMin = -t.length * .5f;
    b.zMax = t.length * .5f;
    b.height = t.height;
    b.ok = true;
    const Vec3 h = t.handleOut;
    if (van072::fine(h) && van072::fine(localAt) && std::abs(localAt.z - h.z) < c.doorHalf) {
        if (h.x < 0 && localAt.x < 0)
            b.xMin = std::clamp(h.x, b.xMin, 0.f);
        else if (h.x > 0 && localAt.x > 0)
            b.xMax = std::clamp(h.x, 0.f, b.xMax);
    }
    return b;
}

// A level point against the box: the way out (world, level, unit) and how far in past `inflate` (<= 0: clear).
struct Contact {
    Vec3 normal{};     // out of the box (world)
    Vec3 point{};      // on the box's face, nearest the point (world; the point's own height)
    float depth = -1e9f; // inflate - distance outside (inside the box: inflate + distance in)
    float inside{};    // how far inside the box itself (0: outside)
    bool ok{};
};
inline Contact contact(const Box& b, Vec3 world, float inflate) {
    Contact c;
    if (!b.ok || !van072::fine(world) || !std::isfinite(inflate))
        return c;
    const Vec3 p = b.frame.local(world);
    if (!van072::fine(p))
        return c;
    const float qx = std::clamp(p.x, b.xMin, b.xMax), qz = std::clamp(p.z, b.zMin, b.zMax);
    const float dx = p.x - qx, dz = p.z - qz;
    Vec3 nLocal{}, qLocal{qx, p.y, qz};
    const float out = std::sqrt(dx * dx + dz * dz);
    if (out > 1e-5f) { // outside: the nearest point of the box (a face or round a corner)
        nLocal = {dx / out, 0, dz / out};
        c.depth = inflate - out;
    } else { // inside: out through the nearest face
        const float faces[4] = {p.x - b.xMin, b.xMax - p.x, p.z - b.zMin, b.zMax - p.z};
        unsigned k = 0;
        for (unsigned i = 1; i < 4; ++i)
            if (faces[i] < faces[k])
                k = i;
        const Vec3 ns[4] = {{-1, 0, 0}, {1, 0, 0}, {0, 0, -1}, {0, 0, 1}};
        nLocal = ns[k];
        qLocal = {k == 0 ? b.xMin : k == 1 ? b.xMax : p.x, p.y, k == 2 ? b.zMin : k == 3 ? b.zMax : p.z};
        c.inside = std::max(faces[k], 0.f);
        c.depth = inflate + c.inside;
    }
    // local (x, z back) to world: x along right, z along -forward
    const Vec3 n = b.frame.right() * nLocal.x + b.frame.forward() * (-nLocal.z);
    c.normal = normalized(Vec3{n.x, 0, n.z});
    c.point = b.frame.world(qLocal);
    c.ok = van072::fine(c.normal) && van072::fine(c.point) && std::isfinite(c.depth) && length(c.normal) > .5f;
    return c;
}

struct In {
    bool enabled = true;           // CAR COLLISION (live.txt)
    van072::State state = van072::none;
    van072::Frame frame{};
    van072::Tune tune{};
    bool boarding{}, carried{}, alighting{}; // van072::Driver: getting in, riding, getting out
    bool airborne{};               // the swing owns the body (flying, swinging, falling)
    Vec3 body{};                   // the swing bridge's body point (world)
    bool bodyKnown{};
    Vec3 head{};                   // the eyes (world)
    Vec3 hands[2]{};               // world
    float grip[2]{};
};
struct Out {
    bool active{};       // the car is solid for him this frame
    bool exempt{};       // a car is there but collision steps aside (door handle, getting in or out, driving)
    bool touching{};     // the body is at a face: the stick's walk into it is taken off
    bool push{};         // carry the body out at `velocity`
    Vec3 normal{};       // the face's way out (world, level)
    Vec3 velocity{};     // the carry (world, level)
    float depth{};       // how far the body is in (past the radius)
    bool face{};         // feed the head's surface
    Vec3 facePoint{}, faceNormal{};
    uint32_t left{};     // frames this answer is still good for (set by the caller)
};

inline Out step(const In& in, const Tune& c = {}) {
    Out o;
    if (!in.enabled || in.state != van072::parked)
        return o; // none: no car; driving: he is in it (boarding and the hideout too)
    if (!van072::fine(in.frame.at) || !std::isfinite(in.frame.yaw))
        return o;
    if (in.boarding || in.carried || in.alighting) {
        o.exempt = true;
        return o;
    }
    const Vec3 handle = in.frame.world(in.tune.handleOut);
    for (unsigned i = 0; i < 2; ++i)
        if (van072::fine(in.hands[i]) && std::isfinite(in.grip[i]) && in.grip[i] > c.grip && van072::fine(handle) &&
            length(in.hands[i] - handle) <= c.handleExempt) {
            o.exempt = true;
            return o;
        }
    const bool headKnown = van072::fine(in.head);
    const bool bodyKnown = in.bodyKnown && van072::fine(in.body);
    if (!headKnown && !bodyKnown)
        return o;
    // Height: from the head (the body point's own height is the bridge's); the car's ground is its frame's.
    const float base = in.frame.at.y;
    const float feetY = headKnown ? in.head.y - c.eyeHeight : in.body.y;
    if (!std::isfinite(feetY) || feetY > base + in.tune.height - c.roofStep || feetY < base - c.below)
        return o;
    o.active = true;
    if (bodyKnown && !in.airborne) {
        const Box b = boxAt(in.frame, in.tune, in.frame.local(in.body), c);
        const Contact k = contact(b, in.body, c.radius);
        if (k.ok && k.depth > -c.skin) {
            o.touching = true;
            o.normal = k.normal;
            o.depth = std::max(k.depth, 0.f);
            if (k.depth > c.deadband) {
                const float speed = std::clamp(k.depth * c.gain, c.least, c.most);
                o.push = true;
                o.velocity = k.normal * speed;
            }
        }
    }
    if (headKnown && in.head.y < base + in.tune.height + c.headMargin) {
        const Box b = boxAt(in.frame, in.tune, in.frame.local(in.head), c);
        const Contact k = contact(b, in.head, 0.f);
        // outside within reach (depth = -distance), or a little in (depth = inside)
        if (k.ok && k.depth > -(c.headMargin + c.headNear) && k.inside <= c.headDeep) {
            o.face = true;
            o.facePoint = k.point;
            o.faceNormal = k.normal;
        }
    }
    return o;
}

// The walk with its part into the touched face taken off (anything else, or no face: as it was).
inline Vec3 clip(Vec3 move, const Out& o) {
    if (!o.active || !o.touching || !van072::fine(move) || !van072::fine(o.normal) || length(o.normal) < .5f)
        return move;
    const float in = dot(move, o.normal);
    if (in >= 0)
        return move;
    const Vec3 out = move - o.normal * in;
    return van072::fine(out) ? out : Vec3{};
}

// Edges of "the car stopped him" for one note a time.
struct Watch {
    bool was{};
    uint32_t stops{};
    int update(bool touching) {
        const int edge = touching == was ? 0 : touching ? 1 : -1;
        stops += edge > 0;
        was = touching;
        return edge;
    }
};

} // namespace spidy::car_collision
