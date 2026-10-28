#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
//
// Eye history (live.txt "history"): each new eye pair is blended with the one
// before it, turned to the new pair's view, so the city's fine detail (window
// grids, railings, far edges) that crawls and shimmers from one picture to the
// next settles. No game address or game setting is involved: it is a pass over
// the eye images the bridge already stages (native_eye_gpu.cpp).
//
// The previous picture is turned, not moved: a pixel's direction in the new
// eye is looked up in the old eye's lens (historyReprojection), which is exact
// for anything far away, where the shimmer is, whatever the head's turn. What
// moved nearer (a swing's travel, a passing car) does not line up, so the old
// colour is first held to the range of the new pixel's 3 x 3 neighbourhood,
// as temporal anti-aliasing does: a ghost cannot leave the new picture's
// colours there. A cut (a turn of more than maximumHistoryTurn between two
// pairs) or a pair without its view starts the history again.
#include <array>
#include <cmath>
#include <vector>
#include <cstdint>

namespace spidy::eye_history {
// Update 21: up to 95 (was 80). From 50% on the 3 x 3 clamp is also widened
// (historyLoosen), so the top of the range is plainly heavy: very calm, with
// visible trails behind motion. A cut still starts the history again.
inline constexpr uint32_t maximumPercent = 95;
inline constexpr uint32_t loosenFrom = 50;
inline constexpr float maximumHistoryTurn = 0.26f; // radians, 15 degrees
// One eye's view as the game was told to render it: the world matrix of
// game_tracking's worldPose (x right, y the eye's down, z its forward, each
// as 3 floats at 0, 4 and 8) and the lens's tangents (left, right, down, up;
// left and down negative).
struct EyeView {
    std::array<float, 16> world{};
    std::array<float, 4> tangents{};
};
// The homography (3 x 3, row-major) that takes a pixel of the current eye, as
// (u, v, 1) with u, v in 0-1 from the top left, to the previous eye's image:
// (U, V, W), the place U/W, V/W, in front of the eye while W > 0. False for a
// degenerate lens.
inline bool historyReprojection(const EyeView& previous, const EyeView& current, std::array<float, 9>& out) {
    const auto& c = current.tangents;
    const auto& p = previous.tangents;
    if (!(c[1] > c[0]) || !(c[3] > c[2]) || !(p[1] > p[0]) || !(p[3] > p[2]))
        return false;
    // Each eye's axes in the world in the OpenXR sense: right, up, back.
    auto axes = [](const EyeView& e, int a) {
        const float sign = a == 0 ? 1.f : -1.f;
        return std::array<float, 3>{sign * e.world[a * 4], sign * e.world[a * 4 + 1], sign * e.world[a * 4 + 2]};
    };
    float turn[3][3]; // previous eye's frame from the current eye's
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) {
            const auto a = axes(previous, i), b = axes(current, j);
            turn[i][j] = a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
        }
    // (u, v, 1) to the current eye's direction (x right, y up, z back).
    const float from[3][3] = {{c[1] - c[0], 0, c[0]}, {0, -(c[3] - c[2]), c[3]}, {0, 0, -1}};
    // A direction in the previous eye to (U, V, W).
    const float w = p[1] - p[0], h = p[3] - p[2];
    const float to[3][3] = {{1 / w, 0, p[0] / w}, {0, -1 / h, -p[3] / h}, {0, 0, -1}};
    float middle[3][3]{};
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            for (int k = 0; k < 3; ++k)
                middle[i][j] += turn[i][k] * from[k][j];
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) {
            float sum = 0;
            for (int k = 0; k < 3; ++k)
                sum += to[i][k] * middle[k][j];
            out[i * 3 + j] = sum;
        }
    for (float v : out)
        if (!std::isfinite(v))
            return false;
    return true;
}
// The angle between the two eyes' forward directions, radians.
inline float historyTurn(const EyeView& previous, const EyeView& current) {
    const float d = previous.world[8] * current.world[8] + previous.world[9] * current.world[9] +
                    previous.world[10] * current.world[10];
    return std::acos(std::fmax(-1.f, std::fmin(1.f, d)));
}
// The share of the old picture a pixel keeps (0 starts the history again).
inline float historyWeight(uint32_t percent, bool bothViews, float turn) {
    if (!bothViews || !(turn <= maximumHistoryTurn))
        return 0;
    return static_cast<float>(percent > maximumPercent ? maximumPercent : percent) / 100.f;
}
// How far the old colour may leave the new pixel's 3 x 3 range, as a share
// of that range on each side: 0 up to 50%, rising to 0.5 at 95%. 0 is the
// plain clamp of Update 14-20.
inline float historyLoosen(uint32_t percent) {
    const uint32_t p = percent > maximumPercent ? maximumPercent : percent;
    if (p <= loosenFrom)
        return 0;
    return .5f * static_cast<float>(p - loosenFrom) / static_cast<float>(maximumPercent - loosenFrom);
}
// Update 23: what a head turn leaves of the history. The reprojection only
// turns the old picture (no head movement, nothing that moved by itself), so
// while the head turns, shadows and edges that shift against the buildings
// were held back by the loosened clamp and dragged behind ("shadows drag as
// you turn your head", October 23). Both now fall off with the turn between
// two pairs: the loosening to none at a cut's turn, the weight to 40% of
// itself there. A still head keeps all of both.
inline float historyTurnShare(float turn) {
    if (!(turn > 0))
        return 0;
    return turn >= maximumHistoryTurn ? 1.f : turn / maximumHistoryTurn;
}
inline float turnedWeight(float weight, float turn) {
    return weight * (1.f - .6f * historyTurnShare(turn));
}
inline float turnedLoosen(float loosen, float turn) {
    return loosen * (1.f - historyTurnShare(turn));
}
// Update 34.2: the swing's smear. The old picture is turned, never moved, so while the
// eye travels (a swing: 0.3-1 m between pairs) everything near is blended with where it
// was. The history now falls with the eyes' travel between the two pairs (metres, the
// world matrix's translation, 12-14) and with the turn: from motionFrom it falls to the
// TRAIL share (0-100, the wrist's EYE AA TRAIL, live.txt trail) at motionFull, and past
// travelCut (or the turn's cut) the pair starts again. The 3 x 3 clamp is the plain one
// (no give) while moving, so what is left cannot trail outside the new colours.
// Update 35: a much harder cut (the Oct 23 player: "turn down the motion blur a lot").
// Run 34.2's eyes travelled 0.043 m a pair on average, under the old full-cut speed
// (0.20 m), so most moving pairs kept a share; the ramps now end at a walk (0.06 m a
// pair, about 3 m/s) and a slow head turn (0.06 rad, 3.4 degrees), and TRAIL is 0.
inline constexpr uint32_t defaultTrail = 0, maximumTrail = 100;
inline constexpr float motionFrom = .005f, motionFull = .06f; // metres between pairs (was .02-.20)
inline constexpr float turnFrom = .005f, turnFull = .06f;     // radians between pairs (was .01-.12)
inline float historyTravel(const EyeView& previous, const EyeView& current) {
    const float x = current.world[12] - previous.world[12], y = current.world[13] - previous.world[13],
                z = current.world[14] - previous.world[14];
    const float d = std::sqrt(x * x + y * y + z * z);
    return std::isfinite(d) ? d : 1e9f;
}
inline float ramp(float v, float from, float full) {
    if (!(v > from))
        return 0;
    return v >= full ? 1.f : (v - from) / (full - from);
}
// 0 still, 1 at swing speed (or a fast turn).
inline float motionShare(float travel, float turn) {
    const float t = std::isfinite(turn) ? ramp(turn, turnFrom, turnFull) : 0.f;
    return std::fmax(ramp(travel, motionFrom, motionFull), t);
}
// Past this the pair starts again (no trail at all), unless TRAIL is at its top.
inline float travelCut(uint32_t trail) {
    return trail >= maximumTrail ? 1e9f : .5f + 1.5f * static_cast<float>(trail) / 100.f;
}
// The share of the old picture kept, from the still weight: down to weight x trail% at
// swing speed; 0 past travelCut.
inline float movedWeight(float weight, float travel, float turn, uint32_t trail) {
    const uint32_t t = trail > maximumTrail ? maximumTrail : trail;
    if (!(weight > 0) || !(travel <= travelCut(t)))
        return 0;
    const float s = motionShare(travel, turn);
    return weight * (1.f - s * (1.f - static_cast<float>(t) / 100.f));
}
// The clamp's give, none at all once moving at half the swing speed.
inline float movedLoosen(float loosen, float travel, float turn) {
    const float s = motionShare(travel, turn);
    return loosen * std::fmax(0.f, 1.f - 2.f * s);
}
// Update 37: HISTORY CUT (live.txt history_cut, 25-400, 100 as Update 35 made it): how much
// motion it takes for the history to fade out. The eyes' travel and turn between pairs are
// divided by cut/100 before the ramps above, so 50 cuts at half the motion (less smear,
// more shimmer) and 200 at twice it (smoother edges, more trail while swinging).
inline constexpr uint32_t defaultCut = 100, minimumCut = 25, maximumCut = 400;
inline float cutMotion(float motion, uint32_t cut) {
    const uint32_t c = cut < minimumCut ? minimumCut : cut > maximumCut ? maximumCut : cut;
    return motion * 100.f / static_cast<float>(c);
}
// 0.0.73 test 3 LIGHT HOLD (combat38 lightHold; ON by default, STRONG, OFF = test 2): street lights and
// other small bright lights flickered in test 2 (owner, test 2 run). CANDIDATE cause: EYE AA moves both
// lenses by a sub-pixel step every frame, but the history that averages the steps is cut while the eyes
// travel or turn (movedWeight: none at swing speed), so a bulb smaller than a pixel lands in and out of
// its pixel each frame unaveraged; and while still, the 3 x 3 clamp and the 0.0.62 anti-ghost cut throw
// away a bulb's own old brightness whenever this frame's step missed it. LIGHT HOLD:
//  - JITTER CALM: the step is scaled by the share of history the last pass kept (none kept: no step), so
//    nothing is jittered that the history cannot average. STRONG: no step at all.
//  - BRIGHT HOLD (the history shader, row0.w): a bright old pixel may stay above this frame's 3 x 3 by
//    part of its excess, the anti-ghost cut eased for it, and the blend weighted 1 / (1 + luma) (Karis),
//    so one frame's bright miss or hit moves the result less. Only while still (none at half swing speed).
inline constexpr uint32_t lightHoldOn = 50, lightHoldStrong = 100;
// The setting's choices: 0 ON, 1 STRONG, 2 OFF (anything else ON).
inline uint32_t lightHoldPercentOf(uint32_t choice) {
    return choice == 1 ? lightHoldStrong : choice == 2 ? 0u : lightHoldOn;
}
// The shader's hold (0..1) for this pair's motion.
inline float lightHoldInForce(uint32_t percent, float travel, float turn) {
    const float p = static_cast<float>(percent > 100 ? 100 : percent) / 100.f;
    return p * std::fmax(0.f, 1.f - 2.f * motionShare(travel, turn));
}
// JITTER CALM: the share of EYE AA's sub-pixel step to use, from the history share the last pass kept
// (0..1) and the share asked for (percent). OFF: the whole step (test 2); STRONG: none.
inline float jitterScale(uint32_t holdPercent, float lastKeep, uint32_t historyPercent) {
    if (!holdPercent)
        return 1.f;
    if (holdPercent >= lightHoldStrong || !historyPercent || !(lastKeep > 0))
        return 0.f;
    const float s = lastKeep * 100.f / static_cast<float>(historyPercent);
    return s >= 1.f ? 1.f : s;
}
// The shader's sum for one grey channel (tests; native_eye_gpu.cpp historyShader is the same).
inline float heldBrightness(float raw) {
    const float b = (raw - .35f) * 2.f;
    return b < 0 ? 0.f : b > 1 ? 1.f : b;
}
inline float heldPixel(float here, float lo, float hi, float raw, float keep, float loosen, float hold) {
    const float sat = [](float x) { return x < 0 ? 0.f : x > 1 ? 1.f : x; }(loosen);
    const float give = (hi - lo) * sat;
    const float bright = heldBrightness(raw) * (hold < 0 ? 0.f : hold > 1 ? 1.f : hold);
    const float lift = std::fmax(raw - (hi + give), 0.f) * bright * .8f;
    const float before = std::fmin(std::fmax(raw, std::fmax(lo - give, 0.f)), hi + give + lift);
    const float outside = std::fmax(std::fmax(lo - raw, raw - hi), 0.f);
    const float miss = outside / ((hi - lo) + .03f);
    float cut = 1.5f - miss * .75f;
    cut = cut < 0 ? 0.f : cut > 1 ? 1.f : cut;
    cut += (1.f - cut) * bright * .6f;
    const float k = keep * cut;
    const float plain = here + (before - here) * k;
    if (!(bright > 0))
        return plain;
    const float a = (1.f - k) / (1.f + here), b = k / (1.f + before);
    const float karis = a + b > 0 ? (here * a + before * b) / (a + b) : here;
    return plain + (karis - plain) * bright;
}
// 0.0.73 IMAGE SPIDYVR: the old picture read with a Catmull-Rom filter (native_eye_gpu.cpp historyShader
// sharpOld) instead of a bilinear one. CONFIRMED in our code (test 3, 0.0.73 export 17:15): the history
// was read bilinearly at the reprojected point while the head nearly always moved (32577 of 32652 pairs),
// so each pair blurred the kept picture by a fraction of a pixel and 80% of it was kept: the smudge.
// The four weights for a fractional offset f (0..1) between taps 1 and 2 (taps 0..3); they sum to 1.
inline void catmullRomWeights(float f, float w[4]) {
    w[0] = f * (-.5f + f * (1.f - .5f * f));
    w[1] = 1.f + f * f * (-2.5f + 1.5f * f);
    w[2] = f * (.5f + f * (2.f - 1.5f * f));
    w[3] = f * f * (-.5f + .5f * f);
}
// One row of pixels read at `shift` (0..1) to the right, Catmull-Rom (sharp) or bilinear, edges clamped
// (tests: how much of a one-pixel light a chain of reads keeps).
inline std::vector<float> resampleRow(const std::vector<float>& row, float shift, bool sharp) {
    const int n = static_cast<int>(row.size());
    std::vector<float> out(row.size());
    auto at = [&](int i) { return row[static_cast<size_t>(i < 0 ? 0 : i >= n ? n - 1 : i)]; };
    for (int i = 0; i < n; ++i) {
        if (sharp) {
            float w[4];
            catmullRomWeights(shift, w);
            out[static_cast<size_t>(i)] = w[0] * at(i - 1) + w[1] * at(i) + w[2] * at(i + 1) + w[3] * at(i + 2);
        } else
            out[static_cast<size_t>(i)] = at(i) * (1.f - shift) + at(i + 1) * shift;
    }
    return out;
}
} // namespace spidy::eye_history
