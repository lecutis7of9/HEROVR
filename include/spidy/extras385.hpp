#pragma once
// Update 38.5 EXTRAS (the holo's 8th page, user requests of 3:04, 3:08 and 3:11): INVINCIBLE, FLY
// MODE, SYMBIOTE LOOK and its chest-pull gesture, PLAY ON MONITOR (the VR / game-screen switch with
// F8) and the prompt pass-through's settings. Every toggle is OFF by default and live.
//
// Pure and allocation-free (fixed members only, no strings built per frame). What each one touches
// in the game, and how sure that is:
// - INVINCIBLE writes ONE float: the hero health record's current hit points (+0xd0), only when the
//   record's vtable is the hero health table (0x541edd0, CONFIRMED live in run384: "health 197.7 /
//   200.0 (confirmed: it changed)"), raised to its max (+0xa0, CONFIRMED read). It never writes a
//   hero at 0 (no resurrection), so a one-shot or scripted death still happens. No save or story flag.
// - FLY MODE drives the hero through the swing's own native velocity command (native_movement,
//   CONFIRMED: every swing since October 5), zero gravity, only while airborne: you jump to take off
//   and it hands the hero back to the game's walk when you come down onto the ground. EXPERIMENTAL.
// - SYMBIOTE LOOK is our overlay only: black goo on the gloves, the symbiote webs and the mirror's
//   black-and-white palette. The game's own suit is not swapped: no suit-ID field or equip function
//   is confirmed (docs/mirror.md section 5/11). Equip the game's symbiote suit in its SUITS menu for
//   the real one; the mirror follows it (SuitColours learns the drawn suit).
// - PLAY ON MONITOR uses the shipping VIEW: GAME SCREEN path (live since October 15): the game's own
//   camera and Xbox-controller play, the headset session kept alive (never stopped, so no device is
//   lost and nothing restarts). The monitor always shows the game's own view in this port.
#include "gestures385.hpp"
#include "math.hpp"
#include "soft_shapes.hpp"
#include <vector>
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace spidy::extras385 {

// ---- INVINCIBLE ------------------------------------------------------------------------------
namespace invincible {
inline constexpr uint64_t healthTable = 0x541edd0; // CONFIRMED (run384): the hero health record's vtable
inline constexpr uint32_t currentAt = 0xd0, maxAt = 0xa0, reduceAt = 0x208; // CONFIRMED reads (game_player.hpp)
inline constexpr const char* help = "Keeps your health topped up; one-shot and scripted deaths still happen.";
// What to write into +0xd0 this frame, or a negative number for nothing. hp, most: the record's
// current and max; reduce: its +0x208 (the game's own cap is (1 - reduce) * max; NaN: unknown).
inline float refill(float hp, float most, float reduce) {
    if (!std::isfinite(hp) || !std::isfinite(most) || !(most > 0) || most > 1e5f)
        return -1;
    float target = most;
    if (std::isfinite(reduce) && reduce > 0 && reduce < 1)
        target = std::min(most, (1 - reduce) * most); // the game's own setter's cap, never above it
    if (!(hp > 0)) // dead or dying: never resurrect
        return -1;
    if (hp >= target - 1e-3f)
        return -1;
    return target;
}
} // namespace invincible

// ---- FLY MODE --------------------------------------------------------------------------------
namespace fly {
enum Speed : uint32_t { off = 0, slow = 1, normal = 2, fast = 3 };
inline constexpr float speeds[4] = {0, 8, 16, 40}; // m/s at full thrust (Version 0.0.5: FAST 28 -> 40)
// Version 0.0.5 SUPERSONIC (fly_boost.hpp): the dash's top, m/s (the movement module's ceiling).
inline constexpr float supersonicTop = 120;
inline const char* speedName(uint32_t s) {
    static constexpr const char* names[4] = {"OFF", "SLOW", "NORMAL", "FAST"};
    return names[s < 4 ? s : 0];
}
// Update 38.5 (owner steering 4:52 and 4:53): the poses fly; lowered arms hover; R3 lands you.
inline constexpr const char* help = "Fly: a hand up rises, both hands forward fly, a T-pose stops, R3 lands.";
inline constexpr const char* speedHelp = "How fast FLY MODE goes at full thrust: SLOW, NORMAL or FAST.";
inline constexpr const char* vignetteHelp = "Darkens the edge of your view while flying fast, for comfort.";

// ---- The poses (only while FLY MODE is on) ----
// Read from where the hands are against the head (the shoulders are gestures385::Tune's, below the
// eyes), with hysteresis: a pose starts on a strict test held a moment and ends on a loose one.
enum Pose : uint32_t { hover = 0, rise = 1, forward = 2, brake = 3 };
inline constexpr uint32_t poses = 4;
inline const char* poseName(uint32_t p) {
    static constexpr const char* names[poses] = {"HOVER (ARMS DOWN)", "RISE (ONE HAND UP)",
                                                 "FORWARD (BOTH HANDS IN FRONT)", "STOP (T-POSE)"};
    return names[p < poses ? p : 0];
}
// One sentence each (the cheat sheet and TRAINING).
inline const char* poseHelp(uint32_t p) {
    static constexpr const char* lines[poses] = {
        "Lower your arms to rest: you hover where you are and never drop.",
        "Hold one hand straight up to rise, faster the higher the hand is.",
        "Hold both hands out in front to fly the way they point, like Superman.",
        "Spread your arms straight out to the sides to brake smoothly to a hover."};
    return lines[p < poses ? p : 0];
}
inline constexpr const char* dropHelp = "Click the right stick on its own to drop and land; FLY MODE stays on.";
inline constexpr const char* ultimateHelp = "Click both sticks together to fire the ultimate, the same as before.";

// ---- The right stick's ROLL (FLY ROLL, ROLL SPEED, AUTO LEVEL) ----
enum RollMode : uint32_t { rollOff = 0, rollComfort = 1, rollFull = 2 };
inline constexpr uint32_t rollModes = 3, rollDefault = rollComfort;
inline const char* rollName(uint32_t m) {
    static constexpr const char* names[rollModes] = {"OFF", "COMFORT", "FULL"};
    return names[m < rollModes ? m : 0];
}
inline constexpr const char* rollHelp = "Right stick rolls you while flying; COMFORT adds a vignette and horizon.";
inline constexpr const char* rollSpeedHelp = "How fast the right stick rolls you while flying, in degrees a second.";
inline constexpr const char* autoLevelHelp = "Rolls you back level when you let go of the right stick while flying.";
inline constexpr float rollSpeedLow = 30, rollSpeedHigh = 180, rollSpeedStep = 15, rollSpeedDefault = 90;
inline constexpr float comfortLimitDegrees = 45;

// The swing command's spare word (game_swing::Command::reserved6, Update 38.5): the speed (bits
// 0-1), each hand's thrust 0-100 (bits 2-8, 9-15), the right stick's rise -100..100 (bits 16-23),
// the poses allowed (bit 24), the pose (bits 25-26), R3's drop (bit 27) and RISE's lift 0-15
// (bits 28-31).
inline uint32_t pack(uint32_t speed, float left, float right, float rise, bool arms, uint32_t pose = hover,
                     float lift = 0, bool drop = false) {
    const auto pct = [](float v) {
        return static_cast<uint32_t>(std::lround(std::clamp(std::isfinite(v) ? v : 0.f, 0.f, 1.f) * 100));
    };
    const int r = static_cast<int>(std::lround(std::clamp(std::isfinite(rise) ? rise : 0.f, -1.f, 1.f) * 100));
    const auto l = static_cast<uint32_t>(std::lround(std::clamp(std::isfinite(lift) ? lift : 0.f, 0.f, 1.f) * 15));
    return (speed & 3u) | pct(left) << 2 | pct(right) << 9 | (static_cast<uint32_t>(static_cast<int8_t>(r)) & 0xffu) << 16 |
           (arms ? 1u << 24 : 0u) | (pose & 3u) << 25 | (drop ? 1u << 27 : 0u) | (l & 15u) << 28;
}
struct Word {
    uint32_t speed{};
    float thrust[2]{}, rise{};
    bool arms{};
    uint32_t pose{};
    bool drop{};
    float lift{};
};
inline Word unpack(uint32_t w) {
    Word o;
    o.speed = w & 3u;
    o.thrust[0] = static_cast<float>(std::min(100u, (w >> 2) & 127u)) * .01f;
    o.thrust[1] = static_cast<float>(std::min(100u, (w >> 9) & 127u)) * .01f;
    o.rise = std::clamp(static_cast<int8_t>((w >> 16) & 0xffu) * .01f, -1.f, 1.f);
    o.arms = (w >> 24) & 1u;
    o.pose = o.arms ? (w >> 25) & 3u : 0u;
    o.drop = (w >> 27) & 1u;
    o.lift = static_cast<float>((w >> 28) & 15u) / 15.f;
    return o;
}

// The poses' reader (VR module side: the tracked hands against the head, tracking space).
struct PoseInput {
    float seconds{};
    bool live{};       // FLY MODE (or TRAINING) on, in play, X not held, no menu open
    Vec3 head{};       // the head's position
    Vec3 forward{};    // the head's look (flattened here)
    bool tracked[2]{};
    Vec3 hands[2]{};   // the grips' positions
};
class Poses {
  public:
    // Metres in the body's frame: x right, y up from the eyes, z forward (gestures385::Tune's
    // shoulder is -0.25, chest -0.40, waist -0.72).
    static constexpr float riseEnter = .12f, riseExit = 0.f, riseSide = .50f;
    static constexpr float frontEnter = .42f, frontExit = .32f, frontLow = -.70f, frontHigh = .30f, frontApart = .75f;
    static constexpr float spanEnter = 1.0f, spanExit = .85f, spanLow = -.60f, spanHigh = .20f, spanDepth = .40f;
    static constexpr float holdRise = .15f, holdFront = .25f, holdBrake = .15f, leaveSeconds = .15f;
    static constexpr float punchSpeed = 1.6f; // m/s: a hand faster than this is a punch, not a pose
    Pose update(const PoseInput& in) {
        const float dt = std::isfinite(in.seconds) ? std::clamp(in.seconds, 0.f, .1f) : 0.f;
        if (!in.live || !finite(in.head)) {
            reset();
            return pose_;
        }
        // 0.0.74 test 11: a hand's tracking blinking (or a junk sample) keeps the pose for the same
        // `leaveSeconds` a loose test failing gets, so a moment's loss no longer drops FORWARD (and the
        // SUPERSONIC dash with it). Lost for longer: nothing, as before.
        if (!in.tracked[0] || !in.tracked[1] || !finite(in.hands[0]) || !finite(in.hands[1])) {
            lost_ += dt;
            seen_ = false; // no hand speed across the gap
            cand_ = hover;
            held_ = 0;
            if (lost_ >= leaveSeconds)
                reset();
            return pose_;
        }
        lost_ = 0;
        Vec3 f{in.forward.x, 0, in.forward.z};
        f = finite(f) && length(f) > .1f ? normalized(f) : Vec3{0, 0, -1};
        const Vec3 right{-f.z, 0, f.x};
        Vec3 local[2];
        float fastest = 0;
        for (int i = 0; i < 2; ++i) {
            const Vec3 d = in.hands[i] - in.head;
            local[i] = {dot(d, right), d.y, dot(d, f)};
            if (seen_ && dt > 1e-4f)
                fastest = std::max(fastest, length(local[i] - last_[i]) / dt);
            last_[i] = local[i];
        }
        seen_ = true;
        lift_ = std::clamp((std::max(local[0].y, local[1].y) - .05f) / .30f, .2f, 1.f);
        // The engaged pose ends on its loose test failing for a moment.
        if (pose_ != hover) {
            leave_ = test(pose_, local, false) ? 0.f : leave_ + dt;
            if (leave_ >= leaveSeconds) {
                pose_ = hover;
                leave_ = 0;
            }
        }
        // A new one: the first strict test (STOP before RISE before FORWARD), held still a moment.
        Pose c = hover;
        for (const Pose p : {brake, rise, forward})
            if (test(p, local, true)) {
                c = p;
                break;
            }
        if (c == hover || c == pose_) {
            cand_ = hover;
            held_ = 0;
        } else {
            if (c != cand_) {
                cand_ = c;
                held_ = 0;
            }
            // A punch reaches the front fast and comes back: FORWARD wants the hands held still.
            if (c == forward && fastest > punchSpeed)
                held_ = 0;
            else
                held_ += dt;
            if (held_ >= hold(c)) {
                pose_ = c;
                ++engaged_[c];
                cand_ = hover;
                held_ = 0;
                leave_ = 0;
            }
        }
        return pose_;
    }
    static float hold(Pose p) { return p == forward ? holdFront : p == brake ? holdBrake : holdRise; }
    Pose pose() const { return pose_; }
    float lift() const { return pose_ == rise ? lift_ : 0.f; }
    // TRAINING's colours: 0 nothing, 1 RED (seen), 2 BLUE (half held), 3 SUCCESS (engaged).
    Pose shown() const { return cand_ != hover ? cand_ : pose_; }
    uint32_t stage() const {
        if (cand_ != hover)
            return held_ * 2 < hold(cand_) ? 1u : 2u;
        return pose_ != hover ? 3u : 0u;
    }
    uint32_t engaged(Pose p) const { return engaged_[p & 3u]; }
    void reset() {
        pose_ = cand_ = hover;
        held_ = leave_ = lift_ = lost_ = 0;
        seen_ = false;
    }
    static bool test(Pose p, const Vec3 (&h)[2], bool strict) {
        const Vec3 L = h[0], R = h[1];
        switch (p) {
        case brake: {
            const float span = length(Vec3{L.x - R.x, 0, L.z - R.z});
            return span >= (strict ? spanEnter : spanExit) && L.x < -.3f && R.x > .3f && L.y >= spanLow &&
                   L.y <= spanHigh && R.y >= spanLow && R.y <= spanHigh && std::fabs(L.z) <= spanDepth &&
                   std::fabs(R.z) <= spanDepth;
        }
        case rise: {
            const auto up = [&](Vec3 v) {
                return v.y >= (strict ? riseEnter : riseExit) && std::fabs(v.x) <= riseSide && v.z > -.35f &&
                       v.z < .55f;
            };
            return up(L) || up(R);
        }
        case forward:
            return L.z >= (strict ? frontEnter : frontExit) && R.z >= (strict ? frontEnter : frontExit) &&
                   L.y >= frontLow && L.y <= frontHigh && R.y >= frontLow && R.y <= frontHigh &&
                   std::fabs(L.x - R.x) <= frontApart;
        default:
            return false;
        }
    }

  private:
    Pose pose_{hover}, cand_{hover};
    float held_{}, leave_{}, lift_{}, lost_{};
    bool seen_{};
    Vec3 last_[2]{};
    uint32_t engaged_[poses]{};
};

// R3 and L3 while flying: R3 on its own drops you and lands you; L3+R3 pressed within `window` of
// each other (and let go before Start's hold) is still the game's ultimate.
class StickChord {
  public:
    static constexpr float window = .12f, tapSeconds = .10f;
    enum Result : uint32_t { none = 0, drop = 1, ultimate = 2 };
    Result update(float seconds, bool active, bool l3, bool r3, float pauseSeconds) {
        const float dt = std::isfinite(seconds) ? std::clamp(seconds, 0.f, .1f) : 0.f;
        tap_ = std::max(0.f, tap_ - dt);
        lAge_ = l3 ? lAge_ + dt : 0.f;
        const bool lFresh = l3 && lAge_ <= window + 1e-4f;
        Result r = none;
        if (!active) {
            spent_ = l3 || r3;
            down_ = chord_ = false;
            return none;
        }
        if (spent_) {
            spent_ = l3 || r3;
            return none;
        }
        if (r3 && !down_) {
            down_ = true;
            held_ = 0;
            chord_ = lFresh;
        } else if (down_ && r3) {
            held_ += dt;
            if (!chord_ && lFresh && lAge_ <= held_ + 1e-4f && held_ <= window + 1e-4f)
                chord_ = true; // L3 pressed just after R3
        }
        if (!down_)
            return none;
        if (chord_) {
            if (!l3 && !r3) {
                if (held_ < pauseSeconds) {
                    r = ultimate;
                    tap_ = tapSeconds;
                    ++ultimates_;
                }
                down_ = chord_ = false;
            }
        } else if (!r3 || held_ >= window) {
            r = drop;
            ++drops_;
            down_ = false;
            spent_ = r3;
        }
        return r;
    }
    bool tapping() const { return tap_ > 0; } // send the game L3+R3 while true
    bool claims() const { return down_; }     // R3 is the chord's: no web pull from it
    uint32_t drops() const { return drops_; }
    uint32_t ultimates() const { return ultimates_; }

  private:
    float lAge_{}, held_{}, tap_{};
    bool down_{}, chord_{}, spent_{};
    uint32_t drops_{}, ultimates_{};
};

// FLY ROLL: the right stick's x rolls the view about the look while flying under a pose or the
// sticks; it levels itself while hovering, landed or with FLY ROLL OFF. A mod-side view transform
// (GameTrackingRig::flyRoll, the tracking space tilted the way the flips and WALL ROLL are), not
// the game's camera.
class Roll {
  public:
    static constexpr float deadzone = .20f, levelOff = 180.f; // degrees a second back to level
    float update(float seconds, uint32_t mode, float degreesPerSecond, bool autoLevel, bool active, float stick) {
        constexpr float toRad = 3.14159265f / 180.f, pi = 3.14159265f;
        const float dt = std::isfinite(seconds) ? std::clamp(seconds, 0.f, .1f) : 0.f;
        const float rate = std::clamp(std::isfinite(degreesPerSecond) ? degreesPerSecond : rollSpeedDefault,
                                      rollSpeedLow, rollSpeedHigh) * toRad;
        const float s = std::isfinite(stick) && std::fabs(stick) > deadzone
                            ? (stick - std::copysign(deadzone, stick)) / (1 - deadzone)
                            : 0.f;
        stickRolls_ = active && mode != rollOff && mode < rollModes;
        const auto level = [&](float speed) {
            const float step = std::max(speed, std::fabs(angle_) * 2.f) * dt;
            angle_ = std::fabs(angle_) <= step ? 0.f : angle_ - std::copysign(step, angle_);
        };
        if (!stickRolls_) {
            level(std::max(rate, levelOff * toRad));
        } else if (s != 0) {
            angle_ += std::clamp(s, -1.f, 1.f) * rate * dt;
        } else if (autoLevel) {
            level(rate * .75f);
        }
        if (mode == rollComfort)
            angle_ = std::clamp(angle_, -comfortLimitDegrees * toRad, comfortLimitDegrees * toRad);
        if (angle_ > pi)
            angle_ -= 2 * pi;
        else if (angle_ < -pi)
            angle_ += 2 * pi;
        if (!std::isfinite(angle_))
            angle_ = 0;
        return angle_;
    }
    float angle() const { return angle_; } // radians, + rolls to the right (the right side drops)
    bool stickRolls() const { return stickRolls_; } // the right stick rolls (no turning) this frame
    // COMFORT's vignette: more the further from level (0..0.6).
    float comfort() const { return std::clamp(std::fabs(angle_) / (comfortLimitDegrees * 3.14159265f / 180.f), 0.f, 1.f) * .6f; }
    void reset() {
        angle_ = 0;
        stickRolls_ = false;
    }

  private:
    float angle_{};
    bool stickRolls_{};
};

struct Hand {
    bool tracked{};
    Vec3 forward{}; // the controller's aim, world, unit
    Vec3 offset{};  // the grip from the head, world-turned (metres)
};
struct In {
    float seconds{};
    Word word{};
    bool airborne{}, grounded{};
    Vec3 look{};     // the head's look, world, unit (zero: unknown)
    Vec3 move{};     // the left stick, world horizontal, |move| <= 1
    Vec3 velocity{}; // the body's now (a catch from a fall starts from it)
    Hand hands[2]{};
    float boost{};   // Version 0.0.5 SUPERSONIC: times FAST while hands-out (0 or <= 1: none)
};
struct Out {
    bool drive{};
    bool moving{};         // a pose, the triggers, the sticks or the drop propel (not HOVER / STOP)
    Vec3 velocity{};
    float vignette{};      // 0..0.6
    const char* how = "";  // THRUST, DROP, STOP, RISE, FORWARD, STICK, HOVER, LANDED, SUPERSONIC
    float top{};           // Version 0.0.5: this frame's top speed (FAST's, or SUPERSONIC's dash)
    bool supersonic{};     // Version 0.0.5: the dash flies now
    bool coasting{};       // 0.0.74 test 11: faster than this frame's top (after a dash), slowing gently
};
// 0.0.74 test 11 (players: "going SUPERSONIC stalls you and drops you lower"): the way the hands point
// during the dash (and the coast after it) flies level within `band` degrees of level; past it, the
// pitch climbs from 0 to straight up / down with no jump.
inline Vec3 levelled(Vec3 d, float bandDegrees) {
    constexpr float halfPi = 1.57079633f;
    const Vec3 h{d.x, 0, d.z};
    if (!finite(d) || length(h) < 1e-3f)
        return d;
    const float pitch = std::atan2(d.y, length(h));
    const float band = std::clamp(bandDegrees, 0.f, 80.f) * (halfPi / 90.f);
    const float a = std::fabs(pitch);
    const float p = a <= band ? 0.f : std::copysign((a - band) * halfPi / (halfPi - band), pitch);
    return normalized(h) * std::cos(p) + Vec3{0, std::sin(p), 0};
}
class Flight {
  public:
    static constexpr float thrustFrom = .15f, stickFrom = .15f;
    static constexpr float accelPerSpeed = 1.6f; // m/s^2 per m/s of top speed: NORMAL reaches 16 m/s in ~0.6 s
    static constexpr float brakeFactor = 2.5f;   // STOP brakes 2.5x as hard, smoothly
    static constexpr float riseShare = .6f, forwardShare = .85f;
    // 0.0.74 test 11: faster than the top (the dash let go) slows at `coastDecel` keeping its way, not
    // the old hard brake back to FLY's speed (80 -> 16 m/s in ~2 s read as a stall). STOP still brakes.
    static constexpr float coastDecel = 10.f;    // m/s^2
    static constexpr float levelBand = 15.f;     // degrees: the dash and the coast fly level inside this
    static constexpr float dashGraceSeconds = .4f; // the dash's hold while the hands-out pose blinks (fly_boost)
    static float dropSpeed(float top) { return std::max(4.f, top * .4f); }
    Out update(const In& in) {
        Out o;
        const float dt = std::isfinite(in.seconds) ? std::clamp(in.seconds, 0.f, .1f) : 0.f;
        const uint32_t speed = in.word.speed & 3u;
        if (!speed) {
            land();
            return o;
        }
        const uint32_t pose = in.word.arms ? in.word.pose & 3u : hover;
        // R3: the controlled descent, until the ground (or a RISE pose takes you up again).
        if (pose == rise)
            descending_ = false;
        else if (in.word.drop && flying_)
            descending_ = true;
        const bool boosting = std::isfinite(in.boost) && in.boost > 1.f;
        // 0.0.74 test 11: the dash goes on along its way while the hands-out pose blinks to HOVER (a
        // tracking drop-out) as long as the boost still asks (fly_boost holds it ~0.4 s).
        const bool held = flying_ && dashing_ && pose == hover && boosting && !descending_ &&
                          in.word.thrust[0] <= thrustFrom && in.word.thrust[1] <= thrustFrom;
        // Version 0.0.5 SUPERSONIC: hands out, in the air, FAST times the boost (never past supersonicTop).
        const bool sonic = flying_ && boosting && (pose == forward || held);
        const float top = sonic ? std::min(speeds[fast] * in.boost, supersonicTop) : speeds[speed];
        o.top = top;
        o.supersonic = sonic;
        const Vec3 up{0, 1, 0};
        Vec3 look = finite(in.look) && length(in.look) > .5f ? normalized(in.look) : Vec3{0, 0, -1};
        const float speedNow = length(v_);
        const bool coasting = flying_ && !sonic && pose != brake && !descending_ && speedNow > top + .01f;
        Vec3 target{};
        float accel = top * accelPerSpeed;
        // 1. A hand pointed with its trigger pulled: thrust along it, the trigger the speed.
        Vec3 dir{};
        float most = 0;
        for (int i = 0; i < 2; ++i) {
            const float t = in.word.thrust[i];
            if (t > thrustFrom && in.hands[i].tracked && finite(in.hands[i].forward) && length(in.hands[i].forward) > .5f) {
                dir = dir + normalized(in.hands[i].forward) * t;
                most = std::max(most, t);
            }
        }
        if (length(dir) > 1e-3f) {
            target = normalized(dir) * (top * std::min(1.f, (most - thrustFrom) / (1 - thrustFrom) * .8f + .2f));
            descending_ = false;
            o.how = "THRUST";
        } else if (descending_) {
            // 2. R3: straight down at a steady speed; the ground lands you (FLY MODE stays on).
            target = up * -dropSpeed(top);
            o.how = "DROP";
        } else if (pose == brake) {
            // 3. T-pose: stop, braking smoothly to a hover.
            accel *= brakeFactor;
            o.how = "STOP";
        } else if (pose == rise) {
            // 4. One hand up: rise, faster the higher the hand.
            target = up * (top * riseShare * std::clamp(in.word.lift, .2f, 1.f));
            o.how = "RISE";
        } else if (pose == forward || held) {
            // 5. Both hands in front: the way they point (Superman), the look if they are unknown.
            Vec3 d{};
            for (const auto& h : in.hands)
                if (h.tracked && finite(h.forward) && length(h.forward) > .5f)
                    d = d + normalized(h.forward);
            d = length(d) > .3f ? normalized(d) : look;
            if (sonic || coasting)
                d = levelled(d, levelBand); // 0.0.74 test 11: a hand a little low no longer sinks the dash
            if (held && length(dashWay_) > .5f)
                d = dashWay_;
            if (sonic)
                dashWay_ = d;
            target = d * (sonic ? top : top * forwardShare);
            o.how = sonic ? "SUPERSONIC" : "FORWARD";
        } else if (length(in.move) > stickFrom || std::fabs(in.word.rise) > stickFrom) {
            // 6. The sticks: the left moves (up and down with the head's look), the right rises / sinks.
            const float m = std::min(1.f, length(in.move));
            // 0.0.74 test 11: coasting out of the dash (the stick still forward), a look a little down
            // no longer sinks you; past the band it steers as before.
            const Vec3 lk = coasting ? levelled(look, levelBand) : look;
            target = in.move * top + up * (in.word.rise * top * .6f + lk.y * m * top);
            target = limited(target, top);
            o.how = "STICK";
        } else {
            o.how = "HOVER"; // arms down or resting: no gravity, no sink, held where you are
        }
        if (!flying_) {
            // On the ground the game walks; a jump, or the RISE pose, takes off.
            const bool lift = pose == rise && target.y > .5f;
            if (!lift && (!in.airborne || in.grounded))
                return o;
            flying_ = true;
            descending_ = false;
            ++takeoffs_;
            // A catch from a fall or a swing starts from its velocity: no snap.
            v_ = finite(in.velocity) ? limited(in.velocity, top) : Vec3{};
        }
        // Down onto the ground: the game's walk has the hero again (no fall: he is standing).
        if (in.grounded && target.y <= .5f) {
            land();
            ++landings_;
            o.how = "LANDED";
            return o;
        }
        if (coasting) {
            // 0.0.74 test 11: past the top (the dash let go): keep the way you were going (or the way
            // you steer) and slow at coastDecel. The vertical part is never stretched, so a coast never
            // speeds up a climb or a sink.
            const float keep = std::max(top, speedNow - coastDecel * dt);
            Vec3 h{target.x, 0, target.z};
            if (length(h) < .5f)
                h = {v_.x, 0, v_.z};
            if (length(h) > 1e-3f) {
                const float vy = std::clamp(target.y, -keep, keep);
                target = normalized(h) * std::sqrt(std::max(0.f, keep * keep - vy * vy)) + up * vy;
                accel = std::max(accel, speeds[fast] * accelPerSpeed); // turns like FAST does
            }
            o.coasting = true;
        }
        const Vec3 before = v_;
        v_ = v_ + limited(target - v_, accel * dt);
        dashing_ = sonic;
        o.drive = true;
        o.moving = pose != brake && length(target) > .5f;
        o.velocity = v_;
        const float a = dt > 0 ? length(v_ - before) / dt : 0;
        const float vTop = std::max(top, length(before)); // a coast is not darker than the dash was
        o.vignette = std::clamp(length(v_) / vTop * .3f + a / (vTop * accelPerSpeed) * .3f, 0.f, .6f);
        return o;
    }
    bool flying() const { return flying_; }
    bool descending() const { return descending_; }
    uint32_t takeoffs() const { return takeoffs_; }
    uint32_t landings() const { return landings_; }
    void land() {
        flying_ = descending_ = dashing_ = false;
        v_ = dashWay_ = {};
    }

  private:
    bool flying_{}, descending_{}, dashing_{};
    Vec3 v_{}, dashWay_{};
    uint32_t takeoffs_{}, landings_{};
};
// What the swing module's flight reports back (SpidyFlySample): the VR module draws the vignette,
// keeps the webs' buttons off while `flying`, rolls only while `moving` and holds R3's drop until
// the flight ends.
struct Data {
    uint32_t flying{}, takeoffs{}, landings{};
    float vignette{};
    uint32_t moving{}, descending{};
};
// 0.0.74 test 11: the flight's branch this frame for the VR module's FLY log line, through its own
// optional export (SpidyFlyTrace) so Data's size - shared with older modules - never changes.
struct Trace {
    uint32_t how{};      // howCode(Out::how)
    uint32_t coasting{}; // Out::coasting
    uint32_t supersonic{};
    float speed{};       // |Out::velocity|, m/s
    float top{};         // Out::top
    float vy{};          // Out::velocity.y
};
inline constexpr const char* hows[] = {"", "THRUST", "DROP", "STOP", "RISE", "FORWARD", "STICK", "HOVER", "LANDED", "SUPERSONIC"};
inline uint32_t howCode(const char* how) {
    if (!how)
        return 0;
    for (uint32_t i = 1; i < sizeof(hows) / sizeof(hows[0]); ++i)
        for (const char *a = how, *b = hows[i];; ++a, ++b) {
            if (*a != *b)
                break;
            if (!*a)
                return i;
        }
    return 0;
}
inline const char* howName(uint32_t code) { return code < sizeof(hows) / sizeof(hows[0]) ? hows[code] : ""; }
} // namespace fly

// ---- SYMBIOTE LOOK and its gesture --------------------------------------------------------------
namespace symbiote {
inline constexpr const char* help = "Black goo gloves, symbiote webs and mirror; the game's suit stays as equipped.";
inline constexpr int webStyle = 3; // web_style::symbiote
// The mirror's palette while on: near-black with the white spider (linear RGB).
inline constexpr Vec3 primary{.015f, .015f, .02f}, secondary{.05f, .05f, .06f}, emblem{.9f, .9f, .92f};
// Both hands grab the chest, then pull outward ("ripping the suit on"): a toggle of SYMBIOTE LOOK.
// Never with X held (the ability modifier's moves), never while a web carries you. The ULTIMATE's
// crossed arms put the left hand right of the middle: a grab never arms from that.
class ChestPull {
  public:
    static constexpr float hold = .3f, window = .5f, spread = .30f, cooldown = 1.5f;
    // Returns true the frame the pull completes.
    bool update(const gestures385::Input& in) {
        const float dt = std::isfinite(in.seconds) ? std::clamp(in.seconds, 0.f, .1f) : 0.f;
        now_ += dt;
        if (!in.play || in.xHeld || in.webAttached || in.holoBusy || !in.hands[0].tracked || !in.hands[1].tracked) {
            gripFor_ = 0;
            armedAt_ = -1e9f;
            return false;
        }
        Vec3 f{in.forward.x, 0, in.forward.z};
        f = length(f) > .1f ? normalized(f) : Vec3{0, 0, -1};
        const Vec3 right{-f.z, 0, f.x};
        const auto local = [&](Vec3 p) {
            const Vec3 d = p - in.head;
            return Vec3{dot(d, right), d.y, dot(d, f)};
        };
        const Vec3 L = local(in.hands[0].position), R = local(in.hands[1].position);
        const bool onChest = L.x >= -.20f && L.x <= .02f && R.x >= -.02f && R.x <= .20f && L.y > -.62f && L.y < -.18f &&
                             R.y > -.62f && R.y < -.18f && L.z > -.05f && L.z < .38f && R.z > -.05f && R.z < .38f &&
                             length(L - R) < .34f;
        gripFor_ = onChest ? gripFor_ + dt : 0;
        if (gripFor_ >= hold)
            armedAt_ = now_;
        if (!onChest && now_ - armedAt_ <= window && L.x <= -spread && R.x >= spread && now_ - firedAt_ > cooldown) {
            armedAt_ = -1e9f;
            firedAt_ = now_;
            ++fired_;
            return true;
        }
        return false;
    }
    bool armed() const { return now_ - armedAt_ <= window; }
    uint32_t fired() const { return fired_; }

  private:
    float now_{}, gripFor_{}, armedAt_ = -1e9f, firedAt_ = -1e9f;
    uint32_t fired_{};
};
// The goo's spread over the gloves: 0 -> 1 in `growSeconds` after it is put on, back to 0 as fast.
struct Spread {
    static constexpr float growSeconds = .8f;
    float amount{};
    void step(float dt, bool on) {
        if (!std::isfinite(dt))
            return;
        amount = std::clamp(amount + (on ? 1 : -1) * std::clamp(dt, 0.f, .1f) / growSeconds, 0.f, 1.f);
    }
};
// The goo on a glove (our overlay, the game's suit material untouched): gloveSpots glossy
// near-black blobs round the fist that writhe with `seconds`, each with a pale sheen spot, at
// `amount` 0..1 (Spread). Turned to the eye; at most 2 * gloveSpots quads a glove.
inline constexpr unsigned gloveSpots = 14;
inline size_t appendGlove(std::vector<Vertex>& out, Vec3 fist, Vec3 aim, float amount, uint32_t seed, Vec3 viewer,
                          float seconds) {
    const size_t before = out.size();
    if (!(amount > .01f) || !finite(fist) || !finite(viewer) || !finite(aim) || length(aim) < .5f ||
        !std::isfinite(seconds))
        return 0;
    const auto h = [](uint32_t x) {
        x ^= x >> 16;
        x *= 0x7feb352du;
        x ^= x >> 15;
        x *= 0x846ca68bu;
        x ^= x >> 16;
        return static_cast<float>(x & 0xffffffu) / 16777215.f;
    };
    const Vec3 forward = normalized(aim), side = soft::perpendicular(forward), up2 = cross(forward, side);
    const float a01 = std::clamp(amount, 0.f, 1.f);
    for (unsigned i = 0; i < gloveSpots; ++i) {
        // The goo climbs from the knuckles back over the wrist as it spreads.
        const float reach = h(seed + i * 31) ;
        if (reach > a01)
            continue;
        const float wob = std::sin(seconds * (2.f + 2.f * h(seed + i * 31 + 1)) + 6.2831853f * h(seed + i * 31 + 2));
        const float a = h(seed + i * 31 + 3) * 6.2831853f + .25f * wob;
        const float r = .028f + .012f * h(seed + i * 31 + 4);
        const Vec3 at = fist + forward * (.03f - .13f * reach) + (side * std::cos(a) + up2 * std::sin(a)) * r;
        const Vec3 view = viewer - at;
        const float d = length(view);
        if (!(d > .05f && d < 5))
            continue;
        const Vec3 f = view / d;
        Vec3 right = cross(Vec3{0, 1, 0}, f);
        right = length(right) > .1f ? normalized(right) : soft::perpendicular(f);
        const Vec3 up = cross(f, right);
        const Vec3 c = at + f * .014f;
        const float size = (.016f + .012f * h(seed + i * 31 + 5)) * (1 + .12f * wob);
        const Vec3 goo{.012f, .012f, .018f};
        const float alpha = .92f * a01;
        const Vec3 across = right * size, along = up * (size * 1.25f);
        soft::quad(out, {c - across - along, goo, alpha}, {c + across - along, goo, alpha},
                   {c + across + along, goo, alpha}, {c - across + along, goo, alpha});
        const Vec3 sheenAt = c + f * .002f + (right * .3f + up * .35f) * size;
        const Vec3 sheen{.30f, .32f, .38f};
        const Vec3 s1 = right * (size * .22f), s2 = up * (size * .16f);
        soft::quad(out, {sheenAt - s1 - s2, sheen, .55f * a01}, {sheenAt + s1 - s2, sheen, .55f * a01},
                   {sheenAt + s1 + s2, sheen, .55f * a01}, {sheenAt - s1 + s2, sheen, .55f * a01});
    }
    return out.size() - before;
}
} // namespace symbiote

// ---- PLAY ON MONITOR (the VR / game-screen switch) ----------------------------------------------
namespace view {
inline constexpr const char* help = "Play on the monitor with the game's camera; F8 or the pause menu brings VR back.";
inline constexpr const char* confirm = "PRESS AGAIN: MONITOR, F8 RETURNS";
inline constexpr float confirmSeconds = 4, hotkeyCooldown = .5f;
// What the session shows: the pause menu's VIEW (valuesFlat, the launcher's tab), the holo's
// confirmed press and the F8 key all move one wanted state; the newest wins. F8 toggles, once per
// press, at most every hotkeyCooldown s (a held key is one press).
class Switch {
  public:
    // request: -1 none, 0 VR, 1 the monitor. hotkey: F8 down now (the game window in front).
    bool update(float seconds, bool valuesFlat, int request, bool hotkey) {
        const float dt = std::isfinite(seconds) ? std::clamp(seconds, 0.f, .1f) : 0.f;
        cool_ = std::max(0.f, cool_ - dt);
        if (!seeded_) {
            seeded_ = true;
            lastValues_ = want_ = valuesFlat;
        }
        if (valuesFlat != lastValues_) {
            lastValues_ = valuesFlat;
            set(valuesFlat);
        }
        if (request == 0 || request == 1)
            set(request == 1);
        if (hotkey && !keyWas_ && cool_ <= 0) {
            set(!want_);
            cool_ = hotkeyCooldown;
            ++hotkeys_;
        }
        keyWas_ = hotkey;
        return want_;
    }
    bool wanted() const { return want_; }
    uint32_t switches() const { return switches_; }
    uint32_t hotkeys() const { return hotkeys_; }

  private:
    void set(bool flat) {
        if (flat != want_)
            ++switches_;
        want_ = flat;
    }
    bool seeded_{}, lastValues_{}, want_{}, keyWas_{};
    float cool_{};
    uint32_t switches_{}, hotkeys_{};
};
// The holo's PLAY ON MONITOR press: the first arms it (its value reads `confirm`), a second within
// confirmSeconds goes.
class Confirm {
  public:
    bool press(float now) {
        if (now - armedAt_ <= confirmSeconds) {
            armedAt_ = -1e9f;
            return true;
        }
        armedAt_ = now;
        return false;
    }
    bool armed(float now) const { return now - armedAt_ <= confirmSeconds; }

  private:
    float armedAt_ = -1e9f;
};
} // namespace view

// ---- PROMPTS --------------------------------------------------------------------------------
namespace prompts {
enum Handling : uint32_t { manual = 0, automatic = 1 };
inline constexpr const char* help = "AUTO: a prompt's buttons pass through by themselves (needs prompts hooked).";
inline constexpr const char* padHelp = "Buttons pass through 1:1 (grips bumpers); press again or click both sticks.";
inline constexpr const char* hint = "PROMPT: buttons pass through";
// SM2 has no prompt hook yet (sm2_port promptsNotPorted, code 9699): AUTO cannot see a prompt.
inline constexpr bool hooked = false;
} // namespace prompts

} // namespace spidy::extras385
