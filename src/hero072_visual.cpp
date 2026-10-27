// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
// Made by Agent Mulder.
// Version 0.0.72 (TEST) looks (hero072_visual.hpp).
#include "spidy/hero072_visual.hpp"
#include "spidy/hero072.hpp"
#include "spidy/pixel_font.hpp"
#include "spidy/soft_shapes.hpp"
#include "spidy/web_visual.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace spidy::hero072 {
namespace {
using soft::perpendicular;
using soft::softArc;
using soft::softBar;
constexpr float pi = 3.14159265f;
const Vec3 green{.15f, 1.f, .35f}, greenDeep{.02f, .45f, .12f}, boltBlue{.65f, .8f, 1.f}, boltCore{1.f, 1.f, 1.f};
const Vec3 hudGold{1.f, .78f, .3f}, hudRed{1.f, .25f, .15f};

bool okv(Vec3 v) { return finite(v); }
bool inRange(Vec3 at, Vec3 viewer, float most) {
    const float d = length(at - viewer);
    return std::isfinite(d) && d >= .05f && d <= most;
}
struct VGuard {
    std::vector<Vertex>& out;
    size_t from;
    explicit VGuard(std::vector<Vertex>& o) : out(o), from(o.size()) {}
    ~VGuard() {
        if (!web_style::sane(out, from))
            out.resize(from);
    }
};
Vec3 viewOf(Vec3 p, Vec3 viewer) { return normalized(viewer - p); }
// A bar whose width never falls under ~1 pixel.
void bar(std::vector<Vertex>& out, Vec3 p, Vec3 q, Vec3 viewer, float pixelAngle, float hw, Vec3 color, float a) {
    if (length(q - p) < 1e-4f)
        return;
    const Vec3 mid = (p + q) * .5f;
    const float range = std::max(length(viewer - mid), .05f);
    const float w = std::max(hw, .75f * (std::isfinite(pixelAngle) ? pixelAngle : .001f) * range);
    softBar(out, p, q, viewOf(mid, viewer), w, color, a, w * .6f);
}
float hash(float x) {
    const float s = std::sin(x * 12.9898f) * 43758.5453f;
    return s - std::floor(s);
}
} // namespace

void appendHammer(std::vector<Vertex>& out, Vec3 at, Vec3 forward, Vec3 up, Vec3 viewer, float pixelAngle) {
    if (!okv(at) || !okv(forward) || !okv(up) || !okv(viewer) || length(forward) < 1e-3f || !inRange(at, viewer, 60.f))
        return;
    VGuard g(out);
    const Vec3 f = normalized(forward);
    Vec3 u = up - f * dot(up, f);
    u = length(u) > 1e-3f ? normalized(u) : perpendicular(f);
    // The handle: 0.28 m of leather-wrapped wood, a steel pommel and a loop.
    const Vec3 neck = at + f * .30f;
    bar(out, at, neck, viewer, pixelAngle, .019f, {.05f, .03f, .02f}, .97f);
    bar(out, at, neck, viewer, pixelAngle, .015f, {.30f, .19f, .10f}, .97f);
    for (int k = 0; k < 5; ++k) { // the wrap's bands
        const Vec3 b = at + f * (.04f + .05f * k);
        bar(out, b, b + f * .012f, viewer, pixelAngle, .017f, {.16f, .09f, .05f}, .97f);
    }
    bar(out, at - f * .02f, at + f * .01f, viewer, pixelAngle, .022f, {.55f, .58f, .62f}, .97f);
    // The head: a 0.20 m x 0.12 m steel block across the handle's end.
    const Vec3 c = neck + f * .06f;
    bar(out, c - u * .10f, c + u * .10f, viewer, pixelAngle, .066f, {.05f, .05f, .06f}, .97f);
    bar(out, c - u * .095f, c + u * .095f, viewer, pixelAngle, .058f, {.42f, .45f, .50f}, .97f);
    bar(out, c - u * .09f + f * .03f, c + u * .09f + f * .03f, viewer, pixelAngle, .012f, {.85f, .88f, .92f}, .8f);
    // The faces' rims.
    bar(out, c + u * .10f - f * .06f, c + u * .10f + f * .06f, viewer, pixelAngle, .012f, {.25f, .27f, .30f}, .97f);
    bar(out, c - u * .10f - f * .06f, c - u * .10f + f * .06f, viewer, pixelAngle, .012f, {.25f, .27f, .30f}, .97f);
}

