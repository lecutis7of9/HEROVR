#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
//
// Update 33: EYE AA (live, default OFF, never kept): the port's own temporal
// anti-aliasing of the eye images, no game address involved. Each eye command's
// lens is moved by a sub-pixel step (Halton 2,3, 8 steps) and the eye history
// pass (eye_history.hpp, native_eye_gpu.cpp) blends each new pair with the one
// before, turned to the new pair's view with the moved lens, so the old picture
// lines up exactly and the steps add up to a supersampled edge; the 3 x 3 clamp
// keeps what moved from ghosting. The headset layer is given the same moved
// lens, so the compositor puts each picture where it was made: nothing wobbles.
// FXAA (ANTI-ALIASING) stays as it is, before it. No eye depth is used: what
// moves on its own (cars, people) is held by the clamp, not reprojected.
#include <cmath>
#include <cstdint>
#include <cstdio>

namespace spidy::eye_taa {
// Update 35: EYE AA's cost on the wrist is the median of the history pass's last
// timings (it was the newest one alone: the Oct 23 run showed "6.33 MS" where its 5858
// passes averaged 0.34 ms, one pass held up on the GPU). NaN while none.
class MsMedian {
public:
    static constexpr unsigned size = 31;
    void note(float ms) {
        if (!(ms >= 0) || !std::isfinite(ms))
            return;
        v_[next_++ % size] = ms;
        if (count_ < size)
            ++count_;
    }
    float median() const {
        if (!count_)
            return std::nanf("");
        float s[size];
        for (unsigned i = 0; i < count_; ++i)
            s[i] = v_[i];
        for (unsigned i = 1; i < count_; ++i) // insertion sort: 31 at most
            for (unsigned j = i; j > 0 && s[j - 1] > s[j]; --j) {
                const float t = s[j];
                s[j] = s[j - 1];
                s[j - 1] = t;
            }
        return count_ % 2 ? s[count_ / 2] : (s[count_ / 2 - 1] + s[count_ / 2]) / 2;
    }
    unsigned count() const { return count_; }

private:
    float v_[size]{};
    unsigned next_{}, count_{};
};
// The history share EYE AA asks for (at least; HISTORY BLEND may ask more).
// Update 34 (Tier 1): 88 (was 80) and 16 steps (was 8): each edge is the mean of
// more sub-pixel positions, so what still flickered at 80 / 8 settles.
inline constexpr uint32_t historyPercent = 88;
inline constexpr unsigned steps = 16;
// Update 34 (Tier 1): while EYE AA is on, the 3 x 3 clamp gives at least this share of
// the range on each side (eye_history::historyLoosen; still none while the head turns,
// turnedLoosen): the plain clamp threw away the steps' own sub-pixel colours, the
// flicker left on edges and far buildings in run 33.
inline constexpr float clampGive = .25f;
inline float loosenInForce(float loosen, bool on) {
    return on && !(loosen >= clampGive) ? clampGive : loosen;
}
// The radical inverse of index in base (index from 1).
inline float halton(unsigned index, unsigned base) {
    float f = 1, r = 0;
    while (index > 0) {
        f /= static_cast<float>(base);
        r += f * static_cast<float>(index % base);
        index /= base;
    }
    return r;
}
// The step of frame n, in pixels, each in -0.5..0.5.
inline void step(uint64_t n, float& x, float& y) {
    const unsigned i = static_cast<unsigned>(n % steps) + 1;
    x = halton(i, 2) - .5f;
    y = halton(i, 3) - .5f;
}
// The tangent shift (dx, dy) that moves a lens (angles in radians: left, right, down,
// up; left and down negative) by (x, y) pixels of a width x height image. False (0, 0)
// for a bad lens or step.
inline bool lensShift(float left, float right, float down, float up, uint32_t width, uint32_t height, float x,
                      float y, float& dx, float& dy) {
    dx = dy = 0;
    if (!width || !height || !std::isfinite(left + right + down + up) || !(right > left) || !(up > down) ||
        std::abs(left) >= 1.5f || std::abs(right) >= 1.5f || std::abs(down) >= 1.5f || std::abs(up) >= 1.5f ||
        !(std::abs(x) <= .5f) || !(std::abs(y) <= .5f))
        return false;
    dx = x * (std::tan(right) - std::tan(left)) / static_cast<float>(width);
    dy = y * (std::tan(up) - std::tan(down)) / static_cast<float>(height);
    return std::isfinite(dx) && std::isfinite(dy);
}
// The lens with its tangents moved by (dx, dy). False (unchanged) for a bad result.
inline bool shiftLens(float& left, float& right, float& down, float& up, float dx, float dy) {
    if (!std::isfinite(left + right + down + up) || std::abs(left) >= 1.5f || std::abs(right) >= 1.5f ||
        std::abs(down) >= 1.5f || std::abs(up) >= 1.5f || !std::isfinite(dx + dy))
        return false;
    const float nl = std::atan(std::tan(left) + dx), nr = std::atan(std::tan(right) + dx);
    const float nd = std::atan(std::tan(down) + dy), nu = std::atan(std::tan(up) + dy);
    if (!std::isfinite(nl + nr + nd + nu) || !(nr > nl) || !(nu > nd))
        return false;
    left = nl, right = nr, down = nd, up = nu;
    return true;
}
// The history share in force: the slider's, or EYE AA's floor while it is on.
inline uint32_t historyInForce(uint32_t slider, bool on) {
    return on && slider < historyPercent ? historyPercent : slider;
}
// The HUD: state off 0, on 1, failed 2 (reason the history pass's fault, 1-5).
enum State : uint32_t { off = 0, on = 1, failed = 2 };
inline constexpr uint32_t reasons = 6;
inline constexpr uint32_t pack(uint32_t state, uint32_t reason) {
    return state | reason << 8;
}
inline const char* reasonText(uint32_t reason) {
    switch (reason) {
    case 1:
        return "the blend's root signature failed";
    case 2:
        return "the blend's shader did not compile";
    case 3:
        return "the blend's pipeline failed";
    case 4:
        return "no eye views to line pictures up";
    case 5:
        return "the blend's pictures failed";
    default:
        return "the blend stopped";
    }
}
inline constexpr const char* help = "Smooths jagged, shimmering edges over several frames; FXAA stays on.";
// The value the row shows: OFF, ON (with the blend's GPU ms when known), OFF (FAILED).
inline void valueText(uint32_t packed, float ms, char* out, unsigned size) {
    const uint32_t s = packed & 0xff;
    if (s == failed)
        std::snprintf(out, size, "OFF (FAILED)");
    else if (s == on && std::isfinite(ms) && ms > 0)
        std::snprintf(out, size, "ON %.2f MS", static_cast<double>(ms));
    else
        std::snprintf(out, size, s == on ? "ON" : "OFF");
}
} // namespace spidy::eye_taa
