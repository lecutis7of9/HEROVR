#pragma once
// 0.0.74 test 4 AA fixes 1-3 (owner: "still looks like shit in the game with aliasing"). Made by Agent Mulder.
// Three switches, all ON by default, each switchable off in live.txt / the wrist menu:
//  1. TAA GATE LOG (taa_gate_log, live): MILES TAA's per-eye TemporalAa log with each eye's last 4 jobs (test 3
//     kept only the newest, so a TemporalAa that ran for the eye's previous job counted as no run: the 54% / 13%
//     of test3-r3 may be that), the calls that matched no eye job (the main view's and others'), and what the
//     history weight is made from: the job's cut flag +0x19ab, its cut frame +0x19ac, its stamp +0x1de8 and the
//     frame counter 0xc2c6c30. Console lines kept under 256 characters.
//  2. EYE TAA REQUEUE (eye_taa_requeue, live): when an eye's jobs stop getting TemporalAa (2 jobs in a row with no
//     call, by the ring above), TemporalAa 0xb1 is queued once more on that eye view's +0x18d0 (Enqueue 0x2c77810),
//     once per job, inside an exception guard. 8 requeues in a row that still bring no call: off for the session.
//  3. MAIN VIEW SMALL (main_view_small, restart): the game's own (flat-screen) view rendered at 50% per side, its
//     TAA resolving it to the window, so the GPU has more time for the eyes (46 fps under 90 Hz in test 3). Only
//     while the game's Anti-Aliasing makes TAA targets. Never view flag 32 (the game's own view not rendered: SM2
//     loses its device without it); the view is still rendered, only smaller, and the monitor shows the left eye.
//
// Addresses (Spider-Man2.exe 2.810.0.0):
//  CONFIRMED (disassembly, 0.0.74 test 3 MILES TAA / test 2b sessions): TemporalAa 0x2c2dc60, job global 0xc2c6af0,
//    F 0xc2c6c2c, frame 0xc2c6c30, Enqueue 0x2c77810 (0xb1 on view +0x18d0, as 0x2d017b3), main view pointer
//    0xc2ca220, a view's buffer set +0x1e38, the set's sizes +0x1b978, job cut +0x19ab, cut frame +0x19ac,
//    stamp +0x1de8.
//  CANDIDATE (not proven on the running game): that the eyes' missing TemporalAa runs are a matching artefact or a
//    dropped queue entry (this log tells which); that a second 0xb1 entry brings the run back (the requeue counts
//    it); that the main view's TAA resolves a 50% scene to the window as the eyes' does at 67% (MILES TAA).
#include <array>
#include <cstdint>

