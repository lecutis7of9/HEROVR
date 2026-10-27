#pragma once
// Update 38.5 GESTURES (the gesture design approved by the user): the body moves that
// send the game's own ability, parry, finisher, camouflage and Web Wings inputs, and TRAINING, which
// shows where each move goes and says SUCCESS or FAIL using this same recogniser.
//
// Every input here is PUBLISHED or CANDIDATE (the game's control list, not read from the game): each
// gesture's input is live-editable (live.txt <gesture>_input, the COMBOS below) and the holo's SLOT
// TEST sends each in turn so one session can confirm them. Nothing here reads or writes the game.
//
// Pure and allocation-free: fixed arrays only, no std::vector, no strings built per frame (the
// reasons are string literals). All positions are tracking space (metres, y up); everything the
// moves measure is relative to the head and its level forward, so the city moving under a swing
// never looks like a move.
#include "math.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace spidy::gestures385 {

// The moves (TRAINING's list, the records' counts and live.txt's <name>_input keys, in this order).
enum Gesture : uint8_t {
    abilityPunch, // X held + one hand punches forward            (LB+X, CANDIDATE)
    abilityLunge, // X held + the head lunges forward 20 cm        (LB+Y)
    abilitySlam,  // X held + both fists slam down from the chest  (LB+B)
    abilityScoop, // X held + both hands scoop waist to overhead   (LB+A)
    ultimate,     // X held + arms crossed 0.4 s then burst out, on the ground (L3+R3, straight to the pad)
    camo,         // an open hand wipes down the face (Miles)      (D-pad Up)
    parry,        // a forearm snapped up across the face, open hand (LB tap under 0.15 s)
    finisher,     // both fists together overhead, hammered down    (Y+B)
    wingsOpen,    // in the air, arms spread wide at the shoulders 0.25 s (Y tap)
    wingsBoost,   // wings open, both arms flap down together       (A tap)
    wingsClose,   // wings open, both arms down below the waist 0.3 s (Y tap)
    wingsDive,    // wings open, a tucked dive then spread again     (Y tap)
    skimJump,     // just landed from a glide (water skim), a flap  (A tap)
    ricochet,     // the trigger and a sideways backhand sweep (combat_input; trained here)
    upshot,       // the trigger and an upward flick (combat_input)
    grabber,      // the trigger and a yank back to the chest (combat_input)
    sonic,        // palms together in front, both triggers (combat_input)
    count
};
inline const char* gestureName(uint32_t g) {
    static constexpr const char* names[count] = {
        "ABILITY PUNCH", "ABILITY LUNGE", "ABILITY SLAM", "ABILITY SCOOP", "ULTIMATE", "CAMOUFLAGE", "PARRY",
        "FINISHER", "WINGS OPEN", "WINGS BOOST", "WINGS CLOSE", "WINGS DIVE", "SKIM JUMP", "RICOCHET", "UPSHOT",
        "GRABBER", "SONIC"};
    return g < count ? names[g] : "";
}
// live.txt's key per gesture (tools/live_settings.py GESTURE_INPUTS, in this order).
inline const char* gestureKey(uint32_t g) {
    static constexpr const char* keys[count] = {
        "punch_input", "lunge_input", "slam_input", "scoop_input", "ultimate_input", "camo_input", "parry_input",
        "finisher_input", "wings_open_input", "wings_boost_input", "wings_close_input", "wings_dive_input",
        "skim_jump_input", "ricochet_train", "upshot_train", "grabber_train", "sonic_train"};
    return g < count ? keys[g] : "";
}
// The ones this module sends (the gadgets are combat_input's, only trained here).
constexpr uint32_t sentGestures = ricochet;
inline bool isGadget(uint32_t g) {
    return g >= ricochet && g < count;
}

