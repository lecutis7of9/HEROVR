#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
// Made by Agent Mulder.
//
// HERO VR 0.0.78 PBR MATERIALS (owner: "not flat textures, it needs to look realistic in game"). Until now the
// models the mod draws itself (the suits, the arsenal, the hammers, the shield, the van, the Batmobiles, the bikes)
// were lit per corner on the CPU (world_light.hpp) and drawn as that colour times the base-colour texture: no
// surface detail finer than a triangle, no highlight but car paint's, no reflection. Now, per pixel:
//   - tangent-space normal maps (glTF normalTexture; the tangents from glTF TANGENT, else made here, MikkTSpace-like:
//     T = dP/du per triangle, angle-weighted over the corners that share a place, normal, uv and handedness;
//     B = w cross(N, T) points to -dP/dv, the image's top, as Blender and glTF bake them);
//   - ORM maps (glTF occlusionTexture R, metallicRoughnessTexture G roughness, B metal), packed into one layer;
//   - GGX specular (Smith height-correlated visibility, Schlick Fresnel) from the game's sun, and an environment
//     reflection of the game's sky (the hemisphere world_light derives: skyUp over skyDown, blurred toward the
//     normal with roughness; the analytic split-sum BRDF);
//   - all of it in the game's own frame light (world_light::Light) and under the GPU sun shadow (the occluded
//     pipeline's march): the diffuse of a flat-mapped, metal-0 surface is cut10's corner light to the bit.
// The fallback: a vertex whose Surface is all 0 (everything not a PBR model corner, STUDIO light, model_pbr OFF, a
// model without maps that is car paint or cockpit glow) is drawn exactly as 0.0.78 cut10.
// CONFIRMED: the packing, the tangent frame, the eye solve and the shading math (tests/model_pbr_tests.cpp; the same
// math as the HLSL in d3d12_renderer.cpp). CANDIDATE: how it reads in the headset (offline renders only so far).
#include "spidy/math.hpp"
#include "spidy/vertex.hpp"
#include "spidy/world_light.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <unordered_map>
#include <vector>

