#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
//
// 0.0.76 SPIDY TAA (live.txt vr_aa, a start setting). How the headset's eyes are anti-aliased:
//   ours (the default): our own per-eye temporal AA in the mod's eye pass (native_eye_gpu.cpp taaShader). Each eye
//     keeps its own history; each new picture is blended with its own eye's last one, turned to it with that eye's
//     own previous and current view and lens (eye_history::historyReprojection, from the pose the headset was given),
//     neighbourhood-clamped and variance-clipped (YCoCg, mean +/- gamma * sigma, inside the 3 x 3 min / max) so what
//     the turn cannot line up is not kept as a ghost. Our own Halton (2, 3) step (eye_taa.hpp) moves each eye's lens
//     tangents by the same sub-pixel amount, the headset layer is given the same lens. The game's TemporalAa never
//     runs on the eyes: the eye buffers get no TAA targets (no SM1STYLE, no -EngineTaa; engine_taa.hpp, CONFIRMED
//     handler 0x2c2dc60 returns without them) and the eye lenses get no game jitter (stereo_probe eyeJitter 0).
//     If our pass cannot be built the eyes fall back to SMAA by themselves.
//   smaa: SMAA 1x per eye (Jimenez et al., spidy/smaa), no history: nothing temporal, nothing to shake or ghost.
//   legacy: what came before (vr_aa = game: SM1STYLE, the game's TAA in each eye; or sm1style = off's 0.0.72
//     picture): our FXAA / EYE AA / sharpen follow their own settings.
// No eye depth or motion vectors are used: the game's depth and motion-vector targets for the eyes have no CONFIRMED
// address in this port, so the turn is from the pose alone; what moves on its own, and near parallax while
// travelling, is held by the clip, not reprojected.
#include <cmath>
#include <cstdint>

