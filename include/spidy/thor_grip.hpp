#pragma once
// Test build 10 THOR HAMMER GRIP ("thors hammer sits really weird"): how Mjolnir sits in the right fist.
//
// Builds 3-9 drew the held hammer along the hand's AIM (the handle pointing out of the knuckles along the
// pointing ray, 30 degrees below the forearm on Touch, the head out in front like a torch), its handle's end
// 6 cm behind the grip point and its face turned to where the player looks - so the head never sat over the
// thumb and the hammer never rolled with the wrist.
//
// Now from the controller's GRIP pose (OpenXR /input/grip/pose, see baton_grip.hpp): its -Z runs through the
// curled fingers' tube from the little finger toward the thumb - the line a held hammer's handle takes.
// The handle runs along that tube (handleOffTubeDeg 0: exactly the controller handle's own line, so the
// hammer's handle IS the controller's handle in the hand), the head UP past the thumb, the handle's end below the little finger; the hand
// gripAlong up the handle from its end (every hammer's grip wrap or shaft above its pommel / strap:
// BOOOOP 0-8 cm pommel, DEVILSEYE shaft from 0, LOVEANDTHUNDER wrap from -15 cm, FCCOOPER strap -4..4 cm,
// conversion-report.json); a striking face toward the knuckles (along the forearm), the other face back.
// The left hand mirrors (the grip frame mirrors itself). No game address.
#include "spidy/baton_grip.hpp"

namespace spidy::thor_grip {
inline constexpr float handleOffTubeDeg = 0.f; // off the grip's -Z toward the forearm (-Y): along it
inline constexpr float gripAlong = .13f;        // m up the handle from its end to the palm's centre
// The handle's way (end -> head) in the grip frame: the grip's -Y turned (90 - handleOffTubeDeg) toward -Z.
inline Vec3 localHandle() { return baton_grip::localDirection(90.f - handleOffTubeDeg); }

struct Held {
    Vec3 at{};                          // the handle's end (the model's origin)
    Vec3 handle{0, 1, 0}, face{0, 0, -1}; // end -> head; a striking face (the model's -z)
    bool ok{};
};
// The held hammer from the right hand's grip pose (tracking space in and out).
inline Held held(const Pose& grip, ControllerKind kind) {
    Held h{};
    const Quat& q = grip.orientation;
    const float n = q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w;
    if (!finite(grip.position) || !std::isfinite(n) || std::abs(n - 1.f) > .01f)
        return h;
    h.handle = normalized(q.rotate(localHandle()));
    const Vec3 forearm = normalized(q.rotate(baton_grip::localForearm(kind)));
    Vec3 f = forearm - h.handle * dot(forearm, h.handle);
    if (!(length(f) > 1e-3f))
        return h;
    h.face = normalized(f);
    h.at = grip.position - h.handle * gripAlong;
    h.ok = finite(h.at) && finite(h.handle) && finite(h.face);
    return h;
}
} // namespace spidy::thor_grip
