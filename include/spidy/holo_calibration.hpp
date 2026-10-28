#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
//
// Version 0.0.73 test 2 HOLO CALIBRATION (fix 25, the user's concept 1): a blue and white holographic
// checklist - STAND TALL, T-POSE ARMS, TOUCH BELT, TOUCH BACK, TOUCH HIP HOLSTERS, CONFIRM - beside a
// wireframe body in a TRUE T-pose (both arms straight out to the sides, level with the shoulders; never the
// raised-arm Y pose).
//  - STAND TALL and T-POSE ARMS are body_calibration's measure (eye height, arm length) unchanged: the
//    checklist only shows them.
//  - Then each touch: the hand on the spot and the grip squeezed. BELT: either hand at the front of the
//    waist. BACK: either hand over a shoulder, behind the head. HIP HOLSTERS: the left hand at the left hip
//    and the right at the right (either order). A touch outside its plausible area is refused (a hint says
//    where), never taken.
//  - CONFIRM: A or both triggers. Only then is the fit (holster::Fit: the belt, back and hip shifts) given;
//    B skips the touch steps and keeps the measured height and arms (the holsters stay at the defaults).
// Pure: tracking-space samples in, the fit and what to draw out; no game memory, no address.
#include "spidy/body_calibration.hpp"
#include "spidy/holster.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace spidy::holo_calibration {
enum Step : uint32_t { stand = 0, tpose, belt, back, holsters, confirm, finished, steps = finished };
inline constexpr const char* stepNames[steps] = {"STAND TALL", "T-POSE ARMS", "TOUCH BELT", "TOUCH BACK",
                                                 "TOUCH HIP HOLSTERS", "CONFIRM"};
enum TouchHint : uint32_t { touchNone = 0, touchSqueeze, touchLower, touchHigher, touchBehind, touchSides,
                            touchConfirm, touchTracking, touchHints };
const char* touchText(uint32_t hint);

// The standing-average places the shifts are measured from (holster.hpp's body frame, standingEye metres).
inline constexpr float refBeltBelow = .55f, refBackBelow = .12f, refHipBelow = .64f, refHipSide = .20f;
inline constexpr float gripPressed = .6f, gripReleased = .3f;

struct Input {
    bool headTracked{};
    Pose head{};
    std::array<bool, 2> tracked{};
    std::array<Vec3, 2> hands{};
    std::array<float, 2> grips{};
    std::array<float, 2> triggers{};
    bool a{};
};
// What was touched (body frame points, standingEye metres; valid only with its flag).
struct Touches {
    bool belt{}, back{};
    std::array<bool, 2> hips{};
    holster::BodyPoint beltAt{}, backAt{};
    std::array<holster::BodyPoint, 2> hipAt{};
};

class Flow {
public:
    // The T-pose measured (eye height, metres): the touch steps begin.
    void start(float eyeHeight);
    void stop() { step_ = finished, active_ = false; }
    // B: the touch steps skipped; nothing fitted.
    void skip() { skipped_ = true, stop(); }
    // True the frame CONFIRM is given (fit() is then the player's).
    bool update(const Input&);
    bool active() const { return active_; }
    bool skipped() const { return skipped_; }
    uint32_t step() const { return step_; }
    uint32_t hint() const { return hint_; }
    uint32_t doneMask() const;
    const Touches& touches() const { return touches_; }
    // The shifts from the touches (a step not touched: 0, its default place).
    holster::Fit fit() const;

private:
    bool active_{}, skipped_{};
    uint32_t step_ = finished, hint_ = touchNone;
    float scale_ = 1;
    std::array<bool, 2> gripHeld_{}, triggerHeld_{};
    bool aHeld_{};
    Touches touches_{};
    void advance();
};
// The checks a touch passes (exposed for the tests): true when `p` is a plausible spot for the step
// (hand 0 left, 1 right for the hips); `hint` the reason when not.
bool plausible(uint32_t step, int hand, holster::BodyPoint p, uint32_t& hint);

// The T-pose figure's joints in the panel's figure box (x right, y up, the box -1..1): a TRUE T-pose - the
// hands at exactly the shoulders' height. Exposed so the tests hold it to that.
struct Figure {
    Vec3 head, neck, shoulders[2], elbows[2], hands[2], chest, waist, hips[2], knees[2], feet[2];
};
Figure tPoseFigure();

// The panel (body_calibration::View with holo set, every phase) and the controllers' rings; drawn with the
// calibration art atlas' white texel as calibration_panel is.
void append(std::vector<Vertex>& out, const body_calibration::View& view, float clock);
} // namespace spidy::holo_calibration
