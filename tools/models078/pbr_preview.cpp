// HERO VR 0.0.78 PBR MATERIALS: an OFFLINE preview of one model slot as the overlay would shade it, before
// (PBR off: the plain colour x texel path) and after (model_pbr::shade, the same math as the pixel shader),
// under sample lights given here (not the game's). A software rasteriser: perspective-correct, 2x2 samples,
// trilinear from the same texture arrays prepareTextures makes. Not run in the game or in CI.
//
// spidy_pbr_preview <before Models\> <after Models\> <cache dir\> <out dir\> [slot]
// Images are decoded through the cache (as model_dump): a missing <hash>.rgba leaves <hash>.enc to be decoded
// (tools/models078/pbr_preview_decode.py) and the run is repeated; pbr_preview_sheet.py makes the labelled sheets.
// Not a CMake target (an offline tool): g++ -std=c++20 -O2 -Iinclude tools/models078/pbr_preview.cpp
// src/sfx_quiet.cpp build/gate/libspidy_core.a -o pbr_preview
#include "spidy/hero007_visual.hpp"
#include "spidy/mesh_model.hpp"
#include "spidy/model_pbr.hpp"
#include "spidy/model_pbr_mesh.hpp"
#include "spidy/world_light.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <set>
#include <string>
#include <vector>

using namespace spidy;
namespace mm = spidy::mesh_model;
namespace mp = spidy::model_pbr;
namespace wl = spidy::world_light;

static std::string cacheDir;
static std::set<std::string> misses;

static std::string hashOf(const uint8_t* d, size_t n) {
    uint64_t h = 1469598103934665603ull;
    for (size_t i = 0; i < n; ++i)
        h = (h ^ d[i]) * 1099511628211ull;
    char s[17];
    std::snprintf(s, sizeof s, "%016llx", static_cast<unsigned long long>(h));
    return s;
}
static bool cacheDecoder(const uint8_t* data, size_t size, mm::Picture& out) {
    const std::string h = hashOf(data, size);
    std::ifstream f(cacheDir + h + ".rgba", std::ios::binary);
    if (!f) {
        std::ofstream e(cacheDir + h + ".enc", std::ios::binary);
        e.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size));
        misses.insert(h);
        return false;
    }
    uint32_t wh[2]{};
    f.read(reinterpret_cast<char*>(wh), 8);
    if (!f || !wh[0] || !wh[1] || wh[0] > 16384 || wh[1] > 16384)
        return false;
    out.width = wh[0];
    out.height = wh[1];
    out.rgba.resize(size_t{wh[0]} * wh[1] * 4);
    f.read(reinterpret_cast<char*>(out.rgba.data()), static_cast<std::streamsize>(out.rgba.size()));
    return static_cast<bool>(f);
}

static float toLinear(float c) { return c <= .04045f ? c / 12.92f : std::pow((c + .055f) / 1.055f, 2.4f); }
static float toSrgb(float c) {
    c = std::clamp(c, 0.f, 1.f);
    return c <= .0031308f ? c * 12.92f : 1.055f * std::pow(c, 1 / 2.4f) - .055f;
}

