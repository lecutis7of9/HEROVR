#pragma once
// 0.0.74 test 3 MILES TAA (live.txt miles_taa, ON by default; owner: "Can you just add in the possible aliasing fix
// you learned looking at miles"). Made by Agent Mulder.
//
// MilesVR / SpidyVR 0.4.1's per-eye TAA, four steps, ported to Spider-Man2.exe 2.810.0.0:
//  1. A jittered eye lens: Halton (2, 3), the same point for both eyes, 2 * (h - 0.5) / render size,
//     clamp(ceil(8 * ratio^2), 8, 32) phases (ratio = eye width / render width), phase = frame % phases + 1.
//  2. The eye's buffer set made with the TAA targets, at the eye's size (the mask table's TAA kind; engine_taa.hpp).
//  3. The eye job's AA forced to TAA: settings +0x570 = 1 per eye job (copied to 0xc2c6a10, TemporalAa's check).
//  4. TemporalAa (0xb1) queued on view +0x18d0 where the profile's +0xcc8 bit 0 makes the game leave it out.
//  And, as MilesVR does with its eye percent: the eye's scene rendered at `upscalePercent` per side, the game's own
//  TemporalAa resolving it to the eye's size (CANDIDATE: see upscaleOk; checked on the running game, never assumed).
//
// Spider-Man2.exe addresses (CONFIRMED by disassembly of the 2.810.0.0 exe, 0.0.74 test 3 MILES TAA session):
//  - 0x2c2dc60 TemporalAa: no arguments read; [0xc2c6a10] == 1 (0x2c2dccb) else out; job = [0xc2c6af0]
//    (0x2c2dcd8); out unless the job's jitter +0x428/+0x42c is not zero (0x2c2dd09); F = [0xc2c6c2c] (0x2c2dd13);
//    current target 0x2c98240(job, F), history 0x2c98290(job, F); input size = set +0x3800/+0x3804 (set = job
//    +0x18e0; 0 = the current target's), output size = the current target's +0x1d0/+0x1d4, both turned into
//    shader constants separately (it can take a smaller input than its output).
//  - 0x2c98240: writes F to *[job + 0x1a28 + 8 * [job + 0x1a38]] (when F != 0); the target is set +0x12478
//    (+0x12848 when [0xc2c7d94] != 0), there only while set +0x12648 (+0x12a18) != 0 (its +0x1d0 width).
//  - 0x2c98290: history only when *[job + 0x1a28 + 8 * (1 - [job + 0x1a38])] == F - 1 and job +0x18e8 is set.
//  - 0x2d017b3 (SetupDisplay 0x2d016d0, rsi = profile r8, rdi = view): test byte [profile + 0xcc8], 1; jne;
//    Enqueue 0x2c77810(view + 0x18d0, 0xb1, 1, 0).
//  - Mask table 0x5d65408 = {7ffff, 7ffff, 3fcc8 x6} in the exe file.
// Every one is still checked against these bytes when the mod starts (rvaBytes below); one that differs and MILES
// TAA is never used that session: SPIDYVR STOCK's path, and one console line.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace spidy::miles_taa {
// 0 ON (the default), 1 OFF.
inline bool on(uint32_t value) { return value != 1; }

inline constexpr uint32_t temporalAaRva = 0x2c2dc60, currentTargetRva = 0x2c98240, historyTargetRva = 0x2c98290;
inline constexpr uint32_t queueSiteRva = 0x2d017b3;
inline constexpr uint32_t jobAaGlobal = 0xc2c6a10, jobGlobal = 0xc2c6af0, frameGlobal = 0xc2c6c2c;
inline constexpr uint32_t jobSlots = 0x1a28, jobSlotIndex = 0x1a38, jobSet = 0x18e0, jobPrevious = 0x18e8,
                          jobCut = 0x19ab;
inline constexpr uint32_t setInputSize = 0x3800, setTargetWidth = 0x12648, setTargetWidthAlt = 0x12a18;
inline constexpr uint32_t targetPickGlobal = 0xc2c7d94; // 0x2c9825f: which of the set's two TAA targets

