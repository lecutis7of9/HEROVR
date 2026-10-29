#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
//
// 0.0.73 STREAMER MODE (wrist BODY AND VIEW: STREAMER MODE, STREAMER SMOOTH, STREAMER EYE):
// the game's window (the PC monitor, a stream's capture) shows exactly what the headset
// shows - a copy of one eye's final image, every drawing of this mod on it (the hands,
// the hero models, Cap's shield, Thor's hammer and lightning, the hologram and wrist
// menus, the Avengers' weapons, batarangs, guns, the van screen, the ghost player) - not
// the game's own image of the eye, which has none of them.
//
// How (native_eye_gpu.cpp): the headset's chosen eye, after the overlay drew into it, is
// copied into one texture (keepShownEye, one GPU copy per monitor picture, never a CPU
// wait); each picture the window takes draws that copy over the game's back buffer
// (mirrorEye) through a rotation-only reprojection: the window looks along a smoothed
// head direction, so head shake is steadied, and the eye's image is cut to the window's
// shape (16:9 on a 16:9 monitor) about the eye's straight-ahead axis. When anything is
// missing (no fresh copy, the pipeline would not build, a float back buffer) the window
// gets what it got before this mode.
//
// Pure: the crop, the smoothing and the shader's mapping, so tests/core_tests.cpp can
// check them; no game address is read here.
#include "spidy/math.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace spidy::streamer_view {

// The wrist values (combat38: value 0 is the default).
inline bool modeOn(uint32_t value) { return value == 0; } // STREAMER MODE: 0 ON, 1 OFF
enum Smooth : uint32_t { smoothLow = 0, smoothOff = 1, smoothHigh = 2, smoothChoices = 3 };
inline uint32_t eyeIndex(uint32_t value) { return value == 1 ? 1u : 0u; } // STREAMER EYE: 0 LEFT EYE, 1 RIGHT EYE

// How long the window's view takes to follow the head (the smoothing's time constant,
// seconds): OFF follows at once, LOW steadies a shake, HIGH a camera's slow pan.
inline float smoothSeconds(uint32_t smooth) {
    switch (smooth) {
    case smoothOff:
        return 0;
    case smoothHigh:
        return .30f;
    default:
        return .12f;
    }
}
// The part of the eye's straight-ahead field (each side) kept free so the smoothed view
// can lag the head without showing the image's edge.
inline float marginOf(uint32_t smooth) {
    switch (smooth) {
    case smoothOff:
        return .02f;
    case smoothHigh:
        return .16f;
    default:
        return .10f;
    }
}

// An eye's field of view, OpenXR's angles in radians (left and down negative).
struct Fov {
    float left{}, right{}, down{}, up{};
};
inline bool usable(const Fov& f) {
    const float limit = 1.55f; // under 89 degrees a side
    return std::isfinite(f.left) && std::isfinite(f.right) && std::isfinite(f.down) && std::isfinite(f.up) &&
           f.left < 0 && f.right > 0 && f.down < 0 && f.up > 0 && -f.left < limit && f.right < limit &&
           -f.down < limit && f.up < limit;
}
// The window's picture in tangents of the view's angles (x right, y up): a rectangle about
// the view's axis. valid false: no crop (the caller keeps the old mirror).
struct Crop {
    float left{}, right{}, down{}, up{};
    bool valid{};
    float aspect() const { return (right - left) / (up - down); }
};
// The window's aspect, sane: a minimised (0 x 0) or absurd window is taken as 16:9.
inline float windowAspect(uint32_t width, uint32_t height) {
    if (!width || !height)
        return 16.f / 9.f;
    const float a = static_cast<float>(width) / static_cast<float>(height);
    return a >= .25f && a <= 6.f ? a : 16.f / 9.f;
}
// The largest rectangle of the window's `aspect` centred on the eye's axis that stays
// inside the eye's field, less `margin` of it each side. The axis, not the image's middle:
// the eye's field is lopsided (wider outward), so the stream looks straight ahead.
inline Crop fitCrop(const Fov& f, float aspect, float margin) {
    Crop c{};
    if (!usable(f) || !(aspect > 0) || !std::isfinite(aspect))
        return c;
    margin = std::clamp(std::isfinite(margin) ? margin : 0.f, 0.f, .5f);
    float halfW = std::min(-std::tan(f.left), std::tan(f.right)) * (1 - margin);
    float halfH = std::min(std::tan(f.up), -std::tan(f.down)) * (1 - margin);
    if (halfW / halfH > aspect)
        halfW = halfH * aspect;
    else
        halfH = halfW / aspect;
    c = {-halfW, halfW, -halfH, halfH, halfW > 0 && halfH > 0};
    return c;
}
// How far (radians) the smoothed view may lag the head before the crop's edge would leave
// the eye's image: the smallest gap between a side of the crop and that side of the field.
inline float slack(const Fov& f, const Crop& c) {
    if (!c.valid || !usable(f))
        return 0;
    const float gaps[] = {-f.left - std::atan(-c.left), f.right - std::atan(c.right), -f.down - std::atan(-c.down),
                          f.up - std::atan(c.up)};
    return std::max(0.f, *std::min_element(std::begin(gaps), std::end(gaps)) * .9f);
}

