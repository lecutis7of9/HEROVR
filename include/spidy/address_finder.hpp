#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
//
// Update 27 / 27.3: pure helpers for the ADDRESS FINDER (candidates only; never invent
// an SM2 address). The native side (native_address_finder.cpp) feeds camera
// descriptors and GPU/CPU bytes; this header matches matrices, jitter and
// projections, rejects trivial values, confirms multi-frame hits, builds
// wildcarded code signatures, finds RIP-relative LEA xrefs and .pdata
// function starts, and escapes JSON Lines for the report.
#include "native_view.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

namespace spidy::address_finder {

inline constexpr float matrixTol = 2e-3f;
inline constexpr float positionTol = .05f;
inline constexpr float jitterTol = 2e-4f; // pre-28 absolute (kept for reference/tests)
// Update 28: jitter is relative (27.4 run: 117 noise hits at ~25% error of a 4e-4 jitter).
inline constexpr float jitterRelTol = .03f;
inline constexpr float jitterAbsFloor = 2e-6f;
// Update 28: combined view*projection rows (relative error) and the "almost" band.
inline constexpr float viewProjTol = .02f;
inline constexpr float nearMissFactor = 4.f;
inline constexpr unsigned cameraLags = 3; // also match the last 3 stock cameras (CB built a frame late)
inline constexpr float projTol = 5e-3f;
inline constexpr unsigned signatureBytes = 32;
// Update 27.3: quality gate — K distinct frames with changing values; raw-hit cap.
inline constexpr unsigned confirmFramesK = 3;
inline constexpr size_t maxRawHits = 256;
inline constexpr float confirmMinDelta = 1e-4f;
inline constexpr float strongScore = 3.f;
inline constexpr float trivialEps = 1e-6f;
inline constexpr float denormalFloor = 1e-38f; // below normal float range

struct Mat4 {
    float m[16]{};
};

struct Camera {
    Mat4 pose{}, view{}; // view undoes pose when the pose is orthonormal
    float jitterX{}, jitterY{};
    float nearZ{}, farZ{};
    float left{}, right{}, top{}, bottom{}; // lens tangents (+400..+40c)
    float pos[3]{};
    bool valid{};
};

enum class MatchKind : uint8_t {
    pose,
    view,
    poseT,
    viewT,
    position,
    jitter,
    projDiag,
    rotation3x3,
    // Update 28: forms the 27.4 run could not see.
    viewRotation, // camera-relative view: R^T (or column-major pose) with translation dropped
    viewProj,     // combined view*projection (rows or columns), camera-relative or not
    invViewProj,  // its inverse (clip to world)
    jitterScaled, // jitter x2 (NDC) or x0.5, and/or y flipped
};

struct Match {
    MatchKind kind{};
    uint32_t offset{}; // bytes into the scanned buffer
    float error{};     // max abs element error (or |pos| error); relative for viewProj/jitter
    bool nearMiss{};   // Update 28: within nearMissFactor x tolerance, not a match ("almost")
    uint8_t form{};    // viewProj: 0 rows 1 columns; jitterScaled: see jitterFormName
    uint8_t lag{};     // which camera (0 newest stock/head; 1..cameraLags older stock)
};

struct Signature {
    uint8_t bytes[signatureBytes]{};
    uint8_t mask[signatureBytes]{}; // 1 keep, 0 wildcard
};

struct LeaXref {
    uint32_t at{};     // RVA of the LEA opcode
    uint32_t target{}; // RVA the disp32 points at
    int32_t disp{};
};

struct RuntimeFunction {
    uint32_t beginAddress{};
    uint32_t endAddress{};
    uint32_t unwindInfo{};
};

inline Mat4 transpose(const Mat4& a) {
    Mat4 o{};
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c)
            o.m[c * 4 + r] = a.m[r * 4 + c];
    return o;
}

// Inverse of a rigid pose (orthonormal 3x3, translation in row 3): R^T and -R^T t.
inline Mat4 inversePose(const Mat4& pose) {
    Mat4 o{};
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 3; ++c)
            o.m[r * 4 + c] = pose.m[c * 4 + r];
    const float tx = pose.m[12], ty = pose.m[13], tz = pose.m[14];
    o.m[12] = -(o.m[0] * tx + o.m[4] * ty + o.m[8] * tz);
    o.m[13] = -(o.m[1] * tx + o.m[5] * ty + o.m[9] * tz);
    o.m[14] = -(o.m[2] * tx + o.m[6] * ty + o.m[10] * tz);
    o.m[15] = 1;
    return o;
}

inline float maxAbsDiff(const Mat4& a, const Mat4& b) {
    float m = 0;
    for (int i = 0; i < 16; ++i)
        m = std::max(m, std::abs(a.m[i] - b.m[i]));
    return m;
}

// 3x3 rotation only; each row may flip sign (some engines store -forward).
inline float rotation3Error(const Mat4& a, const Mat4& b) {
    float worst = 0;
    for (int r = 0; r < 3; ++r) {
        float same = 0, flipped = 0;
        for (int c = 0; c < 3; ++c) {
            same = std::max(same, std::abs(a.m[r * 4 + c] - b.m[r * 4 + c]));
            flipped = std::max(flipped, std::abs(a.m[r * 4 + c] + b.m[r * 4 + c]));
        }
        worst = std::max(worst, std::min(same, flipped));
    }
    return worst;
}

inline bool finiteMat(const Mat4& a) {
    for (float v : a.m)
        if (!std::isfinite(v))
            return false;
    return true;
}

// Update 27.3: reject zeros, identity, ±1, NaN/Inf, denormals — they match everything.
inline bool isDenormal(float v) {
    return v != 0.f && std::isfinite(v) && std::abs(v) < denormalFloor;
}

inline bool isTrivialScalar(float v) {
    if (!std::isfinite(v) || isDenormal(v))
        return true;
    return std::abs(v) <= trivialEps || std::abs(v - 1.f) <= trivialEps || std::abs(v + 1.f) <= trivialEps;
}

inline bool isZeroMat(const Mat4& a) {
    for (float v : a.m)
        if (std::abs(v) > trivialEps)
            return false;
    return true;
}

inline bool isIdentityMat(const Mat4& a) {
    for (int i = 0; i < 16; ++i) {
        const float want = (i % 5 == 0) ? 1.f : 0.f;
        if (std::abs(a.m[i] - want) > 1e-4f)
            return false;
    }
    return true;
}

inline bool isTrivialMat(const Mat4& a) {
    return !finiteMat(a) || isZeroMat(a) || isIdentityMat(a);
}

enum class HitCategory : uint8_t { lighting, taa, lod, other };

inline HitCategory categoryForMatch(MatchKind k) {
    switch (k) {
    case MatchKind::pose:
    case MatchKind::view:
    case MatchKind::poseT:
    case MatchKind::viewT:
    case MatchKind::position:
    case MatchKind::projDiag:
    case MatchKind::rotation3x3:
    case MatchKind::viewRotation:
    case MatchKind::viewProj:
    case MatchKind::invViewProj:
        return HitCategory::lighting;
    case MatchKind::jitter:
    case MatchKind::jitterScaled:
        return HitCategory::taa;
    }
    return HitCategory::other;
}

