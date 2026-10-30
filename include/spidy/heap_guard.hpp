#pragma once

#include <cstdint>

namespace spidy::heap_guard {

// 0.0.74 test 3 HEAP GUARD (the public 0.0.73 player's session game-vr-20261029-204133: the
// game's log "[Mem12] Aliased resource does not fit in the original heap ... Recreating heap
// and aliases." while loading a save, then CreateHeap 0x887A0005 and the device removed,
// 3301 at the eye bridge). The game's render memory is a few D3D12 heaps whose textures
// share (alias) the same bytes. When the game makes a new such heap, the eye images it
// placed in the old one may be rebuilt or stop being the active alias. With HEAP GUARD on
// (the default) the eye bridge then:
//   1. lets go of both eye images (released only after its own GPU work that reads them
//      has finished, like a swapped eye: eye_swap::releasable),
//   2. forgets their states (learned again from the game's own next barriers),
//   3. takes the images again from the game's next frame (the same kind only, else the
//      eyes pause with eye_swap::lostCode, never VR's end), and
//   4. copies no pair that began before the new heap (the headset keeps the last one).
// OFF: test 2's behaviour (the heap watch only counts).

// live.txt / VR SETTINGS: HEAP GUARD, value 0 ON (the default), 1 OFF; anything else ON.
inline bool on(uint32_t value) { return value != 1; }

// d3d12.h values (plain numbers, so this is testable off Windows).
inline constexpr uint32_t heapTypeDefault = 1;              // D3D12_HEAP_TYPE_DEFAULT
inline constexpr uint32_t denyRtDsTextures = 0x400;         // D3D12_HEAP_FLAG_DENY_RT_DS_TEXTURES
inline constexpr uint64_t smallestRenderHeap = 1ull << 20;  // smaller heaps cannot hold an eye image
// ID3D12Device's vtable: IUnknown 0-2, ID3D12Object 3-6, then GetNodeCount 7 ... CreateCommittedResource 27,
// CreateHeap 28, CreatePlacedResource 29 (d3d12.h).
inline constexpr unsigned createHeapSlot = 28;

// The bytes an eye image needs (R8G8B8A8, the only kind the bridge takes); 1 MB before one is known.
constexpr uint64_t eyeBytes(uint64_t width, uint64_t height) {
    return width * height * 4 > smallestRenderHeap ? width * height * 4 : smallestRenderHeap;
}
// A new heap that could hold an eye image: on the GPU, render targets allowed, and at least an eye
// image's bytes (texture streaming's small heaps do not count). A buffer-only, upload or readback
// heap is not one.
constexpr bool renderHeap(uint32_t type, uint32_t flags, uint64_t bytes, uint64_t smallest = smallestRenderHeap) {
    return type == heapTypeDefault && !(flags & denyRtDsTextures) && bytes >= smallest &&
           bytes >= smallestRenderHeap;
}

// The eye bridge's view of the heap watch, under its recording lock.
// `epoch` counts the render heaps the game made (the CreateHeap watch, any thread).
struct Guard {
    uint64_t seenEpoch{};        // the last epoch acted on
    uint64_t holdGeneration{};   // pairs up to this generation are not copied
    uint32_t rebuilds{};         // how many times the eyes were let go of
    bool armed{};                // the first epoch seen only sets the start (the game's own load)

    // Whether the eye images must be let go of now: a new render heap since the last check,
    // the guard on and the bridge holding an eye image. `generation`: the newest pair begun.
    bool check(uint64_t epoch, bool guardOn, bool holdingEyes, uint64_t generation) {
        if (!armed) {
            armed = true;
            seenEpoch = epoch;
            return false;
        }
        if (epoch == seenEpoch)
            return false;
        seenEpoch = epoch;
        if (!guardOn || !holdingEyes)
            return false;
        ++rebuilds;
        // The pair in flight may have begun in the old heap: skip it and the next one.
        holdGeneration = generation + 1;
        return true;
    }
    // Whether a pair (by its generation) may be copied to the headset.
    bool mayCopy(uint64_t generation) const { return generation > holdGeneration; }
};

} // namespace spidy::heap_guard
