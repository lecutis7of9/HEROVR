#pragma once
// 0.0.76 HITCH HOLD (owner, TEST c71a59c: "completely unplayable from lag"; run 060032).
//
// Virtual Desktop held xrEndFrame for a tenth of a second 108 times in that run (end_frame 100.1 / 200.8 /
// 409.8 ms; 0 times in runs 001842 and 005811), so the next frame's display time came 0.10-0.41 s after the
// last. A frame over 0.1 s made the view rig inactive (game_tracking badFrameTime), the gate said "tracking",
// VR play dropped to the game screen for 0.2-2 s (48 presentation changes in 109 s against 12-18 before) and
// the swing let go of its webs (Swing::update). One held frame became a freeze of up to two seconds.
//
// step(): with the hold on (the default), a frame of more than 0.1 s and at most longestHitch is played as a
// 0.1 s step - the longest the rig, the swing and the body already take - so VR play, the webs and the eyes go
// on through a runtime hitch. Longer gaps (a real pause, the headset off) are left as they are. Off: as before.
#include <cmath>
#include <cstdint>

namespace spidy::hitch_hold {
inline constexpr float longestStep = .1f;  // the longest frame the rig and the swing play (game_tracking, swing)
inline constexpr float longestHitch = .5f; // longer than this is not a hitch: left to the old rules
struct Counts {
    uint64_t held{};      // frames played as a 0.1 s step
    float longestMs{};    // the longest of them, in ms
};
// The frame time to play: `seconds` itself, or longestStep for a runtime hitch while `on`.
inline float step(float seconds, bool on, Counts* counts = nullptr) {
    if (!on || !std::isfinite(seconds) || seconds <= longestStep || seconds > longestHitch)
        return seconds;
    if (counts) {
        ++counts->held;
        if (seconds * 1000.f > counts->longestMs)
            counts->longestMs = seconds * 1000.f;
    }
    return longestStep;
}
} // namespace spidy::hitch_hold