namespace spidy::model_pbr {

// Surface::maps
inline constexpr uint32_t layerMask = 0x3ff;      // a layer field (10 bits: the arrays hold at most 1023)
inline constexpr uint32_t ormShift = 10;
inline constexpr uint32_t flagOn = 1u << 20;      // a PBR corner (else the 0.0.78 look)
inline constexpr uint32_t flagFlip = 1u << 21;    // tangent handedness negative (B = -cross(N, T))
inline constexpr uint32_t flagNormalHq = 1u << 22; // the normal layer is in the HQ linear array
inline constexpr uint32_t flagNormal = 1u << 23;  // has a normal map
inline constexpr uint32_t flagOrm = 1u << 24;     // has an ORM layer
inline constexpr uint32_t flagOcclusion = 1u << 25; // the ORM layer's R is occlusion (else 1)
inline constexpr uint32_t flagMirrored = 1u << 26; // reflected in the VR mirror (its sun reflected too)
inline constexpr uint32_t flagTangent = 1u << 27; // Surface::tangent holds one (else the shader's own frame)
inline constexpr uint32_t noHqLayer = 255;

// The switches (live.txt model_pbr, model_textures_hq; game_xr sets them).
inline std::atomic<bool>& switchOn() {
    static std::atomic<bool> on{true};
    return on;
}
inline void setEnabled(bool on) { switchOn().store(on, std::memory_order_relaxed); }
inline bool enabled() { return switchOn().load(std::memory_order_relaxed); }
// The HQ (2048 px) layers are in the overlay's arrays (set once the overlay took them): the emitters may use them.
inline std::atomic<bool>& hqReadyFlag() {
    static std::atomic<bool> on{false};
    return on;
}
inline void setHqReady(bool on) { hqReadyFlag().store(on, std::memory_order_relaxed); }
inline bool hqReady() { return hqReadyFlag().load(std::memory_order_relaxed); }
// The standard linear array (normal / ORM maps) is in the overlay: maps may be sampled.
inline std::atomic<bool>& mapsReadyFlag() {
    static std::atomic<bool> on{false};
    return on;
}
inline void setMapsReady(bool on) { mapsReadyFlag().store(on, std::memory_order_relaxed); }
inline bool mapsReady() { return mapsReadyFlag().load(std::memory_order_relaxed); }

// ---- packing -------------------------------------------------------------------------------------
inline int16_t snorm16(float v) {
    return static_cast<int16_t>(std::lround(std::clamp(std::isfinite(v) ? v : 0.f, -1.f, 1.f) * 32767.f));
}
inline float unsnorm16(int16_t v) { return std::max(static_cast<float>(v) / 32767.f, -1.f); }
// Octahedral: a unit vector as two snorm16 (x low, y high); 0 for a zero vector (decodes to +z, never used so).
inline uint32_t octPack(Vec3 n) {
    const float s = std::abs(n.x) + std::abs(n.y) + std::abs(n.z);
    if (!(s > 1e-12f) || !std::isfinite(s))
        return 0;
    float x = n.x / s, y = n.y / s;
    if (n.z < 0) {
        const float ox = (1 - std::abs(y)) * (x >= 0 ? 1.f : -1.f), oy = (1 - std::abs(x)) * (y >= 0 ? 1.f : -1.f);
        x = ox, y = oy;
    }
    return static_cast<uint16_t>(snorm16(x)) | static_cast<uint32_t>(static_cast<uint16_t>(snorm16(y))) << 16;
}
inline Vec3 octUnpack(uint32_t p) {
    const float x = unsnorm16(static_cast<int16_t>(p & 0xffff)), y = unsnorm16(static_cast<int16_t>(p >> 16));
    Vec3 v{x, y, 1 - std::abs(x) - std::abs(y)};
    if (v.z < 0) {
        const float ox = (1 - std::abs(y)) * (x >= 0 ? 1.f : -1.f), oy = (1 - std::abs(x)) * (y >= 0 ? 1.f : -1.f);
        v.x = ox, v.y = oy;
    }
    return normalized(v);
}
inline uint32_t unorm8(float v) {
    return static_cast<uint32_t>(std::lround(std::clamp(std::isfinite(v) ? v : 0.f, 0.f, 1.f) * 255.f));
}
inline uint32_t pack4(float a, float b, float c, float d) { return unorm8(a) | unorm8(b) << 8 | unorm8(c) << 16 | unorm8(d) << 24; }
inline float byteOf(uint32_t p, int k) { return static_cast<float>((p >> (8 * k)) & 0xff) / 255.f; }
// The albedo factor sqrt-coded (8 bits keep the darks), the HQ base layer in a.
inline uint32_t packTint(Vec3 t, uint32_t hqBase = noHqLayer) {
    return unorm8(std::sqrt(std::max(t.x, 0.f))) | unorm8(std::sqrt(std::max(t.y, 0.f))) << 8 |
           unorm8(std::sqrt(std::max(t.z, 0.f))) << 16 | std::min<uint32_t>(hqBase, noHqLayer) << 24;
}
inline Vec3 unpackTint(uint32_t p) {
    const float r = byteOf(p, 0), g = byteOf(p, 1), b = byteOf(p, 2);
    return {r * r, g * g, b * b};
}

// One triangle's material as the emitters give it (mesh_model::Mesh::Surface with its layers resolved).
struct Material {
    float roughness = 1, metallic = 0, normalScale = 1, occlusionStrength = 1;
    int32_t normalLayer = -1, ormLayer = -1, hqNormalLayer = -1, hqBaseLayer = -1;
    bool occlusion = false; // the ORM layer's R is a real occlusion map
};
// The maps and params words for a corner (maps: flagOn always; tangent: whether a tangent is given, its sign).
inline uint32_t packMaps(const Material& m, bool hq, bool tangent, bool flip) {
    uint32_t w = flagOn;
    if (hq && m.hqNormalLayer >= 0 && static_cast<uint32_t>(m.hqNormalLayer) <= layerMask)
        w |= static_cast<uint32_t>(m.hqNormalLayer) | flagNormal | flagNormalHq;
    else if (m.normalLayer >= 0 && static_cast<uint32_t>(m.normalLayer) <= layerMask)
        w |= static_cast<uint32_t>(m.normalLayer) | flagNormal;
    if (m.ormLayer >= 0 && static_cast<uint32_t>(m.ormLayer) <= layerMask)
        w |= static_cast<uint32_t>(m.ormLayer) << ormShift | flagOrm | (m.occlusion ? flagOcclusion : 0u);
    if (tangent)
        w |= flagTangent | (flip ? flagFlip : 0u);
    return w;
}
inline uint32_t packParams(const Material& m) {
    return pack4(m.roughness, m.metallic, std::clamp(m.normalScale, 0.f, 2.f) * .5f, m.occlusionStrength);
}
// A model corner's whole Surface. n, t: world unit normal and tangent (t zero: none); w: the tangent's sign.
inline Surface surface(const Material& m, Vec3 tint, Vec3 n, Vec3 t, float w, bool hq) {
    Surface s;
    const bool tangent = length(t) > .5f;
    s.normal = octPack(n);
    s.tangent = tangent ? octPack(t) : 0u;
    s.maps = packMaps(m, hq, tangent, w < 0);
    const uint32_t base = hq && m.hqBaseLayer >= 0 && m.hqBaseLayer < static_cast<int32_t>(noHqLayer) ? static_cast<uint32_t>(m.hqBaseLayer) : noHqLayer;
    s.tint = packTint(tint, base);
    s.params = packParams(m);
    return s;
}
// The VR mirror: a reflected corner's normal and tangent reflected (plane normal `m`, unit), its handedness flipped.
inline void reflect(Surface& s, Vec3 m) {
    if (!(s.maps & flagOn))
        return;
    const auto r = [&](Vec3 d) { return d - m * (2 * dot(d, m)); };
    s.normal = octPack(r(octUnpack(s.normal)));
    if (s.maps & flagTangent) {
        s.tangent = octPack(r(octUnpack(s.tangent)));
        s.maps ^= flagFlip;
    }
    s.maps ^= flagMirrored;
}

// ---- the tangents ----------------------------------------------------------------------------------
// 4 a corner (xyz unit, w +-1; all 0 where the uvs give none): `corners`, `normals` (3 a triangle, unit),
// `uvs` (6 a triangle, glTF: v down the image). Corners that share a place, a normal, a uv and a handedness share
// one tangent (angle-weighted), as MikkTSpace welds them.
inline void generateTangents(const std::vector<Vec3>& corners, const std::vector<Vec3>& normals, const std::vector<float>& uvs,
                             std::vector<float>& out) {
    const size_t n = corners.size();
    out.assign(n * 4, 0.f);
    if (n % 3 || normals.size() != n || uvs.size() != n * 2)
        return out.clear();
    struct Acc {
        Vec3 t{}, b{};
    };
    struct Key {
        int32_t q[8];
        bool operator==(const Key& o) const { return std::memcmp(q, o.q, sizeof q) == 0; }
    };
    struct Hash {
        size_t operator()(const Key& k) const {
            uint64_t h = 1469598103934665603ull;
            for (int32_t v : k.q)
                h = (h ^ static_cast<uint32_t>(v)) * 1099511628211ull;
            return static_cast<size_t>(h);
        }
    };
    const auto quant = [](float v, float s) { return static_cast<int32_t>(std::lround(static_cast<double>(v) * s)); };
    std::vector<Vec3> faceT(n / 3), faceB(n / 3);
    std::vector<int8_t> faceSign(n / 3, 0);
    for (size_t f = 0; f < n / 3; ++f) {
        const Vec3 p0 = corners[f * 3], p1 = corners[f * 3 + 1], p2 = corners[f * 3 + 2];
        const float u0 = uvs[f * 6], v0 = uvs[f * 6 + 1], u1 = uvs[f * 6 + 2], v1 = uvs[f * 6 + 3], u2 = uvs[f * 6 + 4], v2 = uvs[f * 6 + 5];
        const Vec3 e1 = p1 - p0, e2 = p2 - p0;
        const float du1 = u1 - u0, dv1 = v1 - v0, du2 = u2 - u0, dv2 = v2 - v0;
        const float r = du1 * dv2 - du2 * dv1;
        if (!(std::abs(r) > 1e-12f) || !std::isfinite(r))
            continue;
        const Vec3 t = (e1 * dv2 - e2 * dv1) * (1.f / r), b = (e2 * du1 - e1 * du2) * (1.f / r);
        if (!finite(t) || !finite(b) || !(length(t) > 1e-12f))
            continue;
        faceT[f] = t;
        faceB[f] = b;
        // the handedness as the face says it (its own normal: the corners' may disagree at a crease)
        const Vec3 fn = cross(e1, e2);
        faceSign[f] = dot(cross(fn, t), b * -1.f) < 0 ? int8_t{-1} : int8_t{1};
    }
    std::unordered_map<Key, Acc, Hash> sums;
    sums.reserve(n);
    const auto keyOf = [&](size_t c) {
        const size_t f = c / 3;
        const Vec3 p = corners[c], nn = normals[c];
        return Key{{quant(p.x, 1e5f), quant(p.y, 1e5f), quant(p.z, 1e5f), quant(nn.x, 1e3f) * 4096 + quant(nn.y, 1e3f),
                    quant(nn.z, 1e3f), quant(uvs[c * 2], 1e5f), quant(uvs[c * 2 + 1], 1e5f), faceSign[f]}};
    };
    for (size_t c = 0; c < n; ++c) {
        const size_t f = c / 3;
        if (!faceSign[f])
            continue;
        const Vec3 p = corners[c], a = corners[f * 3 + (c + 1) % 3], b = corners[f * 3 + (c + 2) % 3];
        const Vec3 da = normalized(a - p), db = normalized(b - p);
        const float angle = std::acos(std::clamp(dot(da, db), -1.f, 1.f));
        // the face's tangent in this corner's plane (Gram-Schmidt on its normal), then weighted
        const Vec3 nn = normals[c];
        const Vec3 t = normalized(faceT[f] - nn * dot(nn, faceT[f])), bb = normalized(faceB[f] - nn * dot(nn, faceB[f]));
        Acc& s = sums[keyOf(c)];
        s.t += t * angle;
        s.b += bb * angle;
    }
    for (size_t c = 0; c < n; ++c) {
        const size_t f = c / 3;
        if (!faceSign[f])
            continue;
        const auto it = sums.find(keyOf(c));
        if (it == sums.end())
            continue;
        const Vec3 nn = normals[c];
        Vec3 t = it->second.t - nn * dot(nn, it->second.t);
        if (!(length(t) > 1e-8f))
            continue;
        t = normalized(t);
        const float w = dot(cross(nn, t), it->second.b * -1.f) < 0 ? -1.f : 1.f;
        out[c * 4] = t.x, out[c * 4 + 1] = t.y, out[c * 4 + 2] = t.z, out[c * 4 + 3] = w;
    }
}

// ---- the eye -----------------------------------------------------------------------------------------
// The eye's world position from its view-projection (row-major, clip = vp (p, 1)): the point whose clip x, y and w
// are 0 (rows 0, 1 and 3). False when they do not meet in one point.
inline bool eyeFromViewProjection(const float* vp, float out[3]) {
    const double a[3][3] = {{vp[0], vp[1], vp[2]}, {vp[4], vp[5], vp[6]}, {vp[12], vp[13], vp[14]}};
    const double b[3] = {-static_cast<double>(vp[3]), -static_cast<double>(vp[7]), -static_cast<double>(vp[15])};
    const double det = a[0][0] * (a[1][1] * a[2][2] - a[1][2] * a[2][1]) - a[0][1] * (a[1][0] * a[2][2] - a[1][2] * a[2][0]) +
                       a[0][2] * (a[1][0] * a[2][1] - a[1][1] * a[2][0]);
    if (!(std::abs(det) > 1e-18) || !std::isfinite(det))
        return false;
    for (int k = 0; k < 3; ++k) {
        double m[3][3];
        for (int r = 0; r < 3; ++r)
            for (int c = 0; c < 3; ++c)
                m[r][c] = c == k ? b[r] : a[r][c];
        const double d = m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1]) - m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0]) +
                         m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);
        out[k] = static_cast<float>(d / det);
        if (!std::isfinite(out[k]))
            return false;
    }
    return true;
}

