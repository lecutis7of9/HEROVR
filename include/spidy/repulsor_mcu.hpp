#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky. Made by Agent Mulder.
//
// Version 0.0.6 (Update 39.0.2) REPULSOR LOOK: a palm charge, a tight white-hot bolt with a cyan-blue
// falloff and a short impact burst, replacing 0.0.4's temporary look (web_visual's appendRepulsorBolt /
// appendRepulsorFlash stay for the tests and the previews). The same flat, eye-facing soft shapes as
// 0.0.4's look (soft_shapes.hpp): no textures, no new passes, no game memory, no new addresses.
// Timing: the charge fills the 0.12 s chord wait the press already has (repulsor.hpp chordWindow),
// so nothing is delayed; the palm fades over muzzleSeconds as the blast leaves; the impact is
// impactSeconds (0.0.4: 0.25 s). At most ~3,600 vertices for two palms, a bolt and a burst (one draw with the webs).
#include "spidy/repulsor.hpp"
#include "spidy/soft_shapes.hpp"
#include "spidy/vertex.hpp"
#include <algorithm>
#include <cmath>
#include <vector>

namespace spidy::repulsor_mcu {
using soft::perpendicular;
using soft::quad;
using soft::softArc;
using soft::softBar;
inline constexpr float pi = 3.14159265f;
// Palette (linear RGB): white core, ice-white hot band, cyan falloff, deep blue fringe.
inline const Vec3 core{1, 1, 1}, hot{.78f, .95f, 1}, cyan{.16f, .74f, 1}, deep{.05f, .30f, 1};

inline constexpr float muzzleSeconds = .09f, impactSeconds = .18f;
// The palm's glow 0..1 from repulsor::Trigger: charging (chargingFor, -1 not) rises over the chord
// window (fast first, so a quick tap still shows), full while a chord waits for the beam; after a
// blast (sinceBlast, -1 none) it fades over muzzleSeconds. Anything not finite: 0.
inline float palmGlow(float charging, float sinceBlast) {
    if (std::isfinite(charging) && charging >= 0)
        return std::pow(std::clamp(charging / repulsor::chordWindow, 0.f, 1.f), .7f);
    if (std::isfinite(sinceBlast) && sinceBlast >= 0 && sinceBlast < muzzleSeconds)
        return 1 - sinceBlast / muzzleSeconds;
    return 0;
}
// The impact burst's life 1..0 from a splash's age (seconds); 0 past impactSeconds or not finite.
inline float impactLife(float age) {
    return std::isfinite(age) && age >= 0 && age < impactSeconds ? 1 - age / impactSeconds : 0.f;
}

inline bool ok(Vec3 v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}
// A round soft disc facing the eye (as web_visual's softGlob): `sides` wedges, solid to 55%.
inline void glob(std::vector<Vertex>& out, Vec3 c, Vec3 viewer, float r, Vec3 color, float a, int sides = 10) {
    const Vec3 v = normalized(viewer - c);
    const Vec3 s1 = perpendicular(v), s2 = cross(v, s1);
    for (int i = 0; i < sides; ++i) {
        const float t0 = 2 * pi * float(i) / float(sides), t1 = 2 * pi * float(i + 1) / float(sides);
        const Vec3 e0 = s1 * std::cos(t0) + s2 * std::sin(t0), e1 = s1 * std::cos(t1) + s2 * std::sin(t1);
        const Vertex m{c, color, a}, m0{c + e0 * (r * .55f), color, a * .7f}, m1{c + e1 * (r * .55f), color, a * .7f};
        out.insert(out.end(), {m, m0, m1});
        quad(out, m0, m1, {c + e1 * r, color, 0}, {c + e0 * r, color, 0});
    }
}
// The pixel floor: nothing thinner than ~1.5 px at its range, so it never shimmers.
inline float floorAt(Vec3 p, Vec3 viewer, float pixelAngle) {
    return 1.5f * pixelAngle * std::max(length(p - viewer), .05f);
}

// A tapering streak from p (width w0, opacity a0) to q (w1, a1), rounded off past q; soft edges.
// 6 quads (36 vertices): one piece, so no seams where pieces overlap.
inline void streak(std::vector<Vertex>& out, Vec3 p, Vec3 q, Vec3 view, float w0, float w1, Vec3 color, float a0,
                   float a1, float soft) {
    const Vec3 along = normalized(q - p), side = normalized(cross(q - p, view));
    const Vec3 c[3] = {p, q, q + along * (w1 + soft)};
    const float w[3] = {w0, w1, w1 * .3f}, a[3] = {a0, a1, 0};
    for (int i = 0; i < 2; ++i) {
        const float ys[4] = {-w[i] - soft, -w[i], w[i], w[i] + soft}, yn[4] = {-w[i + 1] - soft, -w[i + 1], w[i + 1], w[i + 1] + soft};
        for (int j = 0; j < 3; ++j) {


            quad(out, {c[i] + side * ys[j], color, a[i] * (j == 0 ? 0.f : 1.f)},
                 {c[i + 1] + side * yn[j], color, a[i + 1] * (j == 0 ? 0.f : 1.f)},
                 {c[i + 1] + side * yn[j + 1], color, a[i + 1] * (j == 2 ? 0.f : 1.f)},
                 {c[i] + side * ys[j + 1], color, a[i] * (j == 2 ? 0.f : 1.f)});

        }
    }
}

// 1 PALM CHARGE. palm: the emitter's centre; normal: out of the palm; charge 0..1 (0 = nothing).
// A round emitter disc in the palm's plane with a bright ring, a white core facing the eye, a
// cyan halo that swells with the charge, and four short streaks pulling into the palm. ~990 vertices.
inline void appendCharge(std::vector<Vertex>& out, Vec3 palm, Vec3 normal, Vec3 viewer, float pixelAngle, float charge,
                         float spin = 0) {
    if (!ok(palm) || !ok(normal) || !ok(viewer) || !std::isfinite(charge) || length(normal) < .5f)
        return;
    const float k = std::clamp(charge, 0.f, 1.f);
    if (k < .01f)
        return;
    const Vec3 n = normalized(normal);
    const Vec3 a = perpendicular(n), b = cross(n, a);
    const float f = floorAt(palm, viewer, pixelAngle);
    const Vec3 face = palm + n * .004f;
    // the emitter in the palm (lies flat in the glove, so it reads as hardware, not a sprite)
    softArc(out, face, a, b, .028f, .036f, 0, 2 * pi, hot, .55f + .45f * k, std::max(.004f, f), 20);
    softArc(out, face, a, b, 0, .022f, 0, 2 * pi, core, .35f + .65f * k, std::max(.006f, f), 12);
    // the light off it, facing the eye
    glob(out, face + n * .01f, viewer, .07f + .09f * k, deep, .18f * k);
    glob(out, face + n * .01f, viewer, .035f + .04f * k, cyan, .45f * k);
    glob(out, face + n * .01f, viewer, .018f + .014f * k, core, .9f * k);
    // energy pulling in: four short streaks converging on the palm, shrinking as it fills
    const Vec3 view = normalized(viewer - palm);
    for (int i = 0; i < 4; ++i) {
        const float t = spin + float(i) * pi * .5f + .4f;
        const Vec3 dir = a * std::cos(t) + b * std::sin(t);
        const float r0 = .14f - .08f * k, r1 = r0 - .045f; // outside in: faint far, bright near the palm
        streak(out, face + dir * r0 + n * .03f, face + dir * r1 + n * .012f, view, .0008f, std::max(.003f, f * .6f), cyan,
               0, .9f * k * (1.2f - k), std::max(.003f, f));
    }
}

// 2 THE BOLT IN FLIGHT. head: the bolt's front; tail: up to 1.2 m behind it (our_ball's tail()).
// A tapered beam (one piece a layer: cyan falloff, ice band, white core), thin and faint at the tail;
// a compact white-hot head with a cyan bloom; one thin pressure ring just behind it.
inline void appendBolt(std::vector<Vertex>& out, Vec3 head, Vec3 tail, Vec3 viewer, float pixelAngle, float fade) {
    if (!ok(head) || !ok(tail) || !ok(viewer) || !std::isfinite(pixelAngle) || !std::isfinite(fade))
        return;
    const float a = std::clamp(fade, 0.f, 1.f);
    const float range = length(head - viewer);
    if (a < .01f || range < .05f)
        return;
    const float f = floorAt(head, viewer, pixelAngle);
    const float w = std::max(.022f, 2.f * f); // the beam's half width at the head
    const float span = length(head - tail);
    if (span > .02f) { // one tapered piece a layer: thin and faint at the tail, full at the head
        const Vec3 view = normalized(viewer - (head + tail) * .5f);
        streak(out, tail, head, view, w * .1f, std::max(w * .5f, f), cyan, 0, .55f * a, std::max(w * 2.2f, f * 2));
        streak(out, tail, head, view, w * .15f, std::max(w * .6f, f), hot, .1f * a, .9f * a, std::max(w * .3f, f));
        streak(out, tail + (head - tail) * .3f, head, view, w * .05f, std::max(w * .28f, f * .6f), core, 0, a,
               std::max(w * .12f, f * .5f));
    }
    // the head: tight and white-hot, a little longer than wide (motion), cyan bloom round it
    glob(out, head, viewer, w * 5.f, deep, .16f * a);
    glob(out, head, viewer, w * 2.6f, cyan, .5f * a);
    glob(out, head, viewer, w * 1.25f, core, a);
    // one pressure ring a hand behind the head, square to the flight
    if (span > .15f) {
        const Vec3 d = normalized(head - tail);
        const Vec3 r1 = perpendicular(d), r2 = cross(d, r1);
        const float rr = w * 2.2f;
        softArc(out, head - d * .14f, r1, r2, rr * .8f, rr, 0, 2 * pi, cyan, .45f * a, std::max(rr * .25f, f), 16);
    }
}

// 3 IMPACT BURST. life: 1 at the hit, falling to 0 over ~0.18 s (shorter than 0.0.4's .25 s).
// A white flash that pops then shrinks, a thin cyan shock ring racing out, six tapering sparks
// thrown outward and a blue haze.
inline void appendImpact(std::vector<Vertex>& out, Vec3 at, Vec3 from, Vec3 viewer, float pixelAngle, float life,
                         float size = .18f, unsigned seed = 7) {
    if (!ok(at) || !ok(from) || !ok(viewer) || !std::isfinite(pixelAngle) || !std::isfinite(life) || !std::isfinite(size))
        return;
    const float a = std::clamp(life, 0.f, 1.f);
    if (a < .01f || length(at - viewer) < .05f)
        return;
    const float s = std::clamp(size, .03f, 2.f), age = 1 - a;
    const float f = floorAt(at, viewer, pixelAngle);
    const Vec3 v = normalized(viewer - at);
    const Vec3 s1 = perpendicular(v), s2 = cross(v, s1);
    // the pop: a hard white flash for the first frames, cooling into a cyan bloom
    const float pop = a * a * a;
    glob(out, at, viewer, s * (2.4f + 1.4f * age), deep, .25f * a);
    glob(out, at, viewer, s * (1.2f + .5f * age), cyan, .65f * a);
    glob(out, at, viewer, std::max(s * (.55f + .5f * pop), f * 2), core, std::min(1.f, 1.6f * a * a));
    // the shock ring: a thin cyan ring racing out, gone by the end
    const float ring = s * (.5f + 2.6f * std::sqrt(age));
    const float thick = std::max(s * .12f * a, f);
    softArc(out, at + v * .01f, s1, s2, ring - thick, ring, 0, 2 * pi, cyan, .7f * a * a, std::max(thick * 1.5f, f), 24);
    // six sparks of uneven length thrown out, white at the root fading out at the tip
    unsigned x = seed * 2654435761u + 1;
    for (int i = 0; i < 6; ++i) {
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        const float t = (float(i) + float(x % 1000) / 1000.f * .8f) * (2 * pi / 6);
        const Vec3 dir = s1 * std::cos(t) + s2 * std::sin(t);
        const float len = s * (.8f + .9f * float((x >> 10) % 100) / 100.f);
        const float r0 = s * .35f + len * 1.2f * age, r1 = r0 + len * (1 - .6f * age);
        streak(out, at + dir * r0, at + dir * r1, v, std::max(s * .035f * a, f * .6f), std::max(s * .005f, f * .4f),
               i % 2 ? hot : core, a, 0, std::max(s * .03f, f));
    }
}
} // namespace spidy::repulsor_mcu