// The XInput bits (game_pad::Button, checked there with static_asserts in game_xr.cpp).
namespace pad {
constexpr uint16_t dpadUp = 0x1, dpadDown = 0x2, dpadLeft = 0x4, dpadRight = 0x8, l3 = 0x40, r3 = 0x80, lb = 0x100,
                   rb = 0x200, a = 0x1000, b = 0x2000, x = 0x4000, y = 0x8000;
}
// The inputs a gesture can send: a lead button held first (50 ms), then the face; or a short tap.
struct Combo {
    const char* name;
    uint16_t lead, face;
    float seconds; // the whole press
};
constexpr int combos = 20;
inline const Combo& combo(uint32_t i) {
    static constexpr Combo table[combos] = {
        {"OFF", 0, 0, 0},
        {"LB+X", pad::lb, pad::x, .2f},
        {"LB+Y", pad::lb, pad::y, .2f},
        {"LB+B", pad::lb, pad::b, .2f},
        {"LB+A", pad::lb, pad::a, .2f},
        {"L3+R3", 0, pad::l3 | pad::r3, .2f},
        {"DPAD UP", 0, pad::dpadUp, .12f},
        {"LB TAP", 0, pad::lb, .12f}, // under 0.15 s: the game's parry, never its block or ability hold
        {"Y+B", 0, pad::y | pad::b, .2f},
        {"Y TAP", 0, pad::y, .12f},
        {"A TAP", 0, pad::a, .12f},
        {"X TAP", 0, pad::x, .12f},
        {"B TAP", 0, pad::b, .12f},
        {"RB+A", pad::rb, pad::a, .2f},
        {"RB+B", pad::rb, pad::b, .2f},
        {"RB+X", pad::rb, pad::x, .2f},
        {"RB+Y", pad::rb, pad::y, .2f},
        {"DPAD DOWN", 0, pad::dpadDown, .12f},
        {"DPAD LEFT", 0, pad::dpadLeft, .12f},
        {"DPAD RIGHT", 0, pad::dpadRight, .12f},
    };
    return table[i < combos ? i : 0];
}
// Each gesture's default input (the design's PUBLISHED mapping); the gadgets' are combat_input's.
inline uint8_t defaultInput(uint32_t g) {
    static constexpr uint8_t inputs[count] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 9, 9, 10, 0, 0, 0, 0};
    return g < count ? inputs[g] : 0;
}

struct Hand {
    bool tracked{};
    Vec3 position{};  // tracking space
    float trigger{}, grip{};
};
struct Input {
    float seconds{};       // since the previous frame
    bool play{};           // VR play, nothing else holding the controls (no menu, phone, calibration)
    Vec3 head{}, forward{}; // the headset and its look, tracking space
    Hand hands[2]{};       // 0 left, 1 right
    bool xHeld{};          // the left controller's X held (the ability modifier; a tap stays the menu)
    bool airborne{};       // the game's or the swing's airborne
    bool webAttached{};    // a swing web attached
    bool holoBusy{};       // the hologram open or a hand in its zone (it swallows the trigger)
    float stick{};         // the physical left stick's deflection, 0..1
    bool enemyNear = true; // an enemy within 6 m (unknown: true)
};
struct Settings {
    bool ability = true, parry = true, finisher = true, wingArms = true;
    float steer = 1;     // WINGS STEER: 0.5, 1, 1.5, 2
    bool invert{};       // WINGS PITCH INVERT
    uint8_t input[count]{};
    Settings() {
        for (uint32_t g = 0; g < count; ++g)
            input[g] = defaultInput(g);
    }
};

// The design's numbers (gesture-design-385.md), one place.
struct Tune {
    static constexpr float shoulder = -.25f, chest = -.40f, waist = -.72f; // below the eyes, metres
    static constexpr float punchSpeed = 2.0f, punchReach = .25f, otherStill = 1.2f;
    static constexpr float lungeMetres = .20f, lungeWindow = .5f;
    static constexpr float slamSpeed = 1.8f, scoopSpeed = 1.5f, together = .15f;
    static constexpr float crossHold = .4f, burstWindow = .6f, burstSpeed = 1.2f, burstSpread = .8f;
    static constexpr float camoWindow = .4f, camoReach = .15f;
    static constexpr float parrySpeed = 2.2f, parryRise = 1.0f;
    static constexpr float finisherHold = .15f, finisherGap = .3f, finisherSpeed = 2.0f, finisherWindow = .7f;
    static constexpr float wingSpan = 1.0f, wingHold = .25f, closeHold = .3f, flapSpeed = 1.8f;
    static constexpr float rollDead = 8, rollFull = 35, pitchDead = 10, pitchFull = 40, smoothing = .12f,
                           rate = 4.f, stickOverride = .3f;
    static constexpr float abilityExpiry = 1.2f;
};