inline HitCategory categoryForKindName(const char* kind) {
    if (!kind || !kind[0])
        return HitCategory::other;
    if (std::strcmp(kind, "cb_match") == 0 || std::strcmp(kind, "cb_near") == 0 || std::strcmp(kind, "root_cbv") == 0 ||
        std::strcmp(kind, "sl_matrices") == 0)
        return HitCategory::lighting;
    if (std::strcmp(kind, "taa_jitter") == 0 || std::strcmp(kind, "sl_constants") == 0 ||
        std::strcmp(kind, "xess_exec") == 0 || std::strcmp(kind, "mvec_target") == 0 ||
        std::strcmp(kind, "history_color") == 0 || std::strcmp(kind, "export") == 0)
        return HitCategory::taa;
    if (std::strcmp(kind, "lod_scale") == 0)
        return HitCategory::lod;
    return HitCategory::other;
}

// Multi-frame confirmation: same site seen on K distinct frames with changing samples.
struct ConfirmState {
    uint32_t distinctFrames{};
    uint64_t lastFrame{UINT64_MAX};
    float last[4]{};
    bool hasLast{};
};

// Returns true once distinctFrames reaches need (default confirmFramesK).
inline bool noteConfirmSample(ConfirmState& s, uint64_t frame, const float sample[4], unsigned need = confirmFramesK,
                              float minDelta = confirmMinDelta) {
    if (!sample)
        return s.distinctFrames >= need;
    if (s.lastFrame == frame)
        return s.distinctFrames >= need;
    bool changed = !s.hasLast;
    if (s.hasLast) {
        float d = 0;
        for (int i = 0; i < 4; ++i)
            d = std::max(d, std::abs(sample[i] - s.last[i]));
        changed = d >= minDelta;
    }
    if (!changed && s.distinctFrames > 0)
        return s.distinctFrames >= need;
    s.lastFrame = frame;
    for (int i = 0; i < 4; ++i)
        s.last[i] = sample[i];
    s.hasLast = true;
    if (s.distinctFrames < 1000000u)
        ++s.distinctFrames;
    return s.distinctFrames >= need;
}

inline float scoreConfirmed(uint32_t distinctFrames, uint32_t rawHits, float error, HitCategory cat) {
    float score = static_cast<float>(distinctFrames) * 2.f + std::min(static_cast<float>(rawHits), 16.f) * .25f;
    score += std::max(0.f, 1.f - error * 100.f);
    if (cat == HitCategory::lighting || cat == HitCategory::taa || cat == HitCategory::lod)
        score += 1.f;
    return score;
}

inline bool rawHitsAtCap(size_t stored, size_t cap = maxRawHits) { return stored >= cap; }

inline Camera cameraFromDescriptor(const native_view::Descriptor& d) {
    Camera c{};
    if (!native_view::valid(d))
        return c;
    std::memcpy(c.pose.m, d.values, 64);
    c.view = inversePose(c.pose);
    c.jitterX = d.values[0x428 / 4];
    c.jitterY = d.values[0x42c / 4];
    c.nearZ = d.nearZ();
    c.farZ = d.farZ();
    c.left = d.values[0x400 / 4];
    c.right = d.values[0x404 / 4];
    c.top = d.values[0x408 / 4];
    c.bottom = d.values[0x40c / 4];
    c.pos[0] = d.values[12];
    c.pos[1] = d.values[13];
    c.pos[2] = d.values[14];
    c.valid = finiteMat(c.pose) && finiteMat(c.view);
    return c;
}

inline float projSx(const Camera& c) {
    return 2.f / (c.right - c.left);
}
inline float projSy(const Camera& c) {
    return 2.f / (c.bottom - c.top); // top < bottom in engine tangents
}

inline const char* matchKindName(MatchKind k) {
    switch (k) {
    case MatchKind::pose:
        return "pose";
    case MatchKind::view:
        return "view";
    case MatchKind::poseT:
        return "pose_T";
    case MatchKind::viewT:
        return "view_T";
    case MatchKind::position:
        return "position";
    case MatchKind::jitter:
        return "jitter";
    case MatchKind::projDiag:
        return "proj_diag";
    case MatchKind::rotation3x3:
        return "rotation_3x3";
    case MatchKind::viewRotation:
        return "view_rotation";
    case MatchKind::viewProj:
        return "view_proj";
    case MatchKind::invViewProj:
        return "inv_view_proj";
    case MatchKind::jitterScaled:
        return "jitter_scaled";
    }
    return "unknown";
}

// Update 28: jitterScaled forms (scale, y sign).
struct JitterForm {
    float scale, ySign;
    const char* name;
};
inline constexpr JitterForm jitterForms[] = {
    {1.f, 1.f, "as_is"}, {2.f, 1.f, "x2"},         {.5f, 1.f, "x0.5"},
    {1.f, -1.f, "flip_y"}, {2.f, -1.f, "x2_flip_y"}, {.5f, -1.f, "x0.5_flip_y"},
};
inline const char* jitterFormName(uint8_t form) {
    return form < std::size(jitterForms) ? jitterForms[form].name : "?";
}

// Relative error of v against want, with a tiny absolute floor.
inline float relativeError(float v, float want, float floor = jitterAbsFloor) {
    return std::abs(v - want) / std::max(std::abs(want), floor);
}

// Update 28: combined view*projection test. R rows r0 r1 r2 (pose rows, world).
// Row-vector V*P has columns sx*r0+ox*r2, sy*r1+oy*r2; column-vector P*V has
// those as rows; the inverse has rows r0/sx, r1/sy. Returns the worst relative
// error of: v0 has no r1 part, |v0.r0| = s0, v1 has no r0 part, |v1.r1| = s1,
// and (offsetCap) the r2 part stays below offsetCap*s. Translation is ignored, so
// camera-relative and absolute storage both match. 1e9 when not finite.
inline float scaledBasisError(const float v0[3], const float v1[3], const Mat4& pose, float s0, float s1,
                              float offsetCap) {
    const auto dot = [&](const float* v, int r) {
        return v[0] * pose.m[r * 4] + v[1] * pose.m[r * 4 + 1] + v[2] * pose.m[r * 4 + 2];
    };
    const float a0 = dot(v0, 0), b0 = dot(v0, 1), c0 = dot(v0, 2);
    const float a1 = dot(v1, 1), b1 = dot(v1, 0), c1 = dot(v1, 2);
    if (!std::isfinite(a0 + b0 + c0 + a1 + b1 + c1) || !(s0 > 0) || !(s1 > 0))
        return 1e9f;
    float e = std::abs(std::abs(a0) - s0) / s0;
    e = std::max(e, std::abs(std::abs(a1) - s1) / s1);
    e = std::max(e, std::abs(b0) / s0);
    e = std::max(e, std::abs(b1) / s1);
    e = std::max(e, std::max(0.f, std::abs(c0) / s0 - offsetCap));
    e = std::max(e, std::max(0.f, std::abs(c1) / s1 - offsetCap));
    return e;
}

