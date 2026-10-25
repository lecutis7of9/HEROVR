#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
//
// Version 0.0.2 (Update 38.6) TRAINING's guide: for the move picked with TRAIN MOVE, a glowing
// path in the air to follow - a ribbon from a start marker to an end marker with arrows along
// it, and a ghost hand that keeps doing the move. Body-relative (from the head, in the level
// forward's frame) and scaled to the arm, so it sits where the recogniser wants the hands.
//
//   RED       the move is waiting: put the hand(s) on the start marker and follow the path
//   BLUE      the attempt is following the path (or a pose is being held)
//   SUCCESS   the recogniser fired the move (or the FLY pose engaged)
//   FAIL      with the reason: off the path, too slow, not recognised, or the recogniser's own
//
// The paths: gesture_remap::references' (each built-in move as the recogniser expects it), the
// player's own recorded average (a remapped move, MINE), ABILITY LUNGE's head lunge, and FLY
// MODE's four poses as static targets. Pure, fixed size, no heap; tracking-space metres.
#include "gesture_remap.hpp"
#include "gestures385.hpp"
#include "math.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace spidy::training_guide {

constexpr int points = gesture_remap::points;
// The picker's moves: gestures385's, then FLY's four poses.
enum : int { flyRise = gestures385::count, flyForward, flyStop, flyHover, moves };
inline const char* moveName(int m) {
    static constexpr const char* fly[4] = {"FLY RISE", "FLY FORWARD", "FLY STOP", "FLY HOVER"};
    return m >= 0 && m < gestures385::count ? gestures385::gestureName(static_cast<uint32_t>(m))
           : m >= flyRise && m < moves     ? fly[m - flyRise]
                                           : "";
}
inline bool isPose(int m) { return m >= flyRise && m < moves; }

// A move's path: arm lengths, body-relative (x right, y up from the eyes, z forward).
struct Path {
    bool valid{}, custom{}, pose{}, head{};
    uint8_t hands{};      // bit 0 left, bit 1 right (head: bit 0 is the head)
    uint8_t buttons[2]{}; // gesture_remap::Button bits to hold
    float seconds = .4f;
    Vec3 p[2][points]{};
};
inline Path fromShape(const gesture_remap::Shape& s, bool custom) {
    Path out{};
    if (!s.valid)
        return out;
    out.valid = true;
    out.custom = custom;
    out.hands = s.hands & 3u;
    out.buttons[0] = s.anyButtons ? 0 : s.buttons[0];
    out.buttons[1] = s.anyButtons ? 0 : s.buttons[1];
    out.seconds = std::isfinite(s.seconds) ? std::clamp(s.seconds, .15f, 2.f) : .4f;
    for (int h = 0; h < 2; ++h)
        for (int i = 0; i < points; ++i)
            out.p[h][i] = finite(s.path[h][i]) ? s.path[h][i] : Vec3{};
    if (!out.hands) // a recording with no clear mover: show both
        out.hands = 3;
    return out;
}
// The path for picker entry `move`; `mine` the player's recorded move for it (or null).
inline Path pathFor(int move, const gesture_remap::Shape* mine) {
    if (move < 0 || move >= moves)
        return {};
    if (mine && mine->valid && move < gestures385::count)
        return fromShape(*mine, true);
    if (move == gestures385::abilityLunge) { // the head's: 20 cm forward and a little down, X held
        Path out{};
        out.valid = out.head = true;
        out.hands = 1;
        out.buttons[0] = gesture_remap::xButton;
        out.seconds = .30f;
        for (int i = 0; i < points; ++i) {
            float u = static_cast<float>(i) / (points - 1);
            u = u * u * (3 - 2 * u);
            out.p[0][i] = Vec3{0, -.04f * u, .22f * u} * (1.f / gesture_remap::defaultArm);
        }
        return out;
    }
    static constexpr const char* poseRefs[4] = {"FLY RISE", "FLY FORWARD", "FLY STOP", "FLY HOVER"};
    int n = 0;
    const gesture_remap::Reference* table = gesture_remap::references(n);
    for (int k = 0; k < n; ++k) {
        const gesture_remap::Reference& r = table[k];
        if (r.kind != gesture_remap::Kind::builtin)
            continue;
        const bool hit = isPose(move) ? std::strncmp(r.name, poseRefs[move - flyRise], std::strlen(poseRefs[move - flyRise])) == 0
                                      : r.gesture == move;
        if (!hit)
            continue;
        Path out = fromShape(r.shape, false);
        out.pose = isPose(move);
        if (out.pose) { // a pose is where both hands end: both shown, any buttons
            out.hands = 3;
            out.buttons[0] = out.buttons[1] = 0;
        }
        return out;
    }
    return {};
}

enum State : uint8_t { red = 0, blue = 1, success = 2, fail = 3 };
inline const char* stateName(uint32_t s) {
    static constexpr const char* names[4] = {"RED", "BLUE", "SUCCESS", "FAIL"};
    return s < 4 ? names[s] : "";
}

