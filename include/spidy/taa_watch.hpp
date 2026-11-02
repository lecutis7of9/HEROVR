#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
//
// 0.0.76 TAA WATCH (read-only, SpidyTaaWatchData, tools/taa_watch_record.py): what the launcher's report says of
// SPIDY TAA every 10 seconds - the eye pairs a second (fps), the GPU time a pair (our pass, and the game's own eye
// jobs from a timestamp at each job's begin and end marker), and the still / moving split: how many pairs the
// history classed as moving, and how many pairs with a still head (the eye's view moved under 2 degrees and 3 cm a
// second) it classed as moving anyway. If that last share is over 20% the thresholds are wrong, not the blend.
// It also logs, for the first 200 eye pairs, the depth that leaves the G-buffer pass inside each eye's job (the
// fmt 19 R32G8X24_TYPELESS seen at the eye's size): which resource, whether each eye's job had one, whether the flat
// view ever wrote it. CONFIRMED only when all of that holds for 200 pairs; otherwise CANDIDATE. Nothing here changes
// a picture.
#include <cmath>
#include <cstdint>

namespace spidy::taa_watch {
// A still head: the eye's view turned and travelled less than this a second between its two pairs.
inline constexpr float stillTurnPerSecond = .035f; // radians a second, 2 degrees
inline constexpr float stillTravelPerSecond = .03f; // metres a second
inline bool stillHead(float turn, float travel, float seconds) {
    if (!(seconds > 0) || !std::isfinite(seconds) || !std::isfinite(turn) || !std::isfinite(travel))
        return false;
    return turn / seconds < stillTurnPerSecond && travel / seconds < stillTravelPerSecond;
}
// More than this share of still-head pairs classed moving: the thresholds are wrong.
inline constexpr float wrongShare = .2f;
inline constexpr uint32_t windowMs = 10000, windowCount = 90; // 15 minutes kept
struct Window {
    uint32_t ms{};          // how long it ran
    uint32_t pairs{};       // eye pairs through the history pass
    uint32_t moving{};      // classed moving (motion share past half: the report's "moving")
    uint32_t eased{};       // kept less than the still share (any motion share)
    uint32_t stillHead{};   // pairs with a still head
    uint32_t stillEased{};  // of those, kept less than the still share
    uint32_t stillMoving{}; // of those, classed moving
    uint32_t taaTimed{};
    float taaGpuMs{};       // our pass's GPU time (both eyes), summed
    uint32_t eyeTimed{};
    float eyeGpuMs{};       // the game's two eye jobs' GPU span a pair, summed
    uint32_t depthPairs{};  // pairs the history was turned with the eyes' depth (A-depth builds)
    uint32_t depthMissed{}; // depth asked for but not there: the turn alone
    uint32_t restarted{};   // pairs the history started again
};
static_assert(sizeof(Window) == 56);
// The eye depth log (the first depthFrames pairs).
inline constexpr uint32_t depthFrames = 200;
enum Verdict : uint32_t { notSeen = 0, candidate = 1, confirmed = 2 };
struct DepthLog {
    uint32_t frames{};       // pairs logged
    uint32_t bothEyes{};     // of those, a depth out of each eye's job's G-buffer pass
    uint32_t format{}, width{}, height{};
    uint32_t distinct[2]{};  // resources seen per eye (up to 4 counted)
    uint32_t shared{};       // pairs where both eyes' depth was the same resource
    uint32_t flatSeen{};     // that resource left DEPTH_WRITE with no eye job open (the flat view)
    uint32_t otherList{};    // it left DEPTH_WRITE on another list while an eye job was open
    uint32_t verdict{};      // Verdict
    uint32_t exits{};        // eye-sized fmt 19 / 20 depth exits seen in the eye jobs (all pairs)
    uint64_t resource[2]{};  // each eye's most seen resource (its address, for the log)
};
static_assert(sizeof(DepthLog) == 64);
inline uint32_t depthVerdict(const DepthLog& d) {
    if (!d.frames)
        return notSeen;
    if (d.frames >= depthFrames && static_cast<uint64_t>(d.bothEyes) * 10 >= static_cast<uint64_t>(d.frames) * 9 &&
        !d.flatSeen && d.distinct[0] && d.distinct[1])
        return confirmed;
    return candidate;
}
// Which build this is (Data.build): 0 the shared logger only, 1 A-depth, 2 B-ss125, 3 C-still094.
inline constexpr uint32_t thisBuild = 0;
struct Data {
    uint32_t magic = 0x44575453; // STWD
    uint32_t version = 1;
    uint32_t bytes = 5184;
    uint32_t windowMs = taa_watch::windowMs;
    uint32_t closed{}; // windows finished (windows[closed % windowCount] the next)
    uint32_t build = thisBuild;
    DepthLog depth;
    Window current;
    Window windows[windowCount];
};
static_assert(sizeof(Data) == 5184);
// Closes the running window once it has run windowMs (a long pause closes one longer window).
inline void roll(Data& d, uint64_t& startMs, uint64_t nowMs) {
    if (!startMs) {
        startMs = nowMs;
        return;
    }
    if (nowMs < startMs) {
        startMs = nowMs;
        return;
    }
    if (nowMs - startMs >= windowMs) {
        d.current.ms = static_cast<uint32_t>(nowMs - startMs > 0xffffffffull ? 0xffffffffull : nowMs - startMs);
        d.windows[d.closed % windowCount] = d.current;
        ++d.closed;
        d.current = {};
        startMs = nowMs;
    }
}
// One pair into the running window.
inline void notePair(Window& w, bool restarted, bool moving, bool eased, bool still) {
    ++w.pairs;
    w.restarted += restarted;
    w.moving += moving;
    w.eased += eased;
    w.stillHead += still;
    w.stillEased += still && eased;
    w.stillMoving += still && moving;
}
} // namespace spidy::taa_watch