// What a frame's moves did. Allocation-free: at most `events` results a frame.
struct Event {
    uint8_t gesture = count;
    bool success{};
    const char* reason = ""; // FAIL's short reason (a literal)
};
struct Out {
    uint16_t buttons{};       // for the virtual pad (0 in PRACTICE)
    bool steering{};          // the wings steer the left stick this frame
    float stickX{}, stickY{}; // -1..1 (Y up = stick forward = dive)
    static constexpr int events = 4;
    Event event[events]{};
    int eventCount{};
};

// Each gesture's stages for TRAINING's guide: start pose, path, end; set by the recogniser itself.
enum Stage : uint8_t { stageStart = 1, stagePath = 2, stageEnd = 4 };

namespace detail {
inline Vec3 level(Vec3 v) {
    v.y = 0;
    return length(v) > 1e-4f ? normalized(v) : Vec3{0, 0, -1};
}
inline float shaped(float degrees, float dead, float full) {
    const float a = std::abs(degrees);
    const float s = std::clamp((a - dead) / (full - dead), 0.f, 1.f);
    return degrees < 0 ? -s : s;
}
} // namespace detail

// The recogniser: one per player. update() once a frame; everything is in fixed members.
class Recognizer {
  public:
    static constexpr int history = 64; // ~0.7 s at 90 Hz
    struct Body {
        Vec3 right{1, 0, 0}, forward{0, 0, -1};
        Vec3 local(Vec3 p, Vec3 head) const {
            const Vec3 d = p - head;
            return {dot(d, right), d.y, dot(d, forward)};
        }
    };

    Out update(const Input& in, const Settings& s) {
        Out out{};
        out_ = &out;
        const float dt = std::isfinite(in.seconds) ? std::clamp(in.seconds, 0.f, .1f) : 0.f;
        now_ += dt;
        stages_.fill(0);
        body_.forward = detail::level(in.forward);
        body_.right = normalized(cross(body_.forward, Vec3{0, 1, 0}));
        const bool ok = in.play && finite(in.head) && in.hands[0].tracked && in.hands[1].tracked &&
                        finite(in.hands[0].position) && finite(in.hands[1].position) && dt > 0;
        if (!ok) {
            reset(false);
            return out;
        }
        // The samples, head-relative in the body's frame; velocities over ~3 frames.
        head_[ix(at_)] = in.head;
        for (int h = 0; h < 2; ++h)
            local_[h][at_] = body_.local(in.hands[h].position, in.head);
        times_[ix(at_)] = now_;
        filled_ = std::min(filled_ + 1, history);
        for (int h = 0; h < 2; ++h)
            vel_[h] = velocity(h);
        L_ = local_[0][at_];
        R_ = local_[1][at_];
        in_ = &in;
        set_ = &s;
        if (filled_ >= 4) {
            abilities(in, s);
            camouflage(in, s);
            parryMove(in, s);
            finisherMove(in, s);
            wings(in, s, out, dt);
        }
        at_ = (at_ + 1) % history;
        out_ = nullptr;
        return out;
    }
    void reset(bool all = true) {
        filled_ = 0;
        crossedFor_ = finisherFor_ = spreadFor_ = lowFor_ = diveFor_ = 0;
        crossedAt_ = finisherAt_ = -1e9f;
        camoTop_[0] = camoTop_[1] = -1e9f;
        stickX_ = stickY_ = 0;
        if (all) {
            wingsOpen_ = false;
            landedAt_ = -1e9f;
        }
    }
    // TRAINING: the stages each gesture met this frame (Stage bits).
    uint8_t stages(uint32_t g) const {
        return g < count ? stages_[g] : 0;
    }
    bool wingsAreOpen() const {
        return wingsOpen_;
    }
    uint64_t recognised(uint32_t g) const {
        return g < count ? recognised_[g] : 0;
    }
    uint64_t blocked(uint32_t g) const {
        return g < count ? blocked_[g] : 0;
    }
    float lastSpeed() const {
        return std::max(length(vel_[0]), length(vel_[1]));
    }
    const Body& body() const {
        return body_;
    }
    Vec3 handLocal(int h) const {
        return h ? R_ : L_;
    }
    Vec3 handVelocity(int h) const {
        return vel_[h & 1];
    }

