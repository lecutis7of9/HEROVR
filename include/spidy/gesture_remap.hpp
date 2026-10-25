#pragma once
// Update 38.5 GESTURE REMAP: the player records their own motion for a move (a gadget, an ability,
// a special or a wings move) on the hologram, three takes: take 1's trail RED, a take 2 that matches
// it BLUE, a take 3 that matches GREEN - then it is checked against every other move, the built-in
// ones, the player's other custom ones, everyday motion (waving, punching, swinging, checking the
// wrist) and the SAFETY gestures (opening the hologram, PLAY ON MONITOR), and mapped.
//
// A motion is each hand's path relative to the body (the head and its level forward, so turning or
// swinging through the city is not part of it), divided by the player's arm length, resampled to
// `points` samples; plus which hands moved, the buttons each held most of the time (trigger, grip,
// X) and how long it took. Two motions are compared with banded DTW (dynamic time warping), after
// checks that say WHY one does not match: HANDS, BUTTONS, SPEED, DIRECTION, then SHAPE. The match
// tolerance is learned from the three takes' spread and kept forgiving (`minTolerance` .. `maxTolerance`).
//
// Pure and allocation-free in the frame path (fixed arrays; reasons and names are literals). Saving and
// loading build strings, never per frame. Nothing here reads or writes the game.
#include "gestures385.hpp"
#include "math.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace spidy::gesture_remap {

constexpr int points = 32;                       // samples per hand
constexpr int ring = 256;                        // frames kept (2.8 s at 90 Hz)
constexpr int takes = 3;
constexpr int slots = gestures385::count;        // one custom motion per move
constexpr int profiles = 4;                      // PROFILE 1-4
constexpr float defaultArm = .60f;               // metres, until calibrated
constexpr float minTolerance = .18f, maxTolerance = .35f, learnTolerance = .30f; // arm lengths, mean
constexpr float startSpeed = .60f, stillSpeed = .35f, stillHold = .25f, maxSeconds = 2.f, minSeconds = .15f;
constexpr float minTravel = .12f;               // metres, the most-moving hand
constexpr float speedLow = .6f, speedHigh = 1.6f, directionDegrees = 45.f;
constexpr float cooldown = .8f;
constexpr int band = 6;                          // DTW's band (samples)

enum Button : uint8_t { trigger = 1, grip = 2, xButton = 4 };
enum Reason : uint8_t { matched, hands, buttons, speed, direction, shape, tooShort, reasons };
inline const char* reasonText(uint32_t r) {
    static constexpr const char* text[reasons] = {
        "", "HANDS - use the same hand or hands as take 1", "BUTTONS - hold the same buttons as take 1",
        "SPEED - about as fast as take 1", "DIRECTION - move the same way as take 1",
        "SHAPE - the path was different", "TOO SHORT - make a bigger move"};
    return r < reasons ? text[r] : "";
}
inline const char* reasonWord(uint32_t r) {
    static constexpr const char* text[reasons] = {"", "HANDS", "BUTTONS", "SPEED", "DIRECTION", "SHAPE", "TOO SHORT"};
    return r < reasons ? text[r] : "";
}

// One motion (a take, a saved custom move or a reference).
struct Shape {
    bool valid{};
    uint8_t hands{};      // bit 0 left, bit 1 right: the hands that moved
    uint8_t buttons[2]{}; // Button bits each hand held most of the motion
    bool anyButtons{};    // a reference that happens with any trigger or grip (a punch, a wave); X still counts
    float seconds{};
    float tolerance = minTolerance;
    Vec3 path[2][points]{}; // body-relative, arm lengths (x right, y up, z forward)
};

// Each hand's frame, body-relative (head-relative in the level forward's frame), metres.
struct Sample {
    float t{};
    Vec3 local[2]{};
    bool tracked[2]{};
    uint8_t buttons[2]{};
};

namespace detail {
inline Vec3 lerp(Vec3 a, Vec3 b, float s) {
    return a + (b - a) * s;
}
inline float angleDegrees(Vec3 a, Vec3 b) {
    const float la = length(a), lb = length(b);
    if (la < 1e-6f || lb < 1e-6f)
        return 0;
    return std::acos(std::clamp(dot(a, b) / (la * lb), -1.f, 1.f)) * 57.2957795f;
}
} // namespace detail

