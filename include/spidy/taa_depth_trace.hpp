// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
// Made by Agent Mulder.
// 0.0.76 A-depth TRACE (read-only diagnostics; nothing here changes a picture, nothing is on by default). Per 10 s:
//  - the mode the window ran with (vr_aa, vr_taa_depth, vr_ss, the eye size) so test runs can be split;
//  - the frame-time spread (eye pair to eye pair) and render hitches (pairs more than hitchMs apart) on their own
//    lines with a QPC time (the clock of the sound diagnostics and the launcher's time.perf_counter), never averaged;
//  - per eye: the depth there (this pair's, the shown one's), and the path each eye could take on its own
//    (eyeMask: bit 0 eye 0's depth usable, bit 1 eye 1's) with how often the two eyes disagree (a split pair). The
//    depth path itself is unchanged: both eyes or neither (5881bac);
//  - the convention check's round trip (mean and max px; over roundTripLimit the check fails, the depth is not used);
//  - THE HISTORY AGE: 8 fixed pixels of each eye (a 4 x 2 grid about the centre, probePixel) counted only on a
//    static surface (valid depth, probeNearM to probeFarM: a wall, not a hand or the sky). `match` the pairs in a
//    row their history came through the depth and passed the disocclusion test, `kept` the pairs in a row the clip
//    also left it alone. Correct depth: match climbs while swinging; one going back to 0 is not matching geometry;
//  - THE GRAZING ANGLE (graze bins): the counted pixels (one in 16 x 16) by the surface's angle to the view, from
//    normals rebuilt from the eye's depth (n.v; frontal, mid, grazing, beyond 16x anisotropy - where the hardware's
//    16:1 footprint runs out - a depth edge, no depth), each with its history found, clip-only / depth-only / both,
//    head-motion fallback, the history age, the frame-to-frame luma change and the depth residual (how far the last
//    eye's depth at the moved place is from the one it should have: the reprojection's own error where verifiable).
//    Only with vr_taa_depth on (the normals need the depth); vr_taa_depth 0 has none.
//  - per graze bin, THE EDGES apart (a pixel next to a depth step of more than edgeJump or next to no depth - a
//    silhouette - or on a CREASE: the depth's slope, linear across a plane, changes by more than creaseSlope from one
//    side of the pixel to the other - the 90-degree corner where a wall turns into a recessed window, even far off
//    where its step is far under edgeJump): their own clip-only / depth-only /
//    both and luma change; and for every counted pixel: a SPIKE (this pixel's luma above its own 3 x 3 variance box,
//    mean + gamma sigma: one bright sample the box cannot hold, so the history the clip pulls toward the box drops it
//    and the next spike brings it back) and whether the clip moved the history there, and a FLIP (the luma against
//    the history read there changed more than flipLuma: white / normal / white), edges apart too.
#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>