inline constexpr unsigned char temporalAaBytes[] = {
    0x48, 0x8b, 0xc4, 0x48, 0x89, 0x58, 0x08, 0x55, 0x56, 0x57, 0x41, 0x54, 0x41, 0x55, 0x41, 0x56,
    0x41, 0x57, 0x48, 0x8d, 0xa8, 0xe8, 0xfe, 0xff, 0xff, 0x48, 0x81, 0xec, 0xe0, 0x01, 0x00, 0x00};
// 0x2c2dccb: cmp dword [0xc2c6a10], 1; jne; mov r14, [0xc2c6af0]
inline constexpr uint32_t temporalAaCheckRva = 0x2c2dccb;
inline constexpr unsigned char temporalAaCheckBytes[] = {0x83, 0x3d, 0x3e, 0x8d, 0x69, 0x09, 0x01, 0x0f, 0x85, 0x06,
                                                         0x0b, 0x00, 0x00, 0x4c, 0x8b, 0x35, 0x11, 0x8e, 0x69, 0x09};
inline constexpr unsigned char currentTargetBytes[] = {0x85, 0xd2, 0x74, 0x12, 0x48, 0x63, 0x81, 0x38, 0x1a, 0x00,
                                                       0x00, 0x4c, 0x8b, 0x84, 0xc1, 0x28, 0x1a, 0x00, 0x00, 0x41,
                                                       0x89, 0x10, 0x48, 0x8b, 0x91, 0xe0, 0x18, 0x00, 0x00};
inline constexpr unsigned char historyTargetBytes[] = {0x85, 0xd2, 0x74, 0x22, 0xb8, 0x01, 0x00, 0x00, 0x00, 0x2b,
                                                       0x81, 0x38, 0x1a, 0x00, 0x00, 0x48, 0x98, 0x4c, 0x8b, 0x84,
                                                       0xc1, 0x28, 0x1a, 0x00, 0x00, 0x8d, 0x42, 0xff, 0x41, 0x39,
                                                       0x00};
inline constexpr unsigned char queueSiteBytes[] = {0xf6, 0x86, 0xc8, 0x0c, 0x00, 0x00, 0x01, 0x75, 0x3e, 0x45,
                                                   0x33, 0xc9, 0x48, 0x8d, 0x8f, 0xd0, 0x18, 0x00, 0x00, 0xba,
                                                   0xb1, 0x00, 0x00, 0x00};

// The eye's scene size, percent per side, when the eyes are made with MILES TAA (MilesVR's eye percent).
inline constexpr uint32_t upscalePercent = 67;
// Eye jobs the game made in a row that never reached the headset as a pair before MILES TAA turns off.
inline constexpr uint32_t stallPairs = 45;
// SpidyXrStart's options bit (tools/run_game_vr.py MILES_TAA_OPTION) and SpidyEyeTemporal's (value >> 8) bit.
inline constexpr uint32_t startOption = 0x1000000, eyeTemporalBit = 0x20;

