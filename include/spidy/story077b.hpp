#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
// HERO VR 0.0.77 STORY MOVES: the story's own ability, throw and dodge inputs in VR (the player, pt1755u:
// "he has to do the electric punch on sandman ... Can I choose the direction of the web throws ... dodging the
// thrown items is hard").
// The game's controls (PUBLISHED, the SM2 control lists; the PC pad: Square = X, Triangle = Y, Circle = B,
// Cross = A, L1 = LB, R1 = RB):
//   - Abilities: hold LB, then X / Y / B / A (the four ability slots of the hero in play: Peter's Spider
//     Barrage / Rush / Shock / Whiplash and their symbiote forms, the second spider's Venom Punch / Thunder
//     Burst / Venom Smash / Galvanize and their later forms). Ultimate (Symbiote Surge, Mega Venom Blast):
//     L3 + R3. Camouflage (the second spider): D-pad up. Heal: D-pad down. Finisher: Y + B. Parry: LB tapped.
//     Throw the thing in reach (the Sandman opener's debris): LB + RB held. Dodge: B; a perfect dodge is B
//     just before the blow, when the spider-sense turns red.
//   - VR play sent none of the LB chords: the left grip and trigger are webs (game_pad::fromControllers), and
//     only the pad layer (both sticks clicked) had them, with no webs.
// AbilityButton: VR X held is the ability button. While it is held a punch is LB + X, Y (or the right stick
// pushed up) LB + Y, B LB + B, A LB + A, the right stick's click L3 + R3, the right stick pushed down D-pad up
// (camouflage). LB goes down 50 ms before the face button (the game's ComboSender lead, gestures385) and
// stays down while X is; each face press lasts at least 0.2 s. X tapped alone (menuTap) is the game's X now (story077e.hpp; x_button view: the 0.1 s View it always
// was; X held 0.8 s with nothing pressed goes back to the old X (the gadget wheel, or View held).
// ThrowAim: while the game's LB + RB goes out and 0.4 s after it, the left stick points where the player
// looks (or the right hand points), turned into the game camera's frame: the game throws at the enemy the
// hero faces (CANDIDATE, the walkthroughs: "move the left stick to face the enemy").
// DodgeAssist: B is the game's B everywhere the player plays (also in the air and on walls, where the swing
// owned him and B never went out), at least 0.12 s long; a quick sideways step of the head is a dodge that
// way (head_dodge); spider-sense's NOW (its timed warning under 0.3 s to the blow) buzzes both hands.
// Pure and allocation-free; nothing here reads or writes the game. Made by Agent Mulder.
#include "math.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace spidy::story077b {

// The XInput bits (game_pad::Button; checked with static_asserts in game_xr.cpp).
namespace pad {
constexpr uint16_t dpadUp = 0x1, dpadDown = 0x2, back = 0x20, l3 = 0x40, r3 = 0x80, lb = 0x100, rb = 0x200,
                   a = 0x1000, b = 0x2000, x = 0x4000, y = 0x8000;
}

// The ability slots, as counted and named.
enum Slot : uint32_t { slotX = 0, slotY = 1, slotB = 2, slotA = 3, slotUltimate = 4, slotCamo = 5, slots = 6 };
inline const char* slotName(uint32_t s) {
    static constexpr const char* names[] = {"LB + X", "LB + Y", "LB + B", "LB + A", "L3 + R3 (ULTIMATE)",
                                            "D-PAD UP (CAMO)"};
    return s < slots ? names[s] : "NONE";
}
inline uint16_t slotFace(uint32_t s) {
    static constexpr uint16_t face[] = {pad::x, pad::y, pad::b, pad::a, pad::l3 | pad::r3, pad::dpadUp};
    return s < slots ? face[s] : 0;
}
inline bool slotNeedsLb(uint32_t s) { return s <= slotA; }

