#pragma once
// 0.0.74 test 10 SM1STYLE (live.txt sm1style, ON by default; the owner: "try its approach yes try it exactly and
// make sure nothing of ours can overwrite it"). Made by Agent Mulder.
//
// SpidyVR's own per-eye anti-aliasing (SpidyVR src/game_xr.cpp:3311 SpidyEyeTemporal(config.upscale | 7u << 8),
// src/stereo_probe.cpp:906-1045 and 1294-1306, include/spidy/eye_resolution.hpp:25), four steps, ported to
// Spider-Man2.exe 2.810.0.0:
//  1. A jittered eye lens: Halton (2, 3), the same point for both eyes, 2 * (h - 0.5) / render size,
//     clamp(ceil(8 * ratio^2), 8, 32) phases (ratio = eye width / render width), phase = frame % phases + 1.
//  2. The eye's buffer set made with the TAA targets, at the eye's size (the mask table's TAA kind; engine_taa.hpp).
//  3. The eye job's AA forced to TAA: settings +0x570 = 1 per eye job (copied to 0xc2c6a10, TemporalAa's check).
//  4. TemporalAa (0xb1) queued on view +0x18d0 where the profile's +0xcc8 bit 0 makes the game leave it out.
//  And, as SpidyVR does with its default upscale (67): the eye's scene rendered at `upscalePercent` per side, the
//  game's own TemporalAa resolving it to the eye's size (CANDIDATE: see upscaleOk; checked on the running game,
//  never assumed).
// While it runs nothing of ours touches the eyes' picture: no FXAA / downsample, no EYE AA history, no CAS sharpen
// (native_gpu::setSm1StyleLock), no EYE AA lens jitter (so no JITTER CALM / LIGHT HOLD shrink), no ENGINE TAA,
// render scale and supersample 100, the game's own mip bias and anisotropy, the game's sharpness at its lowest.
//
// Spider-Man2.exe addresses (CONFIRMED by disassembly of the 2.810.0.0 exe, 0.0.74 test 3 session):
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
// Every one is still checked against these bytes when the mod starts; one that differs and SM1STYLE is never used
// that session: the 0.0.72 path, and one console line.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>

