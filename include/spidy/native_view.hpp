#pragma once
#include "math.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace spidy::native_view {
// The renderer's pose uses right/down/forward rows and translation in row 3.
// OpenXR uses right/up/back. This conversion does not change world units.
inline Mat4 relativePose(const Mat4& native, Pose relative) {
    const Vec3 right{native[0], native[1], native[2]};
    const Vec3 up{-native[4], -native[5], -native[6]};
    const Vec3 back{-native[8], -native[9], -native[10]};
    auto world = [&](Vec3 p) { return right * p.x + up * p.y + back * p.z; };
    const Vec3 x = world(relative.orientation.rotate({1, 0, 0}));
    const Vec3 y = -world(relative.orientation.rotate({0, 1, 0}));
    const Vec3 z = -world(relative.orientation.rotate({0, 0, 1}));
    const Vec3 p = Vec3{native[12], native[13], native[14]} + world(relative.position);
    return {x.x, x.y, x.z, 0, y.x, y.y, y.z, 0, z.x, z.y, z.z, 0, p.x, p.y, p.z, 1};
}
inline bool validPose(const Mat4& m) {
    for (float f : m)
        if (!std::isfinite(f) || std::abs(f) > 1e6f)
            return false;
    const Vec3 x{m[0], m[1], m[2]}, y{m[4], m[5], m[6]}, z{m[8], m[9], m[10]};
    return std::abs(length(x) - 1) < .01f && std::abs(length(y) - 1) < .01f &&
           std::abs(length(z) - 1) < .01f && std::abs(dot(x, y)) < .01f && std::abs(dot(y, z)) < .01f &&
           std::abs(dot(x, z)) < .01f && dot(cross(x, y), z) > .99f &&
           std::abs(m[3]) + std::abs(m[7]) + std::abs(m[11]) < .01f && std::abs(m[15] - 1) < .01f;
}
// Row-major OpenXR-style view matrix for geometry over the native eye image.
inline Mat4 view(const Mat4& m) {
    const Vec3 right{m[0], m[1], m[2]}, up{-m[4], -m[5], -m[6]}, back{-m[8], -m[9], -m[10]};
    const Vec3 p{m[12], m[13], m[14]};
    return {right.x, right.y, right.z, -dot(right, p), up.x, up.y, up.z, -dot(up, p),
            back.x,  back.y,  back.z,  -dot(back, p),  0,    0,    0,    1};
}
struct Descriptor {
    float values[272]{};
    Mat4 pose() const {
        Mat4 result;
        std::memcpy(result.data(), values, 64);
        return result;
    }
    float nearZ() const {
        return values[0x3f8 / 4];
    }
    float farZ() const {
        return values[0x3fc / 4];
    }
};
static_assert(sizeof(Descriptor) == 0x440);
inline bool valid(const Descriptor& d) {
    if (!validPose(d.pose()) || !std::isfinite(d.nearZ()) || !std::isfinite(d.farZ()) || d.nearZ() <= 0 ||
        d.farZ() <= d.nearZ() || d.farZ() > 1e6f)
        return false;
    for (int i = 0x400 / 4; i <= 0x40c / 4; ++i)
        if (!std::isfinite(d.values[i]) || std::abs(d.values[i]) > 100)
            return false;
    return d.values[0x400 / 4] < d.values[0x404 / 4] && d.values[0x408 / 4] < d.values[0x40c / 4];
}
// The engine's distance scale for a view's lens, as 189e4b0 puts it on view
// +1cc4 in each scene submit: 1 for a 68-degree-wide 16:9 lens (tan 34
// degrees = .6745 over tan of half its width, +410), less for a wider one;
// across the height (tan 20.8 degrees = .3794, +414) where the lens is wider
// than 16:9 (+41c), and the bare number with flag 2 at +438. Every distance a
// view's model job judges by scales with it times view +1cc8 (1796d20): which
// detail level a model draws, how far it is drawn at all, and up to where its
// textures are asked for (16 times its size times the scale).
inline float distanceScale(const Descriptor& d) {
    uint32_t flags{};
    std::memcpy(&flags, &d.values[0x438 / 4], 4);
    const bool wide = d.values[0x41c / 4] > 16.f / 9;
    const float scale = wide ? .37941101f : .67450851f;
    return (flags & 2) ? scale : scale / std::tan(d.values[(wide ? 0x414 : 0x410) / 4] * .5f);
}
// The multiplier (view +1cc8) that gives an eye the distances of the engine's
// 68-degree lens where its own lens is wider, at most `most` times its own.
inline float eyeDetail(const Descriptor& eye, float most = 3) {
    const float scale = distanceScale(eye);
    return std::isfinite(scale) && scale > 0 ? std::clamp(1 / scale, 1.f, most) : 1.f;
}
// Update 28, EYE LOD (stereo_probe.cpp eyeLod): Spider-Man 2's copy of
// distanceScale. The ADDRESS FINDER runs of Oct 23 2026 (27.4) found 0.4667 at
// +1b24 of the game's active view, whose lens there was the union of both eyes
// (about 110 degrees across). A candidate from that evidence, not a
// disassembled address. The Remastered multiplier's SM2 place (+1cc8 there) is
// still SM2TODO.
inline constexpr uint32_t sm2DistanceScaleOffset = 0x1b24;
// Update 28.1: Update 28 asked each EYE view's +1b24 to equal that eye's own
// lens scale (1e-3). A Quest 3 eye lens is about 94 degrees across (0.63), not
// the 110-degree active view's 0.4667, so no frame ever matched and EYE LOD
// failed after 120 frames without a visible reason. Now the value is read
// first and judged by range and steadiness, never by an exact match.
inline constexpr float eyeLodLowest = .05f, eyeLodHighest = 4.f;
inline constexpr unsigned eyeLodSettleFrames = 30;    // the same value this long before the first write
inline constexpr unsigned eyeLodBadFrames = 120;      // out of range this long: failed
inline constexpr unsigned eyeLodUnsteadyFrames = 900; // never steady this long (~10 s): failed
inline bool eyeLodSane(float v) {
    return std::isfinite(v) && v >= eyeLodLowest && v <= eyeLodHighest;
}
// What EYE LOD writes over the engine's value `base`: base times the eye lens's
// eyeDetail (the 68-degree lens's distances, as Update 28), or the flat game's
// own camera scale (`flat`) if that is further; never below base, at most 3
// times it.
inline float eyeLodTarget(float base, const Descriptor& lens, float flat) {
    if (!eyeLodSane(base))
        return base;
    const float raised = std::max(base * eyeDetail(lens), eyeLodSane(flat) ? flat : 0.f);
    return std::clamp(std::clamp(raised, base, base * 3), eyeLodLowest, eyeLodHighest);
}
// Why EYE LOD stopped, for the report, the console and the hologram's subtext.
enum class EyeLodReason : uint32_t { none, readFailed, writeFailed, outOfRange, unsteady };
inline constexpr uint32_t eyeLodReasons = 5;
inline const char* eyeLodReasonText(uint32_t reason) {
    switch (static_cast<EyeLodReason>(reason)) {
    case EyeLodReason::readFailed:
        return "the eye view could not be read";
    case EyeLodReason::writeFailed:
        return "the write to the eye view failed";
    case EyeLodReason::outOfRange:
        return "the eye view's value was out of range";
    case EyeLodReason::unsteady:
        return "the eye view's value never held still";
    default:
        return "";
    }
}
// One eye's EYE LOD bookkeeping (render thread).
struct EyeLodTrack {
    float base{};       // the engine's own value, given back when EYE LOD stops
    float ours{};       // what was last written (0 none)
    unsigned steady{};  // frames the engine's value has stayed at base
    unsigned bad{};     // consecutive out-of-range frames
    unsigned waiting{}; // frames since the start or the last write
};
struct EyeLodStep {
    enum Kind : uint8_t { write, held, settling, outOfRange, unsteady } kind;
    float value; // what to write (write, held), else what was read
};
// One frame of an eye's +1b24 (`current`, already read). `ours` unchanged since
// the last write is held (rewritten only if the target moved). Any other value
// is the engine's: once in range and steady for eyeLodSettleFrames, it becomes
// base and the target is written. outOfRange and unsteady are failures.
inline EyeLodStep eyeLodStep(EyeLodTrack& t, float current, const Descriptor& lens, float flat) {
    if (t.ours > 0 && current == t.ours) {
        t.bad = 0;
        t.waiting = 0;
        return {EyeLodStep::held, eyeLodTarget(t.base, lens, flat)};
    }
    t.ours = 0; // the engine holds its own value there now: nothing of ours to give back
    if (!eyeLodSane(current)) {
        t.steady = 0;
        return {++t.bad > eyeLodBadFrames ? EyeLodStep::outOfRange : EyeLodStep::settling, current};
    }
    t.bad = 0;
    if (t.steady && std::fabs(current - t.base) <= 1e-4f * t.base)
        ++t.steady;
    else
        t.base = current, t.steady = 1;
    if (t.steady >= eyeLodSettleFrames) {
        t.waiting = 0;
        return {EyeLodStep::write, eyeLodTarget(t.base, lens, flat)};
    }
    return {++t.waiting > eyeLodUnsteadyFrames ? EyeLodStep::unsteady : EyeLodStep::settling, current};
}
// SpidyEyeLodData (stereo_probe.cpp), read by tools/eye_lod.py.
struct EyeLodData {
    uint32_t magic = 0x444f4c45, version = 1, bytes = sizeof(EyeLodData), offset = sm2DistanceScaleOffset;
    int64_t sequence{};
    uint32_t state{};  // 0 off, 1 on, 2 failed
    uint32_t reason{}; // EyeLodReason
    uint64_t frames{}, writes{}, held{}, settling{};
    float lastRead{}, base{}, target{}, flat{}, eyeScale{};
    uint32_t eye{}; // the eye whose value failed (0 left, 1 right)
};
static_assert(sizeof(EyeLodData) == 88);
// How strongly a view's model job asks for textures (1796d20, job +2fc): half
// the image's pixels per unit of lens tangent, across whichever of its width
// and its height (behind half tangents +420 and +424) has more of them.
inline float requestScale(float width, float height, float halfWidth, float halfHeight) {
    return .5f * std::max(height / halfHeight, width / halfWidth);
}
} // namespace spidy::native_view
