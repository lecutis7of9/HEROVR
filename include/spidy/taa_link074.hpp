#pragma once
// 0.0.74 test 3 TAA LINK + JITTER GUARD (owner after test 2b: "buildings shake still, it looks just objectively
// worse"; the headset clips: head held still for 7 s, distant windows, edges and lamps keep shimmering and never
// settle). Made by Agent Mulder.
//
// What decides whether an eye's TAA uses its history (Spider-Man2.exe, CONFIRMED by disassembly):
//  - TemporalAa 0x2c2dc60 runs only while the job's stored jitter (+0x428, +0x42c) is not zero (0x2c2dd09).
//  - It takes the frame F from 0xc2c6c2c (0x2c2dd13), which the job copy fills from 0xc2c9d40 (0x2c81958 via
//    0x2c91fe0), and asks 0x2c98290(job, F) for the history target.
//  - 0x2c98290 gives a history target ONLY when the job's other stamp slot, *[job + 0x1a28 + 8*(1 - [job+0x1a38])],
//    equals F - 1, and the previous buffer set job+0x18e8 is there. The current target (0x2c98240) writes F into
//    *[job + 0x1a28 + 8*[job+0x1a38]]. The view flips +0x1a38 (and sets +0x18e8 = +0x18e0) in 0x2c96f80.
//  - With no history target, the current frame's share is 1 (0x2c2e1ee..0x2c2e218): no accumulation at all.
//    Test 2b's cut/stamp formula (taa_hold074.hpp) is only the second half of that test - it never saw this one.
// So: jitter on, history refused = the jitter itself is what the player sees. That is the shake with a still head.
//
// TAA LINK (read only): per eye job, after the game's copy, the history slot's stamp against the frame.
// JITTER GUARD (ON by default, live): when an eye's history slot has not moved for `staleJobs` jobs while ENGINE
// TAA is on (the history can never be F - 1 then: it is refused every frame), the eyes' jitter drops to a
// near-zero constant (TemporalAa still runs, so the check keeps being made, but nothing shimmers). As soon as
// both eyes' slots move again the full jitter is back. OFF: test 2b's jitter, always.
#include <cstdint>

namespace spidy::taa_link {
inline constexpr uint32_t jobSlots = 0x1a28, jobSlotIndex = 0x1a38, jobPreviousSet = 0x18e8;
inline constexpr uint32_t frameRva = 0xc2c9d40;  // 0x2c91fe0's frame, copied to 0xc2c6c2c for TemporalAa
inline constexpr uint32_t staleJobs = 8;         // jobs with an unmoved history slot before the guard acts
inline constexpr float guardJitter = 1e-6f;      // NDC; keeps TemporalAa running (0x2c2dd09), ~0.001 px
// 0 ON (the default), 1 OFF.
inline bool guardOn(uint32_t value) { return value != 1; }

struct Eye {
    int32_t lastStamp{};
    bool haveLast{};
    uint32_t staleRun{};
};
// One eye job: whether its history slot was there and its stamp. Returns true while the eye's history is refused.
inline bool refused(Eye& e, bool haveSlot, int32_t stamp) {
    if (!haveSlot) {
        e.staleRun = staleJobs;
        e.haveLast = false;
        return true;
    }
    e.staleRun = e.haveLast && stamp == e.lastStamp ? e.staleRun + 1 : 0;
    e.lastStamp = stamp;
    e.haveLast = true;
    return e.staleRun >= staleJobs;
}
// How far the frame is past the history stamp: 0, 1 (linked, as the game wants it at its own check), 2, 3+.
inline unsigned bucket(int32_t frame, int32_t stamp) {
    const int64_t d = static_cast<int64_t>(frame) - stamp;
    return d <= 0 ? 0u : d >= 3 ? 3u : static_cast<unsigned>(d);
}
inline constexpr uint32_t magic = 0x4b4e4c54; // TLNK
inline constexpr uint32_t version = 1;
struct Data {
    uint32_t magic = taa_link::magic, version = taa_link::version, bytes = 160, guardOn{};
    uint64_t jobs[2]{};        // eye jobs seen while ENGINE TAA was live
    uint64_t noSlot[2]{};      // jobs with no history slot or no previous set (always refused)
    uint64_t stale[2]{};       // jobs whose history slot had not moved for staleJobs jobs (refused)
    uint64_t ages[2][4]{};     // frame - history stamp at the copy: 0, 1, 2, 3+
    uint64_t guardedLenses{};  // lenses given the guard's near-zero jitter
    int32_t lastIndex[2]{}, lastAge[2]{};
    uint64_t sequence{};
};
static_assert(sizeof(Data) == 160);
} // namespace spidy::taa_link