// The mean per-sample distance of two paths of one hand, with banded DTW (two rows, no heap).
inline float dtw(const Vec3 (&a)[points], const Vec3 (&b)[points]) {
    constexpr float inf = 1e30f;
    std::array<float, points> prev{}, cur{};
    std::array<int, points> prevN{}, curN{};
    prev.fill(inf);
    for (int i = 0; i < points; ++i) {
        cur.fill(inf);
        const int lo = std::max(0, i - band), hi = std::min(points - 1, i + band);
        for (int j = lo; j <= hi; ++j) {
            const float d = length(a[i] - b[j]);
            float best = inf;
            int n = 0;
            if (i == 0 && j == 0) {
                best = 0;
            } else {
                if (i > 0 && prev[static_cast<size_t>(j)] < best) {
                    best = prev[static_cast<size_t>(j)];
                    n = prevN[static_cast<size_t>(j)];
                }
                if (j > 0 && cur[static_cast<size_t>(j - 1)] < best) {
                    best = cur[static_cast<size_t>(j - 1)];
                    n = curN[static_cast<size_t>(j - 1)];
                }
                if (i > 0 && j > 0 && prev[static_cast<size_t>(j - 1)] < best) {
                    best = prev[static_cast<size_t>(j - 1)];
                    n = prevN[static_cast<size_t>(j - 1)];
                }
            }
            if (best >= inf)
                continue;
            cur[static_cast<size_t>(j)] = best + d;
            curN[static_cast<size_t>(j)] = n + 1;
        }
        prev = cur;
        prevN = curN;
    }
    const float total = prev[points - 1];
    const int n = prevN[points - 1];
    return total < inf && n > 0 ? total / static_cast<float>(n) : inf;
}

// The shape distance over the hands `mask` (the mean of the hands').
inline float distance(const Shape& a, const Shape& b, uint8_t mask) {
    float sum = 0;
    int n = 0;
    for (int h = 0; h < 2; ++h)
        if (mask & (1u << h)) {
            sum += dtw(a.path[h], b.path[h]);
            ++n;
        }
    return n ? sum / static_cast<float>(n) : 1e30f;
}

struct Match {
    Reason reason = shape;
    float distance = 1e30f;
};
// Does `a` (a fresh motion) match `t` (a take or a saved move) within `tolerance`? The first
// reason it does not, in the order a player can fix it.
inline Match match(const Shape& a, const Shape& t, float tolerance, bool checkButtons = true) {
    Match m;
    if (!a.valid || !t.valid)
        return m;
    if (a.hands != t.hands || !a.hands) {
        m.reason = hands;
        return m;
    }
    // Everyday motion happens with any trigger or grip; X (the ability modifier, the left
    // controller's) is never an accident, so it counts whichever hands move.
    if (checkButtons) {
        const bool any = a.anyButtons || t.anyButtons;
        if ((a.buttons[0] & xButton) != (t.buttons[0] & xButton)) {
            m.reason = buttons;
            return m;
        }
        for (int h = 0; h < 2 && !any; ++h)
            if ((t.hands & (1u << h)) && (a.buttons[h] & 3) != (t.buttons[h] & 3)) {
                m.reason = buttons;
                return m;
            }
    }
    const float ratio = t.seconds > 1e-3f ? a.seconds / t.seconds : 0.f;
    if (!(ratio >= speedLow && ratio <= speedHigh)) {
        m.reason = speed;
        return m;
    }
    for (int h = 0; h < 2; ++h)
        if (t.hands & (1u << h)) {
            const Vec3 da = a.path[h][points - 1] - a.path[h][0], dt = t.path[h][points - 1] - t.path[h][0];
            if (length(da) > .15f && length(dt) > .15f && detail::angleDegrees(da, dt) > directionDegrees) {
                m.reason = direction;
                return m;
            }
        }
    m.distance = distance(a, t, t.hands);
    m.reason = m.distance <= tolerance ? matched : shape;
    return m;
}

// The body frame: the head's level forward, smoothed so a quick look around is not a motion.
struct BodyFrame {
    Vec3 forward{0, 0, -1};
    bool started{};
    void update(Vec3 look, float dt) {
        look.y = 0;
        if (length(look) < 1e-4f || !finite(look))
            return;
        look = normalized(look);
        if (!started) {
            forward = look;
            started = true;
            return;
        }
        const float a = std::clamp(dt / .30f, 0.f, 1.f);
        const Vec3 f = normalized(forward + (look - forward) * a);
        if (length(f) > .5f)
            forward = f;
    }
    Vec3 local(Vec3 p, Vec3 head) const {
        const Vec3 right = normalized(cross(forward, Vec3{0, 1, 0}));
        const Vec3 d = p - head;
        return {dot(d, right), d.y, dot(d, forward)};
    }
};

