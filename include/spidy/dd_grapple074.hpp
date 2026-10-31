#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
//
// 0.0.74 test 10 DD QUICK GRAPPLE (a tester on DAREDEVIL, owner pt1440u: "the grapple feels a little slow though
// and it would be cool if he had a midair grapple cancel like the arkham games to redirect especially with the
// dual weild"). One live.txt switch (dd_quick_grapple, export SpidyDdQuickGrapple 5552; the wrist HUD is full),
// ON by default, DAREDEVIL only:
//  - QUICK: his pull is faster (quickTune: 30 m/s, was 24) and reaches it sooner, its acceleration eased in by a
//    smoothstep (cape_glide::GrappleTune::rampSeconds) so the start never jerks.
//  - REDIRECT (Arkham's grapple cancel): while a pull (or its vault) is going, a new trigger press from EITHER
//    baton (dual wield) fires at once along that hand's aim - the old line lets go and the new pull eases from the
//    old pull's velocity onto its own line (Grapple::carry, blendSeconds): no stop. A press that hooks nothing
//    is the CANCEL: the line lets go and he COASTS on (Coast: the kept velocity with gravity for coastSeconds,
//    then the game's own fall), never a drop to a standstill.
// OFF: test 9's grapple exactly (24 m/s, presses while pulling do nothing, the default tune).
// Game hooks: none new - the bridge's world raycast and velocity command are the webs' and the grapple's own
// (CONFIRMED: the webs and the grapple use them). That the game's fall keeps the velocity the coast hands it is
// CANDIDATE (not yet seen in a run). No game address.
//
// The bits ride in the swing command's glide word (game_swing::Command::reserved7[0]; bit 0 is GLIDE TILT): an
// older bridge ignores them, an older VR module sends 0 (OFF).
#include "spidy/cape_glide.hpp"
#include "spidy/math.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace spidy::dd_grapple074 {

inline constexpr uint32_t quickBit = 1u << 1, redirectBit = 1u << 2;
// live.txt dd_quick_grapple: 0 ON (the default), 1 OFF; anything else reads as ON (the fix stays).
inline bool onByDefault(uint32_t value) { return value != 1; }
inline uint32_t word(bool daredevil, bool on) { return daredevil && on ? quickBit | redirectBit : 0u; }
inline bool quick(uint32_t glideWord) { return (glideWord & quickBit) != 0; }
inline bool redirect(uint32_t glideWord) { return (glideWord & redirectBit) != 0; }

// The QUICK pull: under game_swing's Config::maxSpeed (32) and the bridge's cap either way.
inline cape_glide::GrappleTune quickTune() {
    cape_glide::GrappleTune t;
    t.speed = 30;
    t.accel = 100;
    t.rampSeconds = .1f;
    t.blendSeconds = .12f;
    return t;
}
inline cape_glide::GrappleTune tuneFor(bool quickOn) { return quickOn ? quickTune() : cape_glide::GrappleTune{}; }

// A new press may fire over a going grapple (REDIRECT on and a pull or its vault going).
inline bool mayRedirect(bool redirectOn, uint32_t phase) {
    return redirectOn && (phase == cape_glide::pull || phase == cape_glide::vault);
}

// The CANCEL's coast: the kept velocity, gravity pulling it down, for coastSeconds; ends early on the ground, when
// the body stops following it (a wall: blocked), when not allowed, or when a new grapple starts (stop()).
struct CoastOut {
    bool drive{}, ended{};
    Vec3 velocity{};
};
class Coast {
  public:
    static constexpr float coastSeconds = .45f, gravity = 9.81f, blockedShare = .35f, blockedGrace = .1f, slowest = 1.f;
    bool start(Vec3 velocity) {
        on_ = finite(velocity) && length(velocity) >= slowest;
        v_ = on_ ? velocity : Vec3{};
        t_ = 0;
        starts_ += on_ ? 1u : 0u;
        return on_;
    }
    CoastOut update(float seconds, bool grounded, Vec3 bodyVelocity, bool allowed) {
        CoastOut o;
        if (!on_)
            return o;
        const float dt = std::isfinite(seconds) ? std::clamp(seconds, 0.f, .1f) : 0.f;
        t_ += dt;
        const float asked = length(v_);
        const bool blocked = t_ > blockedGrace && finite(bodyVelocity) && length(bodyVelocity) < blockedShare * asked;
        if (!allowed || grounded || blocked || t_ >= coastSeconds || !finite(v_)) {
            stop();
            o.ended = true;
            return o;
        }
        v_.y -= gravity * dt;
        o.drive = true;
        o.velocity = v_;
        return o;
    }
    void stop() {
        on_ = false;
        v_ = {};
    }
    bool on() const { return on_; }
    uint32_t starts() const { return starts_; }

  private:
    bool on_{};
    Vec3 v_{};
    float t_{};
    uint32_t starts_{};
};

// One sentence (the live.txt line's subtext).
inline constexpr const char* help =
    "DAREDEVIL's grapple pulls faster and a press while pulled cancels it, keeping your speed, and fires along "
    "the new aim from either baton, live with no restart.";

} // namespace spidy::dd_grapple074
