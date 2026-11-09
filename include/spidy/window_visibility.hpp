#pragma once
// Experimental SM2 window-model visibility gate. No new engine offsets.
// The current 2.810 launcher keeps the primary game view rendering; only its
// impostor/HQ-building visibility result may be lifted. Avoid the historical
// per-eye +0x88 rewrite, which caused lit-window flicker.
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace spidy::window_visibility {

// An impostor query can arrive with either the active game view directly
// or a query context whose existing +0x1dc8 parent points to that view.
// Mirror the two cases already used by the read-only WINDOW PROBE.
enum Owner : uint32_t { other = 0, direct = 1, parent = 2 };
inline Owner owner(uintptr_t view, uintptr_t primary, uintptr_t contextParent, bool eyeQuery) {
    if (!view || !primary || eyeQuery)
        return other;
    if (view == primary)
        return direct;
    if (contextParent == primary)
        return parent;
    return other;
}
inline bool eligible(Owner owner, bool vrEnabled, bool primaryResting, bool bytesKnown, uint32_t percentage) {
    return owner != other && vrEnabled && !primaryResting && bytesKnown && percentage > 100;
}
// First experiment caps the lift at +50%, rather than old +100% on both
// eyes. The game's input is never changed when invalid or disabled.
inline float reach(float game, uint32_t percentage) {
    if (!std::isfinite(game) || game <= 1.f || game >= 5000.f || percentage <= 100)
        return game;
    const uint32_t bounded = std::min(percentage, 150u);
    return game * (static_cast<float>(bounded) / 100.f);
}

} // namespace spidy::window_visibility
