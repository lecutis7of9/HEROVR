#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
//
// ULTIMATE ALLIANCE EXPANSION fix 2 (the furniture's flicker in Stark Tower): the model sink's triangles drawn
// RELATIVE TO THE EYE. The overlay's vertices are game-world metres; the city puts them kilometres from the world's
// origin, so the GPU's float maths (the view-projection times the position: products of thousands summed to a few
// metres) lost the last millimetres to centimetres of each corner's depth - a different loss for every corner,
// changing as the head moves. A UA place is a whole room of close, thin, stacked surfaces (cushions on seats, trims on
// table tops, both sides of every panel: the room is drawn without culling), and they fought in crawling stripes.
// Now, while in a room, D3D12Renderer subtracts `origin` (the eye) from every model-sink vertex on the CPU (the two
// are near each other, so the difference is exact) and draws them with the view-projection moved by the same origin,
// worked out in double: the GPU then only ever multiplies small numbers. Pure (no D3D): the tests check it.
#include "math.hpp"
#include "vertex.hpp"

#include <array>
#include <cmath>
#include <span>
#include <vector>

namespace spidy::model_origin {

// The view-projection (row-major, clip = M (x, y, z, 1)) that takes positions relative to `origin`:
// M T(origin) - only the fourth column changes, each row's  M[r][0..2] . origin + M[r][3]  summed in double.
inline Mat4 relative(const Mat4& m, const std::array<double, 3>& origin) {
    Mat4 out = m;
    for (int r = 0; r < 4; ++r) {
        const double t = static_cast<double>(m[r * 4 + 0]) * origin[0] + static_cast<double>(m[r * 4 + 1]) * origin[1] +
                         static_cast<double>(m[r * 4 + 2]) * origin[2] + static_cast<double>(m[r * 4 + 3]);
        out[r * 4 + 3] = static_cast<float>(t);
    }
    return out;
}

// Usable: finite and inside the overlay's world (no NaN, nothing absurd).
inline bool usable(const std::array<double, 3>& o) {
    for (const double v : o)
        if (!std::isfinite(v) || std::abs(v) > 1e7)
            return false;
    return true;
}

// The vertices moved by -origin (the positions only; colour, opacity and shape as they were).
inline void shift(std::span<const Vertex> in, const std::array<double, 3>& origin, std::vector<Vertex>& out) {
    out.assign(in.begin(), in.end());
    const float ox = static_cast<float>(origin[0]), oy = static_cast<float>(origin[1]), oz = static_cast<float>(origin[2]);
    for (Vertex& v : out)
        v.position = Vec3{v.position.x - ox, v.position.y - oy, v.position.z - oz};
}

// The origin a frame uses: the eye, rounded to whole centimetres (so the float origin is the double one exactly
// enough, and nothing else depends on the eye's last digits).
inline std::array<double, 3> of(Vec3 eye) {
    return {std::round(static_cast<double>(eye.x) * 100.0) / 100.0, std::round(static_cast<double>(eye.y) * 100.0) / 100.0,
            std::round(static_cast<double>(eye.z) * 100.0) / 100.0};
}

} // namespace spidy::model_origin
