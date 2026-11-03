#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
//
// 0.0.76 SHADOW STABLE (live.txt vr_shadow_stable, live, off by default; stereo_probe.cpp).
// The far building shadows come from ONE far cache for the whole frame (0x2ad11b0, the static
// object 0xbf57ce0). Exe-confirmed (Spider-Man2.exe 2.810.0.0, by disassembly; BUILD-SM2.md section 77):
// - 0x2ad5380 stamps the cache: +0x2454 = the frame (0xc323788) when an active view takes it, +0x2458
//   when a view of kind 1 or 2 (the eyes are kind 2, view +0x1db4) does.
// - In 0x2ad11b0 an ACTIVE view (its IsActiveView at 0x2ad1238) recomputes the shared far cache
//   (0x2ad13b8 on) every time; a view that is not active takes what the active view made while that
//   is under 2 frames old (0x2ad1bbf), and only otherwise makes it itself.
// - With far cascades from the cache, the fit trims that view's base cascades to 2 and shrinks their
//   range (0x2a8e2cf-0x2a8e2fa), so what the cache holds also sets the near shadows' layout.
// - FAR SHADOWS EYES+CACHE answers "active" for the left eye inside the cache, so the one shared far
//   cache is recomputed twice a frame, by the game (head) view and by the left eye, on render jobs
//   whose order is not fixed; the right eye takes whichever came last.
// - The game already snaps the cascades to whole shadow texels (the centre with vroundsd floor, the base
//   fit to extent / resolution), so nothing here adds its own snapping.
// CANDIDATE (not confirmed): that the two recomputes a frame are what makes the far building shadows flicker.
// ON: the left eye is no longer answered "active" inside the cache: the head view makes the far cache
// once a frame and both eyes share it. The eyes are still answered "active" at the fit (0x2a8e145), so
// they keep their far cascades; FAR SHADOWS' auto-revert still judges them. With far_shadows off or
// eyes (1), ON changes nothing (the eyes are not "active" in the cache there anyway).
#include <cstdint>

namespace spidy::shadow_stable {
inline constexpr uint32_t magic = 0x42545353; // 'SSTB'
inline constexpr uint32_t version = 1;
inline constexpr uint32_t off = 0, on = 1;
// SpidyShadowStable's refusal (a value past on).
inline constexpr uint32_t refused = 4711;

struct Data {
    uint32_t magic{spidy::shadow_stable::magic}, version{spidy::shadow_stable::version}, size{sizeof(Data)}, on{};
    uint64_t held{};        // the left eye's cache asks left to the game (head) view while ON
    uint64_t answered{};    // the left eye's cache asks answered "active" (EYES+CACHE, OFF)
    uint64_t matchFits{};   // SHADOW MATCH: eye fits made with the head's lens
    uint64_t matchSkipped{}; // SHADOW MATCH: eye fits on the eye's own lens (no fresh head lens)
    uint64_t changes{};     // times the switch changed
    uint64_t reserved{};
    uint64_t sequence{};    // odd while being written
};
static_assert(sizeof(Data) == 72);
} // namespace spidy::shadow_stable
