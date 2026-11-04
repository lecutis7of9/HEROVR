// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
// HERO VR 0.0.77 WAYPOINT: the diamond and the edge arrow (include/spidy/waypoint_mark.hpp).
#include "spidy/waypoint_mark.hpp"
#include "spidy/overlay_text.hpp"
#include <algorithm>
#include <cmath>

namespace spidy::waypoint_mark {
namespace {
bool good(Vec3 centre, Vec3 right, Vec3 up, float size, float alpha) {
    return finite(centre) && finite(right) && finite(up) && std::isfinite(size) && size > 0 && std::isfinite(alpha) &&
           alpha > 0;
}
void tri(std::vector<Vertex>& out, Vec3 a, Vec3 b, Vec3 c, Vec3 color, float alpha) {
    out.push_back(Vertex{a, color, alpha});
    out.push_back(Vertex{b, color, alpha});
    out.push_back(Vertex{c, color, alpha});
}
const Vec3 yellow{1.f, .82f, .08f}, dark{.05f, .04f, .01f};
} // namespace
size_t diamond(std::vector<Vertex>& out, std::string_view text, Vec3 centre, Vec3 right, Vec3 up, float size,
               float alpha) {
    if (!good(centre, right, up, size, alpha))
        return 0;
    const float a = std::min(alpha, 1.f);
    const size_t from = out.size();
    const Vec3 toward = cross(right, up);
    const auto P = [&](float x, float y, float z) { return centre + right * x + up * y + toward * z; };
    // A dark rim, then the yellow diamond over it.
    const float r = size * 1.22f, z = size * .02f;
    tri(out, P(0, r, 0), P(-r * .7f, 0, 0), P(r * .7f, 0, 0), dark, .7f * a);
    tri(out, P(0, -r, 0), P(r * .7f, 0, 0), P(-r * .7f, 0, 0), dark, .7f * a);
    const float s = size;
    tri(out, P(0, s, z), P(-s * .7f, 0, z), P(s * .7f, 0, z), yellow, .95f * a);
    tri(out, P(0, -s, z), P(s * .7f, 0, z), P(-s * .7f, 0, z), yellow * .8f, .95f * a);
    if (!text.empty()) {
        TextStyle ts;
        ts.round = true;
        ts.height = size * .55f;
        ts.soft = ts.height * .04f;
        ts.color = yellow;
        const size_t mark = out.size();
        appendText(out, text, P(-textWidth(text, ts.height) / 2, -r - size * .25f - ts.height, 2 * z), right, up, ts);
        for (size_t i = mark; i < out.size(); ++i)
            out[i].alpha *= a;
    }
    return out.size() - from;
}
size_t arrow(std::vector<Vertex>& out, Vec3 centre, Vec3 right, Vec3 up, float x, float y, float size, float alpha) {
    if (!good(centre, right, up, size, alpha) || !std::isfinite(x) || !std::isfinite(y))
        return 0;
    const float l = std::sqrt(x * x + y * y);
    if (!(l > 1e-4f))
        return 0;
    x /= l, y /= l;
    const float a = std::min(alpha, 1.f);
    const Vec3 way = right * x + up * y, side = right * -y + up * x;
    const size_t from = out.size();
    const Vec3 tip = centre + way * size, back = centre - way * size * .6f;
    tri(out, centre + way * size * 1.25f, back + side * size * 1.1f, back - side * size * 1.1f, dark, .7f * a);
    tri(out, tip, back + side * size * .85f, back - side * size * .85f, yellow, .95f * a);
    return out.size() - from;
}
} // namespace spidy::waypoint_mark