namespace spidy::vr_aa {
enum Mode : uint32_t { ours = 0, smaa = 1, legacy = 2 };
// SpidyXrStart's options (tools/run_game_vr.py VR_AA_SMAA_OPTION / VR_AA_LEGACY_OPTION). Neither: ours. SM1STYLE's
// start bit (sm1style.hpp startOption, 0x1000000) is legacy too: the game's TAA in each eye, nothing of ours over it.
inline constexpr uint32_t smaaOption = 0x2000000, legacyOption = 0x4000000, sm1styleOption = 0x1000000;
inline constexpr Mode modeFor(uint32_t options) {
    return (options & (legacyOption | sm1styleOption)) ? legacy : (options & smaaOption) ? smaa : ours;
}
inline const char* modeName(uint32_t mode) {
    return mode == ours ? "SPIDY TAA" : mode == smaa ? "SMAA" : mode == legacy ? "LEGACY" : "?";
}
// What ran on the last pair (Data::running).
enum Running : uint32_t { none = 0, taa = 1, smaaPass = 2, fxaaPass = 3 };

// SPIDY TAA's blend. The history share kept per pair: still keepStill (0.0.76 SPIDY TAA TUNE: .94, a current-frame
// weight of .06 - about 16 pairs, one full 16-step Halton walk, averaged - was .9), easing to keepMoving at full
// motion - never cut for ordinary motion (0.0.73's cuts were "jagged when moving"); the clip, tighter while moving
// (clipGamma), keeps it from ghosting. The 509bdc9 run (Nov 2): window frames and building detail still crawled -
// 2567 of 6995 pairs eased toward .6 by the small turns a head always makes - so the ease now starts later and ends
// higher.
inline constexpr float keepStill = .94f, keepMoving = .8f;
// Motion share: the eye's turn (radians) and travel (metres) between its two pairs, ramped. TUNE: from .012 rad (about
// 30 deg/s at 45 pairs/s: a head held still, or looking around slowly, keeps the full average; was .004) to .08
// (was .05); travel from .03 m (was .01: walking keeps it).
inline constexpr float turnFrom = .012f, turnFull = .08f, travelFrom = .03f, travelFull = .3f;
// Past these the old picture is not this place any more (a camera cut, a teleport): start again.
inline constexpr float cutTurn = .26f, cutTravel = 3.f;
inline float ramp(float v, float from, float full) {
    if (!(v > from))
        return 0;
    return v >= full ? 1.f : (v - from) / (full - from);
}
inline float motion(float turn, float travel) {
    const float a = std::isfinite(turn) ? ramp(turn, turnFrom, turnFull) : 1.f;
    const float b = std::isfinite(travel) ? ramp(travel, travelFrom, travelFull) : 1.f;
    return a > b ? a : b;
}
// The share kept (0: shown as it came). `both`: the eye's last and current views are known and line up.
inline float keep(bool both, float turn, float travel) {
    if (!both || !(turn <= cutTurn) || !(travel <= cutTravel))
        return 0;
    return keepStill - (keepStill - keepMoving) * motion(turn, travel);
}
// The variance clip's width in standard deviations, inside the 3 x 3 min / max: TUNE 1.5 still (was 1.25: looser on
// still areas, so the Halton steps' sub-pixel window edges are averaged, not clipped back to this frame's alias),
// 1.0 at full motion (was .85).
inline constexpr float gammaStill = 1.5f, gammaMoving = 1.f;
inline float clipGamma(float turn, float travel) {
    return gammaStill - (gammaStill - gammaMoving) * motion(turn, travel);
}
// TUNE: a mild RCAS (AMD FidelityFX's robust contrast-adaptive sharpen, its 0.1875 lobe limit) after the TAA only,
// never fed back into the history: sharpenAmount of its full strength, so the longer average is not soft.
inline constexpr float sharpenAmount = .15f, rcasLimit = .1875f;
// The RCAS lobe (negative: how much of the 4 neighbours is taken away) for one channel; the shader's sum, for tests.
// b d f h the cross neighbours, e the centre, all 0..1.
inline float rcasLobe(float b, float d, float f, float h, float e, float amount) {
    const float mn4 = std::fmin(std::fmin(b, d), std::fmin(f, h)), mx4 = std::fmax(std::fmax(b, d), std::fmax(f, h));
    const float hitMin = std::fmin(mn4, e) / (4.f * mx4 + 1e-5f);
    const float hitMax = (1.f - std::fmax(mx4, e)) / std::fmin(4.f * std::fmin(mn4, e) - 4.f, -1e-5f);
    const float lobe = std::fmax(-hitMin, hitMax);
    return std::fmax(-rcasLimit, std::fmin(lobe, 0.f)) * amount;
}
inline float rcasPixel(float b, float d, float f, float h, float e, float amount) {
    const float lobe = rcasLobe(b, d, f, h, e, amount);
    const float v = (lobe * (b + d + f + h) + e) / (4.f * lobe + 1.f);
    return v < 0 ? 0.f : v > 1 ? 1.f : v;
}
// TUNE: the history's own pictures are R10G10B10A2_UNORM (the staged eyes' 8 bits stall a .94 average: a step under
// half a level, 1/255, times 1 / .06 is never made, ~3% stuck; 10 bits is a quarter of that, at the same size). The
// TAA writes there; the RCAS pass writes the staged eye's colour (its alpha kept) from it.
// SpidyVrAaData, for the launcher's report (tools/vr_aa_record.py).
struct Data {
    uint32_t magic = 0x41415653; // SVAA
    uint32_t version = 1;
    uint32_t bytes = 72;
    uint32_t asked{};       // Mode at this start
    uint32_t running{};     // Running, the last pair
    uint32_t smaaFault{};   // SMAA could not be built: 1 root, 2 shader, 3 pipeline, 4 descriptor heaps, 5 textures
    uint32_t historyFault{};// SPIDY TAA's pass could not be built (eye_taa reasons 1-5)
    uint32_t fellBack{};    // 1: SPIDY TAA asked, SMAA (or FXAA) ran instead
    uint64_t taaPairs{};    // pairs through SPIDY TAA
    uint64_t smaaPairs{};   // pairs through SMAA
    uint64_t fxaaPairs{};   // pairs through FXAA because SMAA failed too
    double smaaGpuMs{};     // GPU time of the timed SMAA passes (both eyes), summed
    uint64_t smaaTimed{};
};
static_assert(sizeof(Data) == 72);
} // namespace spidy::vr_aa
