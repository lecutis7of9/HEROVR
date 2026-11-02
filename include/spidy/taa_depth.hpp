#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
//
// 0.0.76 A-depth (live.txt vr_taa_depth, auto by default: off until the eye depth is CONFIRMED): SPIDY TAA's history turned with the eyes' own depth.
// The history pass (native_eye_gpu.cpp taaShader, its TAA_DEPTH variant) otherwise turns the last picture with
// the head's turn only (eye_history::historyReprojection), so whatever is near and shifts against the far city
// as the head travels (parallax) is clipped away. With the eye's depth each pixel is put back in the world
// (the depth, reversed-Z with an infinite far plane: view distance = near / d), moved into the previous eye
// (its turn and its travel) and looked up there; the previous eye's own depth at that place must agree, or
// the pixel was hidden last pair (disoccluded) and starts again. The blend shares and the YCoCg clip are the
// same as without it. A pair without both eyes' depth (or a depth the checks refuse) is turned as before.
#include <array>
#include <cmath>
#include <cstdint>

#include "spidy/eye_history.hpp"

namespace spidy::taa_depth {
// How far apart the previous eye's depth and the depth this pixel should have there may be, as a share of the
// larger (reversed-Z: a share of the distance too). Any of the 2 x 2 depths around the place may agree.
inline constexpr float tolerance = .1f;
// Only depths of the eyes' render size, one plane of 32-bit floats, rows on 256-byte steps.
inline constexpr uint32_t rowAlign = 256;
inline constexpr uint64_t largest = 256ull << 20; // bytes a copy may take (a 4K x 4K eye is 64 MB)
// live.txt vr_taa_depth: 0 off, 1 on, 2 auto (the default): on only once the TAA WATCH depth log
// (taa_watch::depthVerdict) has CONFIRMED the eye depth - eye-sized, out of both eyes' jobs, never the flat view's.
inline constexpr uint32_t automatic = 2;
inline bool active(uint32_t mode, bool confirmed) {
    return mode == 1 || (mode == automatic && confirmed);
}

// The previous eye's (U, V, W) shift for the eyes' travel, the constant term next to
// historyReprojection's homography: (U, V, W) = distance * H (u, v, 1) + shift, W the distance in the
// previous eye. The world positions are EyeView::world[12..14], metres.
inline bool historyShift(const eye_history::EyeView& previous, const eye_history::EyeView& current,
                         std::array<float, 3>& out) {
    const auto& p = previous.tangents;
    if (!(p[1] > p[0]) || !(p[3] > p[2]))
        return false;
    float moved[3];
    for (int i = 0; i < 3; ++i)
        moved[i] = current.world[12 + i] - previous.world[12 + i];
    float local[3]; // in the previous eye's right, up, back
    for (int a = 0; a < 3; ++a) {
        const float sign = a == 0 ? 1.f : -1.f;
        local[a] = sign * (previous.world[a * 4] * moved[0] + previous.world[a * 4 + 1] * moved[1] +
                           previous.world[a * 4 + 2] * moved[2]);
    }
    const float w = p[1] - p[0], h = p[3] - p[2];
    out = {local[0] / w + p[0] / w * local[2], -local[1] / h - p[3] / h * local[2], -local[2]};
    for (float v : out)
        if (!std::isfinite(v))
            return false;
    return true;
}
struct Place {
    float u{}, v{};   // the previous eye's image, 0-1 from the top left
    float depth{};    // the reversed-Z depth the previous eye should hold there (0: sky)
    bool inFront{};
};
// The shader's sum (taaShader TAA_DEPTH): `h` historyReprojection's rows, `shiftPerNear` historyShift / near,
// `d` this pixel's depth. Sky (d 0) turns only; `nearRatio` the previous pair's near over this one's.
inline Place reproject(const std::array<float, 9>& h, const std::array<float, 3>& shiftPerNear, float u, float v, float d,
                       float nearRatio = 1.f) {
    float q[3];
    for (int r = 0; r < 3; ++r)
        q[r] = h[r * 3] * u + h[r * 3 + 1] * v + h[r * 3 + 2];
    const bool solid = d > 0 && d <= 1;
    if (solid)
        for (int r = 0; r < 3; ++r)
            q[r] = q[r] / d + shiftPerNear[r];
    Place out;
    out.inFront = q[2] > 1e-5f;
    if (!out.inFront)
        return out;
    out.u = q[0] / q[2];
    out.v = q[1] / q[2];
    out.depth = solid ? nearRatio / q[2] : 0.f;
    return out;
}
inline bool matches(float want, float had) {
    if (!std::isfinite(had) || had < 0 || had > 1)
        return false;
    return std::fabs(had - want) <= tolerance * std::fmax(had, want) + 1e-7f;
}
// A copy's row pitch in floats, or 0 when the footprint is not one plane of the eye's own size.
inline uint32_t pitchFloats(uint64_t rowPitch, uint32_t footprintW, uint32_t footprintH, uint64_t width, uint32_t height,
                            uint64_t bytes) {
    if (footprintW != width || footprintH != height || !width || !height || rowPitch % rowAlign ||
        rowPitch < width * 4 || bytes > largest || bytes < rowPitch * (height - 1) + width * 4)
        return 0;
    return static_cast<uint32_t>(rowPitch / 4);
}
} // namespace spidy::taa_depth
