#pragma once
#include "math.hpp"

#include <cstdint>

namespace spidy {
// A web strand vertex's place across and along the strand (web_shading.hpp);
// all 0 for everything else, which the overlay draws in its plain colour.
struct Shape {
    float x{}, y{}, z{}, w{};
};
// HERO VR 0.0.78 PBR MATERIALS (model_pbr.hpp): a model corner's material, packed. All 0 (the default, and every
// vertex that is not a PBR model corner): the 0.0.78 look, the vertex colour times the texture.
//   normal  the corner's world normal (octahedral, 2 x snorm16), the one the CPU lit it with
//   tangent its world tangent (octahedral, 2 x snorm16; 0: none, the pixel shader's own frame from the UVs)
//   maps    the normal-map layer (bits 0-9), the ORM layer (10-19) and the flags (model_pbr::flag*)
//   tint    the albedo factor (sqrt-coded rgb, unorm8) and the HQ base layer (a, 255 none)
//   params  roughness, metallic, normal scale / 2, occlusion strength (unorm8 each)
struct Surface {
    uint32_t normal = 0, tangent = 0, maps = 0, tint = 0, params = 0;
};
// Overlay vertex: world position in metres, linear RGB, and opacity (the
// overlay blends a vertex below 1 over what is drawn behind it).
struct Vertex {
    Vec3 position, color;
    float alpha = 1;
    Shape shape{};
    Surface surface{};
};
} // namespace spidy
