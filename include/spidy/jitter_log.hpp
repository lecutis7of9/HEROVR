#pragma once
// 0.0.76 JITTER FIX (owner): what each view's lens was given and kept for jitter, for the first 200 frames.
//
// The native lens 0x2c75560 (CONFIRMED, docs/AA-0.0.73.md) stores +0x428 = 2 * its jitter x and
// +0x42c = -2 * its jitter y. Until 0.0.76 JITTER FIX the game's head view (stereo_probe activeView) and the
// aligned head view (placeEyes onHead) passed the game camera's already-stored +0x428/+0x42c back in, so those
// views carried twice the game's jitter with y flipped. SpidyJitterLogData records, per view and per lens call
// ("pass"), the source camera's stored pair, the pair passed in and the pair the new view stored, so a run's
// report shows the old doubling is gone (head views) and the eyes are unchanged. Read-only; bounded: each view
// logs its first 200 frames, at most `capacity` entries in all; a full log costs one relaxed load per call.
#include <atomic>
#include <cstdint>

namespace spidy::jitter_log {
inline constexpr uint32_t magic = 0x474f4c4a; // JLOG
inline constexpr uint32_t version = 1;
inline constexpr uint32_t frames = 200, capacity = 2048, views = 4;
enum View : uint32_t {
    headView = 0,    // stereo_probe activeView: the game's own view placed on the head (stock camera's lens)
    alignedHead = 1, // placeEyes onHead: the head view the eyes' shadows and the game view are fitted to
    eyeLeft = 2,     // the headset eyes' descriptors (eyeJitter, raw values; unchanged)
    eyeRight = 3,
};
struct Entry {
    uint32_t frame{}, view{}; // view: View | 0x100 for the game screen in stereo (stock jitter, unchanged)
    float stored[2]{};        // the source camera's +0x428/+0x42c before the call
    float passed[2]{};        // the jitter arguments the lens was given
    float made[2]{};          // the new view's +0x428/+0x42c after the call
};
static_assert(sizeof(Entry) == 32);
struct Data {
    uint32_t magic = jitter_log::magic, version = jitter_log::version, bytes = sizeof(Data), entrySize = sizeof(Entry);
    uint32_t count{}, dropped{}, firstFrame[views]{}, started{}, reserved{};
    Entry entries[capacity]{};
};
static_assert(sizeof(Data) == 48 + 32 * capacity, "SpidyJitterLogData layout (tools/jitter_log_record.py)");

// Records one lens call of `view` at game frame `frame` while that view is in its first 200 frames.
inline void record(Data& data, std::atomic<uint32_t>& reserved, uint32_t view, uint64_t frame, const float stored[2],
                   float passedX, float passedY, const float made[2]) {
    const uint32_t base = view & 0xff;
    if (base >= views || reserved.load(std::memory_order_relaxed) >= capacity)
        return;
    const uint32_t f = static_cast<uint32_t>(frame);
    if (!(data.started & (1u << base))) {
        data.firstFrame[base] = f;
        data.started |= 1u << base;
    }
    if (f - data.firstFrame[base] >= frames)
        return;
    const uint32_t at = reserved.fetch_add(1, std::memory_order_relaxed);
    if (at >= capacity) {
        ++data.dropped;
        return;
    }
    auto& e = data.entries[at];
    e.frame = f;
    e.view = view;
    e.stored[0] = stored[0];
    e.stored[1] = stored[1];
    e.passed[0] = passedX;
    e.passed[1] = passedY;
    e.made[0] = made[0];
    e.made[1] = made[1];
    std::atomic_thread_fence(std::memory_order_release);
    data.count = at + 1 > data.count ? at + 1 : data.count;
}
} // namespace spidy::jitter_log