  private:
    // A recognised move: its input (unless set to OFF) and an event; `why` non-empty fails it.
    void fire(uint32_t g, const char* why = "") {
        if (!out_ || out_->eventCount >= Out::events)
            return;
        const bool success = !why[0];
        if (!success && now_ - lastFail_[g] < failQuiet)
            return; // one FAIL a move per 0.6 s: TRAINING says it once, not every frame
        if (!success)
            lastFail_[g] = now_;
        out_->event[out_->eventCount++] = {static_cast<uint8_t>(g), success, why};
        stages_[g] |= stageStart | stagePath | (success ? stageEnd : 0);
        (success ? recognised_[g] : blocked_[g]) += 1;
        if (success)
            lastFire_[g] = now_;
    }
    bool cooled(uint32_t g, float seconds) const {
        return now_ - lastFire_[g] >= seconds;
    }
    static std::size_t ix(int i) { return static_cast<std::size_t>(i); }
    int back(int frames) const {
        return (at_ - frames + history * 2) % history;
    }
    Vec3 velocity(int h) const {
        const int n = std::min(3, filled_ - 1);
        if (n < 1)
            return {};
        const int b = back(n);
        const float dt = times_[ix(at_)] - times_[ix(b)];
        return dt > 1e-4f ? (local_[h][at_] - local_[h][b]) * (1.f / dt) : Vec3{};
    }
    // The lowest / highest a hand was within `seconds`, and the head's farthest forward travel.
    float minY(int h, float seconds) const {
        float m = 1e9f;
        for (int i = 0; i < filled_; ++i) {
            const int k = back(i);
            if (now_ - times_[ix(k)] > seconds)
                break;
            m = std::min(m, local_[h][k].y);
        }
        return m;
    }
    float maxY(int h, float seconds) const {
        float m = -1e9f;
        for (int i = 0; i < filled_; ++i) {
            const int k = back(i);
            if (now_ - times_[ix(k)] > seconds)
                break;
            m = std::max(m, local_[h][k].y);
        }
        return m;
    }
    float maxSpan(float seconds) const {
        float m = 0;
        for (int i = 0; i < filled_; ++i) {
            const int k = back(i);
            if (now_ - times_[ix(k)] > seconds)
                break;
            m = std::max(m, length(local_[0][k] - local_[1][k]));
        }
        return m;
    }
    float headTravel(float seconds) const {
        float m = 0;
        for (int i = 0; i < filled_; ++i) {
            const int k = back(i);
            if (now_ - times_[ix(k)] > seconds)
                break;
            m = std::max(m, dot(head_[ix(at_)] - head_[ix(k)], body_.forward));
        }
        return m;
    }
    bool open(const Hand& h) const {
        return h.trigger < .2f && h.grip < .5f;
    }

