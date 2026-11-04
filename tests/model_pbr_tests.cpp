// HERO VR 0.0.78 PBR MATERIALS tests (include/spidy/model_pbr.hpp, model_pbr_mesh.hpp).
// 1. The packing: octahedral normals and tangents, the maps word, the tint and params bytes.
// 2. The tangents made where a file gave none (T = dP/du, B = -dP/dv), mirrored uvs signed.
// 3. The mirror: a reflected corner's frame and hand, its sun reflected.
// 4. The shading: a flat, metal-0 corner is diffuse sky + sun as cut10; a normal map tilts it; roughness narrows
//    the highlight; metal tints it; occlusion darkens only the sky; the sun shadow takes only the sun.
// 5. The eye from a view-projection; the light constants (off outside the game's light).
// 6. The mesh glue: the maps and HQ layers only once the renderer bound them.
#include "spidy/model_pbr_mesh.hpp"

#include <cmath>
#include <cstddef>
#include <cstdio>
#include <vector>

using namespace spidy;
namespace mp = spidy::model_pbr;

namespace {
int failures = 0;
void check(bool ok, const char* what) {
    std::printf("%s: %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok)
        ++failures;
}
bool near(Vec3 a, Vec3 b, float e) { return std::abs(a.x - b.x) < e && std::abs(a.y - b.y) < e && std::abs(a.z - b.z) < e; }
mp::LightConstants noonLight() {
    world_light::Light l;
    l.world = true;
    l.toSun = normalized(Vec3{.3f, .8f, .4f});
    l.sun = {2.4f, 2.2f, 1.9f};
    l.skyUp = {.35f, .42f, .55f};
    l.skyDown = {.12f, .11f, .10f};
    return mp::constants(l, true);
}
} // namespace

int main() {
    // 1. packing
    {
        float worst = 0;
        for (int i = 0; i < 2000; ++i) {
            const float a = i * 2.39996f, z = 1 - 2 * (i + .5f) / 2000;
            const Vec3 n{std::sqrt(1 - z * z) * std::cos(a), std::sqrt(1 - z * z) * std::sin(a), z};
            worst = std::max(worst, length(mp::octUnpack(mp::octPack(n)) - n));
        }
        check(worst < 1e-4f, "octahedral: 2000 directions back within 1e-4");
        check(mp::octPack(Vec3{}) == 0, "octahedral: a zero vector packs to 0");
        mp::Material m;
        m.normalLayer = 7, m.ormLayer = 300, m.occlusion = true;
        const uint32_t w = mp::packMaps(m, false, true, true);
        check((w & mp::layerMask) == 7 && ((w >> mp::ormShift) & mp::layerMask) == 300, "maps: the normal and ORM layers");
        check((w & mp::flagOn) && (w & mp::flagNormal) && (w & mp::flagOrm) && (w & mp::flagOcclusion) && (w & mp::flagTangent) &&
                  (w & mp::flagFlip) && !(w & mp::flagNormalHq) && !(w & mp::flagMirrored),
              "maps: the flags");
        m.hqNormalLayer = 3;
        const uint32_t h = mp::packMaps(m, true, false, false);
        check((h & mp::layerMask) == 3 && (h & mp::flagNormalHq) && !(h & mp::flagTangent), "maps: the HQ normal layer wins when bound");
        check((mp::packMaps(m, false, false, false) & mp::layerMask) == 7, "maps: the standard normal layer without HQ");
        check(near(mp::unpackTint(mp::packTint({.25f, .5f, 1.f})), {.25f, .5f, 1.f}, .01f) && (mp::packTint({1, 1, 1}) >> 24) == mp::noHqLayer,
              "tint: sqrt-coded, no HQ base layer");
        mp::Material r;
        r.roughness = .4f, r.metallic = 1.f, r.normalScale = 1.f, r.occlusionStrength = .5f;
        const uint32_t p = mp::packParams(r);
        check(std::abs(mp::byteOf(p, 0) - .4f) < .003f && mp::byteOf(p, 1) == 1.f && std::abs(mp::byteOf(p, 2) - .5f) < .003f &&
                  std::abs(mp::byteOf(p, 3) - .5f) < .003f,
              "params: roughness, metal, normal scale / 2, occlusion strength");
        const Surface s = mp::surface(mp::Material{}, {1, 1, 1}, {0, 1, 0}, {}, 1, false);
        check((s.maps & mp::flagOn) && !(s.maps & (mp::flagNormal | mp::flagOrm | mp::flagTangent)) && s.tangent == 0,
              "surface: a model without maps is PBR at its factors, its frame from the shader");
        check(sizeof(Vertex) == 64 && offsetof(Vertex, surface) == 44, "vertex: 64 bytes, the Surface at 44");
    }
    // 2. tangents
    {
        // a quad in the xy plane facing +z, u along +x, v (glTF: down the image) along -y
        const std::vector<Vec3> c{{0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 0, 0}, {1, 1, 0}, {0, 1, 0}};
        const std::vector<Vec3> n(6, Vec3{0, 0, 1});
        const std::vector<float> uv{0, 1, 1, 1, 1, 0, 0, 1, 1, 0, 0, 0};
        std::vector<float> t;
        mp::generateTangents(c, n, uv, t);
        bool ok = t.size() == 24;
        for (size_t k = 0; ok && k < 6; ++k)
            ok = near({t[k * 4], t[k * 4 + 1], t[k * 4 + 2]}, {1, 0, 0}, 1e-5f) && t[k * 4 + 3] == 1.f;
        check(ok, "tangents: T = +x along u, w +1 (B = cross(N, T) = +y, the image's top)");
        std::vector<float> mirrored = uv;
        for (size_t k = 0; k < 6; ++k)
            mirrored[k * 2] = 1 - mirrored[k * 2];
        mp::generateTangents(c, n, mirrored, t);
        ok = t.size() == 24;
        for (size_t k = 0; ok && k < 6; ++k)
            ok = near({t[k * 4], t[k * 4 + 1], t[k * 4 + 2]}, {-1, 0, 0}, 1e-5f) && t[k * 4 + 3] == -1.f;
        check(ok, "tangents: mirrored u gives T = -x, w -1 (B still +y)");
        std::vector<float> flat(12, .5f);
        mp::generateTangents(c, n, flat, t);
        check(t.size() == 24 && t[3] == 0 && t[23] == 0, "tangents: degenerate uvs leave none (the shader's frame)");
    }
    // 3. the mirror
    {
        mp::Material m;
        Surface s = mp::surface(m, {1, 1, 1}, normalized(Vec3{1, 1, 0}), {0, 0, 1}, 1, false);
        mp::reflect(s, {1, 0, 0});
        check(near(mp::octUnpack(s.normal), normalized(Vec3{-1, 1, 0}), 1e-4f) && near(mp::octUnpack(s.tangent), {0, 0, 1}, 1e-4f),
              "mirror: the normal reflected, a tangent in the plane kept");
        check((s.maps & mp::flagFlip) && (s.maps & mp::flagMirrored), "mirror: the hand flipped, marked mirrored");
        Surface off{};
        mp::reflect(off, {1, 0, 0});
        check(off.maps == 0 && off.normal == 0, "mirror: a non-PBR corner untouched");
        check(mp::handedness({1, 0, 0}, {0, 1, 0}, {0, 0, 1}) == 1 && mp::handedness({-1, 0, 0}, {0, 1, 0}, {0, 0, 1}) == -1,
              "handedness: a reflection's axes negative");
    }
    // 4. shading
    {
        const mp::LightConstants c = noonLight();
        const Vec3 L{c.toSun[0], c.toSun[1], c.toSun[2]};
        mp::Pixel p;
        p.albedo = {.5f, .4f, .3f};
        p.n = {0, 1, 0};
        p.toEye = normalized(Vec3{0, .3f, 1});
        const mp::Lit flat = mp::shade(c, p);
        const Vec3 want = world_light::mul(p.albedo, mp::skyAt(c, 1) + Vec3{c.sun[0], c.sun[1], c.sun[2]} * L.y);
        check(near(flat.diffuse, want, 1e-5f), "shade: flat metal-0 diffuse = albedo (sky + sun n.l)");
        check(flat.specular.x >= 0 && flat.environment.x > 0 && flat.rgb.x >= flat.diffuse.x - 1e-6f, "shade: a dielectric's sheen added");
        mp::Pixel tilted = p;
        tilted.t = {1, 0, 0};
        tilted.mapped = normalized(Vec3{.5f, 0, .8f}); // toward +x: toward the sun's x
        mp::Pixel away = tilted;
        away.mapped = normalized(Vec3{-.5f, 0, .8f});
        check(mp::shade(c, tilted).diffuse.x > mp::shade(c, away).diffuse.x + .02f, "shade: the normal map tilts the light");
        mp::Pixel noFrame = tilted;
        noFrame.t = {};
        check(near(mp::shade(c, noFrame).diffuse, flat.diffuse, 1e-6f), "shade: a map without a frame is flat (never garbage)");
        // the mirror direction of the sun about +y: the highlight's peak
        mp::Pixel shiny = p, rough = p;
        shiny.toEye = rough.toEye = normalized(Vec3{-L.x, L.y, -L.z});
        shiny.roughness = .2f;
        check(mp::shade(c, shiny).specular.x > 4 * mp::shade(c, rough).specular.x, "shade: smooth peaks far above rough at the mirror angle");
        mp::Pixel metal = shiny;
        metal.metallic = 1;
        const mp::Lit ml = mp::shade(c, metal);
        check(ml.diffuse.x == 0 && ml.specular.x > ml.specular.z, "shade: metal has no diffuse, its highlight tinted by albedo");
        mp::Pixel occ = p;
        occ.occlusion = true, occ.orm = {0, 1, 1};
        const mp::Lit ol = mp::shade(c, occ);
        check(near(ol.diffuse, world_light::mul(p.albedo, Vec3{c.sun[0], c.sun[1], c.sun[2]} * L.y), 1e-5f), "shade: occlusion takes the sky only");
        mp::Pixel shaded = p;
        shaded.sunVisible = 0;
        check(near(mp::shade(c, shaded).diffuse, world_light::mul(p.albedo, mp::skyAt(c, 1)), 1e-5f) && mp::shade(c, shaded).specular.x == 0,
              "shade: the sun shadow takes the sun and its highlight only");
        mp::Pixel ormRough = shiny;
        ormRough.orm = {1, 1, 1};
        ormRough.roughness = 1, ormRough.orm.y = .2f;
        check(std::abs(mp::shade(c, ormRough).specular.x - mp::shade(c, shiny).specular.x) < 1e-5f, "shade: ORM G scales the roughness");
        bool finiteAll = true;
        for (int i = 0; i < 200; ++i) {
            mp::Pixel q = p;
            q.roughness = (i % 10) / 9.f;
            q.metallic = (i % 3) / 2.f;
            q.toEye = normalized(Vec3{std::cos(i * .7f), std::sin(i * .3f), std::cos(i * 1.3f)});
            const mp::Lit r = mp::shade(c, q);
            finiteAll = finiteAll && std::isfinite(r.rgb.x + r.rgb.y + r.rgb.z) && r.rgb.x >= 0;
        }
        check(finiteAll, "shade: finite and non-negative everywhere (grazing, back-facing eyes)");
    }
    // 5. the eye and the constants
    {
        // view: eye at (1, 2, 3) looking down -z; projection: x, y kept, w = -z (row-major, clip = vp p)
        const float vp[16] = {1, 0, 0, -1, 0, 1, 0, -2, 0, 0, .01f, .1f, 0, 0, -1, 3};
        float e[3];
        check(mp::eyeFromViewProjection(vp, e) && std::abs(e[0] - 1) < 1e-5f && std::abs(e[1] - 2) < 1e-5f && std::abs(e[2] - 3) < 1e-5f,
              "eye: solved from the view-projection");
        world_light::Light l;
        l.world = false;
        check(mp::constants(l, true).toSun[3] == 0, "constants: off in the studio light");
        l.world = true;
        check(mp::constants(l, false).toSun[3] == 0 && mp::constants(l, true).toSun[3] == 1, "constants: on only with model_pbr on");
    }
    // 6. the mesh glue
    {
        mesh_model::Mesh m;
        m.corners.assign(3, Vec3{});
        m.colors.assign(1, Vec3{1, 1, 1});
        m.image.assign(1, 0);
        m.images.resize(1);
        m.images[0].layer = 4, m.images[0].hqLayer = 2;
        mesh_model::Mesh::Surface s;
        s.normalImage = 0, s.mrImage = 0, s.occlusionImage = 0;
        s.normalLayer = 5, s.ormLayer = 6, s.hqNormalLayer = 1, s.roughness = .5f, s.metallic = .25f;
        m.surfaces.push_back(s);
        m.surface.assign(1, 0);
        mp::setMapsReady(false), mp::setHqReady(false);
        mp::Material a = mp::material(m, 0);
        check(a.normalLayer < 0 && a.ormLayer < 0 && a.hqBaseLayer < 0 && a.roughness == .5f && a.metallic == .25f,
              "glue: no maps before the renderer bound them (the factors kept)");
        mp::setMapsReady(true);
        a = mp::material(m, 0);
        check(a.normalLayer == 5 && a.ormLayer == 6 && a.occlusion && a.hqNormalLayer < 0 && a.hqBaseLayer < 0, "glue: the maps once bound");
        mp::setHqReady(true);
        a = mp::material(m, 0);
        check(a.hqNormalLayer == 1 && a.hqBaseLayer == 2, "glue: the HQ layers once bound");
        const Surface t = mp::corner(a, {1, 1, 1}, {0, 0, 1}, {2, 0, 0}, 1, true);
        check((t.maps & mp::flagTangent) && (t.maps & mp::flagFlip) && (t.tint >> 24) == 2, "glue: a turned corner's hand flipped, the HQ base");
        mp::setMapsReady(false), mp::setHqReady(false);
        mesh_model::Mesh plain;
        plain.corners.assign(3, Vec3{});
        plain.colors.assign(1, Vec3{1, 1, 1});
        const mp::Material d = mp::material(plain, 0);
        check(d.roughness == 1 && d.metallic == 0 && d.normalLayer < 0, "glue: a model without surfaces: rough dielectric");
    }
    std::printf("%s (%d failures)\n", failures ? "FAILED" : "PASSED", failures);
    return failures ? 1 : 0;
}