struct Input {
    float seconds{};       // since the last update
    bool live{};           // TRAINING on and the head tracked
    Vec3 head{}, look{};   // tracking space
    bool tracked[2]{};
    Vec3 hands[2]{};       // the grips
    float arm = gesture_remap::defaultArm;
    bool poseEngaged{};    // FLY: extras385::fly::Poses has this pose engaged
};

class Guide {
  public:
    static constexpr float resultSeconds = 1.6f, anchorSeconds = .5f, offHold = .15f, recogniseWait = .8f,
                           poseHold = .5f;
    void select(int move, const Path& path) {
        move_ = move;
        path_ = path;
        restart();
        anchored_ = false;
    }
    int move() const { return move_; }
    const Path& path() const { return path_; }
    // The recogniser's verdict on this move (game_xr: Recognizer events, the remap's fires).
    void result(bool ok, const char* reason) {
        if (!path_.valid || state_ == success || state_ == fail)
            return;
        if (ok)
            finish(success, "");
        else if (state_ == blue || complete_) // a fail only counts during an attempt
            finish(fail, reason && *reason ? reason : "NOT RECOGNISED");
    }
    void update(const Input& in) {
        const float dt = std::isfinite(in.seconds) ? std::clamp(in.seconds, 0.f, .1f) : 0.f;
        time_ += dt;
        if (!in.live || !path_.valid || !finite(in.head) || !finite(in.look)) {
            on_ = false;
            return;
        }
        on_ = true;
        arm_ = std::isfinite(in.arm) ? std::clamp(in.arm, .45f, .85f) : gesture_remap::defaultArm;
        tol_ = std::clamp(.22f * arm_, .10f, .17f);
        // The anchor follows the body (smoothed), frozen during an attempt and its result.
        Vec3 f{in.look.x, 0, in.look.z};
        if (length(f) > .1f) {
            f = normalized(f);
            if (!anchored_ || state_ == red) {
                const float a = anchored_ ? std::clamp(dt / anchorSeconds, 0.f, 1.f) : 1.f;
                head_ = anchored_ ? head_ + (in.head - head_) * a : in.head;
                const Vec3 g = normalized(forward_ + (f - forward_) * a);
                forward_ = anchored_ && length(g) > .5f ? g : f;
                anchored_ = true;
            }
        }
        if (!anchored_) {
            on_ = false;
            return;
        }
        for (int h = 0; h < 2; ++h)
            for (int i = 0; i < points; ++i)
                world_[h][i] = toTracking(path_.p[h][i]);
        // The ghost: the move over and over (a slow demo), with a pause at the end.
        const float run = std::clamp(path_.seconds * 3.f, 1.f, 2.4f), period = run + .6f;
        const float t = std::fmod(time_, period);
        float u = std::clamp(t / run, 0.f, 1.f);
        u = u * u * (3 - 2 * u);
        ghostU_ = u;
        ghostAlpha_ = std::clamp(t / .15f, 0.f, 1.f) * std::clamp((period - t) / .2f, 0.f, 1.f);
        if (state_ == success || state_ == fail) {
            shown_ += dt;
            if (shown_ >= resultSeconds)
                restart();
            return;
        }
        Vec3 at[2];
        bool seen[2];
        for (int h = 0; h < 2; ++h) {
            at[h] = path_.head && h == 0 ? in.head : in.hands[h];
            seen[h] = path_.head && h == 0 ? true : in.tracked[h] && finite(in.hands[h]);
        }
        if (path_.pose)
            return updatePose(in, at, seen, dt);
        // RED: every moving hand on its start marker arms the attempt; leaving along the path is BLUE.
        bool allStart = true, anyMoved = false, allEnd = true, off = false;
        for (int h = 0; h < 2; ++h) {
            if (!(path_.hands >> h & 1))
                continue;
            if (!seen[h]) {
                allStart = allEnd = false;
                continue;
            }
            if (state_ == red && !armed_) {
                index_[h] = 0;
                allStart = allStart && length(at[h] - world_[h][0]) <= tol_ * 1.3f;
                continue;
            }
            // Follow: the furthest of the next few points within reach.
            float best = 1e9f;
            int bestK = index_[h];
            for (int k = index_[h]; k < std::min(points, index_[h] + 9); ++k) {
                const float d = length(at[h] - world_[h][k]);
                if (d <= tol_)
                    bestK = k; // ascending: the furthest one within reach
                best = std::min(best, d);
            }
            index_[h] = bestK;
            near_[h] = best;
            anyMoved = anyMoved || index_[h] >= 5;
            allEnd = allEnd && index_[h] >= points - 4;
            off = off || best > tol_ * 2.2f;
        }
        if (state_ == red && !armed_) {
            armed_ = allStart && path_.hands;
            return;
        }
        if (state_ == red) {
            if (anyMoved) {
                state_ = blue;
                attempt_ = 0;
            } else if (off) // wandered from the start marker: not armed any more
                armed_ = false;
            return;
        }
        attempt_ += dt;
        offFor_ = off && !complete_ ? offFor_ + dt : 0.f;
        if (offFor_ >= offHold)
            return finish(fail, "OFF THE PATH: FOLLOW THE GHOST");
        if (!complete_ && attempt_ > std::max(2.5f, path_.seconds * 6))
            return finish(fail, "TOO SLOW: MOVE LIKE THE GHOST");
        if (allEnd && !complete_) {
            complete_ = true;
            wait_ = 0;
        }
        if (complete_) {
            wait_ += dt;
            if (wait_ >= recogniseWait)
                return finish(fail, needsX() ? "NOT RECOGNISED: HOLD X ALL THE WAY" : "NOT RECOGNISED: A BIT FASTER");
        }
    }