    void abilities(const Input& in, const Settings& s) {
        const bool x = in.xHeld;
        if (!x) {
            abilitySpent_ = false;
            xFor_ = 0;
        } else {
            xFor_ += in.seconds;
        }
        const bool armed = s.ability && x && !abilitySpent_ && xFor_ <= Tune::abilityExpiry + 2.f;
        const bool fists = in.hands[0].trigger < .5f && in.hands[1].trigger < .5f;
        // ULTIMATE: arms crossed on the chest (the left hand right of the middle, the right left)
        // for 0.4 s, then both burst outward; on the ground only.
        const bool crossed = L_.x >= .03f && R_.x <= -.03f && L_.y > -.70f && L_.y < -.08f && R_.y > -.70f &&
                             R_.y < -.08f && L_.z > -.05f && R_.z > -.05f && L_.z < .5f && R_.z < .5f &&
                             length(L_ - R_) < .4f;
        crossedFor_ = crossed ? crossedFor_ + in.seconds : 0;
        if (crossed)
            stages_[ultimate] |= stageStart;
        if (crossedFor_ >= Tune::crossHold) {
            crossedAt_ = now_;
            stages_[ultimate] |= stagePath;
        }
        if (now_ - crossedAt_ <= Tune::burstWindow && !crossed) {
            stages_[ultimate] |= stageStart | stagePath;
            const bool burst = vel_[0].x <= -Tune::burstSpeed && vel_[1].x >= Tune::burstSpeed;
            if (burst && length(L_ - R_) >= Tune::burstSpread && cooled(ultimate, 1.f)) {
                crossedAt_ = -1e9f;
                if (!s.ability)
                    return;
                fire(ultimate, !x ? "HOLD X" : in.airborne ? "ON THE GROUND ONLY" : abilitySpent_ ? "LET GO OF X FIRST" : "");
                abilitySpent_ = abilitySpent_ || (x && !in.airborne);
                return;
            }
            if (now_ - crossedAt_ > Tune::burstWindow - in.seconds * 1.5f)
                fire(ultimate, length(L_ - R_) < Tune::burstSpread ? "BURST WIDER" : "TOO SLOW");
            return; // the cross belongs to the ultimate: no slam or punch from it
        }
        if (crossedFor_ > 0)
            return;
        // SLAM: both fists down fast together from the chest.
        for (int h = 0; h < 2; ++h)
            if (vel_[h].y <= -Tune::slamSpeed)
                slamAt_[h] = now_;
        const bool slamPair = std::abs(slamAt_[0] - slamAt_[1]) <= Tune::together && now_ - slamAt_[0] < .05f &&
                              now_ - slamAt_[1] < .05f;
        if (maxY(0, .4f) >= Tune::chest - .1f && maxY(1, .4f) >= Tune::chest - .1f && length(L_ - R_) < .6f)
            stages_[abilitySlam] |= stageStart;
        if (slamPair && maxY(0, .4f) >= Tune::chest - .1f && maxY(1, .4f) >= Tune::chest - .1f &&
            L_.y < Tune::chest - .1f && R_.y < Tune::chest - .1f && length(L_ - R_) < .6f && cooled(abilitySlam, .6f)) {
            if (armed && fists)
                fire(abilitySlam), abilitySpent_ = true;
            else if (s.ability && !x && trainingFails_)
                fire(abilitySlam, "HOLD X");
            return;
        }
        // SCOOP: both hands from the waist up past the head.
        for (int h = 0; h < 2; ++h)
            if (vel_[h].y >= Tune::scoopSpeed)
                scoopAt_[h] = now_;
        if (minY(0, .7f) <= Tune::waist + .12f && minY(1, .7f) <= Tune::waist + .12f)
            stages_[abilityScoop] |= stageStart;
        if (std::abs(scoopAt_[0] - scoopAt_[1]) <= Tune::together * 2 && now_ - scoopAt_[0] < .15f &&
            now_ - scoopAt_[1] < .15f && minY(0, .7f) <= Tune::waist + .12f && minY(1, .7f) <= Tune::waist + .12f &&
            L_.y >= 0 && R_.y >= 0 && cooled(abilityScoop, .6f)) {
            if (armed && fists)
                fire(abilityScoop), abilitySpent_ = true;
            else if (s.ability && !x && trainingFails_)
                fire(abilityScoop, "HOLD X");
            return;
        }
        // PUNCH (X held): one hand thrown forward, the other still.
        for (int h = 0; h < 2; ++h) {
            const Vec3 v = vel_[h], o = vel_[1 - h];
            const Vec3 p = h ? R_ : L_;
            if (v.z >= Tune::punchSpeed && v.z >= .75f * length(v) && p.z >= Tune::punchReach &&
                length(o) < Tune::otherStill && cooled(abilityPunch, .5f)) {
                stages_[abilityPunch] |= stageStart;
                if (armed)
                    fire(abilityPunch), abilitySpent_ = true;
                else if (s.ability && !x && trainingFails_)
                    fire(abilityPunch, "HOLD X");
                return;
            }
        }
        // LUNGE: the head (the body) moves 20 cm forward within 0.5 s, X held.
        if (x && headTravel(Tune::lungeWindow) >= Tune::lungeMetres && cooled(abilityLunge, .8f)) {
            stages_[abilityLunge] |= stageStart;
            if (armed)
                fire(abilityLunge), abilitySpent_ = true;
        } else if (x && headTravel(Tune::lungeWindow) >= Tune::lungeMetres * .5f) {
            stages_[abilityLunge] |= stageStart;
        }
    }
    void camouflage(const Input& in, const Settings& s) {
        if (!s.ability || in.xHeld || in.holoBusy)
            return;
        for (int h = 0; h < 2; ++h) {
            const Vec3 p = h ? R_ : L_;
            const float off = std::sqrt(p.x * p.x + (p.z - .08f) * (p.z - .08f));
            const bool nearFace = off <= Tune::camoReach && p.z > -.02f;
            if (!open(in.hands[h])) {
                camoTop_[h] = -1e9f;
                continue;
            }
            if (nearFace && p.y >= .02f && p.y <= .2f) {
                camoTop_[h] = now_; // on the forehead: the wipe starts from here
                stages_[camo] |= stageStart;
                continue;
            }
            const float since = now_ - camoTop_[h];
            if (since > Tune::camoWindow + .3f)
                continue;
            if (!nearFace) {
                if (trainingFails_ && p.y < .02f && p.y > -.2f)
                    fire(camo, "KEEP YOUR HAND ON YOUR FACE");
                camoTop_[h] = -1e9f;
                continue;
            }
            stages_[camo] |= stageStart | stagePath;
            if (p.y <= -.12f) {
                camoTop_[h] = -1e9f;
                if (cooled(camo, 1.5f))
                    fire(camo, since > Tune::camoWindow ? "TOO SLOW" : "");
            }
        }
    }