// Update 28: raw-hit store admission. The 27.4 run filled all 256 slots by
// mid-run with one-frame noise and duplicates, then dropped later real hits
// (its jitter_track offset never got a candidate). Non-value hits stop at 3/4
// of the cap so value-bearing hits always have room; at the cap a value hit may
// evict the weakest unconfirmed one-hit entry.
struct RawHitSummary {
    bool confirmed{}, valueBearing{};
    uint32_t hits{}, distinctFrames{};
};
inline bool rawHitAdmit(size_t stored, bool valueBearing, size_t cap = maxRawHits) {
    if (stored < cap * 3 / 4)
        return true;
    return valueBearing && stored < cap;
}
// Index to evict for a new value-bearing hit, or -1: prefer non-value one-hit
// entries, then value entries seen on only one frame. Never a confirmed one.
inline long evictIndex(const RawHitSummary* rows, size_t n) {
    long best = -1;
    int bestRank = 99;
    for (size_t i = 0; i < n; ++i) {
        const auto& r = rows[i];
        if (r.confirmed || r.hits > 1)
            continue;
        const int rank = !r.valueBearing ? 0 : (r.distinctFrames <= 1 ? 1 : 99);
        if (rank < bestRank) {
            bestRank = rank;
            best = static_cast<long>(i);
        }
    }
    return best;
}

// Update 28: one site for dedupe. Live adds carry no function RVA; the worker
// fills it later. 27.4 keyed on functionRva once known, so later live hits of
// the same call made a new row (nine rows for history 0x2cc66fe).
inline bool sameSite(uint32_t storedFunction, uint32_t storedModule, uint32_t function, uint32_t module) {
    if (storedModule == module && (module || (!function && !storedFunction)))
        return true; // same call (or both site-less, e.g. lod_scale / taa_jitter)
    return function && storedFunction == function;
}

// Update 28: a value that cannot change (lod distance scale) confirms by being
// found again at the same offset on K separate scans (different frames).
struct StableState {
    uint32_t offset = UINT32_MAX;
    uint32_t scans{};
    uint64_t lastFrame = UINT64_MAX;
};
inline bool noteStableSample(StableState& s, uint64_t frame, uint32_t offset, unsigned need = confirmFramesK) {
    if (frame == s.lastFrame)
        return s.scans >= need;
    if (offset != s.offset) {
        s.offset = offset;
        s.scans = 0;
    }
    s.lastFrame = frame;
    ++s.scans;
    return s.scans >= need;
}

// Update 28: "almost" — unconfirmed but worth a look: value-bearing and seen on
// 2+ frames, or a near miss (within nearMissFactor x tolerance).
inline bool isAlmost(bool confirmed, bool valueBearing, uint32_t distinctFrames, bool nearMiss) {
    if (confirmed)
        return false;
    return nearMiss || (valueBearing && distinctFrames >= 2);
}

// Scan `floats` floats (any alignment of interest: call with float* into the
// region). Offsets are in bytes from `data`. Caps hits so a huge ring cannot
// flood the report. Update 28: camera-relative view, combined view*projection
// (and inverse), scaled / flipped jitter with a relative tolerance, and near
// misses (Match::nearMiss, only when `nearMisses`) for the report's "almost" list.
inline std::vector<Match> matchBuffer(const float* data, size_t floats, const Camera& cam,
                                      size_t maxHits = 64, bool nearMisses = false) {
    std::vector<Match> out;
    if (!data || !cam.valid || floats < 16 || isTrivialMat(cam.pose) || isTrivialMat(cam.view))
        return out;
    const Mat4 poseT = transpose(cam.pose), viewT = transpose(cam.view);
    const float sx = projSx(cam), sy = projSy(cam);
    const bool projOk = std::isfinite(sx) && std::isfinite(sy) && std::abs(sx) > .1f && std::abs(sy) > .1f &&
                        std::abs(sx) < 50.f && std::abs(sy) < 50.f;
    const float asx = std::abs(sx), asy = std::abs(sy);
    const bool havePos = std::abs(cam.pos[0]) + std::abs(cam.pos[1]) + std::abs(cam.pos[2]) > 1.f &&
                         !isTrivialScalar(cam.pos[0]) && !isTrivialScalar(cam.pos[1]) &&
                         !isTrivialScalar(cam.pos[2]);
    const float nearScale = nearMisses ? nearMissFactor : 1.f;
    size_t nearCount = 0;
    const auto push = [&](MatchKind kind, uint32_t offset, float err, float tol, uint8_t form = 0) {
        if (out.size() >= maxHits || !(err <= tol * nearScale))
            return;
        const bool miss = err > tol;
        if (miss && nearCount >= maxHits / 2)
            return; // nearScale misses never crowd out real matches
        nearCount += miss;
        out.push_back({kind, offset, err, miss, form, 0});
    };
    const float r00 = std::abs(cam.pose.m[0]);
    for (size_t i = 0; i + 16 <= floats && out.size() < maxHits; ++i) {
        const float d0 = data[i];
        // Cheap prefilters: every rigid form has +-R00 first; view*proj rows have
        // a first-row length nearScale sx (or 1/sx for the inverse).
        const bool rigid = std::abs(std::abs(d0) - r00) <= matrixTol * nearScale;
        bool vp = false;
        if (projOk) {
            const float n0 = std::sqrt(d0 * d0 + data[i + 1] * data[i + 1] + data[i + 2] * data[i + 2]);
            const float c0 = std::sqrt(d0 * d0 + data[i + 4] * data[i + 4] + data[i + 8] * data[i + 8]);
            const auto band = [&](float n, float s) { return n >= s * .9f && n <= s * 1.7f; };
            vp = band(n0, asx) || band(c0, asx) || band(n0, 1.f / asx) || band(c0, 1.f / asx);
        }
        if (!rigid && !vp)
            continue;
        Mat4 at{};
        std::memcpy(at.m, data + i, 64);
        if (isTrivialMat(at))
            continue;
        const uint32_t off = static_cast<uint32_t>(i * 4);
        if (rigid) {
            push(MatchKind::pose, off, maxAbsDiff(at, cam.pose), matrixTol);
            push(MatchKind::view, off, maxAbsDiff(at, cam.view), matrixTol);
            push(MatchKind::poseT, off, maxAbsDiff(at, poseT), matrixTol);
            push(MatchKind::viewT, off, maxAbsDiff(at, viewT), matrixTol);
            const float rp = rotation3Error(at, cam.pose), rv = rotation3Error(at, cam.view);
            push(MatchKind::rotation3x3, off, rp, matrixTol);
            if (rv < rp) // R^T: a camera-relative view (translation removed) or column-major pose
                push(MatchKind::viewRotation, off, rv, matrixTol);
        }
        if (vp) {
            for (uint8_t form = 0; form < 2; ++form) {
                const Mat4& m = form ? transpose(at) : at;
                const float v0[3] = {m.m[0], m.m[1], m.m[2]}, v1[3] = {m.m[4], m.m[5], m.m[6]};
                push(MatchKind::viewProj, off, scaledBasisError(v0, v1, cam.pose, asx, asy, .6f), viewProjTol,
                     form);
                push(MatchKind::invViewProj, off, scaledBasisError(v0, v1, cam.pose, 1.f / asx, 1.f / asy, 0.f),
                     viewProjTol, form);
            }
        }
    }
    if (havePos) {
        for (size_t i = 0; i + 3 <= floats && out.size() < maxHits; ++i) {
            if (isTrivialScalar(data[i]) && isTrivialScalar(data[i + 1]) && isTrivialScalar(data[i + 2]))
                continue;
            const float dx = data[i] - cam.pos[0], dy = data[i + 1] - cam.pos[1],
                        dz = data[i + 2] - cam.pos[2];
            if (!std::isfinite(dx) || !std::isfinite(dy) || !std::isfinite(dz))
                continue;
            const float e = std::max({std::abs(dx), std::abs(dy), std::abs(dz)});
            push(MatchKind::position, static_cast<uint32_t>(i * 4), e, positionTol);
        }
    }
    // Jitter must be non-trivial on the camera and in the buffer (rejects nearScale-zero noise).
    // Update 28: relative tolerance, and x2 / x0.5 / flipped-y forms.
    if (std::isfinite(cam.jitterX) && std::isfinite(cam.jitterY) && !isTrivialScalar(cam.jitterX) &&
        !isTrivialScalar(cam.jitterY) &&
        (std::abs(cam.jitterX) > 1e-4f || std::abs(cam.jitterY) > 1e-4f)) {
        for (size_t i = 0; i + 2 <= floats && out.size() < maxHits; ++i) {
            if (isTrivialScalar(data[i]) || isTrivialScalar(data[i + 1]))
                continue;
            for (uint8_t f = 0; f < std::size(jitterForms); ++f) {
                const auto& jf = jitterForms[f];
                const float e = std::max(relativeError(data[i], cam.jitterX * jf.scale),
                                         relativeError(data[i + 1], cam.jitterY * jf.scale * jf.ySign));
                if (!std::isfinite(e) || e > jitterRelTol * nearScale)
                    continue;
                push(f ? MatchKind::jitterScaled : MatchKind::jitter, static_cast<uint32_t>(i * 4), e,
                     jitterRelTol, f);
                break;
            }
        }
    }
    if (projOk && !isTrivialScalar(sx) && !isTrivialScalar(sy)) {
        for (size_t i = 0; i + 1 <= floats && out.size() < maxHits; ++i) {
            if (isTrivialScalar(data[i]))
                continue;
            const float ex = std::abs(data[i] - sx);
            if (!(ex <= projTol * nearScale))
                continue;
            for (size_t j : {i + 1, i + 5}) {
                if (j >= floats)
                    continue;
                if (isTrivialScalar(data[j]))
                    continue;
                const float ey = std::abs(data[j] - sy);
                if (std::isfinite(ey) && ey <= projTol * nearScale) {
                    push(MatchKind::projDiag, static_cast<uint32_t>(i * 4), std::max(ex, ey), projTol);
                    break;
                }
            }
        }
    }
    return out;
}

