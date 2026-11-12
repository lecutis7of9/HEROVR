#pragma once
// 79.4 (t2188): EYE JITTER's levels and JITTER CHECK, the record of what the eyes' lens stored and what the game's
// TemporalAa jobs actually read.
//
// Levels (live.txt vr_jitter = off | low | med | full, 'on' = full; e5d2746 hidden, live.txt only). The amplitude is
// the largest offset the eye's lens stores, in eye pixels, over the Halton (2, 3) walk of 8 phases: the lens stores
// twice the jitter it is given (+0x428 = 2x, +0x42c = -2y), the walk given is 2 * scale * (h - 0.5) / size NDC, so the
// stored offset is 2 * scale * (h - 0.5) pixels, at most 0.875 * scale (h - 0.5 is 0.4375 at most over 8 phases).
//   off   one fixed 1e-6 NDC (about a thousandth of a pixel; the game's TAA skips a zero jitter)
//   low   scale 0.115 -> about 0.10 px      med  scale 0.25 -> about 0.22 px
//   full  scale 0.5   -> about 0.44 px (the halved walk of 0.0.79.1 t2165)
//   e5d2746 scale 1.0 -> about 0.88 px (the walk of the e5d2746 test build; far buildings shook)
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>

namespace spidy::jitter_check {
enum Level : uint32_t { off, low, med, full, e5d2746, levels };
inline constexpr uint32_t defaultLevel = med;
inline constexpr float scales[levels] = {0.f, 0.115f, 0.25f, 0.5f, 1.f};
inline constexpr const char* names[levels] = {"OFF", "LOW", "MED", "FULL", "E5D2746"};
// The holo row's value: the level and its largest stored offset.
inline constexpr const char* rowText[levels] = {"OFF", "LOW 0.10PX", "MED 0.22PX", "FULL 0.44PX", "OLD 0.88PX"};
inline constexpr float maxPhase = 0.4375f; // the largest |h - 0.5| of the 8 Halton (2, 3) phases
inline constexpr float maxStoredPixels(uint32_t level) { return level < levels ? 2.f * scales[level] * maxPhase : 0.f; }
// The hologram's press: OFF > LOW > MED > FULL > OFF (the hidden e5d2746 goes to OFF).
inline constexpr uint32_t nextLevel(uint32_t level) { return level >= full ? off : level + 1; }
// live.txt's / the exports' number for a level: 0 off .. 4 e5d2746; the old on / off numbers (SpidyVrJitter 0 on, 1 off).
inline constexpr uint32_t levelFromLegacy(uint32_t legacy) { return legacy ? off : full; }

inline constexpr uint32_t magic = 0x4b434a37; // 7JCK
inline constexpr uint32_t version = 1;
inline constexpr uint32_t ringSize = 16, otherJobs = 8, changeCapacity = 16, lags = 16;
// A job's jitter pair against what the lens was given (sent) and stored (made = 2x, -2y of sent).
enum Match : uint32_t { none, lens, twice, once };
inline bool almost(float a, float b) { return std::fabs(a - b) <= 1e-9f + 0.02f * std::fabs(b); }
inline Match classify(const float job[2], const float sent[2], const float made[2]) {
    if (almost(job[0], made[0]) && almost(job[1], made[1]))
        return lens;
    if (almost(std::fabs(job[0]), 2 * std::fabs(sent[0])) && almost(std::fabs(job[1]), 2 * std::fabs(sent[1])))
        return twice;
    if (almost(std::fabs(job[0]), std::fabs(sent[0])) && almost(std::fabs(job[1]), std::fabs(sent[1])))
        return once;
    return none;
}
struct Change {
    uint64_t tickMs{}; // GetTickCount64 when it took effect
    uint32_t level{}, source{}; // source: 0 start, 1 live.txt / the launcher, 2 the hologram
};
struct Eye {
    uint64_t lens{};     // lens calls given the eye's jitter
    uint64_t lensRule{}; // ... whose stored pair is (2x, -2y) of what was given
    uint64_t runs{};     // TemporalAa runs of the eye's job
    uint64_t jobZero{};  // ... with a zero jitter in the job
    uint64_t matchLens{}, matchTwice{}, matchOnce{}, matchNone{}; // ... the job's jitter against the last 16 frames' lens
    uint64_t lag[lags]{}; // frames back at which a run's job jitter matched the lens (lens match)
    float sent[2]{}, made[2]{}, job[2]{};
    float maxSent{}, maxJob{}, lrDiffMax{}; // largest |component| given / in a job; largest left-right job difference
    uint32_t reserved{};
};
struct Other { // a TemporalAa job that is not an eye's (the game's own views, any other layer)
    uint64_t job{}, runs{};
    float jitter[2]{};
    uint32_t aa{}, reserved{};
};
struct Data {
    uint32_t magic = jitter_check::magic, version = jitter_check::version, bytes = 1024, level = defaultLevel;
    uint32_t changeCount{}, otherCount{}, otherDropped{}, reserved{};
    uint64_t startTickMs{}, lastTickMs{};
    Eye eyes[2]{};
    Other others[otherJobs]{};
    Change changes[changeCapacity]{};
};
static_assert(sizeof(Eye) == 232, "jitter_check::Eye layout (tools/jitter_check_record.py)");
static_assert(sizeof(Other) == 32 && sizeof(Change) == 16);
static_assert(sizeof(Data) == 1024, "SpidyJitterCheckData is 1024 bytes (tools/jitter_check_record.py)");
// The console's 79.4 JITTER CHECK line: the level, each eye's lens jitter given / stored (the lens rule 2x, -2y), what its
// TemporalAa job read (the share equal to the stored pair, to twice or once what was given, or neither), the left-right
// difference of the jobs and the other TemporalAa jobs (the game's own views and any other layer).
inline std::string summaryLine(const Data& d) {
    char out[900];
    int n = std::snprintf(out, sizeof out, "79.4 JITTER CHECK: level %s (up to %.2f px)", names[d.level < levels ? d.level : 0],
                          maxStoredPixels(d.level));
    for (unsigned i = 0; i < 2 && n > 0 && n < static_cast<int>(sizeof out) - 200; ++i) {
        const auto& e = d.eyes[i];
        const double runs = e.runs ? static_cast<double>(e.runs) : 1.0;
        n += std::snprintf(out + n, sizeof out - n,
                           "; %s lens given %.2e/%.2e stored %.2e/%.2e (rule 2x,-2y %llu/%llu), job read %.2e/%.2e: = stored %.0f%%, "
                           "2x given %.0f%%, 1x %.0f%%, neither %.0f%%, zero %llu of %llu runs",
                           i ? "right" : "left", e.sent[0], e.sent[1], e.made[0], e.made[1],
                           static_cast<unsigned long long>(e.lensRule), static_cast<unsigned long long>(e.lens), e.job[0], e.job[1],
                           100 * e.matchLens / runs, 100 * e.matchTwice / runs, 100 * e.matchOnce / runs, 100 * e.matchNone / runs,
                           static_cast<unsigned long long>(e.jobZero), static_cast<unsigned long long>(e.runs));
    }
    if (n > 0 && n < static_cast<int>(sizeof out) - 120) {
        n += std::snprintf(out + n, sizeof out - n, "; left-right job difference max %.2e; other TemporalAa jobs %u", d.eyes[0].lrDiffMax > d.eyes[1].lrDiffMax ? d.eyes[0].lrDiffMax : d.eyes[1].lrDiffMax,
                           d.otherCount);
        for (uint32_t k = 0; k < d.otherCount && k < 3 && n > 0 && n < static_cast<int>(sizeof out) - 100; ++k)
            n += std::snprintf(out + n, sizeof out - n, " [#%u %llu runs, jitter %.2e/%.2e, aa %u]", k,
                               static_cast<unsigned long long>(d.others[k].runs), d.others[k].jitter[0], d.others[k].jitter[1], d.others[k].aa);
    }
    return out;
}
} // namespace spidy::jitter_check