    void parryMove(const Input& in, const Settings& s) {
        if (!s.parry || in.holoBusy)
            return;
        for (int h = 0; h < 2; ++h) {
            const Vec3 v = vel_[h], p = h ? R_ : L_;
            const float speed = length(v);
            const bool before = p.y > -.32f && p.y < .15f && p.z > .06f && p.z < .5f && std::abs(p.x) < .35f;
            if (minY(h, .3f) <= -.3f && before)
                stages_[parry] |= stageStart;
            if (speed >= Tune::parrySpeed && v.y >= Tune::parryRise && std::abs(v.z) <= .6f * speed && before &&
                minY(h, .3f) <= -.3f && cooled(parry, .5f)) {
                if (in.xHeld)
                    fire(parry, trainingFails_ ? "LET GO OF X" : "");
                else if (!open(in.hands[h]))
                    fire(parry, "OPEN YOUR HAND");
                else if (!in.enemyNear)
                    fire(parry, "NO ENEMY NEAR");
                else
                    fire(parry);
                return;
            }
        }
    }
    void finisherMove(const Input& in, const Settings& s) {
        if (!s.finisher || wingsOpen_)
            return;
        const bool up = L_.y >= .12f && R_.y >= .12f && length(L_ - R_) <= Tune::finisherGap;
        finisherFor_ = up ? finisherFor_ + in.seconds : 0;
        if (up)
            stages_[finisher] |= stageStart;
        if (finisherFor_ >= Tune::finisherHold)
            finisherAt_ = now_;
        if (now_ - finisherAt_ > Tune::finisherWindow || up)
            return;
        stages_[finisher] |= stageStart | stagePath;
        for (int h = 0; h < 2; ++h)
            if (vel_[h].y <= -Tune::finisherSpeed)
                hammerAt_[h] = now_;
        if (L_.y <= -.25f && R_.y <= -.25f) {
            finisherAt_ = -1e9f;
            const bool fast = now_ - hammerAt_[0] < .4f && now_ - hammerAt_[1] < .4f &&
                              std::abs(hammerAt_[0] - hammerAt_[1]) <= Tune::together * 2;
            if (cooled(finisher, 1.f))
                fire(finisher, !fast ? "TOO SLOW" : in.xHeld ? "LET GO OF X" : length(L_ - R_) > .5f ? "KEEP YOUR FISTS TOGETHER" : "");
        } else if (now_ - finisherAt_ > Tune::finisherWindow - in.seconds * 1.5f) {
            fire(finisher, "TOO SLOW");
        }
    }
    void wings(const Input& in, const Settings& s, Out& out, float dt) {
        if (!s.wingArms) {
            wingsOpen_ = false;
            return;
        }
        const float span = length(L_ - R_);
        const bool clear = in.hands[0].grip < .5f && in.hands[1].grip < .5f && in.hands[0].trigger < .5f &&
                           in.hands[1].trigger < .5f && !in.holoBusy && !in.xHeld;
        const bool shoulders = L_.y > -.62f && L_.y < .12f && R_.y > -.62f && R_.y < .12f;
        if (wingsOpen_ && (!in.airborne || in.webAttached)) {
            wingsOpen_ = false; // landed (or skimming) or a swing web: the game ends its glide itself
            if (!in.airborne && !in.webAttached)
                landedAt_ = now_;
        }
        // SKIM JUMP: just down from a glide, a flap down from wide.
        for (int h = 0; h < 2; ++h)
            if (vel_[h].y <= -Tune::flapSpeed)
                flapAt_[h] = now_;
        const bool flap = std::abs(flapAt_[0] - flapAt_[1]) <= Tune::together && now_ - flapAt_[0] < .05f &&
                          now_ - flapAt_[1] < .05f && maxSpan(.35f) >= Tune::wingSpan * .9f;
        if (!wingsOpen_ && !in.airborne && now_ - landedAt_ < 4.f) {
            stages_[skimJump] |= stageStart;
            if (flap && clear && cooled(skimJump, .8f))
                fire(skimJump);
        }
        if (!wingsOpen_) {
            const bool spread = span >= Tune::wingSpan && shoulders;
            spreadFor_ = spread ? spreadFor_ + dt : 0;
            if (spread)
                stages_[wingsOpen] |= stageStart;
            if (spreadFor_ >= Tune::wingHold && cooled(wingsOpen, .6f)) {
                spreadFor_ = 0;
                if (!in.airborne)
                    fire(wingsOpen, trainingFails_ ? "IN THE AIR ONLY" : "");
                else if (in.webAttached)
                    fire(wingsOpen, trainingFails_ ? "LET GO OF THE WEB" : "");
                else if (!clear)
                    fire(wingsOpen, trainingFails_ ? "LET GO OF THE TRIGGERS" : "");
                else {
                    fire(wingsOpen);
                    wingsOpen_ = true;
                    openedAt_ = now_;
                    lowFor_ = diveFor_ = 0;
                }
            }
            if (!out.steering)
                stickX_ = stickY_ = 0;
            return;
        }
        // Open: BOOST (a flap), DIVE (tucked, then spread again), CLOSE (both arms down 0.3 s).
        if (flap && cooled(wingsBoost, .8f)) {
            fire(wingsBoost);
            boostAt_ = now_;
        }
        const bool low = L_.y <= Tune::waist && R_.y <= Tune::waist;
        lowFor_ = low && now_ - boostAt_ > .6f ? lowFor_ + dt : 0;
        if (low)
            stages_[wingsClose] |= stageStart;
        if (lowFor_ >= Tune::closeHold && now_ - openedAt_ > .4f) {
            fire(wingsClose);
            wingsOpen_ = false;
            stickX_ = stickY_ = 0;
            return;
        }
        // The steering: roll from the hands' height difference, sweep from an arm behind the
        // shoulders, pitch from the arms' angle under the shoulders, tuck from a narrower span.
        const float half = std::max(span * .5f, .2f);
        const float horizontal = std::max(std::sqrt((L_.x - R_.x) * (L_.x - R_.x) + (L_.z - R_.z) * (L_.z - R_.z)), .2f);
        const float roll = std::atan2(L_.y - R_.y, horizontal) * 57.2957795f;
        float x = detail::shaped(roll, Tune::rollDead, Tune::rollFull);
        if (L_.z <= -.15f)
            x -= .5f;
        if (R_.z <= -.15f)
            x += .5f;
        const float avgY = (L_.y + R_.y) * .5f, avgZ = (L_.z + R_.z) * .5f;
        const float below = Tune::shoulder - avgY + .5f * std::max(0.f, avgZ - .1f);
        const float pitch = std::atan2(below, half) * 57.2957795f;
        float y = detail::shaped(pitch, Tune::pitchDead, Tune::pitchFull);
        if (span < Tune::wingSpan * .8f && avgY > Tune::waist + .1f)
            y += std::clamp((Tune::wingSpan * .8f - span) / .5f, 0.f, 1.f) * .8f; // tuck: dive
        if (s.invert)
            y = -y;
        const float steer = std::clamp(s.steer, .25f, 2.f);
        x = std::clamp(x * steer, -1.f, 1.f);
        y = std::clamp(y * steer, -1.f, 1.f);
        const float k = 1.f - std::exp(-dt / Tune::smoothing);
        const float step = Tune::rate * dt;
        stickX_ += std::clamp((x - stickX_) * k, -step, step);
        stickY_ += std::clamp((y - stickY_) * k, -step, step);
        stages_[wingsOpen] |= stageStart | stagePath | stageEnd;
        // DIVE BOOST: half a second tucked into a dive, then spread wide again.
        const bool diving = (s.invert ? -stickY_ : stickY_) >= .6f && span < Tune::wingSpan * .8f;
        diveFor_ = diving ? diveFor_ + dt : (span >= Tune::wingSpan ? diveFor_ : 0);
        if (diving)
            stages_[wingsDive] |= stageStart;
        if (!diving && diveFor_ >= .5f && span >= Tune::wingSpan && cooled(wingsDive, 1.f)) {
            diveFor_ = 0;
            fire(wingsDive);
        }
        if (in.stick <= Tune::stickOverride) {
            out.steering = true;
            out.stickX = stickX_;
            out.stickY = stickY_;
        }
    }