struct V4 {
    float x = 0, y = 0, z = 0, w = 0;
};
// One array (layers of mips, level 0 first): trilinear with the derivatives' level, wrapped like the sampler.
struct Array {
    const std::vector<std::vector<uint8_t>>* layers = nullptr;
    uint32_t size = 0, levels = 0;
    bool srgb = false;
    bool ok(uint32_t layer) const { return layers && layer < layers->size() && size; }
    V4 texel(const uint8_t* lv, uint32_t s, int x, int y) const {
        x = ((x % static_cast<int>(s)) + static_cast<int>(s)) % static_cast<int>(s);
        y = ((y % static_cast<int>(s)) + static_cast<int>(s)) % static_cast<int>(s);
        const uint8_t* p = lv + (size_t(y) * s + size_t(x)) * 4;
        V4 o{p[0] / 255.f, p[1] / 255.f, p[2] / 255.f, p[3] / 255.f};
        if (srgb)
            o = {toLinear(o.x), toLinear(o.y), toLinear(o.z), o.w};
        return o;
    }
    V4 level(uint32_t layer, uint32_t l, float u, float v) const {
        const auto& data = (*layers)[layer];
        size_t offset = 0;
        for (uint32_t k = 0; k < l; ++k) {
            const uint32_t s = std::max(size >> k, 1u);
            offset += size_t(s) * s * 4;
        }
        const uint32_t s = std::max(size >> l, 1u);
        if (offset + size_t(s) * s * 4 > data.size())
            return {1, 1, 1, 1};
        const uint8_t* lv = data.data() + offset;
        const float fx = u * s - .5f, fy = v * s - .5f;
        const int x0 = static_cast<int>(std::floor(fx)), y0 = static_cast<int>(std::floor(fy));
        const float ax = fx - x0, ay = fy - y0;
        const V4 a = texel(lv, s, x0, y0), b = texel(lv, s, x0 + 1, y0), c = texel(lv, s, x0, y0 + 1),
                 d = texel(lv, s, x0 + 1, y0 + 1);
        auto mix = [](V4 p, V4 q, float t) {
            return V4{p.x + (q.x - p.x) * t, p.y + (q.y - p.y) * t, p.z + (q.z - p.z) * t, p.w + (q.w - p.w) * t};
        };
        return mix(mix(a, b, ax), mix(c, d, ax), ay);
    }
    V4 sample(uint32_t layer, float u, float v, float lod) const {
        if (!ok(layer))
            return {1, 1, 1, 1};
        lod = std::clamp(lod, 0.f, static_cast<float>(levels - 1));
        const uint32_t l0 = static_cast<uint32_t>(lod), l1 = std::min(l0 + 1, levels - 1);
        const float t = lod - static_cast<float>(l0);
        const V4 a = level(layer, l0, u, v), b = level(layer, l1, u, v);
        return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t};
    }
};
static float lodOf(float gu, float gv, float hu, float hv, uint32_t size) {
    const float s = static_cast<float>(size);
    const float a = std::sqrt(gu * gu + gv * gv) * s, b = std::sqrt(hu * hu + hv * hv) * s;
    return std::log2(std::max({a, b, 1e-6f}));
}

struct Camera {
    Vec3 eye, target;
    float fovDeg = 30;
    int width = 640, height = 960;
};
struct Projected {
    float x, y, iw; // screen, 1 / view depth
};

struct Run {
    std::vector<Vertex> verts;
    mm::TextureArray array;
    bool pbr = false;
};

static Vec3 v(float x, float y, float z) { return Vec3{x, y, z}; }
static Vec3 mix(Vec3 a, Vec3 b, float t) { return a + (b - a) * t; }
static Vec3 mul(Vec3 a, Vec3 b) { return {a.x * b.x, a.y * b.y, a.z * b.z}; }