// Cuts the frames into motions: from a hand passing `startSpeed` (a few frames before it) to both
// hands under `stillSpeed` for `stillHold`, or `maxSeconds`. Fixed ring, no heap.
class Capture {
  public:
    struct Frame {
        float seconds{};      // since the last
        Vec3 head{}, look{};  // tracking space
        Vec3 hand[2]{};
        bool tracked[2]{};
        uint8_t buttons[2]{};
    };
    // A frame in. True when a motion just ended: `shape()` is it (valid unless too short).
    bool push(const Frame& f, float arm) {
        ended_ = false;
        const float dt = std::isfinite(f.seconds) ? std::clamp(f.seconds, 0.f, .1f) : 0.f;
        if (dt <= 0 || !finite(f.head) || !(f.tracked[0] || f.tracked[1])) {
            if (moving_)
                abandon();
            return false;
        }
        now_ += dt;
        body_.update(f.look, dt);
        Sample& s = ring_[static_cast<size_t>(at_)];
        s.t = now_;
        for (int h = 0; h < 2; ++h) {
            s.tracked[h] = f.tracked[h] && finite(f.hand[h]);
            s.local[h] = s.tracked[h] ? body_.local(f.hand[h], f.head) : (filled_ ? prior(1).local[h] : Vec3{});
            s.buttons[h] = f.buttons[h];
        }
        filled_ = std::min(filled_ + 1, ring);
        float speedNow = 0;
        if (filled_ > 2) {
            const Sample& b = prior(2);
            const float span = s.t - b.t;
            for (int h = 0; h < 2; ++h)
                if (span > 1e-4f && s.tracked[h])
                    speedNow = std::max(speedNow, length(s.local[h] - b.local[h]) / span);
        }
        speed_ = speedNow;
        if (!moving_) {
            if (speedNow >= startSpeed && filled_ > 4) {
                moving_ = true;
                start_ = (at_ - std::min(6, filled_ - 1) + ring) % ring;
                stillFor_ = 0;
            }
        } else {
            if (speedNow < stillSpeed) {
                if (stillFor_ == 0)
                    stillAt_ = at_;
                stillFor_ += dt;
            } else {
                stillFor_ = 0;
            }
            const float length_ = s.t - ring_[static_cast<size_t>(start_)].t;
            if (stillFor_ >= stillHold || length_ >= maxSeconds) {
                const int end = stillFor_ >= stillHold ? stillAt_ : at_;
                cut(start_, end, arm);
                moving_ = false;
                ended_ = true;
            }
        }
        at_ = (at_ + 1) % ring;
        return ended_;
    }
    const Shape& shape() const {
        return shape_;
    }
    bool tooShort() const {
        return ended_ && !shape_.valid;
    }
    bool moving() const {
        return moving_;
    }
    float speed() const {
        return speed_;
    }
    void reset() {
        filled_ = 0;
        moving_ = ended_ = false;
        stillFor_ = 0;
        shape_ = Shape{};
    }
    // The last motion's or the one being made's path, for the hologram's trail (arm lengths).
    int trail(Vec3 (&out)[points], int hand) const {
        if (!moving_) {
            if (!shape_.valid)
                return 0;
            for (int i = 0; i < points; ++i)
                out[i] = shape_.path[hand & 1][i];
            return points;
        }
        const int n = (at_ - start_ + ring) % ring;
        if (n < 2)
            return 0;
        for (int i = 0; i < points; ++i) {
            const int k = (start_ + (n - 1) * i / (points - 1)) % ring;
            out[i] = ring_[static_cast<size_t>(k)].local[hand & 1] * (1.f / defaultArm);
        }
        return points;
    }

  private:
    const Sample& prior(int back) const {
        return ring_[static_cast<size_t>((at_ - back + ring * 2) % ring)];
    }
    void abandon() {
        moving_ = false;
        stillFor_ = 0;
    }
    void cut(int from, int to, float arm) {
        shape_ = Shape{};
        const float scale = 1.f / (std::isfinite(arm) && arm > .3f && arm < 1.2f ? arm : defaultArm);
        const int n = (to - from + ring) % ring + 1;
        if (n < 4)
            return;
        const auto at = [&](int i) -> const Sample& { return ring_[static_cast<size_t>((from + i) % ring)]; };
        const float t0 = at(0).t, t1 = at(n - 1).t;
        if (t1 - t0 < minSeconds)
            return;
        float travel[2]{}, most = 0;
        int held[2][3]{};
        for (int i = 1; i < n; ++i)
            for (int h = 0; h < 2; ++h) {
                travel[h] += length(at(i).local[h] - at(i - 1).local[h]);
                for (int b = 0; b < 3; ++b)
                    held[h][b] += (at(i).buttons[h] >> b) & 1;
            }
        most = std::max(travel[0], travel[1]);
        if (most < minTravel)
            return;
        for (int h = 0; h < 2; ++h) {
            if (travel[h] >= .35f * most && travel[h] >= .08f)
                shape_.hands |= static_cast<uint8_t>(1u << h);
            for (int b = 0; b < 3; ++b)
                if (held[h][b] * 2 > n - 1)
                    shape_.buttons[h] |= static_cast<uint8_t>(1u << b);
        }
        // Resampled by time.
        int k = 0;
        for (int i = 0; i < points; ++i) {
            const float t = t0 + (t1 - t0) * static_cast<float>(i) / static_cast<float>(points - 1);
            while (k + 2 < n && at(k + 1).t < t)
                ++k;
            const Sample& a = at(k);
            const Sample& b = at(std::min(k + 1, n - 1));
            const float span = b.t - a.t;
            const float s = span > 1e-6f ? std::clamp((t - a.t) / span, 0.f, 1.f) : 0.f;
            for (int h = 0; h < 2; ++h)
                shape_.path[h][i] = detail::lerp(a.local[h], b.local[h], s) * scale;
        }
        shape_.seconds = t1 - t0;
        shape_.valid = true;
    }
    std::array<Sample, ring> ring_{};
    BodyFrame body_{};
    Shape shape_{};
    int at_{}, filled_{}, start_{}, stillAt_{};
    float now_{}, stillFor_{}, speed_{};
    bool moving_{}, ended_{};
};