  public:
    // TRAINING asks for the FAIL reasons of moves with no game reason to fail (e.g. HOLD X); live
    // play leaves them off, which changes no success.
    bool trainingFails_{};

  private:
    Out* out_{};
    const Input* in_{};
    const Settings* set_{};
    Body body_{};
    float now_{};
    int at_{}, filled_{};
    std::array<Vec3, history> head_{};
    Vec3 local_[2][history]{};
    std::array<float, history> times_{};
    Vec3 vel_[2]{}, L_{}, R_{};
    std::array<uint8_t, count> stages_{};
    std::array<uint64_t, count> recognised_{}, blocked_{};
    static constexpr float failQuiet = .6f;
    std::array<float, count> lastFail_ = [] {
        std::array<float, count> a{};
        a.fill(-1e9f);
        return a;
    }();
    std::array<float, count> lastFire_ = [] {
        std::array<float, count> a{};
        a.fill(-1e9f);
        return a;
    }();
    bool abilitySpent_{}, wingsOpen_{};
    float xFor_{}, crossedFor_{}, crossedAt_ = -1e9f, finisherFor_{}, finisherAt_ = -1e9f, spreadFor_{}, lowFor_{},
                    diveFor_{}, openedAt_{}, boostAt_ = -1e9f, landedAt_ = -1e9f, stickX_{}, stickY_{};
    float slamAt_[2]{-1e9f, -1e9f}, scoopAt_[2]{-1e9f, -1e9f}, hammerAt_[2]{-1e9f, -1e9f}, flapAt_[2]{-1e9f, -1e9f};
    float camoTop_[2]{-1e9f, -1e9f};
};