// MilesVR's eyeJitter: its phase count for a scene `renderWidth` wide in an eye `eyeWidth` wide.
inline uint32_t phases(uint32_t eyeWidth, uint32_t renderWidth) {
    const float ratio = renderWidth ? static_cast<float>(eyeWidth) / renderWidth : 1.f;
    return std::clamp<uint32_t>(static_cast<uint32_t>(std::ceil(8 * ratio * ratio)), 8, 32);
}
inline float halton(uint32_t index, uint32_t radix) {
    float value = 0, fraction = 1.f / radix;
    for (; index; index /= radix, fraction /= radix)
        value += fraction * (index % radix);
    return value;
}
// The lens jitter (NDC) for frame `frame`, the same for both eyes.
inline std::array<float, 2> jitter(uint64_t frame, uint32_t eyeWidth, uint32_t renderWidth, uint32_t renderHeight) {
    const uint32_t phase = static_cast<uint32_t>(frame % phases(eyeWidth, renderWidth)) + 1;
    return {2 * (halton(phase, 2) - .5f) / std::max(1u, renderWidth),
            2 * (halton(phase, 3) - .5f) / std::max(1u, renderHeight)};
}
// The percent the eyes are made at: the upscale only when MILES TAA was on at the start, every exe check passed, the
// mask table is the exe's (the TAA targets will be made) and the game's AA makes them.
inline uint32_t madePercent(bool onAtStart, bool exeKnown, bool maskStock, bool aaMakesTargets) {
    return onAtStart && exeKnown && maskStock && aaMakesTargets ? upscalePercent : 100u;
}
// What TemporalAa read for one eye: its input (set +0x3800) and output (the target's +0x1d0) widths. At 100% both
// are the eye's (input 0 = the target's); upscaled, the input is the render width and the output the eye's (to 8).
inline bool upscaleOk(uint32_t inputWidth, uint32_t outputWidth, uint32_t eyeWidth, uint32_t renderWidth) {
    if (!outputWidth || outputWidth < eyeWidth || outputWidth > ((eyeWidth + 7) & ~7u))
        return false;
    if (renderWidth >= eyeWidth)
        return !inputWidth || inputWidth == outputWidth || inputWidth == eyeWidth;
    return inputWidth == renderWidth;
}
// The safety: eye jobs (left ones) counted since the last pair reached the headset.
struct StallGuard {
    uint64_t jobsAtPair{};
    bool armed{}, tripped{};
    // `jobs` the left eye jobs so far, `pair` a new pair reached the headset this frame. True once, when it trips.
    bool frame(uint64_t jobs, bool pair, bool active) {
        if (tripped)
            return false;
        if (!active) {
            armed = false;
            return false;
        }
        if (pair || !armed) {
            armed = armed || pair;
            jobsAtPair = jobs;
            return false;
        }
        if (jobs - jobsAtPair >= stallPairs) {
            tripped = true;
            return true;
        }
        return false;
    }
};

enum Reason : uint32_t {
    none,
    exeDiffers,    // a byte check above failed
    maskDiffers,   // the mask table was not the exe's when the eyes were made
    gameAa,        // the game's AA makes no TAA targets when the eyes were made
    stalled,       // stallPairs eye jobs with no pair at the headset
    upscaleWrong,  // TemporalAa's input / output widths were not the eye's (upscaled eyes: restart with it OFF)
    hookFailed,    // TemporalAa's hook could not be made (MILES TAA still runs; no per-eye proof)
};
inline const char* reasonText(uint32_t r) {
    switch (r) {
    case exeDiffers: return "this game's TAA code differs from 2.810";
    case maskDiffers: return "the game's buffer mask table was not its own";
    case gameAa: return "the game's Anti-Aliasing makes no TAA targets";
    case stalled: return "eye pairs stopped reaching the headset";
    case upscaleWrong: return "the game's TAA did not take the smaller scene";
    case hookFailed: return "no per-eye TAA log (hook)";
    default: return "";
    }
}

inline constexpr uint32_t magic = 0x4154544d; // MTTA
inline constexpr uint32_t version = 1;
// SpidyMilesTaaData (stereo_probe.cpp): the per-eye proof that TemporalAa ran, and the history gate.
struct Data {
    uint32_t magic = miles_taa::magic, version = miles_taa::version, bytes = 256, flags{}; // flags: Flag bits
    uint32_t reason{}, percent = 100, renderWidth{}, renderHeight{};
    uint64_t calls[2]{};      // TemporalAa runs for the eye's job
    uint64_t ran[2]{};        // ... that wrote this frame's stamp (AA 1, a target, jitter not zero)
    uint64_t history[2]{};    // ... whose history gate passed (other stamp F - 1, +0x18e8 set)
    uint64_t aaWrites[2]{};   // eye jobs given AA 1 (+0x570)
    uint64_t queued[2]{};     // TemporalAa queued by step 4
    int32_t frame[2]{}, stamp[2]{}, otherStamp[2]{};
    uint32_t slotIndex[2]{}, previous[2]{}, cut[2]{}, aa[2]{};
    uint32_t inputWidth[2]{}, outputWidth[2]{};
    uint64_t leftJobs{};      // left eye jobs (the safety's count)
    uint64_t sequence{};
    uint64_t reserved[7]{};
};
enum Flag : uint32_t { flagOn = 1, flagLive = 2, flagHooked = 4, flagExeKnown = 8, flagAtStart = 16, flagFellBack = 32 };
static_assert(sizeof(Data) == 256, "SpidyMilesTaaData is 256 bytes");
} // namespace spidy::miles_taa
