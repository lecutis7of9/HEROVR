#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
//
// Update 34: ENGINE TAA (live, default OFF, never kept): the game's own temporal
// anti-aliasing run on each eye, as SpidyVR has it on Spider-Man Remastered.
// Exe-confirmed (Spider-Man2.exe 2.810.0.0, by disassembly):
// - TemporalAa is render command 0xb1, handler 0x2c2dc60. It runs only when the
//   job's anti-aliasing ([0xc2c6a10], the settings copy's +0x570) is 1 and the job's
//   lens jitter (+0x428, +0x42c) is not zero; its targets come from 0x2c98240
//   (job +0x18e0's +0x12478 / +0x12848, present when +0x12648 / +0x12a18 is set),
//   and it returns at once without them.
// - The game queues 0xb1 for every view itself (0x2d017b3) unless profile +0xcc8
//   bit 0 is set.
// - Target maker 0x2c6eec0 makes a target of kind k only where the buffer set's
//   mask (table 0x5d65408 in .rdata, index set +0x38, checked when set +0x3c & 4)
//   has k. The table is {0x7ffff, 0x7ffff, 0x3fcc8 x 6}: 0x3fcc8 lacks 4, the kind
//   of kTaaTarget, kTaaTargetHistory, kTaaDepth0/1. kTaaDepth0/1 are made only if
//   the game's anti-aliasing (0xa164900) is 1 (TAA) or 5..7 when the set is made.
// So: -EngineTaa (launch) adds 4 to entries 2..7 while each eye's buffers are made
// (then puts the table back); the live toggle gives the eyes a jittered lens and
// their job +0x570 = 1. Candidates (not confirmed): the eyes' set index; the jitter
// units (2 x pixel / render size, as Remastered's camera; the game camera's own is
// logged to compare).
#include <cmath>
#include <cstdint>
#include <cstdio>

