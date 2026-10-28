#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
// Made by Agent Mulder.
//
// Test build 10 CONSTRUCT FIST (the user: "when you do the punch glove it isn't like 3d image it's a totally flat
// plane"): GREEN LANTERN's CONSTRUCT FIST was soft bars turned to the eye (billboards: flat from any side). Now a real
// closed 3D mesh made here (no outside asset, so nothing to credit): the back of the hand (a rounded box), four
// curled fingers (three capsules each), the thumb across them and the wrist, smooth normals, drawn lit through the
// model pipeline like every other model. Model space as mesh_model's: metres, +x right, +y up, -z forward (the
// knuckles), the wrist toward +z, the origin at the knuckles' middle. A right fist, palm down; about 0.75 m long.
#include "spidy/math.hpp"
#include "spidy/mesh_model.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace spidy::construct_fist {
inline constexpr Vec3 green{.18f, 1.f, .38f};

namespace detail {
inline void tri(mesh_model::Mesh& m, Vec3 a, Vec3 b, Vec3 c, Vec3 na, Vec3 nb, Vec3 nc, Vec3 colour) {
    m.corners.insert(m.corners.end(), {a, b, c});
    m.normals.insert(m.normals.end(), {na, nb, nc});
    m.colors.push_back(colour);
}
// A capsule from p to q, radius r: `around` segments, `cap` rings per hemisphere; smooth normals.
inline void capsule(mesh_model::Mesh& m, Vec3 p, Vec3 q, float r, Vec3 colour, uint32_t around = 8, uint32_t cap = 3) {
    const float pi = 3.14159265f;
    Vec3 ax = q - p;
    const float len = length(ax);
    ax = len > 1e-6f ? ax * (1.f / len) : Vec3{0, 1, 0};
    const Vec3 side = std::abs(ax.y) < .9f ? normalized(cross(ax, Vec3{0, 1, 0})) : normalized(cross(ax, Vec3{1, 0, 0}));
    const Vec3 other = cross(ax, side);
    // rings from the p pole (-1) to the q pole (+1): cap rings on p's half, cap on q's
    const uint32_t rings = 2 * cap + 2;
    const auto at = [&](uint32_t ring, uint32_t k, Vec3& n) {
        const bool far = ring > cap;
        const float t = far ? static_cast<float>(ring - cap - 1) / cap : static_cast<float>(ring) / cap; // 0..1
        const float phi = far ? t * pi * .5f : -pi * .5f + t * pi * .5f;                                    // -90..90
        const float th = 2 * pi * static_cast<float>(k % around) / around;
        n = ax * std::sin(phi) + (side * std::cos(th) + other * std::sin(th)) * std::cos(phi);
        return (far ? q : p) + n * r;
    };
    for (uint32_t ring = 0; ring + 1 < rings; ++ring)
        for (uint32_t k = 0; k < around; ++k) {
            Vec3 n0, n1, n2, n3;
            const Vec3 a = at(ring, k, n0), b = at(ring, k + 1, n1), c = at(ring + 1, k + 1, n2), d = at(ring + 1, k, n3);
            if (ring != 0)
                tri(m, a, b, c, n0, n1, n2, colour);
            if (ring + 2 != rings)
                tri(m, a, c, d, n0, n2, n3, colour);
        }
}
// A rounded box (superellipsoid, exponent e: 2 an ellipsoid, toward 0 a box) of half sizes h at c.
inline void roundBox(mesh_model::Mesh& m, Vec3 c, Vec3 h, Vec3 colour, float e = .45f, uint32_t around = 16, uint32_t rings = 10) {
    const float pi = 3.14159265f;
    const auto sp = [](float x, float p) { return (x < 0 ? -1.f : 1.f) * std::pow(std::abs(x), p); };
    const auto at = [&](uint32_t i, uint32_t k, Vec3& n) {
        const float v = -pi * .5f + pi * static_cast<float>(i) / rings, u = 2 * pi * static_cast<float>(k % around) / around;
        const float cv = std::cos(v), sv = std::sin(v), cu = std::cos(u), su = std::sin(u);
        const Vec3 p{h.x * sp(cv, e) * sp(cu, e), h.y * sp(sv, e), h.z * sp(cv, e) * sp(su, e)};
        // the normal: the implicit surface's gradient
        const float q = 2.f / e;
        const auto g = [&](float x, float s) { return (x < 0 ? -1.f : 1.f) * std::pow(std::abs(x) / s, q - 1) / s; };
        n = Vec3{g(p.x, h.x), g(p.y, h.y), g(p.z, h.z)};
        n = length(n) > 1e-8f ? normalized(n) : Vec3{0, sv < 0 ? -1.f : 1.f, 0};
        return c + p;
    };
    for (uint32_t i = 0; i < rings; ++i)
        for (uint32_t k = 0; k < around; ++k) {
            Vec3 n0, n1, n2, n3;
            const Vec3 a = at(i, k, n0), b = at(i, k + 1, n1), d = at(i + 1, k, n3), cc = at(i + 1, k + 1, n2);
            if (i != 0)
                tri(m, a, b, cc, n0, n1, n2, colour);
            if (i + 1 != rings)
                tri(m, a, cc, d, n0, n2, n3, colour);
        }
}
} // namespace detail

inline mesh_model::Mesh make() {
    using detail::capsule;
    mesh_model::Mesh m;
    const Vec3 deep = green * .62f, bright = green;
    // the back of the hand: knuckles at z 0, the wrist at z .30
    detail::roundBox(m, {0, .02f, .15f}, {.20f, .075f, .17f}, deep);
    // four fingers curled under: the knuckle, down the front, back under the palm, the tip tucked
    for (int k = 0; k < 4; ++k) {
        const float x = .15f - .10f * k, r = k == 3 ? .043f : .05f, d = k == 0 || k == 3 ? .015f : 0.f;
        const Vec3 knuckle{x, .03f, -.02f + d}, mid{x, -.10f, -.06f + d}, low{x, -.15f, .05f + d}, tip{x, -.08f, .10f + d};
        capsule(m, knuckle, mid, r, bright);
        capsule(m, mid, low, r * .92f, bright);
        capsule(m, low, tip, r * .82f, bright);
    }
    // the thumb (a right fist palm down: on the left, -x) folded across the middle fingers
    capsule(m, {-.19f, -.03f, .20f}, {-.17f, -.12f, .04f}, .055f, bright);
    capsule(m, {-.17f, -.12f, .04f}, {-.02f, -.15f, -.05f}, .046f, bright);
    // the wrist and the start of the forearm
    capsule(m, {0, .0f, .30f}, {0, .0f, .48f}, .12f, deep, 12, 3);
    m.low = m.high = m.corners.front();
    for (const Vec3& p : m.corners) {
        m.low = {std::min(m.low.x, p.x), std::min(m.low.y, p.y), std::min(m.low.z, p.z)};
        m.high = {std::max(m.high.x, p.x), std::max(m.high.y, p.y), std::max(m.high.z, p.z)};
    }
    m.sourceTriangles = m.triangles();
    return m;
}
// The one fist, made on first use (thread-safe).
inline const mesh_model::Mesh& mesh() {
    static const mesh_model::Mesh m = make();
    return m;
}
} // namespace spidy::construct_fist
