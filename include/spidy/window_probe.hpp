#pragma once
// 0.0.74 test 10 WINDOW PROBE (read-only telemetry, logging only, always on, no switch): the night windows
// whose light pops on/off near the edge of an eye's view. Built only on two hooks the mod already installs
// (stereo_probe.cpp): the impostor visibility queries (2a6bc60, CONFIRMED: called per view from the scene
// submit 2d01b30 at 2d01cc0 with the view and its cull object) and isActiveView (2c991a0, CONFIRMED). No new
// game address and no write to the game: per view (the game's, the left eye's, the right eye's) it counts the
// queries and, a few times a second, keeps a sample of what that view culled with:
//  - the view's pose (+0: right, down, ahead rows and position, CONFIRMED) and its lens tangents (+400 left, +404 right, +408 top, +40c bottom: CONFIRMED, native_view.hpp), its
//    near / far (+3f8 / +3fc, CONFIRMED) and its flags (+438: bit 2 makes the cull build 2c775b0 use the lens
//    tangents, CONFIRMED);
//  - the two matrices the cull build hands the frustum maker 30f4f70 otherwise (+2e0 and +280, CONFIRMED as its
//    inputs; which is the projection is the report's to work out, tools/window_probe.py: CANDIDATE);
//  - the first 128 bytes of the cull object 30f4f70 fills (the frustum's planes as stored: CANDIDATE layout);
//  - the impostor queries' result pointer (non-null counted) and its +88 full-model distance (read only; the
//    field the impostor jobs read, CANDIDATE as in FAR WINDOWS);
//  - the game's own isActiveView answer for the view, and the time (QueryPerformanceCounter seconds).
// The impostor and full-model INSTANCE counts are not read: no offset for them is confirmed (left out).
// tools/run_game_vr.py appends each new sample to reports/window-probe-<stamp>.jsonl beside the session's
// game-vr report. Fail-safe: every game read is guarded (a fault skips the sample), nothing is written to the
// game, and the record is a seqlock the reader retries.
#include <cstdint>

namespace spidy::window_probe {
inline constexpr uint32_t views = 3;          // 0 the game's view, 1 the left eye, 2 the right eye
inline constexpr uint32_t ringSize = 96;      // samples kept (the reader polls about once a second)
inline constexpr double sampleSeconds = .1;   // at most one sample per view this often
inline constexpr uint32_t cullFloats = 32;    // 128 bytes of the cull object
// One sample of one view's impostor query.
struct Sample {
    double seconds{};      // QueryPerformanceCounter seconds
    uint64_t frame{};      // the mod's frame index when it was taken
    uint32_t view{};       // 0 game, 1 left, 2 right
    uint32_t flags{};      // bit 0 the result was non-null, bit 1 the game called it the active view,
                           // bit 2 the view's +438 flag 2 (lens tangents path), bit 3 the reads all held
    float lens[4]{};       // +400 left, +404 right, +408 top, +40c bottom tangents
    float nearFar[2]{};    // +3f8, +3fc
    float reach{};         // the result's +88 (0: unread)
    uint32_t viewFlags{};  // +438 as read
    float pose[16]{};      // view +0: right, down, ahead rows and position (CONFIRMED, native_view.hpp)
    float matrixA[16]{};   // view +2e0
    float matrixB[16]{};   // view +280
    float cull[cullFloats]{};
};
static_assert(sizeof(Sample) == 376);
// The record (SpidyWindowProbeData); `sequence` odd while it changes.
struct Data {
    uint32_t magic = 0x42525057, version = 1, bytes = 36192, count{}; // WPRB; count: samples ever written
    uint64_t calls[views]{};    // impostor queries seen per view
    uint64_t results[views]{};  // of them, with a non-null result
    uint64_t other{};           // queries of other views (offscreen, shadows): counted only
    uint64_t faults{};          // samples skipped because a read failed
    double started{};           // QueryPerformanceCounter seconds at the first query
    Sample ring[ringSize]{};
    uint64_t sequence{};
};
static_assert(sizeof(Data) == 36192);
// Whether a view's next sample is due (never on NaN, always the first).
inline bool due(double last, double now) {
    if (!(now == now) || !(last == last))
        return false;
    return last <= 0 || now - last >= sampleSeconds || now < last;
}
} // namespace spidy::window_probe