// Wildcard RIP-relative and disp32 slots in a 32-byte window (E8/E9 rel32,
// 0F 80..8F jcc rel32, and ModRM mod=00 rm=101 disp32 / RIP-relative).
inline Signature signatureAt(const uint8_t* code, size_t available) {
    Signature s{};
    const size_t n = std::min<size_t>(signatureBytes, available);
    for (size_t i = 0; i < n; ++i) {
        s.bytes[i] = code[i];
        s.mask[i] = 1;
    }
    auto wild4 = [&](size_t at) {
        for (size_t k = 0; k < 4 && at + k < n; ++k)
            s.mask[at + k] = 0;
    };
    for (size_t i = 0; i + 5 <= n; ++i) {
        const uint8_t b = code[i];
        if (b == 0xe8 || b == 0xe9)
            wild4(i + 1);
        if (b == 0x0f && i + 6 <= n && code[i + 1] >= 0x80 && code[i + 1] <= 0x8f)
            wild4(i + 2);
    }
    for (size_t i = 0; i + 7 <= n; ++i) {
        // REX.W (48/4C) 8D ModRM(mod=00,rm=101) disp32 — LEA r64, [rip+disp32]
        if ((code[i] == 0x48 || code[i] == 0x4c) && code[i + 1] == 0x8d && (code[i + 2] & 0xc7) == 0x05)
            wild4(i + 3);
        // 8B / 89 / 8D without REX but with mod=00 rm=101 (disp32 absolute in 64-bit = RIP-rel)
        if ((code[i] == 0x8b || code[i] == 0x89 || code[i] == 0x8d) && (code[i + 1] & 0xc7) == 0x05 &&
            i + 6 <= n)
            wild4(i + 2);
    }
    return s;
}

inline std::string signatureText(const Signature& s) {
    std::string out;
    out.reserve(signatureBytes * 3);
    for (unsigned i = 0; i < signatureBytes; ++i) {
        if (i)
            out.push_back(' ');
        if (!s.mask[i])
            out += "??";
        else {
            static constexpr char hex[] = "0123456789ABCDEF";
            out.push_back(hex[s.bytes[i] >> 4]);
            out.push_back(hex[s.bytes[i] & 15]);
        }
    }
    return out;
}

// Find RIP-relative LEA (REX 48/4C, 8D, ModRM mod=00 rm=101) whose target is
// `targetRva`. `image` is the mapped PE (or any bytes); `rvaBase` is the RVA of
// image[0] (0 when `image` starts at the module base).
inline std::vector<LeaXref> findLeaXrefs(const uint8_t* image, size_t size, uint32_t targetRva,
                                         uint32_t rvaBase = 0, size_t maxHits = 64) {
    std::vector<LeaXref> out;
    if (!image || size < 7)
        return out;
    for (size_t i = 0; i + 7 <= size && out.size() < maxHits; ++i) {
        if ((image[i] != 0x48 && image[i] != 0x4c) || image[i + 1] != 0x8d || (image[i + 2] & 0xc7) != 0x05)
            continue;
        int32_t disp{};
        std::memcpy(&disp, image + i + 3, 4);
        const uint32_t at = rvaBase + static_cast<uint32_t>(i);
        const uint32_t target = at + 7 + static_cast<uint32_t>(disp);
        if (target == targetRva)
            out.push_back({at, target, disp});
    }
    return out;
}

// Binary search .pdata for the RUNTIME_FUNCTION covering `rva` (begin inclusive,
// end exclusive). Returns beginAddress, or 0 when none.
inline uint32_t functionBegin(const RuntimeFunction* pdata, size_t count, uint32_t rva) {
    if (!pdata || !count)
        return 0;
    size_t lo = 0, hi = count;
    while (lo < hi) {
        const size_t mid = lo + (hi - lo) / 2;
        if (rva < pdata[mid].beginAddress)
            hi = mid;
        else if (rva >= pdata[mid].endAddress)
            lo = mid + 1;
        else
            return pdata[mid].beginAddress;
    }
    return 0;
}

