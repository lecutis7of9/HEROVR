#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
//
// Update 33: SHADOW DISTANCE (GRAPHICS page). The sun's hard shadows end a few metres
// from the hero because the game's sun cascades are small boxes (run32's ORTHO CAPTURE:
// about 24.6 x 24.6 x 131 m, 28.9 x 28.9 x 154 m, 56.2 x 56.2 x 300 m half sizes, the
// depth about 5.33 times the width). Nobody knows yet which CPU value sizes them, so the
// slider starts locked. After ADDRESS FINDER starts, a read-only search of this session's
// memory looks for floats or doubles equal to (or in the same proportions as) the box
// sizes it sees live, keeps the ones that hold still, and only then unlocks the slider.
// Moving it writes a trial value through an SEH helper and watches the next frames'
// sun boxes: they must scale with it, else the value is put back (NO EFFECT); a fault,
// vanished boxes or out-of-range boxes put it back with the reason on the hologram.
// Update 34: the search first looks for the game's EnvLightingDef objects (the reflection
// fields are confirmed in the exe: RegLodRange +0x6c, TotalShadowRange +0x78, ...; the
// heap instances are candidates until a change moves the boxes). Changes only grow the
// reach; each trial is checked against the box the candidate matched, with a tolerance
// relative to the change asked; the widest-box mix no longer puts a change back; and only
// heap or writable module data is ever written (Update 33 could write a code constant).
// Everything here is pure (no Windows): the box test, the targets, the verdicts, texts.
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

