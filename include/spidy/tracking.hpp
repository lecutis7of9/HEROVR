#pragma once
#include "math.hpp"
#include <cstdint>

namespace spidy {
struct TrackedHand {
    Pose aim, grip;
    bool valid{};
    float trigger{}, squeeze{}, stickX{}, stickY{};
    bool stickClick{};
};
struct EyeFov {
    float left{}, right{}, down{}, up{};
};
struct TrackedEye {
    Pose pose{};
    EyeFov fov{};
    unsigned width{}, height{};
};
// Face and menu buttons of the VR controllers (XrFrame::buttons). On Touch
// controllers A and B are on the right, X, Y and the menu button on the left.
// The Steam Frame's have all five on the right, and on the left a View button
// and a D-pad, which no other controller has (controller_layout.hpp).
enum ControllerButton : uint32_t {
    buttonA = 1,
    buttonB = 2,
    buttonX = 4,
    buttonY = 8,
    buttonMenu = 16,
    buttonView = 32,
    buttonDpadUp = 64,
    buttonDpadDown = 128,
    buttonDpadLeft = 256,
    buttonDpadRight = 512,
};
struct XrFrame {
    Pose head{};
    std::array<TrackedHand, 2> hands{};
    std::array<TrackedEye, 2> eyes{};
    std::int64_t predictedDisplayTime{};
    float seconds = 1.f / 90;
    bool focused{}, valid{}, jump{}, reset{}, recentered{};
    uint32_t buttons{};
};
inline bool validTrackedPose(Pose p) {
    const auto q = p.orientation;
    const float n = q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w;
    return finite(p.position) && std::isfinite(n) && std::abs(n - 1) < .001f;
}
// Spider-Man's web shooter on the hand a controller's OpenXR grip pose holds:
// on the palm's side of the wrist the player's body draws there
// (body::Config::wristFromGrip has that wrist 9 cm from the grip point toward
// the forearm, the grip's +y, and 2 cm toward the back of the hand), looking
// where the hand points - from the wrist out past the knuckles, the grip's -y
// - with the thumb's side up, the grip's -z. The grip's +x is normal to the
// palm and to the right on both hands: out of a left palm, into a right one.
// hand: 0 the left, 1 the right.
//
// Webs leave from here, and web balls, picks and pointing aim along it. Until
// October 15 they aimed along the controller's aim pose, which Meta's runtime
// (and Virtual Desktop's, calibrated to it) turns 30 degrees from the hand
// toward the thumb: with the palm down, as a web is shot, 30 degrees inward
// of where the hand pointed ("it feels off to the side a bit").
//
// Where: the suit's own nozzle, the hero rig's webStart joint (on the end of
// the forearm, 1.5 cm back from the wrist joint toward the elbow and 3 cm out
// on the palm's side, reports/body-probe-f3-hands.json): 10.5 cm from the grip
// toward the forearm, 1 cm toward the palm. From October 15 to 18 it was 2.5
// cm further on toward the fingers, on the heel of the palm, where the web
// shooter's button is ("it comes out of the button instead of the webshooter
// part", October 16).
//
// Update 20: on the underside of the wrist, at the heel of the palm. The
// grip's +y is not the forearm: a hand round a Touch handle holds it tilted
// some 30 degrees, so +y rises off the arm toward the thumb's side, and 10.5
// cm along it was 5 cm above the wrist, behind the hand; and the line out of
// it along -y ran through the grip, the middle of the closed fist (October
// 23, Quest 3: "really weird coming out of hand"). Now 5.8 cm along the
// forearm (the grip's +y turned 30 degrees toward its +z, the little
// finger's side: 5 cm on +y, 3 cm on +z), 3.5 cm out on the palm's side, so
// the line passes 4.6 cm from the grip, below and in front of the curled
// fingers. It still looks where the hand points, the grip's -y: only where
// webs leave moved, not where they go.
inline Pose webShooter(Pose grip, unsigned hand) {
    const float palm = hand == 0 ? .035f : -.035f;
    return compose(grip, {{palm, .05f, .03f}, {-.70710678f, 0, 0, .70710678f}});
}
// Update 23: where a web is DRAWN leaving the hand (not where it aims: that
// stays webShooter's line). Update 20's spot was 5.5 cm out from the wrist
// joint body_ik draws (body::Config::wristFromGrip: 9 cm toward the forearm,
// 2 cm toward the back of the hand), past the palm's skin and 3 cm toward the
// little finger, so the October 23 player saw webs leave "at a weird spot"
// in the air under the drawn hand. Now on the drawn wrist itself: the hero
// rig's webStart joint, 1.5 cm toward the elbow from that wrist joint and
// 3 cm out on the palm's side (reports/body-probe-f3-hands.json), on both
// hands alike (a left hand mirrors x). live.txt web_start = palm (1) draws
// from Update 20's spot again.
inline constexpr unsigned webStartWrist = 0, webStartPalm = 1, webStarts = 2;
inline Vec3 webStart(Pose grip, unsigned hand, unsigned spot = webStartWrist) {
    if (spot == webStartPalm)
        return webShooter(grip, hand).position;
    const float palm = hand == 0 ? .01f : -.01f;
    return compose(grip, {{palm, .105f, 0}, {0, 0, 0, 1}}).position;
}
} // namespace spidy
