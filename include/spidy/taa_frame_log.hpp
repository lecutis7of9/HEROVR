#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
//
// 0.0.76 TAA FRAME LOG (read-only, SpidyTaaFrameLogData, tools/taa_frame_log_record.py): SPIDY TAA's history pass,
// eye 0, one entry a pair, the last `capacity` (300) pairs kept: the view the pair was rendered with and the one the
// history was fetched from (each a camera-to-world matrix and its lens tangents, our sub-pixel step already in the
// lens), the step applied to each, where the history was read for the eye's centre pixel (its UV and its shift in
// eye pixels), that shift with the step's change taken out (the drift), the shift's change from the pair before,
// and whether the head was still. Nothing here changes a picture.
#include <cmath>
#include <cstdint>

namespace spidy::taa_frame_log {
inline constexpr uint32_t capacity = 300;
// Worked out from the code (game_xr's EYE AA block, noteEyeViews, drawHistory's historyReprojection) and proved in
// core_tests (0.0.76 TAA JITTER): the history's views are the JITTERED ones on both sides (each pair's lens carries
// its own step before noteEyeViews), so the step is applied ONCE in the history UV, with the + sign (the history
// moves by + (step now - step last): x as is, y flipped for rows going down - taa_watch::jitterShift with times 1).
inline constexpr uint32_t matricesJittered = 1; // 1 jittered, 0 unjittered
inline constexpr uint32_t appliedTimes = 1;     // 0 none, 1 once, 2 twice
inline constexpr int32_t appliedSign = 1;       // +1 or -1
enum Flags : uint32_t { still = 1, history = 2, ours = 4 }; // bits 8-15: taa_jitter::Mode
struct Entry {
    uint32_t serial{};
    uint32_t flags{};
    float turn{}, travel{};          // radians and metres since the pair the history is of
    float jitterNow[2]{};            // our step on this pair's lens (eye pixels, x right, y up)
    float jitterBefore[2]{};         // and on the history's
    float uv[2]{};                   // the history's place for the centre pixel (0-1, from the top left)
    float shift[2]{};                // (uv - 0.5) x the eye's size: eye pixels, x right, y down
    float expected[2]{};             // the step's change counted once (taa_watch::jitterShift, times 1)
    float drift[2]{};                // shift - expected
    float delta[2]{};                // shift - the logged pair before's
    float world[16]{}, tangents[4]{};             // the view this pair was rendered with
    float worldBefore[16]{}, tangentsBefore[4]{}; // the view the history was fetched from
};
static_assert(sizeof(Entry) == 232);
struct Data {
    uint32_t magic = 0x474c4654; // TFLG
    uint32_t version = 1;
    uint32_t bytes = 48 + 232 * capacity;
    uint32_t entryBytes = sizeof(Entry);
    uint32_t cap = capacity;
    uint32_t written{}; // entries ever written (entries[(written - 1) % capacity] the newest)
    uint32_t matrices = matricesJittered;
    uint32_t applied = appliedTimes;
    int32_t sign = appliedSign;
    uint32_t width{}, height{}; // the eye's size the pixels are of
    uint32_t reserved{};
    Entry entries[capacity];
};
static_assert(sizeof(Data) == 48 + 232 * capacity);
// One pair: delta from the logged pair before (none for the first), the drift, then the entry and the count.
inline void note(Data& d, Entry e) {
    if (d.written) {
        const Entry& last = d.entries[(d.written - 1) % capacity];
        for (int k = 0; k < 2; ++k)
            e.delta[k] = (e.flags & history) && (last.flags & history) ? e.shift[k] - last.shift[k] : 0.f;
    }
    for (int k = 0; k < 2; ++k)
        e.drift[k] = (e.flags & history) ? e.shift[k] - e.expected[k] : 0.f;
    d.entries[d.written % capacity] = e;
    ++d.written;
}
} // namespace spidy::taa_frame_log
