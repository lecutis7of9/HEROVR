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
    // 0.0.76 TAA JITTER: SPIDY TAA's history rejection - of the pixels that had a history to blend (one in 16 x 16
    // counted, both eyes), how many the YCoCg clip moved (clipped), split by a still head or not. And what the
    // window ran with (setupWith): live.txt vr_taa_jitter and vr_taa_still.
    uint32_t countedPairs{};  // pairs whose counts came back
    uint32_t stillSampled{}, stillClipped{};
    uint32_t movingSampled{}, movingClipped{};
    uint32_t setup{};         // bits 0-7: each taa_jitter::Mode seen (1 << mode); bits 16-31: the still share, 1/1000
    // 0.0.76 JITTER CHECK (noteJitterCheck): with a still head (eye 0, a history to turn), our own sub-pixel step
    // this pair and last (eye pixels, as the lens was moved: x right, y up) and how far the history pass turned the
    // eye's centre (the previous image minus current, x right, y down). The sums: how far that turn was from the step
    // counted none / once / twice (jitterOff[n]); the least is how the history counts it.
    uint32_t jitterPairs{};
    float jitterNow[2]{}, jitterBefore[2]{}, shift[2]{}; // the window's last still pair's
    float jitterOff[3]{};
};
static_assert(sizeof(Window) == 120);
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
    uint32_t version = 3;
    uint32_t bytes = 11008;
    uint32_t windowMs = taa_watch::windowMs;
    uint32_t closed{}; // windows finished (windows[closed % windowCount] the next)
    uint32_t build = thisBuild;
    DepthLog depth;
    Window current;
    Window windows[windowCount];
};
static_assert(sizeof(Data) == 11008);
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
// The window's setup with one more pair: its jitter mode and the still share in force (thousandths; 0 not SPIDY TAA).
inline uint32_t setupWith(uint32_t setup, uint32_t jitterMode, uint32_t stillMilli) {
    const uint32_t seen = (setup & 0xff) | (jitterMode < 8 ? 1u << jitterMode : 0u);
    return seen | (stillMilli ? (stillMilli & 0xffff) << 16 : setup & 0xffff0000u);
}
// One pair's rejection counts into the running window (still: the head was still for that pair).
inline void noteRejection(Window& w, bool still, uint32_t sampled, uint32_t clipped) {
    if (clipped > sampled)
        return; // not one pair's counts
    ++w.countedPairs;
    (still ? w.stillSampled : w.movingSampled) += sampled;
    (still ? w.stillClipped : w.movingClipped) += clipped;
}
// The history turn's shift (eye pixels: the previous image's place minus the current one's, x right, y down) a
// still lens moved by `before` last pair and `now` this one (eye pixels, x right, y up: eye_taa::lensShift's) makes,
// with the step counted `times` times: a lens moved right shows everything further left, one moved up lower.
inline void jitterShift(const float now[2], const float before[2], float times, float out[2]) {
    out[0] = times * (now[0] - before[0]);
    out[1] = times * (before[1] - now[1]);
}
// One still-head pair's check into the running window: the step now and last pair, and the turn's shift measured.
inline void noteJitterCheck(Window& w, const float now[2], const float before[2], const float shift[2]) {
    if (!std::isfinite(now[0] + now[1] + before[0] + before[1] + shift[0] + shift[1]) || std::abs(shift[0]) > 64 ||
        std::abs(shift[1]) > 64)
        return; // not a still head's
    ++w.jitterPairs;
    for (int n = 0; n < 3; ++n) {
        float e[2];
        jitterShift(now, before, static_cast<float>(n), e);
        w.jitterOff[n] += std::hypot(shift[0] - e[0], shift[1] - e[1]);
    }
    for (int k = 0; k < 2; ++k) {
        w.jitterNow[k] = now[k];
        w.jitterBefore[k] = before[k];
        w.shift[k] = shift[k];
    }
}
// How the history turn counts our step (the least of jitterOff): 0 none, 1 once, 2 twice; -1 not known (no
// pairs, or no step moved: all three the same).
inline int jitterCounted(const Window& w) {
    if (!w.jitterPairs || (w.jitterOff[0] == w.jitterOff[1] && w.jitterOff[1] == w.jitterOff[2]))
        return -1;
    int best = 0;
    for (int n = 1; n < 3; ++n)
        if (w.jitterOff[n] < w.jitterOff[best])
            best = n;
    return best;
}
} // namespace spidy::taa_watch
