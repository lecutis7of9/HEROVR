#pragma once
#include <cstdint>

// Update 24: live.txt eye_debug, a diagnostic for far-away flicker. With
// eyeDebugSame both of the headset's images are made from the LEFT eye's
// picture (the same game render, sharpened the same way), so the two eyes can
// no longer disagree: a shadow edge, roof line or distant window that each eye
// renders differently (its own shadow cascades, its own pixel grid) cannot
// flicker between them. The world looks flat (no depth) while it is on, the
// game renders exactly as before and nothing is restarted, so it is safe to
// switch on and off while playing. Only what reaches the headset changes:
// both eyes still get their own pose, so Spidy's own drawings (hands, webs,
// hologram) are drawn per eye on top and may look slightly doubled.
// Update 26: the headset layer now gives the right image the left eye's view
// (game_xr.cpp, native_gpu::pairEyeDebug): the picture was made with the left
// eye's lens, and shown through the right eye's mirror-image lens it slid and
// stretched sideways. Spidy's drawings follow, so nothing is doubled now.
// Update 25's HEAD SHADOWS (mode 2) is gone again: SM2 does not fit an eye's
// shadow cascades inside its scene submit, so it could never take effect.
namespace spidy::eye_debug {
inline constexpr uint32_t eyeDebugOff = 0, eyeDebugSame = 1, eyeDebugModes = 2;
inline constexpr bool validEyeDebug(uint64_t mode) {
    return mode < eyeDebugModes;
}
// The staged eye picture headset image `eye` (0 left, 1 right) is made from.
inline constexpr unsigned eyeDebugSource(uint32_t mode, unsigned eye) {
    return mode == eyeDebugSame ? 0u : (eye ? 1u : 0u);
}
} // namespace spidy::eye_debug