class AbilityButton {
  public:
    static constexpr float lead = .05f, press = .2f, tapSeconds = .35f, giveBackSeconds = .8f;
    struct In {
        float seconds{};   // this frame's length
        bool on{};         // story_abilities on, a spider in gameplay, nothing else holding the controls
        bool xHeld{};      // VR X
        bool punch{};      // the fight's X this frame (a punch it sends)
        bool y{}, b{}, a{}; // VR buttons held
        bool stickUp{}, stickDown{}, stickClick{}; // the right stick
    };
    struct Out {
        bool owned{};     // X is the ability button now: no X (View, wheel) to the old path, no face presses
        bool giveBack{};  // X held long with no chord: the old X for the rest of this hold
        bool menuTap{};   // X tapped alone: the old 0.1 s View
        uint16_t buttons{}; // the game's presses to add
        int fired = -1;   // a slot started this frame
    };
    Out update(const In& in) {
        Out o;
        const float dt = std::isfinite(in.seconds) ? std::clamp(in.seconds, 0.f, .1f) : 0.f;
        const bool x = in.on && in.xHeld;
        bool edge[slots]{};
        edge[slotX] = in.punch && !punchWas_;
        edge[slotY] = (in.y && !yWas_) || (in.stickUp && !upWas_);
        edge[slotB] = in.b && !bWas_;
        edge[slotA] = in.a && !aWas_;
        edge[slotUltimate] = in.stickClick && !clickWas_;
        edge[slotCamo] = in.stickDown && !downWas_;
        punchWas_ = in.punch, yWas_ = in.y, upWas_ = in.stickUp, bWas_ = in.b, aWas_ = in.a;
        clickWas_ = in.stickClick, downWas_ = in.stickDown;
        if (!in.on) {
            reset();
            return o;
        }
        if (x && !holding_) {
            holding_ = true, chorded_ = false, giveBack_ = false, lbChord_ = false, held_ = 0;
        }
        if (holding_ && x)
            held_ += dt;
        if (holding_ && !x) {
            if (!chorded_ && !giveBack_ && held_ < tapSeconds) {
                o.menuTap = true;
                ++taps;
            }
            holding_ = false, giveBack_ = false;
        }
        if (holding_ && !chorded_ && !giveBack_ && held_ >= giveBackSeconds && send_ < 0) {
            giveBack_ = true;
            ++givenBack;
        }
        if (holding_ && !giveBack_) {
            for (uint32_t s = 0; s < slots; ++s)
                if (edge[s] && (send_ < 0 || sendAt_ > lead + press)) {
                    send_ = static_cast<int>(s), sendAt_ = 0, chorded_ = true;
                    lbChord_ = slotNeedsLb(s);
                    ++fired[s];
                    o.fired = static_cast<int>(s);
                    break;
                }
        }
        // The press under way plays out, even once X is let go.
        if (send_ >= 0) {
            const uint32_t s = static_cast<uint32_t>(send_);
            const bool lbFirst = slotNeedsLb(s);
            const float faceFrom = lbFirst ? lead : 0.f;
            if (lbFirst)
                o.buttons |= pad::lb;
            const bool sourceHeld = s == slotY ? (in.y || in.stickUp)
                                    : s == slotB ? in.b
                                    : s == slotA ? in.a
                                    : s == slotUltimate ? in.stickClick
                                    : s == slotCamo ? in.stickDown
                                                    : false;
            if (sendAt_ >= faceFrom)
                o.buttons |= slotFace(s);
            sendAt_ += dt;
            if (sendAt_ >= faceFrom + press && !(sourceHeld && x))
                send_ = -1;
        }
        // LB stays down while X is held after an LB chord: the next press is one face button away.
        if (holding_ && chorded_ && lbChord_ && !giveBack_)
            o.buttons |= pad::lb;
        o.owned = holding_ && !giveBack_;
        o.giveBack = holding_ && giveBack_;
        if (o.owned || send_ >= 0)
            ++ownedFrames;
        return o;
    }
    bool sending() const { return send_ >= 0; }
    bool holding() const { return holding_ && !giveBack_; }
    void reset() {
        holding_ = chorded_ = giveBack_ = lbChord_ = false;
        held_ = sendAt_ = 0;
        send_ = -1;
    }
    uint32_t fired[slots]{};
    uint32_t taps{}, givenBack{}, ownedFrames{};

  private:
    bool holding_{}, chorded_{}, giveBack_{}, lbChord_{};
    float held_{}, sendAt_{};
    int send_ = -1;
    bool punchWas_{}, yWas_{}, upWas_{}, bWas_{}, aWas_{}, clickWas_{}, downWas_{};
};

