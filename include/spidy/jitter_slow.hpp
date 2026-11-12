#pragma once
// 79.7 SLOW-WALK JITTER and its per-level counters (the report's 'jitter_levels').
//
// 79.6's report: the game's TemporalAa job read the lens value of the PREVIOUS frame for the left eye 40% of the time
// and the right eye's 76%, so over the 8-phase Halton walk (neighbouring phases up to a whole stored pixel apart) a lagging
// job saw an offset up to ~1 px away from what the picture was rendered with, and by a different amount in each eye:
// far edges shimmered, and more the more jitter there was ("the higher it went the worse it got").
//
// The slow walk is a small circle in 32 steps a turn, the SAME phase for both eyes (one value per frame, as before):
// two neighbouring phases differ by 2 pi / 32 = 0.196 of the radius, so a job one frame behind is off by at most 0.2 of
// the stored amplitude (0.02 / 0.04 / 0.09 px for LOW / MED / FULL), not up to 2x. The amplitude is the level's own
// (jitter_check::maxStoredPixels). OFF stays the fixed offset.
#include <cmath>
#include <cstdint>

namespace spidy::jitter_slow {
inline constexpr uint32_t steps = 32, lagBuckets = 4, levels = 5;
inline constexpr float turn = 6.28318531f;
// The step of frame `frame`: its offset in stored eye pixels (x, y) on a circle of `amplitudePx`.
inline void walk(uint64_t frame, float amplitudePx, float& x, float& y) {
    const float angle = turn * static_cast<float>(frame % steps) / static_cast<float>(steps);
    x = amplitudePx * std::cos(angle);
    y = amplitudePx * std::sin(angle);
}
// The most a job one frame behind can be off by, as a share of the amplitude.
inline constexpr float lagShare() { return 0.19603428f; } // 2 sin(pi / 32): the chord of one step on the unit circle
inline constexpr uint32_t magic = 0x4c4a4a37; // 7JJL
struct Level {
    uint64_t runs[2]{};                 // TemporalAa runs of each eye's job at this level
    uint64_t lag[2][lagBuckets]{};      // runs whose job matched the lens N frames back: 0, 1, 2, 3 or more
    uint64_t unmatched[2]{};            // runs matching no lens value in the last 16 frames
    uint64_t lrCompared{}, lrDiffers{}; // runs compared to the other eye's last job, and those that differed
    float lrDiffMax{}, lrDiffSum{};     // the largest and the summed component difference (NDC)
};
struct Data {
    uint32_t magic = jitter_slow::magic, version = 1, bytes = 624, level{};
    uint64_t frames{}; // lens calls counted
    Level levels[jitter_slow::levels]{};
};
static_assert(sizeof(Level) == 120 && sizeof(Data) == 624, "SpidyJitterLevelData (tools/hero797_record.py)");
inline uint32_t bucket(uint32_t lag) { return lag < lagBuckets ? lag : lagBuckets - 1; }
} // namespace spidy::jitter_slow
