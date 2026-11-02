#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
//
// 0.0.76 TAA JITTER (live.txt vr_taa_jitter, a diagnostic for the far windows that swim with a still head): what our
// own Halton (2, 3) sub-pixel lens step (eye_taa::step, game_xr) does. on: the 16-step walk as before (the default);
// off: no step, the lens and the history's views unjittered; fixed: one constant offset (the walk's second step,
// -0.25, +0.17 pixel) every pair; half: the same 16-step walk at half its size. SPIDY TAA and each eye's own history run
// in every mode; whatever the lens gets, the history's views and the headset layer get the same (game_xr). The game's
// own jitter (stereo_probe eyeJitter, vr_jitter / vr_game_jitter) is not this.
#include <cstdint>

#include "spidy/eye_taa.hpp"

namespace spidy::taa_jitter {
enum Mode : uint32_t { on = 0, off = 1, fixed = 2, half = 3 };
inline constexpr uint32_t modes = 4;
inline constexpr uint64_t fixedStep = 1; // eye_taa::step(1): the walk's second step
inline const char* name(uint32_t mode) {
    return mode == on ? "on" : mode == off ? "off" : mode == fixed ? "fixed" : mode == half ? "half" : "?";
}
// False: no step this pair (off). Else this pair's step in pixels, as eye_taa::step.
inline bool step(uint32_t mode, uint64_t n, float& x, float& y) {
    x = y = 0;
    if (mode == off)
        return false;
    eye_taa::step(mode == fixed ? fixedStep : n, x, y); // an unknown mode (never set: refused) walks as on
    if (mode == half) {
        x *= .5f;
        y *= .5f;
    }
    return true;
}
} // namespace spidy::taa_jitter
