// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
// Version 0.0.7 HERO PROFILES' looks (hero007_visual.hpp).
#include "spidy/hero007_visual.hpp"
#include "spidy/hero007.hpp"
#include "spidy/captain.hpp"
#include "spidy/gun_grip.hpp"
#include "spidy/pixel_font.hpp"
#include "spidy/soft_shapes.hpp"
#include "spidy/web_visual.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace spidy::hero007 {
namespace {
using soft::perpendicular;
using soft::softArc;
using soft::softBar;
using soft::softTriangle;
constexpr float pi = 3.14159265f;

// A material: the rim (silhouette edge), the body, the light streak and how bright it is.
struct Material {
    Vec3 rim, base, light;
    float shine;
};
constexpr Material steel{{.03f, .03f, .035f}, {.16f, .17f, .19f}, {.70f, .73f, .78f}, .75f};
constexpr Material polymer{{.008f, .008f, .01f}, {.05f, .05f, .055f}, {.24f, .24f, .26f}, .35f};
constexpr Material blued{{.01f, .01f, .02f}, {.08f, .09f, .12f}, {.50f, .55f, .66f}, .7f};
constexpr Material wood{{.10f, .05f, .02f}, {.33f, .18f, .08f}, {.60f, .38f, .19f}, .35f};
constexpr Material leather{{.04f, .02f, .01f}, {.13f, .08f, .05f}, {.30f, .20f, .12f}, .25f};
constexpr Material bright{{.12f, .12f, .14f}, {.55f, .58f, .62f}, {.95f, .97f, 1.f}, .9f};
constexpr Material lacquer{{.12f, .0f, .01f}, {.45f, .03f, .04f}, {.95f, .40f, .38f}, .6f};
constexpr Material rubber{{.0f, .0f, .0f}, {.045f, .04f, .04f}, {.20f, .19f, .19f}, .3f};
constexpr Material brass{{.25f, .17f, .05f}, {.70f, .52f, .20f}, {1.f, .88f, .55f}, .8f};
constexpr Material shellRed{{.20f, .02f, .02f}, {.62f, .07f, .06f}, {.95f, .45f, .40f}, .5f};
constexpr Material batBlack{{0, 0, 0}, {.07f, .07f, .08f}, {.36f, .37f, .42f}, .6f};
// Version 0.0.72 GRAPPLE ROPE: braided grey-black cord, a lighter rim so it reads against night sky and walls.
constexpr Material cord{{.30f, .31f, .33f}, {.11f, .11f, .12f}, {.48f, .49f, .52f}, .45f};

bool ok(Vec3 v) { return finite(v); }
// The light comes from above and a little behind the viewer: the streak sits on that side.
Vec3 lightSide(Vec3 p, Vec3 q, Vec3 view) {
    const Vec3 side = normalized(cross(q - p, view));
    return dot(side, Vec3{0, 1, 0}) + .3f * dot(side, view) >= 0 ? side : -side;
}
// A shaded part p-q: rim, body, light streak (162 vertices).
void part(std::vector<Vertex>& out, Vec3 p, Vec3 q, Vec3 viewer, float pixelAngle, float hw, const Material& m,
          float a = .97f) {
    if (length(q - p) < 1e-4f)
        return;
    const Vec3 mid = (p + q) * .5f;
    const float range = std::max(length(viewer - mid), .05f);
    const Vec3 view = normalized(viewer - mid);
    const float floor = .75f * pixelAngle * range;
    const float w = std::max(hw, floor), s = std::max(w * .25f, floor * .5f);
    softBar(out, p, q, view, w * 1.16f, m.rim, a, s);
    softBar(out, p, q, view, w, m.base, a, s * .5f);
    const Vec3 side = lightSide(p, q, view) * (w * .45f);
    softBar(out, p + side, q + side, view, w * .22f, m.light, a * m.shine, s * .5f);
}
// A flat bar of one colour (54 vertices).
void line(std::vector<Vertex>& out, Vec3 p, Vec3 q, Vec3 viewer, float pixelAngle, float hw, Vec3 c, float a) {
    if (length(q - p) < 1e-4f)
        return;
    const Vec3 mid = (p + q) * .5f;
    const float range = std::max(length(viewer - mid), .05f);
    const float w = std::max(hw, .5f * pixelAngle * range);
    softBar(out, p, q, normalized(viewer - mid), w, c, a, w * .3f);
}
// A disc facing the eye (120 vertices at 10 sides).
void disc(std::vector<Vertex>& out, Vec3 at, Vec3 viewer, float r, Vec3 c, float a) {
    const Vec3 v = normalized(viewer - at);
    const Vec3 s1 = perpendicular(v), s2 = cross(v, s1);
    softArc(out, at, s1, s2, 0, r, 0, 2 * pi, c, a, r * .25f, 10);
}
// A soft round glob facing the eye (90 vertices).
void glob(std::vector<Vertex>& out, Vec3 center, Vec3 viewer, float radius, Vec3 color, float alpha) {
    const Vec3 v = normalized(viewer - center);
    const Vec3 a = perpendicular(v), b = cross(v, a);
    constexpr int sides = 10;
    for (int i = 0; i < sides; ++i) {
        const float t0 = 2 * pi * static_cast<float>(i) / sides, t1 = 2 * pi * static_cast<float>(i + 1) / sides;
        const Vec3 e0 = a * std::cos(t0) + b * std::sin(t0), e1 = a * std::cos(t1) + b * std::sin(t1);
        const Vertex c{center, color, alpha}, m0{center + e0 * (radius * .55f), color, alpha * .7f},
            m1{center + e1 * (radius * .55f), color, alpha * .7f};
        out.insert(out.end(), {c, m0, m1});
        soft::quad(out, m0, m1, {center + e1 * radius, color, 0}, {center + e0 * radius, color, 0});
    }
}
// The item's frame: f forward, u up (square to f), r right.
struct Frame {
    Vec3 o, f, u, r;
};
bool frameOf(const Item& it, Frame& k) {
    if (!ok(it.at) || !ok(it.forward) || !ok(it.up) || length(it.forward) < 1e-4f)
        return false;
    k.o = it.at;
    k.f = normalized(it.forward);
    Vec3 u = it.up - k.f * dot(it.up, k.f);
    if (length(u) < 1e-4f)
        u = perpendicular(k.f);
    k.u = normalized(u);
    k.r = cross(k.f, k.u);
    return true;
}
bool inRange(Vec3 at, Vec3 viewer, float most) {
    const float d = length(at - viewer);
    return std::isfinite(d) && d >= .05f && d <= most;
}
// Put the vertices back when anything came out insane.
struct Guard {
    std::vector<Vertex>& out;
    size_t from;
    explicit Guard(std::vector<Vertex>& o) : out(o), from(o.size()) {}
    ~Guard() {
        if (!web_style::sane(out, from))
            out.resize(from);
    }
};
} // namespace

