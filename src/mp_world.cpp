// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
// Made by Agent Mulder.
// 0.0.76 MP WORLD: see include/spidy/mp_world.hpp.
#include "spidy/mp_world.hpp"

#include "spidy/soft_shapes.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace spidy::mp {
namespace {
bool finiteV(Vec3 v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }
} // namespace

bool useWorld(uint32_t place, bool peerGame, bool buddy, const PoseState& s) {
    return place == placeWorld && peerGame && buddy && s.hasWorld && s.worldValid && finiteV(s.root) &&
           std::isfinite(s.worldYaw);
}

float bodyAlpha(float distance, const WorldTune& t) {
    if (!std::isfinite(distance) || distance < 0)
        return 0;
    if (distance <= t.bodyFull)
        return 1;
    if (distance >= t.bodyGone || t.bodyGone <= t.bodyFull)
        return 0;
    return 1.f - (distance - t.bodyFull) / (t.bodyGone - t.bodyFull);
}

std::string distanceText(float metres) {
    if (!std::isfinite(metres) || metres < 0)
        return "--";
    char b[32];
    if (metres < 1000.f)
        std::snprintf(b, sizeof b, "%d M", static_cast<int>(std::lround(metres)));
    else
        std::snprintf(b, sizeof b, "%.1f KM", std::min(metres, 999000.f) / 1000.f);
    return b;
}

std::string worldNameplate(const std::string& base, float metres) { return base + "  " + distanceText(metres); }

Vec3 tagSpot(Vec3 viewer, Vec3 above, const WorldTune& t) {
    if (!finiteV(viewer) || !finiteV(above))
        return above;
    const Vec3 d = above - viewer;
    const float n = length(d);
    if (!(n > t.tagNear) || !(t.tagRange > 0))
        return above;
    return viewer + d * (t.tagRange / n);
}

Arrow offscreenArrow(Vec3 viewer, Vec3 forward, Vec3 up, Vec3 target, const WorldTune& t) {
    Arrow a;
    if (!finiteV(viewer) || !finiteV(forward) || !finiteV(up) || !finiteV(target))
        return a;
    const Vec3 f = normalized(forward);
    Vec3 to = target - viewer;
    const float n = length(to);
    if (!(n > 1e-3f) || !(length(f) > .5f))
        return a;
    to = to * (1.f / n);
    const float off = std::acos(std::clamp(dot(f, to), -1.f, 1.f));
    if (!(off > t.arrowCone))
        return a;
    // The way to him across the view (behind: still the side he is on; straight behind: down... to the right).
    Vec3 u = up - f * dot(up, f);
    u = length(u) > 1e-3f ? normalized(u) : soft::perpendicular(f);
    const Vec3 r = normalized(cross(f, u));
    Vec3 across = r * dot(to, r) + u * dot(to, u);
    if (!(length(across) > 1e-4f))
        across = r;
    across = normalized(across);
    a.show = true;
    a.way = across;
    a.face = -f;
    a.at = viewer + f * t.arrowAhead + across * t.arrowOut;
    return a;
}

size_t appendArrow(std::vector<Vertex>& out, const Arrow& a, float size, Vec3 color, float alpha) {
    if (!a.show || !(alpha > 0) || !(size > 0) || !finiteV(a.at) || !finiteV(a.way) || !finiteV(a.face))
        return 0;
    const size_t before = out.size();
    const Vec3 w = normalized(a.way);
    const Vec3 side = normalized(cross(a.face, w));
    if (!finiteV(side) || length(side) < .5f)
        return 0;
    const float h = size * .5f, thick = size * .18f;
    // A chevron: two bars meeting at the tip.
    const Vec3 tip = a.at + w * h;
    for (float s : {1.f, -1.f}) {
        const Vec3 tail = a.at - w * h + side * (s * h);
        const Vec3 along = normalized(tip - tail);
        const Vec3 across = normalized(cross(a.face, along)) * (thick * .5f);
        const Vertex p{tail - across, color, alpha}, q{tail + across, color, alpha}, r{tip + across, color, alpha},
            o{tip - across, color, alpha};
        soft::quad(out, p, q, r, o);
    }
    return out.size() - before;
}
} // namespace spidy::mp
