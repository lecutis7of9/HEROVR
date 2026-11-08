#pragma once
// HERO VR 0.0.78 — alternate-eye rendering (AER) scheduling prototype.
//
// PURE PLANNER ONLY. It never suppresses a draw or reuses an old eye texture.
// The actual renderer must separately prove that a real cached source is
// fence-complete, and reproject it to the target XR pose with depth/occlusion
// handling before enabling AER. No launch flag or runtime AER is provided here.
#include <cstdint>

namespace spidy::aer {
enum class Reason : uint32_t {
    ready = 0,
    noGeneration,
    noTracking,
    sceneCut,
    noCachedRealEye,
    staleCachedRealEye,
    gpuNotComplete,
    missingDepthReprojection,
    missedDeadline,
};
inline constexpr uint64_t maxSourceAge = 2; // game-frame generations, not seconds
struct Input {
    uint64_t generation{};
    uint64_t lastRealEyeGeneration[2]{};
    bool trackingValid{};
    bool sceneCut{};
    bool realEyeReady[2]{};
    bool gpuFenceComplete[2]{};
    bool depthReprojectionReady{};
    bool beforeDeadline{};
};
struct Plan {
    uint32_t renderEye{};      // the one eye we WOULD render this frame
    uint32_t synthesizeEye{};  // the other eye whose last real frame must be warped
    Reason reason{Reason::noGeneration};
    bool eligible() const { return reason == Reason::ready; }
};
inline Plan schedule(const Input& input) {
    // Choose by the monotonically increasing game generation, *not* by the
    // asynchronous callback order (which can arrive in different orders).
    const uint32_t fresh = static_cast<uint32_t>(input.generation & 1u);
    const uint32_t synthetic = 1u - fresh;
    Plan plan{fresh, synthetic, Reason::noGeneration};
    if (!input.generation) return plan;
    if (!input.trackingValid) { plan.reason = Reason::noTracking; return plan; }
    if (input.sceneCut) { plan.reason = Reason::sceneCut; return plan; }
    const uint64_t real = input.lastRealEyeGeneration[synthetic];
    if (!input.realEyeReady[synthetic] || !real || real > input.generation) {
        plan.reason = Reason::noCachedRealEye; return plan;
    }
    if (input.generation - real > maxSourceAge) {
        plan.reason = Reason::staleCachedRealEye; return plan;
    }
    if (!input.gpuFenceComplete[synthetic]) {
        plan.reason = Reason::gpuNotComplete; return plan;
    }
    // Rotation-only homography is not sufficient for Spider-Man swinging
    // close to buildings. Until depth-backed reprojection is verified, the
    // planner always requests full stereo fallback.
    if (!input.depthReprojectionReady) {
        plan.reason = Reason::missingDepthReprojection; return plan;
    }
    if (!input.beforeDeadline) {
        plan.reason = Reason::missedDeadline; return plan;
    }
    plan.reason = Reason::ready;
    return plan;
}
} // namespace spidy::aer
