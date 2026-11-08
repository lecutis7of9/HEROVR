#pragma once
// HERO VR 0.0.78: isolated ENGINE TAA 70% diagnostic experiment.
//
// The public 0.0.78 launcher passes 67 for live.txt:
//     vr_aa = game
//     sm1style = on
//     vr_render = upscaled
// Its own per-eye engine-TAA path uses the reduced size if the executable and
// TAA-target checks succeed. On this experimental DLL only, shift that one
// explicit 67% request to 70%, matching our successful custom-TAA baseline.
// The launcher default (100%) and all non-SM1STYLE AA modes stay unchanged.
//
// This header never enables TAA by itself; existing buffer, command, history,
// and upscaler-validation guards still determine whether it can run.
#include <cstdint>
namespace spidy::engine_upscale70 {
inline constexpr uint32_t selectedByLauncher = 67;
inline constexpr uint32_t experimentalPercent = 70;
inline constexpr uint32_t percent(uint32_t requested, bool sm1styleRequested) {
    return sm1styleRequested && requested == selectedByLauncher ? experimentalPercent : requested;
}
static_assert(percent(67, true) == 70);
static_assert(percent(67, false) == 67);
static_assert(percent(100, true) == 100);
static_assert(percent(0, true) == 0);
} // namespace spidy::engine_upscale70
