// HERO VR 0.0.78 cut24 WAYPOINT LOCK tests (include/spidy/prompt_split.hpp).
// cut23 placed a marker by reading its layer place through the head that is showing the picture, 6 m out. These
// tests hold the icon on the ray of the camera that drew the pixels and then move the viewer. The game's destination
// position is not read: the hold is markerLockMetres along that ray. Code and these tests are not a headset run.
#include "spidy/game_pad.hpp"
#include "spidy/prompt_split.hpp"
#include "spidy/prompt_world.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

using namespace spidy;
using namespace spidy::prompt_split;

namespace {
int failures = 0;
void check(bool ok, const char* what) {
    std::printf("%s: %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok)
        ++failures;
}
constexpr float deg = 3.14159265f / 180.f;
Pose headAt(float yawDeg, float pitchDeg = 0, Vec3 at = {0, 1.6f, 0}) {
    return {at, Quat::yaw(yawDeg * deg) * Quat::around({1, 0, 0}, pitchDeg * deg)};
}
float bearingError(const Pose& eye, Vec3 world, Vec3 destination) {
    const Vec3 a = normalized(world - eye.position), b = normalized(destination - eye.position);
    if (!finite(a) || !finite(b))
        return 180.f;
    return std::acos(std::min(1.f, dot(a, b))) / deg;
}
void questEyes(const Pose& head, Pose eyes[2], float fov[2][4]) {
    for (int e = 0; e < 2; ++e) {
        eyes[e] = compose(head, Pose{{e ? .032f : -.032f, 0, 0}, {}});
        fov[e][0] = (e ? -44.f : -54.f) * deg;
        fov[e][1] = (e ? 54.f : 44.f) * deg;
        fov[e][2] = -48.f * deg;
        fov[e][3] = 48.f * deg;
    }
}
// Where `source` draws a world point into its layer. The tangent is what markerPlace reads back.
bool tangentOf(const Pose& source, const Lens& lens, Vec3 world, float& tx, float& ty) {
    const Vec3 l = source.orientation.conjugate().rotate(world - source.position);
    if (!(-l.z > .05f))
        return false;
    const float u = (l.x / -l.z - lens.left) / (lens.right - lens.left);
    const float v = (lens.up - l.y / -l.z) / (lens.up - lens.down);
    if (!(u > 0 && u < 1 && v > 0 && v < 1))
        return false;
    layerTangent(lens, u, v, tx, ty);
    return std::isfinite(tx + ty);
}
Mat4 vpOf(const Pose& eye) {
    const float a = 50.f * deg;
    return multiply(projection(-a, a, -a, a, .03f, 2000.f), viewMatrix(eye));
}
float rectCentreX(const float r[4]) {
    return (r[0] + r[2]) * .5f;
}
} // namespace

int main() {
    check(markerLockMetres > 500.f && markerMetres < 20.f, "the hold is far; cut23's 6 m stays named and unused");
    check(markerDepth(false) == markerLockMetres && markerDepth(true) == markerViewMetres &&
              markerViewMetres < 3.f,
          "CANDIDATE: a stick-up zip marker is in the view, not at the 1024 m far stand-in");
    game_pad::ZipPress zip;
    check(!zip.zipOnScreen(), "no stick-up zip: the circle is not claimed on screen");
    check(zip.update(.016f, true, true, false).zip && zip.zipOnScreen(),
          "stick up sends the game's zip and the circle path is the view, not 1024 m");
    zip.update(.016f, false, true, false);
    check(!zip.zipOnScreen(), "stick released before the aim: circle path off");
    const Vec3 destination{0, 20.f, -1173.f}; // the video's 1173 m objective, level with a rooftop camera
    const Pose capture = headAt(0, 0, {0, 20.f, 0});
    Pose eyes[2];
    float fov[2][4];
    questEyes(capture, eyes, fov);
    Lens lens;
    check(headLens(capture, eyes, fov, 10.f, lens), "the capture camera has a lens");
    float tx{}, ty{};
    check(tangentOf(capture, lens, destination, tx, ty), "the game drew the objective in that camera's layer");

    Vec3 locked{}, near{};
    check(lockPoint(capture, tx, ty, markerLockMetres, locked) && lockPoint(capture, tx, ty, markerMetres, near),
          "both holds lie on the capture ray");
    check(bearingError(capture, locked, destination) < .05f && bearingError(capture, near, destination) < .05f,
          "from the capture camera either distance is the same pixel");

    // The picture is shown later: the head has yawed, pitched, and the hero has fallen off the roof.
    const Pose fallen = headAt(25.f, -40.f, {1.5f, 12.f, 2.f});
    const float lockErr = bearingError(fallen, locked, destination);
    const float nearErr = bearingError(fallen, near, destination);
    Vec3 dragged{};
    check(lockPoint(fallen, tx, ty, markerMetres, dragged), "cut23 re-aims the old tangent from the new head");
    const float dragErr = bearingError(fallen, dragged, destination);
    std::printf("    bearing error deg: lock %.3f, 6 m on the capture ray %.3f, cut23 re-aim %.3f\n", lockErr,
                nearErr, dragErr);
    check(lockErr < 1.f, "yaw, pitch and an 8 m fall: the 1024 m hold stays within 1 degree of 1173 m");
    check(nearErr > 20.f, "the same fall: 6 m on that ray points off the objective (the roof)");
    check(dragErr > 20.f, "re-aiming the old pixels from the new head drags the icon off the objective");

    const Pose yawed = headAt(35.f, 0, capture.position);
    const Pose lookedUp = headAt(0, 30.f, capture.position);
    Vec3 yawDrag{}, pitchDrag{};
    lockPoint(yawed, tx, ty, markerMetres, yawDrag);
    lockPoint(lookedUp, tx, ty, markerMetres, pitchDrag);
    check(bearingError(yawed, locked, destination) < .05f && bearingError(yawed, yawDrag, destination) > 20.f,
          "a right head turn leaves the locked icon on the objective; cut23 drags it");
    check(bearingError(lookedUp, locked, destination) < .05f && bearingError(lookedUp, pitchDrag, destination) > 20.f,
          "looking up leaves the locked icon on the objective; cut23 carries it up");

    // The hero moves in the game world. The icon's world point does not; tracking space takes the difference.
    const Pose before{{0, 0, 0}, {}};
    const Pose after{{10.f, -8.f, 4.f}, Quat::yaw(20.f * deg)};
    const Vec3 world = locked;
    const Vec3 trackBefore = trackingOfWorld(before, world);
    const Vec3 trackAfter = trackingOfWorld(after, world);
    check(length(trackBefore - world) < 1e-3f, "identity world-from-tracking leaves the point");
    const Vec3 back = compose(after, Pose{trackAfter, {}}).position;
    check(length(back - world) < 1e-2f, "after a move and a yaw the tracking point is still that world point");

    // Stereo: both eyes project one world point. A 1024 m point's parallax matches 1173 m, not 6 m.
    const Pose left = compose(capture, Pose{{-.032f, 0, 0}, {}});
    const Pose right = compose(capture, Pose{{.032f, 0, 0}, {}});
    const float tans[4] = {-std::tan(50.f * deg), std::tan(50.f * deg), std::tan(50.f * deg), -std::tan(50.f * deg)};
    auto disparity = [&](Vec3 p) {
        float rectL[4]{}, rectR[4]{}, crop[4]{}, full[4] = {0, 0, 1, 1};
        const float width = std::max(1.f, length(p - capture.position)) * .05f; // a few degrees, on the screen
        const bool okL = prompt_world::cardRect(vpOf(left), p, width, 1.f, tans[0], tans[1], tans[2], tans[3], 1000,
                                                1000, full, rectL, crop);
        const bool okR = prompt_world::cardRect(vpOf(right), p, width, 1.f, tans[0], tans[1], tans[2], tans[3], 1000,
                                                1000, full, rectR, crop);
        check(okL && okR, "both eyes see the point");
        return std::abs(rectCentreX(rectL) - rectCentreX(rectR));
    };
    const float dLock = disparity(locked), dFar = disparity(destination), dNear = disparity(near);
    std::printf("    stereo disparity px: lock %.3f, 1173 m %.3f, 6 m %.3f\n", dLock, dFar, dNear);
    check(std::abs(dLock - dFar) < std::abs(dNear - dFar), "the far hold's stereo is closer to 1173 m than 6 m is");

    // Look away: the world point leaves the view. The HUD point stays in the head.
    const Pose away = headAt(160.f, 0, capture.position);
    float rect[4]{}, crop[4]{}, full[4] = {0, 0, 1, 1};
    check(!prompt_world::cardRect(vpOf(away), trackingOfWorld(before, locked), 8.f, 1.f, tans[0], tans[1], tans[2],
                                  tans[3], 1000, 1000, full, rect, crop),
          "looking away, the waypoint leaves the view");
    const auto hud = prompt_layer::Region{60, 40, 360, 80, 30};
    float cx{}, cy{}, tw{}, th{};
    check(hudPlace(hud, 1280, 720, cx, cy, tw, th), "the health bar still places");
    const Vec3 hudA = headPoint(capture, cx, cy, hudMetres);
    const Vec3 hudB = headPoint(away, cx, cy, hudMetres);
    const Vec3 localA = capture.orientation.conjugate().rotate(hudA - capture.position);
    const Vec3 localB = away.orientation.conjugate().rotate(hudB - away.position);
    check(length(localA - localB) < 1e-3f, "health stays at the same place in the head while the head turns");

    // These poses and tangents are supplied by the test. Nothing here reads the game's classifier output or proves
    // the latched eye command is the camera cohtml used.
    check(matchedCapture(0, 0) == 0 && matchedCapture(5, 0) == 0 && matchedCapture(5, 9) == 0 &&
              matchedCapture(5, 5) == 5,
          "a marker locks only from the serial stored with its pixels; the frame on the headset is not a substitute");
    const Box edge{.1f, .4f, .14f, .44f}; // outer band: the same shape as cut23's "edge waypoint"
    check(prior(edge) == kindHud, "KNOWN HOLE: an outer-band piece starts as HUD, diamond or health bar alike");
    Split hole;
    Box center[1] = {{.45f, .40f, .50f, .45f}};
    hole.update(center, 1, headAt(0).orientation);
    check(hole.kind(0) == kindMarker, "a centre piece starts as a marker, before any vote");
    hole.update(center, 1, headAt(15).orientation);
    check(hole.kind(0) == kindHud,
          "KNOWN HOLE: a centre piece that never leaves its first place is voted HUD when the head turns");
    // Session 162732 prompts log: one blob left the left edge, was locked, then the same box was view-locked on the
    // next sample. A piece that has left its first place stays a marker through that still sample.
    Split logged;
    const Box seen[] = {
        {.02f, .48f, .08f, .56f}, {.03f, .62f, .09f, .69f}, {.16f, .57f, .20f, .67f},
        {.23f, .53f, .29f, .63f}, {.30f, .51f, .35f, .58f}, {.46f, .39f, .52f, .49f},
        {.46f, .42f, .52f, .52f}, {.46f, .42f, .52f, .52f}, {.02f, .46f, .08f, .56f},
    };
    logged.update(seen + 0, 1, headAt(0).orientation);
    check(logged.kind(0) == kindHud, "the logged blob starts at the left edge as HUD, before it has travelled");
    bool becameMarker = false;
    for (int i = 1; i < 7; ++i) {
        logged.update(seen + i, 1, headAt(0).orientation);
        becameMarker = logged.kind(0) == kindMarker;
    }
    check(becameMarker, "after the logged travel to mid-screen the blob is a marker");
    logged.update(seen + 7, 1, headAt(15).orientation); // same box, head turned: the sample that view-locked it
    check(logged.kind(0) == kindMarker, "a still sample after that travel does not put it back on the head");
    // The log walked back to the left edge in small steps (still one piece). A jump bigger than the match window
    // would be a new piece; these steps stay inside that window.
    const Box walked[] = {{.40f, .48f, .46f, .58f}, {.34f, .48f, .40f, .58f}, {.28f, .48f, .34f, .58f},
                          {.22f, .48f, .28f, .58f}, {.16f, .48f, .22f, .58f}, {.10f, .46f, .16f, .56f},
                          {.04f, .46f, .10f, .56f}};
    for (const Box& b : walked) {
        const Box one[1] = {b};
        logged.update(one, 1, headAt(40).orientation);
    }
    check(logged.kind(0) == kindMarker, "walked back to the left edge, still the world piece");
    Split bar;
    Box health[1] = {{.05f, .06f, .30f, .10f}};
    bar.update(health, 1, headAt(0).orientation);
    health[0] = {.05f, .06f, .18f, .10f}; // lost from the right end only
    bar.update(health, 1, headAt(20).orientation);
    check(bar.kind(0) == kindHud, "a health bar that shrinks from one end stays view-locked");
    PieceNote notes[2] = {{kindHud, .04f, .05f, .28f, .11f, 1}, {kindMarker, .61f, .40f, .66f, .48f, 0}};
    char line[240];
    const uint32_t got = pieceLine(line, sizeof line, 0, -1.f, 0, notes, 2);
    check(got > 0 && std::strstr(line, "match=0") && std::strstr(line, "0H") && std::strstr(line, "view") &&
              std::strstr(line, "1M") && std::strstr(line, "drop") && !std::strstr(line, "lock"),
          "a miss logs the split vote and a drop; it does not claim a world lock or a destination");
    notes[1].how = 2;
    pieceLine(line, sizeof line, 1, 4.2f, 99, notes, 2);
    check(std::strstr(line, "match=1") && std::strstr(line, "turn=4.2") && std::strstr(line, "serial=99") &&
              std::strstr(line, "lock") && std::strstr(line, "view"),
          "a matched sample names the turn and which piece was view-locked or world-locked");
    char tiny[48];
    pieceLine(tiny, sizeof tiny, 1, 0, 1, notes, 2);
    check(std::strstr(tiny, "|+") != nullptr, "a short line stops and marks that a piece was left out");

    std::printf("%s (%d)\n", failures ? "FAILED" : "passed", failures);
    return failures ? 1 : 0;
}