// Renders `run` from `cam` (2x2 samples, box down) into linear RGB, width x height.
static std::vector<float> render(const Run& run, const Camera& cam, const wl::Light& light) {
    const int ss = 2, W = cam.width * ss, H = cam.height * ss;
    const Vec3 f = normalized(cam.target - cam.eye), r = normalized(cross(f, v(0, 1, 0))), u = cross(r, f);
    const float th = std::tan(cam.fovDeg * 3.14159265f / 360.f), aspect = static_cast<float>(W) / static_cast<float>(H);
    auto project = [&](Vec3 p) {
        const Vec3 d = p - cam.eye;
        const float z = dot(d, f);
        const float x = dot(d, r) / (z * th * aspect), y = dot(d, u) / (z * th);
        return Projected{(x * .5f + .5f) * W, (.5f - y * .5f) * H, 1.f / z};
    };
    std::vector<float> depth(size_t(W) * H, 0.f); // 1/z, larger nearer
    std::vector<int32_t> tri(size_t(W) * H, -1);
    const size_t triangles = run.verts.size() / 3;
    std::vector<Projected> pr(run.verts.size());
    for (size_t i = 0; i < run.verts.size(); ++i)
        pr[i] = project(run.verts[i].position);
    for (size_t t = 0; t < triangles; ++t) {
        const Projected a = pr[t * 3], b = pr[t * 3 + 1], c = pr[t * 3 + 2];
        if (a.iw <= 0 || b.iw <= 0 || c.iw <= 0)
            continue;
        const float area = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
        if (std::abs(area) < 1e-12f)
            continue;
        const int x0 = std::max(0, static_cast<int>(std::floor(std::min({a.x, b.x, c.x})))),
                  x1 = std::min(W - 1, static_cast<int>(std::ceil(std::max({a.x, b.x, c.x})))),
                  y0 = std::max(0, static_cast<int>(std::floor(std::min({a.y, b.y, c.y})))),
                  y1 = std::min(H - 1, static_cast<int>(std::ceil(std::max({a.y, b.y, c.y}))));
        for (int y = y0; y <= y1; ++y)
            for (int x = x0; x <= x1; ++x) {
                const float px = x + .5f, py = y + .5f;
                const float w0 = ((b.x - px) * (c.y - py) - (b.y - py) * (c.x - px)) / area,
                            w1 = ((c.x - px) * (a.y - py) - (c.y - py) * (a.x - px)) / area, w2 = 1 - w0 - w1;
                if (w0 < 0 || w1 < 0 || w2 < 0)
                    continue;
                const float iw = w0 * a.iw + w1 * b.iw + w2 * c.iw;
                const size_t k = size_t(y) * W + x;
                if (iw > depth[k]) {
                    depth[k] = iw;
                    tri[k] = static_cast<int32_t>(t);
                }
            }
    }
    // Perspective-correct barycentrics of triangle t at screen (px, py), outside it too (for derivatives).
    auto bary = [&](size_t t, float px, float py, float out[3]) {
        const Projected a = pr[t * 3], b = pr[t * 3 + 1], c = pr[t * 3 + 2];
        const float area = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
        const float w0 = ((b.x - px) * (c.y - py) - (b.y - py) * (c.x - px)) / area,
                    w1 = ((c.x - px) * (a.y - py) - (c.y - py) * (a.x - px)) / area, w2 = 1 - w0 - w1;
        const float q0 = w0 * a.iw, q1 = w1 * b.iw, q2 = w2 * c.iw, s = q0 + q1 + q2;
        out[0] = q0 / s;
        out[1] = q1 / s;
        out[2] = q2 / s;
    };
    const mp::LightConstants lc = mp::constants(light, run.pbr);
    const bool hqBound = !run.array.hq.empty();
    Array skins{&run.array.layers, run.array.size, run.array.levels, true};
    Array maps{&run.array.linear, run.array.size, run.array.levels, false};
    Array skinsHq{&run.array.hq, run.array.hqSize, run.array.hqLevels, true};
    Array mapsHq{&run.array.hqLinear, run.array.hqSize, run.array.hqLevels, false};
    std::vector<float> hi(size_t(W) * H * 3);
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            const size_t k = size_t(y) * W + x;
            Vec3 rgb = mix(v(.30f, .31f, .33f), v(.12f, .12f, .13f), static_cast<float>(y) / H); // backdrop
            const int32_t t = tri[k];
            if (t >= 0) {
                float b[3], bx[3], by[3];
                const float px = x + .5f, py = y + .5f;
                bary(size_t(t), px, py, b);
                bary(size_t(t), px + 1, py, bx);
                bary(size_t(t), px, py + 1, by);
                const Vertex* c = &run.verts[size_t(t) * 3];
                auto at = [&](const float w[3], auto get) { return get(c[0]) * w[0] + get(c[1]) * w[1] + get(c[2]) * w[2]; };
                auto U = [](const Vertex& q) { return q.shape.x; };
                auto Vv = [](const Vertex& q) { return q.shape.y; };
                auto P = [](const Vertex& q) { return q.position; };
                const float su = at(b, U), sv = at(b, Vv);
                // The sample grid is 2x the game's pixel: derivatives per game pixel.
                const float gu = (at(bx, U) - su) * ss, gv = (at(bx, Vv) - sv) * ss, hu = (at(by, U) - su) * ss,
                            hv = (at(by, Vv) - sv) * ss;
                const Vec3 world = at(b, P), dpx = (at(bx, P) - world) * static_cast<float>(ss),
                           dpy = (at(by, P) - world) * static_cast<float>(ss);
                const Vertex& q = c[0]; // nointerpolation: the first corner's
                V4 tex{1, 1, 1, 1};
                const bool textured = q.shape.z >= 0;
                const bool pbr = run.pbr && (q.surface.maps & mp::flagOn) && lc.toSun[3] > .5f;
                const uint32_t hqBase = q.surface.tint >> 24;
                if (textured) {
                    if (pbr && hqBound && hqBase != mp::noHqLayer)
                        tex = skinsHq.sample(hqBase, su, sv, lodOf(gu, gv, hu, hv, skinsHq.size));
                    else
                        tex = skins.sample(static_cast<uint32_t>(q.shape.z + .5f), su, sv, lodOf(gu, gv, hu, hv, skins.size));
                }
                const Vec3 texel{tex.x, tex.y, tex.z};
                if (!pbr) {
                    Vec3 col{0, 0, 0};
                    for (int i = 0; i < 3; ++i)
                        col = col + c[i].color * b[i];
                    rgb = mul(col, texel);
                } else {
                    mp::Pixel p;
                    p.albedo = mul(mp::unpackTint(q.surface.tint), texel);
                    Vec3 n{0, 0, 0}, tg{0, 0, 0};
                    for (int i = 0; i < 3; ++i) {
                        n = n + mp::octUnpack(c[i].surface.normal) * b[i];
                        if (q.surface.maps & mp::flagTangent)
                            tg = tg + mp::octUnpack(c[i].surface.tangent) * b[i];
                    }
                    p.n = normalized(n);
                    p.w = (q.surface.maps & mp::flagFlip) ? -1.f : 1.f;
                    const uint32_t s = q.surface.maps;
                    if (s & mp::flagNormal) {
                        const uint32_t layer = s & mp::layerMask;
                        const V4 m = (s & mp::flagNormalHq) && hqBound
                                         ? mapsHq.sample(layer, su, sv, lodOf(gu, gv, hu, hv, mapsHq.size))
                                         : maps.sample(layer, su, sv, lodOf(gu, gv, hu, hv, maps.size));
                        p.mapped = v(m.x * 2 - 1, m.y * 2 - 1, m.z * 2 - 1);
                        if (std::getenv("PBR_PREVIEW_FLAT")) // a check: the maps' share of the look
                            p.mapped = v(0, 0, 1);
                        if (s & mp::flagTangent)
                            p.t = tg;
                        else { // the shader's derivative frame, as a tangent with B = -bc
                            const Vec3 a = cross(dpy, p.n), bb = cross(p.n, dpx);
                            const Vec3 tc = a * gu + bb * hu, bc = a * gv + bb * hv;
                            const float qq = std::max(dot(tc, tc), dot(bc, bc));
                            if (qq > 1e-24f) {
                                p.t = tc * (1 / std::sqrt(qq));
                                p.w = dot(cross(p.n, p.t), bc) > 0 ? -1.f : 1.f;
                            }
                        }
                    }
                    if (s & mp::flagOrm) {
                        const V4 o = maps.sample((s >> mp::ormShift) & mp::layerMask, su, sv, lodOf(gu, gv, hu, hv, maps.size));
                        p.orm = v(o.x, o.y, o.z);
                    }
                    p.occlusion = (s & mp::flagOcclusion) != 0;
                    p.roughness = mp::byteOf(q.surface.params, 0);
                    p.metallic = mp::byteOf(q.surface.params, 1);
                    p.normalScale = mp::byteOf(q.surface.params, 2) * 2;
                    p.occlusionStrength = mp::byteOf(q.surface.params, 3);
                    p.toEye = cam.eye - world;
                    p.sunVisible = 1;
                    rgb = mp::shade(lc, p).rgb;
                }
            }
            hi[k * 3] = rgb.x;
            hi[k * 3 + 1] = rgb.y;
            hi[k * 3 + 2] = rgb.z;
        }
    std::vector<float> img(size_t(cam.width) * cam.height * 3, 0.f);
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
            for (int ch = 0; ch < 3; ++ch)
                img[(size_t(y / ss) * cam.width + x / ss) * 3 + ch] += hi[(size_t(y) * W + x) * 3 + ch] / (ss * ss);
    return img;
}

