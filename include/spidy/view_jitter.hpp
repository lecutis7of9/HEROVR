#pragma once
// 0.0.74 test 8 VIEW JITTER (stereo_probe's lens calls, game_xr's console lines): which views get lens jitter, and how
// much in pixels at each view's own size, with the VIEW JITTER FIX switch (combat38 view_jitter_fix, ON by default).
//
// The lens function (0x2c75560, CONFIRMED) stores +0x428 = 2 * argX and +0x42c = -2 * argY, and a stored value s moves
// the picture s * size / 2 pixels (right for x; up for y, so down = -s_y * H / 2). Test 7 passed the game view's STORED
// values back in as arguments on the game view and the head views, which doubles them (and flips Y) on each call.
//
// VIEW JITTER FIX ON (shipped): the head shadow view (fits the cascades; draws no picture, so no TAA resolves it) gets
// 0; the game view gets 0 while it is rested (it does not render, so its TAA does not run) and otherwise the game's
// own jitter, so it stores exactly what the game asked for (CANDIDATE: when the game view renders, e.g. RECORDING FOV
// or streamer, the game's flat TAA resolves it). ENGINE TAA: the same. The eyes keep their own (test 7) jitter.
// OFF: the test 7 arguments, unchanged.
#include <cmath>
#include <cstdint>

namespace spidy::view_jitter {

enum View : uint32_t { eyeLeft, eyeRight, gameView, gameHead, shadowHead, screenEye, views };

constexpr const char* viewName(uint32_t view) {
    return view == eyeLeft      ? "eye L"
           : view == eyeRight   ? "eye R"
           : view == gameView   ? "game view"
           : view == gameHead   ? "game head"
           : view == shadowHead ? "shadow head"
           : view == screenEye  ? "screen eye"
                                : "?";
}

// Pixels a stored +0x428 / +0x42c value moves the picture at a size: right, and down.
constexpr double pxRight(float storedX, uint32_t width) { return static_cast<double>(storedX) * static_cast<double>(width) / 2.0; }
constexpr double pxDown(float storedY, uint32_t height) { return -static_cast<double>(storedY) * static_cast<double>(height) / 2.0; }

struct Args {
    float x, y;
};
// The lens arguments that store exactly (storedX, storedY): the lens stores (2x, -2y).
constexpr Args argsFor(float storedX, float storedY) { return {storedX * .5f, -storedY * .5f}; }

enum Use : uint32_t { useTestSeven, useZero, useOwn };
// What a view's lens call passes. fix: VIEW JITTER FIX on; rendered: the game view draws this frame (not rested).
constexpr Use choose(uint32_t view, bool fix, bool rendered) {
    if (!fix)
        return useTestSeven;
    if (view == shadowHead)
        return useZero;
    if (view == gameView || view == gameHead)
        return rendered ? useOwn : useZero;
    return useTestSeven; // the eyes and the game screen keep theirs
}
// own: the game's own STORED jitter this frame; testSeven: the arguments test 7 passed.
constexpr Args fixedArgs(uint32_t view, bool fix, bool rendered, float ownX, float ownY, Args testSeven) {
    const Use use = choose(view, fix, rendered);
    return use == useZero ? Args{0.f, 0.f} : use == useOwn ? argsFor(ownX, ownY) : testSeven;
}

// One view's lens this frame (stored values after the call, the size its pixels are counted at).
struct Seen {
    float x, y;
    uint32_t width, height;
    uint32_t count; // lens calls this frame
};
// One frame (detail log): the switch, ENGINE TAA, whether the game view was rested, the game's own stored jitter.
struct Frame {
    uint64_t frame;
    uint64_t pass;
    uint32_t fix, engine, rested;
    float ownX, ownY;
    Seen seen[views];
};
inline void note(Frame& f, uint32_t view, float x, float y, uint32_t width, uint32_t height) {
    if (view >= views)
        return;
    f.seen[view] = Seen{x, y, width, height, f.seen[view].count + 1};
}

// Since the start, per view: runs, last size and stored value, biggest shift, nonzero count, and the Y sign against
// the game's own (same, flipped, or one of them zero).
struct Stats {
    uint64_t frames, restedFrames;
    uint64_t runs[views];
    uint64_t nonzero[views], ySame[views], yFlipped[views], yZero[views];
    uint32_t width[views], height[views];
    float lastX[views], lastY[views];
    float maxPx[views]; // since the last read (SpidyViewJitterStats clears it)
};
inline void add(Stats& s, uint32_t view, float x, float y, uint32_t width, uint32_t height, float ownY) {
    if (view >= views)
        return;
    ++s.runs[view];
    s.width[view] = width;
    s.height[view] = height;
    s.lastX[view] = x;
    s.lastY[view] = y;
    const double px = std::fmax(std::fabs(pxRight(x, width)), std::fabs(pxDown(y, height)));
    if (std::isfinite(px) && px > static_cast<double>(s.maxPx[view]))
        s.maxPx[view] = static_cast<float>(px);
    if (x != 0.f || y != 0.f)
        ++s.nonzero[view];
    if (y == 0.f || ownY == 0.f)
        ++s.yZero[view];
    else if ((y > 0.f) == (ownY > 0.f))
        ++s.ySame[view];
    else
        ++s.yFlipped[view];
}

// ENGINE TAA COST's head STILL (test 8): under 10 cm and 8 degrees from where the 10 s began (test 7: 3 cm and about
// 3 degrees, which a standing player's sway breaks nearly every window), and how many whole seconds stayed within
// 2 cm and 2 degrees (a per-second view, so a window with one step in it still shows its quiet seconds).
struct HeadStill {
    static constexpr float windowTravel = .10f, windowTurnDeg = 8.f, secondTravel = .02f, secondTurnDeg = 2.f;
    bool known{};
    float at[3]{}, look[3]{}, secAt[3]{}, secLook[3]{};
    uint64_t secMs{};
    float maxTravel{}, maxTurnDeg{}, secTravel{}, secTurnDeg{};
    uint32_t seconds{}, stillSeconds{};