// The mean of shapes (pointwise) and their tolerance from the spread, forgiving.
inline Shape learn(const Shape* shapes, int n) {
    Shape m{};
    if (n < 1)
        return m;
    m = shapes[0];
    float seconds = 0;
    for (int h = 0; h < 2; ++h)
        for (int i = 0; i < points; ++i) {
            Vec3 sum{};
            for (int k = 0; k < n; ++k)
                sum += shapes[k].path[h][i];
            m.path[h][i] = sum * (1.f / static_cast<float>(n));
        }
    for (int k = 0; k < n; ++k)
        seconds += shapes[k].seconds;
    m.seconds = seconds / static_cast<float>(n);
    float spread = 0;
    for (int k = 0; k < n; ++k)
        spread = std::max(spread, distance(shapes[k], m, m.hands));
    m.tolerance = std::clamp(std::max(minTolerance, 2.5f * spread), minTolerance, maxTolerance);
    m.valid = true;
    return m;
}

// The references a new motion is checked against.
enum class Kind : uint8_t { none, builtin, custom, everyday, safety };
struct Reference {
    const char* name;
    Kind kind;
    uint8_t gesture; // builtin: gestures385::Gesture; else count
    Shape shape;
};
namespace refs {
// A path through key points (metres, body-relative), smoothstep between them.
struct Keys {
    Vec3 k[3];
    int n;
};
inline void fill(Vec3 (&out)[points], const Keys& keys) {
    for (int i = 0; i < points; ++i) {
        const float u = static_cast<float>(i) / static_cast<float>(points - 1) * static_cast<float>(keys.n - 1);
        const int a = std::min(static_cast<int>(u), keys.n - 2);
        float s = u - static_cast<float>(a);
        s = s * s * (3 - 2 * s);
        out[i] = detail::lerp(keys.k[a], keys.k[a + 1], s) * (1.f / defaultArm);
    }
}
inline Shape make(Keys left, Keys right, uint8_t hands, uint8_t bl, uint8_t br, float seconds, bool any = false) {
    Shape s{};
    s.valid = true;
    s.hands = hands;
    s.buttons[0] = bl;
    s.buttons[1] = br;
    s.anyButtons = any;
    s.seconds = seconds;
    s.tolerance = .22f;
    fill(s.path[0], left);
    fill(s.path[1], right);
    return s;
}
inline Keys still(Vec3 p) {
    return {{p, p, p}, 2};
}
inline Keys line(Vec3 a, Vec3 b) {
    return {{a, b, b}, 2};
}
inline Keys via(Vec3 a, Vec3 b, Vec3 c) {
    return {{a, b, c}, 3};
}
inline Vec3 mirror(Vec3 p) {
    return {-p.x, p.y, p.z};
}
constexpr Vec3 restL{-.20f, -.55f, .25f}, restR{.20f, -.55f, .25f};
constexpr uint8_t L = 1, R = 2, both = 3, X = xButton, T = trigger, G = grip;
} // namespace refs

