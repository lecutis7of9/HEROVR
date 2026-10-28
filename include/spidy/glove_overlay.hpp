#pragma once
#include "soft_shapes.hpp"
#include "vertex.hpp"
#include <algorithm>
#include <cmath>
#include <vector>

// Version 0.0.72 GAME BODY (EXPERIMENTAL) fallback (spec point 3, "Fallback"): while GAME BODY is
// wanted but the game's hero is not drawn as the VR body (no live command, an unknown rig, a job
// that is not the hero's, the hook not on yet), the hero stays hidden by the game's own visibility
// switch and the overlay draws a simple glove on each tracked controller instead, so the player
// never loses his hands. Pure geometry, no game memory: flat soft bars across the line of sight.
namespace spidy::glove_overlay {
// Glove frame from the controller's grip pose: fingers along -z, the back of the hand +y
// (health_ring's convention); metres.
inline constexpr float palmBack = .07f;    // the wrist, behind the grip
inline constexpr float palmFront = .025f;  // the knuckles, ahead of it
inline constexpr float palmHalf = .036f;   // the palm's half width (drawn)
inline constexpr float fingerLength = .075f;
inline constexpr float fingerHalf = .0085f;
inline constexpr Vec3 suitRed{.62f, .03f, .04f};
inline constexpr Vec3 batDark{.10f, .10f, .12f}; // Test build 10: BATMAN's gloves (no suit model shown)
// Whether the fallback gloves show: GAME BODY wanted for this hero, a headset picture (not the
// flat screen), and the hero's body not drawn right now (native_body::drawn).
constexpr bool wanted(bool gameBodyOn, bool heroShowsBody, bool flatScreen, bool bodyDrawn) {
    return gameBodyOn && heroShowsBody && !flatScreen && !bodyDrawn;
}
inline bool usable(const Pose& grip) {
    const auto& q = grip.orientation;
    const float n = q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w;
    return std::isfinite(grip.position.x) && std::isfinite(grip.position.y) && std::isfinite(grip.position.z) &&
           std::isfinite(n) && std::abs(n - 1) < .1f;
}
// One glove at `grip` (world, already moved with the image), `right` the right hand, `fist`
// 0 open .. 1 closed (curls the fingers toward the palm), seen from `viewer`; `soft` the edge
// fade (about a pixel's width at that distance). Returns how many vertices it added (0: none
// for an unusable pose or input).
inline size_t append(std::vector<Vertex>& out, const Pose& grip, bool right, float fist, Vec3 viewer, float soft,
                     Vec3 colour = suitRed) {
    if (!usable(grip) || !std::isfinite(soft) || !std::isfinite(viewer.x) || !std::isfinite(viewer.y) ||
        !std::isfinite(viewer.z))
        return 0;
    const size_t before = out.size();
    fist = std::isfinite(fist) ? std::clamp(fist, 0.f, 1.f) : 0.f;
    soft = std::clamp(soft, .0002f, .01f);
    const Vec3 forward = grip.orientation.rotate({0, 0, -1});
    const Vec3 back = grip.orientation.rotate({0, 1, 0});
    const Vec3 thumbSide = grip.orientation.rotate({right ? -1.f : 1.f, 0, 0});
    const Vec3 wrist = grip.position - forward * palmBack;
    const Vec3 knuckles = grip.position + forward * palmFront;
    const Vec3 view = normalized(grip.position - viewer);
    soft::softBar(out, wrist, knuckles, view, palmHalf, colour, .95f, soft);
    // Four fingers from the knuckle line; a fist folds them down past the palm.
    const float bend = fist * 1.9f;
    const Vec3 along = forward * std::cos(bend) - back * std::sin(bend);
    for (int f = 0; f < 4; ++f) {
        const float across = (f - 1.5f) * (palmHalf * 2 / 4);
        const Vec3 base = knuckles - thumbSide * across;
        soft::softBar(out, base, base + along * fingerLength, view, fingerHalf, colour * .85f, .95f, soft);
    }
    // The thumb from the palm's thumb side, closing in over the fingers with the fist.
    const Vec3 thumbBase = grip.position + thumbSide * palmHalf - forward * .015f;
    const Vec3 thumbWay = normalized(forward * (.6f + .3f * fist) + thumbSide * (.5f - .6f * fist) - back * .3f * fist);
    soft::softBar(out, thumbBase, thumbBase + thumbWay * .055f, view, fingerHalf * 1.1f, colour * .8f, .95f, soft);
    return out.size() - before;
}
} // namespace spidy::glove_overlay