    static float distance(const float* a, const float* b) {
        const float dx = a[0] - b[0], dy = a[1] - b[1], dz = a[2] - b[2];
        return std::sqrt(dx * dx + dy * dy + dz * dz);
    }
    static float turnDeg(const float* a, const float* b) {
        float dot = a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
        dot = dot > 1.f ? 1.f : dot < -1.f ? -1.f : dot;
        return std::acos(dot) * 57.2957795f;
    }
    // position in metres, look a unit vector, ms a clock.
    void add(const float* position, const float* lookNow, uint64_t ms) {
        if (!known) {
            for (int i = 0; i < 3; ++i) {
                at[i] = secAt[i] = position[i];
                look[i] = secLook[i] = lookNow[i];
            }
            secMs = ms;
            known = true;
        }
        maxTravel = std::fmax(maxTravel, distance(position, at));
        maxTurnDeg = std::fmax(maxTurnDeg, turnDeg(lookNow, look));
        secTravel = std::fmax(secTravel, distance(position, secAt));
        secTurnDeg = std::fmax(secTurnDeg, turnDeg(lookNow, secLook));
        if (ms - secMs >= 1000) {
            ++seconds;
            if (secTravel < secondTravel && secTurnDeg < secondTurnDeg)
                ++stillSeconds;
            for (int i = 0; i < 3; ++i) {
                secAt[i] = position[i];
                secLook[i] = lookNow[i];
            }
            secMs = ms;
            secTravel = secTurnDeg = 0.f;
        }
    }
    bool still() const { return maxTravel < windowTravel && maxTurnDeg < windowTurnDeg; }
    void restart() { *this = HeadStill{}; }
};

} // namespace spidy::view_jitter