// ASCII needle in `image`; returns RVAs (rvaBase + offset). Caps hits.
inline std::vector<uint32_t> findAscii(const uint8_t* image, size_t size, std::string_view needle,
                                       uint32_t rvaBase = 0, size_t maxHits = 32) {
    std::vector<uint32_t> out;
    if (!image || needle.empty() || size < needle.size())
        return out;
    for (size_t i = 0; i + needle.size() <= size && out.size() < maxHits; ++i) {
        if (std::memcmp(image + i, needle.data(), needle.size()) == 0)
            out.push_back(rvaBase + static_cast<uint32_t>(i));
    }
    return out;
}

inline std::string jsonEscape(std::string_view s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (unsigned char c : s) {
        switch (c) {
        case '"':
            out += "\\\"";
            break;
        case '\\':
            out += "\\\\";
            break;
        case '\b':
            out += "\\b";
            break;
        case '\f':
            out += "\\f";
            break;
        case '\n':
            out += "\\n";
            break;
        case '\r':
            out += "\\r";
            break;
        case '\t':
            out += "\\t";
            break;
        default:
            if (c < 0x20) {
                char buf[8];
                std::snprintf(buf, sizeof buf, "\\u%04x", c);
                out += buf;
            } else {
                out.push_back(static_cast<char>(c));
            }
        }
    }
    return out;
}

// Track a jitter pair across frames: the same buffer offset whose float2
// follows the camera's +428/+42c while the pose moves is a strong TAA hint.
struct JitterTrack {
    uint32_t offset = UINT32_MAX;
    uint32_t hits{};
    float lastX{}, lastY{};
    uint32_t changes{}; // Update 28: hits whose jitter differed from the previous hit
};

inline void noteJitterHit(JitterTrack& t, uint32_t offset, float x, float y) {
    if (t.offset == UINT32_MAX || t.offset == offset) {
        if (t.hits && (std::abs(x - t.lastX) > jitterAbsFloor || std::abs(y - t.lastY) > jitterAbsFloor))
            ++t.changes;
        t.offset = offset;
        ++t.hits;
        t.lastX = x;
        t.lastY = y;
    } else if (t.hits < 2) {
        t = {offset, 1, x, y, 0};
    }
}

// Update 28: a jitter offset is confirmed only when it followed a jitter that
// changed (27.4's cb+0x2e00 saw the same frozen value 4 times: "almost").
inline bool jitterTrackConfirmed(const JitterTrack& t) {
    return t.hits >= confirmFramesK && t.changes >= confirmFramesK - 1;
}


// Update 27.2: per-frame live-sampling budget (render thread). Pure so tests can
// simulate overruns without D3D12. 0.5 ms and a small op cap; over budget → backoff.
inline constexpr uint64_t frameBudgetNanoseconds = 500000; // 0.5 ms
inline constexpr uint32_t frameBudgetMaxOps = 8;

inline bool frameBudgetAllows(uint32_t opsDone, uint32_t maxOps, uint64_t elapsedNs, uint64_t budgetNs) {
    return opsDone < maxOps && elapsedNs < budgetNs;
}

// After a frame that overran, skip this many later frames of live sampling.
inline uint32_t frameBudgetNextSkip(uint32_t previousSkip, bool overBudget) {
    if (overBudget)
        return previousSkip == 0 ? 2u : (previousSkip < 32u ? previousSkip * 2u : 32u);
    return previousSkip > 0 ? previousSkip - 1u : 0u;
}

// Update 29: which camera a constant buffer was built from. The 28.1 run
// matched only the flat (stock) camera and its history; a still camera made
// every lag match, so each hit said "lag=3" (the last one tried). Now the head
// view and both eyes are tried too, identical cameras are tried once, and a
// hit lists every camera it matched ("cams=flat+flat-1"), lag being the newest.
inline constexpr unsigned cameraSlots = 1 + cameraLags + 3; // flat, flat-1..-3, head, eye0, eye1
inline constexpr unsigned headSlot = 1 + cameraLags;
inline constexpr unsigned eyeSlot0 = headSlot + 1;
inline constexpr uint32_t flatSlotsMask = (1u << (1 + cameraLags)) - 1u;
inline constexpr float flatHeadApartM = .25f; // below this, flat and head cannot be told apart

inline const char* cameraSlotName(unsigned k) {
    static const char* const names[] = {"flat", "flat-1", "flat-2", "flat-3", "head", "eye0", "eye1"};
    static_assert(sizeof(names) / sizeof(names[0]) == cameraSlots, "one name per camera slot");
    return k < cameraSlots ? names[k] : "?";
}

inline bool sameCamera(const Camera& a, const Camera& b) {
    return a.valid && b.valid && std::memcmp(a.pose.m, b.pose.m, sizeof a.pose.m) == 0 &&
           a.jitterX == b.jitterX && a.jitterY == b.jitterY && a.left == b.left && a.right == b.right &&
           a.top == b.top && a.bottom == b.bottom;
}

struct CameraHit {
    Match match{};    // the best (smallest error) match; lag = newest flat slot matched, else 0
    uint32_t mask{};  // bit k: matched cameraSlotName(k)
    unsigned best{};  // slot whose camera gave `match`
};

inline std::vector<CameraHit> matchCameras(const float* data, size_t floats, const Camera (&cams)[cameraSlots],
                                           size_t maxHits = 64, bool nearMisses = false) {
    std::vector<CameraHit> out;
    unsigned sameAs[cameraSlots];
    for (unsigned k = 0; k < cameraSlots; ++k) {
        sameAs[k] = k;
        for (unsigned j = 0; j < k; ++j)
            if (sameAs[j] == j && sameCamera(cams[k], cams[j])) {
                sameAs[k] = j;
                break;
            }
    }
    for (unsigned k = 0; k < cameraSlots; ++k) {
        if (!cams[k].valid || sameAs[k] != k)
            continue;
        uint32_t bits = 0;
        for (unsigned j = k; j < cameraSlots; ++j)
            if (sameAs[j] == k)
                bits |= 1u << j;
        for (const auto& h : matchBuffer(data, floats, cams[k], maxHits, nearMisses)) {
            CameraHit* at = nullptr;
            for (auto& o : out)
                if (o.match.offset == h.offset && o.match.kind == h.kind && o.match.form == h.form &&
                    o.match.nearMiss == h.nearMiss) {
                    at = &o;
                    break;
                }
            if (!at) {
                if (out.size() >= maxHits)
                    continue;
                out.push_back({h, 0u, k});
                at = &out.back();
            } else if (h.error < at->match.error) {
                at->match = h;
                at->best = k;
            }
            at->mask |= bits;
        }
    }
    for (auto& o : out) {
        o.match.lag = 0;
        for (unsigned k = 0; k <= cameraLags; ++k)
            if (o.mask & (1u << k)) {
                o.match.lag = static_cast<uint8_t>(k);
                break;
            }
    }
    return out;
}

// "flat+flat-1+head" (empty mask: "none").
inline std::string cameraMaskText(uint32_t mask) {
    std::string s;
    for (unsigned k = 0; k < cameraSlots; ++k)
        if (mask & (1u << k)) {
            if (!s.empty())
                s += '+';
            s += cameraSlotName(k);
        }
    return s.empty() ? std::string("none") : s;
}

