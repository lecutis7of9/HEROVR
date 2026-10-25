#pragma once
#include "math.hpp"
#include "slow_motion.hpp"
#include "vertex.hpp"
#include <limits>
#include <vector>

// Update 36: the comic style's focus meter becomes the 2004 console game's
// health ring, on the underside of the left wrist. A chrome rim; a red disc
// with Spidy's own spider (the art atlas's emblem); between them two tapered
// crescents, thin on the left and wide on the right: HEALTH in red above,
// FOCUS in blue below. Each fill sits at the thin left tip and reaches right
// as far as its share, so each drains from the wide right end toward the
// left; what is empty is dark glass. The fill's edge is exactly where the
// share puts it (no steps); the caller eases health's jumps (Eased). It is the focus meter's
// replacement, so it shows exactly when the meter did (SlowMotionView::meter
// is its opacity; the caller keeps the meter's conditions), and keeps the
// meter's signs: in slow motion the focus glows and breathes, a refused press
// or an empty meter flashes it red.
namespace spidy::health_ring {
// Where it sits from the left hand's glove pose (fingers along -z, the back
// of the hand +y): the meter's place mirrored through the wrist, under it.
inline constexpr Vec3 fromHand{0, -.045f, .065f};
// Sizes, metres: the rim's outer radius; the disc's (with its rim), whose
// centre is this far left of the ring's so the crescents taper.
inline constexpr float outerRadius = .026f, discRadius = .0132f, discShift = .0074f;
// The health shown: hitPoints over their most, 0..1; NaN (or any value
// outside 0..1.001) where they cannot be read, drawn as an empty crescent
// marked N/A, never a guess.
float healthShare(float hitPoints, float maxHitPoints);
// The shown share follows the read one with a short ease (a hit's drop
// glides in about a fifth of a second instead of jumping): a time constant
// of `tau` seconds, snapping when it is unknown (NaN, shown at once as
// N/A), when it first becomes known, or within a thousandth.
struct Eased {
    float shown = std::numeric_limits<float>::quiet_NaN();
    float update(float target, float seconds, float tau = .06f);
};
// Appends the ring as `art` vertices for D3D12Renderer::renderViews (each
// vertex's shape.xy its atlas coordinates), facing the viewer as the meter
// did. `health` from healthShare; `seconds` any running clock. Nothing when
// slow.meter is 0, anything is not finite, or it is nearer than 8 cm or
// further than 3 m from the viewer.
void append(std::vector<Vertex>& art, const SlowMotionView& slow, float health, Pose hand, Vec3 viewer,
            float pixelAngle, float seconds);
// Update 37: the ring flush on the wrist's underside at the web shooter (live.txt ring_face
// 1), fading as the wrist turns away from the eyes. Update 37.2: ring_face 0, facing the
// viewer as Update 36 did, is the default again (flat = false). ring_offset: millimetres toward the forearm from the
// glove's centre; ring_tilt: degrees about the wrist's width.
struct Place {
    bool flat = false;
    float offsetMm = fromHand.z * 1000, tiltDegrees = 0;
    static constexpr float minOffset = 0, maxOffset = 150, maxTilt = 60;
    static constexpr float skin = .012f;                       // above the glove's underside
    static constexpr float fadeWhole = .57f, fadeGone = .17f;  // cosines: ~55 and ~80 degrees
};
struct Placed {
    bool valid{};
    Vec3 centre{}, normal{}, up{}, right{};
    float distance{}, facing{}, fade{};
};
Placed placed(Pose hand, const Place& place, Vec3 viewer);
void append(std::vector<Vertex>& art, const SlowMotionView& slow, float health, Pose hand, const Place& place,
            Vec3 viewer, float pixelAngle, float seconds);
} // namespace spidy::health_ring