namespace spidy::taa_gate {
// 0 ON (the default), 1 OFF, for all three.
inline bool on(uint32_t value) { return value != 1; }

inline constexpr uint32_t jobCut = 0x19ab, jobCutFrame = 0x19ac, jobStamp = 0x1de8; // CONFIRMED
inline constexpr uint32_t frameGlobal2 = 0xc2c6c30;                                 // CONFIRMED
inline constexpr uint32_t mainViewGlobal = 0xc2ca220, viewBuffers = 0x1e38, setSizes = 0x1b978; // CONFIRMED
inline constexpr uint32_t enqueueRva = 0x2c77810, viewTaaList = 0x18d0, temporalAaCommand = 0xb1; // CONFIRMED

inline constexpr unsigned ringSize = 4;
// Each eye's last jobs, newest first. find: 0 the newest, 1..3 older, -1 none.
struct JobRing {
    std::array<uintptr_t, ringSize> jobs{};
    void push(uintptr_t job) {
        if (!job || jobs[0] == job)
            return;
        for (unsigned i = ringSize - 1; i; --i)
            jobs[i] = jobs[i - 1];
        jobs[0] = job;
    }
    int find(uintptr_t job) const {
        for (unsigned i = 0; job && i < ringSize; ++i)
            if (jobs[i] == job)
                return static_cast<int>(i);
        return -1;
    }
};

inline constexpr uint32_t stallJobs = 2;     // jobs in a row with no TemporalAa call before a requeue
inline constexpr uint32_t requeueFailMax = 8; // requeues in a row with no call: off for the session
// The requeue's state for one eye. newJob at each of the eye's jobs (copyJob) with the eye's TemporalAa calls so
// far; want(job) in the eye's display setup; queued(job) after a requeue.
struct Requeue {
    uint64_t callsAtJob{};
    uint32_t missed{}, fails{};
    uintptr_t lastQueued{};
    bool pending{}, tripped{};
    // True once, when it trips.
    bool newJob(uint64_t calls) {
        const bool none = calls == callsAtJob;
        callsAtJob = calls;
        missed = none ? missed + 1 : 0;
        if (pending) {
            pending = false;
            fails = none ? fails + 1 : 0;
            if (fails >= requeueFailMax && !tripped) {
                tripped = true;
                return true;
            }
        }
        return false;
    }
    bool want(uintptr_t job, bool active) const {
        return active && !tripped && job && missed >= stallJobs && job != lastQueued;
    }
    void queued(uintptr_t job) {
        lastQueued = job;
        pending = true;
    }
};

inline constexpr uint32_t mainPercent = 50;
// SpidyXrStart's options bit (tools/run_game_vr.py MAIN_VIEW_SMALL_OPTION) and SpidyEyeTemporal's (value >> 8) bit.
inline constexpr uint32_t mainStartOption = 0x2000000, mainEyeTemporalBit = 0x40;
// The main view's scene size for its output `width` x `height`: mainPercent per side, in multiples of 8 (at least 8).
inline std::array<uint32_t, 2> mainSize(uint32_t width, uint32_t height) {
    auto scaled = [](uint32_t v) {
        const uint32_t s = (v * mainPercent / 100 + 7) & ~7u;
        return s < 8 ? (v < 8 ? v : 8u) : (s > v ? v : s);
    };
    return {scaled(width), scaled(height)};
}
// Whether the main view's buffers are made smaller: on at the VR start, the game's AA makes TAA targets, a sane size.
inline bool shrinkMain(bool onAtStart, bool aaMakesTargets, uint32_t width, uint32_t height) {
    return onAtStart && aaMakesTargets && width >= 64 && height >= 64 && width <= 16384 && height <= 16384;
}
// The size flags (+0x1b9b4) after a resize: bit 2 (render == output) kept true to the sizes; every other bit as
// the game left it.
inline uint32_t sizeFlags(uint32_t flags, bool sameSize) { return sameSize ? (flags | 2u) : (flags & ~2u); }

enum Reason : uint32_t {
    none,
    requeueFailed, // requeueFailMax requeues in a row brought no TemporalAa run
    requeueFault,  // the game's Enqueue faulted (caught)
    mainGameAa,    // the game's AA makes no TAA targets: the main view kept its size
};
inline const char* reasonText(uint32_t r) {
    switch (r) {
    case requeueFailed: return "EYE TAA REQUEUE: 8 requeues brought no TAA run; off for this session.";
    case requeueFault: return "EYE TAA REQUEUE: the game's queue refused it; off for this session.";
    case mainGameAa: return "MAIN VIEW SMALL: the game's Anti-Aliasing is not TAA; the main view kept its size.";
    default: return "";
    }
}

inline constexpr uint32_t magic = 0x54414754; // TGAT
inline constexpr uint32_t version = 1;
// SpidyTaaGateData (stereo_probe.cpp).
struct Data {
    uint32_t magic = taa_gate::magic, version = taa_gate::version, bytes = 256, flags{}; // flags: Flag bits
    uint32_t reason{}, mainPercent = 100, mainWidth{}, mainHeight{};
    uint64_t unmatched{};       // TemporalAa runs for no eye job (the main view's, other views')
    uint64_t matchedOld[2]{};   // eye runs that matched an older job of the ring (test 3 counted these as none)
    uint64_t requeued[2]{};     // EYE TAA REQUEUE entries queued
    uint64_t requeueFailed[2]{};// ... that brought no run
    uint64_t mainResized{};     // main view buffer sizings made small
    uint32_t cut[2]{};          // job +0x19ab at the eye's last run
    int32_t cutFrame[2]{}, jobStamp[2]{}, frame2[2]{}; // job +0x19ac, +0x1de8, [0xc2c6c30] at it
    int32_t mainCut{}, mainCutFrame{}, mainStamp{}, mainFrame2{}; // the same for the last unmatched run
    uint64_t eyeJobs[2]{};      // eye jobs seen (copyJob)
    uint64_t missedJobs[2]{};   // eye jobs that got no TemporalAa call
    uint64_t sequence{};
    uint64_t reserved[9]{};
};
enum Flag : uint32_t { flagLog = 1, flagRequeue = 2, flagRequeueOff = 4, flagMainAtStart = 8, flagMainSmall = 16 };
static_assert(sizeof(Data) == 256, "SpidyTaaGateData is 256 bytes");

inline constexpr unsigned lineMax = 256; // every console line of the log is shorter than this
} // namespace spidy::taa_gate