// The built-in moves as the recogniser expects them (gestures385's design, synthesised), the
// everyday motions and the safety gestures. ABILITY LUNGE is the head's, not the hands': none.
inline const Reference* references(int& n) {
    using namespace refs;
    static const Reference table[] = {
        {"ABILITY PUNCH", Kind::builtin, gestures385::abilityPunch,
         make(still(restL), line({.15f, -.40f, .15f}, {.10f, -.30f, .65f}), R, X, 0, .30f)},
        {"ABILITY SLAM", Kind::builtin, gestures385::abilitySlam,
         make(line({-.12f, -.35f, .30f}, {-.12f, -.85f, .35f}), line({.12f, -.35f, .30f}, {.12f, -.85f, .35f}), both, X, 0, .35f)},
        {"ABILITY SCOOP", Kind::builtin, gestures385::abilityScoop,
         make(line({-.15f, -.75f, .30f}, {-.15f, .10f, .35f}), line({.15f, -.75f, .30f}, {.15f, .10f, .35f}), both, X, 0, .50f)},
        {"ULTIMATE", Kind::builtin, gestures385::ultimate,
         make(line({.05f, -.40f, .25f}, {-.55f, -.25f, .20f}), line({-.05f, -.40f, .25f}, {.55f, -.25f, .20f}), both, X, 0, .50f)},
        {"CAMOUFLAGE", Kind::builtin, gestures385::camo,
         make(still(restL), line({.0f, .05f, .15f}, {.0f, -.25f, .20f}), R, 0, 0, .35f)},
        {"PARRY", Kind::builtin, gestures385::parry,
         make(still(restL), line({.15f, -.55f, .25f}, {-.05f, -.05f, .25f}), R, 0, 0, .25f)},
        {"FINISHER", Kind::builtin, gestures385::finisher,
         make(line({-.05f, .25f, .25f}, {-.05f, -.45f, .35f}), line({.05f, .25f, .25f}, {.05f, -.45f, .35f}), both, 0, 0, .35f)},
        {"WINGS OPEN", Kind::builtin, gestures385::wingsOpen,
         make(line(restL, {-.75f, -.25f, .0f}), line(restR, {.75f, -.25f, .0f}), both, 0, 0, .50f)},
        {"WINGS BOOST", Kind::builtin, gestures385::wingsBoost,
         make(line({-.70f, -.10f, 0}, {-.50f, -.60f, 0}), line({.70f, -.10f, 0}, {.50f, -.60f, 0}), both, 0, 0, .30f)},
        {"WINGS CLOSE", Kind::builtin, gestures385::wingsClose,
         make(line({-.70f, -.25f, 0}, {-.20f, -.80f, .05f}), line({.70f, -.25f, 0}, {.20f, -.80f, .05f}), both, 0, 0, .50f)},
        {"WINGS DIVE", Kind::builtin, gestures385::wingsDive,
         make(via({-.70f, -.25f, 0}, {-.10f, -.30f, .40f}, {-.70f, -.25f, 0}),
              via({.70f, -.25f, 0}, {.10f, -.30f, .40f}, {.70f, -.25f, 0}), both, 0, 0, .80f)},
        {"SKIM JUMP", Kind::builtin, gestures385::skimJump,
         make(line({-.65f, -.15f, 0}, {-.45f, -.65f, .05f}), line({.65f, -.15f, 0}, {.45f, -.65f, .05f}), both, 0, 0, .30f)},
        {"RICOCHET", Kind::builtin, gestures385::ricochet,
         make(still(restL), line({-.10f, -.35f, .35f}, {.45f, -.35f, .35f}), R, 0, T | G, .25f)},
        {"UPSHOT", Kind::builtin, gestures385::upshot,
         make(still(restL), line({.20f, -.70f, .40f}, {.20f, -.10f, .45f}), R, 0, T, .25f)},
        {"GRABBER", Kind::builtin, gestures385::grabber,
         make(still(restL), line({.20f, -.35f, .60f}, {.20f, -.40f, .15f}), R, 0, T, .25f)},
        {"SONIC", Kind::builtin, gestures385::sonic,
         make(line({-.25f, -.40f, .30f}, {-.04f, -.35f, .50f}), line({.25f, -.40f, .30f}, {.04f, -.35f, .50f}), both, T, T, .35f)},
        // Update 38.5 FLY MODE's poses (extras385::fly::Poses, any buttons): a recorded move
        // that looks like one is flagged. R3's drop and L3+R3's ultimate are buttons, not motion.
        {"FLY RISE (ONE HAND UP)", Kind::builtin, gestures385::count,
         make(still(restL), line(restR, {.15f, .25f, .10f}), R, 0, 0, .50f, true)},
        {"FLY FORWARD (BOTH HANDS IN FRONT)", Kind::builtin, gestures385::count,
         make(line(restL, {-.15f, -.25f, .55f}), line(restR, {.15f, -.25f, .55f}), both, 0, 0, .50f, true)},
        {"FLY STOP (T-POSE)", Kind::builtin, gestures385::count,
         make(line(restL, {-.80f, -.22f, -.02f}), line(restR, {.80f, -.22f, -.02f}), both, 0, 0, .50f, true)},
        {"FLY HOVER (ARMS LOWERED)", Kind::builtin, gestures385::count,
         make(line({-.25f, -.40f, .40f}, {-.22f, -.85f, .05f}), line({.25f, -.40f, .40f}, {.22f, -.85f, .05f}), both, 0, 0, .50f, true)},
        // Everyday motion (any buttons): what play does all the time.
        {"WAVING", Kind::everyday, gestures385::count,
         make(still(restL), via({.30f, .0f, .30f}, {.55f, .05f, .30f}, {.30f, .0f, .30f}), R, 0, 0, .80f, true)},
        {"A PUNCH (RIGHT)", Kind::everyday, gestures385::count,
         make(still(restL), line({.15f, -.40f, .15f}, {.10f, -.30f, .65f}), R, 0, 0, .25f, true)},
        {"A PUNCH (LEFT)", Kind::everyday, gestures385::count,
         make(line({-.15f, -.40f, .15f}, {-.10f, -.30f, .65f}), still(restR), L, 0, 0, .25f, true)},
        {"A WEB SWING (RIGHT)", Kind::everyday, gestures385::count,
         make(still(restL), line({.30f, .20f, .40f}, {.20f, -.50f, .10f}), R, 0, T, .60f)},
        {"A WEB SWING (LEFT)", Kind::everyday, gestures385::count,
         make(line({-.30f, .20f, .40f}, {-.20f, -.50f, .10f}), still(restR), L, T, 0, .60f)},
        {"A WEB SWING (BOTH)", Kind::everyday, gestures385::count,
         make(line({-.30f, .20f, .40f}, {-.20f, -.50f, .10f}), line({.30f, .20f, .40f}, {.20f, -.50f, .10f}), both, T, T, .60f)},
        {"THE RADAR (A WRIST CHECK)", Kind::everyday, gestures385::count,
         make(line({-.25f, -.55f, .20f}, {-.15f, -.40f, .30f}), still(restR), L, 0, 0, .40f, true)},
        // Safety: never anything else's, whatever the buttons.
        {"OPENING THE HOLOGRAM (FOREARM TAP)", Kind::safety, gestures385::count,
         make(still({-.15f, -.45f, .30f}), line({.25f, -.50f, .35f}, {-.12f, -.42f, .30f}), R, 0, T, .40f, true)},
        {"OPENING THE HOLOGRAM (PALM LOOK)", Kind::safety, gestures385::count,
         make(line(restL, {-.08f, -.30f, .35f}), still(restR), L, 0, 0, .50f, true)},
        {"PLAY ON MONITOR", Kind::safety, gestures385::count,
         make(still({-.15f, -.40f, .30f}), line({.20f, -.50f, .30f}, {-.10f, -.30f, .40f}), R, 0, T, .45f, true)},
    };
    n = static_cast<int>(sizeof(table) / sizeof(table[0]));
    return table;
}

