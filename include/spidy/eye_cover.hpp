#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
//
// 0.0.74 test 10 WINDOW EDGES: lit windows went dark in a band about 10-15% wide at the outer edge of each eye
// (left edge of the left eye, right edge of the right eye), turning back on when the head turned toward them.
//
// CONFIRMED (Spider-Man2.exe 2.810.0.0, by disassembly):
// - The lens setter 0x2c75560 keeps the exact tangents at +400/+404 (left/right) and +408/+40c (top/bottom), and
//   besides them SYMMETRIC summaries: +420 = (right - left) / 2 at 0x2c7584c, +424 = (bottom - top) / 2 at
//   0x2c7589c, the full angles +410 = atan(right) - atan(left) and +414 = atan(bottom) - atan(top). It sets +438 = 1
//   (the "symmetric" flag) only when right == -left and bottom == -top (0x2c758a4..0x2c758bc).
// - A headset eye is off-centre (probe: left -1.486, right .906, top -1.043, bottom 1.542), so +420 = 1.196
//   (+-50.1 degrees) while the outer edge is at 1.486 (56.1 degrees); half of +410 is 49.1 degrees.
// - SetPose 0x2c76310 ends in 0x2c74d20, which builds the view's matrices from the exact tangents (the jitter
//   term uses +420 x +428, so the cover below is written after it).
// - Dozens of render readers take +420/+424 or +410/+414 without testing +438 (e.g. 0x2c9a000 detail scale,
//   0x2bcc1b0 occlusion reprojection, 0x2abb0c5 ApplyLighting's 1/+420, 0x2c77000 a symmetric frustum).
// CANDIDATE (not confirmed by a headset A/B yet): one of them culls the lit-window layer with a symmetric frustum
// from those summaries. That matches every observation: the band is about 12% (1 - 1.196/1.486 over the eye's
// width, +420) to 14% (+410) of the eye wide, only on the outer side, cut column by column, unchanged by CULL
// MARGIN (the game view's frustum) and by eye_occlusion = 0, and gone when the head turns.
// The fix: for each eye, after SetPose and before the eye's Submit 0x2c95200, the summaries cover the whole lens
// (+420 = max(|left|, |right|), +424 = max(|top|, |bottom|), +410 / +414 their full angles). The matrices and the
// exact tangents are untouched, so what is drawn where does not move; only a reader that used the summaries now
// sees the whole eye. The descriptor gets its own values back after Submit, so the mod's own lens maths (EYE LOD,
// texture requests, the window probe) are unchanged.
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace spidy::eye_cover {
// The two writes in 0x2c75560 that make the summaries (vmovss [rbx+420], xmm0 / vmovss [rbx+424], xmm1).
inline constexpr uint32_t halfWidthWriteRva = 0x2c7584c, halfHeightWriteRva = 0x2c7589c;
inline constexpr unsigned char halfWidthWriteBytes[] = {0xc5, 0xfa, 0x11, 0x83, 0x20, 0x04, 0x00, 0x00};
inline constexpr unsigned char halfHeightWriteBytes[] = {0xc5, 0xfa, 0x11, 0x8b, 0x24, 0x04, 0x00, 0x00};
inline constexpr uint32_t left = 0x400, right = 0x404, top = 0x408, bottom = 0x40c;
inline constexpr uint32_t angleWide = 0x410, angleHigh = 0x414, halfWide = 0x420, halfHigh = 0x424;

struct Cover {
    float halfWide{}, halfHigh{}, angleWide{}, angleHigh{};
    bool valid{};
};
// The symmetric lens that covers an off-centre one (tangents left < right, top < bottom).
inline Cover cover(float l, float r, float t, float b) {
    Cover c;
    if (!std::isfinite(l) || !std::isfinite(r) || !std::isfinite(t) || !std::isfinite(b) || !(l < r) || !(t < b))
        return c;
    c.halfWide = std::max(std::abs(l), std::abs(r));
    c.halfHigh = std::max(std::abs(t), std::abs(b));
    if (!(c.halfWide > 0) || !(c.halfHigh > 0) || c.halfWide > 100 || c.halfHigh > 100)
        return c;
    c.angleWide = 2 * std::atan(c.halfWide);
    c.angleHigh = 2 * std::atan(c.halfHigh);
    c.valid = true;
    return c;
}
// The fraction of an eye's width (outer side) a symmetric frustum of half tangent `half` leaves out.
inline float bandOutside(float l, float r, float half) {
    const float outer = std::max(std::abs(l), std::abs(r));
    return outer > half && r > l ? (outer - half) / (r - l) : 0.f;
}
// Writes the cover into a 0x440-byte view descriptor's float array; returns false (nothing written) for a lens
// that is not a usable off-centre one.
inline bool apply(float* values) {
    const auto c = cover(values[left / 4], values[right / 4], values[top / 4], values[bottom / 4]);
    if (!c.valid)
        return false;
    values[halfWide / 4] = c.halfWide;
    values[halfHigh / 4] = c.halfHigh;
    values[angleWide / 4] = c.angleWide;
    values[angleHigh / 4] = c.angleHigh;
    return true;
}
// The summaries saved before apply, put back after the eye's Submit.
struct Saved {
    float v[4]{};
};
inline Saved save(const float* values) {
    return {{values[angleWide / 4], values[angleHigh / 4], values[halfWide / 4], values[halfHigh / 4]}};
}
inline void restore(float* values, const Saved& s) {
    values[angleWide / 4] = s.v[0];
    values[angleHigh / 4] = s.v[1];
    values[halfWide / 4] = s.v[2];
    values[halfHigh / 4] = s.v[3];
}
inline constexpr const char* fallbackLine =
    "WINDOW EDGES: off - this game build's lens setter differs (2.810.0.0 expected), eye lenses left as they are\n";
} // namespace spidy::eye_cover
