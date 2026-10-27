#pragma once
// Version 0.0.7 DAREDEVIL BATON GRIP: how a baton held in the hand sits in the fist.
//
// 0.0.6 to 0.0.7 af399a9 drew the held baton along the hand's AIM (Frame::Hand::aim: AIM AXIS
// HAND = webShooter's -Z, the grip's -Y; AIM POSE = the runtime's aim pose) from 0.12 m behind
// the grip point to 0.45 m ahead: a pointer sticking out past the knuckles, 30 degrees below the
// forearm toward the little finger on Touch, its butt inside the wrist, the hand 21% of the way up,
// and turning with AIM AXIS and the runtime's aim pose ("daredevils batons sit weird in his
// hands not like a natural grip", public 0.0.63).
//
// Now from the controller's GRIP pose only (OpenXR /input/grip/pose, the pose meant "to reliably
// render a virtual object held in the user's hand"): its +X normal to the palm (right on both
// hands), -Z through the curled fingers' tube from the little finger toward the thumb, +Y by the
// right-hand rule (toward the wrist), the position the closed fist's palm centroid, centred in
// the controller's grip (OpenXR 1.1 specification, "Standard pose identifiers"). The shaft runs
// through that fist from under the heel of the hand out over the index knuckle, offForearmDeg off
// the forearm toward the thumb's side (an escrima stick's or a hammer's line), the hand gripShare
// of the way from the butt. Only the grip's Y-Z plane is used, so a left hand (whose grip frame
// mirrors a right one's with +X kept to the right) holds the mirror image of a right hand's.
//
// The forearm is not the grip's +Y: how a hand holds a controller's handle tilts the grip off it,
// handleTiltDeg toward +Z (the little finger's side), per controller (ControllerKind,
// controller_layout.hpp: runtime.controllers()):
//  - Touch: 30 degrees (CONFIRMED in this project: tracking.hpp Update 20, Quest 3, "a hand round
//    a Touch handle holds it tilted some 30 degrees"; webShooter and body_ik use it).
//  - Index: 15 degrees (CANDIDATE: the Index handle sits more upright in the strapped hand; no
//    Index headset check yet - HERO-0.0.7.md headset checklist).
//  - Steam Frame, other and unknown: the Touch value (CANDIDATE: SteamVR maps the Frame from the
//    Touch layout; Virtual Desktop reports Touch).
// Visual and the in-hand swing's shaft only: throws, the chain, the aim assist and the holsters
// still use the hand's position and aim as before. No game address.
#include "spidy/controller_layout.hpp"
#include "spidy/math.hpp"

#include <algorithm>
#include <cmath>

namespace spidy::baton_grip {
inline constexpr float length = .57f;          // m butt to tip (0.0.62's held baton, the Yanez model's 57 cm)
inline constexpr float gripShare = 1.f / 3.f;  // the hand this far from the butt
inline constexpr float offForearmDeg = 20.f;   // the shaft off the forearm toward the thumb (15..25)
inline constexpr float touchTiltDeg = 30.f;    // CONFIRMED (tracking.hpp Update 20)
inline constexpr float indexTiltDeg = 15.f;    // CANDIDATE

// How far the grip's +Y is off the forearm (toward the grip's +Z) for that controller.
constexpr float handleTiltDeg(ControllerKind kind) {
    return kind == ControllerKind::index ? indexTiltDeg : touchTiltDeg;
}
// The grip's -Y turned deg toward its -Z (the thumb's side), grip-local.
inline Vec3 localDirection(float deg) {
    const float a = deg * 3.14159265f / 180.f;
    return {0, -std::cos(a), -std::sin(a)};
}
// The forearm's way (wrist toward the knuckles) in the grip frame for that controller.
inline Vec3 localForearm(ControllerKind kind) { return localDirection(handleTiltDeg(kind)); }
// The shaft's way (butt toward tip) in the grip frame for that controller.
inline Vec3 localShaft(ControllerKind kind) { return localDirection(handleTiltDeg(kind) + offForearmDeg); }

struct Shaft {
    Vec3 butt{}, tip{};
    bool ok{};
};
// The held baton from a hand's grip pose (tracking space in, tracking space out). hand: 0 left,
// 1 right (the same transform: the grip frame mirrors itself). A pose with no usable rotation or
// position: ok false, nothing drawn from it (the caller keeps 0.0.62's shaft).
inline Shaft shaft(const Pose& grip, unsigned hand, ControllerKind kind, float len = length) {
    (void)hand;
    Shaft s{};
    const Quat& q = grip.orientation;
    const float n = q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w;
    if (!finite(grip.position) || !std::isfinite(n) || std::abs(n - 1.f) > .01f || !std::isfinite(len) || len < .05f)
        return s;
    const Vec3 way = normalized(q.rotate(localShaft(kind)));
    if (!finite(way))
        return s;
    s.butt = grip.position - way * (len * gripShare);
    s.tip = grip.position + way * (len * (1.f - gripShare));
    s.ok = finite(s.butt) && finite(s.tip);
    return s;
}
} // namespace spidy::baton_grip