void appendBolt(std::vector<Vertex>& out, Vec3 from, Vec3 to, float seed, Vec3 viewer, float pixelAngle) {
    if (!okv(from) || !okv(to) || !okv(viewer) || !std::isfinite(seed) || length(to - from) < .02f ||
        length(to - from) > 30.f || !inRange(from, viewer, 80.f))
        return;
    VGuard g(out);
    constexpr int pieces = 8;
    const Vec3 way = to - from;
    const float span = length(way);
    const Vec3 side = perpendicular(normalized(way)), other = normalized(cross(normalized(way), side));
    Vec3 last = from;
    for (int i = 1; i <= pieces; ++i) {
        const float t = static_cast<float>(i) / pieces;
        const float jag = i == pieces ? 0.f : span * .09f;
        const Vec3 p = from + way * t + side * ((hash(seed + i) - .5f) * 2.f * jag) +
                       other * ((hash(seed * 1.7f + i) - .5f) * 2.f * jag);
        bar(out, last, p, viewer, pixelAngle, .012f, boltBlue, .55f);
        bar(out, last, p, viewer, pixelAngle, .004f, boltCore, .95f);
        last = p;
    }
}

void appendLanternBeam(std::vector<Vertex>& out, Vec3 from, Vec3 to, Vec3 viewer, float pixelAngle) {
    if (!okv(from) || !okv(to) || !okv(viewer) || length(to - from) < .05f || length(to - from) > 60.f ||
        !inRange(from, viewer, 80.f))
        return;
    VGuard g(out);
    constexpr int pieces = 5;
    Vec3 last = from;
    for (int i = 1; i <= pieces; ++i) {
        const Vec3 p = from + (to - from) * (static_cast<float>(i) / pieces);
        bar(out, last, p, viewer, pixelAngle, .05f, greenDeep, .45f);
        bar(out, last, p, viewer, pixelAngle, .022f, green, .9f);
        bar(out, last, p, viewer, pixelAngle, .007f, {.85f, 1.f, .85f}, .95f);
        last = p;
    }
}

void appendBubble(std::vector<Vertex>& out, Vec3 at, float radius, Vec3 viewer) {
    if (!okv(at) || !okv(viewer) || !std::isfinite(radius) || radius < .2f || radius > 5.f)
        return;
    const float d = length(viewer - at);
    if (!std::isfinite(d) || d > 80.f)
        return;
    VGuard g(out);
    const Vec3 v = d > 1e-3f ? (viewer - at) / d : Vec3{0, 0, 1};
    const Vec3 a = perpendicular(v), b = normalized(cross(v, a));
    // Seen from inside (the player in it): a faint tint ring at the edge of the view instead of a rim.
    if (d < radius) {
        softArc(out, at + v * (d * .5f), a, b, radius * .55f, radius * .8f, 0, 2 * pi, green, .10f, radius * .2f, 28);
        return;
    }
    softArc(out, at, a, b, 0, radius * .96f, 0, 2 * pi, green, .07f, radius * .04f, 28);
    softArc(out, at, a, b, radius * .93f, radius, 0, 2 * pi, green, .55f, radius * .03f, 28);
}

void appendChain(std::vector<Vertex>& out, Vec3 from, Vec3 to, Vec3 viewer, float pixelAngle) {
    if (!okv(from) || !okv(to) || !okv(viewer) || length(to - from) < .1f || length(to - from) > 30.f ||
        !inRange(from, viewer, 60.f))
        return;
    VGuard g(out);
    const Vec3 way = normalized(to - from);
    const Vec3 side = perpendicular(way);
    const int links = std::clamp(static_cast<int>(length(to - from) / .12f), 2, 24);
    for (int i = 0; i < links; ++i) {
        const Vec3 p = from + (to - from) * (static_cast<float>(i) / links);
        const Vec3 q = from + (to - from) * (static_cast<float>(i + 1) / links);
        const Vec3 s = side * ((i % 2) ? .018f : -.018f);
        bar(out, p + s, q + s, viewer, pixelAngle, .009f, green, .9f);
    }
    // The cuff on him.
    const Vec3 v = viewOf(to, viewer);
    const Vec3 a = perpendicular(v), b = normalized(cross(v, a));
    softArc(out, to, a, b, .14f, .18f, 0, 2 * pi, green, .85f, .02f, 16);
}