// FLAT-ONLY: built from the game's own camera, not where the eyes are (only
// said when the two were at least flatHeadApartM apart). HEAD-ONLY / EYE-ONLY
// likewise; "" when mixed or when flat and head could not be told apart.
inline const char* cameraTag(uint32_t mask, float flatHeadGapM) {
    const uint32_t flat = mask & flatSlotsMask, head = mask & (1u << headSlot),
                   eyes = mask & (3u << eyeSlot0);
    if (!mask)
        return "";
    if (flat && !head && !eyes)
        return flatHeadGapM >= flatHeadApartM ? "FLAT-ONLY" : "";
    if (head && !flat && !eyes)
        return flatHeadGapM >= flatHeadApartM ? "HEAD-ONLY" : "";
    if (eyes && !flat && !head)
        return "EYE-ONLY";
    return "";
}

// Update 30: BIND TRACE. Where camera data lands on the GPU (a copy_buffer hit at
// dst VA + cb offset, plus 8 page seeds from the Update 29 run) is watched; a later
// root CBV whose 64 KiB window covers a watched value is a bind of that camera's
// data. Each bind row keeps the call site, root slot, the value's offset in the
// bound buffer and the pass it fed: depth-only (no colour target, a depth target:
// likely a shadow map), colour, compute (a lighting / shadow resolve may run here)
// or unknown. Bind rows have their own store (bindRowsCap) so value hits in the
// 256 raw-hit store can never crowd them out. Candidates only.
// Update 31: 64 -> 256 (the 30.1 run filled 64 with one frame's ring of records).
inline constexpr size_t maxWatched = 256;
inline constexpr size_t bindRowsCap = 96;
inline constexpr uint64_t cbvWindowBytes = 65536; // D3D12 CBV max size
// Update 30.1: "frames" are stock view updates, ~6 per game frame in the 29 runs (637 in 2 s),
// so 8 could expire a value before the same frame's pass bound it. 48 is ~7 game frames.
inline constexpr uint64_t watchExpiryFrames = 48; // ring buffers reuse a VA for other data
inline constexpr uint64_t seedPageBytes = 65536;

struct Watched {
    uint64_t va{};         // matched value address (or seed page base)
    uint32_t camMask{};    // cameras it matched (cameraSlots bits); 0 for a seed
    const char* tag = "";  // FLAT-ONLY / HEAD-ONLY / EYE-ONLY / ""
    MatchKind kind{};
    bool seed{};           // Update 29 page seed: whole 64 KiB page, camera unknown this session
    uint64_t lastFrame{};
};

// Insert or refresh. When full, replace the oldest non-seed entry.
inline size_t noteWatched(Watched* w, size_t& n, size_t cap, uint64_t va, uint32_t mask, const char* tag,
                          MatchKind kind, uint64_t frame) {
    for (size_t i = 0; i < n; ++i)
        if (!w[i].seed && w[i].va == va) {
            w[i].camMask = mask;
            w[i].tag = tag ? tag : "";
            w[i].kind = kind;
            w[i].lastFrame = frame;
            return i;
        }
    size_t at = n;
    if (n >= cap) {
        at = cap;
        for (size_t i = 0; i < n; ++i)
            if (!w[i].seed && (at == cap || w[i].lastFrame < w[at].lastFrame))
                at = i;
        if (at == cap)
            return cap;
    } else {
        ++n;
    }
    w[at] = Watched{va, mask, tag ? tag : "", kind, false, frame};
    return at;
}

// The watched value a bind at bindVa covers (smallest offset first), or -1. Live
// entries older than watchExpiryFrames are skipped; seeds never expire. Live
// entries win over seeds.
inline long findWatched(const Watched* w, size_t n, uint64_t bindVa, uint64_t frame) {
    long best = -1, seedBest = -1;
    uint64_t bestOff = ~0ull, seedOff = ~0ull;
    if (!bindVa)
        return -1;
    for (size_t i = 0; i < n; ++i) {
        const auto& e = w[i];
        if (e.seed) {
            if (bindVa >= e.va && bindVa < e.va + seedPageBytes && bindVa - e.va < seedOff) {
                seedOff = bindVa - e.va;
                seedBest = static_cast<long>(i);
            }
            continue;
        }
        if (frame > e.lastFrame + watchExpiryFrames)
            continue;
        if (e.va >= bindVa && e.va - bindVa < cbvWindowBytes && e.va - bindVa < bestOff) {
            bestOff = e.va - bindVa;
            best = static_cast<long>(i);
        }
    }
    return best >= 0 ? best : seedBest;
}

enum class PassKind : uint8_t { unknown, depthOnly, color, compute };
inline const char* passKindName(PassKind k) {
    switch (k) {
    case PassKind::depthOnly:
        return "depth_only";
    case PassKind::color:
        return "color";
    case PassKind::compute:
        return "compute";
    default:
        return "unknown";
    }
}

// What the command list was drawing into when the bind happened (per thread).
struct PassTarget {
    const void* list{};     // command list the targets were set on
    uint32_t rtCount{};     // OMSetRenderTargets NumRenderTargetDescriptors
    bool hasDsv{};
    bool known{};           // OMSetRenderTargets seen on this list
    uint32_t vpW{}, vpH{};  // RSSetViewports[0]
    uint32_t depthFormat{}, depthW{}, depthH{}; // last texture moved to DEPTH_WRITE on this thread
};

inline PassKind passKindFor(const PassTarget& t, bool compute, const void* list) {
    if (compute)
        return PassKind::compute;
    if (!t.known || t.list != list)
        return PassKind::unknown;
    if (t.rtCount == 0 && t.hasDsv)
        return PassKind::depthOnly;
    return t.rtCount ? PassKind::color : PassKind::unknown;
}

struct BindRow {
    uint32_t rva{}, functionRva{};
    uint32_t slot{}, offset{};  // root slot; watched value's offset in the bound buffer
    uint8_t api{};              // 0 graphics CBV, 1 compute CBV, 2 graphics consts, 3 compute consts,
                                // 4 graphics table, 5 compute table (Update 31)
    uint32_t table{};           // Update 31: descriptor index in the bound table + 1 (0: not a table bind)
    PassKind pass{};
    MatchKind kind{};
    bool seed{};
    uint32_t camMask{};
    const char* tag = "";
    uint32_t rtCount{};
    bool hasDsv{};
    uint32_t vpW{}, vpH{}, depthFormat{}, depthW{}, depthH{};
    uint32_t hits{}, frames{};
    uint64_t lastFrame = ~0ull;
};

inline const char* bindApiName(uint8_t api) {
    static const char* names[] = {"graphics_cbv",    "compute_cbv",   "graphics_consts",
                                  "compute_consts",  "graphics_table", "compute_table"};
    return api < 6 ? names[api] : "?";
}

// A depth-only pass fed camera data from this session (not a seed): the shadow suspect.
inline bool shadowPassCandidate(const BindRow& r) {
    return r.pass == PassKind::depthOnly && !r.seed && r.camMask != 0;
}