static void writePpm(const std::string& path, const std::vector<float>& img, int w, int h) {
    std::ofstream f(path, std::ios::binary);
    f << "P6\n" << w << " " << h << "\n255\n";
    for (size_t i = 0; i < size_t(w) * h * 3; ++i)
        f.put(static_cast<char>(static_cast<uint8_t>(std::lround(toSrgb(img[i]) * 255))));
}

struct Loaded {
    mm::Library lib;
    uint32_t slot = 0;
    bool after = false;
};
static bool prepare(const std::string& root, uint32_t slot, bool after, Loaded& ld, Run& run, std::string& note) {
    ld.slot = slot;
    ld.after = after;
    if (ld.lib.load(slot, root) != mm::loaded || !ld.lib.model(slot)) {
        note = "slot not loaded from " + root;
        return false;
    }
    mm::MapOptions o;
    if (after)
        o = {true, mm::hqTextureSize, mm::hqMostLayers};
    run.array = mm::prepareTextures(ld.lib, cacheDecoder, mm::textureSize, mm::textureLevels, o);
    run.pbr = after;
    return misses.empty();
}
// The model's corners as the overlay emits them under `light` (the corner colours take the light; PBR on after).
static std::string emit(const Loaded& ld, Run& run, const wl::Light& light) {
    wl::setCurrent(light);
    mp::setEnabled(ld.after);
    mp::setMapsReady(ld.after && !run.array.linear.empty());
    mp::setHqReady(ld.after && !run.array.hq.empty());
    std::vector<Vertex> plain, sink;
    hero007::setModelSink(&sink);
    const Vec3 eye{0, 1.2f, 3};
    hero007::appendPlacedModel(plain, *ld.lib.model(ld.slot), v(0, 0, 0), v(0, 0, -1), v(0, 1, 0), eye);
    hero007::setModelSink(nullptr);
    run.verts = sink.empty() ? plain : sink;
    size_t on = 0, nrm = 0, hqn = 0, orm = 0, tan = 0, hqb = 0;
    for (const auto& q : run.verts) {
        const uint32_t f = q.surface.maps;
        on += (f & mp::flagOn) != 0;
        nrm += (f & mp::flagNormal) != 0;
        hqn += (f & mp::flagNormalHq) != 0;
        orm += (f & mp::flagOrm) != 0;
        tan += (f & mp::flagTangent) != 0;
        hqb += (f & mp::flagOn) && (q.surface.tint >> 24) != mp::noHqLayer;
    }
    char s[400];
    std::snprintf(s, sizeof s,
                  "%s: %zu vertices (%zu B), %zu layers, %zu map layers, %zu+%zu HQ layers (%u), %u tris | corners: pbr %zu "
                  "normal %zu (HQ %zu) orm %zu tangent %zu HQ base %zu",
                  ld.after ? "after" : "before", run.verts.size(), run.verts.size() * sizeof(Vertex), run.array.layers.size(),
                  run.array.linear.size(), run.array.hq.size(), run.array.hqLinear.size(), run.array.hqSize,
                  ld.lib.model(ld.slot)->triangles(), on, nrm, hqn, orm, tan, hqb);
    return s;
}