// ---- the light constants (the pixel shader's b2, 24 floats a eye) ------------------------------------
struct LightConstants {
    float toSun[4]{0, 1, 0, 0};   // w: 1 PBR shading on
    float sun[4]{};               // the direct term at n.l = 1 (world_light::Light::sun); w: specular gain
    float skyUp[4]{};             // w: environment gain
    float skyDown[4]{};           // w: 1 the HQ arrays are bound
    float eye[4]{};               // this eye's world position (renderViews fills it)
    float mirror[4]{};            // the VR mirror's unit plane normal; w: 1 a mirror is drawn
};
static_assert(sizeof(LightConstants) == 24 * 4);
inline LightConstants constants(const world_light::Light& l, bool on) {
    LightConstants c;
    const bool lit = on && l.world && finite(l.toSun) && finite(l.sun) && finite(l.skyUp) && finite(l.skyDown);
    const Vec3 s = normalized(l.toSun);
    c.toSun[0] = s.x, c.toSun[1] = s.y, c.toSun[2] = s.z, c.toSun[3] = lit ? 1.f : 0.f;
    c.sun[0] = l.sun.x, c.sun[1] = l.sun.y, c.sun[2] = l.sun.z, c.sun[3] = 1;
    c.skyUp[0] = l.skyUp.x, c.skyUp[1] = l.skyUp.y, c.skyUp[2] = l.skyUp.z, c.skyUp[3] = 1;
    c.skyDown[0] = l.skyDown.x, c.skyDown[1] = l.skyDown.y, c.skyDown[2] = l.skyDown.z;
    return c;
}

