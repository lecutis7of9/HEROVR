#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
//
// 0.0.76 TAA STILL (live.txt vr_taa_still, e.g. 0.97): SPIDY TAA's still share only, in place of this build's
// vr_aa::keepStill (0.9; C-still094 0.94). Unset (0): the build's own. The moving share (vr_aa::keepMoving), the
// easing between them, the clip and the camera-cut reset are unchanged: a pair's motion share is read back from the
// build's own keep and the same share is eased from the new still share instead.
#include <cmath>
#include <cstdint>

#include "spidy/vr_aa.hpp"

namespace spidy::taa_still {
inline constexpr uint32_t lowest = 600, highest = 990; // thousandths: 0.60-0.99 (never under the moving share)
inline bool valid(uint32_t milli) {
    return milli == 0 || (milli >= lowest && milli <= highest);
}
inline float share(uint32_t milli) {
    return milli && valid(milli) ? static_cast<float>(milli) / 1000.f : vr_aa::keepStill;
}
// `keep`: what the build's vr_aa gave this pair (0: shown as it came, kept 0).
inline float withStill(float keep, uint32_t milli) {
    if (!(keep > 0) || !milli || !valid(milli))
        return keep;
    const float span = vr_aa::keepStill - vr_aa::keepMoving;
    float m = span > 0 ? (vr_aa::keepStill - keep) / span : 0.f;
    m = !(m > 0) ? 0.f : m > 1 ? 1.f : m;
    const float still = share(milli);
    return still - (still - vr_aa::keepMoving) * m;
}
} // namespace spidy::taa_still
