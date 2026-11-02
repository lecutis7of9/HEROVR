#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
//
// 0.0.76 A-depth CHECKED (live.txt vr_taa_depth, still off by default): before SPIDY TAA trusts the eyes' depth to
// move its history, the depth's convention is checked, and every 10 s the depth path's own numbers are logged
// (SpidyTaaDepthCheckData, tools/taa_depth_check_record.py). Read-only: nothing here changes a picture, the TAA's
// blend shares, its clip, the lens calls or the game's own TAA.
//
// The check (one pair in 30 until it passes, then one a second; the pass stays off until it has passed):
//  - the eye's view-projection is built as the lens was (native lens 0x2c75560, CONFIRMED: the eye's pose, its
//    four tangents, the near and far the lens was given, this pair's sub-pixel step: the JITTERED matrix), in the
//    lens's own convention: D3D 0..1, reversed-Z, reciprocal, finite far. The descriptor's own matrix is not read
//    (no CONFIRMED offset for it).
//  - round trip: the centre pixel (or the nearest pixel on the centre row with a depth, its offset logged) is put
//    back in the world through that matrix's inverse with its depth, and projected back through the same matrix;
//    the error in pixels must be 0.05 or less. That proves the matrix and its inverse; it cannot tell the depth's
//    convention (any depth moves the point along the pixel's own ray), so the convention is told by the stereo
//    test next.
//  - stereo test: the left eye's centre row (the points near a depth edge) is put in the world with each convention
//    in turn (reversed-Z finite, reversed-Z infinite, standard 0..1, GL -1..1, linear) and projected into the right
//    eye: the right eye's own depth there must give the same distance (3%). Only near an edge does a wrong distance
//    (a wrong disparity) land on another surface. The convention that fits best is the depth's; the pass is on
//    only when that is reversed-Z 0..1 reciprocal (finite or infinite far, the one the shader is then given) and it
//    fits clearly (60% of the points, 10 points ahead of every other convention). Flat walls square to the eyes fit
//    any convention: the test is then inconclusive and the pass waits.
// No motion vectors, no object motion: static geometry only (the eyes' own travel and turn).
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

#include "spidy/eye_history.hpp"

