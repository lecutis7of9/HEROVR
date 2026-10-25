// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR (Ilya Mezerowsky).
#pragma once
// Update 37.2: FOVEATED (EXPERIMENTAL), fixed foveated rendering with D3D12 Variable Rate
// Shading (tier 2, a shading-rate image). Default OFF; live; the hologram's GRAPHICS page
// (FOVEATED, INNER SIZE, FOVEATED DEBUG); live.txt foveated, fovea_inner, fovea_debug.
//
// Where it applies (native_eye_gpu.cpp): only while the game records an EYE VIEW's render job
// (between the job's begin and end timestamps Spidy already marks per eye, stereo_probe.cpp
// beginJob/endJob: confirmed, since Update 13), on the same command list, and only for a pass
// that binds 3 or more colour targets plus a depth buffer (the opaque G-buffer pass). Shadow
// maps and depth prepasses (no colour target), UI, the hologram, TAA/resolve, post-FX, the
// swapchain copy and the flat view (one target, or outside an eye job) never get it, and the
// rate is set back to null (1x1, no image) at the next pass and at the job's end.
// That the G-buffer is recorded inside the eye job on that list is a CANDIDATE (not seen on
// the PC yet): telemetry (SpidyFoveaData) counts tagged passes, applied passes and multi-target
// passes outside any eye job, so the first session shows it. No game address is used.
//
// Never a crash or the flat screen: no tier 2 (OPTIONS6), a refused interface, an eye view
// drawn at another size than the eye images (an upscaler) or any D3D12 failure turns it off
// for the session with a reason (Fault) and a log line; the game renders as without it.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace spidy::foveation {
enum Level : uint32_t { off = 0, light, balanced, aggressive, levels };
inline constexpr const char* levelNames[levels] = {"OFF", "LIGHT", "BALANCED", "AGGRESSIVE"};
inline const char* levelName(uint32_t l) { return l < levels ? levelNames[l] : "OFF"; }
inline constexpr const char* levelKeys[levels] = {"off", "light", "balanced", "aggressive"};
inline constexpr uint32_t defaultLevel = off;
inline constexpr uint32_t innerMin = 50, innerMax = 150, innerStep = 5, defaultInner = 100;

// D3D12_SHADING_RATE values (d3d12.h), written into the R8_UINT image.
inline constexpr uint8_t rate1x1 = 0x0, rate2x2 = 0x5, rate4x4 = 0xA;

// The zones in lens units: the distance from the lens centre, 1 = half the image height.
// The full-rate circle; 2x2 beyond it; 4x4 beyond `outer` (AGGRESSIVE only).
struct Zones {
    float inner, outer; // outer: +inf for no 4x4
};
inline Zones zones(uint32_t level, uint32_t innerPercent) {
    const float s = static_cast<float>(std::clamp(innerPercent, innerMin, innerMax)) / 100.f;
    switch (level) {
    case light:
        return {.85f * s, INFINITY};
    case balanced:
        return {.62f * s, INFINITY};
    case aggressive:
        return {.48f * s, .95f * s};
    default:
        return {INFINITY, INFINITY};
    }
}
// The lens centre in the eye image (0..1 across, 0..1 down) from the eye's OpenXR field of view
// (tangents of its angles: left and down negative). The straight-ahead direction lands there; an
// asymmetric frustum puts it off the image centre (toward the nose).
struct Centre {
    float u = .5f, v = .5f;
};
inline Centre lensCentre(float tanLeft, float tanRight, float tanUp, float tanDown) {
    Centre c;
    if (tanRight - tanLeft > 1e-4f)
        c.u = std::clamp(-tanLeft / (tanRight - tanLeft), .25f, .75f);
    if (tanUp - tanDown > 1e-4f)
        c.v = std::clamp(tanUp / (tanUp - tanDown), .25f, .75f);
    return c;
}
// The rate of a pixel at (u, v) (0..1) in an image width/height `aspect`.
inline uint8_t rateAt(const Zones& z, Centre c, float aspect, float u, float v, bool has4x4) {
    const float du = (u - c.u) * 2 * aspect, dv = (v - c.v) * 2;
    const float d = std::sqrt(du * du + dv * dv);
    if (d <= z.inner)
        return rate1x1;
    if (d <= z.outer || !has4x4)
        return rate2x2;
    return rate4x4;
}
// The tier-2 image: one byte per tile x tile pixels (row major, `tilesW` a row).
inline std::vector<uint8_t> image(uint32_t level, uint32_t innerPercent, Centre c, uint32_t width, uint32_t height,
                                  uint32_t tile, bool has4x4, uint32_t& tilesW, uint32_t& tilesH) {
    tile = std::max(tile, 1u);
    tilesW = (width + tile - 1) / tile;
    tilesH = (height + tile - 1) / tile;
    std::vector<uint8_t> out(static_cast<size_t>(tilesW) * tilesH, rate1x1);
    if (!width || !height || level == off || level >= levels)
        return out;
    const auto z = zones(level, innerPercent);
    const float aspect = static_cast<float>(width) / static_cast<float>(height);
    for (uint32_t y = 0; y < tilesH; ++y)
        for (uint32_t x = 0; x < tilesW; ++x) {
            const float u = (static_cast<float>(x) + .5f) * static_cast<float>(tile) / static_cast<float>(width);
            const float v = (static_cast<float>(y) + .5f) * static_cast<float>(tile) / static_cast<float>(height);
            out[static_cast<size_t>(y) * tilesW + x] = rateAt(z, c, aspect, std::min(u, 1.f), std::min(v, 1.f), has4x4);
        }
    return out;
}
// The share of pixels shaded (1 = all) an image leaves: an estimate of the pass's saving.
inline float shadedShare(const std::vector<uint8_t>& img) {
    if (img.empty())
        return 1;
    double s = 0;
    for (const auto r : img)
        s += r == rate1x1 ? 1. : r == rate2x2 ? .25 : 1. / 16;
    return static_cast<float>(s / static_cast<double>(img.size()));
}
// The eye images' centres: the right eye's is the left's mirrored when only the left is known.
inline Centre mirrored(Centre c) {
    return {1 - c.u, c.v};
}