namespace spidy::engine_taa {
inline constexpr uint32_t maskRva = 0x5d65408, aaRva = 0xa164900;
inline constexpr uint32_t maskStock[8] = {0x7ffff, 0x7ffff, 0x3fcc8, 0x3fcc8, 0x3fcc8, 0x3fcc8, 0x3fcc8, 0x3fcc8};
inline constexpr uint32_t taaKind = 4;
inline constexpr uint32_t jobAa = 0x570, jobJitter = 0x428;
// The table as the exe has it (else the build differs: leave it alone).
inline bool maskIsStock(const uint32_t* m) {
    for (unsigned i = 0; i < 8; ++i)
        if (m[i] != maskStock[i])
            return false;
    return true;
}
// The game's anti-aliasing values that make the TAA depth targets (0x2c6ef5f..6c).
inline bool aaMakesTargets(uint32_t aa) { return aa == 1 || (aa >= 5 && aa <= 7); }

enum State : uint32_t { off = 0, on = 1, unavailable = 2 };
enum Reason : uint32_t {
    none = 0,
    notPrepared = 1, // launched without -EngineTaa
    gameAa = 2,      // the game's AA was not TAA/DLAA when the eyes were made
    maskDiffers = 3, // the mask table is not the exe's
    noTargets = 4,   // the eyes have no TAA targets
    targetsGone = 5, // they went away while on
    slow = 6,        // the frame rate halved: EYE AA instead
    vrEnded = 7,
    noEyes = 8,      // no eyes yet
    reasons = 9
};
inline constexpr uint32_t pack(uint32_t state, uint32_t reason) { return state | reason << 8; }
inline uint32_t stateOf(uint32_t packed) { return packed & 0xff; }
inline uint32_t reasonOf(uint32_t packed) { return packed >> 8 & 0xff; }
inline const char* reasonText(uint32_t reason) {
    switch (reason) {
    case notPrepared: return "launch with -EngineTaa to use it (restart)";
    case gameAa: return "set the game's Anti-Aliasing to TAA, then restart";
    case maskDiffers: return "this game build differs; left alone";
    case noTargets: return "the eyes got no TAA targets; use EYE AA";
    case targetsGone: return "its targets went away; EYE AA is on instead";
    case slow: return "the frame rate halved; EYE AA is on instead";
    case vrEnded: return "VR ended";
    case noEyes: return "the eyes are not made yet; try again";
    default: return "";
    }
}
// True for reasons that need a restart to clear.
inline bool needsRestart(uint32_t reason) { return reason == notPrepared || reason == gameAa; }
inline constexpr const char* help = "The game's own anti-aliasing on each eye; turns itself off if it fails.";
inline void valueText(uint32_t packed, char* out, unsigned size) {
    const uint32_t s = stateOf(packed), r = reasonOf(packed);
    if (s == on)
        std::snprintf(out, size, "ON");
    else if (s == unavailable)
        std::snprintf(out, size, needsRestart(r) ? "OFF (RESTART)" : "OFF (FAILED)");
    else
        std::snprintf(out, size, "OFF");
}
inline constexpr const char* widestValue() { return "OFF (RESTART)"; }

// Update 34.2: the jitter given to the eyes' lens while ENGINE TAA is on (live: the wrist's
// TAA JITTER, live.txt taa_jitter; never kept). run341: distant buildings shook back and
// forth with OURS. Which is right is not confirmed, so every candidate is one press away:
// - ours:   2 x Halton step / render size (Remastered's camera units), as Update 34;
// - game:   the game camera's own jitter this frame (main +0x428/+0x42c), so the eyes
//           and whatever TemporalAa takes from the game's camera agree (double jitter);
// - pixels: the Halton step itself, -0.5..0.5 (the stock jitter in render pixels);
// - none:   no jitter - and 0.0.73 test 4: the game's TemporalAa does not run at all then
//           (exe-confirmed: handler 0x2c2dc60 returns when the job's jitter is zero), so NONE
//           is "ENGINE TAA's targets, no TAA"; IMAGE STYLE SPIDYVR uses OURS in its place
//           (image_style::jitterMode).
// scalePercent (live.txt taa_jitter_scale, 0-200, default 100) multiplies ours / pixels / game.
enum JitterMode : uint32_t { ours = 0, game = 1, pixels = 2, noJitter = 3, jitterModes = 4 };
inline const char* jitterName(uint32_t m) {
    switch (m) {
    case ours: return "OURS";
    case game: return "GAME";
    case pixels: return "PIXELS";
    case noJitter: return "NONE";
    default: return "?";
    }
}
inline constexpr const char* jitterHelp =
    "ENGINE TAA's jitter, only while it is on: try NONE if far buildings shake.";
// Update 35: the wrist's press steps OURS <-> NONE only. GAME and PIXELS showed no
// difference in the Oct 23 runs (both still shook); live.txt taa_jitter still sets them.
inline uint32_t pressNext(uint32_t mode) { return mode == ours ? noJitter : ours; }
// Update 34.2: TAA JITTER's subtext while it does nothing (ENGINE TAA off, or EYE AA on).
inline constexpr const char* jitterInactiveHelp = "Only works while ENGINE TAA is on.";
// TAA JITTER acts only while ENGINE TAA is on and EYE AA is off (stereo_probe.cpp eyeJitter).
inline bool jitterActive(uint32_t enginePacked, bool eyeAaOn) { return stateOf(enginePacked) == on && !eyeAaOn; }
inline constexpr const char* jitterWidest = "PIXELS";
inline constexpr uint32_t defaultJitterScale = 100, maximumJitterScale = 200;
// The jitter for a mode: halton in -0.5..0.5 (this frame's step), render the eye render
// size, gameJitter the game camera's own. Zero for a bad input.
inline void jitterFor(uint32_t mode, float hx, float hy, uint32_t width, uint32_t height, float gx, float gy,
                      uint32_t scalePercent, float& x, float& y) {
    x = y = 0;
    const float k = static_cast<float>(scalePercent > maximumJitterScale ? maximumJitterScale : scalePercent) / 100.f;
    switch (mode) {
    case ours:
        if (width && height) {
            x = 2 * hx / static_cast<float>(width) * k;
            y = 2 * hy / static_cast<float>(height) * k;
        }
        break;
    case game:
        x = gx * k;
        y = gy * k;
        break;
    case pixels:
        x = hx * k;
        y = hy * k;
        break;
    default:
        break;
    }
    if (!std::isfinite(x) || !std::isfinite(y) || std::abs(x) > 4 || std::abs(y) > 4)
        x = y = 0;
}
// Shared with tools/run_game_vr.py (SpidyEngineTaaData): what ENGINE TAA did, for the report.
inline constexpr uint32_t dataMagic = 0x41544e45; // ENTA
inline constexpr uint32_t dataVersion = 1;
struct Data {
    uint32_t magic = dataMagic, version = dataVersion, bytes = 144, packed{};
    uint32_t mode{}, scalePercent = defaultJitterScale, gameAa = 0xffffffff, maskSeen{};
    uint32_t widened{}, turnedOn{}, refused{}, wentOff{};
    uint32_t lastRefusal{}, lastOff{}, renderWidth{}, renderHeight{};
    uint64_t jitteredLenses{};   // eye lenses made with a jitter while on
    uint64_t onFrames{};         // frames ENGINE TAA was on
    float eyeJitter[2]{}, gameJitter[2]{};          // the newest
    float gameJitterMax[2]{}, eyeJitterMax[2]{};    // the largest seen while on (absolute)
    uint32_t targetsLeft{}, targetsRight{};         // the eyes had the TAA targets (newest check)
    uint32_t modeChanges{}, reserved[3]{};
    uint64_t sequence{};
};
static_assert(sizeof(Data) == 144);

// The FPS guard: new eye pairs a second over the 2 s before ENGINE TAA went on and
// the 2 s after the first second on (the change settles); collapsed if under half.
class FpsGuard {
public:
    static constexpr double window = 2, settle = 1;
    void frame(double seconds, bool newPair) {
        if (!(seconds >= last_))
            reset(seconds);
        last_ = seconds;
        if (!armed_) { // the rolling 2 s before
            if (seconds - start_ >= window) {
                prior_ = count_ / (seconds - start_);
                start_ = seconds;
                count_ = 0;
            }
            count_ += newPair ? 1 : 0;
            return;
        }
        if (seconds < armedAt_ + settle)
            return;
        count_ += newPair ? 1 : 0;
        if (seconds - armedAt_ - settle >= window) {
            after_ = count_ / (seconds - armedAt_ - settle);
            done_ = true;
        }
    }
    // ENGINE TAA went on at `seconds`: compare with the rate before.
    void arm(double seconds) {
        before_ = prior_ > 0 ? prior_ : (seconds - start_ > .5 ? count_ / (seconds - start_) : 0);
        armed_ = true;
        done_ = false;
        armedAt_ = seconds;
        count_ = 0;
    }
    void disarm(double seconds) { reset(seconds); }
    bool armed() const { return armed_; }
    bool decided() const { return armed_ && done_; }
    bool collapsed() const { return decided() && before_ > 5 && after_ < .5 * before_; }
    double before() const { return before_; }
    double after() const { return after_; }

private:
    void reset(double seconds) {
        armed_ = done_ = false;
        start_ = last_ = seconds;
        count_ = 0;
        prior_ = before_ = after_ = 0;
    }
    bool armed_{}, done_{};
    double start_{}, last_{}, armedAt_{}, count_{}, prior_{}, before_{}, after_{};
};
} // namespace spidy::engine_taa