void appendFist(std::vector<Vertex>& out, Vec3 at, Vec3 forward, float alpha, Vec3 viewer, float pixelAngle) {
    if (!okv(at) || !okv(forward) || !okv(viewer) || !std::isfinite(alpha) || alpha <= .01f || length(forward) < 1e-3f ||
        !inRange(at, viewer, 60.f))
        return;
    VGuard g(out);
    const Vec3 f = normalized(forward), s = perpendicular(f), u = normalized(cross(s, f));
    const float a = std::clamp(alpha, 0.f, 1.f);
    // A 0.9 m construct fist: the back of the hand, four knuckles and the thumb, glowing green.
    const Vec3 c = at + f * .35f;
    bar(out, c - f * .25f, c + f * .15f, viewer, pixelAngle, .30f, greenDeep, .35f * a);
    for (int k = 0; k < 4; ++k) {
        const Vec3 k0 = c + f * .15f + s * (-.24f + .16f * k);
        bar(out, k0, k0 + f * .14f, viewer, pixelAngle, .075f, green, .8f * a);
    }
    bar(out, c - s * .30f - f * .05f, c - s * .18f + f * .12f + u * -.08f, viewer, pixelAngle, .06f, green, .8f * a);
    bar(out, c - f * .45f, c - f * .25f, viewer, pixelAngle, .16f, green, .5f * a); // the wrist
}

void appendCharge(std::vector<Vertex>& out, Vec3 at, Vec3 up, float level, bool empty, Vec3 viewer) {
    if (!okv(at) || !okv(up) || !okv(viewer) || !std::isfinite(level) || !inRange(at, viewer, 3.f))
        return;
    VGuard g(out);
    const Vec3 v = viewOf(at, viewer);
    Vec3 u = up - v * dot(up, v);
    u = length(u) > 1e-3f ? normalized(u) : perpendicular(v);
    const Vec3 r = normalized(cross(u, v));
    const float l = std::clamp(level, 0.f, 1.f);
    // The meter: a 9 cm bar, filled to the level, and its readout.
    softBar(out, at - r * .045f, at + r * .045f, v, .006f, {.02f, .08f, .03f}, .8f, .002f);
    if (l > .01f)
        softBar(out, at - r * .045f, at - r * .045f + r * (.09f * l), v, .0045f, empty ? hudRed : green, .95f, .0015f);
    char t[24];
    std::snprintf(t, sizeof t, empty ? "RING EMPTY" : "RING %d%%", static_cast<int>(l * 100.f + .5f));
    PixelTextStyle st;
    st.height = .009f;
    st.top = empty ? hudRed : Vec3{.6f, 1.f, .7f};
    st.bottom = empty ? Vec3{.7f, .1f, .1f} : green;
    const float w = pixelTextWidth(t, st);
    appendPixelText(out, t, at + u * .012f - r * (w * .5f), r, u, st);
}