// ---- the shading (the HLSL's math, line for line; tests and the offline renders use it) -----------------
struct Pixel {
    Vec3 albedo{1, 1, 1};    // tint x the base texel (linear)
    Vec3 n{0, 0, 1};         // the interpolated world normal (unit)
    Vec3 t{};                // the world tangent (zero: none)
    float w = 1;             // its sign
    Vec3 mapped{0, 0, 1};    // the normal map texel decoded to -1..1 (0 0 1: flat)
    Vec3 orm{1, 1, 1};       // the ORM texel (1 1 1: none)
    float roughness = 1, metallic = 0, normalScale = 1, occlusionStrength = 1;
    bool occlusion = false;
    Vec3 toEye{0, 0, 1};     // unit, toward the eye
    float sunVisible = 1;    // 1 - the sun shadow (1 - sun.w blocked)
};
struct Lit {
    Vec3 diffuse{}, specular{}, environment{}, rgb{};
};
inline Vec3 skyAt(const LightConstants& c, float up) {
    const float t = std::clamp(.5f + .5f * up, 0.f, 1.f);
    return Vec3{c.skyDown[0], c.skyDown[1], c.skyDown[2]} * (1 - t) + Vec3{c.skyUp[0], c.skyUp[1], c.skyUp[2]} * t;
}
inline float maxOf(Vec3 v) { return std::max({v.x, v.y, v.z}); }
inline Lit shade(const LightConstants& c, const Pixel& p) {
    Lit o;
    const Vec3 L{c.toSun[0], c.toSun[1], c.toSun[2]}, sun{c.sun[0], c.sun[1], c.sun[2]};
    const Vec3 N = normalized(p.n);
    Vec3 Nm = N;
    if (p.mapped.x != 0 || p.mapped.y != 0) {
        Vec3 T = p.t - N * dot(N, p.t);
        if (length(T) > 1e-6f) {
            T = normalized(T);
            const Vec3 B = cross(N, T) * (p.w < 0 ? -1.f : 1.f);
            const float s = p.normalScale;
            const Vec3 m = normalized(Vec3{p.mapped.x * s, p.mapped.y * s, std::max(p.mapped.z, 1e-3f)});
            Nm = normalized(T * m.x + B * m.y + N * m.z);
        }
    }
    const float occl = p.occlusion ? 1 + (p.orm.x - 1) * std::clamp(p.occlusionStrength, 0.f, 1.f) : 1.f;
    const float rough = std::clamp(p.roughness * p.orm.y, .045f, 1.f), metal = std::clamp(p.metallic * p.orm.z, 0.f, 1.f);
    const float geo = std::clamp(dot(N, L) * 4 + 1, 0.f, 1.f); // a mapped normal never lights the far side
    const float nl = std::max(dot(Nm, L), 0.f) * geo;
    const Vec3 V = normalized(p.toEye);
    const float nv = std::max(dot(Nm, V), 1e-4f);
    const Vec3 f0 = Vec3{.04f, .04f, .04f} * (1 - metal) + p.albedo * metal;
    o.diffuse = world_light::mul(p.albedo * (1 - metal), skyAt(c, Nm.y) * occl + sun * (nl * p.sunVisible));
    const Vec3 hv = L + V;
    if (nl > 0 && length(hv) > 1e-6f) {
        const Vec3 H = normalized(hv);
        const float nh = std::clamp(dot(Nm, H), 0.f, 1.f), vh = std::clamp(dot(V, H), 0.f, 1.f);
        const float a = rough * rough, a2 = a * a;
        const float dd = nh * nh * (a2 - 1) + 1;
        const float D = a2 / (3.14159265f * dd * dd);
        const float vis = .5f / std::max(nl * std::sqrt(nv * nv * (1 - a2) + a2) + nv * std::sqrt(nl * nl * (1 - a2) + a2), 1e-6f);
        const float q = 1 - vh, q5 = q * q * q * q * q;
        const Vec3 F = f0 + (Vec3{1, 1, 1} - f0) * q5;
        o.specular = world_light::mul(F, sun) * (3.14159265f * D * vis * nl * p.sunVisible * c.sun[3]);
    }
    // the sky in the reflection, blurred toward the normal as it roughens; the split sum's analytic fit
    const Vec3 R = Nm * (2 * dot(Nm, V)) - V;
    const Vec3 Rb = normalized(R + (Nm - R) * (rough * rough));
    const float r0x = rough * -1 + 1, r0y = rough * -.0275f + .0425f, r0z = rough * -.572f + 1.04f, r0w = rough * .022f - .04f;
    const float a004 = std::min(r0x * r0x, std::exp2(-9.28f * nv)) * r0x + r0y;
    const float A = a004 * -1.04f + r0z, Bf = a004 * 1.04f + r0w;
    const float spOcc = std::clamp(std::pow(nv + occl, std::exp2(-16.f * rough - 1)) - 1 + occl, 0.f, 1.f);
    o.environment = world_light::mul(skyAt(c, Rb.y), f0 * A + Vec3{Bf, Bf, Bf}) * (spOcc * c.skyUp[3]);
    // the highlights' soft shoulder (above .8: toward 1.6), the diffuse kept
    const Vec3 sp = o.specular + o.environment, all = o.diffuse + sp;
    const float m = maxOf(all), md = maxOf(o.diffuse);
    o.rgb = all;
    if (m > .8f && m > md + 1e-6f) {
        const float t = std::max(.8f + (m - .8f) / (1 + (m - .8f) / .8f), md);
        o.rgb = o.diffuse + sp * ((t - md) / (m - md));
    }
    return o;
}

} // namespace spidy::model_pbr