// live.txt story_throw_aim: 0 look (the head's way), 1 point (the right hand's ray, latched as the throw
// starts; pointed far down it is the head's way), 2 off.
enum AimMode : uint32_t { aimLook = 0, aimPoint = 1, aimOff = 2 };
inline const char* aimName(uint32_t m) {
    static constexpr const char* names[] = {"LOOK", "POINT", "OFF"};
    return m <= aimOff ? names[m] : "OFF";
}
class ThrowAim {
  public:
    static constexpr float after = .4f, strength = .6f, idle = .25f;
    struct Out {
        bool active{};
        float right{}, forward{}; // the stick, -1..1
        float degrees{};          // the aim off the camera's ahead, + to the right
    };
    // throwing: the game's LB + RB goes out this frame. look / point: unit-ish ways in the camera's space;
    // ahead: the game camera's ahead (camera[8], camera[10]); stickX / stickY the player's own left stick.
    Out update(float dt, uint32_t mode, bool throwing, Vec3 look, Vec3 point, Vec3 ahead, float stickX,
               float stickY) {
        Out o;
        dt = std::isfinite(dt) ? std::clamp(dt, 0.f, .1f) : 0.f;
        if (mode >= aimOff) {
            left_ = 0, was_ = false;
            return o;
        }
        if (throwing && !was_) {
            latched_ = pick(mode, look, point);
            ++throws;
        }
        was_ = throwing;
        if (throwing)
            left_ = after;
        else
            left_ = std::max(0.f, left_ - dt);
        if (left_ <= 0)
            return o;
        const Vec3 way = mode == aimPoint ? latched_ : pick(aimLook, look, point);
        ahead = Vec3{ahead.x, 0, ahead.z};
        const float theirs = std::hypot(std::isfinite(stickX) ? stickX : 0.f, std::isfinite(stickY) ? stickY : 0.f);
        if (length(ahead) < .3f || length(way) < .3f)
            return o;
        if (theirs > idle) { // the player's own stick wins
            ++overridden;
            return o;
        }
        ahead = normalized(ahead);
        const Vec3 w = normalized(way);
        o.forward = strength * dot(w, ahead);
        o.right = strength * dot(w, cross(ahead, Vec3{0, 1, 0}));
        o.degrees = std::atan2(o.right, o.forward) * 57.2957795f;
        o.active = true;
        lastDegrees = o.degrees;
        ++frames;
        return o;
    }
    uint32_t throws{}, frames{}, overridden{};
    float lastDegrees{};

  private:
    static Vec3 level(Vec3 v) { return finite(v) ? Vec3{v.x, 0, v.z} : Vec3{}; }
    static Vec3 pick(uint32_t mode, Vec3 look, Vec3 point) {
        // A hand pointed more than 50 degrees down says nothing of a way across the ground.
        if (mode == aimPoint && finite(point) && length(point) > .3f && normalized(point).y > -.77f &&
            length(level(point)) > .3f)
            return level(point);
        return level(look);
    }
    Vec3 latched_{};
    float left_{};
    bool was_{};
};

// live.txt head_dodge: 0 fight (only while enemies are near or a spider-sense warning stands), 1 always,
// 2 off.
enum HeadMode : uint32_t { headFight = 0, headAlways = 1, headOff = 2 };
inline const char* headName(uint32_t m) {
    static constexpr const char* names[] = {"FIGHT", "ALWAYS", "OFF"};
    return m <= headOff ? names[m] : "OFF";
}
// A quick sideways step or lean of the head (CANDIDATE: 0.16 m along the head's level right within 0.25 s,
// not much up or down), once per 0.6 s.
class HeadSidestep {
  public:
    static constexpr float window = .25f, distance = .16f, upDown = .12f, rest = .6f;
    // Returns -1 (left), 1 (right) or 0.
    int update(float dt, bool on, Vec3 head, Vec3 headRight) {
        dt = std::isfinite(dt) ? std::clamp(dt, 0.f, .1f) : 0.f;
        cool_ = std::max(0.f, cool_ - dt);
        if (!on || !finite(head) || !finite(headRight)) {
            count_ = 0;
            return 0;
        }
        for (int i = 0; i < count_; ++i)
            ages_[i] += dt;
        while (count_ > 0 && ages_[0] > window) { // drop the oldest
            for (int i = 1; i < count_; ++i)
                ages_[i - 1] = ages_[i], at_[i - 1] = at_[i];
            --count_;
        }
        if (count_ == kept) {
            for (int i = 1; i < count_; ++i)
                ages_[i - 1] = ages_[i], at_[i - 1] = at_[i];
            --count_;
        }
        ages_[count_] = 0, at_[count_] = head, ++count_;
        Vec3 right{headRight.x, 0, headRight.z};
        if (count_ < 2 || cool_ > 0 || length(right) < .3f)
            return 0;
        right = normalized(right);
        const Vec3 moved = head - at_[0];
        const float side = dot(Vec3{moved.x, 0, moved.z}, right);
        if (std::abs(side) >= distance && std::abs(moved.y) <= upDown) {
            cool_ = rest;
            count_ = 0;
            ++steps;
            return side > 0 ? 1 : -1;
        }
        return 0;
    }
    uint32_t steps{};

