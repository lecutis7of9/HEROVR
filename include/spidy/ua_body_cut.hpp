#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
//
// ULTIMATE ALLIANCE EXPANSION fix 3 - THE PLAYER'S OWN SM2 SPIDER-MAN IN A UA PLACE (owner: "my Spider-Man, not
// MUA's"). The game keeps drawing its hero: native_body poses him to the headset and the controllers and the eyes
// sit in his head, so his body is already right under the player's eyes - only the UA place's black box and room,
// drawn over the game's picture, hid him. Now the overlay CUTS them where the game's picture is the hero: a pixel
// is his when the game's own depth there puts its surface inside a BODY-SHAPED VOLUME round the tracked player
// (capsules for the torso, the arms to the controllers, the legs to the game's ground) and above his feet (the
// street under him never shows). There the overlay writes the game surface's depth and no colour, so the box behind
// is not drawn (the game's hero shows) and room furniture in front of him still is.
//
// The volume is drawn as PROXIES: a box round each capsule (D3D12Renderer::setBodyCut), each corner carrying its
// capsule (Vertex: position the corner, color the capsule's end a, shape.xyz its end b, shape.w its radius); the
// pixel shader takes the game's depth, finds the game's surface point along its ray and keeps it only inside that
// capsule. Pure here (the capsules, the proxies and the same test in C++): the tests check them.
#include "math.hpp"
#include "vertex.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace spidy::ua_body_cut {

struct Capsule {
    Vec3 a{}, b{};
    float r{};
};
inline constexpr int most = 16;
// Kept only this far above the feet (the game's ground under them: never the street).
inline constexpr float footLift = .03f;

// The tracked player (world): the eyes, where they look, the game's ground under his feet (the hero's feet: the
// game's body stands on the game's ground), and the grips (OpenXR: -z through the fist).
struct Player {
    Vec3 eye{}, eyeForward{0, 0, -1};
    float footY{};
    bool hand[2]{};
    Vec3 grip[2]{};
};

inline bool fine(Vec3 v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }

// The body's capsules, generous (the game's body is posed toward the same targets by native_body; its suit is
// thicker than a skeleton). No head and no neck (the game shrinks the head away: the eyes are in it). 0: none.
inline int capsules(const Player& p, Capsule out[most]) {
    if (!fine(p.eye) || !fine(p.eyeForward) || !std::isfinite(p.footY))
        return 0;
    const float h = p.eye.y - p.footY;
    if (!(h > .6f && h < 2.6f))
        return 0;
    const float s = std::clamp(h / 1.65f, .7f, 1.3f);
    const Vec3 up{0, 1, 0};
    Vec3 fwd{p.eyeForward.x, 0, p.eyeForward.z};
    fwd = length(fwd) > .1f ? normalized(fwd) : Vec3{0, 0, -1};
    const Vec3 right = normalized(cross(fwd, up)); // the player's right
    const Vec3 under{p.eye.x, p.footY, p.eye.z};
    const Vec3 neck = under + up * (h - .2f * s) - fwd * (.07f * s);
    const Vec3 pelvis = under + up * (.95f * s) - fwd * (.04f * s);
    int n = 0;
    out[n++] = {pelvis, neck - up * (.1f * s), .2f * s}; // the torso (chest to hips)
    const Vec3 shoulder[2] = {neck - right * (.19f * s) - up * (.03f * s), neck + right * (.19f * s) - up * (.03f * s)};
    for (int i = 0; i < 2; ++i) { // [0] the left, [1] the right
        const float side = i ? 1.f : -1.f;
        const bool tracked = p.hand[i] && fine(p.grip[i]) && length(p.grip[i] - shoulder[i]) < 1.6f * s;
        const Vec3 hand = tracked ? p.grip[i] : shoulder[i] - up * (.62f * s) + right * (side * .04f * s);
        const Vec3 elbow = (shoulder[i] + hand) * .5f - up * (.1f * s) + right * (side * .06f * s);
        out[n++] = {shoulder[i], elbow, .1f * s};
        out[n++] = {elbow, hand, .09f * s};
        Vec3 way = hand - elbow;
        way = length(way) > .05f ? normalized(way) : -up;
        out[n++] = {hand - way * (.04f * s), hand + way * (.1f * s), .11f * s}; // the hand round the controller
    }
    for (int i = 0; i < 2; ++i) {
        const float side = i ? 1.f : -1.f;
        const Vec3 hip = pelvis + right * (side * .1f * s);
        const Vec3 foot = Vec3{hip.x, p.footY + .08f * s, hip.z} + fwd * (.03f * s);
        const Vec3 knee = (hip + foot) * .5f + fwd * (.07f * s);
        out[n++] = {hip, knee, .12f * s};
        out[n++] = {knee, foot, .1f * s};
        out[n++] = {foot - fwd * (.07f * s), foot + fwd * (.2f * s), .09f * s}; // the foot
    }
    out[n++] = {neck - up * (.12f * s), neck, .09f * s}; // the shoulders' join (no head)
    return n;
}

