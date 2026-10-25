// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
#pragma once
// Update 37 performance pass: where the frame's CPU time goes, and one safe saving.
//
// Sections: the frame's prepare (game_xr's first frameStereo callback) split at fixed points
//   (menu, motion, combat, swing, haptics, screen copies, publish), each with mean and peak ms, and
//   the worst slow frame (over spikeMs) named by its slowest section: the 1.7 s prepare spike is
//   found by it in the next run's log (perf line, update37.perf in the report).
// Overlay parts: the hologram, the wheel/phone/extras and the drawing (renderViews), mean/peak ms.
// HoloReuse (live, HOLO REUSE / holo_reuse, default ON): the hologram's triangles are rebuilt at
//   most every other frame. In between, the last ones are carried rigidly onto the panel's new place
//   (its own axes), but only while nothing on it changed (same page, scroll, hover, press, no
//   pointer) and the panel moved under 3 cm and turned under 3 degrees. Anything else rebuilds, so
//   what you see is never more than one frame old. OFF = every frame rebuilt, as before.
#include "spidy/vertex.hpp"
#include "spidy/wrist_hud.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <vector>

namespace spidy::perf37 {
inline constexpr const char* holoReuseHelp =
    "Redraws the hologram every other frame while it sits still, to save CPU; OFF redraws every frame.";

struct Metric {
    double total{}, peak{}, last{};
    uint64_t count{};
    void add(double ms) {
        if (!std::isfinite(ms) || ms < 0)
            return;
        total += ms; ++count; last = ms;
        peak = std::max(peak, ms);
    }
    double mean() const { return count ? total / static_cast<double>(count) : 0; }
};

// ---- HoloReuse -------------------------------------------------------------------------
struct HoloKey {
    int page = -1;
    float scroll = 0;
    uint32_t hover = 0, held = 0, theme = 0, extra = 0; // extra: anything else the caller folds in
    float help = 0; // Update 38.5: the scrolling subtext's control and lines (a change rebuilds)
    bool pointing = false;
    bool operator==(const HoloKey&) const = default;
};
inline constexpr float reuseMetres = .03f, reuseCos = .99863f; // 3 cm, cos 3 degrees
class HoloReuse {
public:
    // Whether this frame may replay the last build, given its key and the panel's frame now.
    bool mayReuse(const HoloKey& key, const wrist_hud::Frame& f) const {
        if (!valid_ || reusedLast_ || key.pointing || !(key == key_))
            return false;
        const Vec3 d = f.centre - frame_.centre;
        if (!(length(d) < reuseMetres))
            return false;
        return turnCos(frame_.right, f.right) > reuseCos && turnCos(frame_.up, f.up) > reuseCos;
    }
    // After a build: keep [from, end) of `out`, in the panel's own axes.
    void store(const std::vector<Vertex>& out, size_t from, const HoloKey& key, const wrist_hud::Frame& f) {
        local_.clear();
        valid_ = false;
        reusedLast_ = false;
        const Vec3 n = cross(f.right, f.up);
        const float rr = dot(f.right, f.right), uu = dot(f.up, f.up), nn = dot(n, n);
        if (!(rr > 1e-10f) || !(uu > 1e-10f) || !(nn > 1e-14f) || from > out.size())
            return;
        local_.reserve(out.size() - from);
        for (size_t i = from; i < out.size(); ++i) {
            Vertex v = out[i];
            const Vec3 p = v.position - f.centre;
            v.position = {dot(p, f.right) / rr, dot(p, f.up) / uu, dot(p, n) / nn};
            local_.push_back(v);
        }
        key_ = key;
        frame_ = f;
        valid_ = true;
    }
    // Appends the kept triangles placed on `f`; the count appended.
    size_t replay(std::vector<Vertex>& out, const wrist_hud::Frame& f) {
        const Vec3 n = cross(f.right, f.up);
        for (Vertex v : local_) {
            const Vec3 l = v.position;
            v.position = f.centre + f.right * l.x + f.up * l.y + n * l.z;
            out.push_back(v);
        }
        reusedLast_ = true;
        return local_.size();
    }
    void reset() {
        valid_ = false;
        reusedLast_ = false;
        local_.clear();
    }
    size_t kept() const { return local_.size(); }

private:
    static float turnCos(Vec3 a, Vec3 b) {
        const float la = length(a), lb = length(b);
        return la > 1e-6f && lb > 1e-6f ? dot(a, b) / (la * lb) : -1.f;
    }
    std::vector<Vertex> local_;
    HoloKey key_{};
    wrist_hud::Frame frame_{};
    bool valid_ = false, reusedLast_ = false;
};

// ---- Sections --------------------------------------------------------------------------
// In the callback's order; each runs from the previous mark to its own.
enum Section : uint32_t { menu, motion, combat, swing, haptics, screen, publish, sections };
inline constexpr const char* sectionNames[sections] = {
    "phone+holo menu+settings", "motion+view+calibration", "slow motion+combat+heal", "swing+wall+gadgets",
    "haptics+body",             "screen copies",           "publish (to the end)"};
inline constexpr double spikeMs = 100.0;
struct Spike {
    double ms{}, sectionMs{}, seconds{};
    uint32_t section{};
    uint64_t count{};
};
class Sections {
public:
    using Clock = std::chrono::steady_clock;
    void begin() {
        start_ = last_ = Clock::now();
        std::fill(std::begin(now_), std::end(now_), 0.0);
        open_ = true;
    }
    void mark(Section s) {
        if (!open_ || s >= sections)
            return;
        const auto t = Clock::now();
        now_[s] += std::chrono::duration<double, std::milli>(t - last_).count();
        last_ = t;
    }
    // At the end of the callback (every return path: see Guard).
    void end(double sinceStartSeconds) {
        if (!open_)
            return;
        mark(publish);
        open_ = false;
        double total = 0;
        uint32_t worst = 0;
        for (uint32_t i = 0; i < sections; ++i) {
            metrics[i].add(now_[i]);
            total += now_[i];
            if (now_[i] > now_[worst])
                worst = i;
        }
        if (total > spikeMs) {
            ++spike.count;
            if (total > spike.ms)
                spike = {total, now_[worst], sinceStartSeconds, worst, spike.count};
        }
    }
    Metric metrics[sections];
    Spike spike;

private:
    Clock::time_point start_{}, last_{};
    double now_[sections]{};
    bool open_ = false;
};
// Ends the sections on any way out of the callback.
struct Guard {
    Sections& s;
    double seconds;
    ~Guard() { s.end(seconds); }
};

// ---- the record (PRF7), read by tools/update37_records.py ------------------------------
inline constexpr uint32_t dataMagic = 0x37465250; // PRF7
struct Data {
    uint32_t magic = dataMagic, version = 1, bytes = 288, holoReuse{};
    uint64_t holoBuilt{}, holoReused{};
    float holoBuildMs{}, holoBuildPeakMs{}, holoReplayMs{}, holoShownMs{}; // shownMs: build or replay, mean
    float sectionMean[sections]{}, sectionPeak[sections]{};
    float spikeMs{}, spikeSectionMs{}, spikeSeconds{};
    uint32_t spikeSection{};
    uint64_t spikes{}, frames{};
    float hudMean{}, hudPeak{}, extrasMean{}, extrasPeak{}, drawMean{}, drawPeak{}, overlayMean{}, overlayPeak{};
    float bloodMean{}, mirrorMean{}, worldScanMean{}, reserved{};
    uint64_t reservedWords[12]{};
    uint64_t sequence{};
};
static_assert(sizeof(Data) == 288);
} // namespace spidy::perf37
