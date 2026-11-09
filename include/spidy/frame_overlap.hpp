#pragma once
// Which overlay submission slot the next frame may record into.
//
// D3D12Renderer::renderViews used to wait, on the VR thread, until the single
// command allocator, upload buffer, depth target and descriptors had retired,
// and only then record the frame that xrEndFrame hands to the runtime. A ring
// of slots lets that record happen while an earlier slot is still on the GPU.
// The picture does not change: each slot owns the resources that frame writes,
// and a slot is recorded again only after its fence has retired (or, when every
// slot is still busy, after the oldest one has).
//
// Pure. No device, no wait. Tests decide the slot; the renderer does the wait.
#include <cstdint>

namespace spidy::frame_overlap {
inline constexpr uint32_t slots = 3;
inline constexpr uint32_t off = 0, on = 1;

struct Choice {
    uint32_t index{};
    uint64_t waitFor{}; // 0: that slot's fence has already retired
};

// `fences[i]` is the last value signaled for slot i (0: never submitted).
// `completed` is the queue fence's completed value. The caller rejects
// UINT64_MAX (device removed) before this; that value compares as already
// retired and must not be passed in.
inline Choice pick(const uint64_t* fences, uint32_t count, uint64_t completed) {
    Choice waiting{0, 0};
    bool any = false;
    if (!fences || !count)
        return waiting;
    for (uint32_t i = 0; i < count; ++i) {
        if (fences[i] <= completed)
            return Choice{i, 0};
        if (!any || fences[i] < waiting.waitFor) {
            waiting = Choice{i, fences[i]};
            any = true;
        }
    }
    return waiting;
}
} // namespace spidy::frame_overlap
