#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
// Made by Agent Mulder.
//
// 0.0.74 test 2 CAR PAINT (owner: "the 1989 looks untextured"). CONFIRMED why: the 1989 model (Dreamloft OBJ)
// ships no textures and a near-black Kd; the mod's model pass lit every corner diffuse only
// (.32 + .68 |n.l|), metallic 0, so a 0.03 black came out a flat dark grey with no highlight or reflection.
// The converter now writes satin black (base .03, metallic .40, roughness .42; the trim and the tyres their
// own), and the overlay lights a painted (untextured, metallic) triangle's corners with a satin term:
//   diffuse  base (.32 + .68 |n.l|) (1 - .6 metal)
//   specular Blinn, exponent 2 / r^4 - 2, normalised (e + 8) / 8pi, Schlick F0 = mix(.04, base, metal)
//   sky      reflect(-v, n).y blends a street-dark ground into a night sky, Schlick at n.v, damped by r
// Textured triangles (ODAMI, FOREVER 2, the cockpit) and vertex-coloured ones (FOREVER) are not painted.
// CAR PAINT OFF: every triangle the 0.0.74 test 1 diffuse light.
#include "spidy/math.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>

namespace spidy::car_paint {

// The least metallic a triangle's material must say to be painted (the converter's satin black is .4,
// its trim .35, its tyres .2).
inline constexpr float paintFrom = .15f;

inline std::atomic<bool>& switchOn() {
    static std::atomic<bool> on{true};
    return on;
}
inline void setEnabled(bool on) { switchOn().store(on, std::memory_order_relaxed); }
inline bool enabled() { return switchOn().load(std::memory_order_relaxed); }

// metal / rough from a Mesh::paint byte pair (0..255 each).
inline float unpack(uint8_t b) { return static_cast<float>(b) / 255.f; }

// One corner's colour: `base` (linear), unit normal n (either winding), unit light l, unit toward-eye v and
// unit world up `up`, all in one frame (the model's or the world's). Non-finite input: the diffuse-only colour.
// Cheap enough for 300,000 corners a frame: no pow but the lobe's, and that only where it is seen.
inline Vec3 shade(Vec3 base, Vec3 n, Vec3 l, Vec3 v, Vec3 up, float metal, float rough) {
    const float d = std::abs(dot(n, l));
    const Vec3 diffuseOnly = base * (.32f + .68f * d);
    if (!(std::isfinite(metal) && std::isfinite(rough) && std::isfinite(v.x + v.y + v.z) && std::isfinite(up.x + up.y + up.z)))
        return diffuseOnly;
    metal = std::clamp(metal, 0.f, 1.f);
    rough = std::clamp(rough, .08f, 1.f);
    if (dot(n, v) < 0) // the side he sees
        n = n * -1.f;
    const float nv = std::clamp(dot(n, v), 0.f, 1.f), nl = std::max(dot(n, l), 0.f);
    const Vec3 f0 = Vec3{.04f, .04f, .04f} * (1 - metal) + base * metal;
    Vec3 c = base * (.32f + .68f * d) * (1 - .6f * metal);
    const Vec3 hv = l + v;
    const float hl = spidy::length(hv);
    if (nl > 0 && hl > 1e-4f) {
        const Vec3 h = hv * (1.f / hl);
        const float r2 = rough * rough;
        const float e = std::min(2.f / (r2 * r2) - 2.f, 2000.f);
        const float nh = std::clamp(dot(n, h), 0.f, 1.f);
        if (nh > 0 && e * (1 - nh) < 6.f) { // else under .25 % of the peak: nothing seen
            const float vh = std::clamp(dot(v, h), 0.f, 1.f), q = 1 - vh, q2 = q * q, fr = q2 * q2 * q;
            const Vec3 F = f0 + (Vec3{1, 1, 1} - f0) * fr;
            const float lobe = std::exp(e * std::log(nh)) * (e + 8.f) * (1.f / 25.1327f) * nl;
            c = c + F * (1.6f * lobe);
        }
    }
    const Vec3 rv = n * (2 * dot(n, v)) - v; // reflect(-v, n)
    const float sky = std::clamp((dot(rv, up) + .1f) * 2.f, 0.f, 1.f), s = sky * sky * (3 - 2 * sky);
    const Vec3 env = Vec3{.035f, .035f, .04f} * (1 - s) + Vec3{.30f, .33f, .38f} * s;
    const float g = 1 - nv, g2 = g * g, grazing = g2 * g2 * g;
    const Vec3 Fe = f0 + (Vec3{1, 1, 1} * std::max(1 - rough, .04f) - f0) * grazing;
    c = c + Vec3{env.x * Fe.x, env.y * Fe.y, env.z * Fe.z} * (1 - .5f * rough);
    c = {std::min(c.x, 1.5f), std::min(c.y, 1.5f), std::min(c.z, 1.5f)};
    if (!std::isfinite(c.x + c.y + c.z))
        return diffuseOnly;
    return c;
}

// The same light for a big model (van_fps): what one material keeps (Material), what one triangle keeps (its
// toward-eye and half vector: Facing), then a few dot products a corner. Matches shade() to float rounding when
// v is the corner's own toward-eye.
struct Material {
    Vec3 base{}, f0{}, diffuse{}; // diffuse = base (1 - .6 metal)
    float e = 0, norm = 0, roughKeep = 1, edge = .04f;
    uint16_t key = 0xffff;
    bool ok = false;
};
inline void material(Material& m, Vec3 base, uint8_t metalByte, uint8_t roughByte) {
    const uint16_t key = static_cast<uint16_t>(metalByte << 8 | roughByte);
    if (m.ok && m.key == key && m.base.x == base.x && m.base.y == base.y && m.base.z == base.z)
        return;
    const float metal = std::clamp(unpack(metalByte), 0.f, 1.f), rough = std::clamp(unpack(roughByte), .08f, 1.f);
    const float r2 = rough * rough;
    m.base = base;
    m.f0 = Vec3{.04f, .04f, .04f} * (1 - metal) + base * metal;
    m.diffuse = base * (1 - .6f * metal);
    m.e = std::min(2.f / (r2 * r2) - 2.f, 2000.f);
    m.norm = 1.6f * (m.e + 8.f) * (1.f / 25.1327f);
    m.roughKeep = 1 - .5f * rough;
    m.edge = std::max(1 - rough, .04f);
    m.key = key;
    m.ok = std::isfinite(m.f0.x + m.f0.y + m.f0.z + m.e + m.norm);
}
struct Facing {
    Vec3 v{}, h{};
    Vec3 F{}; // the specular Fresnel at v.h
    float vUp = 0;
    bool lit = false, ok = false;
};
inline Facing facing(const Material& m, Vec3 l, Vec3 v, Vec3 up) {
    Facing f;
    if (!m.ok || !std::isfinite(v.x + v.y + v.z) || !(spidy::length(v) > .5f))
        return f;
    f.v = v;
    f.vUp = dot(v, up);
    const Vec3 hv = l + v;
    const float hl = spidy::length(hv);
    if (hl > 1e-4f) {
        f.h = hv * (1.f / hl);
        const float q = 1 - std::clamp(dot(v, f.h), 0.f, 1.f), q2 = q * q;
        f.F = m.f0 + (Vec3{1, 1, 1} - m.f0) * (q2 * q2 * q);
        f.lit = true;
    }
    f.ok = true;
    return f;
}
// Branch free (a car's corners come in any order): the lobe nh^e as (1 - e (1 - nh) / 16)^16, within 1 % of
// the peak's height of the true power.
inline Vec3 corner(const Material& m, const Facing& f, Vec3 n, Vec3 l, Vec3 up) {
    const float d = std::abs(dot(n, l));
    if (!f.ok)
        return m.base * (.32f + .68f * d);
    const float raw = dot(n, f.v), side = raw < 0 ? -1.f : 1.f;
    const float nv = std::min(std::abs(raw), 1.f);
    const float nl = std::max(side * dot(n, l), 0.f), nh = std::clamp(side * dot(n, f.h), 0.f, 1.f);
    float p = 1 - m.e * (1 - nh) * (1.f / 16);
    p = p > .1f ? p : 0.f; // under 1e-16 after: no denormals
    p *= p, p *= p, p *= p, p *= p;
    const float lobe = f.lit ? p * m.norm * nl : 0.f;
    Vec3 c = m.diffuse * (.32f + .68f * d) + f.F * lobe;
    const float ry = 2 * nv * side * dot(n, up) - f.vUp; // reflect(-v, n) . up
    const float sky = std::clamp((ry + .1f) * 2.f, 0.f, 1.f), s = sky * sky * (3 - 2 * sky);
    const Vec3 env = Vec3{.035f, .035f, .04f} * (1 - s) + Vec3{.30f, .33f, .38f} * s;
    const float g = 1 - nv, g2 = g * g, grazing = g2 * g2 * g;
    const Vec3 Fe = m.f0 + (Vec3{m.edge, m.edge, m.edge} - m.f0) * grazing;
    c = c + Vec3{env.x * Fe.x, env.y * Fe.y, env.z * Fe.z} * m.roughKeep;
    return {std::min(c.x, 1.5f), std::min(c.y, 1.5f), std::min(c.z, 1.5f)};
}

} // namespace spidy::car_paint
