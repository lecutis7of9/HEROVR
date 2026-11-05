// HERO VR 0.0.78 cut23 PROMPT SPLIT tests (include/spidy/prompt_split.hpp): the game's HUD pieces are locked to the
// view (the same place in the eyes however the head moves or turns), its world markers stand where the game drew them
// (through the game's own view lens on the head, so a marker drawn 50 degrees out lands 50 degrees out, not 20), the
// split tells a screen-fixed piece from a moving one as the head turns, and an edge prompt lands well inside a
// Quest's view.
#include "spidy/prompt_split.hpp"
#include "spidy/prompt_world.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>

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
Pose headAt(float yawDeg, float pitchDeg = 0, Vec3 at = {0, 1.6f, 0}, float rollDeg = 0) {
    return {at, Quat::yaw(yawDeg * deg) * Quat::around({1, 0, 0}, pitchDeg * deg) *
                    Quat::around({0, 0, 1}, rollDeg * deg)};
}
void anglesFrom(const Pose& head, Vec3 p, float& rightDeg, float& upDeg) {
    const Vec3 v = head.orientation.conjugate().rotate(p - head.position);
    rightDeg = std::atan2(v.x, -v.z) / deg;
    upDeg = std::atan2(v.y, std::sqrt(v.x * v.x + v.z * v.z)) / deg;
}
// A Quest 3's eyes (54 degrees out, 44 in, 48 up and down; parallel, 64 mm apart) on `head`.
void questEyes(const Pose& head, Pose eyes[2], float fov[2][4]) {
    for (int e = 0; e < 2; ++e) {
        eyes[e] = compose(head, Pose{{e ? .032f : -.032f, 0, 0}, {}});
        fov[e][0] = (e ? -44.f : -54.f) * deg;
        fov[e][1] = (e ? 54.f : 44.f) * deg;
        fov[e][2] = -48.f * deg;
        fov[e][3] = 48.f * deg;
    }
}
// Where the game draws a room direction `dir` in its layer (`w` x `h`), seen from `head` through `lens`.
bool drawnAt(const Pose& head, const Lens& lens, Vec3 dir, uint32_t w, uint32_t h, float& u, float& v) {
    const Vec3 l = head.orientation.conjugate().rotate(dir);
    if (!(-l.z > .01f))
        return false;
    const float tx = l.x / -l.z, ty = l.y / -l.z;
    u = (tx - lens.left) / (lens.right - lens.left);
    v = (lens.up - ty) / (lens.up - lens.down);
    (void)w;
    (void)h;
    return u > 0 && u < 1 && v > 0 && v < 1;
}
prompt_layer::Region regionAt(float u, float v, uint32_t w, uint32_t h, uint32_t half = 16) {
    const uint32_t cx = static_cast<uint32_t>(u * w), cy = static_cast<uint32_t>(v * h);
    return {cx - half, cy - half, cx + half, cy + half, 4};
}
Mat4 vpOf(const Pose& eye, float t) {
    const float a = std::atan(t);
    return multiply(projection(-a, a, -a, a, .03f, 200.f), viewMatrix(eye));
}
} // namespace

