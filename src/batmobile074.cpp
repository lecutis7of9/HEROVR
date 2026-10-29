// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
// Made by Agent Mulder.
// Version 0.0.74 test 1 BATMOBILE (batmobile074.hpp).
#include "spidy/batmobile074.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace spidy::batmobile074 {

namespace {
// MEASURED by tools/batmobile074/convert.py (each car's layout.json); exit x = -(width / 2 + .75).
constexpr Layout table[cars] = {
    {6.398f, 2.538f, 1.417f, {0, .092f, .0644f}, {-.37f, 1.112f, .2444f}, {-.36f, .832f, -.2656f}, {0, .342f, .94f},
     {-.665f, .692f, -.1556f}, {-.5993f, .892f, -.0356f}, {-2.019f, 0, .2444f}, {0, .782f, -.5161f}, true},
    {6.077f, 2.423f, 1.409f, {0, .03f, 0}, {-.37f, 1.05f, .18f}, {-.36f, .77f, -.33f}, {0, .342f, .94f},
     {-.665f, .63f, -.22f}, {-.7041f, .83f, -.1f}, {-1.962f, 0, .18f}, {0, .72f, -.5805f}, true},
    {6.506f, 2.117f, 2.475f, {0, .444f, -.407f}, {-.37f, 1.464f, -.227f}, {-.36f, 1.184f, -.737f}, {0, .342f, .94f},
     {-.665f, 1.044f, -.627f}, {-.5439f, 1.244f, -.507f}, {-1.809f, 0, -.227f}, {0, 1.134f, -.9875f}, true},
    {6.381f, 2.338f, 2.859f, {0, .1344f, .084f}, {-.37f, 1.1544f, .264f}, {-.36f, .8744f, -.246f}, {0, .342f, .94f},
     {-.665f, .7344f, -.136f}, {-1.2072f, .9344f, -.016f}, {-1.919f, 0, .264f}, {0, .8244f, -.4965f}, false},
};
bool okv(Vec3 v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }
} // namespace

const Layout& layout(uint32_t car) { return table[clampCar(car)]; }

van072::Tune tune(uint32_t car) {
    const Layout& l = layout(car);
    van072::Tune t{};
    t.handleOut = l.handleOut;
    t.handleIn = l.handleIn;
    t.eye = l.eye;
    t.hub = l.hub;
    t.axis = l.axis;
    t.exit = l.exit;
    t.length = l.length;
    t.width = l.width;
    // the box the summon and the bumper check: the body to its roof (FOREVER's fins included)
    t.height = l.height;
    t.wheelBase = l.length * .56f;
    t.wheelRadius = .185f;
    return t;
}

bool appendGlass(std::vector<Vertex>& out, const Glass& g, Vec3 viewer) {
    if (!g.on || !g.mesh || !g.mesh->ok() || g.mesh->triangles() > mesh_model::drawTriangles || !okv(g.at) || !okv(g.forward) ||
        !okv(viewer) || length(g.at - viewer) > 80.f)
        return false;
    Vec3 f = g.forward;
    f.y = 0;
    if (length(f) < 1e-4f)
        return false;
    f = normalized(f);
    const Vec3 u{0, 1, 0}, r = cross(f, u);
    const auto place = [&](Vec3 c) { return g.at + r * c.x + u * c.y - f * c.z; };
    const size_t from = out.size();
    try {
        thread_local std::vector<std::pair<float, uint32_t>> order;
        const uint32_t n = g.mesh->triangles();
        order.clear();
        order.reserve(n);
        for (uint32_t t = 0; t < n; ++t) {
            const Vec3 mid = place((g.mesh->corners[t * 3] + g.mesh->corners[t * 3 + 1] + g.mesh->corners[t * 3 + 2]) * (1.f / 3));
            if (!okv(mid))
                return false;
            order.emplace_back(length(mid - viewer), t);
        }
        std::sort(order.begin(), order.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
        const Vec3 light = normalized(Vec3{.3f, 1.f, .25f});
        for (const auto& [far, t] : order) {
            (void)far;
            const Vec3 a = place(g.mesh->corners[t * 3]), b = place(g.mesh->corners[t * 3 + 1]), c = place(g.mesh->corners[t * 3 + 2]);
            const Vec3 n3 = cross(b - a, c - a);
            const float len = length(n3);
            if (!(len > 1e-12f))
                continue;
            // a faint sheen where the glass faces the sky (both sides), the tint otherwise
            const float sheen = .5f + .5f * std::abs(dot(n3 * (1.f / len), light));
            const Vec3 colour = glassTint * sheen;
            for (const Vec3& p : {a, b, c})
                out.push_back(Vertex{p, colour, glassAlpha, {}});
        }
        return out.size() > from;
    } catch (...) {
        out.resize(from);
        return false;
    }
}

} // namespace spidy::batmobile074