namespace spidy::sm1style {
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

// The eye's scene size, percent per side, when the eyes are made with SM1STYLE (SpidyVR's default upscale, tools/run_game_vr.py:52).
inline constexpr uint32_t upscalePercent = 67;
// Eye jobs the game made in a row that never reached the headset as a pair before SM1STYLE turns off.
inline constexpr uint32_t stallPairs = 45;
// SpidyXrStart's options bit (tools/run_game_vr.py SM1STYLE_OPTION) and SpidyEyeTemporal's (value >> 8) bit.
inline constexpr uint32_t startOption = 0x1000000, eyeTemporalBit = 0x20;
// What SpidyVR keeps while it runs (SpidyVR src/game_xr.cpp:112 renderScale 100; no supersample, FXAA, history or
// sharpen of its own): the values SM1STYLE locks.
inline constexpr uint32_t renderScale = 100, supersample = 100;

// SpidyVR's eyeJitter (src/stereo_probe.cpp:1294-1306): its phase count for a scene `renderWidth` wide in an eye `eyeWidth` wide.
inline uint32_t phases(uint32_t eyeWidth, uint32_t renderWidth) {
    const float ratio = renderWidth ? static_cast<float>(eyeWidth) / static_cast<float>(renderWidth) : 1.f;
    return std::clamp<uint32_t>(static_cast<uint32_t>(std::ceil(8 * ratio * ratio)), 8, 32);
}
inline float halton(uint32_t index, uint32_t radix) {
    float value = 0, fraction = 1.f / static_cast<float>(radix);
    for (; index; index /= radix, fraction /= static_cast<float>(radix))
        value += fraction * static_cast<float>(index % radix);
    return value;
}
// The lens jitter (NDC) for frame `frame`, the same for both eyes.
// 0.0.76 FAR SHAKE (live.txt vr_jitter / vr_game_jitter, both live).
// vr_jitter off: the eyes keep one fixed lens offset instead of the Halton walk.
// It is not zero because the game's TemporalAa 0x2c2dc60 (CONFIRMED, see
// temporalAaCheckRva) skips a job whose jitter (+0x428/+0x42c) is zero: the eyes
// would lose their TAA. 1e-6 NDC is about a thousandth of an eye pixel.
inline constexpr float steadyJitter = 1e-6f;
inline std::array<float, 2> eyeJitterFor(bool off, std::array<float, 2> walk) {
    return off ? std::array<float, 2>{steadyJitter, steadyJitter} : walk;
}
// vr_game_jitter: the game's own view (on the head while the eyes show, never
// shown itself) and its flat-game TAA jitter, sized for the flat 1280x720
// picture (run 001842: [0.00059, 0.00243] NDC against the eyes' 0.00019).
// 0.0.76 GHOST FIX: KEEP is the default again (0 keep, 1 off). Zeroing it (c8974f4)
// made TemporalAa skip that view's job every frame (0x2c2dd09) and the owner's
// next run (005811) brought "ghosting of the city and buildings" while the far
// shake stayed: the zero bought nothing and cost a picture the eyes had in 001842.
// `off` stays a live test switch only.
// 0.0.76 JITTER FIX (owner): `stock` is the stock camera's STORED +0x428/+0x42c, which the lens 0x2c75560
// (CONFIRMED) wrote as 2 * x and -2 * y; passed back in, the head views carried twice the game's jitter with y
// flipped. OFF (pass 0) is the default now; KEEP is c71a59c's doubled value, to compare.
inline float gameViewJitter(bool off, float stock) { return off ? 0.f : stock; }
// 0.0.76 EYE HISTORY CHECK (the outside advice: each eye its own TAA history).
// Disassembly of 2.810.0.0 (CONFIRMED): the job's history set is job +0x18e8, its current one +0x18e0
// (0x2c98290 / 0x2c98240); the job start 0x2c98440 sets both to the set it is given (the game's main set
// [0xc15d7d8] when none) and +0x13f9 = (set == main set); each frame 0x2c96f80 copies +0x18e0 to +0x18e8 and,
// only with +0x13f9, swaps to the main ping-pong pair [0xc15d7d8] / [0xc15d7e0]. TemporalAa copies its output
// into the set's history target (0x2c2e783: set +0x12660, or +0x12a30 with [0xc2c7d95]).
// So an eye reads only its own history when its job's sets are one set that is neither the main set nor the
// other eye's: counted per TemporalAa run (Data::ownHistory / sharedHistory).
inline constexpr uint32_t mainSetGlobal = 0xc15d7d8;
inline bool ownHistory(uintptr_t set, uintptr_t history, uintptr_t mainSet, uintptr_t otherEyeSet) {
    return set && history == set && set != mainSet && set != otherEyeSet;
}
inline std::array<float, 2> jitter(uint64_t frame, uint32_t eyeWidth, uint32_t renderWidth, uint32_t renderHeight) {
    const uint32_t phase = static_cast<uint32_t>(frame % phases(eyeWidth, renderWidth)) + 1;
    return {2 * (halton(phase, 2) - .5f) / std::max(1u, renderWidth),
            2 * (halton(phase, 3) - .5f) / std::max(1u, renderHeight)};
}
// The percent the eyes are made at: the upscale only when SM1STYLE was on at the start, every exe check passed, the
// mask table is the exe's (the TAA targets will be made) and the game's AA makes them. 0.0.76 VISUAL: `wanted`, the
// start's percent (SpidyEyeTemporal's low byte: tools/run_game_vr.py sends 100, native, by default, or 67 with
// live.txt vr_render = upscaled); one outside 50..100 gives SpidyVR's upscalePercent.
inline uint32_t madePercent(bool onAtStart, bool exeKnown, bool maskStock, bool aaMakesTargets,
                            uint32_t wanted = upscalePercent) {
    if (wanted < 50 || wanted > 100)
        wanted = upscalePercent;
    return onAtStart && exeKnown && maskStock && aaMakesTargets ? wanted : 100u;
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
    hookFailed,    // TemporalAa's hook could not be made (SM1STYLE still runs; no per-eye proof)
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

// 0.0.75: the one console line SM1STYLE prints at a start where it fell back or warns (game_xr). The game's
// Anti-Aliasing fall back says how to fix it; the launcher looks for aaFixHint (LiveFile.Sm1StyleAaLine).
inline constexpr const char* aaFixHint = "set the game's Anti-Aliasing to TAA";
inline std::string reasonLine(uint32_t r) {
    const bool runs = r == upscaleWrong || r == hookFailed;
    std::string line = std::string(runs ? "SM1STYLE warning: " : "SM1STYLE: off, not used - ") + reasonText(r);
    if (r == upscaleWrong)
        return line + " (live.txt sm1style = off + restart VR if the eyes look wrong)";
    if (runs)
        return line;
    if (r == gameAa)
        return line + " - the 0.0.72 eye picture this session; " + aaFixHint +
               " (game Settings > Graphics) and restart VR to use SM1STYLE";
    return line + " - the 0.0.72 eye picture this session";
}
inline constexpr const char* notAskedLine =
    "SM1STYLE: off - turned off for this start (live.txt sm1style = off); sm1style = on + restart VR to use it";
inline constexpr const char* neverStartedLine =
    "SM1STYLE: off - asked for, but no eye pair was made with it in the first minute (the 0.0.72 eye picture)";
inline constexpr const char* noRecordLine =
    "SM1STYLE: off - the stereo renderer's SM1STYLE record is missing (the 0.0.72 eye picture)";
// 0.0.75 WINDOW EDGES (stereo_probe.cpp SpidyWindowEdgesNow: 1 on, 2 off): its one console line.
inline const char* windowEdgesLine(uint32_t state) {
    return state == 1 ? "WINDOW EDGES: on - each eye's lens summaries cover its whole off-centre lens (lit windows "
                        "reach the outer edge)"
                      : "WINDOW EDGES: off - this game build's lens setter differs (2.810.0.0 expected), eye lenses "
                        "left as they are";
}

inline constexpr uint32_t magic = 0x53314d53; // SM1S
inline constexpr uint32_t version = 3; // 79.3: allCalls / eyeByTable
// SpidySm1StyleData (stereo_probe.cpp): the per-eye proof that TemporalAa ran, and the history gate.
struct Data {
    uint32_t magic = sm1style::magic, version = sm1style::version, bytes = 256, flags{}; // flags: Flag bits
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
    // Version 2 (0.0.76 EYE HISTORY CHECK, ownHistory above): runs whose history was the eye's own set, runs whose
    // set was shared (the main set, the other eye's, or a history set not the current one), and runs whose job was
    // not the eye's job of its run before.
    uint64_t ownHistory[2]{}, sharedHistory[2]{}, jobChanged[2]{};
    // Version 3 (79.3, t2172): every TemporalAa call (the game's own view's too) and the eye runs found through the eye
    // job table (the job's eye from its copy until it ends) - sm1Jobs (each eye's latest job only) missed the runs of a
    // job whose eye had copied the next frame's job already (the right eye's 76 of 3695 in the fbd8c29 report).
    uint32_t allCalls{}, eyeByTable{};
};
// flagSteps: the jitter and the job AA run (live, or eyes made smaller than their image after a fall back).
enum Flag : uint32_t {
    flagOn = 1, flagLive = 2, flagHooked = 4, flagExeKnown = 8, flagAtStart = 16, flagFellBack = 32, flagSteps = 64
};
static_assert(sizeof(Data) == 256, "SpidySm1StyleData is 256 bytes");
} // namespace spidy::sm1style
