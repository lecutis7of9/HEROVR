#pragma once
// Version 0.0.7 PUNISHER GUN GRIP: how a pistol or the sawed-off sits in the hand.
//
// 0.0.7 up to the BATON GRIP fix drew a held gun along the hand's AIM (Frame::Hand::aim: AIM AXIS
// HAND = the grip's -Y, webShooter's pointing line, 30 degrees below the forearm toward the little
// finger on Touch; AIM POSE = the runtime's aim pose) and kept it upright to the world (gunUp: the
// world's up off the aim), its origin - the front of the grip, at the trigger guard - on the grip
// point: the barrel tipped down off the forearm, the gun never rolled with the wrist, and the grip
// sat ahead of the palm with the frame in the fingers ("make sure punishers gun sit right in his
// hand", 26 Oct).
//
// Now from the controller's GRIP pose (OpenXR /input/grip/pose, see baton_grip.hpp): the barrel
// along the forearm (the grip's -Y turned handleTiltDeg toward -Z, per controller: Touch 30
// CONFIRMED in this project, Index 15 CANDIDATE), the gun's top toward the thumb (the slide over the
// web of the hand), its right the grip's +X (right on both hands, so the left hand holds the mirror
// image), and the look's own grip - its middle, palmPoint - on the grip point (the palm's centroid
// round the fist), so the web of the hand sits high on the backstrap and the index finger lies
// along the trigger guard. On Touch the barrel is then the controller's aim ray (Meta's aim pose
// is the hand turned 30 degrees toward the thumb: tracking.hpp, October 15), and the gun fires
// along it from its muzzle, so the sights are on the shot. No game address.
#include "spidy/baton_grip.hpp"

namespace spidy::gun_grip {
enum Weapon : uint32_t { pistol = 0, sawedOff = 1 };
// The look's grip middle in its own frame (metres forward along the barrel, up toward the top),
// the point the palm's centroid holds. BUILT-IN: hero007_visual's appendPistol / appendSawedOff
// grips; MODEL: the converted Models\Pistol and Models\SawedOff\Frame (tools/models007/convert_models.py).
struct Point {
    float forward{}, up{};
};
constexpr Point palmPoint(Weapon w, bool model) {
    return w == sawedOff ? (model ? Point{-.038f, -.016f} : Point{-.018f, -.015f})
                         : (model ? Point{-.026f, -.034f} : Point{-.028f, -.012f});
}
// The muzzle (the bore's end) from the grip point, in the gun's frame (the BUILT-IN look's).
constexpr Point muzzleFromPalm(Weapon w) {
    return w == sawedOff ? Point{.388f, .072f} : Point{.190f, .074f};
}
// The breech / ejection port from the grip point (the spent shells' start).
constexpr Point breechFromPalm(Weapon w) {
    return w == sawedOff ? Point{.088f, .065f} : Point{.050f, .074f};
}
// The gun's top toward the thumb, perpendicular to the barrel, grip-local.
inline Vec3 localUp(ControllerKind kind) { return baton_grip::localDirection(baton_grip::handleTiltDeg(kind) + 90.f); }

struct Held {
    Vec3 at{};                         // the grip point (tracking space): the look's palmPoint goes here
    Vec3 forward{0, 0, -1}, up{0, 1, 0}; // along the barrel; the gun's top
    bool ok{};
};
// A held gun from a hand's grip pose. hand: 0 left, 1 right (the same transform: the grip frame
// mirrors itself). A pose with no usable rotation or position: ok false (the caller keeps the
// 0.0.62 placement along the aim, upright).
inline Held held(const Pose& grip, unsigned hand, ControllerKind kind) {
    (void)hand;
    Held h{};
    const Quat& q = grip.orientation;
    const float n = q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w;
    if (!finite(grip.position) || !std::isfinite(n) || std::abs(n - 1.f) > .01f)
        return h;
    h.forward = normalized(q.rotate(baton_grip::localForearm(kind)));
    h.up = normalized(q.rotate(localUp(kind)));
    h.at = grip.position;
    h.ok = finite(h.forward) && finite(h.up) && std::abs(dot(h.forward, h.up)) < 1e-3f;
    return h;
}
// A point of the gun given from the grip point (forward, up) in tracking space.
inline Vec3 from(const Held& h, Point p) { return h.at + h.forward * p.forward + h.up * p.up; }
// The look's origin when its palmPoint is on `at` (forward, up orthonormal).
inline Vec3 originFor(Vec3 at, Vec3 forward, Vec3 up, Point palm) { return at - forward * palm.forward - up * palm.up; }
} // namespace spidy::gun_grip
