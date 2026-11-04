#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
// HERO VR 0.0.77 WAYPOINT (story077g.hpp): the yellow objective diamond with its distance beneath, and the arrow at
// the view's edge. The mod's own drawing; never the game's picture.
#include "math.hpp"
#include "vertex.hpp"
#include <string_view>
#include <vector>

namespace spidy::waypoint_mark {
// `centre` the diamond's middle, `right` / `up` unit and square, `size` its half height (metres), `alpha` 0..1.
// Returns the vertices added (0: bad numbers).
size_t diamond(std::vector<Vertex>& out, std::string_view text, Vec3 centre, Vec3 right, Vec3 up, float size,
               float alpha);
// An arrow at `centre` pointing along (x, y) of right / up.
size_t arrow(std::vector<Vertex>& out, Vec3 centre, Vec3 right, Vec3 up, float x, float y, float size, float alpha);
} // namespace spidy::waypoint_mark