namespace spidy::taa_depth_check {
enum Convention : uint32_t {
    reversedFinite = 0,   // D3D 0..1, reversed-Z, reciprocal, finite far: d = n (f - z) / (z (f - n)) (the lens's)
    reversedInfinite = 1, // D3D 0..1, reversed-Z, reciprocal, infinite far: d = n / z (the shader's first guess)
    standard = 2,         // D3D 0..1, standard Z, reciprocal: d = f (z - n) / (z (f - n))
    gl = 3,               // GL -1..1, standard Z, reciprocal: ndc = (f + n - 2 f n / z) / (f - n), d = ndc / 2 + 1 / 2
    linear = 4,           // linear: d = z / f
    conventions = 5,
};
inline const char* conventionName(uint32_t c) {
    switch (c) {
    case reversedFinite: return "reversed-Z, D3D 0..1, reciprocal, finite far";
    case reversedInfinite: return "reversed-Z, D3D 0..1, reciprocal, infinite far";
    case standard: return "standard Z, D3D 0..1, reciprocal";
    case gl: return "GL -1..1, standard Z, reciprocal";
    case linear: return "linear 0..1";
    default: return "unknown";
    }
}
// The view distance (along the eye's forward axis, metres) a stored depth means in a convention; NaN if none.
inline double distanceOf(uint32_t c, double d, double n, double f) {
    if (!std::isfinite(d) || !(n > 0) || !(f > n))
        return NAN;
    double z = NAN;
    switch (c) {
    case reversedFinite: z = n * f / (d * (f - n) + n); break;
    case reversedInfinite: z = d > 0 ? n / d : NAN; break;
    case standard: z = f * n / (f - d * (f - n)); break;
    case gl: z = 2 * f * n / ((f + n) - (2 * d - 1) * (f - n)); break;
    case linear: z = d * f; break;
    default: break;
    }
    return std::isfinite(z) && z > 0 ? z : NAN;
}
// A stored depth that means no surface for the depth path: 0 (cleared, sky), 1 (the near plane), NaN, or outside.
inline bool usable(float d) {
    return std::isfinite(d) && d > 0 && d < 1;
}
// One eye as its lens was built: the pose (EyeView::world), tangents, near, far, this pair's step (eye pixels, x
// right, y up, as the lens is given it) and the eye's size in pixels.
struct Lens {
    eye_history::EyeView view{};
    double nearM{}, farM{};
    double jitter[2]{};
    uint32_t width{}, height{};
};
using Matrix = std::array<double, 16>; // row-major, column vectors
inline Matrix multiply(const Matrix& a, const Matrix& b) {
    Matrix o{};
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c)
            for (int k = 0; k < 4; ++k)
                o[r * 4 + c] += a[r * 4 + k] * b[k * 4 + c];
    return o;
}
// The jittered view-projection the lens renders with (reversedFinite; reversedInfinite with `infinite`).
inline bool viewProjection(const Lens& lens, bool infinite, Matrix& out) {
    const auto& t = lens.view.tangents;
    const double l = t[0], r = t[1], dn = t[2], up = t[3], n = lens.nearM, f = lens.farM;
    if (!(r > l) || !(up > dn) || !(n > 0) || (!infinite && !(f > n)) || !lens.width || !lens.height)
        return false;
    const double ox = 2 * lens.jitter[0] / lens.width, oy = 2 * lens.jitter[1] / lens.height;
    Matrix p{};
    p[0] = 2 / (r - l);
    p[2] = (r + l) / (r - l) - ox;
    p[5] = 2 / (up - dn);
    p[6] = (up + dn) / (up - dn) - oy;
    p[10] = infinite ? 0 : n / (f - n);
    p[11] = infinite ? n : n * f / (f - n);
    p[14] = -1;
    // The view: the eye's right, up and back (EyeView: x right, y down, z forward) about its position.
    const auto& w = lens.view.world;
    Matrix v{};
    for (int a = 0; a < 3; ++a) {
        const double sign = a == 0 ? 1 : -1;
        double dot = 0;
        for (int k = 0; k < 3; ++k) {
            v[a * 4 + k] = sign * w[a * 4 + k];
            dot += v[a * 4 + k] * w[12 + k];
        }
        v[a * 4 + 3] = -dot;
    }
    v[15] = 1;
    out = multiply(p, v);
    for (double x : out)
        if (!std::isfinite(x))
            return false;
    return true;
}
inline bool invert(const Matrix& m, Matrix& out) {
    std::array<double, 32> a{};
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c) {
            a[r * 8 + c] = m[r * 4 + c];
            a[r * 8 + 4 + c] = r == c ? 1 : 0;
        }
    for (int c = 0; c < 4; ++c) {
        int best = c;
        for (int r = c + 1; r < 4; ++r)
            if (std::fabs(a[r * 8 + c]) > std::fabs(a[best * 8 + c]))
                best = r;
        if (!(std::fabs(a[best * 8 + c]) > 1e-300))
            return false;
        if (best != c)
            for (int k = 0; k < 8; ++k)
                std::swap(a[c * 8 + k], a[best * 8 + k]);
        const double pivot = a[c * 8 + c];
        for (int k = 0; k < 8; ++k)
            a[c * 8 + k] /= pivot;
        for (int r = 0; r < 4; ++r)
            if (r != c) {
                const double s = a[r * 8 + c];
                for (int k = 0; k < 8; ++k)
                    a[r * 8 + k] -= s * a[c * 8 + k];
            }
    }
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c)
            out[r * 4 + c] = a[r * 8 + 4 + c];
    return true;
}
inline std::array<double, 4> apply(const Matrix& m, const std::array<double, 4>& x) {
    std::array<double, 4> o{};
    for (int r = 0; r < 4; ++r)
        for (int k = 0; k < 4; ++k)
            o[r] += m[r * 4 + k] * x[k];
    return o;
}
// A pixel (its centre: x + .5, y + .5 from the top left) and its stored depth into the world through the inverse.
inline bool unproject(const Matrix& inverse, double px, double py, double depth, uint32_t width, uint32_t height,
                      std::array<double, 3>& world) {
    const double x = 2 * px / width - 1, y = 1 - 2 * py / height;
    const auto h = apply(inverse, {x, y, depth, 1});
    if (!(std::fabs(h[3]) > 1e-300))
        return false;
    world = {h[0] / h[3], h[1] / h[3], h[2] / h[3]};
    return std::isfinite(world[0] + world[1] + world[2]);
}
// A world point through a view-projection: its pixel, the depth it would store and its view distance (w).
inline bool project(const Matrix& vp, const std::array<double, 3>& world, uint32_t width, uint32_t height, double& px,
                    double& py, double& depth, double& distance) {
    const auto c = apply(vp, {world[0], world[1], world[2], 1});
    if (!(c[3] > 1e-9))
        return false;
    px = (c[0] / c[3] + 1) * .5 * width;
    py = (1 - c[1] / c[3]) * .5 * height;
    depth = c[2] / c[3];
    distance = c[3];
    return std::isfinite(px + py + depth);
}
// The round trip for one pixel: through the inverse with its depth and back through the same matrix. The error in
// pixels (NaN when the matrix cannot be inverted); `distance` the view distance it found, `inFrustum` whether that
// lies between the lens's near and far.
struct Trip {
    double errorPx = NAN, distance = NAN;
    bool inFrustum{};
};
inline Trip roundTrip(const Lens& lens, bool infinite, double px, double py, double depth) {
    Trip t;
    Matrix vp{}, inverse{};
    std::array<double, 3> world{};
    double x{}, y{}, d{}, w{};
    if (!viewProjection(lens, infinite, vp) || !invert(vp, inverse) ||
        !unproject(inverse, px, py, depth, lens.width, lens.height, world) ||
        !project(vp, world, lens.width, lens.height, x, y, d, w))
        return t;
    t.errorPx = std::hypot(x - px, y - py);
    t.distance = w;
    t.inFrustum = w >= lens.nearM * (1 - 1e-4) && (infinite || w <= lens.farM * (1 + 1e-4));
    return t;
}
// A band of rows about each eye's centre row (both eyes the same rows), as read back: band[row * pitch + x].
struct Band {
    const float* depth{};
    uint32_t firstRow{}, rows{}, pitch{};
    float at(uint32_t x, uint32_t y) const {
        if (!depth || y < firstRow || y >= firstRow + rows)
            return NAN;
        return depth[static_cast<size_t>(y - firstRow) * pitch + x];
    }
};
inline constexpr uint32_t bandRows = 33;             // the centre row and 16 each side
inline constexpr uint32_t stereoStep = 2;            // one left-eye pixel in 2 along its centre row
inline constexpr double stereoTolerance = .03;       // the right eye's distance there within 3%
inline constexpr uint32_t stereoLeast = 48;          // points near an edge needed for a verdict
inline constexpr double fitLeast = .6, fitLead = .1; // the winner fits 60% and leads every other by 10 points
inline constexpr double roundTripLimit = .05;        // pixels
struct Stereo {
    uint32_t tested{};
    std::array<uint32_t, conventions> agreed{};
    double share(uint32_t c) const {
        return tested ? static_cast<double>(agreed[c]) / tested : 0;
    }
};
// A point near a depth edge (within edgeReach pixels along its row the depth changes by edgeRatio, or the sky
// begins): only there does the place a convention puts it in the other eye tell the conventions apart. On a wall
// square to the eyes every convention fits, whatever distance it reads.
inline constexpr uint32_t edgeReach = 12;
inline constexpr float edgeRatio = 1.15f;
inline bool edge(const Band& b, uint32_t x, uint32_t row, uint32_t width) {
    // Both d and 1 - d: a standard (not reversed) depth sits near 1, where only 1 - d shows the edge.
    float lo = INFINITY, hi = 0, farLo = INFINITY, farHi = 0;
    for (int64_t k = -static_cast<int64_t>(edgeReach); k <= static_cast<int64_t>(edgeReach); ++k) {
        const int64_t at = static_cast<int64_t>(x) + k;
        if (at < 0 || at >= static_cast<int64_t>(width))
            continue;
        const float d = b.at(static_cast<uint32_t>(at), row);
        if (!usable(d))
            return true;
        lo = std::fmin(lo, d);
        hi = std::fmax(hi, d);
        farLo = std::fmin(farLo, 1.0f - d);
        farHi = std::fmax(farHi, 1.0f - d);
    }
    return (lo > 0 && hi / lo >= edgeRatio) || (farLo > 0 && farHi / farLo >= edgeRatio);
}
// The stereo test: the left eye's centre row (its middle 80%, points near a depth edge) through each convention
// into the right eye.
inline Stereo stereo(const Lens& left, const Lens& right, const Band& l, const Band& r) {
    Stereo s;
    Matrix vpL{}, invL{}, vpR{};
    if (!viewProjection(left, false, vpL) || !invert(vpL, invL) || !viewProjection(right, false, vpR) ||
        left.width != right.width || left.height != right.height)
        return s;
    const uint32_t row = left.height / 2;
    const double n = left.nearM, f = left.farM;
    for (uint32_t x = left.width / 10; x < left.width - left.width / 10; x += stereoStep) {
        const float d = l.at(x, row);
        if (!usable(d) || !edge(l, x, row, left.width))
            continue;
        ++s.tested;
        for (uint32_t c = 0; c < conventions; ++c) {
            const double z = distanceOf(c, d, n, f);
            if (!std::isfinite(z))
                continue;
            // The point at that distance on the pixel's ray (the lens's own depth for it, reversedFinite).
            const double stored = (n * f / z - n) / (f - n);
            std::array<double, 3> world{};
            double px{}, py{}, pd{}, pw{};
            if (!unproject(invL, x + .5, row + .5, stored, left.width, left.height, world) ||
                !project(vpR, world, right.width, right.height, px, py, pd, pw))
                continue;
            const auto rx = static_cast<int64_t>(std::floor(px)), ry = static_cast<int64_t>(std::floor(py));
            if (ry < 0 || rx < 1 || rx + 1 >= static_cast<int64_t>(right.width) || ry >= static_cast<int64_t>(right.height))
                continue;
            bool fits = false;
            for (int64_t k = -1; k <= 1 && !fits; ++k) {
                const float dr = r.at(static_cast<uint32_t>(rx + k), static_cast<uint32_t>(ry));
                const double zr = usable(dr) ? distanceOf(c, dr, n, f) : NAN;
                fits = std::isfinite(zr) && std::fabs(zr - pw) <= stereoTolerance * pw;
            }
            s.agreed[c] += fits;
        }
    }
    return s;
}
enum State : uint32_t { waiting = 0, passed = 1, failed = 2 };
enum Reason : uint32_t {
    fine = 0,
    roundTripOver = 1,  // the round trip missed by more than 0.05 px: the matrix or its inverse is wrong
    noCentreDepth = 2,  // no pixel on either eye's centre row had a depth (0, 1, NaN everywhere)
    fewPoints = 3,      // the stereo test had fewer than 48 points near a depth edge
    notClear = 4,       // no convention fitted clearly (flat walls fit them all)
    otherConvention = 5,// another convention fitted clearly better than reversed-Z 0..1 reciprocal
    noLens = 6,         // no near / far from the eye lens yet
    outsideFrustum = 7, // the centre depth put the point outside the eye's frustum
};
inline const char* reasonText(uint32_t r) {
    switch (r) {
    case fine: return "ok";
    case roundTripOver: return "round trip over 0.05 px: the matrix or its inverse is wrong";
    case noCentreDepth: return "no depth on the centre row (0, 1 or NaN)";
    case fewPoints: return "too few stereo points near a depth edge";
    case notClear: return "no convention fitted clearly (flat surfaces fit all)";
    case otherConvention: return "another convention fits better than reversed-Z 0..1 reciprocal";
    case noLens: return "no near/far from the eye lens yet";
    case outsideFrustum: return "the centre depth lies outside the eye frustum";
    default: return "unknown";
    }
}
struct Verdict {
    uint32_t outcome{waiting}; // passed, failed, or waiting (inconclusive: decides nothing)
    uint32_t reason{fine};
    uint32_t convention{conventions}; // the best fit (conventions: none)
};
// The verdict of one check from its round trips (NaN: not measured) and its stereo test.
inline Verdict decide(const double trip[2], const bool inFrustum[2], const Stereo& s) {
    Verdict v;
    for (int i = 0; i < 2; ++i)
        if (std::isfinite(trip[i]) && trip[i] > roundTripLimit) {
            v.outcome = failed;
            v.reason = roundTripOver;
            return v;
        }
    for (int i = 0; i < 2; ++i)
        if (std::isfinite(trip[i]) && !inFrustum[i]) {
            v.outcome = failed;
            v.reason = outsideFrustum;
            return v;
        }
    if (!std::isfinite(trip[0]) || !std::isfinite(trip[1])) {
        v.reason = noCentreDepth;
        return v;
    }
    if (s.tested < stereoLeast) {
        v.reason = fewPoints;
        return v;
    }
    uint32_t best = 0;
    for (uint32_t c = 1; c < conventions; ++c)
        if (s.share(c) > s.share(best) + 1e-12)
            best = c;
    // Finite and infinite far differ only past a few kilometres: a near tie goes to the lens's own (finite).
    if (best == reversedInfinite && s.share(reversedInfinite) - s.share(reversedFinite) < .02)
        best = reversedFinite;
    v.convention = best;
    const bool reversed = best == reversedFinite || best == reversedInfinite;
    double rival = 0;
    for (uint32_t c = 0; c < conventions; ++c) {
        const bool sameFamily = (c == reversedFinite || c == reversedInfinite) == reversed;
        if (!sameFamily)
            rival = std::fmax(rival, s.share(c));
    }
    if (s.share(best) < fitLeast || s.share(best) - rival < fitLead) {
        v.reason = notClear;
        return v;
    }
    v.outcome = reversed ? passed : failed;
    v.reason = reversed ? fine : otherConvention;
    return v;
}
// What the check's verdict does to the session's state: a pass turns the depth path on, a clear failure turns it
// off for the session (logged), an inconclusive check changes nothing.
inline uint32_t nextState(uint32_t state, const Verdict& v) {
    if (state == failed)
        return failed;
    if (v.outcome == failed)
        return failed;
    if (v.outcome == passed)
        return passed;
    return state;
}
// The shader's depth (taaShader TAA_DEPTH, `lin`): a stored depth d to near / distance, d * scale + offset.
inline void linearOf(uint32_t convention, double n, double f, float& scale, float& offset) {
    if (convention == reversedFinite && n > 0 && f > n) {
        scale = static_cast<float>(1 - n / f);
        offset = static_cast<float>(n / f);
    } else {
        scale = 1;
        offset = 0;
    }
}

