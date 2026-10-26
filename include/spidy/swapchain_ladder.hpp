#pragma once
// 0.0.6 hotfix (Update 39.1.1, Bear's report rep0060: Virtual Desktop answered "Create eye swapchain
// XrResult=-2", XR_ERROR_RUNTIME_FAILURE, and VR ended with the game on its flat screen). The eyes'
// swapchains are tried again in steps instead: first exactly as 0.0.5 made them (format, size, flags
// and counts are unchanged since 0.0.5), again after a pause, then with fewer usage flags, the other
// format the copy allows, and smaller sizes. Every try is logged. No game address is involved.
#include "spidy/eye_resolution.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace spidy {
struct SwapchainTry {
    int64_t format{};
    uint64_t usage{};
    uint32_t width{}, height{};
    unsigned delayMs{}; // the pause before this try
    const char* why = "";
};
// format: the chosen format; others: the runtime's other formats the eyes may use (in preference
// order); fullUsage: the 0.0.5 usage flags; lessUsage: the flags without what only some paths need
// (equal to fullUsage when there is nothing to drop); width x height: the chosen eye size;
// fixedSize: a session built again must keep the game's eyes (no smaller sizes, no pauses).
inline std::vector<SwapchainTry> swapchainLadder(int64_t format, const std::vector<int64_t>& others,
                                                 uint64_t fullUsage, uint64_t lessUsage, uint32_t width,
                                                 uint32_t height, bool fixedSize) {
    std::vector<SwapchainTry> tries;
    const auto add = [&](int64_t f, uint64_t u, uint32_t w, uint32_t h, unsigned delay, const char* why) {
        if (!validEyeSize(w) || !validEyeSize(h))
            return;
        for (const auto& t : tries)
            if (t.format == f && t.usage == u && t.width == w && t.height == h && !delay)
                return;
        tries.push_back({f, u, w, h, delay, why});
    };
    add(format, fullUsage, width, height, 0, "the 0.0.5 parameters");
    // A session built again after a loss is tried every 2 s anyway (XrRuntime::recover): no pauses there.
    if (!fixedSize) {
        add(format, fullUsage, width, height, 500, "the 0.0.5 parameters again after 0.5 s");
        add(format, fullUsage, width, height, 1500, "the 0.0.5 parameters again after 1.5 s");
    }
    if (lessUsage != fullUsage)
        add(format, lessUsage, width, height, 0, "without the transfer-source usage");
    for (const auto f : others)
        if (f != format)
            add(f, fullUsage, width, height, 0, "another format");
    if (!fixedSize)
        for (const uint32_t percent : {90u, 80u, 70u, 60u}) {
            const auto size = scaledEyeSize(width, height, percent);
            add(format, fullUsage, size[0], size[1], 0, "a smaller eye size");
        }
    return tries;
}
inline std::string describeSwapchainTry(unsigned index, unsigned count, const SwapchainTry& t) {
    return "Eye swapchain try " + std::to_string(index + 1) + "/" + std::to_string(count) + ": " + t.why +
           " (format " + std::to_string(t.format) + ", " + std::to_string(t.width) + "x" +
           std::to_string(t.height) + ", usage 0x" + [&] {
               static const char digits[] = "0123456789abcdef";
               std::string s;
               uint64_t u = t.usage;
               do {
                   s.insert(s.begin(), digits[u & 15]);
                   u >>= 4;
               } while (u);
               return s;
           }() + ", 1 sample, 1 mip, 1 layer)";
}
} // namespace spidy
