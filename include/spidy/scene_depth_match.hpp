#pragma once
#include <cstdint>

// 0.0.75 SEE-THROUGH: which game depth buffers can stand for an eye's scene depth, and which
// eye a G-buffer pass belongs to when its list carries no eye marker.
namespace spidy::scene_depth {

// The eye's own size, or the same picture at a lower render size (an upscaler, SM1STYLE's
// 67 %): the same shape within 1.5 % and at least 30 % of the eye's width. The overlay reads it
// by the pixel's place in the picture, so any such size lines up.
constexpr bool sizeFits(uint64_t depthW, uint64_t depthH, uint64_t eyeW, uint64_t eyeH) {
    if (!depthW || !depthH || !eyeW || !eyeH || depthW > eyeW || depthH > eyeH)
        return false;
    if (depthW == eyeW && depthH == eyeH)
        return true;
    if (depthW * 10 < eyeW * 3)
        return false;
    const auto a = depthW * eyeH, b = depthH * eyeW; // same shape: dw / dh == w / h
    const auto diff = a > b ? a - b : b - a;
    return diff * 1000 <= b * 15;
}

// The eye whose render job is the only one open now (bit 0 left, bit 1 right), else 2: a pass
// recorded while both or neither are open is not given to an eye.
constexpr unsigned soleOpenEye(unsigned openMask) {
    return openMask == 1 ? 0u : openMask == 2 ? 1u : 2u;
}

} // namespace spidy::scene_depth
