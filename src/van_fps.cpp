// Version 0.0.73 VAN FPS (van_fps.hpp).
#include "spidy/van_fps.hpp"
#include "spidy/car_paint.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>

namespace spidy::vanfps {
namespace {
bool fin(Vec3 v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }

struct Kept {
    const mesh_model::Mesh* mesh{};
    const Vec3* corners{};
    const Vec3* normals{};
    uint32_t triangles{};
    bool smooth{};
    std::vector<Vec3> face;    // 1 a triangle: its unit normal (model space)
    std::vector<float> area;   // 1 a triangle: |cross| in model space (0: a sliver)
    std::vector<Vec3> corner;  // 3 a triangle: the corner's unit normal (smooth models)
    std::vector<float> length; // 3 a triangle: the given normal's length
    // 0.0.74 test 2 CAR PAINT: the satin colours (3 a triangle), a quarter of them relit each frame (every
    // fourth triangle, so no band lags), all of them when the eye or the light moved far in model space.
    std::vector<Vec3> satin;
    Vec3 satinEye{}, satinLight{};
    uint32_t satinPhase = 0;
    bool satinValid = false;
};
thread_local Kept kept[preparedKept];
thread_local uint32_t nextKept = 0;
thread_local Stats counts;
std::atomic<bool> on{true};

bool same(const Kept& k, const mesh_model::Mesh& m) {
    return k.mesh == &m && k.corners == m.corners.data() && k.triangles == m.triangles() &&
           k.smooth == m.hasNormals() && k.normals == (m.hasNormals() ? m.normals.data() : nullptr);
}

Kept* prepare(const mesh_model::Mesh& m) {
    for (auto& k : kept)
        if (same(k, m))
            return &k;
    Kept& k = kept[nextKept];
    nextKept = (nextKept + 1) % preparedKept;
    k = Kept{};
    const uint32_t n = m.triangles();
    const bool smooth = m.hasNormals();
    k.face.resize(n);
    k.area.resize(n);
    if (smooth)
        k.corner.resize(size_t{n} * 3), k.length.resize(size_t{n} * 3);
    for (uint32_t t = 0; t < n; ++t) {
        const Vec3 a = m.corners[t * 3], b = m.corners[t * 3 + 1], c = m.corners[t * 3 + 2];
        for (const Vec3& p : {a, b, c})
            if (!fin(p) || std::max({std::abs(p.x), std::abs(p.y), std::abs(p.z)}) > 1e4f)
                return nullptr; // odd numbers: the old path (it checks everything)
        const Vec3 fn = cross(b - a, c - a);
        const float fl = spidy::length(fn);
        k.area[t] = fl;
        k.face[t] = fl > 0 ? fn * (1.f / fl) : Vec3{};
        if (smooth)
            for (uint32_t i = 0; i < 3; ++i) {
                const Vec3 nm = m.normals[t * 3 + i];
                const float l = spidy::length(nm);
                if (!fin(nm) || !std::isfinite(l))
                    return nullptr;
                k.length[t * 3 + i] = l;
                k.corner[t * 3 + i] = l > 0 ? nm * (1.f / l) : Vec3{};
            }
    }
    k.mesh = &m;
    k.corners = m.corners.data();
    k.normals = smooth ? m.normals.data() : nullptr;
    k.triangles = n;
    k.smooth = smooth;
    ++counts.prepared;
    return &k;
}
} // namespace

bool uniformPlace(Vec3 o, Vec3 ex, Vec3 ey, Vec3 ez, float& scale) {
    if (!fin(o) || !fin(ex) || !fin(ey) || !fin(ez) || std::max({std::abs(o.x), std::abs(o.y), std::abs(o.z)}) > 1e6f)
        return false;
    const float s = spidy::length(ex), sy = spidy::length(ey), sz = spidy::length(ez);
    if (!(s > 1e-4f) || !(s < 100.f) || std::abs(sy - s) > 1e-3f * s || std::abs(sz - s) > 1e-3f * s)
        return false;
    const float s2 = s * s;
    if (std::abs(dot(ex, ey)) > 1e-3f * s2 || std::abs(dot(ey, ez)) > 1e-3f * s2 || std::abs(dot(ex, ez)) > 1e-3f * s2)
        return false;
    scale = s;
    return true;
}

bool drawPrepared(std::vector<Vertex>& sink, const mesh_model::Mesh& model, Vec3 o, Vec3 ex, Vec3 ey, Vec3 ez, const Vec3* viewer) {
    float s = 0;
    if (!on.load(std::memory_order_relaxed) || !model.ok() || model.triangles() < preparedFrom || !uniformPlace(o, ex, ey, ez, s)) {
        ++counts.declined;
        return false;
    }
    Kept* k = prepare(model);
    if (!k) {
        ++counts.declined;
        return false;
    }
    const Vec3 light = normalized(Vec3{.3f, 1.f, .25f});
    // The light in model space: a world normal R n (R = the axes over s) meets it as n meets R^T light.
    const Vec3 lm = Vec3{dot(ex, light), dot(ey, light), dot(ez, light)} * (1.f / s);
    const bool uvs = model.hasUvs(), smooth = k->smooth;
    const float sliver = 1e-14f / (s * s), flat = 1e-12f / s;
    const uint32_t n = k->triangles;
    const size_t from = sink.size();
    sink.reserve(from + size_t{n} * 3); // no zeroing: every vertex is written once
    const auto P = [&](Vec3 c) { return o + ex * c.x + ey * c.y + ez * c.z; };
    // 0.0.74 test 2 CAR PAINT: the eye and world up in model space (a world vector w meets the model as R^T w / s).
    const bool paint = viewer && fin(*viewer) && car_paint::enabled() && model.paint.size() == size_t{n} * 2;
    const Vec3 rel = paint ? *viewer - o : Vec3{};
    const Vec3 em = Vec3{dot(ex, rel), dot(ey, rel), dot(ez, rel)} * (1.f / (s * s));
    const Vec3 um = Vec3{ex.y, ey.y, ez.y} * (1.f / s);
    car_paint::Material mat;
    if (paint) {
        if (k->satin.size() != size_t{n} * 3)
            k->satin.assign(size_t{n} * 3, Vec3{}), k->satinValid = false;
        if (!k->satinValid || spidy::length(em - k->satinEye) > satinRelightEye || spidy::length(lm - k->satinLight) > satinRelightLight)
            k->satinValid = false;
        k->satinPhase = (k->satinPhase + 1) % satinSlices;
        k->satinEye = em, k->satinLight = lm; // the next frame's jump is from here
    } else {
        k->satinValid = false; // FLAT or no eye: the kept colours are stale when SATIN comes back
    }
    const bool relightAll = paint && !k->satinValid;
    for (uint32_t t = 0; t < n; ++t) {
        if (!(k->area[t] > sliver))
            continue; // the old path's sliver (world |cross| <= 1e-14): nothing drawn
        const int16_t im = uvs && t < model.image.size() ? model.image[t] : int16_t{-1};
        const int32_t at = im >= 0 && static_cast<size_t>(im) < model.images.size() ? model.images[static_cast<size_t>(im)].layer : -1;
        const float layer = at >= 0 ? static_cast<float>(at) : -1.f;
        const Vec3 tint = at >= 0 && t < model.tints.size() ? model.tints[t] : model.colors[t];
        const float faceLit = std::abs(dot(k->face[t], lm));
        const bool painted = paint && at < 0 && model.paint[t * 2] != 0;
        car_paint::Facing face;
        const bool relight = painted && (relightAll || t % satinSlices == k->satinPhase);
        if (relight) { // one toward-eye a triangle (its middle): the corners' normals carry the shape
            car_paint::material(mat, tint, model.paint[t * 2], model.paint[t * 2 + 1]);
            const Vec3 mid = (model.corners[t * 3] + model.corners[t * 3 + 1] + model.corners[t * 3 + 2]) * (1.f / 3);
            const Vec3 to = em - mid;
            const float tl = spidy::length(to);
            if (tl > 1e-6f)
                face = car_paint::facing(mat, lm, to * (1.f / tl), um);
        }
        for (uint32_t i = 0; i < 3; ++i) {
            const size_t c = size_t{t} * 3 + i;
            float d = faceLit;
            if (smooth && k->length[c] > flat)
                d = std::abs(dot(k->corner[c], lm));
            const float lit = .32f + .68f * d;
            const float u = uvs ? model.uvs[t * 6 + i * 2] : 0.f, v = uvs ? model.uvs[t * 6 + i * 2 + 1] : 0.f;
            Vec3 colour = tint * lit;
            if (relight) {
                colour = face.ok ? car_paint::corner(mat, face, smooth && k->length[c] > flat ? k->corner[c] : k->face[t], lm, um) : colour;
                k->satin[c] = colour;
            } else if (painted) {
                colour = k->satin[c];
            }
            sink.push_back(Vertex{P(model.corners[c]), colour, 1.f, {u, v, layer, 1.f}});
        }
    }
    if (relightAll)
        k->satinValid = true;
    // Colours and uvs come from the model as they are now (they are not kept); one cheap look at them.
    for (size_t i = from; i < sink.size(); ++i) {
        const Vertex& v = sink[i];
        if (!std::isfinite(v.color.x + v.color.y + v.color.z + v.shape.x + v.shape.y)) {
            sink.resize(from);
            ++counts.declined;
            return false;
        }
    }
    ++counts.drawn;
    return true;
}

Stats stats() { return counts; }
void setEnabled(bool v) { on.store(v); }
bool enabled() { return on.load(); }
void forget() {
    for (auto& k : kept)
        k = Kept{};
    nextKept = 0;
    counts = {};
}
} // namespace spidy::vanfps