// Why it turned itself off (FOVEATED shows it; the log says it).
enum Fault : uint32_t {
    fine = 0,
    noTier2,          // the GPU or driver has no VRS tier 2 (D3D12_OPTIONS6)
    noInterface,      // a game list refused ID3D12GraphicsCommandList5
    imageFailed,      // the shading-rate image could not be made or uploaded
    sizeMismatch,     // the eye view is drawn at another size than the eye images (an upscaler)
    hookFailed,       // the pass hooks could not go in
    faults
};
inline constexpr const char* faultNames[faults] = {"", "NO VRS TIER 2", "LIST REFUSED", "IMAGE FAILED",
                                                   "OTHER SIZE", "HOOK FAILED"};
inline const char* faultName(uint32_t f) { return f < faults ? faultNames[f] : "FAULT"; }

inline constexpr const char* help = "EXPERIMENTAL: shades each lens's edge at a lower rate for more FPS.";
inline constexpr const char* innerHelp = "How big the full-detail circle in the middle of each lens is.";
inline constexpr const char* debugHelp = "Tints the zones: green full detail, yellow half, red quarter, to check them.";
inline constexpr const char* statsHelp = "FPS HIGH / LOW / AVG and eye GPU ms at this level; press to reset all levels.";

// Per level FPS / eye GPU ms, for the perf report and the hologram (compare live).
struct LevelStats {
    uint64_t frames{};
    double fpsSum{}, gpuSum{};
    uint32_t gpuFrames{};
    float low = INFINITY, high = 0;
    float avg() const { return frames ? static_cast<float>(fpsSum / static_cast<double>(frames)) : 0.f; }
    float gpuAvg() const { return gpuFrames ? static_cast<float>(gpuSum / gpuFrames) : NAN; }
};
class Perf {
  public:
    // One second's FPS and that second's eye GPU ms (NaN unknown), at `level`.
    void note(uint32_t level, float fps, float gpuMs) {
        if (level >= levels || !std::isfinite(fps) || fps <= 0)
            return;
        auto& s = stats_[level];
        ++s.frames;
        s.fpsSum += fps;
        s.low = std::min(s.low, fps);
        s.high = std::max(s.high, fps);
        if (std::isfinite(gpuMs)) {
            s.gpuSum += gpuMs;
            ++s.gpuFrames;
        }
    }
    void reset() { stats_ = {}; }
    const LevelStats& at(uint32_t level) const { return stats_[std::min(level, levels - 1)]; }

  private:
    std::array<LevelStats, levels> stats_{};
};

// Telemetry, SpidyFoveaData ('FOVE').
inline constexpr uint32_t dataMagic = 0x45564f46;
struct LevelData {
    uint32_t seconds{};
    float avgFps{}, lowFps{}, highFps{}, gpuMs{};
    uint32_t unused{};
};
struct Data {
    uint32_t magic = dataMagic, version = 1, bytes = 216, level{};
    uint32_t inner{}, debug{}, tier{}, tile{};
    uint32_t has4x4{}, fault{}, imageWidth{}, imageHeight{};
    float shadedShare = 1, centreU = .5f, centreV = .5f, unused{};
    uint64_t taggedPasses{}, appliedPasses{}, untaggedPasses{}, resets{}, images{}, mismatches{};
    LevelData perLevel[levels]{};
    uint64_t sequence{}; // odd while written (publish)
};
static_assert(sizeof(LevelData) == 24);
static_assert(sizeof(Data) == 216);
} // namespace spidy::foveation