void appendIronHud(std::vector<Vertex>& out, const Look& l, Vec3 viewer) {
    if (!l.hud || !okv(l.hudEye) || !okv(l.hudForward) || !okv(l.hudUp) || length(l.hudForward) < 1e-3f)
        return;
    VGuard g(out);
    const Vec3 f = normalized(l.hudForward);
    Vec3 u = l.hudUp - f * dot(l.hudUp, f);
    u = length(u) > 1e-3f ? normalized(u) : perpendicular(f);
    const Vec3 r = normalized(cross(f, u));
    constexpr float dist = 1.2f, halfW = .52f, halfH = .36f, corner = .09f;
    const Vec3 c = l.hudEye + f * dist;
    const Vec3 v = -f;
    // The light helmet frame: four corner brackets and a thin top arc, at the edges of the view.
    for (int sx = -1; sx <= 1; sx += 2)
        for (int sy = -1; sy <= 1; sy += 2) {
            const Vec3 k = c + r * (sx * halfW) + u * (sy * halfH);
            softBar(out, k, k - r * (sx * corner), v, .0022f, hudGold, .45f, .0015f);
            softBar(out, k, k - u * (sy * corner), v, .0022f, hudGold, .45f, .0015f);
        }
    softArc(out, c + u * (halfH * -.2f), r, u, halfW * .98f, halfW * 1.f, pi * .3f, pi * .7f, hudGold, .25f, .002f, 24);
    // The lines: centred, under the top edge.
    PixelTextStyle st;
    st.height = .016f;
    float y = halfH - .05f;
    for (uint32_t i = 0; i < hudLines; ++i) {
        if (!l.hudText[i][0] || !(l.hudAlpha[i] > .01f))
            continue;
        const bool red = i == 0 || i == 1;
        st.top = red ? hudRed : hudGold;
        st.bottom = red ? Vec3{.7f, .1f, .05f} : Vec3{.8f, .55f, .15f};
        st.alpha = std::clamp(l.hudAlpha[i], 0.f, 1.f);
        const float w = pixelTextWidth(l.hudText[i], st);
        // the speed / altitude line sits at the bottom edge
        const float yy = i == 4 ? -halfH + .04f : y;
        appendPixelText(out, l.hudText[i], c + u * yy - r * (w * .5f), r, u, st);
        if (i != 4)
            y -= .03f;
    }
    // The lock box: a 0.6 m square round the target, turned to the eye.
    if (l.lock && okv(l.lockAt) && okv(viewer) && inRange(l.lockAt, viewer, 60.f)) {
        const Vec3 lv = viewOf(l.lockAt, viewer);
        Vec3 lu = Vec3{0, 1, 0} - lv * dot(Vec3{0, 1, 0}, lv);
        lu = length(lu) > 1e-3f ? normalized(lu) : perpendicular(lv);
        const Vec3 lr = normalized(cross(lu, lv));
        const float s = .3f, k = .12f;
        const float hw = std::max(.004f, .0025f * length(l.lockAt - viewer));
        for (int sx = -1; sx <= 1; sx += 2)
            for (int sy = -1; sy <= 1; sy += 2) {
                const Vec3 p = l.lockAt + lr * (sx * s) + lu * (sy * s);
                softBar(out, p, p - lr * (sx * k), lv, hw, hudRed, .85f, hw * .5f);
                softBar(out, p, p - lu * (sy * k), lv, hw, hudRed, .85f, hw * .5f);
            }
    }
}

void appendBlood(std::vector<Vertex>& out, const Blood& b, Vec3 viewer, float pixelAngle) {
    if (!okv(viewer))
        return;
    VGuard g(out);
    const size_t from = out.size();
    const Vec3 dark{.32f, .01f, .02f}, wet{.55f, .02f, .03f};
    for (const auto& p : b.pools) {
        if (!p.live || !okv(p.at) || !inRange(p.at, viewer, 25.f) || out.size() - from + 80 > budget::blood)
            continue;
        const float fade = std::clamp(1.f - (p.age - bloodPoolSeconds[1] * .6f) / 4.f, 0.f, 1.f);
        softArc(out, p.at, {1, 0, 0}, {0, 0, 1}, 0, p.radius, 0, 2 * pi, dark, .85f * fade, p.radius * .25f, 10);
    }
    for (const auto& d : b.drops) {
        if (!d.live || !okv(d.at) || !okv(d.velocity) || !inRange(d.at, viewer, 25.f) || out.size() - from + 60 > budget::blood)
            continue;
        const Vec3 tail = d.at - d.velocity * .025f; // a short streak along its flight
        bar(out, tail, d.at, viewer, pixelAngle, d.size * .5f, wet, .95f);
    }
}
void appendSpray(std::vector<Vertex>& out, uint32_t kind, Vec3 from, Vec3 dir, float reach, float time, Vec3 viewer,
                 float pixelAngle) {
    if (!okv(from) || !okv(dir) || !okv(viewer) || length(dir) < 1e-3f || !std::isfinite(reach) || reach <= 0 ||
        !std::isfinite(time) || !inRange(from, viewer, 40.f))
        return;
    VGuard g(out);
    const Vec3 colors[4] = {{.95f, .97f, 1.f}, {.7f, .9f, 1.f}, green, {.85f, .9f, .95f}};
    const float alphas[4] = {.7f, .55f, .75f, .18f};
    const Vec3 c = colors[std::min(kind, 3u)];
    const float a = alphas[std::min(kind, 3u)];
    const Vec3 f = normalized(dir), s = perpendicular(f), u = normalized(cross(s, f));
    reach = std::min(reach, 12.f);
    if (kind == 2) // the construct hose: a solid green line
        bar(out, from, from + f * reach, viewer, pixelAngle, .03f, green, .6f);
    for (int i = 0; i < 24; ++i) {
        const float t = std::fmod(time * 2.f + hash(float(i)) , 1.f);
        const float r = t * reach, w = .04f + t * reach * .12f;
        const float ang = hash(float(i) + 7.f) * 2 * pi;
        const Vec3 p = from + f * r + (s * std::cos(ang) + u * std::sin(ang)) * (w * hash(float(i) + 3.f) * 2.f);
        const Vec3 v = viewOf(p, viewer), aa = perpendicular(v), bb = normalized(cross(v, aa));
        softArc(out, p, aa, bb, 0, w * .6f, 0, 2 * pi, c, a * (1.f - t), w * .4f, 6);
    }
}
void appendShock(std::vector<Vertex>& out, Vec3 at, float radius, float age, Vec3 viewer) {
    if (!okv(at) || !okv(viewer) || !std::isfinite(radius) || !std::isfinite(age) || age < 0 || age >= 1 || radius <= 0 ||
        !inRange(at, viewer, 60.f))
        return;
    VGuard g(out);
    const float r = std::min(radius, 12.f) * (.2f + .8f * age);
    softArc(out, at, {1, 0, 0}, {0, 0, 1}, r * .85f, r, 0, 2 * pi, {.9f, .95f, 1.f}, .45f * (1.f - age), r * .1f, 32);
}
void appendHook(std::vector<Vertex>& out, Vec3 from, Vec3 to, Vec3 viewer, float pixelAngle) {
    if (!okv(from) || !okv(to) || !okv(viewer) || length(to - from) < .2f || length(to - from) > 60.f ||
        !inRange(from, viewer, 60.f))
        return;
    VGuard g(out);
    bar(out, from, to, viewer, pixelAngle, .004f, {.12f, .12f, .12f}, .95f); // the line
    const Vec3 f = normalized(to - from), s = perpendicular(f);
    for (int i = 0; i < 3; ++i) { // the hook's three tines
        const float ang = i * 2 * pi / 3;
        const Vec3 side = s * std::cos(ang) + normalized(cross(f, s)) * std::sin(ang);
        bar(out, to, to + side * .07f - f * .05f, viewer, pixelAngle, .006f, {.35f, .35f, .37f}, 1.f);
    }
}