int main() {
    const uint32_t W = 1280, H = 720;
    // The anchor values (live.txt prompt_anchor): split the default, cut21's view and cut22's room kept.
    check(anchorSplit == 0 && anchorView == 1 && anchorRoom == 2, "prompt_anchor 0 split, 1 view, 2 room");
    check(static_cast<uint32_t>(prompt_world::anchorRoom) == anchorRoom &&
              static_cast<uint32_t>(prompt_world::anchorView) == anchorView,
          "prompt_world's anchors agree with prompt_split's");

    // The head lens: as the game's view lens on the head (headView's union x1.05, recording_fov's widening).
    {
        const Pose head = headAt(0);
        Pose eyes[2];
        float fov[2][4];
        for (int e = 0; e < 2; ++e) {
            eyes[e] = compose(head, Pose{{e ? .032f : -.032f, 0, 0}, {}});
            fov[e][0] = fov[e][2] = -45.f * deg;
            fov[e][1] = fov[e][3] = 45.f * deg;
        }
        Lens l;
        const bool ok = headLens(head, eyes, fov, 10.f, l);
        const float want = recording_fov::widenedTangent(1.05f, 10.f);
        check(ok && std::abs(l.right - want) < 1e-3f && std::abs(l.left + want) < 1e-3f &&
                  std::abs(l.up - want) < 1e-3f && std::abs(l.down + want) < 1e-3f,
              "a 90-degree pair: tan 45 x1.05 widened by 10 degrees each way");
        Lens l0;
        headLens(head, eyes, fov, 0.f, l0);
        check(std::abs(l0.right - 1.05f) < 1e-3f, "no margin: the eyes' union x1.05");
        Lens rec;
        check(headLens(head, eyes, fov, 10.f, rec, 150, 16.f / 9) && rec.right > l.right &&
                  std::abs(rec.right - std::tan(75.f * deg)) < .02f,
              "RECORDING FOV 150: recording_fov::covering's wider lens");
        Pose qe[2];
        float qf[2][4];
        questEyes(head, qe, qf);
        Lens q;
        check(headLens(head, qe, qf, 10.f, q) && q.right > 2.f && q.up > 1.4f && q.right > prompt_layer::fieldTan * 2.5f,
              "a Quest 3's head lens is about 130 degrees across, far wider than cut21/22's .72 (72 degrees)");
        float tx{}, ty{};
        layerTangent(q, .5f, .5f, tx, ty);
        check(std::abs(tx) < 1e-5f && std::abs(ty) < 1e-5f, "the layer's middle is straight ahead");
        layerTangent(q, 0, 0, tx, ty);
        check(std::abs(tx - q.left) < 1e-5f && std::abs(ty - q.up) < 1e-5f, "the layer's top left is the lens's");
        Pose bad[2] = {qe[0], qe[1]};
        float badFov[2][4] = {{0, 0, 0, 0}, {0, 0, 0, 0}};
        Lens none;
        check(!headLens(head, bad, badFov, 10.f, none), "an empty pair: no lens");
        const Pose h2 = headOf(headAt(30, 10), headAt(30, 10, {.064f, 1.6f, 0}));
        float r{}, u{};
        anglesFrom(h2, h2.position + headAt(30, 10).orientation.rotate({0, 0, -1}), r, u);
        check(std::abs(r) < .01f && std::abs(u) < .01f && std::abs(h2.position.x - .032f) < 1e-5f,
              "the head is between the eyes, facing their way");
    }

    // WORLD MARKERS: where the game drew them, at their target in its world, however the head turns.
    {
        for (float dirYaw : {-50.f, -20.f, 0.f, 35.f, 55.f})
            for (float headYaw : {0.f, 25.f, -30.f}) {
                const Pose head = headAt(headYaw, headYaw > 0 ? -8.f : 5.f, {.3f, 1.7f, -.2f});
                Pose eyes[2];
                float fov[2][4];
                questEyes(head, eyes, fov);
                Lens lens;
                headLens(head, eyes, fov, 10.f, lens);
                const Vec3 dir = normalized(Quat::yaw(-dirYaw * deg).rotate(Vec3{0, .15f, -1}));
                float u{}, v{};
                if (!drawnAt(head, lens, dir, W, H, u, v))
                    continue;
                const auto r = regionAt(u, v, W, H);
                float cx{}, cy{}, tw{}, th{}, crop[4]{};
                const bool ok = markerPlace(r, W, H, lens, cx, cy, tw, th, crop);
                const Vec3 p = headPoint(head, cx, cy, markerMetres);
                const Vec3 got = normalized(p - head.position);
                char what[160];
                std::snprintf(what, sizeof what,
                              "a marker the game drew %.0f deg right (head %.0f) stands on its target (within 0.5 deg)",
                              dirYaw, headYaw);
                check(ok && std::acos(std::min(1.f, dot(got, dir))) / deg < .5f, what);
            }
        // cut22's mapping put a marker 50 degrees right at about 20: the bug the owner saw ("runs off the side").
        const Pose head = headAt(0);
        Pose eyes[2];
        float fov[2][4];
        questEyes(head, eyes, fov);
        Lens lens;
        headLens(head, eyes, fov, 10.f, lens);
        float u{}, v{};
        drawnAt(head, lens, normalized(Vec3{std::tan(50.f * deg), 0, -1}), W, H, u, v);
        float otx{}, oty{};
        prompt_world::pieceTangent(regionAt(u, v, W, H), W, H, otx, oty);
        check(std::atan(otx) / deg < 30.f, "cut22's fixed .72 view put that 50-degree marker under 30 degrees");
        // Its card: its own size through the lens, its crop padded alike (a moving icon stays whole).
        const auto r = regionAt(.5f, .5f, W, H, 20);
        float cx{}, cy{}, tw{}, th{}, crop[4]{};
        markerPlace(r, W, H, lens, cx, cy, tw, th, crop);
        const float wantTw = (40.f + 2 * markerPadPixels) / W * (lens.right - lens.left);
        check(std::abs(tw - wantTw) < 1e-4f && std::abs(crop[0] - (620.f - markerPadPixels) / W) < 1e-5f &&
                  std::abs(crop[2] - (660.f + markerPadPixels) / W) < 1e-5f,
              "a marker's card is its padded crop at its true size");
        const auto edge = prompt_layer::Region{2, 2, 30, 30, 4};
        markerPlace(edge, W, H, lens, cx, cy, tw, th, crop);
        check(crop[0] == 0 && crop[1] == 0, "padding stops at the layer's edge");
    }

    // VIEW HUD: locked to the view - the same place in each eye however the head moves, never left in the world.
    {
        const auto health = prompt_layer::Region{60, 40, 360, 80, 30};   // top left bar
        const auto yPrompt = prompt_layer::Region{40, 600, 110, 680, 12}; // the bottom-left Y prompt
        float cx{}, cy{}, tw{}, th{};
        check(hudPlace(yPrompt, W, H, cx, cy, tw, th), "the Y prompt is placed");
        const Pose straight = headAt(0);
        float r{}, u{};
        anglesFrom(straight, headPoint(straight, cx, cy, hudMetres), r, u);
        check(r < -10.f && r > -25.f && u < -8.f && u > -22.f,
              "the bottom-left Y lands 10-25 degrees left, 8-22 down: inside a Quest's view");
        check(th >= minCardTan - 1e-6f && th <= maxCardTan[1] + 1e-6f && tw <= maxCardTan[0] + 1e-6f,
              "readable, capped");
        float hx{}, hy{}, hw{}, hh{};
        hudPlace(health, W, H, hx, hy, hw, hh);
        check(hx < 0 && hy > 0 && hw <= maxCardTan[0] + 1e-6f, "the health bar top left, capped");
        // Through each eye's own room pose, as the layer is submitted: the same rect for every head pose.
        float first[2][4]{};
        bool same = true, drawn = true;
        int k = 0;
        for (const Pose& head : {headAt(0), headAt(70, -20, {3, 1, -2}), headAt(-120, 35, {-5, 2.5f, 7}, 15),
                                 headAt(10, 0, {0, -30, 0})}) {
            const Vec3 centre = headPoint(head, cx, cy, hudMetres);
            for (int e = 0; e < 2; ++e) {
                const Pose eye = compose(head, Pose{{e ? .032f : -.032f, 0, 0}, {}});
                const float full[4] = {0, 0, 1, 1};
                float rect[4]{}, crop[4]{};
                drawn &= prompt_world::cardRect(vpOf(eye, 1.f), centre, tw * hudMetres, tw / th, -1, 1, 1, -1, 1000,
                                                1000, full, rect, crop);
                for (int i = 0; i < 4; ++i) {
                    if (!k)
                        first[e][i] = rect[i];
                    else
                        same &= std::abs(first[e][i] - rect[i]) < .05f;
                }
            }
            ++k;
        }
        check(drawn && same, "a HUD card is in the same place in each eye as the head turns, travels and falls");
        // Not on the head's place in the room: a jump or fall moves it with the head (cut22's board stayed).
        const Pose low = headAt(0, 0, {0, -10, 0});
        check(std::abs((headPoint(low, cx, cy, hudMetres) - headPoint(straight, cx, cy, hudMetres)).y + 11.6f) < 1e-3f,
              "a fall of 11.6 m takes the HUD with it");
    }

    // THE SPLIT: screen-fixed pieces are HUD, pieces the game moves are markers.
    {
        Split s;
        const Box mid{.45f, .45f, .5f, .5f};   // middle: a marker by prior
        const Box bar{.05f, .06f, .3f, .1f};   // edge: HUD by prior
        Box boxes[2] = {mid, bar};
        s.update(boxes, 2, headAt(0).orientation);
        check(s.kind(0) == kindMarker && s.kind(1) == kindHud, "priors: the middle a marker, the edge HUD");
        // The head turns 15 degrees: the middle piece stays put (a generic prompt the game keeps on screen): HUD.
        // The bar shrinks from its right end (health lost) but its left stays: still HUD.
        Box b2[2] = {mid, {.05f, .06f, .2f, .1f}};
        s.update(b2, 2, headAt(15).orientation);
        check(s.kind(0) == kindHud && s.kind(1) == kindHud,
              "a piece still while the head turns is HUD; a shrinking bar too");
        check(s.hudVotes() == 2 && s.markerVotes() == 0, "two HUD votes");

        Split m;
        Box w[1] = {{.1f, .4f, .14f, .44f}}; // an edge waypoint: HUD by prior
        m.update(w, 1, headAt(0).orientation);
        check(m.kind(0) == kindHud, "an edge waypoint starts as HUD (its prior)");
        Box w2[1] = {{.15f, .4f, .19f, .44f}};
        m.update(w2, 1, headAt(-8).orientation);
        check(m.kind(0) == kindHud, "one move: not yet (hysteresis)");
        Box w3[1] = {{.2f, .41f, .24f, .45f}};
        m.update(w3, 1, headAt(-8).orientation); // the hero travelled, the head still: it moved again
        check(m.kind(0) == kindMarker, "two moves: a world marker");
        Box w4[1] = {{.2f, .41f, .24f, .45f}};
        m.update(w4, 1, headAt(-8).orientation);
        check(m.kind(0) == kindMarker, "no turn, no move: no vote, it stays a marker");
        m.update(w4, 1, headAt(-16).orientation);
        check(m.kind(0) == kindMarker, "one still-under-a-turn vote does not undo two moves");

        Split f;
        Box p1[1] = {{.5f, .3f, .55f, .35f}};
        f.update(p1, 1, headAt(0).orientation);
        Box none[1];
        f.update(none, 0, headAt(0).orientation);
        check(f.count() == 0 && f.kind(0) == kindHud, "gone pieces are forgotten; an unknown index is HUD");
        Box far[1] = {{.9f, .9f, .95f, .95f}};
        f.update(far, 1, headAt(0).orientation);
        check(f.fresh() == 2 && f.kind(0) == kindHud, "a new piece takes its own prior");
        check(std::abs(turnedDegrees(headAt(0).orientation, headAt(20).orientation) - 20.f) < .05f,
              "turned degrees");
        check(movedBy({0, 0, .2f, .1f}, {0, 0, .4f, .1f}) < 1e-6f && movedBy({0, 0, .1f, .1f}, {.1f, 0, .2f, .1f}) > .09f,
              "a bar growing from its end is still; a slid piece moved");
    }
    std::printf("%d failure(s)\n", failures);
    return failures ? 1 : 0;
}
