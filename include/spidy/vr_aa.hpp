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

// SPIDY TAA's blend. The history share kept per pair: still keepStill (0.0.76 C-still094: .94, about 16 pairs
// averaged - one whole Halton cycle - was .9), easing to keepMoving at full motion - never cut for ordinary motion
// (0.0.73's cuts were "jagged when moving"); the clip, tighter while moving (clipGamma), keeps it from ghosting.
inline constexpr float keepStill = .94f, keepMoving = .6f;
// Motion share: the eye's turn (radians) and travel (metres) between its two pairs, ramped.
inline constexpr float turnFrom = .004f, turnFull = .05f, travelFrom = .01f, travelFull = .3f;
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
inline bool cutAway(bool both, float turn, float travel) {
    return !both || !(turn <= cutTurn) || !(travel <= cutTravel);
}
// The share kept for a motion share `m` (0: shown as it came). `both`: the eye's last and current views are known
// and line up.
inline float keepFor(bool both, float turn, float travel, float m) {
    if (cutAway(both, turn, travel))
        return 0;
    return keepStill - (keepStill - keepMoving) * m;
}
inline float keep(bool both, float turn, float travel) {
    return keepFor(both, turn, travel, motion(turn, travel));
}
// The variance clip's width in standard deviations: 1.25 still (sub-pixel steps kept), 0.85 at full motion.
inline constexpr float gammaStill = 1.25f, gammaMoving = .85f;
inline float clipGammaFor(float m) {
    return gammaStill - (gammaStill - gammaMoving) * m;
}
inline float clipGamma(float turn, float travel) {
    return clipGammaFor(motion(turn, travel));
}
// 0.0.76 C-still094: the still / moving classification, so a still head is classed still. The turn and travel are
// between the eye's last two pairs; a pair that came late (a held or skipped frame) spans more time, so its motion
// is scaled to one nominal pair (1/45 s) - only ever down, at most to a quarter. And one pair of motion alone (a
// tracking blip, a hitch) does not leave still: it takes `settlePairs` moving pairs in a row (Settle, per eye).
inline constexpr float pairSeconds = 1.f / 45;
inline constexpr uint32_t settlePairs = 2;
inline float perPair(float v, float seconds) {
    if (!(seconds > pairSeconds) || !std::isfinite(seconds))
        return v;
    const float s = pairSeconds / seconds;
    return v * (s < .25f ? .25f : s);
}
struct Settle {
    uint32_t moved{}; // moving pairs in a row
};
// The motion share the blend and the clip use (0 still).
inline float classify(bool both, float turn, float travel, float seconds, Settle& s) {
    if (cutAway(both, turn, travel)) {
        s.moved = 0;
        return 0;
    }
    const float m = motion(perPair(turn, seconds), perPair(travel, seconds));
    if (!(m > 0)) {
        s.moved = 0;
        return 0;
    }
    if (s.moved < settlePairs)
        ++s.moved;
    return s.moved >= settlePairs ? m : 0;
}
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
