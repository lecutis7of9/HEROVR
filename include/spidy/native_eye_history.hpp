#pragma once
#include "body_calibration.hpp"
#include "gadget_wheel.hpp"
#include "game_swing.hpp"
#include "game_tracking.hpp"
#include "offer_pointing.hpp"
#include "phone.hpp"
#include "presentation_gate.hpp"
#include "web_cocoon.hpp"
#include "hero007_visual.hpp"
#include "web_visual.hpp"

#include <limits>

namespace spidy {
struct NativeEyeFrame {
    uint64_t serial{};
    GameMotionFrame motion;
    std::array<TrackedEye, 2> trackingEyes;
    std::array<game_swing::WebState, 2> webs;
    std::array<WebTimeline::Hand, 2> webTimes{};
    // The game rope that draws each hand's web (WebRopes; -1: none). One web
    // in both hands: the hand its strand comes to first, the other hand's
    // part running from wrist to wrist; -1 otherwise.
    std::array<int, 2> webSlots{-1, -1};
    int strandUpper = -1;
    // What each hand's grip press would do (game_swing::AimKind none while
    // the aim markers are off).
    std::array<game_swing::Aim, 2> aims{};
    // The game's interaction each hand points at on Spidy's wall (handle 0:
    // none), drawn instead of its aim marker.
    std::array<offer_pointing::Pointed, 2> offers{};
    // The T-pose calibration's panel and controller rings (phase idle: none).
    body_calibration::View calibration{};
    // Slow motion: the image's recolouring and the focus meter (none at rest).
    SlowMotionView slow{};
    // Update 36: the hero's share of his most hit points, for the wrist ring
    // (NaN: unread).
    float health = std::numeric_limits<float>::quiet_NaN();
    // Update 38: each hand's web-ball glob in this frame's world (BALL LOOK GLOW; fade 0:
    // none), and the frame's world-from-tracking pose (RING ANCHOR HAND).
    std::array<Vec3, 2> globAt{}, globTail{};
    std::array<float, 2> globFade{};
    // Update 38.2 GRAB WEB: each hand's rope to the enemy it holds, this frame's world (on 0: none).
    std::array<Vec3, 2> grabFrom{}, grabTo{};
    std::array<float, 2> grabOn{};
    // Update 38.3 GADGET HINT: each hand's label (combat38::HintLabel), where (this frame's
    // world, over the wrist) and its fade (0: none).
    std::array<Vec3, 2> hintAt{};
    std::array<float, 2> hintFade{};
    std::array<uint32_t, 2> hintLabel{};
    // Version 0.0.3 WALL STICK: the web nets on walls and floors (this frame's world, fade 0: none);
    // AUTO PERFORMANCE's SHOW STATUS place (over the left wrist, world; ok 0: not shown).
    std::array<Vec3, 6> netAt{}, netNormal{};
    std::array<float, 6> netFade{}, netSeed{};
    Vec3 perfAt{};
    float perfAtOk{};
    // Version 0.0.3 WEB BALL HIT: the webbed-up look on struck enemies (feet in this frame's world; on false: none).
    std::array<web_cocoon::Cocoon, web_cocoon::Set::most> cocoons{};
    // Version 0.0.2 BALL LOOK OURS: our balls (fade 1 flying, 0 none) and their splashes, this
    // frame's world; TRAINING's guide (world), its label's place and two lines.
    std::array<Vec3, 6> oursAt{}, oursTail{}, splashAt{};
    std::array<float, 6> oursFade{}, splashFade{};
    // Version 0.0.4 IRON MAN ABILITIES (REPULSOR): the shots above drawn as blasts and flashes (no web
    // look); the chest beam (this frame's world; fade 0: none). Version 0.0.5: the beam's cooldown
    // as a ring on the right wrist (ok 0: not shown; share 0..1 charged; flash 1..0 just ready),
    // instead of 0.0.4's floating "BEAM n S" line.
    bool repulsorLook{};
    Vec3 beamFrom{}, beamTo{};
    float beamFade{}, beamRingOk{}, beamShare{}, beamFlash{};
    // Version 0.0.62 HAND COOLDOWN: each hand's 2 s ring on its own wrist (ok 0: hidden; share 0..1; flash 1..0 ready).
    std::array<float, 2> blastRingOk{}, blastShare{1.f, 1.f}, blastFlash{};
    // Version 0.0.6 (Update 39.0.2) REPULSOR LOOK: each palm's charge glow (0: none), place and way out.
    std::array<Vec3, 2> palmAt{}, palmWay{};
    std::array<float, 2> palmGlow{};
    float palmSpin{};
    // Version 0.0.6 DAREDEVIL ABILITIES: each hand's baton (this frame's world; on 0: none), grip to tip.
    std::array<Vec3, 2> batonGrip{}, batonTip{};
    std::array<float, 2> batonOn{};
    // Version 0.0.7: BATON LOOK CLUB (the billy club instead of the 0.0.62 baton) and the 0.0.7 heroes'
    // looks (hero007_visual.hpp; any false: nothing - always nothing while the 0.0.7 options are OFF).
    bool batonClub{};
    hero007::Draw hero007{};
    // Version 0.0.62 HIT INDICATOR: the hit marks (this frame's world; fade 0: none - always 0 while OFF).
    std::array<Vec3, 8> markAt{};
    std::array<float, 8> markFade{};
    // Version 0.0.5 SUPERSONIC: the shockwave (fade 1..0, 0 none), the flight's way (world, unit), its age.
    float shockFade{}, shockAge{};
    Vec3 shockWay{};
    training_guide::Draw guide{};
    Vec3 guideAt{};
    char guideLine[2][72]{};
    Pose worldFromTracking{};
    // The gadget wheel, open while X is held (gadget_wheel.hpp).
    GadgetWheelView gadgetWheel{};
    // The phone, open from a pocket draw until B: whether the image has it,
    // and the image's way into its world (phone.hpp).
    PhoneImage phone{};
    // How Spidy's marks look in this image (the tab's UI STYLE).
    UiStyle uiStyle{};
    bool flatScreen{};
    // The game screen in stereo (eye command 3): the pair goes on the screen,
    // the left image for the left eye and the right one for the right.
    bool stereoScreen{};
    Pose screenPose{};
    float screenAspect = 16.f / 9;
};
class NativeEyeHistory {
  public:
    void remember(const NativeEyeFrame& frame) {
        frames_[frame.serial % frames_.size()] = frame;
    }
    const NativeEyeFrame* find(uint64_t serial, int64_t now) const {
        const auto& frame = frames_[serial % frames_.size()];
        const auto captured = frame.motion.predictedDisplayTime;
        return serial && frame.serial == serial && captured > 0 && now >= captured &&
                       now - captured <= static_cast<int64_t>(imageHoldMs) * 1000000
                   ? &frame
                   : nullptr;
    }
    void clear() {
        for (auto& frame : frames_)
            frame.serial = 0;
    }

  private:
    std::array<NativeEyeFrame, 256> frames_{};
};
} // namespace spidy