// PERCH TAKEDOWN's light: a small diamond with a soft ring, just over the target's head.
void appendPerchLight(std::vector<Vertex>& out, Vec3 at, bool red, Vec3 viewer) {
    if (!okv(at) || !okv(viewer) || !inRange(at, viewer, 40.f))
        return;
    VGuard g(out);
    const Vec3 c = red ? Vec3{1.f, .12f, .08f} : Vec3{.15f, 1.f, .35f};
    const Vec3 v = viewOf(at, viewer), up{0, 1, 0};
    Vec3 side = cross(up, v);
    side = length(side) > 1e-3f ? normalized(side) : perpendicular(v);
    const float range = std::max(length(viewer - at), .5f), s = std::clamp(.012f * range, .05f, .35f);
    const Vec3 top = at + up * s, bottom = at - up * s, l = at - side * s * .7f, r = at + side * s * .7f;
    for (const auto& e : {std::pair{top, r}, std::pair{r, bottom}, std::pair{bottom, l}, std::pair{l, top}})
        softBar(out, e.first, e.second, v, s * .12f, c, .95f, s * .08f);
    softArc(out, at, side, normalized(cross(v, side)), s * 1.1f, s * 1.3f, 0, 2 * pi, c, .35f, s * .1f, 16);
}
// The swoop's rope: a dark braided line (never a web).
void appendRope(std::vector<Vertex>& out, Vec3 from, Vec3 to, Vec3 viewer, float pixelAngle) {
    if (!okv(from) || !okv(to) || !okv(viewer) || length(to - from) < .2f || length(to - from) > 60.f ||
        !inRange(from, viewer, 60.f))
        return;
    VGuard g(out);
    bar(out, from, to, viewer, pixelAngle, .006f, {.16f, .13f, .1f}, .95f);
    bar(out, from, to, viewer, pixelAngle, .0025f, {.3f, .26f, .2f}, .9f);
}
Look worldLook(const Look& l, const Pose& w) {
    Look o{};
    if (!l.any || !okv(w.position) || !std::isfinite(w.orientation.w) || !std::isfinite(w.orientation.x) ||
        !std::isfinite(w.orientation.y) || !std::isfinite(w.orientation.z))
        return o;
    o = l;
    o.blood = nullptr;
    const auto P = [&](Vec3 p) { return w.position + w.orientation.rotate(p); };
    const auto D = [&](Vec3 d) { return w.orientation.rotate(d); };
    o.hammerAt = P(l.hammerAt);
    o.hammerForward = D(l.hammerForward);
    o.hammerUp = D(l.hammerUp);
    for (uint32_t i = 0; i < std::min(l.bolts, boltsMost); ++i) {
        o.boltFrom[i] = P(l.boltFrom[i]);
        o.boltTo[i] = P(l.boltTo[i]);
    }
    for (int i = 0; i < 2; ++i)
        o.ringAt[i] = P(l.ringAt[i]);
    o.beamFrom = P(l.beamFrom);
    o.beamTo = P(l.beamTo);
    o.bubbleAt = P(l.bubbleAt);
    o.chainFrom = P(l.chainFrom);
    o.chainTo = P(l.chainTo);
    o.fistAt = P(l.fistAt);
    o.fistForward = D(l.fistForward);
    o.chargeAt = P(l.chargeAt);
    o.chargeUp = D(l.chargeUp);
    o.hudEye = P(l.hudEye);
    o.hudForward = D(l.hudForward);
    o.hudUp = D(l.hudUp);
    o.lockAt = P(l.lockAt);
    o.sprayFrom = P(l.sprayFrom);
    o.sprayDir = D(l.sprayDir);
    o.shockAt = P(l.shockAt);
    o.hookFrom = P(l.hookFrom);
    o.hookTo = P(l.hookTo);
    o.perchAt = P(l.perchAt);
    o.ropeFrom = P(l.ropeFrom);
    o.ropeTo = P(l.ropeTo);
    return o;
}