// The closest reference (or other custom move) a new motion is too close to.
struct Conflict {
    Kind kind = Kind::none;
    const char* name = "";
    float distance = 1e30f;
    int index = -1; // the custom slot, or the reference's
};
// Too close: the shapes are within the larger tolerance (either would be taken for the other);
// a reference that happens with any buttons ignores them. `target`'s own built-in is the
// move being replaced, so not a conflict; a built-in already replaced by a custom one is
// checked as that custom one. Safety gestures first, whatever else is closer.
inline Conflict conflictOf(const Shape& s, uint32_t target, const Shape (&custom)[slots], int extraCount = 0,
                           const Shape* extra = nullptr, const char* const* extraNames = nullptr) {
    Conflict best{}, safety{};
    const auto consider = [&](const Shape& r, Kind kind, const char* name, int index) {
        if (!r.valid)
            return;
        const float tol = std::max(s.tolerance, r.tolerance) * 1.1f;
        const Match m = match(s, r, tol);
        if (m.reason != matched)
            return;
        Conflict& slot = kind == Kind::safety ? safety : best;
        if (m.distance < slot.distance)
            slot = {kind, name, m.distance, index};
    };
    int n = 0;
    const Reference* table = references(n);
    for (int i = 0; i < n; ++i) {
        const Reference& r = table[i];
        if (r.kind == Kind::builtin && (r.gesture == target || (r.gesture < slots && custom[r.gesture].valid)))
            continue;
        consider(r.shape, r.kind, r.name, i);
    }
    for (int g = 0; g < slots; ++g)
        if (static_cast<uint32_t>(g) != target)
            consider(custom[g], Kind::custom, gestures385::gestureName(static_cast<uint32_t>(g)), g);
    for (int i = 0; i < extraCount && extra; ++i)
        consider(extra[i], Kind::everyday, extraNames ? extraNames[i] : "EVERYDAY MOTION", 1000 + i);
    return safety.kind != Kind::none ? safety : best;
}

// RECORD's steps, one move at a time.
enum class Step : uint8_t { idle, take1, take2, take3, conflict, blocked, saved, cancelled };
inline const char* stepName(Step s) {
    switch (s) {
    case Step::idle: return "";
    case Step::take1: return "TAKE 1 OF 3 - MAKE THE MOVE";
    case Step::take2: return "TAKE 2 OF 3 - DO IT AGAIN";
    case Step::take3: return "TAKE 3 OF 3 - ONCE MORE";
    case Step::conflict: return "TOO CLOSE TO ANOTHER MOVE";
    case Step::blocked: return "THAT IS A SAFETY GESTURE";
    case Step::saved: return "SAVED";
    case Step::cancelled: return "CANCELLED";
    }
    return "";
}
// The trail's colour: none, RED (take 1 in), BLUE (take 2 matched), GREEN (take 3: saved).
enum class Trail : uint8_t { none, red, blue, green };

