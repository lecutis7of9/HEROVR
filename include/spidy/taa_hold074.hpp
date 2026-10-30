#pragma once
// 0.0.74 test 2b TAA HOLD (owner, relayed: "that TAA setting still shakes so bad, then off it's fuzzy or too
// sharp"; the SpidyVR creator: his first stereo build ran the AA pass on the monitor image only).
//
// What TemporalAa 0x2c2dc60 does (exe, CONFIRMED code): it reads its view from the render context's +0x650
// (0xc2c6af0 = 0xc2c64a0 + 0x650: the JOB copyJob makes, settings +0x650), and the frame from the context's
// +0x790 (0xc2c6c30). The current frame's share of the result is
//     max(1 / (frame - job[+0x19ac] + 1), 1 / (frame - job[+0x1de8] + 1)), or 1 on the cut flag job[+0x19ab]
// (0x2c2e1b0..0x2c2e218). Share 1 = the history is not used at all: only the jittered frame shows, and the
// sub-pixel jitter reads as shake.
// Test 2's report (CANDIDATE: read from the eye VIEW every 250 ms, not from the job the handler reads): share
// 1.0 at every readback (1451 reset samples; cut age 0 and stamp age 0 in each eye).
//
// The hold, ON by default and live: per eye job, after the game's copy, when the game did NOT flag a cut and
// our eye did not jump, but both ages have read 0 on two jobs in a row (the stamps follow the frame, which a
// real cut never does twice), the job's two stamps are set holdAge frames back, so the share drops to
// 1/(holdAge+1) and the game's own blend limits take over. Only the job copy is written (the view, and the
// game's 64-frame staleness test on it, are untouched). A real cut (the flag, or a jump past jumpMetres) is
// always left to the game. OFF writes nothing.
#include <cstdint>

namespace spidy::taa_hold {
inline constexpr uint32_t settingsJob = 0x650, settingsFrame = 0x790; // context copy: 0xc2c6af0, 0xc2c6c30
inline constexpr uint32_t jobCut = 0x19ab, jobCutFrame = 0x19ac, jobStamp = 0x1de8;
inline constexpr int32_t holdAge = 16;      // < 64, the game's own staleness limit (0x2c9bf89)
inline constexpr float jumpMetres = 1.0f;   // an eye moving this far in one frame is a real cut
inline constexpr uint32_t stuckRun = 2;     // jobs in a row with both ages 0 before we hold

// Both stamps exactly on the frame: the history is about to be thrown away. Exact, so a frame number read from
// the wrong place (the context +0x790 is CANDIDATE) can only ever mean "no hold".
inline bool stuck(int32_t frame, int32_t cutFrame, int32_t stamp) {
    return frame != 0 && cutFrame == frame && stamp == frame;
}
struct Eye {
    uint32_t zeroRun{}; // jobs in a row with both ages exactly 0
};
struct Plan {
    bool hold{};
    int32_t cutFrame{}, stamp{};
};
// One eye job: cut flag, the job's frame, its two stamps, how far the eye moved since its previous frame.
inline Plan plan(Eye& eye, bool on, uint8_t cut, int32_t frame, int32_t cutFrame, int32_t stamp, float moved) {
    Plan p{false, cutFrame, stamp};
    const bool zero = stuck(frame, cutFrame, stamp);
    eye.zeroRun = zero ? eye.zeroRun + 1 : 0;
    if (!on || cut || !(moved < jumpMetres) || eye.zeroRun < stuckRun)
        return p;
    p.hold = true;
    p.cutFrame = frame - holdAge;
    p.stamp = frame - holdAge;
    return p;
}
// The share TemporalAa gives the current frame (same formula as engine_taa::currentWeight).
inline float share(uint8_t cut, int32_t cutAge, int32_t stampAge) {
    if (cut)
        return 1;
    const float a = 1.f / static_cast<float>((cutAge > 0 ? cutAge : 0) + 1);
    const float b = 1.f / static_cast<float>((stampAge > 0 ? stampAge : 0) + 1);
    return a > b ? a : b;
}
// What the report reads (SpidyTaaHoldData, tools/run_game_vr.py TAA_HOLD_FORMAT).
inline constexpr uint32_t magic = 0x444c4854; // THLD
inline constexpr uint32_t version = 1;
struct Data {
    uint32_t magic = taa_hold::magic, version = taa_hold::version, bytes = 120, on{};
    uint64_t jobs[2]{};       // eye jobs seen while ENGINE TAA was live
    uint64_t held[2]{};       // jobs whose stamps we set back
    uint64_t zeroAges[2]{};   // jobs whose ages both read 0 from the game (the CANDIDATE, on the job itself)
    uint64_t cuts[2]{};       // jobs the game flagged as a cut (left alone)
    int32_t lastFrame[2]{}, lastCutAge[2]{}, lastStampAge[2]{}; // as the game left them, newest job
    uint32_t reserved{};
    uint64_t sequence{};
};
static_assert(sizeof(Data) == 120);
} // namespace spidy::taa_hold