inline bool sameBind(const BindRow& a, const BindRow& b) {
    return a.rva == b.rva && a.slot == b.slot && a.offset == b.offset && a.api == b.api && a.pass == b.pass &&
           a.table == b.table &&
           a.rtCount == b.rtCount && a.hasDsv == b.hasDsv && a.vpW == b.vpW && a.vpH == b.vpH && a.seed == b.seed;
}

// Store a bind (dedupe, count hits and distinct frames). At the cap a new shadow
// candidate replaces the weakest one-hit non-shadow row; anything else is dropped.
// Returns false when it was not stored.
inline bool noteBind(BindRow* rows, size_t& n, size_t cap, const BindRow& r, uint64_t frame) {
    for (size_t i = 0; i < n; ++i)
        if (sameBind(rows[i], r)) {
            auto& e = rows[i];
            ++e.hits;
            if (e.lastFrame != frame) {
                ++e.frames;
                e.lastFrame = frame;
            }
            e.camMask |= r.camMask;
            if (r.tag && r.tag[0] && !e.tag[0])
                e.tag = r.tag;
            if (r.depthFormat) {
                e.depthFormat = r.depthFormat;
                e.depthW = r.depthW;
                e.depthH = r.depthH;
            }
            return true;
        }
    size_t at = n;
    if (n >= cap) {
        if (!shadowPassCandidate(r))
            return false;
        at = cap;
        for (size_t i = 0; i < n; ++i)
            if (!shadowPassCandidate(rows[i]) && rows[i].hits <= 1) {
                at = i;
                break;
            }
        if (at == cap)
            return false;
    } else {
        ++n;
    }
    rows[at] = r;
    rows[at].hits = 1;
    rows[at].frames = 1;
    rows[at].lastFrame = frame;
    return true;
}

inline size_t shadowPassCount(const BindRow* rows, size_t n) {
    size_t k = 0;
    for (size_t i = 0; i < n; ++i)
        k += shadowPassCandidate(rows[i]) ? 1 : 0;
    return k;
}

// Update 31: DESCRIPTOR TABLES. SM2 may bind camera data through descriptor tables
// rather than root CBVs. The finder maps CBV / buffer-SRV descriptors (device
// CreateConstantBufferView / CreateShaderResourceView, and CopyDescriptorsSimple
// copies of them) whose range covers a watched page, then checks each table bind's
// first tableWindow descriptors in the shader-visible heap (SetDescriptorHeaps).
// Tables that start at heap index 0 most of the time mean a bindless heap (one
// table covers everything): then the table bind says nothing about the pass.
inline constexpr size_t descSlots = 4096; // power of two
inline constexpr size_t descProbe = 16;
inline constexpr uint64_t descTombstone = ~0ull;
inline constexpr uint32_t tableWindow = 64;

struct DescEntry {
    uint64_t cpu{}; // CPU descriptor handle (0 empty, descTombstone removed)
    uint64_t va{};  // buffer range the descriptor views
    uint32_t bytes{};
    uint64_t frame{};
};

inline size_t descHash(uint64_t cpu) {
    return static_cast<size_t>((cpu * 0x9E3779B97F4A7C15ull) >> 40) & (descSlots - 1);
}

inline long findDesc(const DescEntry* t, uint64_t cpu) {
    if (!cpu || cpu == descTombstone)
        return -1;
    const size_t h = descHash(cpu);
    for (size_t i = 0; i < descProbe; ++i) {
        const auto& e = t[(h + i) & (descSlots - 1)];
        if (e.cpu == cpu)
            return static_cast<long>((h + i) & (descSlots - 1));
        if (!e.cpu)
            return -1;
    }
    return -1;
}

// Insert or refresh; a full probe run replaces its oldest entry. Returns true when stored.
inline bool noteDesc(DescEntry* t, size_t& n, uint64_t cpu, uint64_t va, uint32_t bytes, uint64_t frame) {
    if (!cpu || cpu == descTombstone || !va || !bytes)
        return false;
    if (const long at = findDesc(t, cpu); at >= 0) {
        t[at] = DescEntry{cpu, va, bytes, frame};
        return true;
    }
    const size_t h = descHash(cpu);
    size_t pick = descSlots;
    for (size_t i = 0; i < descProbe; ++i) {
        const size_t k = (h + i) & (descSlots - 1);
        if (!t[k].cpu || t[k].cpu == descTombstone) {
            pick = k;
            break;
        }
        if (pick == descSlots || t[k].frame < t[pick].frame)
            pick = k;
    }
    if (!t[pick].cpu || t[pick].cpu == descTombstone)
        ++n;
    t[pick] = DescEntry{cpu, va, bytes, frame};
    return true;
}

// A descriptor rewritten with a range we do not watch: forget its old range.
inline bool forgetDesc(DescEntry* t, size_t& n, uint64_t cpu) {
    const long at = findDesc(t, cpu);
    if (at < 0)
        return false;
    t[at] = DescEntry{descTombstone, 0, 0, 0};
    if (n)
        --n;
    return true;
}

struct HeapInfo {
    uint64_t cpuStart{}, gpuStart{};
    uint32_t count{}, inc{};
};

// Heap index of a GPU descriptor handle, or -1 when it is outside the heap.
inline long tableIndex(const HeapInfo& h, uint64_t gpu) {
    if (!h.inc || !h.count || gpu < h.gpuStart)
        return -1;
    const uint64_t d = gpu - h.gpuStart;
    if (d % h.inc)
        return -1;
    const uint64_t i = d / h.inc;
    return i < h.count ? static_cast<long>(i) : -1;
}

inline uint64_t cpuAt(const HeapInfo& h, uint64_t index) { return h.cpuStart + index * h.inc; }

// The watched value inside [va, va + bytes) (live first, smallest offset), or -1.
inline long findWatchedIn(const Watched* w, size_t n, uint64_t va, uint64_t bytes, uint64_t frame) {
    long best = -1, seedBest = -1;
    uint64_t bestOff = ~0ull;
    if (!va || !bytes)
        return -1;
    for (size_t i = 0; i < n; ++i) {
        const auto& e = w[i];
        if (e.seed) {
            if (seedBest < 0 && va < e.va + seedPageBytes && e.va < va + bytes)
                seedBest = static_cast<long>(i);
            continue;
        }
        if (frame > e.lastFrame + watchExpiryFrames)
            continue;
        if (e.va >= va && e.va - va < bytes && e.va - va < bestOff) {
            bestOff = e.va - va;
            best = static_cast<long>(i);
        }
    }
    return best >= 0 ? best : seedBest;
}

// "likely": most table binds start at heap index 0 (a bindless heap); "partly": some do.
inline const char* bindlessVerdict(uint64_t heapStartBinds, uint64_t tableBinds) {
    if (tableBinds < 64)
        return "unknown";
    if (heapStartBinds * 2 >= tableBinds)
        return "likely";
    return heapStartBinds ? "partly" : "no";
}

// Update 31: depth targets seen this session (shadow maps are usually large and square,
// or an atlas), with how many depth-only passes drew into each. Candidates only.
struct DepthTarget {
    uint32_t format{}, width{}, height{};
    uint32_t transitions{}, depthOnlyBinds{};
};
inline constexpr size_t maxDepthTargets = 16;