class Learner {
  public:
    void begin(uint32_t target) {
        target_ = target < slots ? target : 0u;
        step_ = Step::take1;
        trail_ = Trail::none;
        reason_ = matched;
        taken_ = 0;
        conflict_ = {};
        result_ = Shape{};
    }
    // A take in. Returns true when it changed anything the hologram shows.
    bool offer(const Shape& s, const Shape (&custom)[slots], int extraCount = 0, const Shape* extra = nullptr,
               const char* const* extraNames = nullptr) {
        if (step_ != Step::take1 && step_ != Step::take2 && step_ != Step::take3)
            return false;
        if (!s.valid) {
            reason_ = tooShort;
            return true;
        }
        if (step_ == Step::take1) {
            takes_[0] = s;
            taken_ = 1;
            step_ = Step::take2;
            trail_ = Trail::red;
            reason_ = matched;
            return true;
        }
        const Shape sofar = learn(takes_, taken_);
        const Match m = match(s, sofar, learnTolerance);
        reason_ = m.reason;
        lastDistance_ = m.distance;
        if (m.reason != matched)
            return true; // "didn't match, try again": the reason says why; the trail stays
        takes_[taken_++] = s;
        if (step_ == Step::take2) {
            step_ = Step::take3;
            trail_ = Trail::blue;
            return true;
        }
        trail_ = Trail::green;
        result_ = learn(takes_, taken_);
        conflict_ = conflictOf(result_, target_, custom, extraCount, extra, extraNames);
        step_ = conflict_.kind == Kind::safety ? Step::blocked : conflict_.kind != Kind::none ? Step::conflict : Step::saved;
        return true;
    }
    // The popup's answers.
    void keepAnyway() {
        if (step_ == Step::conflict)
            step_ = Step::saved; // never for a safety gesture
    }
    void tryAgain() {
        if (step_ == Step::conflict || step_ == Step::blocked)
            begin(target_);
    }
    void cancel() {
        step_ = Step::cancelled;
        trail_ = Trail::none;
    }
    bool popup() const {
        return step_ == Step::conflict || step_ == Step::blocked;
    }
    bool recording() const {
        return step_ == Step::take1 || step_ == Step::take2 || step_ == Step::take3;
    }
    Step step() const {
        return step_;
    }
    Trail trail() const {
        return trail_;
    }
    Reason reason() const {
        return reason_;
    }
    float lastDistance() const {
        return lastDistance_;
    }
    uint32_t target() const {
        return target_;
    }
    const Conflict& conflict() const {
        return conflict_;
    }
    const Shape& result() const {
        return result_;
    }
    const Shape& take(int k) const {
        return takes_[std::clamp(k, 0, takes - 1)];
    }
    int taken() const {
        return taken_;
    }

  private:
    Shape takes_[takes]{};
    Shape result_{};
    Conflict conflict_{};
    uint32_t target_{};
    int taken_{};
    float lastDistance_{};
    Step step_ = Step::idle;
    Trail trail_ = Trail::none;
    Reason reason_ = matched;
};

// Play: each finished motion against the saved custom moves; the best match fires, at most one
// per `cooldown`. Never while the hologram is busy (its own gestures are the safety ones). A
// motion nearer one of the everyday play motions (`extra`) than to the move does not fire: that
// was normal play that happened to look a bit like it.
class Detector {
  public:
    struct Fired {
        int gesture = -1;
        float distance{};
    };
    Fired update(const Capture::Frame& f, float arm, bool holoBusy, const Shape (&custom)[slots],
                 int extraCount = 0, const Shape* extra = nullptr) {
        Fired out;
        const float dt = std::isfinite(f.seconds) ? std::clamp(f.seconds, 0.f, .1f) : 0.f;
        now_ += dt;
        if (holoBusy) {
            capture_.reset();
            return out;
        }
        if (!capture_.push(f, arm) || !capture_.shape().valid || now_ - last_ < cooldown)
            return out;
        float best = 1e30f;
        for (int g = 0; g < slots; ++g) {
            if (!custom[g].valid)
                continue;
            const Match m = match(capture_.shape(), custom[g], custom[g].tolerance);
            if (m.reason == matched && m.distance < best) {
                best = m.distance;
                out.gesture = g;
                out.distance = m.distance;
            }
        }
        for (int i = 0; out.gesture >= 0 && extra && i < extraCount; ++i)
            if (extra[i].valid && extra[i].hands == capture_.shape().hands &&
                distance(capture_.shape(), extra[i], extra[i].hands) < best) {
                out = {};
                rejected_++;
            }
        if (out.gesture >= 0)
            last_ = now_;
        return out;
    }
    const Capture& capture() const {
        return capture_;
    }
    void reset() {
        capture_.reset();
    }
    int rejected() const {
        return rejected_;
    }

  private:
    Capture capture_{};
    float now_{}, last_ = -1e9f;
    int rejected_{};
};