  private:
    static constexpr int kept = 48;
    Vec3 at_[kept]{};
    float ages_[kept]{};
    int count_{};
    float cool_{};
};

// B as the game's dodge: every press at least `least` long; a head sidestep is a press with the stick
// that way for as long.
class DodgeAssist {
  public:
    static constexpr float least = .12f, stick = .9f;
    struct Out {
        bool dodge{};
        float right{}; // the stick's side while a head dodge goes out (0: the player's own)
        bool started{};
    };
    Out update(float dt, bool on, bool b, int sidestep) {
        Out o;
        dt = std::isfinite(dt) ? std::clamp(dt, 0.f, .1f) : 0.f;
        if (!on) {
            left_ = 0, side_ = 0, bWas_ = b;
            return o;
        }
        if (b && !bWas_) {
            left_ = least, side_ = 0;
            o.started = true;
            ++presses;
        } else if (sidestep && left_ <= 0) {
            left_ = least, side_ = sidestep;
            o.started = true;
            ++headDodges;
        }
        bWas_ = b;
        o.dodge = b || left_ > 0;
        o.right = side_ ? stick * static_cast<float>(side_) : 0.f;
        left_ = std::max(0.f, left_ - dt);
        if (left_ <= 0 && !b)
            side_ = 0;
        return o;
    }
    uint32_t presses{}, headDodges{};

  private:
    float left_{};
    int side_{};
    bool bWas_ = true; // a B already down when play starts is no press
};

// HERO VR 0.0.77 STORY INPUTS (the parent's list, 2:07 PM): the game's presses as they go out, counted. Each
// is a rising edge of the pad's own bits (a chord counts when its last button joins); RT HOLD is the right
// trigger over 200 without the left; LT + RT both over 200. Per second for X, RB and LB + RB: the most in any
// 1 s (a mash's or a repeat's rate).
enum Input : uint32_t {
    inB, inLbRb, inY, inRb, inX, inLbX, inYB, inDpadDown, inLtRt, inA, inRtHold, inLbB, inLbY, inputs
};
inline constexpr const char* inputNames[inputs] = {"B (dodge)",       "LB+RB (throw)", "Y (web strike / zip / save)",
                                                   "RB (webball)",    "X (strike)",    "LB+X (barrage / venom punch)",
                                                   "Y+B (finisher)",  "D-PAD DOWN (heal)", "LT+RT (point launch)",
                                                   "A (jump)",        "RT HOLD (sprint)", "LB+B (ability)",
                                                   "LB+Y (ability)"};
inline const char* inputName(uint32_t i) { return i < inputs ? inputNames[i] : "?"; }