inline long noteDepthTarget(DepthTarget* t, size_t& n, uint32_t format, uint32_t w, uint32_t h) {
    for (size_t i = 0; i < n; ++i)
        if (t[i].format == format && t[i].width == w && t[i].height == h) {
            ++t[i].transitions;
            return static_cast<long>(i);
        }
    if (n >= maxDepthTargets)
        return -1;
    t[n] = DepthTarget{format, w, h, 1, 0};
    return static_cast<long>(n++);
}

// Update 31: an orthographic matrix in uploaded constants (a shadow cascade's box is
// one). Either layout: translation in row 3 or column 3, the other one (0,0,0,1);
// a 3x3 part of real scale (a box at least 8 m and at most 40 km across on each axis);
// not a camera (those are perspective: w carries depth). halfExtents gets each axis's
// half size in metres (1 / length of its 3x3 row or column, whichever layout matched).
inline bool orthoBox(const float* m, float* halfExtents) {
    for (int i = 0; i < 16; ++i)
        if (!std::isfinite(m[i]))
            return false;
    const bool rowForm = m[3] == 0.f && m[7] == 0.f && m[11] == 0.f && m[15] == 1.f;
    const bool colForm = m[12] == 0.f && m[13] == 0.f && m[14] == 0.f && m[15] == 1.f;
    if (rowForm == colForm) // neither, or an identity-like matrix with no translation
        return false;
    for (int a = 0; a < 3; ++a) {
        // rowForm (row vectors, translation in m[12..14]): axis a's scale is column a.
        const float x = rowForm ? m[a] : m[a * 4];
        const float y = rowForm ? m[4 + a] : m[a * 4 + 1];
        const float z = rowForm ? m[8 + a] : m[a * 4 + 2];
        const float len = std::sqrt(x * x + y * y + z * z);
        if (!(len > 0.f))
            return false;
        const float half = 1.f / len;
        if (half < 4.f || half > 20000.f)
            return false;
        halfExtents[a] = half;
    }
    // Real boxes are not unit-scale on every axis (that is a pure rotation / identity).
    if (std::fabs(halfExtents[0] - 1.f) < .01f && std::fabs(halfExtents[1] - 1.f) < .01f &&
        std::fabs(halfExtents[2] - 1.f) < .01f)
        return false;
    return true;
}

struct OrthoRow {
    uint32_t rva{}, recordOffset{}; // copy call site; offset of the matrix in its 256-byte record
    float half[3]{};
    uint32_t hits{};
};
inline constexpr size_t maxOrthoRows = 24;

// Same site and record offset with boxes within 1% merge (cascades keep their size).
inline bool noteOrtho(OrthoRow* rows, size_t& n, uint32_t rva, uint32_t off, const float* half) {
    for (size_t i = 0; i < n; ++i) {
        auto& r = rows[i];
        if (r.rva != rva || r.recordOffset != off)
            continue;
        bool same = true;
        for (int a = 0; a < 3; ++a)
            same = same && std::fabs(r.half[a] - half[a]) <= .01f * std::max(r.half[a], half[a]);
        if (same) {
            ++r.hits;
            return true;
        }
    }
    if (n >= maxOrthoRows)
        return false;
    rows[n] = OrthoRow{rva, off, {half[0], half[1], half[2]}, 1};
    ++n;
    return true;
}

// Update 32: ORTHO CAPTURE. Update 31 kept only box sizes; the dump needs the
// matrices themselves, the record around them, who uploaded them and where the
// head and flat camera were. Groups of up to orthoPerGroup boxes, each group a
// short burst (orthoGroupSpanMs, a few frames) every orthoGroupEveryMs, so a
// minute of head pitch shows up as about 24 snapshots.
inline constexpr uint32_t orthoGroups = 24, orthoPerGroup = 6, maxOrthoCaptures = orthoGroups * orthoPerGroup;
inline constexpr uint64_t orthoGroupEveryMs = 2500, orthoGroupSpanMs = 50;
inline constexpr uint32_t orthoWindowFloats = 96, orthoChain = 6;
struct OrthoCapture {
    uint64_t ms{}, frame{}, gpuVa{};  // since the run began; finder frame; copy destination + its offset
    uint32_t group{}, copyBytes{};     // group number; bytes in the copy (sampled)
    uint32_t matrixOffset{}, windowOffset{}; // bytes into the copy: the matrix, window[0]
    uint32_t windowFloats{}, rowForm{};      // floats kept; 1 translation in row 3, 0 in column 3
    uint32_t chain[orthoChain]{};            // game-module return RVAs, innermost first
    float half[3]{};
    float matrix[16]{};
    float window[orthoWindowFloats]{};       // the 256-byte record holding the matrix and 128 bytes after
    float head[6]{}, flat[6]{};              // position, forward (pose rows 3 and 2)
    uint32_t headValid{}, flatValid{};
};
struct OrthoGroupState {
    uint64_t startMs{};
    uint32_t groups{}, inGroup{};
    bool open{};
};
// Whether a box seen at nowMs is captured; opens a new group when due.
inline bool orthoTake(OrthoGroupState& s, uint64_t nowMs) {
    if (s.open && nowMs < s.startMs + orthoGroupSpanMs) {
        if (s.inGroup >= orthoPerGroup)
            return false;
        ++s.inGroup;
        return true;
    }
    s.open = false;
    if (s.groups >= orthoGroups || (s.groups && nowMs < s.startMs + orthoGroupEveryMs))
        return false;
    s = OrthoGroupState{nowMs, s.groups + 1, 1, true};
    return true;
}
// Cascade-shaped only: not a cube (decals, probes: all three halves within 2%) and
// not world-scale (any half over 2 km).
inline bool orthoWorthCapture(const float* half) {
    const float lo = std::min({half[0], half[1], half[2]}), hi = std::max({half[0], half[1], half[2]});
    return hi <= 2000.f && hi > lo * 1.02f;
}
// The window: the 256-byte record holding the matrix, plus 128 bytes, clipped to the copy.
inline void orthoWindow(uint32_t matrixOffset, uint32_t copyBytes, uint32_t& start, uint32_t& floats) {
    start = matrixOffset & ~255u;
    const uint32_t end = std::min<uint32_t>(start + orthoWindowFloats * 4, copyBytes & ~3u);
    floats = end > start ? (end - start) / 4 : 0;
}

// Kinds worth watching for binds: camera matrices, positions, projections (not jitter).
inline bool watchKind(MatchKind k) {
    return k != MatchKind::jitter && k != MatchKind::jitterScaled;
}

// Update 30.1: the finder's early stop (lighting + TAA + LOD each strongly confirmed) ended
// the 29 runs after ~2 s, too short to see binds while the head tilts. While the bind trace is
// on, only the ~60 s window (or the hard frame cap) ends a run.
inline bool finishNow(bool timedOut, bool categoriesDone, bool bindTraceActive) {
    return timedOut || (categoriesDone && !bindTraceActive);
}

} // namespace spidy::address_finder