namespace spidy::taa_depth_trace {
inline constexpr uint32_t probes = 8;      // per eye
inline constexpr float probeNearM = 3.f;   // a static surface: no nearer (hands, body, the web)
inline constexpr float probeFarM = 300.f;  // and no farther
inline constexpr float hitchMs = 50.f;     // two eye pairs this far apart: a hitch line
// The probe pixels (the shader uses the same fractions): x 35 / 45 / 55 / 65 %, y 42 / 58 % of the eye.
inline constexpr float probeX[4] = {.35f, .45f, .55f, .65f};
inline constexpr float probeY[2] = {.42f, .58f};
inline void probePixel(uint32_t k, uint32_t w, uint32_t h, uint32_t& x, uint32_t& y) {
    x = static_cast<uint32_t>(static_cast<float>(w) * probeX[k & 3]);
    y = static_cast<uint32_t>(static_cast<float>(h) * probeY[(k >> 2) & 1]);
}
// The shader's probe words (4 x 32-bit each, after the eye's w * h ages): age (match << 16 | kept), flags,
// residual (float; -1 none), distance (float, metres; 0 none).
enum ProbeFlag : uint32_t {
    written = 1, deep = 2, staticSurface = 4, refusedFlag = 8, clippedFlag = 16, outsideFlag = 32, fallbackFlag = 64,
    agedBefore = 128
};
inline constexpr uint32_t probeWords = 4;
inline uint32_t packAge(uint32_t match, uint32_t kept) {
    return (match > 0xffffu ? 0xffffu : match) << 16 | (kept > 0xffffu ? 0xffffu : kept);
}
inline uint32_t matchOf(uint32_t age) { return age >> 16; }
inline uint32_t keptOf(uint32_t age) { return age & 0xffffu; }
inline uint32_t eyeMask(bool eye0, bool eye1) { return (eye0 ? 1u : 0u) | (eye1 ? 2u : 0u); }
inline bool split(uint32_t mask) { return mask == 1 || mask == 2; }

// The graze bins (the shader's, by |n.v| of the depth-rebuilt surface; an edge: the depth jumps more than
// edgeJump relative to both neighbours on a side).
enum Bin : uint32_t { frontal = 0, mid = 1, grazing = 2, beyondAniso = 3, depthEdge = 4, noDepth = 5, bins = 6 };
inline constexpr float midBelow = .5f, grazingBelow = .25f, anisoBelow = 1.f / 16.f, edgeJump = .1f,
                       creaseSlope = .25f;
inline uint32_t binOf(float nDotV) {
    if (!std::isfinite(nDotV))
        return noDepth;
    const float a = std::fabs(nDotV);
    return a >= midBelow ? frontal : a >= grazingBelow ? mid : a >= anisoBelow ? grazing : beyondAniso;
}
// The shader's words per bin (each eye's 6 bins x 10 words, zeroed before each pass).
enum GrazeWord : uint32_t {
    gPixels = 0, gSampled = 1, gClipOnly = 2, gDepthOnly = 3, gBoth = 4, gFallback = 5, gAgeSum = 6,
    gLumaMilli = 7, gResidualMicro = 8, gResidualMicroMax = 9, gEdgePixels = 10, gEdgeSampled = 11,
    gEdgeClipOnly = 12, gEdgeDepthOnly = 13, gEdgeBoth = 14, gEdgeLumaMilli = 15, gSpikes = 16, gSpikeClipped = 17,
    gFlips = 18, gEdgeFlips = 19, gEdgeOutLumaMilli = 20, gEdgeAgeSum = 21, gEdgeAgeMax = 22,
    grazeWords = 23
};
inline constexpr float flipLuma = .25f;           // the shader's FLIP: |luma now - history luma| over this
inline constexpr uint32_t grazeBytesPerEye = 1024; // 6 x 23 x 4 = 552, padded
static_assert(static_cast<uint32_t>(bins) * static_cast<uint32_t>(grazeWords) * 4 <= grazeBytesPerEye);

struct Ages {
    uint32_t samples{};            // static-surface probe readings
    uint32_t matchSum{}, matchMax{};
    uint32_t keptSum{}, keptMax{};
    uint32_t matchResets{};        // readings with match 0 (no depth history that pair)
    uint32_t residualN{};
    float residualSum{}, residualMax{};
};
static_assert(sizeof(Ages) == 36);
struct Graze {
    uint32_t pixels{}, sampled{}, clipOnly{}, depthOnly{}, both{}, fallback{};
    uint32_t ageSum{};          // the match age (pairs) over sampled pixels
    float lumaSum{};            // |luma now - luma of the history read| over sampled pixels (0..1)
    float residualSum{};        // relative depth residual over depth-tested pixels
    float residualMax{};
    uint32_t edgePixels{}, edgeSampled{}, edgeClipOnly{}, edgeDepthOnly{}, edgeBoth{};
    float edgeLumaSum{};
    uint32_t spikes{}, spikeClipped{}; // this pixel above its own 3 x 3 box; and the clip moved the history there
    uint32_t flips{}, edgeFlips{};     // |luma change| over flipLuma
    float edgeOutLumaSum{};            // edges: |TAA output luma - luma of the history read| (what is shown changes)
    uint32_t edgeAgeSum{}, edgeAgeMax{}; // edges: the match age (pairs in a row through the depth), sum and max
};
static_assert(sizeof(Graze) == 92);
inline constexpr uint32_t frameBuckets = 6; // pair-to-pair ms: < 12, 12-17, 17-25, 25-34, 34-50, 50+
inline uint32_t frameBucket(float ms) {
    return ms < 12 ? 0 : ms < 17 ? 1 : ms < 25 ? 2 : ms < 34 ? 3 : ms < 50 ? 4 : 5;
}
struct Window {
    uint32_t ms{};
    uint32_t pairs{}, wanted{}, used{};
    uint32_t aaMode{};        // vr_aa::Mode at the window's last pair
    uint32_t depthSetting{};  // live.txt vr_taa_depth (0 off, 1 on, 2 auto)
    uint32_t eyeW{}, eyeH{};  // the eyes' render size (vr_ss already applied)
    uint32_t outW{}, outH{};  // the headset's eye size (the TAA's; the eyes are filtered down to it with vr_ss)
    uint32_t ssPercent{};     // the eye supersample in force, per side (100 none)
    uint32_t spare{};
    uint32_t nowValid[2]{}, beforeValid[2]{}; // per eye: this pair's depth there, the shown pair's too
    uint32_t masks[4]{};      // pairs per eyeMask (what each eye could take on its own)
    uint32_t tripN{};
    float tripSum{}, tripMax{};
    uint32_t frames[frameBuckets]{};
    float frameMsSum{}, frameMsMax{};
    uint32_t hitches{};
    uint32_t probeReads{}, probeSkipped{};
    uint32_t grazeReads{};    // eyes whose graze bins came back
    Ages ages[3];             // taa_depth_check::Category: still, moving, fast
    Graze graze[3][bins];
};
static_assert(sizeof(Window) == 4 * 35 + 3 * 36 + 18 * 92); // 1904
struct Hitch {
    uint64_t qpcUs{};  // when the late pair was drawn (QPC, microseconds)
    float ms{};        // since the pair before
    uint32_t serial{};
};
static_assert(sizeof(Hitch) == 16);
inline constexpr uint32_t windowCount = 90;
inline constexpr uint32_t hitchCount = 64;
struct Data {
    uint32_t magic = 0x52544454; // TDTR
    uint32_t version = 1;
    uint32_t bytes = 32 + sizeof(Window) * (windowCount + 1) + sizeof(Hitch) * hitchCount;
    uint32_t windowMs = 10000;
    uint32_t closed{};
    uint32_t hitchesNoted{};  // all hitches so far (the ring holds the last hitchCount)
    uint32_t ageFault{};      // the age / graze buffers could not be made (0 none): the depth path runs without them
    uint32_t spare{};
    Window current;
    Window windows[windowCount];
    Hitch hitch[hitchCount];
};
static_assert(sizeof(Window) % 8 == 0);
static_assert(sizeof(Data) == 32 + sizeof(Window) * 91 + 16 * 64);
inline void roll(Data& d, uint64_t& startMs, uint64_t nowMs) {
    if (!startMs || nowMs < startMs) {
        startMs = nowMs;
        return;
    }
    if (nowMs - startMs >= d.windowMs) {
        d.current.ms = static_cast<uint32_t>(nowMs - startMs > 0xffffffffull ? 0xffffffffull : nowMs - startMs);
        d.windows[d.closed % windowCount] = d.current;
        ++d.closed;
        d.current = {};
        startMs = nowMs;
    }
}
inline void noteTrip(Window& w, double px) {
    if (!std::isfinite(px) || px < 0)
        return;
    ++w.tripN;
    w.tripSum += static_cast<float>(px);
    if (static_cast<float>(px) > w.tripMax)
        w.tripMax = static_cast<float>(px);
}
// One pair's time since the last (ms; 0 or not finite: the first, not counted).
inline void noteFrame(Data& d, uint64_t qpcUs, float ms, uint32_t serial) {
    if (!(ms > 0) || !std::isfinite(ms) || ms > 60000)
        return;
    auto& w = d.current;
    ++w.frames[frameBucket(ms)];
    w.frameMsSum += ms;
    w.frameMsMax = ms > w.frameMsMax ? ms : w.frameMsMax;
    if (ms >= hitchMs) {
        d.hitch[d.hitchesNoted % hitchCount] = {qpcUs, ms, serial};
        ++d.hitchesNoted;
        ++w.hitches;
    }
}
inline float floatOf(uint32_t bits) {
    float f;
    std::memcpy(&f, &bits, sizeof f);
    return f;
}
// One eye's probe words (probes x probeWords) read back, in the pair's movement category.
inline void noteProbes(Window& w, uint32_t category, const uint32_t* words) {
    if (category >= 3)
        return;
    ++w.probeReads;
    auto& a = w.ages[category];
    for (uint32_t k = 0; k < probes; ++k) {
        const uint32_t* p = words + probeWords * k;
        const uint32_t flags = p[1];
        if (!(flags & written) || !(flags & staticSurface)) {
            ++w.probeSkipped;
            continue;
        }
        const uint32_t match = matchOf(p[0]), kept = keptOf(p[0]);
        ++a.samples;
        a.matchSum += match;
        a.keptSum += kept;
        a.matchMax = match > a.matchMax ? match : a.matchMax;
        a.keptMax = kept > a.keptMax ? kept : a.keptMax;
        a.matchResets += match ? 0 : 1;
        const float residual = floatOf(p[2]);
        if (std::isfinite(residual) && residual >= 0) {
            ++a.residualN;
            a.residualSum += residual;
            a.residualMax = residual > a.residualMax ? residual : a.residualMax;
        }
    }
}
// One eye's graze words (bins x grazeWords, this pair's alone) in the pair's movement category.
inline void noteGraze(Window& w, uint32_t category, const uint32_t* words) {
    if (category >= 3)
        return;
    for (uint32_t b = 0; b < bins; ++b) {
        const uint32_t* g = words + static_cast<uint32_t>(grazeWords) * b;
        if (g[gSampled] > g[gPixels] || g[gClipOnly] + g[gDepthOnly] + g[gBoth] > g[gSampled] ||
            g[gEdgePixels] > g[gPixels] || g[gEdgeSampled] > g[gEdgePixels] ||
            g[gEdgeClipOnly] + g[gEdgeDepthOnly] + g[gEdgeBoth] > g[gEdgeSampled] || g[gSpikes] > g[gSampled] ||
            g[gSpikeClipped] > g[gSpikes] || g[gFlips] > g[gSampled] || g[gEdgeFlips] > g[gFlips])
            return; // not one pass's words
    }
    ++w.grazeReads;
    for (uint32_t b = 0; b < bins; ++b) {
        const uint32_t* g = words + static_cast<uint32_t>(grazeWords) * b;
        auto& o = w.graze[category][b];
        o.pixels += g[gPixels];
        o.sampled += g[gSampled];
        o.clipOnly += g[gClipOnly];
        o.depthOnly += g[gDepthOnly];
        o.both += g[gBoth];
        o.fallback += g[gFallback];
        o.ageSum += g[gAgeSum];
        o.lumaSum += static_cast<float>(g[gLumaMilli]) / 1000.f;
        o.residualSum += static_cast<float>(g[gResidualMicro]) / 1e6f;
        const float most = static_cast<float>(g[gResidualMicroMax]) / 1e6f;
        o.residualMax = most > o.residualMax ? most : o.residualMax;
        o.edgePixels += g[gEdgePixels];
        o.edgeSampled += g[gEdgeSampled];
        o.edgeClipOnly += g[gEdgeClipOnly];
        o.edgeDepthOnly += g[gEdgeDepthOnly];
        o.edgeBoth += g[gEdgeBoth];
        o.edgeLumaSum += static_cast<float>(g[gEdgeLumaMilli]) / 1000.f;
        o.spikes += g[gSpikes];
        o.spikeClipped += g[gSpikeClipped];
        o.flips += g[gFlips];
        o.edgeFlips += g[gEdgeFlips];
        o.edgeOutLumaSum += static_cast<float>(g[gEdgeOutLumaMilli]) / 1000.f;
        o.edgeAgeSum += g[gEdgeAgeSum];
        o.edgeAgeMax = g[gEdgeAgeMax] > o.edgeAgeMax ? g[gEdgeAgeMax] : o.edgeAgeMax;
    }
}
} // namespace spidy::taa_depth_trace
