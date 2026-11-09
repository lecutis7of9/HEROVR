#pragma once
// Experimental 0.0.79.1: keep the VR head outside the face of a building
// while HEROVR's hand-climb wall-run owns the movement. This corrects the
// tracked VIEW placement, not the game's collision body or any eye shaders.
#include "math.hpp"
#include <algorithm>
#include <cmath>

namespace spidy::wall_run_clearance {

// The game gives the actual contact anchor and outward face normal. A
// world-upright head can otherwise be rotated onto the wall's plane by the
// standing transition (GameTrackingRig::surfaceTurn_), putting the avatar
// and/or camera inside the facade. This is for ACTIVE side-wall running only;
// ground, roofs, ordinary native crawling and airborne flight stay unchanged.
//
// Return a world-space correction to the already-placed headset rig. Bounded
// to 75 cm if the collision anchor is stale: prefer a repeatable small
// correction to suddenly teleporting the player's head across a room.
inline Vec3 headPush(bool running, Vec3 eye, Vec3 anchor, Vec3 outward, float wantedMetres) {
    if (!running || !finite(eye) || !finite(anchor) || !finite(outward) ||
        !std::isfinite(wantedMetres) || length(outward) < .5f)
        return {};
    const Vec3 n = normalized(outward);
    if (std::abs(n.y) >= .5f) // not the side of a building
        return {};
    const float gap = dot(eye - anchor, n);
    if (!std::isfinite(gap))
        return {};
    const float minimum = std::clamp(wantedMetres, .5f, 1.f);
    return n * std::clamp(minimum - gap, 0.f, .75f);
}

} // namespace spidy::wall_run_clearance