class InputWatch {
  public:
    // Returns the inputs that started this frame (bit i = Input i).
    uint32_t update(float seconds, uint16_t buttons, uint8_t lt, uint8_t rt, bool air) {
        now_ += std::isfinite(seconds) ? std::clamp(seconds, 0.f, .1f) : 0.f;
        const auto has = [&](uint16_t m) { return (buttons & m) == m; };
        bool on[inputs]{};
        on[inB] = has(pad::b);
        on[inLbRb] = has(pad::lb | pad::rb);
        on[inY] = has(pad::y);
        on[inRb] = has(pad::rb);
        on[inX] = has(pad::x);
        on[inLbX] = has(pad::lb | pad::x);
        on[inYB] = has(pad::y | pad::b);
        on[inDpadDown] = has(pad::dpadDown);
        on[inLtRt] = lt > 200 && rt > 200;
        on[inA] = has(pad::a);
        on[inRtHold] = rt > 200 && lt < 100;
        on[inLbB] = has(pad::lb | pad::b);
        on[inLbY] = has(pad::lb | pad::y);
        uint32_t started{};
        for (uint32_t i = 0; i < inputs; ++i) {
            if (on[i] && !was_[i]) {
                started |= 1u << i;
                ++presses[i];
                inAir[i] += air;
                const int r = rateSlot(i);
                if (r >= 0) {
                    times_[r][head_[r]] = now_;
                    head_[r] = (head_[r] + 1) % window;
                    uint32_t n{};
                    for (float t : times_[r])
                        n += t > -1e5f && now_ - t <= 1.f;
                    best1s[r] = std::max(best1s[r], n);
                }
            }
            was_[i] = on[i];
        }
        return started;
    }
    float now() const { return now_; }
    uint32_t presses[inputs]{}, inAir[inputs]{};
    uint32_t best1s[3]{}; // X, RB, LB + RB

  private:
    static constexpr unsigned window = 32;
    static int rateSlot(uint32_t i) { return i == inX ? 0 : i == inRb ? 1 : i == inLbRb ? 2 : -1; }
    float now_{};
    bool was_[inputs]{};
    float times_[3][window]{};
    unsigned head_[3]{};

  public:
    InputWatch() {
        for (auto& row : times_)
            for (float& t : row)
                t = -1e6f;
    }
};

// StorySprint (story_sprint, CANDIDATE): the game runs (its parkour sprint, "Hold R2" in the run-for-your-life
// and up-the-roof sections) while RT is held; VR sent RT on the ground only crawling. With the left stick
// pushed past `push` on the ground (not swung, not on a wall, no ability being sent) for `after` seconds, RT
// goes out held until the stick drops under `release`.
class StorySprint {
  public:
    static constexpr float push = .85f, release = .6f, after = .15f;
    bool update(float seconds, bool on, bool grounded, float stickX, float stickY) {
        const float dt = std::isfinite(seconds) ? std::clamp(seconds, 0.f, .1f) : 0.f;
        const float x = std::isfinite(stickX) ? stickX : 0.f, y = std::isfinite(stickY) ? stickY : 0.f;
        const float m = std::sqrt(x * x + y * y);
        if (!on || !grounded) {
            pushed_ = 0;
            running_ = false;
            return false;
        }
        if (running_) {
            running_ = m >= release;
        } else {
            pushed_ = m >= push ? pushed_ + dt : 0;
            if (pushed_ >= after) {
                running_ = true;
                ++starts;
            }
        }
        frames += running_;
        return running_;
    }
    uint32_t starts{}, frames{};

  private:
    float pushed_{};
    bool running_{};
};

// Spider-sense's NOW: a timed warning closer than `now` seconds to its blow (game seconds, CANDIDATE).
constexpr float senseNow = .3f;

#pragma pack(push, 4)
struct Data {
    uint32_t magic = 0x42525453, version = 1, bytes = 128; // "STRB"
    uint32_t abilitiesOn{}, aimMode{}, dodgeOn{}, headMode{};
    uint32_t fired[slots]{};
    uint32_t taps{}, givenBack{}, ownedFrames{}, lastSlot{};
    uint32_t throws{}, aimFrames{}, aimOverridden{};
    float lastAimDegrees{};
    uint32_t dodges{}, dodgesInAir{}, dodgesOnWall{}, headDodges{}, senseNowCues{}, senseDodges{};
    uint32_t reserved[3]{};
    uint64_t sequence{};
};
static_assert(sizeof(Data) == 128);

// "STRC" (SpidyStoryInputsData): InputWatch's counts and the sprint's.
struct InputsData {
    uint32_t magic = 0x43525453, version = 1, bytes = 192; // "STRC"
    uint32_t sprintOn{};
    uint32_t presses[inputs]{}, inAir[inputs]{};
    uint32_t best1s[3]{};
    uint32_t mashSplits{}, sprintStarts{}, sprintFrames{};
    uint32_t reserved[10]{};
    uint64_t sequence{};
};
static_assert(sizeof(InputsData) == 192);
#pragma pack(pop)

} // namespace spidy::story077b
