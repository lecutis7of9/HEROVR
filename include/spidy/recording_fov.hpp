#pragma once
#include "native_eye_frame.hpp"
#include <algorithm>
#include <cmath>
#include <iterator>

// RECORDING FOV (vr_settings::Values::recordingFov; pull request 21, iplusdvr):
// the monitor's view in immersive VR, for recording. Off, the window mirrors
// the left eye the headset showed, Spidy's drawings and all. An angle has the
// game render its own view again (stereo_probe.cpp, restGameView), which costs
// frame time, and the window shows that view, without Spidy's drawings. The
// view is the engine's active view, whose culling, occlusion and far
// buildings' full models the eyes rely on, so its lens never covers less than
// the head's lens covering both eyes: the angle is the least the window shows
// across, and a taller headset or a wider window gets more. A Quest 3 (54
// degrees out, 44 in, 48 up and down) in a 16:9 window needs about 128.
namespace spidy::recording_fov {
// The tab's choices, degrees across; 0 is off.
inline constexpr int choices[] = {0, 130, 140, 150};
// 0 for off; any other angle within the choices'.
inline int sanitized(int degrees) {
    return degrees <= 0 ? 0 : std::clamp(degrees, choices[1], std::end(choices)[-1]);
}
// The lens (tangents, as native_eyes::headView gives them) of a window
// `aspect` times as wide as it is high that holds `eyes` and shows at least
// `degrees` across: centred, so the window's picture is not stretched.
inline native_eyes::Bounds covering(const native_eyes::Bounds& eyes, int degrees, float aspect) {
    if (!std::isfinite(aspect) || aspect < .2f || aspect > 6.f)
        aspect = 16.f / 9;
    const float across = std::tan(static_cast<float>(sanitized(degrees)) * .00872664626f);
    const float halfHeight =
        std::max({across / aspect, -eyes.top, eyes.bottom, -eyes.left / aspect, eyes.right / aspect});
    return {-halfHeight * aspect, halfHeight * aspect, -halfHeight, halfHeight};
}
// Build 12 WINDOW LIGHTS: the game's view decides, for both eyes, which far
// buildings get their lit full models (ImpostorHQModelRegJob through IsActiveView
// 0x2c991a0) from visibility queries that answer frames later. Its lens is the
// head's lens covering both eyes, plus this margin on every side, so a building
// turning into either eye's edge was already in the game's view: never less.
inline constexpr float cullMarginDegrees = 6.f, widestDegrees = 80.f;
inline float widenedTangent(float t, float degrees) {
    if (!std::isfinite(t))
        return t;
    const float a = std::min(std::abs(std::atan(t)) * 57.2957795f + std::max(degrees, 0.f), widestDegrees);
    return std::copysign(std::max(std::tan(a * .0174532925f), std::abs(t)), t);
}
inline native_eyes::Bounds widened(const native_eyes::Bounds& head, float degrees = cullMarginDegrees) {
    return {widenedTangent(head.left, degrees), widenedTangent(head.right, degrees),
            widenedTangent(head.top, degrees), widenedTangent(head.bottom, degrees)};
}
// From the VR settings (game_xr.cpp), as they start and whenever they change.
void set(int degrees);
} // namespace spidy::recording_fov
