#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
//
// 0.0.73 test 6 WINDOW POP FIX (the owner after test 5: building faces snap from sparse to densely lit
// windows one or two blocks ahead, and the facade texture sharpens with them).
// CONFIRMED in this mod's code: while immersive, activeView (stereo_probe.cpp) gives the game's own view
// (the active view, [0xc2cae70]) a lens covering both eyes plus CULL MARGIN (recording_fov::widened; about
// 128 degrees across on a Quest 3) so its culling covers both eyes; that view also decides which far
// buildings get their lit full models for both eyes (recording_fov.hpp WINDOW LIGHTS).
// CONFIRMED by disassembly (shared_lod.hpp): 0x2c9a000 writes view +0x1b24 = K / tangent, and the
// level-of-detail and screen-size tests multiply it by +0x1b28 (1.0 from the view's init, written by
// nothing else in the render code). So the widened lens shrinks the game view's detail distances by
// about the stock tangent over ours - flat play's camera never has that.
// CANDIDATE (not measured in the headset yet): that this is the window pop the owner sees, and that the
// lit full models' swap follows +0x1b28 (the impostors' own +0x88 145 m distance is the other candidate,
// still gated off in stereo_probe.cpp for its flicker).
// The fix: ON writes the game view's +0x1b28 = flatMatch(): the widening undone (flat play's detail distance),
// at most maxBoost; the eyes' SHARED LOD keeps reading the game view's own scale (unchanged from test 5).
// OFF (or not immersive) puts 1.0 back. SpidyVR does the same thing for its eyes (matchDistances: the eyes'
// detail distances matched to the flat camera).
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace spidy::window_pop {
inline constexpr float maxBoost = 2.f; // moderate GPU: never more than twice the game view's own reach
// The game view's +0x1b28 that gives it the flat camera's detail distance: the smaller of our lens' half
// width and half height over the stock camera's (tangents), within 1..maxBoost; 1 for anything unusable.
inline float flatMatch(float stockHalfWidth, float stockHalfHeight, float oursHalfWidth, float oursHalfHeight) {
    for (const float v : {stockHalfWidth, stockHalfHeight, oursHalfWidth, oursHalfHeight})
        if (!std::isfinite(v) || !(v > .01f) || v > 100.f)
            return 1.f;
    const float r = std::min(oursHalfWidth / stockHalfWidth, oursHalfHeight / stockHalfHeight);
    return std::isfinite(r) ? std::clamp(r, 1.f, maxBoost) : 1.f;
}
// The game view's own scale for the eyes' SHARED LOD: its +0x1b28 without our boost (`applied`, 1 when none).
inline float ownMultiplier(float multiplier, float applied) {
    if (!std::isfinite(multiplier) || !std::isfinite(applied) || !(applied >= 1.f))
        return multiplier;
    return std::abs(multiplier - applied) < 1e-4f ? 1.f : multiplier;
}
// combat38 window_pop: value 0 ON (the default), 1 OFF.
inline bool on(uint32_t value) { return value == 0; }
} // namespace spidy::window_pop
