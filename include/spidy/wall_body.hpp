#pragma once
// Update 37.2: WALL BODY (live.txt wall_body, SpidyWallBody; default OFF). On a wall the body
// stands straight out from it with the player (the view's own wall hold). On, the drawn body
// lies against the wall instead: a turn about the wall's level axis lays its up along the wall
// (the way up it) and its front onto the wall, about the eyes, and - tilted - the legs keep
// the game's own wall-crawl pose. Only the picture: the view, the aim and the climbing are the
// same either way. Floors and ceilings (the surface's normal within 45 degrees of vertical)
// are left as they are.
#include "spidy/math.hpp"

#include <cmath>

namespace spidy::wall_body {

inline constexpr bool defaultOn = false;
inline constexpr float wallLimit = .7071f; // |normal.y| under this: a wall
inline constexpr float layAngle = 1.3f;    // radians (about 75 degrees: the chest just off the wall)

inline bool isWall(Vec3 normal) {
    const float n = length(normal);
    return std::isfinite(n) && n > .5f && std::fabs(normal.y / n) < wallLimit;
}
// The tilt (world axes) for a body standing along `normal` (the wall's, out of it): the
// identity off a wall or when off.
inline Quat tilt(bool on, Vec3 normal) {
    if (!on || !isWall(normal))
        return {};
    const Vec3 n = normalized(normal);
    Vec3 alongUp = Vec3{0, 1, 0} - n * n.y; // up the wall
    if (!(length(alongUp) > 1e-3f))
        return {};
    alongUp = normalized(alongUp);
    // Turning n toward alongUp lays the body's up along the wall; its front (into the wall)
    // follows, so the chest faces the wall.
    const Vec3 axis = normalized(cross(n, alongUp));
    return Quat::around(axis, layAngle);
}

} // namespace spidy::wall_body