// GESTURE MOVES (the EXTRAS row): OFF, TRAINING (recognised and named, nothing sent), ON (sent).
enum Mode : uint32_t { modeOff = 0, modeTraining = 1, modeOn = 2, modes = 3 };
inline const char* modeName(uint32_t m) {
    return m == modeTraining ? "TRAINING" : m == modeOn ? "ON" : "OFF";
}
inline constexpr const char* help = "TRAINING draws a path to follow; ON also sends abilities, parry and wings.";

// The pad presses a recognised move makes: its lead button alone for 50 ms, then lead and face
// together until the press's time is up. Up to four queued; one at a time; allocation-free.
class ComboSender {
  public:
    static constexpr float leadSeconds = .05f;
    static constexpr int queued = 4;
    // Queue a move's input (gesture's Settings::input); false when full or OFF.
    bool push(uint8_t input) {
        const Combo& c = combo(input);
        if (!c.face || count_ >= queued)
            return false;
        queue_[(head_ + count_) % queued] = input;
        ++count_;
        ++pushed_;
        return true;
    }
    // This frame's buttons.
    uint16_t update(float seconds) {
        const float dt = std::isfinite(seconds) ? std::clamp(seconds, 0.f, .1f) : 0.f;
        if (!count_)
            return 0;
        const Combo& c = combo(queue_[head_]);
        const float lead = c.lead ? leadSeconds : 0.f;
        const uint16_t buttons = static_cast<uint16_t>(t_ < lead ? c.lead : (c.lead | c.face));
        t_ += dt;
        if (t_ >= lead + c.seconds) {
            t_ = 0;
            head_ = (head_ + 1) % queued;
            --count_;
            ++sent_;
        }
        return buttons;
    }
    void clear() {
        count_ = head_ = 0;
        t_ = 0;
    }
    bool busy() const {
        return count_ > 0;
    }
    uint64_t pushed() const {
        return pushed_;
    }
    uint64_t sent() const {
        return sent_;
    }

  private:
    uint8_t queue_[queued]{};
    int head_{}, count_{};
    float t_{};
    uint64_t pushed_{}, sent_{};
};

} // namespace spidy::gestures385
