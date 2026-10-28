#pragma once
#include <algorithm>
#include <cmath>
// Test build 11 EYE MARGIN: the 0.0.72 test video (Oct 28, a night city pan) showed a smeared
// band up to ~100 px wide at the right edge of the left eye's capture, wider while the head
// turned. The game draws about 39 pictures a second for a 90 Hz headset, so the runtime shows
// each one 2-3 times, turned to the newest head pose; turning reveals what lies past the
// picture's edge, and with no pixels there the runtime stretches the edge column.
// Each eye's picture is now drawn (and submitted) with its tangents 8% wider than the
// runtime's view on every side, so a turn shows real city there. Both axes grow alike, so
// pixels stay square; the swapchain keeps its size (8% fewer pixels per degree).
namespace spidy::eye_margin {
inline constexpr float scale = 1.08f;
inline constexpr float maxAngle = 1.48f; // ~85 degrees: a tangent never runs away
// One fov angle (radians, signed as OpenXR gives it) widened by scale in tangent space.
inline float widen(float angle, float by = scale) {
    if (!std::isfinite(angle))
        return angle;
    const float wider = std::atan(std::tan(angle) * by);
    return std::clamp(wider, -maxAngle, maxAngle);
}
} // namespace spidy::eye_margin
