#pragma once
// HERO VR 0.0.78: isolated building detail/primary-view window test.
// Changes are opt-in through the separate experimental DLL, not the release.
// The eye texture request lift below never lowers the engine's request and
// caps the extra demand; the window distance lift applies only to one primary
// camera (never both eyes), and respects the existing FAR WINDOWS off switch.
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace spidy::vr_building_detail {
// This is a request for higher mip residency, not a command to load assets.
// On a 70% scene the engine can ask for a texture as if it were only ~70% of
// its final pixel footprint. Cap the lift while checking the copied job's
// scalar before any native write.
inline float textureRequest(float asked, float fullImageWanted) {
    if (!std::isfinite(asked) || !std::isfinite(fullImageWanted) ||
        !(asked > 1.f && asked < 100000.f) || !(fullImageWanted > 1.f && fullImageWanted < 100000.f))
        return asked;
    const float raised = std::min(fullImageWanted, asked * 1.5f);
    return raised > asked * 1.02f ? raised : asked;
}
// Unlike the older per-eye approach, one active game view owns the decision
// to use lit full-building models. Rewriting separate eye queries made them
// compete, flickering windows. Only the primary (game) view may be lifted.
inline bool primaryWindowOwner(bool enabled, bool restingMain, bool knownLayout,
                               uintptr_t view, uintptr_t primary, uint32_t farWindowsPercent) {
    return enabled && !restingMain && knownLayout && view && primary && view == primary &&
           farWindowsPercent > 100;
}
inline float windowReach(float original, uint32_t requestedPercent) {
    if (!std::isfinite(original) || !(original > 1.f && original < 5000.f) ||
        requestedPercent <= 100)
        return original;
    // We deliberately limit the first experiment to +50%, rather than the
    // old +100% per-eye query change that visibly flickered.
    const uint32_t pct = std::min(requestedPercent, 150u);
    return original * (static_cast<float>(pct) / 100.f);
}
} // namespace spidy::vr_building_detail