int main(int argc, char** argv) {
    if (argc < 5) {
        std::fprintf(stderr, "spidy_pbr_preview <before Models/> <after Models/> <cache/> <out/> [slot]\n");
        return 2;
    }
    cacheDir = argv[3];
    const std::string out = argv[4];
    const uint32_t slot = argc > 5 ? static_cast<uint32_t>(std::stoul(argv[5])) : mm::slotSuitFirst + 16;
    Run before, after;
    std::string nb, na;
    static Loaded lb, la;
    const bool okB = prepare(argv[1], slot, false, lb, before, nb), okA = prepare(argv[2], slot, true, la, after, na);
    if (!misses.empty()) {
        std::printf("MISSES %zu (decode the .enc files, then run again)\n", misses.size());
        return 3;
    }
    if (!okB || !okA) {
        std::fprintf(stderr, "%s\n%s\n", nb.c_str(), na.c_str());
        return 1;
    }
    // SAMPLE lights (offline, not read from the game): a high sun and a low warm one, world-light style.
    struct Named {
        const char* name;
        wl::Light l;
    };
    wl::Light noon, low;
    noon.world = low.world = true;
    noon.oneSided = low.oneSided = true;
    noon.why = low.why = wl::lit;
    noon.toSun = normalized(v(.45f, .75f, .5f));
    noon.sun = v(1.45f, 1.38f, 1.25f);
    noon.skyUp = v(.40f, .46f, .58f);
    noon.skyDown = v(.16f, .15f, .13f);
    low.toSun = normalized(v(-.7f, .3f, .45f));
    low.sun = v(1.25f, .85f, .55f);
    low.skyUp = v(.22f, .25f, .34f);
    low.skyDown = v(.09f, .08f, .07f);
    for (wl::Light* l : {&noon, &low}) {
        l->specular = l->sun * (1.f / .68f);
        l->env = l->skyUp * (1.f / .32f);
    }
    const Named lights[] = {{"noon", noon}, {"low", low}};
    struct View {
        const char* name;
        Camera c;
    };
    const View views[] = {{"front", {v(0, 1.0f, 3.9f), v(0, .93f, 0), 30, 560, 960}},
                          {"three", {v(2.4f, 1.35f, 2.9f), v(0, .93f, 0), 30, 560, 960}},
                          {"chest", {v(.25f, 1.5f, 1.05f), v(0, 1.35f, 0), 30, 640, 640}}};
    // 0.0.78 PUNISHER ARSENAL: a gun (its grip point at the origin, the barrel along -z) framed close instead
    const View gunViews[] = {{"front", {v(.95f, .05f, -.16f), v(0, .03f, -.17f), 30, 960, 560}},
                             {"three", {v(.62f, .30f, .32f), v(0, .03f, -.15f), 30, 960, 560}},
                             {"chest", {v(-.10f, .16f, .30f), v(0, .07f, -.10f), 30, 640, 640}}};
    const bool gun = mm::slotIsArsenal078(slot);
    for (const auto& L : lights) {
        std::printf("%s\n  %s\n  %s\n", L.name, emit(lb, before, L.l).c_str(), emit(la, after, L.l).c_str());
        for (const auto& V : gun ? gunViews : views) {
            const auto a = render(before, V.c, L.l), b = render(after, V.c, L.l);
            writePpm(out + V.name + "_" + L.name + "_before.ppm", a, V.c.width, V.c.height);
            writePpm(out + V.name + "_" + L.name + "_after.ppm", b, V.c.width, V.c.height);
            std::printf("%s %s done\n", V.name, L.name);
        }
    }
    return 0;
}
