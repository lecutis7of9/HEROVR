#pragma once
// Test build 10 MIRROR VIEW: the suit mirror's frame budget (game_xr's MIRROR, vr_mirror.hpp).
// The mirror redraws the worn suit's whole reflection every frame. When that costs too much for
// 90 Hz the reflection is drawn every other frame (45 Hz, the last one reused); when even that
// stays too slow for a few seconds the mirror turns itself OFF and the wrist says why. Never a crash.
#include <cmath>
#include <cstdint>

namespace spidy::mirror_guard {
constexpr float halfRateMs = 1.5f; // the mirror's mean cost per frame above this: the reflection at 45 Hz
constexpr float fullRateMs = .6f;  // back under this (at 45 Hz): every frame again
constexpr float offMs = 3.5f;      // still above this at 45 Hz ...
constexpr float offSeconds = 3.f;  // ... for this long: OFF, with the wrist note
constexpr double noteSeconds = 20; // the wrist note's time

struct Guard {
    float meanMs = 0;   // the cost per frame, smoothed (a lone hitch moves it little)
    float slowFor = 0;  // seconds spent above offMs at 45 Hz
    bool half = false;  // the reflection at 45 Hz
    uint32_t frame = 0; // frames since ON
};
// This frame may reuse the last reflection (45 Hz and an odd frame).
inline bool reuse(const Guard& g) { return g.half && (g.frame & 1u); }
// After the frame's mirror work (ms, its length dt in seconds): true = turn the mirror OFF now.
inline bool step(Guard& g, float ms, float dt) {
    if (!std::isfinite(ms) || ms < 0)
        return false;
    dt = std::isfinite(dt) ? (dt < 0 ? 0 : dt > .1f ? .1f : dt) : 0;
    ++g.frame;
    g.meanMs = g.frame <= 1 ? ms : g.meanMs * .95f + ms * .05f;
    if (g.frame < 30) // the first frames (models paging in) are not judged
        return false;
    if (!g.half && g.meanMs > halfRateMs)
        g.half = true, g.slowFor = 0;
    else if (g.half && g.meanMs < fullRateMs)
        g.half = false, g.slowFor = 0;
    g.slowFor = g.half && g.meanMs > offMs ? g.slowFor + dt : 0;
    return g.slowFor >= offSeconds;
}
} // namespace spidy::mirror_guard
