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
//    With vr_taa_depth on; 0.0.76 SEAM TRACE: or with vr_taa_depth off and live.txt vr_taa_seam on (the eyes'
//    depth copied and read for these bins only, the history turned by the head as always, the picture unchanged).
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
// 0.0.76 SEAM TRACE: after the bins, the SEAM cells (the shader's noteSeam): each counted pixel with a history and
// a depth, by the surface's angle (seamFrontal: bin frontal, seamMid: mid, seamGrazing: grazing and beyond 16x,
// seamSilhouette: a depth edge with no normal) x its distance (near under seamNearM, mid under seamFarM, far) x
// flat or seam (grazeBin's edge: a depth step, a silhouette or a crease - real geometry), 5 words each: pixels, the
// pre-TAA change (this pair's unaccumulated eye image against the history read there, milli-luma), the shown
// change (the TAA's output against the same history read), and how many of each went over flickerLuma.
enum SeamAngle : uint32_t { seamFrontal = 0, seamMid = 1, seamGrazing = 2, seamSilhouette = 3, seamAngles = 4 };
enum SeamWord : uint32_t { sPixels = 0, sInMilli = 1, sOutMilli = 2, sInFlicker = 3, sOutFlicker = 4, seamWords = 5 };
inline constexpr uint32_t seamDistances = 3, seamKinds = 2; // near / mid / far; flat / seam
inline constexpr float seamNearM = 8.f, seamFarM = 30.f, flickerLuma = 1.f / 16.f;
inline constexpr uint32_t seamCells = seamAngles * seamDistances * seamKinds;     // 24
inline constexpr uint32_t seamAt = static_cast<uint32_t>(bins) * static_cast<uint32_t>(grazeWords); // word 138
inline uint32_t seamAngleOf(uint32_t bin) {
    return bin == frontal ? seamFrontal : bin == mid ? seamMid : bin == depthEdge ? seamSilhouette : seamGrazing;
}
inline uint32_t seamDistanceOf(float metres) { return metres < seamNearM ? 0 : metres < seamFarM ? 1 : 2; }
inline uint32_t seamCell(uint32_t angle, uint32_t distance, bool seam) {
    return (angle * seamDistances + distance) * seamKinds + (seam ? 1 : 0);
}
// 0.0.76 EDGE TRACE: after the seam cells, the HOT EDGES - the counted pixels on a seam (as above) whose own 3 x 3
// luma spans more than hotSpread (a lit frame face against the dark recess: the coverage flash), by SeamAngle,
// hotWords each (HotWord); then a sample counter and the first `edgeSamples` hot edges of the pass, sampleWords each
// (SampleWord), raw.
enum HotWord : uint32_t {
    hPixels = 0, hAgeSum = 1, hAgeMax = 2, hWeightMilli = 3, hClipOnly = 4, hDepthOnly = 5, hBoth = 6, hNeither = 7,
    hSpreadMilli = 8, hSigmaMilli = 9, hDepthSpreadMilli = 10, hBright = 11, hInMilli = 12, hOutMilli = 13,
    hOutFlicker = 14, hotWords = 15
};
// A sample: 0 x | y << 16 (the eye's pixel), 1 the age (pairs in a row the clip left its history alone), 2 flags
// (SampleFlag | the graze bin << 8), 3 the history weight applied (milli), 4 luma milli | 3 x 3 luma span milli << 16,
// 5 the depth there (float bits), 6 the 3 x 3 depth spread (milli of the distance), 7 pre-TAA change milli | shown
// change milli << 16.
enum SampleFlag : uint32_t { sampleClipped = 1, sampleRefused = 2, sampleBright = 4, sampleWritten = 128 };
inline constexpr uint32_t edgeSamples = 8, sampleWords = 8;
inline constexpr float hotSpread = .25f;
inline constexpr uint32_t hotAt = seamAt + seamCells * static_cast<uint32_t>(seamWords);           // word 258
inline constexpr uint32_t sampleCountAt = hotAt + static_cast<uint32_t>(seamAngles) * hotWords;   // word 318
inline constexpr uint32_t samplesAt = sampleCountAt + 1;                                           // word 319
inline constexpr uint32_t grazeBytesPerEye = 2048; // bins 552 B, seam cells 480, hot edges 240, samples 4 + 256
static_assert(seamAt * 4 == 552 && hotAt == 258 && sampleCountAt == 318 &&
              (samplesAt + edgeSamples * sampleWords) * 4 <= grazeBytesPerEye);

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
struct Seam {
    uint32_t pixels{};
    float inSum{}, outSum{};             // pre-TAA change and shown change (luma 0..1), summed
    uint32_t inFlicker{}, outFlicker{};  // over flickerLuma
};
static_assert(sizeof(Seam) == 20);
struct Hot {
    uint32_t pixels{}, ageSum{}, ageMax{}, clipOnly{}, depthOnly{}, both{}, neither{}, bright{}, outFlicker{};
    float weightSum{}, spreadSum{}, sigmaSum{}, depthSpreadSum{}, inSum{}, outSum{};
};
static_assert(sizeof(Hot) == 60);
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
    uint32_t seamSetting{};   // 0.0.76 SEAM TRACE: live.txt vr_taa_seam at the window's last pair (0 off, 1 on)
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
    // 0.0.76 SEAM TRACE: the seam cells per movement category; seamReads the eyes whose cells came back, seamPairs
    // the pairs traced with the depth off (vr_taa_depth 0, vr_taa_seam on: the picture as without it).
    uint32_t seamReads{}, seamPairs{};
    Seam seam[3][seamCells];
    // 0.0.76 EDGE TRACE: the hot edges per movement category and SeamAngle; the window's last fast (else moving, else
    // still) pass's samples of eye 0 (sampleCount of them, sampleCategory its category).
    Hot hot[3][seamAngles];
    uint32_t sampleCount{}, sampleCategory{};
    uint32_t sample[edgeSamples][sampleWords]{};
};
static_assert(sizeof(Window) == 4 * 35 + 3 * 36 + 18 * 92 + 8 + 72 * 20 + 12 * 60 + 8 + 256); // 4336
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
    uint32_t version = 2; // 0.0.76 SEAM TRACE: the seam cells
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
// 0.0.76 SEAM TRACE: one eye's seam cells (seamCells x seamWords after the bins, this pass's alone) in the pair's
// movement category. Not one pass's words: none of them.
inline void noteSeam(Window& w, uint32_t category, const uint32_t* words) {
    if (category >= 3)
        return;
    const uint32_t* cells = words + seamAt;
    for (uint32_t c = 0; c < seamCells; ++c) {
        const uint32_t* s = cells + static_cast<uint32_t>(seamWords) * c;
        if (s[sInFlicker] > s[sPixels] || s[sOutFlicker] > s[sPixels] || s[sPixels] > (1u << 20))
            return;
    }
    const uint32_t* hot = words + hotAt;
    for (uint32_t a = 0; a < seamAngles; ++a) {
        const uint32_t* h = hot + static_cast<uint32_t>(hotWords) * a;
        if (h[hClipOnly] + h[hDepthOnly] + h[hBoth] + h[hNeither] != h[hPixels] || h[hBright] > h[hPixels] ||
            h[hOutFlicker] > h[hPixels])
            return;
    }
    ++w.seamReads;
    for (uint32_t c = 0; c < seamCells; ++c) {
        const uint32_t* s = cells + static_cast<uint32_t>(seamWords) * c;
        auto& o = w.seam[category][c];
        o.pixels += s[sPixels];
        o.inSum += static_cast<float>(s[sInMilli]) / 1000.f;
        o.outSum += static_cast<float>(s[sOutMilli]) / 1000.f;
        o.inFlicker += s[sInFlicker];
        o.outFlicker += s[sOutFlicker];
    }
    for (uint32_t a = 0; a < seamAngles; ++a) {
        const uint32_t* h = hot + static_cast<uint32_t>(hotWords) * a;
        auto& o = w.hot[category][a];
        o.pixels += h[hPixels];
        o.ageSum += h[hAgeSum];
        o.ageMax = h[hAgeMax] > o.ageMax ? h[hAgeMax] : o.ageMax;
        o.clipOnly += h[hClipOnly];
        o.depthOnly += h[hDepthOnly];
        o.both += h[hBoth];
        o.neither += h[hNeither];
        o.bright += h[hBright];
        o.outFlicker += h[hOutFlicker];
        o.weightSum += static_cast<float>(h[hWeightMilli]) / 1000.f;
        o.spreadSum += static_cast<float>(h[hSpreadMilli]) / 1000.f;
        o.sigmaSum += static_cast<float>(h[hSigmaMilli]) / 1000.f;
        o.depthSpreadSum += static_cast<float>(h[hDepthSpreadMilli]) / 1000.f;
        o.inSum += static_cast<float>(h[hInMilli]) / 1000.f;
        o.outSum += static_cast<float>(h[hOutMilli]) / 1000.f;
    }
}
// 0.0.76 EDGE TRACE: eye 0's samples of a pass kept when its category is at least the kept one's (fast over moving
// over still): the window shows the fastest swing's edges.
inline void noteSamples(Window& w, uint32_t category, const uint32_t* words) {
    if (category >= 3 || (w.sampleCount && category < w.sampleCategory))
        return;
    uint32_t n = words[sampleCountAt];
    n = n > edgeSamples ? edgeSamples : n;
    if (!n)
        return;
    w.sampleCount = n;
    w.sampleCategory = category;
    for (uint32_t k = 0; k < edgeSamples; ++k)
        for (uint32_t j = 0; j < sampleWords; ++j)
            w.sample[k][j] = k < n ? words[samplesAt + sampleWords * k + j] : 0;
}
} // namespace spidy::taa_depth_trace