inline bool inCapsule(const Capsule& c, Vec3 p) {
    const Vec3 ab = c.b - c.a;
    const float l2 = dot(ab, ab);
    const float t = l2 > 1e-8f ? std::clamp(dot(p - c.a, ab) / l2, 0.f, 1.f) : 0.f;
    return length(p - (c.a + ab * t)) <= c.r;
}
// The shader's test: inside some capsule and above the feet.
inline bool inside(const Capsule* c, int n, Vec3 p, float footY) {
    if (!fine(p) || p.y < footY + footLift)
        return false;
    for (int i = 0; i < n; ++i)
        if (inCapsule(c[i], p))
            return true;
    return false;
}

// Each capsule's proxy: the box round it (its axis, two across; r past each end), 12 triangles.
inline void appendProxies(std::vector<Vertex>& out, const Capsule* c, int n) {
    for (int i = 0; i < n; ++i) {
        const Capsule& k = c[i];
        if (!fine(k.a) || !fine(k.b) || !(k.r > 0) || !std::isfinite(k.r))
            continue;
        Vec3 ax = k.b - k.a;
        const float l = length(ax);
        ax = l > 1e-4f ? ax * (1.f / l) : Vec3{0, 1, 0};
        const Vec3 helper = std::abs(ax.y) < .9f ? Vec3{0, 1, 0} : Vec3{1, 0, 0};
        const Vec3 u = normalized(cross(ax, helper)), v = cross(ax, u);
        const float r = k.r * 1.05f; // the box's faces a little outside the capsule
        Vec3 q[8];
        for (int j = 0; j < 8; ++j)
            q[j] = (j & 1 ? k.b + ax * r : k.a - ax * r) + u * (j & 2 ? r : -r) + v * (j & 4 ? r : -r);
        static constexpr int f[6][4] = {{0, 2, 6, 4}, {1, 5, 7, 3}, {0, 4, 5, 1}, {2, 3, 7, 6}, {0, 1, 3, 2}, {4, 6, 7, 5}};
        for (const auto& s : f)
            for (const int j : {s[0], s[1], s[2], s[0], s[2], s[3]})
                out.push_back(Vertex{q[j], k.a, 1.f, {k.b.x, k.b.y, k.b.z, k.r}});
    }
}

// v3 gave the cut up for the session once 240 requested frames had drawn nothing and then showed MUA's body instead
// (ua_body::Source::uaBody). v4 never gives up and has no stand-in: the cut is asked for every frame the game's hero is
// drawn (ua_body::pick -> gameHero) and the room alone covers him until the cut comes through. What stops him showing is
// said, not hidden: ua_body_cut::why() names the first thing missing, game_xr.cpp logs it and puts it on screen.
enum class Why : uint8_t { shown, notInUa, notRequested, noDepth, cutFault, heroNotPosed };
inline constexpr const char* whyNames[6] = {"his own hero is shown through the room",
                                            "not in a UA place",
                                            "no cut requested yet (head or hands not tracked)",
                                            "the cut was requested but the game's depth never arrived (scene depth capture)",
                                            "the renderer switched the cut off (see the fault)",
                                            "native_body is not posing the game's hero (the game's body switch, the rig or a job that does not run)"};
// The first thing missing, given: in a UA place, native_body drawing the hero, a cut requested lately, the cuts the
// renderer drew since entering, the renderer's fault (non-empty = off).
constexpr Why why(bool inUa, bool nativeBodyDrawn, bool requested, uint64_t drawnSinceEntering, bool fault) {
    if (!inUa)
        return Why::notInUa;
    if (!nativeBodyDrawn)
        return Why::heroNotPosed;
    if (fault)
        return Why::cutFault;
    if (!requested)
        return Why::notRequested;
    return drawnSinceEntering > 0 ? Why::shown : Why::noDepth;
}

} // namespace spidy::ua_body_cut
