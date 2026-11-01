// HERO VR 0.0.76 SM1: CAPTAIN AMERICA's and DAREDEVIL's grapple in the add-on (pure logic, unit tested). Made by
// Agent Mulder. The pull, its tunes, the CANCEL coast and the JUMP OUT are HERO VR's own SM2 code, unchanged:
// cape_glide::Grapple (the pull and the vault over the ledge), dd_grapple074 (quickTune for DAREDEVIL, capTune for
// CAPTAIN AMERICA, Coast, JumpOut). The layer feeds it the hit his ray bridge previews for each hand
// (SpidyAimSample, an export of iilyn's spidy_ray_bridge.dll, CONFIRMED by name in his 0.7.0 sources) and drives
// the body with the velocity through his SpidyMotionSubmit, as the flight does.
//
// Controls (only while the hero is CAPTAIN AMERICA or DAREDEVIL and live.txt grapple=1):
//   hold a GRIP (his web on that grip is hidden from him) and pull that hand's TRIGGER: the line fires at what the
//   hand points at (a wall or roof 2.5 to 80 m off) and pulls you there, then vaults you over the edge;
//   fire again from either hand while pulled: the pull turns onto the new line, keeping your speed;
//   let go of the line's grip: the pull stops and you coast on with your speed;
//   press A while pulled: the line lets go and you are launched up (DAREDEVIL's JUMP OUT).
#pragma once
#include "spidy/cape_glide.hpp"
#include "spidy/dd_grapple074.hpp"
#include "spidy/math.hpp"

#include <cstdint>

namespace spidy::sm1 {

// His ray bridge's per-hand preview (game_swing::Aim, 32 bytes; AimKind anchor = 1, air = 2).
inline constexpr uint32_t aimAnchor = 1;

struct GrappleInput {
    bool allowed = false;  // the hero, the switch, his movement module, his VR data, the menu shut
    bool captain = false;  // capTune (CAPTAIN AMERICA) or quickTune (DAREDEVIL)
    float grip[2]{}, trigger[2]{};
    bool aimOk[2]{};       // a fresh preview for that hand
    uint32_t aimKind[2]{};
    Vec3 aimPoint[2]{}, aimNormal[2]{};
    Vec3 body{}, velocity{};
    bool grounded = false;
    bool jump = false;     // A held
    float dt = 0;
};
struct GrappleStep {
    bool drive = false;    // submit `velocity` this frame
    Vec3 velocity{};
    bool started = false, missed = false, cancelled = false, jumped = false, ended = false;
    int line = -1;         // the hand the line is drawn from, -1 none
    Vec3 anchor{};
};

class GrappleDriver {
  public:
    GrappleStep step(const GrappleInput& in);
    bool driving() const { return pull_.phase() != cape_glide::idle || coast_.on(); }
    bool gripHeld(int hand) const { return hand >= 0 && hand < 2 && grip_[hand]; }
    void stop();
    uint32_t fires() const { return fires_; }
    uint32_t misses() const { return misses_; }

  private:
    cape_glide::Grapple pull_;
    dd_grapple074::Coast coast_;
    dd_grapple074::JumpOut jumpOut_;
    bool grip_[2]{}, trigger_[2]{};
    Vec3 carried_{};
    uint32_t fires_ = 0, misses_ = 0;
};

} // namespace spidy::sm1