inline Quat normalizedQuat(Quat q) {
    const float n = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
    if (!(n > 1e-6f) || !std::isfinite(n))
        return {};
    return {q.x / n, q.y / n, q.z / n, q.w / n};
}
inline float quatDot(Quat a, Quat b) { return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w; }
// The angle (radians) between two orientations.
inline float angleBetween(Quat a, Quat b) {
    const float d = std::min(1.f, std::abs(quatDot(normalizedQuat(a), normalizedQuat(b))));
    return 2 * std::acos(d);
}
// From a to b by t (0..1), the short way.
inline Quat slerp(Quat a, Quat b, float t) {
    a = normalizedQuat(a);
    b = normalizedQuat(b);
    float d = quatDot(a, b);
    if (d < 0) {
        b = {-b.x, -b.y, -b.z, -b.w};
        d = -d;
    }
    t = std::clamp(t, 0.f, 1.f);
    if (d > .9995f)
        return normalizedQuat({a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t,
                               a.w + (b.w - a.w) * t});
    const float theta = std::acos(d), s = std::sin(theta);
    const float wa = std::sin((1 - t) * theta) / s, wb = std::sin(t * theta) / s;
    return normalizedQuat({a.x * wa + b.x * wb, a.y * wa + b.y * wb, a.z * wa + b.z * wb, a.w * wa + b.w * wb});
}

// The window's view direction, following the head's with a lag of `seconds` and never
// more than `limit` radians behind it. A big jump (a recentre, a gap of more than half a
// second, a bad pose) starts afresh at the head.
struct Smoother {
    Quat view{};
    bool primed{};
    void reset() { primed = false; }
    Quat step(Quat head, float dt, float seconds, float limit) {
        head = normalizedQuat(head);
        if (!primed || !(seconds > 0) || !std::isfinite(dt) || dt < 0 || dt > .5f ||
            angleBetween(view, head) > .8f) {
            view = head;
            primed = true;
            return view;
        }
        view = slerp(view, head, 1 - std::exp(-dt / seconds));
        const float off = angleBetween(view, head);
        if (limit >= 0 && off > limit && off > 0)
            view = slerp(head, view, limit / off);
        return view;
    }
};

// The shader's turn (row-major 3 x 3): a direction in the window's (smoothed) view to the
// same direction in the eye's (head) view: inverse(head) * smoothed.
struct Turn {
    float m[9]{1, 0, 0, 0, 1, 0, 0, 0, 1};
};
inline Turn turnOf(Quat head, Quat smoothed) {
    const Quat q = normalizedQuat(normalizedQuat(head).conjugate() * normalizedQuat(smoothed));
    const Vec3 x = q.rotate({1, 0, 0}), y = q.rotate({0, 1, 0}), z = q.rotate({0, 0, 1});
    // columns are the turned axes
    return {{x.x, y.x, z.x, x.y, y.y, z.y, x.z, y.z, z.z}};
}
// What the shader samples for the window's point (u, v) (0..1, v down): the eye image's
// (s, t) (0..1, t down); false off the image (drawn black).
inline bool sample(const Turn& turn, const Crop& c, const Fov& f, float u, float v, float& s, float& t) {
    const float tx = c.left + (c.right - c.left) * u, ty = c.up + (c.down - c.up) * v;
    const float* m = turn.m;
    const float ex = m[0] * tx + m[1] * ty - m[2], ey = m[3] * tx + m[4] * ty - m[5], ez = m[6] * tx + m[7] * ty - m[8];
    if (ez > -1e-4f)
        return false;
    const float sx = ex / -ez, sy = ey / -ez;
    const float l = std::tan(f.left), r = std::tan(f.right), d = std::tan(f.down), up = std::tan(f.up);
    s = (sx - l) / (r - l);
    t = (up - sy) / (up - d);
    return s >= 0 && s <= 1 && t >= 0 && t <= 1;
}

// The 24 root constants the streamer shader takes (six float4s): the turn's rows, the
// crop's tangents, the eye's tangents, the window's pixel size.
struct Constants {
    float c[24]{};
};
inline Constants constantsOf(const Turn& turn, const Crop& c, const Fov& f, uint32_t width, uint32_t height) {
    Constants k{};
    for (int row = 0; row < 3; ++row)
        for (int col = 0; col < 3; ++col)
            k.c[row * 4 + col] = turn.m[row * 3 + col];
    k.c[12] = c.left, k.c[13] = c.right, k.c[14] = c.down, k.c[15] = c.up;
    k.c[16] = std::tan(f.left), k.c[17] = std::tan(f.right), k.c[18] = std::tan(f.down), k.c[19] = std::tan(f.up);
    k.c[20] = width ? 1.f / static_cast<float>(width) : 0;
    k.c[21] = height ? 1.f / static_cast<float>(height) : 0;
    return k;
}

} // namespace spidy::streamer_view