namespace spidy::shadow_distance {

inline constexpr float carMetres = 4.5f;   // one car length
inline constexpr int maxCars = 200;        // the slider's top: 200 car lengths, 900 m (Update 34.2; was 100)
inline constexpr float maxMetres = carMetres * maxCars;
inline constexpr float lowestScale = 1.f; // Update 34: growth only (was a quarter: run33 asked for 0.89x)

// A sun cascade box from an orthographic matrix (either layout, as address_finder::orthoBox)
// with sizes it can reach after a change (half sizes from 0.25 m: a quarter of the smallest
// cascade is still found). Square across the light (within 1%), deeper than wide (2-80x),
// and lit from a slant (the light axis is not a world axis: those are probes and decals).
struct Box {
    float half[3]{};  // across, across, along the light (metres)
    float light[3]{}; // unit direction of the depth axis in the world
};
inline bool sunBox(const float* m, Box& out) {
    for (int i = 0; i < 16; ++i)
        if (!std::isfinite(m[i]))
            return false;
    const bool rowForm = m[3] == 0.f && m[7] == 0.f && m[11] == 0.f && m[15] == 1.f;
    const bool colForm = m[12] == 0.f && m[13] == 0.f && m[14] == 0.f && m[15] == 1.f;
    if (rowForm == colForm)
        return false;
    float axis[3][3]{};
    for (int a = 0; a < 3; ++a) {
        const float x = rowForm ? m[a] : m[a * 4];
        const float y = rowForm ? m[4 + a] : m[a * 4 + 1];
        const float z = rowForm ? m[8 + a] : m[a * 4 + 2];
        const float len = std::sqrt(x * x + y * y + z * z);
        if (!(len > 0.f) || !std::isfinite(len))
            return false;
        const float half = 1.f / len;
        if (!(half >= .25f && half <= 20000.f))
            return false;
        out.half[a] = half;
        axis[a][0] = x / len;
        axis[a][1] = y / len;
        axis[a][2] = z / len;
    }
    const float w = std::max(out.half[0], out.half[1]);
    if (std::fabs(out.half[0] - out.half[1]) > .01f * w)
        return false;
    const float ratio = out.half[2] / w;
    if (!(ratio >= 2.f && ratio <= 80.f))
        return false;
    for (int i = 0; i < 3; ++i)
        out.light[i] = axis[2][i];
    const float most = std::max({std::fabs(axis[2][0]), std::fabs(axis[2][1]), std::fabs(axis[2][2])});
    return most < .99f;
}

// The sizes seen lately: up to 8 kinds of box (within 1% across and deep), each with when
// it was last seen. reach() is the widest box seen since a moment: the sun shadows' extent.
struct Seen {
    float half{}, depth{};
    uint64_t firstMs{}, lastMs{};
    uint32_t hits{};
};
inline constexpr unsigned maxSeen = 8;
struct Watch {
    Seen kinds[maxSeen]{};
    unsigned n{};
    uint64_t boxes{}, lastMs{};
    void note(float half, float depth, uint64_t ms) {
        ++boxes;
        lastMs = std::max(lastMs, ms);
        for (unsigned i = 0; i < n; ++i) {
            Seen& s = kinds[i];
            if (std::fabs(s.half - half) <= .01f * std::max(s.half, half) &&
                std::fabs(s.depth - depth) <= .01f * std::max(s.depth, depth)) {
                s.lastMs = std::max(s.lastMs, ms);
                ++s.hits;
                return;
            }
        }
        unsigned slot = n < maxSeen ? n++ : 0;
        if (slot == 0 && n == maxSeen) // full: the one seen longest ago goes
            for (unsigned i = 1; i < maxSeen; ++i)
                if (kinds[i].lastMs < kinds[slot].lastMs)
                    slot = i;
        kinds[slot] = Seen{half, depth, ms, ms, 1};
    }
    // Widest box seen at or after sinceMs (0 if none), and how many kinds that was.
    float reach(uint64_t sinceMs, unsigned* kindsOut = nullptr) const {
        float r = 0;
        unsigned k = 0;
        for (unsigned i = 0; i < n; ++i)
            if (kinds[i].lastMs >= sinceMs) {
                r = std::max(r, kinds[i].half);
                ++k;
            }
        if (kindsOut)
            *kindsOut = k;
        return r;
    }
};

// What the memory search looks for: each live box's half size, double and half of it,
// and its depth likewise (a setting may be a width, a half width or the box's depth).
enum TargetKind : uint8_t { tHalf = 0, tWidth = 1, tHalfOfHalf = 2, tDepth = 3, tFullDepth = 4, tHalfDepth = 5 };
inline constexpr unsigned targetKinds = 6;
inline constexpr unsigned maxTargets = maxSeen * targetKinds;
struct Targets {
    float value[maxTargets]{};
    uint8_t kind[maxTargets]{}, box[maxTargets]{};
    unsigned n{};
    float sizes[maxSeen]{}; // the distinct half sizes, ascending (for proportional triples)
    unsigned sizeN{};
};
inline Targets targetsFrom(const Watch& w, uint64_t sinceMs) {
    Targets t;
    for (unsigned i = 0; i < w.n; ++i) {
        const Seen& s = w.kinds[i];
        if (s.lastMs < sinceMs || s.hits < 2)
            continue;
        const float v[targetKinds]{s.half, s.half * 2, s.half / 2, s.depth, s.depth * 2, s.depth / 2};
        for (unsigned k = 0; k < targetKinds && t.n < maxTargets; ++k) {
            t.value[t.n] = v[k];
            t.kind[t.n] = static_cast<uint8_t>(k);
            t.box[t.n] = static_cast<uint8_t>(i);
            ++t.n;
        }
        bool dup = false;
        for (unsigned j = 0; j < t.sizeN; ++j)
            dup = dup || std::fabs(t.sizes[j] - s.half) <= .01f * s.half;
        if (!dup && t.sizeN < maxSeen)
            t.sizes[t.sizeN++] = s.half;
    }
    std::sort(t.sizes, t.sizes + t.sizeN);
    return t;
}
inline constexpr double matchTolerance = 5e-4; // relative
inline bool within(double x, double target, double tol = matchTolerance) {
    return std::isfinite(x) && target > 0 && std::fabs(x - target) <= tol * target;
}
// The target x equals, or -1.
inline int matchTarget(double x, const Targets& t) {
    if (!(x > 0) || !std::isfinite(x))
        return -1;
    for (unsigned i = 0; i < t.n; ++i)
        if (within(x, t.value[i]))
            return static_cast<int>(i);
    return -1;
}
// Three ascending values in the same proportions as three of the live sizes (within 0.5%):
// cascade splits kept as distances rather than sizes. Not the sizes themselves (those are
// matchTarget's) and not all equal.
inline bool tripleMatch(double a, double b, double c, const Targets& t) {
    if (!(a > .1 && b > a * 1.01 && c > b * 1.01) || !std::isfinite(c) || c > 1e5 || t.sizeN < 3)
        return false;
    for (unsigned i = 0; i + 2 < t.sizeN; ++i)
        for (unsigned j = i + 1; j + 1 < t.sizeN; ++j)
            for (unsigned k = j + 1; k < t.sizeN; ++k) {
                const double r1 = t.sizes[j] / t.sizes[i], r2 = t.sizes[k] / t.sizes[i];
                if (within(b / a, r1, 5e-3) && within(c / a, r2, 5e-3))
                    return true;
            }
    return false;
}

// The slider: 0 is the game's own reach (no change); n is n car lengths, held between a
// quarter of the game's reach and maxCars.
inline float metresFor(int cars, float gameReach) {
    if (cars <= 0 || !(gameReach > 0))
        return gameReach > 0 ? gameReach : 0.f;
    const float want = static_cast<float>(std::min(cars, maxCars)) * carMetres;
    return std::clamp(want, gameReach * lowestScale, std::max(maxMetres, gameReach));
}
inline float scaleFor(int cars, float gameReach) {
    return gameReach > 0 ? metresFor(cars, gameReach) / gameReach : 1.f;
}
inline int carsOf(float metres) {
    return std::isfinite(metres) && metres > 0 ? static_cast<int>(std::lround(metres / carMetres)) : 0;
}

// Did the boxes follow? before: the reach with the game's value; after: with the change;
// scale: what was asked. Scaled: within 15% of asked, or at least half of the asked change
// (in log terms) the same way (the game may clamp). OutOfRange: not finite or over 10x
// the asked size. NoChange: anything else.
enum Verdict : uint8_t { scaled = 0, noChange = 1, outOfRange = 2 };
inline Verdict verify(float before, float after, float scale) {
    if (!(before > 0) || !std::isfinite(after) || !(after > 0) || !std::isfinite(scale) || !(scale > 0))
        return outOfRange;
    const float asked = before * scale;
    if (after > asked * 10.f || after > 20000.f || after < asked / 10.f)
        return outOfRange;
    if (std::fabs(after - asked) <= .15f * asked && std::fabs(scale - 1.f) > .02f)
        return scaled;
    const double want = std::log(static_cast<double>(scale)), got = std::log(static_cast<double>(after / before));
    if (std::fabs(want) > .05 && want * got > 0 && std::fabs(got) >= .5 * std::fabs(want))
        return scaled;
    return noChange;
}

// Update 34: the sizes seen in a window (each kind's half size and depth), the verdict
// against the one box a candidate matched, and against all boxes for an EnvLightingDef.
struct Sizes {
    float half[maxSeen]{}, depth[maxSeen]{};
    unsigned n{};
};
inline Sizes sizesSince(const Watch& w, uint64_t sinceMs, uint64_t firstAfterMs = 0) {
    Sizes s;
    for (unsigned i = 0; i < w.n && s.n < maxSeen; ++i)
        if (w.kinds[i].lastMs >= sinceMs && w.kinds[i].lastMs >= firstAfterMs) {
            s.half[s.n] = w.kinds[i].half;
            s.depth[s.n] = w.kinds[i].depth;
            ++s.n;
        }
    return s;
}
// The tolerance for "the box is now at m * scale": a quarter of the asked change, at least 3%.
inline float trialTolerance(float scale) { return std::max(.03f, .25f * std::fabs(scale - 1.f)); }
inline bool sizeNear(const Sizes& s, bool depth, float want, float tol) {
    for (unsigned i = 0; i < s.n; ++i) {
        const float v = depth ? s.depth[i] : s.half[i];
        if (std::isfinite(v) && std::fabs(v - want) <= tol * want)
            return true;
    }
    return false;
}
// What box dimension a match was against (the box's half size or depth) and its size.
inline bool targetIsDepth(uint8_t kind) { return kind == tDepth || kind == tFullDepth || kind == tHalfDepth; }
inline float targetBoxSize(uint8_t kind, float value) {
    switch (kind) {
    case tWidth: case tFullDepth: return value / 2;
    case tHalfOfHalf: case tHalfDepth: return value * 2;
    default: return value;
    }
}
// The trial grows the matched box by 1.5, 2.5 or 1.3 times: the first where no box of that
// size was there before (else a growth could not be told from another cascade).
inline float pickTrialScale(const Sizes& before, bool depth, float matched) {
    for (const float s : {1.5f, 2.5f, 1.3f})
        if (!sizeNear(before, depth, matched * s, trialTolerance(s)))
            return s;
    return 0;
}
// Scaled: a box at matched * scale (relative tolerance) after, none before. outOfRange:
// any box not finite. noChange otherwise. Growth only (scale > 1).
inline Verdict verifyMatched(const Sizes& before, const Sizes& after, bool depth, float matched, float scale) {
    if (!(scale > 1.f) || !(matched > 0) || !std::isfinite(scale))
        return noChange;
    for (unsigned i = 0; i < after.n; ++i)
        if (!std::isfinite(after.half[i]) || !std::isfinite(after.depth[i]))
            return outOfRange;
    const float tol = trialTolerance(scale), want = matched * scale;
    return sizeNear(after, depth, want, tol) && !sizeNear(before, depth, want, tol) ? scaled : noChange;
}
// For a change to every cascade (EnvLightingDef): at least a third of the boxes seen before
// (and one) show up grown by the scale, where no box of that size was before.
inline Verdict verifyAll(const Sizes& before, const Sizes& after, float scale) {
    if (!(scale > 1.f) || !std::isfinite(scale) || !before.n)
        return noChange;
    for (unsigned i = 0; i < after.n; ++i)
        if (!std::isfinite(after.half[i]) || !std::isfinite(after.depth[i]))
            return outOfRange;
    const float tol = trialTolerance(scale);
    unsigned grown = 0, tried = 0;
    for (unsigned i = 0; i < before.n; ++i) {
        const float want = before.half[i] * scale;
        if (sizeNear(before, false, want, tol))
            continue; // that size was already there: cannot tell
        ++tried;
        grown += sizeNear(after, false, want, tol) ? 1u : 0u;
    }
    return tried && grown >= 1 && grown * 3 >= tried ? scaled : noChange;
}
// The scale a slider value asks of a reach: growth only (1 means nothing to change).
inline float growthFor(int cars, float gameReach) {
    if (cars <= 0 || !(gameReach > 0))
        return 1.f;
    return std::max(1.f, std::min(static_cast<float>(cars), static_cast<float>(maxCars)) * carMetres / gameReach);
}

// EnvLightingDef (reflection type in the exe, size 0x1c0; field offsets confirmed from its
// field table). A heap block is taken for one when every field is in a sane range.
inline constexpr uint32_t envSize = 0x1c0;
inline constexpr uint32_t envRegLodRange = 0x6c, envRegLodCount = 0x70, envFarLodCount = 0x71,
                          envPerfModeOpts = 0x72, envShadowSoftness = 0x74, envTotalShadowRange = 0x78,
                          envTimeOfDay = 0x7c, envSunDiameter = 0x80, envEnabled = 0x20, envAzimuth = 0x5c,
                          envElevation = 0x60, envCasterLower = 0x150, envCasterUpper = 0x154;
struct EnvValues {
    float regLodRange{}, totalShadowRange{}, softness{}, timeOfDay{}, casterLower{}, casterUpper{};
    uint8_t regLodCount{}, farLodCount{};
};
inline float envF32(const uint8_t* p, uint32_t at) {
    float f;
    std::memcpy(&f, p + at, 4);
    return f;
}
inline bool envLooks(const uint8_t* p, EnvValues* out = nullptr) {
    const uint8_t reg = p[envRegLodCount], farN = p[envFarLodCount], perf = p[envPerfModeOpts], on = p[envEnabled];
    if (reg < 1 || reg > 16 || farN > 16 || perf > 1 || on > 1)
        return false;
    const float range = envF32(p, envRegLodRange), total = envF32(p, envTotalShadowRange);
    if (!(range > 1.f && range < 5000.f) || !(total >= range && total <= 100000.f))
        return false;
    const float az = envF32(p, envAzimuth), el = envF32(p, envElevation), soft = envF32(p, envShadowSoftness),
                tod = envF32(p, envTimeOfDay), sun = envF32(p, envSunDiameter), lo = envF32(p, envCasterLower),
                hi = envF32(p, envCasterUpper);
    if (!(az >= -720.f && az <= 720.f) || !(el >= -90.f && el <= 90.f) || !(soft >= 0.f && soft <= 1000.f) ||
        !(tod >= 0.f && tod <= 24.f) || !(sun >= 0.f && sun <= 90.f) || !std::isfinite(lo) || !std::isfinite(hi) ||
        !(lo <= hi))
        return false;
    if (out)
        *out = EnvValues{range, total, soft, tod, lo, hi, reg, farN};
    return true;
}

// State shown on the slider; reason says why it was put back or not found.
enum State : uint32_t {
    idle = 0,      // ADDRESS FINDER not run yet this session
    searching = 1, // the read-only search is running
    ready = 2,     // a value is found; the slider works (nothing written yet, or put back to the game's)
    testing = 3,   // a change is being checked against the boxes
    applied = 4,   // a change is in and the boxes follow it
    noEffect = 5,  // every value found was tried; none moved the boxes; all put back
    reverted = 6,  // an anomaly put it back for good this session
    notFound = 7,  // the search ended with nothing usable
};
inline constexpr uint32_t states = 8;
enum Reason : uint32_t {
    none = 0,
    writeFault = 1,     // the memory write faulted
    boxesStopped = 2,   // sun shadow boxes stopped coming after the change
    boxesWild = 3,      // the boxes went out of range
    gameReset = 4,      // the game put its own value back
    valueMoved = 5,     // the value changed under the change
    noSun = 6,          // no sun shadow boxes seen
    noMatch = 7,        // nothing in memory matched the sizes
    unsteady = 8,       // what matched did not hold still
    searchFault = 9,    // the search itself failed
    noHooks = 10,       // ADDRESS FINDER's upload hook is not in (it failed)
    noSunNow = 11,      // a change was asked with no sun boxes in view
    ended = 12,         // VR ended; put back
    stopped = 13,       // a value that worked stopped moving the boxes
    alreadyFarther = 14, // the game already reaches farther than asked (only growth is written)
};
inline constexpr uint32_t reasons = 15;
inline const char* reasonText(uint32_t r) {
    switch (r) {
    case writeFault: return "the memory write faulted";
    case boxesStopped: return "sun shadows stopped coming after the change";
    case boxesWild: return "the shadow boxes went out of range";
    case gameReset: return "the game set its own value again";
    case valueMoved: return "the value changed under it";
    case noSun: return "no sun shadows seen; play outdoors by day";
    case noMatch: return "no value in memory matched the shadow size";
    case unsteady: return "the values found did not hold still";
    case searchFault: return "the memory search faulted";
    case noHooks: return "ADDRESS FINDER's upload hook is off";
    case noSunNow: return "no sun shadows in view; try outdoors";
    case ended: return "VR ended";
    case stopped: return "the shadows stopped following the value";
    case alreadyFarther: return "the game already reaches that far";
    default: return "unknown";
    }
}
inline uint32_t pack(uint32_t state, uint32_t reason) { return (state & 0xff) | (reason & 0xff) << 8; }
inline uint32_t stateOf(uint32_t packed) { return packed & 0xff; }
inline uint32_t reasonOf(uint32_t packed) { return packed >> 8 & 0xff; }
// The slider moves only when a found value can take a change.
inline bool sliderLive(uint32_t packed) {
    const uint32_t s = stateOf(packed);
    return s == ready || s == testing || s == applied;
}
// Update 34.2: the slider takes a value from the search's start too, kept and applied the
// moment it unlocks (run341: moved while SEARCHING, it was thrown away; nothing was tried).
inline bool sliderMoves(uint32_t packed) { return sliderLive(packed) || stateOf(packed) == searching; }

// The hologram's number: the distance asked, in metres and car lengths, or the state.
inline std::string valueText(uint32_t packed, float cars, float gameReach) {
    const uint32_t s = stateOf(packed);
    if (s == idle || s == notFound)
        return "NOT FOUND";
    if (s == searching)
        return "SEARCHING";
    if (s == noEffect)
        return "NO EFFECT";
    if (s == reverted)
        return "PUT BACK";
    const int c = std::isfinite(cars) ? static_cast<int>(std::lround(cars)) : 0;
    const float m = metresFor(c, gameReach);
    char buf[32];
    std::snprintf(buf, sizeof buf, "%s%d M, %d CARS", c <= 0 ? "GAME " : "", static_cast<int>(std::lround(m)),
                  carsOf(m));
    return buf;
}
inline const char* widestValue() { return "GAME 900 M, 200 CARS"; } // for the layout

// One sentence of at most 80 characters under the slider.
inline constexpr const char* help = "Sun shadow reach; set it after ADDRESS FINDER starts, it applies when found.";
inline std::string helpText(uint32_t packed, float liveMetres, float gameReach = 0) {
    const uint32_t s = stateOf(packed), r = reasonOf(packed);
    char buf[128];
    switch (s) {
    case idle:
        return "Not found yet: press ADDRESS FINDER, then play outdoors in daylight.";
    case searching:
        return r == noSun ? "Searching: no sun shadows seen yet; play outdoors in daylight."
                          : "Searching, read only: set it now; it applies once the value is found.";
    case ready:
        if (r == alreadyFarther && gameReach > 0) // Update 34.2: say how far to go
            std::snprintf(buf, sizeof buf, "Game reaches %d m: go past %d cars for longer shadows.",
                          static_cast<int>(std::lround(gameReach)), carsOf(gameReach));
        else if (r)
            std::snprintf(buf, sizeof buf, "Not changed: %s.", reasonText(r));
        else
            return "Farther shows more shadow, but each shadow pixel covers more ground.";
        return buf;
    case testing:
        return "Testing: shadows may jump for a moment while each found value is checked.";
    case applied:
        if (std::isfinite(liveMetres) && liveMetres > 0)
            std::snprintf(buf, sizeof buf, "Now %d m (%d cars); farther means coarser, blurrier shadows.",
                          static_cast<int>(std::lround(liveMetres)), carsOf(liveMetres));
        else
            return "Applied; farther means coarser, blurrier shadows.";
        return buf;
    case noEffect:
        return "No effect: no value found moved the sun's shadows, so all were put back.";
    case reverted:
        std::snprintf(buf, sizeof buf, "Put back: %s.", reasonText(r));
        return buf;
    case notFound:
    default:
        std::snprintf(buf, sizeof buf, "Not found: %s.", reasonText(r ? r : noMatch));
        return buf;
    }
}

// Shared with tools/shadow_distance.py (SpidyShadowDistanceData).
inline constexpr uint32_t dataMagic = 0x54534453; // SDST
inline constexpr uint32_t dataVersion = 2; // 2: Update 34 (trial and EnvLightingDef records)
inline constexpr uint32_t maxCandidates = 16, maxEvents = 32;
enum Origin : uint8_t { fromCode = 0, fromModule = 1, fromHeap = 2, fromEnv = 3 };
// envPair: an EnvLightingDef's RegLodRange and TotalShadowRange (two floats 0xc apart), scaled together.
enum Form : uint8_t { f32 = 0, f64 = 1, triple32 = 2, triple64 = 3, envPair = 4 };
inline constexpr uint8_t targetProportions = 0xff, targetAll = 0xfe; // Candidate::target besides a TargetKind
enum Outcome : uint8_t { untried = 0, worked = 1, noMove = 2, moved = 3, faulted = 4, dropped = 5 };
struct Candidate {
    uint64_t address{};
    uint32_t rva{};         // module offset, or UINT32_MAX for heap
    uint8_t origin{}, form{}, target{}, outcome{}; // Origin, Form, TargetKind (0xff triple), Outcome
    uint16_t stride{}, confirms{};                  // triple stride in bytes; re-reads that matched
    float value[3]{};
};
static_assert(sizeof(Candidate) == 32);
struct Event {
    uint64_t ms{};
    uint32_t kind{}, code{}; // kind: 1 state change, 2 trial, 3 apply, 4 revert, 5 search step
    float a{}, b{};
};
static_assert(sizeof(Event) == 24);
inline constexpr uint32_t maxTrials = 8, maxEnv = 8;
struct Trial {
    uint32_t candidate{};          // index, or UINT32_MAX for a group of candidates
    float scale{}, matched{};      // asked scale; the matched box size (0 for all boxes)
    uint8_t verdict{}, nBefore{}, nAfter{}, depth{}; // Verdict; box kinds before / after; matched a depth
    float before[maxSeen]{}, after[maxSeen]{};       // box half sizes (depths if depth)
};
static_assert(sizeof(Trial) == 80);
struct Env {
    uint64_t address{};
    float regLodRange{}, totalShadowRange{}, casterLower{}, casterUpper{};
    uint8_t regLodCount{}, farLodCount{}, pad[6]{};
};
static_assert(sizeof(Env) == 32);
struct Data {
    uint32_t magic = dataMagic, version = dataVersion, bytes = 0, packed = 0;
    int64_t sequence{};          // odd while written
    float gameReach{}, liveReach{}, wantCars{}, scale{};
    uint32_t sizes{}, candidates{}, stable{}, trials{};
    uint64_t scannedBytes{}, regions{}, rawHits{}, boxes{};
    float sizeHalf[maxSeen]{}, sizeDepth[maxSeen]{};
    uint32_t sites[4]{};         // upload call sites (module offsets) the sun boxes came from
    uint32_t active{}, events{}; // index of the candidate in use (UINT32_MAX none); events written
    Candidate candidate[maxCandidates]{};
    Event event[maxEvents]{};    // ring: event[i % maxEvents]
    uint32_t trialsLogged{}, envFound{};
    Trial trial[maxTrials]{};    // ring: trial[i % maxTrials], the boxes before and after each trial
    Env env[maxEnv]{};           // the first EnvLightingDef objects found (their game values)
};
static_assert(offsetof(Data, candidate) == 176 && offsetof(Data, event) == 688 && offsetof(Data, trial) == 1464 &&
                  offsetof(Data, env) == 2104 && sizeof(Data) == 2360,
              "tools/shadow_distance.py reads this layout");

} // namespace spidy::shadow_distance