void appendPistol(std::vector<Vertex>& out, const Item& it, Vec3 viewer, float pixelAngle) {
    Frame k{};
    if (!frameOf(it, k) || !ok(viewer) || !std::isfinite(pixelAngle) || !inRange(k.o, viewer, 40.f))
        return;
    Guard g(out);
    const Vec3 o = k.o, f = k.f, u = k.u, r = k.r;
    const auto P = [&](float fwd, float up, float right = 0) { return o + f * fwd + u * up + r * right; };
    // The grip (polymer, raked back ~18 degrees) and its stippling.
    part(out, P(-.015f, .035f), P(-.042f, -.065f), viewer, pixelAngle, .014f, polymer);
    for (int i = 0; i < 3; ++i) {
        const float t = .3f + .2f * static_cast<float>(i);
        const Vec3 c = P(-.015f - .027f * t, .035f - .1f * t);
        line(out, c - f * .009f, c + f * .009f, viewer, pixelAngle, .0016f, {.015f, .015f, .017f}, .9f);
    }
    // The frame and dust cover, the trigger guard and trigger.
    part(out, P(-.03f, .04f), P(.12f, .04f), viewer, pixelAngle, .011f, polymer);
    line(out, P(.0f, .03f), P(.02f, .006f), viewer, pixelAngle, .003f, polymer.base, .97f);
    line(out, P(.02f, .006f), P(.062f, .03f), viewer, pixelAngle, .003f, polymer.base, .97f);
    line(out, P(.022f, .03f), P(.019f, .014f), viewer, pixelAngle, .0028f, {.02f, .02f, .02f}, .97f);
    // The slide (steel), its rear serrations, the sights and the muzzle.
    part(out, P(-.035f, .062f), P(.16f, .062f), viewer, pixelAngle, .013f, steel);
    for (int i = 0; i < 3; ++i) {
        const float x = -.026f + .007f * static_cast<float>(i);
        line(out, P(x, .066f, -.011f), P(x, .066f, .011f), viewer, pixelAngle, .0013f, {.02f, .02f, .025f}, .9f);
    }
    line(out, P(.148f, .076f), P(.152f, .076f), viewer, pixelAngle, .0022f, {.01f, .01f, .01f}, .97f);
    line(out, P(-.03f, .076f, -.004f), P(-.026f, .076f, -.004f), viewer, pixelAngle, .002f, {.01f, .01f, .01f}, .97f);
    line(out, P(-.03f, .076f, .004f), P(-.026f, .076f, .004f), viewer, pixelAngle, .002f, {.01f, .01f, .01f}, .97f);
    disc(out, P(.162f, .062f), viewer, .0055f, {.005f, .005f, .005f}, .97f);
}

void appendSawedOff(std::vector<Vertex>& out, const Item& it, bool open, Vec3 viewer, float pixelAngle) {
    Frame k{};
    if (!frameOf(it, k) || !ok(viewer) || !std::isfinite(pixelAngle) || !inRange(k.o, viewer, 40.f))
        return;
    Guard g(out);
    const Vec3 o = k.o, f = k.f, u = k.u, r = k.r;
    const auto P = [&](float fwd, float up, float right = 0) { return o + f * fwd + u * up + r * right; };
    // The wooden pistol grip, the trigger guard, the steel receiver.
    part(out, P(.0f, .04f), P(-.035f, -.07f), viewer, pixelAngle, .016f, wood);
    line(out, P(.005f, .03f), P(.03f, .008f), viewer, pixelAngle, .003f, steel.base, .97f);
    line(out, P(.03f, .008f), P(.065f, .032f), viewer, pixelAngle, .003f, steel.base, .97f);
    part(out, P(-.04f, .05f), P(.07f, .05f), viewer, pixelAngle, .02f, steel);
    // The barrels turn down 35 degrees about the hinge when broken open.
    const Vec3 hinge = P(.07f, .045f);
    const float a = open ? 35.f * pi / 180.f : 0.f;
    const Vec3 bd = f * std::cos(a) - u * std::sin(a), bu = u * std::cos(a) + f * std::sin(a);
    part(out, hinge - bu * .006f + bd * .02f, hinge - bu * .006f + bd * .17f, viewer, pixelAngle, .016f, wood);
    for (const float side : {-.011f, .011f}) {
        const Vec3 b0 = hinge + bu * .012f + r * side, b1 = b0 + bd * .30f;
        part(out, b0, b1, viewer, pixelAngle, .0105f, blued);
        disc(out, b1 + bd * .001f, viewer, .0075f, {.004f, .004f, .006f}, .97f);
        if (open) // the empty chambers at the breech
            disc(out, b0 - bd * .001f, viewer, .0075f, {.25f, .18f, .07f}, .97f);
    }
    line(out, hinge + bu * .024f, hinge + bu * .024f + bd * .29f, viewer, pixelAngle, .0015f, blued.light, .5f);
}

void appendBatarang(std::vector<Vertex>& out, const Item& it, Vec3 viewer, float pixelAngle) {
    Frame k{};
    if (!frameOf(it, k) || !ok(viewer) || !std::isfinite(pixelAngle) || !inRange(k.o, viewer, 80.f))
        return;
    Guard g(out);
    const float s = std::isfinite(it.spin) ? it.spin : 0.f;
    // In the plane of f and r, turned by spin about u; y toward the nose, x across.
    const Vec3 ny = k.f * std::cos(s) + k.r * std::sin(s), nx = cross(ny, k.u);
    const float grow = std::max(.0015f, .5f * pixelAngle * length(k.o - viewer));
    const Vec3 view = normalized(viewer - k.o);
    const auto P = [&](float x, float y) { return k.o + nx * x + ny * y; };
    const auto tri = [&](float ax, float ay, float bx, float by, float cx, float cy, Vec3 c, float a) {
        softTriangle(out, {P(ax, ay), P(bx, by), P(cx, cy)}, view, c, a, grow * .5f, grow);
    };
    for (const float m : {-1.f, 1.f}) {
        tri(0, .035f, m * .018f, 0, 0, -.03f, batBlack.base, .97f);                   // the body
        tri(m * .014f, .012f, m * .10f, .03f, m * .03f, -.02f, batBlack.base, .97f);    // the wing
        tri(m * .10f, .03f, m * .085f, -.018f, m * .05f, -.008f, batBlack.base, .97f);  // its scalloped tip
        tri(m * .004f, .03f, m * .012f, .052f, m * .011f, .028f, batBlack.base, .97f);  // the ear
        tri(0, .024f, m * .009f, 0, 0, -.018f, Vec3{.16f, .16f, .18f}, .9f);            // the raised centre
    }
    line(out, P(.016f, .013f), P(.098f, .03f), viewer, pixelAngle, .0012f, batBlack.light, .55f);
    line(out, P(-.016f, .013f), P(-.098f, .03f), viewer, pixelAngle, .0012f, batBlack.light, .55f);
}

namespace {
std::vector<Vertex>* gModelSink = nullptr;
}
void setModelSink(std::vector<Vertex>* sink) {
    gModelSink = sink;
}
std::vector<Vertex>* modelSink() {
    return gModelSink;
}

