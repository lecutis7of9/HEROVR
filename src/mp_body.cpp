// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
// Made by Agent Mulder.
//
// 0.0.73 MPSUIT PLAYER 2 BODY: the procedural hero body (mp_body.hpp). Every shape is a map from two
// parameters to a surface point (and the axis it bulges from); the grids are coloured per triangle (crisp suit
// panels) and lit per vertex, the web lines, lenses and spiders lie on the surfaces a few millimetres out.
#include "spidy/mp_body.hpp"

#include "spidy/mp_ghost.hpp"
#include "spidy/vr_mirror.hpp"

#include <algorithm>
#include <cmath>
#include <functional>

namespace spidy::mp {
namespace {
using namespace suit_wear;
constexpr float pi = 3.14159265f, tau = 6.2831853f;

bool okV(Vec3 v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }
Vec3 lerpV(Vec3 a, Vec3 b, float t) { return a + (b - a) * t; }
Vec3 squareTo(Vec3 v, Vec3 axis) { return normalized(v - axis * dot(v, axis)); }
float wrapPi(float a) {
    a = std::fmod(a + pi, tau);
    if (a < 0)
        a += tau;
    return a - pi;
}
Vec3 fromSrgb(uint32_t c) {
    return {vr_mirror::toLinear(static_cast<unsigned char>(c >> 16)), vr_mirror::toLinear(static_cast<unsigned char>(c >> 8)),
            vr_mirror::toLinear(static_cast<unsigned char>(c))};
}
float luma(Vec3 c) { return .2126f * c.x + .7152f * c.y + .0722f * c.z; }

struct SP {
    Vec3 p, c; // the point and the axis point it bulges out from (for the normal's side)
};
using Map = std::function<SP(float, float)>;

struct Ctx {
    std::vector<Vertex>& out;
    Vec3 viewer, up;
    float alpha, sc;
    bool bad = false;
};
Vec3 lit(const Ctx& x, Vec3 base, Vec3 p, Vec3 n) {
    const Vec3 toEye = normalized(x.viewer - p);
    const Vec3 key = normalized(x.up + toEye * .6f);
    const float diffuse = std::max(0.f, dot(n, key)), fill = .5f + .5f * dot(n, x.up);
    const Vec3 half = normalized(key + toEye);
    const float spec = std::pow(std::max(0.f, dot(n, half)), 30.f) * .22f;
    const float rim = std::pow(1.f - std::clamp(dot(n, toEye), 0.f, 1.f), 3.f);
    Vec3 c = base * (.16f + .70f * diffuse + .14f * fill) + Vec3{1, 1, 1} * spec + base * (.35f * rim) + Vec3{.03f, .03f, .035f} * rim;
    return {std::clamp(c.x, 0.f, 1.f), std::clamp(c.y, 0.f, 1.f), std::clamp(c.z, 0.f, 1.f)};
}
// The surface's normal at (a, b): the cross of its two tangents, out from its axis point.
Vec3 normalAt(const Map& m, float a, float b, const SP& s) {
    constexpr float e = 1e-3f;
    const Vec3 da = m(a + e, b).p - m(a - e, b).p, db = m(a, b + e).p - m(a, b - e).p;
    Vec3 n = normalized(cross(da, db));
    const Vec3 o = normalized(s.p - s.c);
    if (!(length(n) > .5f))
        return o;
    return dot(n, o) < 0 ? n * -1.f : n;
}
void tri(Ctx& x, Vec3 col, const Vec3 (&p)[3], const Vec3 (&n)[3]) {
    for (int i = 0; i < 3; ++i) {
        if (!okV(p[i]) || !okV(n[i])) {
            x.bad = true;
            return;
        }
    }
    for (int i = 0; i < 3; ++i)
        x.out.push_back(Vertex{p[i], lit(x, col, p[i], n[i]), x.alpha, {}});
}
// A grid over a in [a0, a1] (rows), b in [b0, b1] (columns), each cell in colour(a, b) at its middle.
void grid(Ctx& x, const Map& m, float a0, float a1, int rows, float b0, float b1, int cols,
          const std::function<Vec3(float, float)>& colour) {
    std::vector<Vec3> P((rows + 1) * (cols + 1)), N(P.size());
    for (int i = 0; i <= rows; ++i)
        for (int j = 0; j <= cols; ++j) {
            const float a = a0 + (a1 - a0) * i / rows, b = b0 + (b1 - b0) * j / cols;
            const SP s = m(a, b);
            P[i * (cols + 1) + j] = s.p;
            N[i * (cols + 1) + j] = normalAt(m, a, b, s);
        }
    for (int i = 0; i < rows; ++i)
        for (int j = 0; j < cols; ++j) {
            const float a = a0 + (a1 - a0) * (i + .5f) / rows, b = b0 + (b1 - b0) * (j + .5f) / cols;
            const Vec3 col = colour(a, b);
            const int k00 = i * (cols + 1) + j, k01 = k00 + 1, k10 = k00 + cols + 1, k11 = k10 + 1;
            tri(x, col, {P[k00], P[k10], P[k11]}, {N[k00], N[k10], N[k11]});
            tri(x, col, {P[k00], P[k11], P[k01]}, {N[k00], N[k11], N[k01]});
        }
}
// A ribbon `width` wide along the parameter path, `lift` out of the surface.
void ribbon(Ctx& x, const Map& m, const std::vector<std::pair<float, float>>& path, float width, float lift, Vec3 col) {
    if (path.size() < 2)
        return;
    std::vector<Vec3> P(path.size()), N(path.size());
    for (size_t k = 0; k < path.size(); ++k) {
        const SP s = m(path[k].first, path[k].second);
        N[k] = normalAt(m, path[k].first, path[k].second, s);
        P[k] = s.p + N[k] * lift;
    }
    std::vector<Vec3> L(P.size()), R(P.size());
    for (size_t k = 0; k < P.size(); ++k) {
        const Vec3 t = P[std::min(k + 1, P.size() - 1)] - P[k ? k - 1 : 0];
        Vec3 side = normalized(cross(N[k], t));
        if (!(length(side) > .5f))
            side = normalized(cross(N[k], Vec3{0.3f, 1, .1f}));
        L[k] = P[k] - side * (width * .5f), R[k] = P[k] + side * (width * .5f);
    }
    for (size_t k = 0; k + 1 < P.size(); ++k) {
        tri(x, col, {L[k], L[k + 1], R[k + 1]}, {N[k], N[k + 1], N[k + 1]});
        tri(x, col, {L[k], R[k + 1], R[k]}, {N[k], N[k + 1], N[k]});
    }
}
// A filled polygon (star-shaped about its mean) in the map's parameters, `lift` out, in rings so it follows
// the curve.
void fan(Ctx& x, const Map& m, const std::vector<std::pair<float, float>>& poly, float lift, Vec3 col, int rings = 3) {
    if (poly.size() < 3)
        return;
    float ca = 0, cb = 0;
    for (const auto& [a, b] : poly)
        ca += a, cb += b;
    ca /= poly.size(), cb /= poly.size();
    const auto at = [&](float a, float b, Vec3& n) {
        const SP s = m(a, b);
        n = normalAt(m, a, b, s);
        return s.p + n * lift;
    };
    for (size_t k = 0; k < poly.size(); ++k) {
        const auto& p0 = poly[k];
        const auto& p1 = poly[(k + 1) % poly.size()];
        for (int r = 0; r < rings; ++r) {
            const float t0 = static_cast<float>(r) / rings, t1 = static_cast<float>(r + 1) / rings;
            Vec3 n[4];
            const Vec3 a0 = at(ca + (p0.first - ca) * t0, cb + (p0.second - cb) * t0, n[0]);
            const Vec3 a1 = at(ca + (p1.first - ca) * t0, cb + (p1.second - cb) * t0, n[1]);
            const Vec3 b0 = at(ca + (p0.first - ca) * t1, cb + (p0.second - cb) * t1, n[2]);
            const Vec3 b1 = at(ca + (p1.first - ca) * t1, cb + (p1.second - cb) * t1, n[3]);
            tri(x, col, {a0, b0, b1}, {n[0], n[2], n[3]});
            if (r)
                tri(x, col, {a0, b1, a1}, {n[0], n[3], n[1]});
        }
    }
}
// An ellipsoid: a lat (-pi/2 .. pi/2) along Y, b lon (0 .. tau).
Map ellipsoid(Vec3 c, Vec3 X, Vec3 Y, Vec3 Z) {
    return [=](float a, float b) {
        return SP{c + X * (std::cos(a) * std::cos(b)) + Y * std::sin(a) + Z * (std::cos(a) * std::sin(b)), c};
    };
}
void blob(Ctx& x, Vec3 c, Vec3 X, Vec3 Y, Vec3 Z, Vec3 col, int rows = 8, int cols = 12) {
    grid(x, ellipsoid(c, X, Y, Z), -pi / 2, pi / 2, rows, 0, tau, cols, [&](float, float) { return col; });
}
// A tube from a to b (a: u 0, b: u 1), radii ra -> rb with a bulge, v around (0 .. tau) from `ref`.
Map tube(Vec3 a, Vec3 b, float ra, float rb, float bulge, Vec3 ref) {
    const Vec3 d = normalized(b - a);
    Vec3 e1 = squareTo(ref, d);
    if (!(length(e1) > .5f))
        e1 = squareTo(Vec3{0, 0, 1}, d);
    if (!(length(e1) > .5f))
        e1 = squareTo(Vec3{1, 0, 0}, d);
    const Vec3 e2 = cross(d, e1);
    const float L = length(b - a);
    return [=](float u, float v) {
        const float r = ra + (rb - ra) * u + bulge * ra * std::sin(pi * std::clamp(u, 0.f, 1.f));
        const Vec3 c = a + d * (L * u);
        return SP{c + (e1 * std::cos(v) + e2 * std::sin(v)) * r, c};
    };
}

struct Body {
    const Skeleton& k;
    const Look& look;
    float sc;
    Vec3 up, fwd, left; // the torso's
};

// ---- the torso: s 0 (under the pelvis) .. 1 (the neck), th around (0 the front, +pi/2 his left)
struct Section {
    float s, w, d;
};
constexpr Section sections[] = {{0.f, .150f, .100f}, {.16f, .165f, .105f}, {.40f, .135f, .092f}, {.62f, .160f, .112f},
                                {.82f, .192f, .110f}, {.93f, .160f, .092f}, {1.f, .062f, .058f}};
void sectionAt(float s, float& w, float& d) {
    s = std::clamp(s, 0.f, 1.f);
    for (size_t i = 0; i + 1 < std::size(sections); ++i)
        if (s <= sections[i + 1].s) {
            const float t = (s - sections[i].s) / (sections[i + 1].s - sections[i].s);
            const float e = t * t * (3 - 2 * t);
            w = sections[i].w + (sections[i + 1].w - sections[i].w) * e;
            d = sections[i].d + (sections[i + 1].d - sections[i].d) * e;
            return;
        }
    w = sections[std::size(sections) - 1].w, d = sections[std::size(sections) - 1].d;
}
float se(float c) { return (c < 0 ? -1.f : 1.f) * std::pow(std::abs(c), 2.f / 2.6f); }
Map torsoMap(const Body& B) {
    const Vec3 pel = B.k.at[pelvis], che = B.k.at[chest], nec = B.k.at[neck];
    const Vec3 low = pel - B.up * (.09f * B.sc);
    return [=](float s, float th) {
        const float sc = std::clamp(s, 0.f, 1.f);
        Vec3 c = sc < .2f ? lerpV(low, pel, sc / .2f) : sc < .62f ? lerpV(pel, che, (sc - .2f) / .42f) : lerpV(che, nec, (sc - .62f) / .38f);
        c += B.fwd * (.012f * B.sc * std::exp(-((sc - .72f) * (sc - .72f)) / .02f));
        float w = 0, d = 0;
        sectionAt(sc, w, d);
        const float cf = std::cos(th), sf = std::sin(th);
        const float depth = (cf >= 0 ? d : d * .86f) * B.sc, width = w * B.sc;
        return SP{c + B.fwd * (depth * se(cf)) + B.left * (width * se(sf)), c};
    };
}
// SPIDER-MAN's torso panels: the primary over the chest, the back and a belt; the secondary on the flanks
// (narrowing up to the chest) and under the belt.
bool torsoPrimary(const Look& l, float s, float th) {
    const float a = std::abs(wrapPi(th)), from = std::min(a, pi - a);
    if (!l.spider)
        return s > .16f && std::abs(wrapPi(th)) < 1.25f;
    if (s < .14f)
        return false;
    if (s < .20f)
        return true; // the belt
    const float hw = s < .55f ? .55f + (1.f - .55f) * (s - .2f) / .35f : s < .80f ? 1.f + .9f * (s - .55f) / .25f : 1.9f;
    return from < hw;
}
// the torso's unrolled coordinates (X across his left, Y up, metres) about a point on the front or back
Map torsoFlat(const Body& B, float sCentre, bool back) {
    const Map t = torsoMap(B);
    const float H = length(B.k.at[neck] - B.k.at[pelvis]) + .09f * B.sc;
    return [=](float X, float Y) {
        const float s = sCentre + Y / H;
        float w = 0, d = 0;
        sectionAt(s, w, d);
        const float r = (w + d) * .5f * B.sc;
        const float th = back ? pi - X / r : X / r;
        return t(s, th);
    };
}
void spiderShape(Ctx& x, const Map& m, float size, float lift, Vec3 col, bool flipX) {
    const float f = flipX ? -1.f : 1.f;
    std::vector<std::pair<float, float>> body, head;
    for (int i = 0; i < 12; ++i) {
        const float a = tau * i / 12;
        body.push_back({f * size * .10f * std::cos(a), -size * .12f + size * .22f * std::sin(a)});
        head.push_back({f * size * .07f * std::cos(a), size * .20f + size * .08f * std::sin(a)});
    }
    fan(x, m, body, lift, col, 2);
    fan(x, m, head, lift, col, 2);
    static constexpr float legs[4][4] = {{.30f, .30f, .38f, .62f}, {.34f, .10f, .50f, .25f}, {.34f, -.08f, .50f, -.22f}, {.28f, -.25f, .36f, -.58f}};
    for (const auto& L : legs)
        for (float side : {-1.f, 1.f}) {
            std::vector<std::pair<float, float>> p = {{side * size * .06f, size * .02f}, {side * size * L[0], size * L[1]}, {side * size * L[2], size * L[3]}};
            ribbon(x, m, p, size * .05f, lift, col);
        }
}
// The web over a flat map: radial lines from the centre and sagging arcs between them, only where `inside`.
void web(Ctx& x, const Map& m, float reach, int spokes, float ringStep, float width, float lift, Vec3 col,
         const std::function<bool(float, float)>& inside) {
    const auto keep = [&](std::vector<std::pair<float, float>>& run) {
        if (run.size() >= 2)
            ribbon(x, m, run, width, lift, col);
        run.clear();
    };
    const float step = .012f * x.sc;
    for (int i = 0; i < spokes; ++i) {
        const float a = tau * (i + .5f) / spokes;
        std::vector<std::pair<float, float>> run;
        for (float r = .015f * x.sc; r <= reach; r += step) {
            const float X = r * std::cos(a), Y = r * std::sin(a);
            if (inside(X, Y))
                run.push_back({X, Y});
            else
                keep(run);
        }
        keep(run);
    }
    for (float R = ringStep; R <= reach; R += ringStep * (1.f + R / reach * .4f)) {
        for (int i = 0; i < spokes; ++i) {
            const float a0 = tau * (i + .5f) / spokes, a1 = tau * (i + 1.5f) / spokes;
            std::vector<std::pair<float, float>> run;
            for (int j = 0; j <= 6; ++j) {
                const float t = j / 6.f, a = a0 + (a1 - a0) * t, r = R * (1.f - .10f * std::sin(pi * t));
                const float X = r * std::cos(a), Y = r * std::sin(a);
                if (inside(X, Y))
                    run.push_back({X, Y});
                else
                    keep(run);
            }
            keep(run);
        }
    }
}

void drawTorso(Ctx& x, const Body& B) {
    const Look& l = B.look;
    const Map t = torsoMap(B);
    grid(x, t, 0, 1, 24, -pi, pi, 36, [&](float s, float th) { return torsoPrimary(l, s, th) ? l.primary : l.secondary; });
    const float H = length(B.k.at[neck] - B.k.at[pelvis]) + .09f * B.sc;
    const float lw = .0042f * B.sc, lift = .0022f * B.sc;
    if (l.spider) {
        const Vec3 lines = vr_mirror::webLineColour(l.primary);
        for (int side = 0; side < 2; ++side) {
            const bool back = side == 1;
            const float sc0 = back ? .66f : .74f;
            const Map f = torsoFlat(B, sc0, back);
            const auto inside = [&](float X, float Y) {
                const float s = sc0 + Y / H;
                float w = 0, d = 0;
                sectionAt(s, w, d);
                const float th = back ? pi - X / ((w + d) * .5f * B.sc) : X / ((w + d) * .5f * B.sc);
                return s > .21f && s < .97f && std::abs(X / ((w + d) * .5f * B.sc)) < 1.45f && torsoPrimary(l, s, th);
            };
            web(x, f, .45f * B.sc, 14, .055f * B.sc, lw, lift, lines, inside);
            spiderShape(x, f, (back ? .15f : .10f) * B.sc, lift * 2.f, l.emblem, back);
        }
        // the spider's outline over a light primary (a white suit): the spider stays readable
    } else {
        const Map f = torsoFlat(B, .74f, false);
        std::vector<std::pair<float, float>> badge;
        for (int i = 0; i < 16; ++i)
            badge.push_back({.045f * B.sc * std::cos(tau * i / 16), .045f * B.sc * std::sin(tau * i / 16)});
        fan(x, f, badge, lift * 2.f, l.emblem, 3);
    }
}

// ---- the head: u 0 the face's front .. 1 the back of the head, v around the face (0 his left)
struct HeadFrame {
    Vec3 c, R, U, F; // radii included
};
HeadFrame headFrame(const Body& B) {
    const Skeleton& k = B.k;
    Vec3 F = normalized(k.face), U = squareTo(k.faceUp, F);
    if (!(length(F) > .5f) || !(length(U) > .5f))
        F = B.fwd, U = B.up;
    const Vec3 R = cross(U, F);
    const Vec3 c = k.at[head] + (k.at[headTop] - k.at[head]) * .46f;
    return {c, R * (.090f * B.sc), U * (.120f * B.sc), F * (.104f * B.sc)};
}
Map headMap(const HeadFrame& h) {
    return [=](float u, float v) {
        const float a = pi * std::clamp(u, 0.f, 1.f);
        const float y = std::sin(a) * std::sin(v);
        const float narrow = y < 0 ? 1.f + .16f * y : 1.f; // the jaw narrower
        return SP{h.c + h.F * std::cos(a) * narrow + (h.R * (std::cos(v) * narrow) + h.U * std::sin(v)) * std::sin(a), h.c};
    };
}
// the face seen from the front: x his left, y up (in head radii)
Map faceMap(const HeadFrame& h) {
    return [=](float X, float Y) {
        const float r2 = std::min(.97f, X * X + Y * Y);
        const float narrow = Y < 0 ? 1.f + .16f * Y : 1.f;
        return SP{h.c + h.R * (X * narrow) + h.U * Y + h.F * (std::sqrt(1.f - r2) * narrow), h.c};
    };
}
void drawHead(Ctx& x, const Body& B) {
    const Look& l = B.look;
    const HeadFrame h = headFrame(B);
    const Map m = headMap(h), face = faceMap(h);
    if (!l.spider) {
        grid(x, m, 0, 1, 16, -pi, pi, 24, [&](float u, float v) {
            const float a = pi * u, X = std::sin(a) * std::cos(v), Y = std::sin(a) * std::sin(v);
            return u < .38f && Y > -.12f && Y < .18f && std::abs(X) < .85f ? l.secondary : l.primary;
        });
        return;
    }
    grid(x, m, 0, 1, 16, -pi, pi, 24, [&](float, float) { return l.primary; });
    const float lw = .0034f * B.sc;
    const Vec3 lines = vr_mirror::webLineColour(l.primary);
    // the web: meridians from between the eyes, rings round it
    for (int i = 0; i < 16; ++i) {
        const float v = -pi + tau * (i + .5f) / 16;
        std::vector<std::pair<float, float>> p;
        for (int j = 0; j <= 20; ++j)
            p.push_back({.04f + .92f * j / 20.f, v});
        ribbon(x, m, p, lw, .0018f * B.sc, lines);
    }
    for (float u : {.12f, .21f, .30f, .40f, .51f, .63f, .76f, .88f}) {
        for (int i = 0; i < 16; ++i) {
            const float v0 = -pi + tau * (i + .5f) / 16, v1 = v0 + tau / 16;
            std::vector<std::pair<float, float>> p;
            for (int j = 0; j <= 5; ++j) {
                const float t = j / 5.f;
                p.push_back({u * (1.f - .06f * std::sin(pi * t)), v0 + (v1 - v0) * t});
            }
            ribbon(x, m, p, lw, .0018f * B.sc, lines);
        }
    }
    // the lenses: white in a dark rim, swept up and out
    static constexpr float lens[][2] = {{.10f, -.02f}, {.15f, .16f}, {.30f, .32f}, {.50f, .36f}, {.66f, .25f},
                                        {.70f, .08f},  {.58f, -.08f}, {.36f, -.13f}, {.18f, -.10f}};
    const Vec3 rim = l.secondary * .5f + Vec3{.01f, .01f, .012f}, white{.86f, .87f, .90f};
    for (float side : {-1.f, 1.f}) {
        std::vector<std::pair<float, float>> in, out;
        float cx = 0, cy = 0;
        for (const auto& p : lens)
            cx += p[0], cy += p[1];
        cx /= std::size(lens), cy /= std::size(lens);
        for (const auto& p : lens) {
            in.push_back({side * p[0], p[1]});
            out.push_back({side * (cx + (p[0] - cx) * 1.22f), cy + (p[1] - cy) * 1.30f});
        }
        fan(x, face, out, .0030f * B.sc, rim, 3);
        fan(x, face, in, .0046f * B.sc, white, 3);
    }
}

// ---- the limbs
bool limbPrimary(const Look& l, bool arm, float u, float v) {
    // v 0 points into the body (arms) / ahead (legs)
    const float a = std::abs(wrapPi(v));
    if (arm)
        return l.spider ? a > 1.0f : a > .9f;
    if (u > .58f && !arm)
        return true; // the boots (the shin's lower part; u is along the shin here)
    return l.spider && a < 1.05f;
}
void limb(Ctx& x, const Body& B, Vec3 a, Vec3 b, float ra, float rb, float bulge, Vec3 ref, bool arm, bool shin) {
    const Look& l = B.look;
    const Map m = tube(a, b, ra * B.sc, rb * B.sc, bulge, ref);
    grid(x, m, -.04f, 1.04f, 10, -pi, pi, 16, [&](float u, float v) {
        const bool prim = arm ? limbPrimary(l, true, u, v) : (shin ? limbPrimary(l, false, u, v) : limbPrimary(l, false, 0, v));
        return prim ? l.primary : l.secondary;
    });
    if (!l.spider)
        return;
    const Vec3 lines = vr_mirror::webLineColour(l.primary);
    const float lw = .0036f * B.sc, lift = .0020f * B.sc, len = length(b - a);
    const auto prim = [&](float u, float v) { return arm ? limbPrimary(l, true, u, v) : (shin ? limbPrimary(l, false, u, v) : limbPrimary(l, false, 0, v)); };
    for (int i = 0; i < 10; ++i) {
        const float v = -pi + tau * (i + .5f) / 10;
        std::vector<std::pair<float, float>> run;
        for (int j = 0; j <= 14; ++j) {
            const float u = j / 14.f;
            if (prim(u, v))
                run.push_back({u, v});
            else {
                if (run.size() >= 2)
                    ribbon(x, m, run, lw, lift, lines);
                run.clear();
            }
        }
        if (run.size() >= 2)
            ribbon(x, m, run, lw, lift, lines);
    }
    const int rings = std::max(2, static_cast<int>(len / (.07f * B.sc)));
    for (int r = 1; r < rings; ++r) {
        const float u = static_cast<float>(r) / rings;
        for (int i = 0; i < 10; ++i) {
            const float v0 = -pi + tau * (i + .5f) / 10, v1 = v0 + tau / 10;
            std::vector<std::pair<float, float>> p;
            for (int j = 0; j <= 4; ++j) {
                const float t = j / 4.f, uu = u - .012f * std::sin(pi * t);
                if (prim(uu, v0 + (v1 - v0) * t))
                    p.push_back({uu, v0 + (v1 - v0) * t});
            }
            if (p.size() == 5)
                ribbon(x, m, p, lw, lift, lines);
        }
    }
}
void drawLimbs(Ctx& x, const Body& B) {
    const Skeleton& k = B.k;
    const Look& l = B.look;
    const Vec3 mid = (k.at[pelvis] + k.at[chest]) * .5f;
    for (int side = 0; side < 2; ++side) {
        const Vec3 sh = k.at[side ? shoulderR : shoulderL], el = k.at[side ? elbowR : elbowL], wr = k.at[side ? wristR : wristL],
                   tp = k.at[side ? tipR : tipL];
        // the arm: v 0 into the body (and down)
        const Vec3 inUp = normalized(mid - sh) + B.up * -.3f, inFo = normalized(mid - el) + B.up * -.4f;
        limb(x, B, sh, el, .052f, .042f, .12f, inUp, true, false);
        limb(x, B, el, wr, .042f, .030f, .15f, inFo, true, false);
        blob(x, sh, B.left * (.062f * B.sc), B.up * (.058f * B.sc), B.fwd * (.060f * B.sc), l.primary);
        blob(x, el, B.left * (.043f * B.sc), B.up * (.043f * B.sc), B.fwd * (.043f * B.sc),
             l.spider ? l.primary : l.primary * .8f);
        // the hand (a glove): from the wrist toward the fingertips, its back up
        const Vec3 way = normalized(tp - wr);
        Vec3 back = squareTo(k.handBack[side], way);
        if (!(length(back) > .5f))
            back = squareTo(B.up, way);
        const Vec3 across = cross(back, way);
        const float curl = std::clamp(k.curl[side], 0.f, 1.f);
        const float L = std::clamp(length(tp - wr), .05f * B.sc, .24f * B.sc) * (1.f - .35f * curl);
        const Vec3 hc = wr + way * (L * .5f);
        blob(x, hc, across * (.046f * B.sc), way * (L * .55f), back * ((.024f + .012f * curl) * B.sc), l.primary, 8, 12);
        blob(x, wr + way * (L * .22f) + across * ((side ? -.032f : .032f) * B.sc) - back * (.008f * B.sc), across * (.016f * B.sc),
             normalized(way + across * (side ? -.6f : .6f)) * (.035f * B.sc), back * (.014f * B.sc), l.primary, 6, 8); // thumb
        blob(x, wr, across * (.034f * B.sc), way * (.030f * B.sc), back * (.026f * B.sc), l.primary, 6, 10);
        // the leg: thigh then shin (v 0 ahead), the knee and the boot
        const Vec3 hp = k.at[side ? hipR : hipL], kn = k.at[side ? kneeR : kneeL], ft = k.at[side ? footR : footL];
        limb(x, B, hp, kn, .085f, .058f, .10f, B.fwd, false, false);
        limb(x, B, kn, ft, .056f, .040f, .18f, B.fwd, false, true);
        blob(x, hp, B.left * (.085f * B.sc), B.up * (.080f * B.sc), B.fwd * (.085f * B.sc), l.secondary);
        blob(x, kn, B.left * (.058f * B.sc), B.up * (.058f * B.sc), B.fwd * (.060f * B.sc), l.spider ? l.primary : l.secondary);
        const Vec3 toes = squareTo(B.fwd, B.up);
        blob(x, ft + toes * (.045f * B.sc) - B.up * (.012f * B.sc), B.left * (.048f * B.sc), B.up * (.045f * B.sc),
             toes * (.125f * B.sc), l.primary, 8, 12);
    }
    // the neck
    const Vec3 nk = k.at[neck], hd = k.at[head];
    const Map m = tube(nk - B.up * (.02f * B.sc), hd + (k.at[headTop] - hd) * .2f, .058f * B.sc, .050f * B.sc, 0, B.fwd);
    grid(x, m, 0, 1, 3, -pi, pi, 16, [&](float, float) { return l.primary; });
}
} // namespace

Look lookOf(const PoseState& s) {
    Look l;
    if (s.hero == 0) {
        const vr_mirror::SuitPalette d;
        l.primary = d.primary, l.secondary = d.secondary, l.emblem = d.emblem;
        if (s.hasLook) {
            l.primary = fromSrgb(s.lookPrimary), l.secondary = fromSrgb(s.lookSecondary), l.emblem = fromSrgb(s.lookEmblem);
            l.learned = s.lookLearned;
            // a spider that would vanish into the chest: white over a dark primary, black over a light one
            if (std::abs(luma(l.emblem) - luma(l.primary)) < .08f)
                l.emblem = luma(l.primary) > .3f ? Vec3{.02f, .02f, .025f} : Vec3{.85f, .85f, .88f};
        }
        l.spider = true;
        return l;
    }
    const Vec3 c = heroColor(s.hero);
    l.primary = c;
    l.secondary = c * .28f + Vec3{.012f, .012f, .015f};
    l.emblem = c * .4f + Vec3{.55f, .55f, .55f};
    l.spider = false;
    return l;
}
void setLook(PoseState& s, Vec3 primary, Vec3 secondary, Vec3 emblem, bool learned) {
    if (!okV(primary) || !okV(secondary) || !okV(emblem)) {
        s.hasLook = false;
        return;
    }
    s.hasLook = true;
    s.lookLearned = learned;
    s.lookPrimary = vr_mirror::packSrgb(primary), s.lookSecondary = vr_mirror::packSrgb(secondary), s.lookEmblem = vr_mirror::packSrgb(emblem);
}

const suit_wear::Rig& bodyRig() {
    static const suit_wear::Rig r = [] {
        suit_wear::Rig g;
        g.ok = true;
        g.height = 1.8f;
        Skeleton& k = g.skeleton;
        const auto set = [&](uint32_t e, float x, float y, float z) { k.at[e] = {x, y, z}; };
        set(head, 0, 1.58f, 0), set(headTop, 0, 1.80f, 0), set(neck, 0, 1.50f, 0), set(chest, 0, 1.30f, .01f), set(pelvis, 0, 1.00f, 0);
        for (int side = 0; side < 2; ++side) {
            const float s = side ? -1.f : 1.f; // his left at +x
            set(side ? shoulderR : shoulderL, s * .19f, 1.45f, 0);
            set(side ? elbowR : elbowL, s * .24f, 1.17f, -.01f);
            set(side ? wristR : wristL, s * .27f, .92f, .01f);
            set(side ? tipR : tipL, s * .28f, .74f, .02f);
            set(side ? hipR : hipL, s * .10f, .95f, 0);
            set(side ? kneeR : kneeL, s * .11f, .52f, .02f);
            set(side ? footR : footL, s * .12f, .06f, 0);
            k.handBack[side] = {s, 0, 0};
            g.profileBack[side] = k.handBack[side];
        }
        k.face = {0, 0, 1}, k.faceUp = {0, 1, 0}, k.up = {0, 1, 0};
        k.ok = true;
        return g;
    }();
    return r;
}

size_t appendHeroBody(std::vector<Vertex>& out, const Skeleton& k, const Look& look, Vec3 viewer, float alpha) {
    const size_t start = out.size();
    for (const Vec3& p : k.at)
        if (!okV(p))
            return 0;
    if (!okV(viewer) || !std::isfinite(alpha))
        return 0;
    Vec3 up = normalized(k.at[neck] - k.at[pelvis]);
    Vec3 left = squareTo(k.at[shoulderL] - k.at[shoulderR], up);
    if (!(length(up) > .5f) || !(length(left) > .5f))
        return 0;
    const Vec3 fwd = cross(left, up);
    const float sc = std::clamp(length(k.at[neck] - k.at[pelvis]) / .50f, .5f, 1.6f);
    Vec3 wup = normalized(k.up);
    if (!(length(wup) > .5f))
        wup = up;
    try {
        Ctx x{out, viewer, wup, std::clamp(alpha, 0.f, 1.f), sc};
        const Body B{k, look, sc, up, fwd, left};
        drawTorso(x, B);
        drawLimbs(x, B);
        drawHead(x, B);
        if (x.bad || out.size() - start > bodyMaxVertices) {
            out.resize(start);
            return 0;
        }
    } catch (...) {
        out.resize(start);
        return 0;
    }
    return out.size() - start;
}
size_t appendHeroBody(std::vector<Vertex>& out, const Tracked& t, const Look& look, Vec3 viewer, float alpha) {
    Skeleton k;
    if (!estimate(bodyRig(), t, k))
        return 0;
    return appendHeroBody(out, k, look, viewer, alpha);
}
} // namespace spidy::mp
