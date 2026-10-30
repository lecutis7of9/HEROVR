#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
// Made by Agent Mulder.
//
// 0.0.74 test 3 FINGER RAY (owner: "the dash laser comes out below the pinky - it should come from the index
// fingertip, along the finger; both hands, every vehicle and the Batcomputer; a bigger hit area or a snap").
// CONFIRMED (code): the dash pointer (van_screen / batcave074) started its beam at the controller's GRIP pose
// origin (the handle's centre, in the palm) and touched fingerReach (.085 m) out along the hand's AIM AXIS ray
// (by default the web shooter's, pitched down from the handle), so the beam left the bottom of the fist.
// The grip pose's axes (tracking.hpp, CONFIRMED in its comments): -y where the hand points (wrist to
// knuckles), -z the thumb side (up), +x out of the left palm / into the right.
// CANDIDATE (hand measurements, not the runtime's hand skeleton - the mod reads none): a pointing index's tip
// sits ~4 cm ahead of and ~3 cm above the grip origin at its knuckle, the finger ~7.5 cm long, pointing along
// the hand tilted 10 degrees up toward the thumb side: tip ~(0, -.114, -.043) in the grip pose.
// ON: the beam starts at that tip and runs along that finger for every vehicle's dash and the Batcomputer;
// the touch is the tip itself; the hit area grows (fingerSlack) and the cursor snaps to the nearest button
// within snapReach. OFF: test 2b exactly.
#include "spidy/math.hpp"

#include <cmath>
#include <cstdint>

namespace spidy::finger_ray {

inline constexpr Vec3 knuckleInGrip{0, -.04f, -.03f};
inline constexpr float fingerLength = .075f;
// The finger's direction in the grip pose (unit): along -y, 10 degrees toward -z.
inline constexpr Vec3 dirInGrip{0, -.98481f, -.17365f};
inline constexpr Vec3 tipInGrip{knuckleInGrip.x + dirInGrip.x * fingerLength, knuckleInGrip.y + dirInGrip.y * fingerLength,
                                knuckleInGrip.z + dirInGrip.z * fingerLength};
// The grown hit area (design units of margin round the dash face / Batcomputer panel) and the snap's reach.
inline constexpr float dashSlack = .05f, dashSnap = .03f, panelSlack = .06f, panelSnap = .05f;

struct Box {
    float x0, y0, x1, y1;
};
// The box `p` (design units) snaps to: inside one (its index), else the nearest within `reach` of its edge;
// -1: none. `centre` is that box's centre (p unchanged when none).
inline int snap(float px, float py, const Box* boxes, uint32_t n, float reach, float& cx, float& cy) {
    cx = px, cy = py;
    if (!std::isfinite(px) || !std::isfinite(py) || !boxes)
        return -1;
    int best = -1;
    float bestD = std::isfinite(reach) && reach > 0 ? reach : 0.f;
    for (uint32_t k = 0; k < n; ++k) {
        const Box& b = boxes[k];
        const float dx = std::fmax(std::fmax(b.x0 - px, px - b.x1), 0.f), dy = std::fmax(std::fmax(b.y0 - py, py - b.y1), 0.f);
        const float d = std::sqrt(dx * dx + dy * dy);
        if (d <= bestD && (best < 0 || d < bestD || d == 0)) {
            best = static_cast<int>(k);
            bestD = d;
            if (d == 0)
                break;
        }
    }
    if (best >= 0) {
        cx = (boxes[best].x0 + boxes[best].x1) * .5f;
        cy = (boxes[best].y0 + boxes[best].y1) * .5f;
    }
    return best;
}

} // namespace spidy::finger_ray
