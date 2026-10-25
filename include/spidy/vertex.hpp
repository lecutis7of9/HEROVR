#pragma once
#include "math.hpp"

namespace spidy {
// A web strand vertex's place across and along the strand (web_shading.hpp);
// all 0 for everything else, which the overlay draws in its plain colour.
struct Shape {
    float x{}, y{}, z{}, w{};
};
// Overlay vertex: world position in metres, linear RGB, and opacity (the
// overlay blends a vertex below 1 over what is drawn behind it).
struct Vertex {
    Vec3 position, color;
    float alpha = 1;
    Shape shape{};
};
} // namespace spidy