void appendLook(std::vector<Vertex>& out, const Look& l, Vec3 travel, Vec3 viewer, float pixelAngle) {
    if (!l.any || !okv(travel) || !okv(viewer) || !std::isfinite(pixelAngle))
        return;
    const auto T = [&](Vec3 p) { return p + travel; };
    if (l.hammer)
        appendHammer(out, T(l.hammerAt), l.hammerForward, l.hammerUp, viewer, pixelAngle);
    for (uint32_t i = 0; i < std::min(l.bolts, boltsMost); ++i)
        appendBolt(out, T(l.boltFrom[i]), T(l.boltTo[i]), l.boltSeed[i], viewer, pixelAngle);
    for (int i = 0; i < 2; ++i)
        if (l.ring[i] && okv(l.ringAt[i]) && inRange(T(l.ringAt[i]), viewer, 4.f)) {
            VGuard g(out);
            const Vec3 p = T(l.ringAt[i]), v = viewOf(p, viewer);
            const Vec3 a = perpendicular(v), b = normalized(cross(v, a));
            softArc(out, p, a, b, 0, .016f, 0, 2 * pi, green, .9f, .01f, 10);
        }
    if (l.beam)
        appendLanternBeam(out, T(l.beamFrom), T(l.beamTo), viewer, pixelAngle);
    if (l.chain)
        appendChain(out, T(l.chainFrom), T(l.chainTo), viewer, pixelAngle);
    if (l.fist)
        appendFist(out, T(l.fistAt), l.fistForward, l.fistAlpha, viewer, pixelAngle);
    if (l.bubble)
        appendBubble(out, T(l.bubbleAt), l.bubbleRadius, viewer);
    if (l.charge)
        appendCharge(out, T(l.chargeAt), l.chargeUp, l.chargeLevel, l.chargeEmpty, viewer);
    if (l.hud) {
        Look h = l;
        h.hudEye = T(l.hudEye);
        h.lockAt = T(l.lockAt);
        appendIronHud(out, h, viewer);
    }
    if (l.blood)
        appendBlood(out, *l.blood, viewer, pixelAngle);
    if (l.spray)
        appendSpray(out, l.sprayKind, T(l.sprayFrom), l.sprayDir, l.sprayReach, l.sprayTime, viewer, pixelAngle);
    if (l.shock)
        appendShock(out, T(l.shockAt), l.shockRadius, l.shockAge, viewer);
    if (l.hook)
        appendHook(out, T(l.hookFrom), T(l.hookTo), viewer, pixelAngle);
    if (l.perchLight)
        appendPerchLight(out, T(l.perchAt), l.perchRed, viewer);
    if (l.rope)
        appendRope(out, T(l.ropeFrom), T(l.ropeTo), viewer, pixelAngle);
}
} // namespace spidy::hero072