// The 10 s log (SpidyTaaDepthCheckData).
// Movement: still (taa_watch::stillHead: under 2 degrees and 3 cm a second), fast swing (the eye travelled 6 m a
// second or more, or turned 120 degrees a second or more), moving (anything between).
inline constexpr float fastTravelPerSecond = 6.f;     // metres a second
inline constexpr float fastTurnPerSecond = 2.0944f;   // radians a second, 120 degrees
enum Category : uint32_t { stillCategory = 0, movingCategory = 1, fastCategory = 2, categories = 3 };
inline uint32_t category(bool still, float turn, float travel, float seconds) {
    if (still)
        return stillCategory;
    if (seconds > 0 && std::isfinite(seconds) && std::isfinite(turn) && std::isfinite(travel) &&
        (travel / seconds >= fastTravelPerSecond || turn / seconds >= fastTurnPerSecond))
        return fastCategory;
    return movingCategory;
}
enum Path : uint32_t { headPath = 0, depthPath = 1, paths = 2 };
// One pixel in 16 x 16 of both eyes, counted by the shader (TAA_COUNT), per path and movement.
struct Counts {
    uint32_t pairs{};       // pairs whose counts came back
    uint32_t pixels{};      // counted pixels
    uint32_t sampled{};     // of those, with a history place in the image (the rest start again)
    uint32_t clipOnly{};    // the variance clip moved the history, the depth test passed it (or did not run)
    uint32_t depthOnly{};   // the depth test refused the history, the clip would not have moved it
    uint32_t both{};        // the depth test refused it and the clip would have moved it
    uint32_t fallback{};    // the depth path wanted, the head-motion place used (depth 0, 1, NaN, out of the frustum)
    uint32_t depthTested{}; // the history place came from the depth and its test ran
};
static_assert(sizeof(Counts) == 32);
// The shader's counters for one eye (8 x 32-bit, at 32 * eye in a slot's 64 bytes), in this order.
enum Counter : uint32_t {
    sampledAt = 0, rejectedAt = 1, clipOnlyAt = 2, depthOnlyAt = 3, bothAt = 4, fallbackAt = 5, depthTestedAt = 6,
    pixelsAt = 7, counters = 8
};
struct Window {
    uint32_t ms{};
    uint32_t pairs{};      // SPIDY TAA pairs
    uint32_t wanted{};     // of those, with vr_taa_depth on
    uint32_t found[2]{};   // of those, eye 0's / eye 1's depth there this pair
    uint32_t used{};       // turned with the depth (both eyes)
    uint32_t gated{};      // an eye's depth (this pair's or the last's) missing: both eyes turned as before
    uint32_t unchecked{};  // both there, the convention check not passed yet: both eyes turned as before
    uint32_t refused{};    // both there, the convention check failed: both eyes turned as before
    uint32_t copyTimed{};
    float copyMs{};        // the two depth copies on the game's lists, summed
    uint32_t depthDrawTimed{};
    float depthDrawMs{};   // SPIDY TAA's pass (both eyes) on pairs turned with the depth, summed
    uint32_t plainDrawTimed{};
    float plainDrawMs{};   // the same pass on the other pairs, summed
    uint32_t checks{}, checksPassed{};
    float roundTripMax{}, roundTripLast{};
    Counts counts[paths][categories];
    uint32_t spare{};
};
static_assert(sizeof(Window) == 272);
struct Check {
    uint32_t probes{}, passed{}, failed{}, inconclusive{};
    uint32_t state{};       // State
    uint32_t convention{};  // the last verdict's best fit (conventions: none)
    uint32_t reason{};      // Reason of the last check
    uint32_t tested{};      // stereo points near a depth edge
    float agree[conventions]{};
    float roundTrip[2]{};   // px, per eye (NaN: no depth on the centre row)
    float centreDepth[2]{}; // the stored depth used
    float centreDistance[2]{};
    int32_t centreOffset[2]{}; // pixels along the centre row from the centre to the pixel used
    float nearM{}, farM{};
    uint32_t clearReversed{}, clearStandard{}; // the G-buffer's depth clears seen: to 0 (reversed), to 1
    uint32_t inForce{};     // the convention the shader was given (conventions: none yet)
    uint32_t serial{};      // the pair last checked
    uint32_t spare{};
};
static_assert(sizeof(Check) == 112);
inline constexpr uint32_t windowCount = 90;
struct Data {
    uint32_t magic = 0x4b434454; // TDCK
    uint32_t version = 1;
    uint32_t bytes = 24888;
    uint32_t windowMs = 10000;
    uint32_t closed{};
    uint32_t spare{};
    Check check;
    Window current;
    Window windows[windowCount];
};
static_assert(sizeof(Data) == 24888);
inline void roll(Data& d, uint64_t& startMs, uint64_t nowMs) {
    if (!startMs || nowMs < startMs) {
        startMs = nowMs;
        return;
    }
    if (nowMs - startMs >= d.windowMs) {
        d.current.ms = static_cast<uint32_t>(nowMs - startMs > 0xffffffffull ? 0xffffffffull : nowMs - startMs);
        d.windows[d.closed % windowCount] = d.current;
        ++d.closed;
        d.current = {};
        startMs = nowMs;
    }
}
// One eye pair's shader counters (both eyes, as differences) into a window.
inline void noteCounts(Window& w, uint32_t path, uint32_t category, const uint32_t eyes[2][counters]) {
    if (path >= paths || category >= categories)
        return;
    auto& c = w.counts[path][category];
    ++c.pairs;
    for (int i = 0; i < 2; ++i) {
        const uint32_t* e = eyes[i];
        if (e[clipOnlyAt] + e[depthOnlyAt] + e[bothAt] != e[rejectedAt] || e[rejectedAt] > e[sampledAt] ||
            e[sampledAt] > e[pixelsAt])
            continue; // not one pair's counts
        c.pixels += e[pixelsAt];
        c.sampled += e[sampledAt];
        c.clipOnly += e[clipOnlyAt];
        c.depthOnly += e[depthOnlyAt];
        c.both += e[bothAt];
        c.fallback += e[fallbackAt];
        c.depthTested += e[depthTestedAt];
    }
}
} // namespace spidy::taa_depth_check
