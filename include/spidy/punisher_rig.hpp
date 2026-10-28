// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
#pragma once
// Version 0.0.73 test 3 PUNISHER HOOK GRAB (the user, test 2: "still impossible to grab your grapple"; the
// calibration screen is fine, so not the body size). Why test 2's rig (hero072::atRig) missed:
//  - it was measured from the EYES: looking down at the chest to grab (the natural move) swings the eyes
//    forward and down about the neck, and the hand on the sternum fell behind rigNear (.04 m ahead of the
//    eyes) and above rigBelow - out of the zone exactly when the player looked at it;
//  - it was small (30 cm wide, 20 cm tall, 28 cm deep), unscaled (no calibrated eye height), and the left
//    hand only;
//  - the hip racks (fix 8) ran their own body yaw that never turned (game_xr's holster dt was about 0), and
//    a calibrated hip (Fit::hipBelow) moves the pistols' zone up toward the chest.
// Now: the grab is an ellipsoid around the chest plate, anchored at the NECK PIVOT (the eyes' position
// minus the head's own offset, so a head tilt does not move it), along the body's yaw, scaled with the
// calibrated eye height (holster::scaleFor), either empty hand. Where it meets a hip zone the RIG wins: the
// pistol rack takes no new draw from a hand at the rig (hero007_play, guardScale a little bigger).
// The neck and chest offsets are average adult proportions (CANDIDATE: tuned by the player's report).
#include "spidy/holster.hpp"
#include "spidy/math.hpp"

#include <cmath>

namespace spidy::punisher_rig {

// the head frame (x right, y up, -z ahead): the eyes to the neck pivot, standing metres
inline constexpr float neckDown = .10f, neckBack = .09f;
// the neck pivot to the grab centre (the sternum plate, a hand's depth out), body frame, standing metres
inline constexpr float rigDown = .22f, rigAhead = .17f;
// the ellipsoid's half sizes (standing metres): side, up, down, in (toward the body), out (ahead)
inline constexpr float halfSide = .22f, halfUp = .15f, halfDown = .15f, halfIn = .15f, halfOut = .22f;
// the pistol rack's guard: a little bigger than the grab, so the hook always wins at its edge
inline constexpr float guardScale = 1.15f;

inline bool fine(Vec3 v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }
inline float scaleOf(float s) { return std::isfinite(s) && s > .5f && s < 2.f ? s : 1.f; }

// The neck pivot (tracking space) for the head pose: steady under a nod or a look down.
inline Vec3 neck(const Pose& head, float scale) {
    const float k = scaleOf(scale);
    return head.position + head.orientation.rotate(Vec3{0, -neckDown * k, neckBack * k});
}
// The grab centre (tracking space).
inline Vec3 centre(const Pose& head, const holster::BodyYaw& body) {
    const float k = scaleOf(body.scale);
    return neck(head, k) + Vec3{0, -rigDown * k, 0} + body.forward() * (rigAhead * k);
}
// 0 at the centre, 1 on the ellipsoid's surface (NaN-safe: 1e9 for no answer).
inline float reach(Vec3 hand, const Pose& head, const holster::BodyYaw& body) {
    if (!body.valid || !fine(hand) || !fine(head.position))
        return 1e9f;
    const float k = scaleOf(body.scale);
    const Vec3 d = (hand - centre(head, body)) * (1.f / k);
    const float s = dot(d, body.right()) / halfSide;
    const float u = d.y / (d.y >= 0 ? halfUp : halfDown);
    const float a = dot(d, body.forward());
    const float f = a / (a >= 0 ? halfOut : halfIn);
    const float r = s * s + u * u + f * f;
    return std::isfinite(r) ? r : 1e9f;
}
inline bool at(Vec3 hand, const Pose& head, const holster::BodyYaw& body) { return reach(hand, head, body) <= 1.f; }
// The pistol rack's guard (a hand here never draws a gun: the grip is the hook's).
inline bool guarded(Vec3 hand, const Pose& head, const holster::BodyYaw& body) {
    return reach(hand, head, body) <= guardScale * guardScale;
}

} // namespace spidy::punisher_rig