// Saved moves: %APPDATA%\Spidy\gestures.txt (outside the mod's folder, so an update keeps it).
// Version 2: "SPIDY GESTURES 2", "active <profile>", then per profile "profile <n>" and per
// custom move "move <key> <hands> <buttonsL> <buttonsR> <seconds> <tolerance>" and one line of
// 2*points*3 numbers. Version 1 (38.5 beta: no profiles, no tolerance) loads into profile 1
// with the minimum tolerance and is written back as 2.
struct Store {
    int active{}; // 0..profiles-1
    Shape moves[profiles][slots]{};
    int version{};  // the file's, as loaded (0 none)
    bool migrated{};
};
inline int slotOfKey(const char* key) {
    for (uint32_t g = 0; g < static_cast<uint32_t>(slots); ++g)
        if (std::strcmp(gestures385::gestureKey(g), key) == 0)
            return static_cast<int>(g);
    return -1;
}
inline std::string save(const Store& s) {
    std::string out = "SPIDY GESTURES 2\n";
    char line[160];
    std::snprintf(line, sizeof line, "active %d\n", std::clamp(s.active, 0, profiles - 1) + 1);
    out += line;
    for (int p = 0; p < profiles; ++p) {
        std::snprintf(line, sizeof line, "profile %d\n", p + 1);
        out += line;
        for (int g = 0; g < slots; ++g) {
            const Shape& m = s.moves[p][g];
            if (!m.valid)
                continue;
            std::snprintf(line, sizeof line, "move %s %u %u %u %.4f %.4f\n", gestures385::gestureKey(static_cast<uint32_t>(g)),
                          m.hands, m.buttons[0], m.buttons[1], static_cast<double>(m.seconds),
                          static_cast<double>(m.tolerance));
            out += line;
            for (int h = 0; h < 2; ++h)
                for (int i = 0; i < points; ++i) {
                    std::snprintf(line, sizeof line, "%s%.4f %.4f %.4f", h || i ? " " : "",
                                  static_cast<double>(m.path[h][i].x), static_cast<double>(m.path[h][i].y),
                                  static_cast<double>(m.path[h][i].z));
                    out += line;
                }
            out += "\n";
        }
    }
    return out;
}
// Anything unreadable is skipped (a move, or the whole file if it is not ours); never throws.
inline Store load(const std::string& text) {
    Store s{};
    size_t at = 0;
    const auto next = [&](std::string& line) {
        if (at >= text.size())
            return false;
        size_t end = text.find('\n', at);
        if (end == std::string::npos)
            end = text.size();
        line.assign(text, at, end - at);
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        at = end + 1;
        return true;
    };
    std::string line;
    if (!next(line))
        return s;
    if (line == "SPIDY GESTURES 2")
        s.version = 2;
    else if (line == "SPIDY GESTURES 1")
        s.version = 1;
    else
        return s;
    int profile = 0;
    while (next(line)) {
        char key[48]{};
        int n = 0;
        if (std::sscanf(line.c_str(), "active %d", &n) == 1) {
            s.active = std::clamp(n - 1, 0, profiles - 1);
            continue;
        }
        if (std::sscanf(line.c_str(), "profile %d", &n) == 1) {
            profile = std::clamp(n - 1, 0, profiles - 1);
            continue;
        }
        unsigned hands = 0, bl = 0, br = 0;
        float seconds = 0, tolerance = minTolerance;
        const int got = s.version == 2 ? std::sscanf(line.c_str(), "move %47s %u %u %u %f %f", key, &hands, &bl, &br,
                                                     &seconds, &tolerance)
                                       : std::sscanf(line.c_str(), "move %47s %u %u %u %f", key, &hands, &bl, &br, &seconds);
        if (got != (s.version == 2 ? 6 : 5))
            continue;
        std::string numbers;
        if (!next(numbers))
            break;
        const int g = slotOfKey(key);
        if (g < 0 || hands < 1 || hands > 3 || !(seconds > 0 && seconds <= maxSeconds * 1.5f))
            continue;
        Shape m{};
        const char* c = numbers.c_str();
        bool ok = true;
        for (int h = 0; h < 2 && ok; ++h)
            for (int i = 0; i < points && ok; ++i) {
                float v[3];
                for (float& x : v) {
                    char* e = nullptr;
                    x = std::strtof(c, &e);
                    if (e == c || !std::isfinite(x) || std::abs(x) > 10) {
                        ok = false;
                        break;
                    }
                    c = e;
                }
                if (ok)
                    m.path[h][i] = {v[0], v[1], v[2]};
            }
        if (!ok)
            continue;
        m.valid = true;
        m.hands = static_cast<uint8_t>(hands);
        m.buttons[0] = static_cast<uint8_t>(bl & 7);
        m.buttons[1] = static_cast<uint8_t>(br & 7);
        m.seconds = seconds;
        m.tolerance = std::isfinite(tolerance) ? std::clamp(tolerance, minTolerance, maxTolerance) : minTolerance;
        s.moves[profile][g] = m;
    }
    s.migrated = s.version == 1;
    return s;
}

} // namespace spidy::gesture_remap