    // Outputs.
    bool on() const { return on_; }
    State state() const { return state_; }
    bool armed() const { return armed_; }
    const char* reason() const { return reason_; }
    float arm() const { return arm_; }
    float tolerance() const { return tol_; }
    // How far along each hand got (0..1).
    float progress(int h) const { return h == 0 || h == 1 ? static_cast<float>(index_[h]) / (points - 1) : 0.f; }
    Vec3 point(int h, int i) const { return world_[h & 1][std::clamp(i, 0, points - 1)]; }
    // The ghost hand's place (head for LUNGE) and how visible it is.
    Vec3 ghost(int h) const {
        const float x = ghostU_ * (points - 1);
        const int a = std::min(static_cast<int>(x), points - 2);
        const float s = x - static_cast<float>(a);
        return world_[h & 1][a] + (world_[h & 1][a + 1] - world_[h & 1][a]) * s;
    }
    float ghostAlpha() const { return ghostAlpha_; }
    // Above the path, where its words go.
    Vec3 labelAt() const { return head_ + forward_ * (.55f * arm_ / gesture_remap::defaultArm) + Vec3{0, .05f, 0}; }
    bool needsX() const {
        return ((path_.buttons[0] | path_.buttons[1]) & gesture_remap::xButton) != 0;
    }
    // A short instruction for the label (the move name goes in front of it).
    const char* instruction() const {
        switch (state_) {
        case blue:
            return path_.pose ? "HOLD IT" : "GOOD - KEEP GOING";
        case success:
            return "SUCCESS";
        case fail:
            return "FAIL";
        default:
            if (path_.pose)
                return "PUT YOUR HANDS ON THE RINGS";
            if (path_.head)
                return armed_ ? "HOLD X, LUNGE YOUR HEAD FORWARD" : "HOLD X, HEAD ON THE START";
            if (needsX())
                return armed_ ? "HOLD X AND FOLLOW THE PATH" : "HOLD X, HAND ON THE START";
            return armed_ ? "FOLLOW THE PATH" : "HAND ON THE START";
        }
    }
    uint32_t successes() const { return successes_; }
    uint32_t fails() const { return fails_; }

  private:
    Vec3 toTracking(Vec3 local) const {
        const Vec3 right{-forward_.z, 0, forward_.x};
        const Vec3 m = local * arm_;
        return head_ + right * m.x + Vec3{0, m.y, 0} + forward_ * m.z;
    }
    void updatePose(const Input& in, const Vec3 (&at)[2], const bool (&seen)[2], float dt) {
        bool on = true;
        for (int h = 0; h < 2; ++h) {
            near_[h] = seen[h] ? length(at[h] - world_[h][points - 1]) : 1e9f;
            on = on && near_[h] <= tol_ * 1.6f;
            index_[h] = on ? points - 1 : 0;
        }
        if (!on) {
            state_ = red;
            attempt_ = 0;
            return;
        }
        state_ = blue;
        attempt_ += dt;
        if (in.poseEngaged || attempt_ >= poseHold * 2)
            finish(success, "");
    }
    void finish(State s, const char* reason) {
        state_ = s;
        reason_ = reason;
        shown_ = 0;
        (s == success ? successes_ : fails_)++;
    }
    void restart() {
        state_ = red;
        armed_ = complete_ = false;
        index_[0] = index_[1] = 0;
        near_[0] = near_[1] = 0;
        attempt_ = offFor_ = wait_ = shown_ = 0;
        reason_ = "";
    }
    int move_ = 0;
    Path path_{};
    bool on_{}, anchored_{}, armed_{}, complete_{};
    State state_ = red;
    const char* reason_ = "";
    Vec3 head_{}, forward_{0, 0, -1};
    float arm_ = gesture_remap::defaultArm, tol_ = .13f;
    float time_{}, ghostU_{}, ghostAlpha_{}, attempt_{}, offFor_{}, wait_{}, shown_{};
    int index_[2]{};
    float near_[2]{};
    Vec3 world_[2][points]{};
    uint32_t successes_{}, fails_{};
};

// What the eyes draw (game_xr: world space, written once a frame, read by both eyes).
struct Draw {
    bool on{}, pose{}, head{}, armed{};
    uint8_t state{}, hands{};
    float progress[2]{}, ghostAlpha{}, pulse{};
    Vec3 p[2][points]{}, ghost[2]{};
};

} // namespace spidy::training_guide