namespace {
// Version 0.0.72 REAL MODELS: a textured model into the model sink (the overlay's depth-tested
// texture-array pass: no sort), each corner lit by its own normal (smooth where the model is
// smooth), shape = (u, v, layer, 1); a triangle without a texture layer has layer -1 (its colour).
template <class Place>
bool drawTextured(std::vector<Vertex>& sink, const mesh_model::Mesh& model, Place place) {
    const Vec3 o = place(Vec3{0, 0, 0});
    const Vec3 ex = place(Vec3{1, 0, 0}) - o, ey = place(Vec3{0, 1, 0}) - o, ez = place(Vec3{0, 0, 1}) - o;
    if (!ok(o) || !ok(ex) || !ok(ey) || !ok(ez))
        return false;
    const size_t from = sink.size();
    if (from + size_t{model.triangles()} * 3 > modelSinkBudget)
        return false;
    const Vec3 light = normalized(Vec3{.3f, 1.f, .25f});
    const bool smooth = model.hasNormals(), uvs = model.hasUvs();
    sink.reserve(from + size_t{model.triangles()} * 3); // build 7: one growth, not a dozen
    // Build 7: the place is affine (at + axes), so each corner is o + ex x + ey y + ez z: no call per corner.
    const auto P = [&](Vec3 c) { return o + ex * c.x + ey * c.y + ez * c.z; };
    for (uint32_t t = 0; t < model.triangles(); ++t) {
        const int16_t im = uvs && t < model.image.size() ? model.image[t] : int16_t{-1};
        const int32_t at = im >= 0 && static_cast<size_t>(im) < model.images.size() ? model.images[static_cast<size_t>(im)].layer : -1;
        // An image without a layer (it did not decode): its mean colour (colors[]) untextured.
        const float layer = at >= 0 ? static_cast<float>(at) : -1.f;
        const Vec3 tint = at >= 0 && t < model.tints.size() ? model.tints[t] : model.colors[t];
        const Vec3 a = P(model.corners[t * 3]), b = P(model.corners[t * 3 + 1]), c = P(model.corners[t * 3 + 2]);
        const Vec3 fn = cross(b - a, c - a);
        const float fl = length(fn);
        if (!(fl > 1e-14f))
            continue;
        const Vec3 p[3]{a, b, c};
        for (uint32_t k = 0; k < 3; ++k) {
            Vec3 n = fn * (1.f / fl);
            if (smooth) {
                const Vec3 m = model.normals[t * 3 + k];
                const Vec3 w = ex * m.x + ey * m.y + ez * m.z;
                if (length(w) > 1e-12f)
                    n = normalized(w);
            }
            const float lit = .32f + .68f * std::abs(dot(n, light)); // both sides (any winding)
            const float u = uvs ? model.uvs[t * 6 + k * 2] : 0.f, v = uvs ? model.uvs[t * 6 + k * 2 + 1] : 0.f;
            sink.push_back(Vertex{p[k], tint * lit, 1.f, {u, v, layer, 1.f}});
        }
    }
    for (size_t i = from; i < sink.size(); ++i) {
        const Vertex& v = sink[i];
        if (!ok(v.position) || !ok(v.color) || !std::isfinite(v.shape.x) || !std::isfinite(v.shape.y)) {
            sink.resize(from);
            return false;
        }
    }
    return true;
}

// Draws a model's triangles placed by `place` (model space -> world), far to near, lit per face.
// Version 0.0.72: textured into the model sink when the overlay has one; otherwise (the overlay
// could not make its texture array) untextured, its light copy (Mesh::lite) when it has one.
template <class Place>
bool drawMesh(std::vector<Vertex>& out, const mesh_model::Mesh& full, Place place, Vec3 viewer) {
    if (!full.ok() || full.triangles() > std::min(full.cap, mesh_model::vanTriangles))
        return false;
    if (gModelSink)
        return drawTextured(*gModelSink, full, place);
    const mesh_model::Mesh& model = full.lite && full.lite->ok() ? *full.lite : full;
    if (model.triangles() > mesh_model::liteTriangles)
        return false;
    thread_local std::vector<std::pair<float, uint32_t>> order;
    thread_local std::vector<Vec3> placed;
    const uint32_t n = model.triangles();
    order.clear();
    placed.resize(size_t{n} * 3);
    for (uint32_t t = 0; t < n; ++t) {
        Vec3 sum{};
        for (uint32_t c = 0; c < 3; ++c)
            sum = sum + (placed[t * 3 + c] = place(model.corners[t * 3 + c]));
        if (!ok(sum))
            return false;
        order.emplace_back(length(sum * (1.f / 3) - viewer), t);
    }
    std::sort(order.begin(), order.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
    Guard g(out);
    const Vec3 light = normalized(Vec3{.3f, 1.f, .25f});
    for (const auto& [far, t] : order) {
        (void)far;
        const Vec3 a = placed[t * 3], b = placed[t * 3 + 1], c = placed[t * 3 + 2];
        const Vec3 n3 = cross(b - a, c - a);
        const float len = length(n3);
        if (!(len > 1e-12f))
            continue; // a sliver: nothing to see
        const float lit = .32f + .68f * std::abs(dot(n3 * (1.f / len), light)); // both sides (any winding)
        const Vec3 colour = model.colors[t] * lit;
        for (const Vec3& p : {a, b, c})
            out.push_back(Vertex{p, colour, 1.f, {}});
    }
    return true;
}
} // namespace

bool appendBatarangModel(std::vector<Vertex>& out, const Item& it, const mesh_model::Mesh& model, Vec3 viewer) {
    Frame k{};
    if (!model.ok() || model.triangles() > std::min(model.cap, mesh_model::vanTriangles) || !frameOf(it, k) || !ok(viewer) ||
        !inRange(k.o, viewer, 80.f))
        return false;
    const mesh_model::Fit fit = mesh_model::fit(model, .20f);
    if (!(fit.scale > 0) || !std::isfinite(fit.scale))
        return false;
    const float s = std::isfinite(it.spin) ? it.spin : 0.f;
    const Vec3 ny = k.f * std::cos(s) + k.r * std::sin(s), nx = cross(ny, k.u);
    Vec3 axis[3];
    axis[fit.across] = nx * fit.scale;
    axis[fit.along] = ny * fit.scale;
    axis[fit.thin] = k.u * fit.scale;
    return drawMesh(out, model, [&](Vec3 c) {
        const Vec3 d = c - fit.centre;
        return k.o + axis[0] * d.x + axis[1] * d.y + axis[2] * d.z;
    }, viewer);
}

namespace {
// A converted model's size sanity: the converter writes metres; anything over 2 m is not ours.
bool metric(const mesh_model::Mesh& m) {
    const Vec3 s = m.high - m.low;
    return m.ok() && std::max({s.x, s.y, s.z}) < 2.f && std::max({s.x, s.y, s.z}) > .01f;
}
} // namespace

bool appendModel(std::vector<Vertex>& out, const Item& it, const mesh_model::Mesh& model, Vec3 viewer) {
    Frame k{};
    if (!metric(model) || !frameOf(it, k) || !ok(viewer) || !inRange(k.o, viewer, 40.f))
        return false;
    return drawMesh(out, model, [&](Vec3 c) { return k.o + k.r * c.x + k.u * c.y - k.f * c.z; }, viewer);
}

// Version 0.0.72 PUNISHER VAN: a model placed by its own axes (world = at + right x + up y - forward z), any
// size to 8 m (the van and its parts), seen to 80 m. right / up / forward are made orthonormal here (forward kept).
bool appendPlacedModel(std::vector<Vertex>& out, const mesh_model::Mesh& model, Vec3 at, Vec3 forward, Vec3 up, Vec3 viewer,
                       float scale) {
    const Vec3 s = model.high - model.low;
    const float most = std::max({s.x, s.y, s.z});
    if (!model.ok() || !(most < 8.f) || !(most > .01f) || !(scale > .01f) || !(scale < 8.f) || !ok(at) || !ok(forward) || !ok(up) || !ok(viewer) ||
        length(forward) < 1e-4f || !inRange(at, viewer, 80.f))
        return false;
    const Vec3 f = normalized(forward);
    Vec3 u = up - f * dot(up, f);
    if (length(u) < 1e-4f)
        return false;
    u = normalized(u);
    const Vec3 r = cross(f, u) * scale, us = u * scale, fs = f * scale;
    return drawMesh(out, model, [&](Vec3 c) { return at + r * c.x + us * c.y - fs * c.z; }, viewer);
}

bool grappleMuzzle(const Item& gun, const mesh_model::Mesh& model, Vec3& muzzle) {
    Frame k{};
    if (!model.hasPivot || !ok(model.pivot) || !frameOf(gun, k))
        return false;
    muzzle = k.o + k.r * model.pivot.x + k.u * model.pivot.y - k.f * model.pivot.z;
    return ok(muzzle);
}

bool appendSawedOffModel(std::vector<Vertex>& out, const Item& it, const mesh_model::Mesh& frame,
                         const mesh_model::Mesh& barrels, bool open, Vec3 viewer) {
    Frame k{};
    if (!metric(frame) || !metric(barrels) || !barrels.hasPivot || frame.triangles() + barrels.triangles() > mesh_model::drawTriangles ||
        !frameOf(it, k) || !ok(viewer) || !inRange(k.o, viewer, 40.f))
        return false;
    const size_t mark = out.size();
    if (!drawMesh(out, frame, [&](Vec3 c) { return k.o + k.r * c.x + k.u * c.y - k.f * c.z; }, viewer))
        return false;
    const float a = open ? 35.f * pi / 180.f : 0.f;
    const Vec3 bd = k.f * std::cos(a) - k.u * std::sin(a), bu = k.u * std::cos(a) + k.f * std::sin(a);
    const Vec3 pv = barrels.pivot;
    const Vec3 hinge = k.o + k.r * pv.x + k.u * pv.y - k.f * pv.z;
    if (!drawMesh(out, barrels, [&](Vec3 c) {
            const Vec3 d = c - pv;
            return hinge + k.r * d.x + bu * d.y - bd * d.z;
        }, viewer)) {
        out.resize(mark);
        return false;
    }
    return true;
}

BatonFit batonFit(Vec3 grip, Vec3 tip, const mesh_model::Mesh& model, bool inHand) {
    BatonFit f{};
    const Vec3 s = model.high - model.low;
    const float span = length(tip - grip), thick = std::min(s.x, s.y);
    if (!ok(grip) || !ok(tip) || span < .05f || !(s.z > .05f) || !(thick > .005f))
        return f;
    f.across = std::clamp(batonDiameter / thick, .1f, 4.f);
    f.along = std::clamp(span / s.z, .1f, 4.f);
    const Vec3 way = (tip - grip) * (1.f / span);
    f.at = inHand ? grip + way * (span / 3.f)                         // the model's grip on the hand (gripShare)
                  : grip + way * (std::max(0.f, model.high.z) * f.along); // the butt at the grip end
    f.ok = ok(f.at);
    return f;
}

bool appendBatonModel(std::vector<Vertex>& out, Vec3 grip, Vec3 tip, const mesh_model::Mesh& model, Vec3 viewer, bool inHand) {
    if (!ok(grip) || !ok(tip) || length(tip - grip) < .05f || !metric(model) || !ok(viewer))
        return false;
    const BatonFit fit = batonFit(grip, tip, model, inHand);
    if (!fit.ok || !inRange(fit.at, viewer, 40.f))
        return false;
    const Vec3 f = normalized(tip - grip);
    Vec3 up = Vec3{0, 1, 0} - f * f.y;
    up = length(up) > 1e-3f ? normalized(up) : Vec3{0, 0, 1};
    const Vec3 r = cross(f, up) * fit.across, u = up * fit.across, fz = f * fit.along;
    return drawMesh(out, model, [&](Vec3 c) { return fit.at + r * c.x + u * c.y - fz * c.z; }, viewer);
}

void appendBatonCable(std::vector<Vertex>& out, Vec3 from, Vec3 to, Vec3 viewer, float pixelAngle) {
    if (!ok(from) || !ok(to) || !ok(viewer) || !std::isfinite(pixelAngle) || length(to - from) < .05f ||
        length(to - from) > 40.f || !inRange(from, viewer, 40.f))
        return;
    Guard g(out);
    const float span = length(to - from);
    constexpr int pieces = 8;
    Vec3 last = from;
    for (int i = 1; i <= pieces; ++i) {
        const float t = static_cast<float>(i) / pieces;
        const Vec3 p = from + (to - from) * t - Vec3{0, .06f * span * 4.f * t * (1.f - t), 0};
        line(out, last, p, viewer, pixelAngle, .0022f, steel.base, .95f);
        last = p;
    }
}

void appendGrappleLine(std::vector<Vertex>& out, Vec3 from, Vec3 hook, Vec3 viewer, float pixelAngle, float flown) {
    if (!ok(from) || !ok(hook) || !ok(viewer) || !std::isfinite(pixelAngle) || length(hook - from) < .05f ||
        length(hook - from) > 50.f || !inRange(from, viewer, 50.f))
        return;
    // Version 0.0.72: the line flies out of the muzzle (flown 0-1) with the hook on its tip, then is taut.
    const float f = std::isfinite(flown) ? std::clamp(flown, 0.f, 1.f) : 1.f;
    const Vec3 to = from + (hook - from) * f;
    if (length(to - from) < .05f)
        return;
    Guard g(out);
    // The rope: an 8 mm cord, taut, in ten pieces (each piece keeps its own width floor along a long line).
    constexpr int pieces = 10;
    Vec3 last = from;
    for (int i = 1; i <= pieces; ++i) {
        const Vec3 p = from + (to - from) * (static_cast<float>(i) / pieces);
        part(out, last, p, viewer, pixelAngle, .004f, cord, .98f);
        last = p;
    }
    // The hook: a short steel shank along the line and three prongs bent back from its tip.
    const Vec3 way = normalized(to - from);
    Vec3 side = cross(way, Vec3{0, 1, 0});
    side = length(side) > 1e-3f ? normalized(side) : Vec3{1, 0, 0};
    const Vec3 other = normalized(cross(side, way));
    const Vec3 tail = to - way * .14f;
    part(out, tail, to, viewer, pixelAngle, .012f, steel, .97f);
    for (int k = 0; k < 3; ++k) {
        const float a = 2.f * pi * static_cast<float>(k) / 3.f;
        const Vec3 out1 = side * std::cos(a) + other * std::sin(a);
        part(out, to, to - way * .07f + out1 * .09f, viewer, pixelAngle, .008f, steel, .97f);
    }
}

namespace {
// CAPTAIN AMERICA: the shield Item's disc (its middle, face normal and the in-plane axes, the spin
// applied), from captain.hpp's frame.
struct Disc {
    Vec3 c, n, a, b;
};
bool discOf(const Item& it, Disc& d) {
    Frame k{};
    if (!frameOf(it, k))
        return false;
    const float s = std::isfinite(it.spin) ? it.spin : 0.f;
    d.n = k.f;
    d.a = k.r * std::cos(s) + k.u * std::sin(s);
    if (it.side == 1) // the RIGHT arm's shield is the LEFT's turned half a turn in its plane
        d.a = -d.a;
    d.b = cross(d.n, d.a);
    if (it.look == lookShieldSpin) {
        d.c = k.o;
    } else {
        const Vec3 m = captain::forSide(captain::centreLeft, it.side);
        d.c = k.o + k.r * m.x + k.u * m.y - k.f * m.z;
    }
    return ok(d.c) && ok(d.a) && ok(d.b);
}
} // namespace

void appendShield(std::vector<Vertex>& out, const Item& it, Vec3 viewer, float pixelAngle) {
    Disc d{};
    if (!discOf(it, d) || !ok(viewer) || !std::isfinite(pixelAngle) || !inRange(d.c, viewer, 80.f))
        return;
    Guard g(out);
    const float R = captain::diameter * .5f;
    const float soft = std::max(.004f, .75f * pixelAngle * length(viewer - d.c));
    constexpr int segs = 20;
    if (dot(viewer - d.c, d.n) > 0) { // the face: red, white, red rings, the blue middle, the white star
        const Vec3 red{.55f, .04f, .05f}, white{.86f, .86f, .88f}, blue{.05f, .12f, .45f};
        softArc(out, d.c, d.a, d.b, R * .80f, R, 0, 2 * pi, red, .97f, soft, segs);
        softArc(out, d.c, d.a, d.b, R * .60f, R * .80f, 0, 2 * pi, white, .97f, soft * .5f, segs);
        softArc(out, d.c, d.a, d.b, R * .40f, R * .60f, 0, 2 * pi, red, .97f, soft * .5f, segs);
        softArc(out, d.c, d.a, d.b, 0, R * .40f, 0, 2 * pi, blue, .97f, soft * .5f, segs);
        // The star: five points round a pentagon (its top along -a: the model's -x, upright on the back).
        const float outer = R * .37f, inner = outer * .382f;
        Vec3 tip[5], in[5];
        for (int i = 0; i < 5; ++i) {
            const float t = pi + 2 * pi * static_cast<float>(i) / 5, u = t + pi / 5;
            tip[i] = d.c + (d.a * std::cos(t) + d.b * std::sin(t)) * outer + d.n * .002f;
            in[i] = d.c + (d.a * std::cos(u) + d.b * std::sin(u)) * inner + d.n * .002f;
        }
        for (int i = 0; i < 5; ++i) {
            softTriangle(out, {tip[i], in[i], in[(i + 4) % 5]}, d.n, white, .97f, 0, soft * .5f);
            if (i >= 1 && i <= 3)
                softTriangle(out, {in[0], in[i], in[i + 1]}, d.n, white, .97f, 0, 0);
        }
    } else { // the back: brushed steel, the two leather straps across the forearm's line
        const Vec3 steelBack{.30f, .31f, .33f};
        softArc(out, d.c, d.a, d.b, 0, R, 0, 2 * pi, steelBack, .97f, soft, segs);
        const Vec3 along = cross(d.a, d.n); // in-plane, square to a (the forearm's line before the spin)
        const Vec3 lift = d.n * -.02f;
        for (const float y : {.10f, -.10f})
            part(out, d.c + along * y - d.a * .07f + lift, d.c + along * y + d.a * .07f + lift, viewer, pixelAngle, .012f, leather);
    }
}

namespace {
// The shield model drawn in two layers when it has extras.layer: from the face side the straps
// (behind the disc) first, then the disc; from behind the disc first, then the straps. Within the
// disc, the triangles facing away from the eye go first; each run far to near.
template <class Place>
bool drawLayered(std::vector<Vertex>& out, const mesh_model::Mesh& model, Place place, Vec3 viewer, Vec3 layerCentre, Vec3 layerNormal) {
    if (!model.ok() || model.triangles() > mesh_model::liteTriangles || !model.hasLayer)
        return false;
    thread_local std::vector<std::pair<std::pair<int, float>, uint32_t>> order;
    thread_local std::vector<Vec3> placed;
    const uint32_t n = model.triangles();
    order.clear();
    placed.resize(size_t{n} * 3);
    const bool faceSide = dot(viewer - layerCentre, layerNormal) > 0;
    for (uint32_t t = 0; t < n; ++t) {
        Vec3 sum{};
        for (uint32_t c = 0; c < 3; ++c)
            sum = sum + (placed[t * 3 + c] = place(model.corners[t * 3 + c]));
        if (!ok(sum))
            return false;
        const Vec3 mid = sum * (1.f / 3);
        const bool disc = t < model.layerSplit;
        const Vec3 nrm = cross(placed[t * 3 + 1] - placed[t * 3], placed[t * 3 + 2] - placed[t * 3]);
        const bool away = dot(nrm, viewer - mid) < 0;
        int group = 0;
        if (faceSide)
            group = disc ? (away ? 1 : 2) : 0;
        else
            group = disc ? (away ? 0 : 1) : 2;
        order.push_back({{group, length(mid - viewer)}, t});
    }
    std::sort(order.begin(), order.end(), [](const auto& a, const auto& b) {
        return a.first.first != b.first.first ? a.first.first < b.first.first : a.first.second > b.first.second;
    });
    Guard g(out);
    const Vec3 light = normalized(Vec3{.3f, 1.f, .25f});
    for (const auto& [key, t] : order) {
        (void)key;
        const Vec3 a = placed[t * 3], b = placed[t * 3 + 1], c = placed[t * 3 + 2];
        const Vec3 n3 = cross(b - a, c - a);
        const float len = length(n3);
        if (!(len > 1e-12f))
            continue;
        const float lit = .32f + .68f * std::abs(dot(n3 * (1.f / len), light));
        const Vec3 colour = model.colors[t] * lit;
        for (const Vec3& p : {a, b, c})
            out.push_back(Vertex{p, colour, 1.f, {}});
    }
    return true;
}
} // namespace

bool appendShieldModel(std::vector<Vertex>& out, const Item& it, const mesh_model::Mesh& model, Vec3 viewer) {
    Frame k{};
    if (!metric(model) || !frameOf(it, k) || !ok(viewer) || !inRange(k.o, viewer, 80.f))
        return false;
    const Vec3 pv = model.hasPivot ? model.pivot : captain::pivotLeft;
    const uint32_t side = it.side == 1 ? 1u : 0u;
    const bool spinning = it.look == lookShieldSpin;
    const Vec3 middle = captain::forSide((model.low + model.high) * .5f, side, pv);
    const float s = spinning && std::isfinite(it.spin) ? it.spin : 0.f;
    const float cs = std::cos(s), sn = std::sin(s);
    const auto local = [&](Vec3 c) {
        c = captain::forSide(c, side, pv);
        if (spinning) {
            c = c - middle;
            c = Vec3{cs * c.x - sn * c.y, sn * c.x + cs * c.y, c.z};
        }
        return c;
    };
    const auto place = [&](Vec3 c) {
        const Vec3 l = local(c);
        return k.o + k.r * l.x + k.u * l.y - k.f * l.z;
    };
    if (model.hasLayer && !gModelSink && !model.lite) { // depth-tested or light: no layers needed
        const Vec3 lc = place(model.layerCentre);
        const Vec3 ln = place(model.layerCentre + model.layerNormal * .01f) - lc;
        if (ok(lc) && ok(ln) && length(ln) > 1e-6f)
            return drawLayered(out, model, place, viewer, lc, ln);
    }
    return drawMesh(out, model, place, viewer);
}

void appendClaws(std::vector<Vertex>& out, const Item& it, Vec3 viewer, float pixelAngle) {
    Frame k{};
    if (!frameOf(it, k) || !ok(viewer) || !std::isfinite(pixelAngle) || !inRange(k.o, viewer, 40.f))
        return;
    Guard g(out);
    // Version 0.0.72: from the knuckles, slim, tapering and curving a little down, polished steel;
    // it.spin is the SNIKT extension (hero007::clawExtend; 0 reads as fully out for old callers).
    const float ext = it.spin > 0 && std::isfinite(it.spin) ? std::clamp(it.spin, 0.f, 1.1f) : 1.f;
    const float len = hero007::clawLength * ext;
    if (len < .01f)
        return;
    constexpr int segs = 5;
    for (int i = -1; i <= 1; ++i) {
        const float x = hero007::clawGap * static_cast<float>(i);
        const Vec3 base = k.o + k.f * hero007::clawKnuckle + k.u * hero007::clawRise + k.r * x;
        const Vec3 way = normalized(k.f + k.r * (x * .25f));
        Vec3 from = base;
        for (int s = 1; s <= segs; ++s) {
            const float t = static_cast<float>(s) / segs;
            const float droop = .045f * t * t; // the curve: 4.5 cm down at the tip of 30 cm
            const Vec3 to = base + way * (len * t) - k.u * (droop * ext);
            const float width = .0042f - .0030f * t;
            part(out, from, to, viewer, pixelAngle, width, s == segs ? bright : steel);
            from = to;
        }
    }
}

void appendBatonClub(std::vector<Vertex>& out, Vec3 grip, Vec3 tip, Vec3 viewer, float pixelAngle) {
    if (!ok(grip) || !ok(tip) || !ok(viewer) || !std::isfinite(pixelAngle) || length(tip - grip) < .05f)
        return;
    const Vec3 mid = (grip + tip) * .5f;
    if (!inRange(mid, viewer, 60.f))
        return;
    Guard g(out);
    const Vec3 d = tip - grip, n = normalized(d);
    part(out, grip - n * .012f, grip + n * .008f, viewer, pixelAngle, .02f, steel);           // the pommel cap
    part(out, grip, grip + d * .32f, viewer, pixelAngle, .017f, rubber);                      // the grip wrap
    const Vec3 view = normalized(viewer - mid), side = normalized(cross(d, view));
    for (int i = 1; i <= 5; ++i) { // its ribs
        const Vec3 c = grip + d * (.32f * static_cast<float>(i) / 6.f);
        line(out, c - side * .018f, c + side * .018f, viewer, pixelAngle, .0016f, {.0f, .0f, .0f}, .95f);
    }
    part(out, grip + d * .32f, grip + d * .66f, viewer, pixelAngle, .016f, lacquer);          // the red shaft
    part(out, grip + d * .66f, tip - n * .012f, viewer, pixelAngle, .0185f, lacquer);         // swelling a little
    part(out, tip - n * .015f, tip, viewer, pixelAngle, .019f, steel);                        // the striking cap
}

void appendHolster(std::vector<Vertex>& out, const Item& it, Vec3 viewer, float pixelAngle) {
    Frame k{};
    if (!it.holster || !frameOf(it, k) || !ok(viewer) || !std::isfinite(pixelAngle) || !inRange(k.o, viewer, 40.f))
        return;
    Guard g(out);
    switch (it.holster) {
    case 1: // a hip holster round the pistol's slide and barrel, a belt loop over it
        part(out, k.o + k.u * .055f + k.f * .02f, k.o + k.u * .055f + k.f * .175f, viewer, pixelAngle, .021f, leather);
        line(out, k.o + k.u * .085f + k.f * .03f, k.o + k.u * .085f + k.f * .07f, viewer, pixelAngle, .006f,
             leather.rim, .97f);
        break;
    case 4: { // Version 0.0.72 DROP-LEG HOLSTER: the pistol's sheath down the outside of the thigh, its
              // belt drop above and two leg straps round the front of the thigh (side: 0 left, 1 right)
        const Vec3 in = k.r * (it.side ? -1.f : 1.f); // toward the leg's middle
        part(out, k.o + k.u * .055f + k.f * .02f, k.o + k.u * .055f + k.f * .19f, viewer, pixelAngle, .023f, leather);
        part(out, k.o + k.u * .02f - k.f * .03f, k.o + k.u * .03f - k.f * .14f, viewer, pixelAngle, .012f, leather); // the drop
        for (int b = 0; b < 2; ++b) {
            const Vec3 c = k.o + k.u * .05f + k.f * (b ? .17f : .07f);
            line(out, c, c + in * .07f + k.u * .05f, viewer, pixelAngle, .006f, leather.rim, .97f);
            line(out, c + in * .07f + k.u * .05f, c + in * .14f + k.u * .03f, viewer, pixelAngle, .006f, leather.rim, .97f);
        }
        break;
    }
    case 2: // a back sheath over the barrels
        part(out, k.o + k.f * .12f, k.o + k.f * .40f, viewer, pixelAngle, .028f, leather);
        break;
    default: // a baton tube
        part(out, k.o + k.f * .15f, k.o + k.f * .45f, viewer, pixelAngle, .024f, leather);
        break;
    }
}

namespace {
void heatBar(std::vector<Vertex>& out, Vec3 p, Vec3 q, Vec3 viewer, float pixelAngle, float width, float a) {
    const Vec3 mid = (p + q) * .5f;
    const float range = std::max(length(mid - viewer), .05f);
    const Vec3 view = normalized(viewer - mid);
    const float floor = 1.2f * pixelAngle * range;
    softBar(out, p, q, view, std::max(width, floor * 2), {1.f, .12f, .04f}, .3f * a, std::max(width, floor * 2));
    softBar(out, p, q, view, std::max(width * .4f, floor), {1.f, .42f, .18f}, .75f * a, std::max(width * .3f, floor));
    softBar(out, p, q, view, std::max(width * .14f, floor * .6f), {1.f, .92f, .75f}, a, floor * .5f);
}
} // namespace

void appendHeatBeam(std::vector<Vertex>& out, Vec3 from, Vec3 to, Vec3 viewer, float pixelAngle, float fade) {
    if (!ok(from) || !ok(to) || !ok(viewer) || !std::isfinite(pixelAngle) || !std::isfinite(fade))
        return;
    const float a = std::clamp(fade, 0.f, 1.f);
    if (a < .01f || length(to - from) < .1f)
        return;
    Guard g(out);
    constexpr int pieces = 4; // near pieces turn to the eye on their own
    for (int i = 0; i < pieces; ++i) {
        const float t0 = static_cast<float>(i) / pieces, t1 = static_cast<float>(i + 1) / pieces;
        // Version 0.0.72 THIN BEAMS (0.0.71 videos: thick, widening): one even thin beam from each eye.
        heatBar(out, from + (to - from) * t0, from + (to - from) * t1, viewer, pixelAngle, heatBeamWidth, a);
        (void)t0;
    }
    glob(out, to, viewer, .06f, {1.f, .55f, .2f}, .7f * a); // a small hot point where it lands (sparks: appendHeatSparks)
}

namespace {
// A repeatable 0-1 hash (the sparks' flicker without a random state).
float hash01(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return static_cast<float>(x & 0xffffffu) / static_cast<float>(0x1000000u);
}
} // namespace

void appendHeatSparks(std::vector<Vertex>& out, Vec3 from, Vec3 at, float clock, Vec3 viewer, float pixelAngle) {
    if (!ok(from) || !ok(at) || !ok(viewer) || !std::isfinite(pixelAngle) || !std::isfinite(clock) ||
        length(at - from) < .1f || !inRange(at, viewer, 80.f))
        return;
    Guard g(out);
    const Vec3 back = normalized(from - at);
    Vec3 s = cross(back, Vec3{0, 1, 0});
    s = length(s) > 1e-3f ? normalized(s) : Vec3{1, 0, 0};
    const Vec3 u = normalized(cross(s, back));
    const uint32_t tick = static_cast<uint32_t>(std::max(clock, 0.f) * 30.f); // a new spray 30 times a second
    constexpr int sparks = 7;
    for (int k = 0; k < sparks; ++k) {
        const uint32_t h = tick * 131u + static_cast<uint32_t>(k) * 7919u;
        const float ang = 6.2831853f * hash01(h), lift = hash01(h + 1), len = .05f + .12f * hash01(h + 2);
        // thrown back out of the burn and to the sides, falling a little
        const Vec3 way = normalized(back * (.4f + .6f * lift) + (s * std::cos(ang) + u * std::sin(ang)) * .9f);
        const Vec3 p = at + way * (.02f + .05f * hash01(h + 3));
        line(out, p, p + way * len - Vec3{0, .02f, 0}, viewer, pixelAngle, .0016f, {1.f, .78f, .35f}, .95f);
    }
    glob(out, at, viewer, .05f + .03f * hash01(tick), {1.f, .85f, .5f}, .8f);
}

void appendScorch(std::vector<Vertex>& out, Vec3 at, float age, Vec3 viewer) {
    if (!ok(at) || !ok(viewer) || !std::isfinite(age) || age < 0 || age >= scorchLife || !inRange(at, viewer, 60.f))
        return;
    Guard g(out);
    const float t = age / scorchLife;
    glob(out, at, viewer, .09f, {.05f, .03f, .02f}, .65f * (1.f - t * t));            // the burn
    glob(out, at, viewer, .035f, {1.f, .35f, .06f}, .8f * std::max(0.f, 1.f - 2.5f * t)); // its ember, fading first
}

void appendMuzzleFlash(std::vector<Vertex>& out, Vec3 at, Vec3 way, bool shotgun, float age, Vec3 viewer, float pixelAngle) {
    if (!ok(at) || !ok(way) || length(way) < 1e-3f || !ok(viewer) || !std::isfinite(pixelAngle) || !std::isfinite(age) ||
        age < 0 || age >= flashLife || !inRange(at, viewer, 40.f))
        return;
    Guard g(out);
    const float a = 1.f - age / flashLife, size = shotgun ? 1.6f : 1.f;
    const Vec3 f = normalized(way);
    Vec3 s = cross(f, Vec3{0, 1, 0});
    s = length(s) > 1e-3f ? normalized(s) : Vec3{1, 0, 0};
    const Vec3 u = normalized(cross(s, f));
    glob(out, at + f * (.03f * size), viewer, .045f * size, {1.f, .80f, .40f}, .9f * a);  // the fireball
    glob(out, at + f * (.015f * size), viewer, .02f * size, {1.f, .97f, .85f}, a);        // its white core
    line(out, at, at + f * (.13f * size), viewer, pixelAngle, .012f * size, {1.f, .65f, .25f}, .8f * a); // the jet
    for (int k = 0; k < 4; ++k) { // four side spikes (the muzzle's star)
        const float ang = 1.5707963f * static_cast<float>(k) + .785f;
        const Vec3 d = s * std::cos(ang) + u * std::sin(ang);
        line(out, at + f * .01f, at + f * (.04f * size) + d * (.05f * size), viewer, pixelAngle, .005f * size,
             {1.f, .72f, .3f}, .7f * a);
    }
}

void appendImpact(std::vector<Vertex>& out, Vec3 at, Vec3 way, float age, Vec3 viewer, float pixelAngle) {
    if (!ok(at) || !ok(way) || length(way) < 1e-3f || !ok(viewer) || !std::isfinite(pixelAngle) || !std::isfinite(age) ||
        age < 0 || age >= impactLife || !inRange(at, viewer, 60.f))
        return;
    Guard g(out);
    const float t = age / impactLife, a = 1.f - t;
    const Vec3 back = -normalized(way);
    Vec3 s = cross(back, Vec3{0, 1, 0});
    s = length(s) > 1e-3f ? normalized(s) : Vec3{1, 0, 0};
    const Vec3 u = normalized(cross(s, back));
    glob(out, at, viewer, .04f + .06f * t, {.55f, .52f, .48f}, .6f * a); // the dust / cloth puff
    glob(out, at, viewer, .025f, {1.f, .85f, .55f}, a * std::max(0.f, 1.f - 4.f * t)); // the hit's spark
    for (int k = 0; k < 5; ++k) { // the spray back out of the hit
        const float ang = 1.2566371f * static_cast<float>(k);
        const Vec3 d = normalized(back + (s * std::cos(ang) + u * std::sin(ang)) * .7f);
        line(out, at + d * (.02f + .12f * t), at + d * (.05f + .16f * t), viewer, pixelAngle, .003f, {.60f, .58f, .55f}, .7f * a);
    }
}

void appendFreezeBreath(std::vector<Vertex>& out, Vec3 from, Vec3 to, Vec3 viewer, float pixelAngle, float fade) {
    if (!ok(from) || !ok(to) || !ok(viewer) || !std::isfinite(pixelAngle) || !std::isfinite(fade))
        return;
    const float a = std::clamp(fade, 0.f, 1.f);
    if (a < .01f || length(to - from) < .1f)
        return;
    Guard g(out);
    constexpr int puffs = 10; // a frost cone widening from the mouth
    for (int i = 0; i < puffs; ++i) {
        const float t = (static_cast<float>(i) + .5f) / puffs;
        const Vec3 c = from + (to - from) * t;
        if (length(c - viewer) < .15f)
            continue;
        glob(out, c, viewer, .04f + .6f * t, {.70f, .90f, 1.f}, .28f * a * (1.f - .5f * t));
    }
    line(out, from + (to - from) * .1f, to, viewer, pixelAngle, .01f, {.92f, .98f, 1.f}, .5f * a);
    glob(out, to, viewer, .5f, {.85f, .95f, 1.f}, .45f * a); // frost where it lands
}

void appendSmoke(std::vector<Vertex>& out, Vec3 at, float age, Vec3 viewer) {
    constexpr float seconds = 1.6f; // hero007::smokeSeconds
    if (!ok(at) || !ok(viewer) || !std::isfinite(age) || age < 0 || age >= seconds || !inRange(at, viewer, 80.f))
        return;
    Guard g(out);
    const float t = age / seconds, a = .55f * (1.f - t) * std::min(1.f, age * 8.f + .2f);
    const float r = .3f + .7f * t;
    glob(out, at + Vec3{0, .5f * t, 0}, viewer, r, {.50f, .51f, .53f}, a);
    glob(out, at + Vec3{.25f * r, .3f * t, 0}, viewer, r * .7f, {.62f, .63f, .65f}, a * .8f);
    glob(out, at + Vec3{-.25f * r, .2f * t, .1f * r}, viewer, r * .6f, {.42f, .43f, .45f}, a * .8f);
}

void appendPellet(std::vector<Vertex>& out, Vec3 at, Vec3 viewer) {
    if (!ok(at) || !ok(viewer) || !inRange(at, viewer, 40.f))
        return;
    Guard g(out);
    glob(out, at, viewer, .022f, {.16f, .17f, .19f}, .97f);
    glob(out, at + (normalized(viewer - at) + Vec3{0, 1, 0}) * .006f, viewer, .008f, {.55f, .57f, .6f}, .6f);
}

void appendShell(std::vector<Vertex>& out, Vec3 at, Vec3 way, bool shotgun, float age, Vec3 viewer, float pixelAngle) {
    constexpr float seconds = 1.2f; // hero007::shellSeconds
    if (!ok(at) || !ok(way) || !ok(viewer) || !std::isfinite(pixelAngle) || !std::isfinite(age) || age < 0 ||
        age >= seconds || !inRange(at, viewer, 20.f))
        return;
    Guard g(out);
    // Tumbling: the case's axis turns with its age.
    const Vec3 s1 = perpendicular(length(way) > 1e-3f ? normalized(way) : Vec3{0, 1, 0});
    const Vec3 s2 = cross(length(way) > 1e-3f ? normalized(way) : Vec3{0, 1, 0}, s1);
    const float spin = age * 25.f;
    const Vec3 axis = s1 * std::cos(spin) + s2 * std::sin(spin);
    const float a = .97f * std::min(1.f, (seconds - age) * 5.f);
    if (shotgun) {
        part(out, at - axis * .03f, at + axis * .022f, viewer, pixelAngle, .009f, shellRed, a);
        line(out, at + axis * .022f, at + axis * .03f, viewer, pixelAngle, .0095f, brass.base, a);
    } else {
        part(out, at - axis * .0095f, at + axis * .0095f, viewer, pixelAngle, .0045f, brass, a);
    }
}

void appendAmmo(std::vector<Vertex>& out, const char* text, const Item& gun, Vec3 viewer) {
    Frame k{};
    if (!text || !text[0] || !frameOf(gun, k) || !ok(viewer))
        return;
    const Vec3 origin = k.o + k.u * .095f - k.f * .03f;
    if (!inRange(origin, viewer, 3.f)) // a wrist-distance readout: unreadable further away anyway
        return;
    Guard g(out);
    const Vec3 v = normalized(viewer - origin);
    Vec3 up = k.u - v * dot(k.u, v);
    if (length(up) < 1e-3f)
        up = perpendicular(v);
    up = normalized(up);
    const Vec3 right = normalized(cross(up, v));
    PixelTextStyle style;
    style.height = .012f;
    const bool warn = !(text[0] >= '0' && text[0] <= '9') || std::strncmp(text, "0/", 2) == 0;
    style.top = warn ? Vec3{1.f, .35f, .25f} : Vec3{.55f, 1.f, .6f};
    style.bottom = warn ? Vec3{.8f, .15f, .1f} : Vec3{.25f, .8f, .35f};
    const float w = pixelTextWidth(text, style);
    appendPixelText(out, text, origin - right * (w * .5f), right, up, style);
}

void appendItem(std::vector<Vertex>& out, const Item& it, Vec3 viewer, float pixelAngle) {
    switch (it.look) {
    case lookPistol: appendPistol(out, it, viewer, pixelAngle); break;
    case lookSawedOff: appendSawedOff(out, it, false, viewer, pixelAngle); break;
    case lookSawedOffOpen: appendSawedOff(out, it, true, viewer, pixelAngle); break;
    case lookBatarang: appendBatarang(out, it, viewer, pixelAngle); break;
    case lookClaws: appendClaws(out, it, viewer, pixelAngle); break;
    case lookClub:
        if (ok(it.forward) && length(it.forward) > 1e-3f)
            appendBatonClub(out, it.at, it.at + normalized(it.forward) * .55f, viewer, pixelAngle);
        break;
    case lookBaton062:
        if (ok(it.forward) && length(it.forward) > 1e-3f)
            appendBaton(out, it.at, it.at + normalized(it.forward) * .55f, viewer, pixelAngle);
        break;
    case lookShield:
    case lookShieldSpin: appendShield(out, it, viewer, pixelAngle); break;
    case lookBatonModel: // no model given here: CLUB
        if (ok(it.forward) && length(it.forward) > 1e-3f)
            appendBatonClub(out, it.at, it.at + normalized(it.forward) * .55f, viewer, pixelAngle);
        break;
    default: break;
    }
    if (it.holster)
        appendHolster(out, it, viewer, pixelAngle);
}

void appendHero007(std::vector<Vertex>& out, const Draw& d, Vec3 travel, Vec3 viewer, float pixelAngle) {
    if (!d.any || !ok(travel) || !ok(viewer) || !std::isfinite(pixelAngle))
        return;
    const auto moved = [&](Item it) {
        it.at = it.at + travel;
        return it;
    };
    // BATARANG STYLE / GUN MODELS / BATON LOOK MODEL: the model in the built-in look's place; the
    // built-in look when it cannot be drawn.
    // Version 0.0.7 GUN GRIP: a palm Item's at is the grip point; the look (model or BUILT-IN) is
    // moved so its own grip middle (gun_grip::palmPoint) lands there.
    const auto inPalm = [&](Item it, bool model) {
        if (!it.palm || (it.look != lookPistol && it.look != lookSawedOff && it.look != lookSawedOffOpen))
            return it;
        Frame k{};
        if (!frameOf(it, k))
            return it;
        const auto w = it.look == lookPistol ? gun_grip::pistol : gun_grip::sawedOff;
        it.at = gun_grip::originFor(k.o, k.f, k.u, gun_grip::palmPoint(w, model));
        return it;
    };
    const auto item = [&](const Item& given) {
        bool drawn = false;
        if (given.look == lookGrappleGun) { // the model or nothing (no built-in grapple gun, no empty holster)
            if (d.grappleGunModel && appendModel(out, given, *d.grappleGunModel, viewer) && given.holster)
                appendHolster(out, given, viewer, pixelAngle);
            return;
        }
        const Item it = inPalm(given, false);
        if (it.look == lookBatarang && d.batarangModel)
            drawn = appendBatarangModel(out, it, *d.batarangModel, viewer);
        else if (it.look == lookPistol && d.pistolModel)
            drawn = appendModel(out, inPalm(given, true), *d.pistolModel, viewer);
        else if ((it.look == lookSawedOff || it.look == lookSawedOffOpen) && d.sawedFrame && d.sawedBarrels)
            drawn = appendSawedOffModel(out, inPalm(given, true), *d.sawedFrame, *d.sawedBarrels, it.look == lookSawedOffOpen, viewer);
        else if (it.look == lookBatonModel && it.side < 2 && d.batonModel[it.side] && ok(it.forward) && length(it.forward) > 1e-3f)
            drawn = appendBatonModel(out, it.at, it.at + normalized(it.forward) * .57f, *d.batonModel[it.side], viewer);
        else if ((it.look == lookShield || it.look == lookShieldSpin) && d.shieldModel)
            drawn = appendShieldModel(out, it, *d.shieldModel, viewer);
        if (!drawn && noStandIn(d.modelWanted, it.look)) {
            // Version 0.0.72 MODEL MISSING: no fake lookalike (the wrist names the missing model).
        } else if (!drawn) {
            appendItem(out, it, viewer, pixelAngle);
        } else if (it.holster) {
            appendHolster(out, it, viewer, pixelAngle);
        }
    };
    for (const Item& it : d.worn)
        if (it.look || it.holster)
            item(moved(it));
    for (const Item& it : d.flying)
        if (it.look)
            item(moved(it));
    for (int i = 0; i < static_cast<int>(drawSmokes); ++i)
        appendSmoke(out, d.smokeAt[i] + travel, d.smokeAge[i], viewer);
    for (int i = 0; i < 2; ++i)
        if (d.pellet[i])
            appendPellet(out, d.pelletAt[i] + travel, viewer);
    for (uint32_t i = 0; i < std::min(d.shells, drawShells); ++i)
        appendShell(out, d.shellAt[i] + travel, d.shellWay[i], d.shellShotgun[i], d.shellAge[i], viewer, pixelAngle);
    if (d.beamFade > 0) {
        if (d.beamFreeze)
            appendFreezeBreath(out, d.beamFrom[0] + travel, d.beamTo + travel, viewer, pixelAngle, d.beamFade);
        else
            for (const Vec3& eye : d.beamFrom)
                appendHeatBeam(out, eye + travel, d.beamTo + travel, viewer, pixelAngle, d.beamFade);
        if (!d.beamFreeze && d.beamHit && d.beamFade > .5f) // Version 0.0.72: the hit sparks
            appendHeatSparks(out, (d.beamFrom[0] + d.beamFrom[1]) * .5f + travel, d.beamTo + travel, d.beamClock, viewer,
                             pixelAngle);
    }
    for (uint32_t i = 0; i < drawScorches; ++i) // Version 0.0.72 scorch marks, PUNISHER flashes and hits
        appendScorch(out, d.scorchAt[i] + travel, d.scorchAge[i], viewer);
    for (int i = 0; i < 2; ++i)
        appendMuzzleFlash(out, d.flashAt[i] + travel, d.flashWay[i], d.flashShotgun[i], d.flashAge[i], viewer, pixelAngle);
    for (uint32_t i = 0; i < drawImpacts; ++i)
        appendImpact(out, d.impactAt[i] + travel, d.impactWay[i], d.impactAge[i], viewer, pixelAngle);
    for (int i = 0; i < 2; ++i)
        if (d.cable[i])
            appendBatonCable(out, d.cableFrom[i] + travel, d.cableTo[i] + travel, viewer, pixelAngle);
    for (int i = 0; i < 2; ++i)
        if (d.grapple[i]) {
            // From the gun's muzzle when the gun model is in that hand; the hand otherwise.
            Vec3 from = d.grappleFrom[i] + travel;
            Vec3 muzzle{};
            if (d.held[i].look == lookGrappleGun && d.grappleGunModel && grappleMuzzle(moved(d.held[i]), *d.grappleGunModel, muzzle))
                from = muzzle;
            appendGrappleLine(out, from, d.grappleTo[i] + travel, viewer, pixelAngle, d.grappleOut[i]);
        }
    for (int i = 0; i < 2; ++i) {
        if (!d.held[i].look)
            continue;
        const Item it = moved(d.held[i]);
        item(it);
        appendAmmo(out, d.ammo[i], it, viewer);
    }
}
} // namespace spidy::hero007
