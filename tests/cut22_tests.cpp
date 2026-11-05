// HERO VR 0.0.78 cut22 PROMPT WORLD tests (include/spidy/prompt_world.hpp): the game's UI pieces stand still in the
// room (the board holds while the head looks around, glides back after a held turn), an edge prompt is pulled into a
// Quest's view at a readable size, markers are told from HUD pieces by how they move as the head turns, and each card
// is projected through the eye's room pose (cut off at the view's edge, never behind the eye).
#include "spidy/prompt_world.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>

using namespace spidy;
using namespace spidy::prompt_world;

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
// Degrees right of / above the head's forward a room point stands.
void anglesFrom(const Pose& head, Vec3 p, float& rightDeg, float& upDeg) {
    const Vec3 v = head.orientation.conjugate().rotate(p - head.position);
    rightDeg = std::atan2(v.x, -v.z) / deg;
    upDeg = std::atan2(v.y, std::sqrt(v.x * v.x + v.z * v.z)) / deg;
}
Mat4 vpOf(const Pose& eye, float half) {
    return multiply(projection(-half, half, -half, half, .03f, 200.f), viewMatrix(eye));
}
} // namespace

int main() {
    // The board is put in front of the head and holds while it looks around.
    {
        Board b;
        check(!b.placed(), "no board before a HUD card");
        b.update(headAt(0), 1000, .011f, true);
        check(b.placed() && b.puts() == 1, "the first HUD card puts the board");
        const Vec3 before = b.point(0, 0);
        float r{}, u{};
        anglesFrom(headAt(0), before, r, u);
        check(std::abs(r) < .5f && u < -4.f && u > -10.f, "the board's middle is ahead, a little under the eyes");
        uint64_t ms = 1000;
        for (int i = 0; i < 300; ++i) // 3.3 s looking 35 degrees right and 20 down: within turnDegrees
            b.update(headAt(-35, -20), ms += 11, .011f, true);
        const Vec3 after = b.point(0, 0);
        check(length(after - before) < 1e-4f && !b.gliding(), "a look within 50 degrees leaves the board where it is");
        for (int i = 0; i < 40; ++i) // 0.44 s turned 70 degrees: a glance
            b.update(headAt(-70), ms += 11, .011f, true);
        check(length(b.point(0, 0) - before) < 1e-4f, "a glance past 50 degrees under 0.8 s leaves it too");
        for (int i = 0; i < 400; ++i) // held: it glides back in front, then holds
            b.update(headAt(-70), ms += 11, .011f, true);
        anglesFrom(headAt(-70), b.point(0, 0), r, u);
        check(b.glides() == 1 && std::abs(r) < 2.5f && !b.gliding(), "a turn held past 0.8 s glides it back in front");
        const Vec3 settled = b.point(0, 0);
        for (int i = 0; i < 100; ++i)
            b.update(headAt(-60, 0, {.1f, 1.6f, -.1f}), ms += 11, .011f, true);
        check(length(b.point(0, 0) - settled) < 1e-4f, "a small step and turn leave it settled");
        b.update(headAt(0), ms += 5000, .011f, false);
        b.update(headAt(0), ms += 11, .011f, true);
        anglesFrom(headAt(0), b.point(0, 0), r, u);
        check(b.puts() == 2 && std::abs(r) < .5f, "after 3 s with no HUD card the next one puts it in front again");
        for (int i = 0; i < 200; ++i)
            b.update(headAt(0, 0, {1.f, 1.6f, 0}), ms += 11, .011f, true);
        check(b.glides() == 2, "a step of a metre held brings it along");
    }
    // The game's Y prompt at the bottom-left edge of its 1280x720 view: pulled well into view, readable.
    {
        Board b;
        b.update(headAt(0), 1, .011f, true);
        prompt_layer::Region y{40, 640, 104, 688, 12};
        float tx{}, ty{};
        check(pieceTangent(y, 1280, 720, tx, ty), "the prompt's place in the game's view");
        float r{}, u{};
        anglesFrom(headAt(0), b.point(tx, ty), r, u);
        // A Quest 3's eye sees about 40 degrees in and 45 down; cut21 had it at about -32, -20 and shrunk.
        check(r < -10.f && r > -25.f, "the bottom-left prompt within 25 degrees left");
        check(u < -8.f && u > -22.f, "and within 22 degrees down");
        float tw{}, th{}, bw{}, bh{};
        pieceSize(y, 1280, 720, tw, th);
        boardSize(tw, th, bw, bh);
        check(bh >= minCardTan - 1e-5f && bw > tw && std::abs(bw / bh - tw / th) < 1e-3f,
              "grown to at least minCardTan high, its shape kept");
        boardSize(1.f, .5f, bw, bh);
        check(bw <= maxCardTan[0] + 1e-5f && bh <= maxCardTan[1] + 1e-5f, "a large piece capped");
        boardSize(0, .1f, bw, bh);
        check(bw == 0 && bh == 0, "an empty piece no card");
        float cx{}, cy{};
        prompt_layer::Region mid{624, 344, 656, 376, 4};
        pieceTangent(mid, 1280, 720, cx, cy);
        check(std::abs(cx) < 1e-4f && std::abs(cy) < 1e-4f, "the view's middle is the board's middle");
        check(!pieceTangent(prompt_layer::Region{10, 10, 5, 20, 1}, 1280, 720, tx, ty), "a bad piece none");
    }
    // A world marker moves across the view against the head's turn; a HUD piece stays.
    {
        float px{}, py{};
        check(markerMoved(Quat{}, Quat::yaw(-10 * deg), 0, 0, px, py) && std::abs(px + std::tan(10 * deg)) < 1e-3f &&
                  std::abs(py) < 1e-4f,
              "a head turn of 10 degrees right moves a marker 10 degrees left");
        check(!markerMoved(Quat{}, Quat::yaw(180 * deg), 0, 0, px, py), "behind after a half turn: none");
        Sorter s;
        float hudX[2] = {-.5f, .1f}, hudY[2] = {-.3f, 0};
        s.update(hudX, hudY, 2, Quat{});
        check(!s.marker(0) && !s.marker(1), "new pieces are HUD pieces");
        // Piece 0 stays (HUD); piece 1 moves as a marker would over head turns of 5 degrees a grid.
        float yaw = 0;
        float mx = .1f, my = 0;
        for (int g = 0; g < 4; ++g) {
            const Quat was = Quat::yaw(yaw * deg);
            yaw -= 5;
            const Quat now = Quat::yaw(yaw * deg);
            markerMoved(was, now, mx, my, mx, my);
            float xs[2] = {-.5f, mx}, ys[2] = {-.3f, my};
            s.update(xs, ys, 2, now);
        }
        check(!s.marker(0), "a piece that stays while the head turns stays a HUD piece");
        check(s.marker(1), "a piece that moves as the turn says is a marker");
        // Head still: no votes either way.
        float xs[2] = {-.5f, mx}, ys[2] = {-.3f, my};
        s.update(xs, ys, 2, Quat::yaw(yaw * deg));
        check(s.marker(1) && !s.marker(0), "a still head changes nothing");
        // The marker stops moving with turns (the game moved it to its HUD): back to the board.
        for (int g = 0; g < 6; ++g) {
            yaw -= 5;
            s.update(xs, ys, 2, Quat::yaw(yaw * deg));
        }
        check(!s.marker(1), "a marker that stays through turns is a HUD piece again");
        const Pose at = headAt(0);
        const Vec3 m = markerPoint(at, 0, 0);
        check(std::abs(length(m - at.position) - markerDistance) < 1e-3f && m.z < 0, "a marker stands markerDistance out");
    }
    // The card through the eye's room pose: centred ahead, still in the room as the head turns, cut at the edge.
    {
        const float half = 45 * deg, t = std::tan(half);
        const float full[4] = {0, 0, 1, 1};
        float rect[4], crop[4];
        const Pose eye = headAt(0);
        const Vec3 ahead = eye.position + Vec3{0, 0, -1.4f};
        check(cardRect(vpOf(eye, half), ahead, .2f, 2.f, -t, t, t, -t, 1000, 1000, full, rect, crop) &&
                  std::abs((rect[0] + rect[2]) * .5f - 500) < 1 && std::abs((rect[1] + rect[3]) * .5f - 500) < 1 &&
                  std::abs(rect[2] - rect[0] - 2 * (.1f / 1.4f) * 500) < 1,
              "a card 1.4 m ahead at the view's middle, its size by its distance");
        // The head turns 20 degrees left: the room card moves right in the view by that turn.
        const Pose turned = headAt(20);
        check(cardRect(vpOf(turned, half), ahead, .2f, 2.f, -t, t, t, -t, 1000, 1000, full, rect, crop) &&
                  std::abs((rect[0] + rect[2]) * .5f - (500 + std::tan(20 * deg) * 500)) < 2,
              "turning the head leaves the card where it is in the room");
        check(!cardRect(vpOf(eye, half), eye.position + Vec3{0, 0, 1.4f}, .2f, 2.f, -t, t, t, -t, 1000, 1000, full, rect,
                        crop),
              "behind the eye: none");
        check(!cardRect(vpOf(eye, half), eye.position + Vec3{5, 0, -1}, .2f, 2.f, -t, t, t, -t, 1000, 1000, full, rect,
                        crop),
              "off the view: none");
        const Vec3 edge = eye.position + Vec3{1.4f, 0, -1.4f}; // its middle on the right edge
        check(cardRect(vpOf(eye, half), edge, .2f, 1.f, -t, t, t, -t, 1000, 1000, full, rect, crop) &&
                  rect[2] <= 1000 && std::abs(crop[2] - .5f) < .02f && crop[0] == 0,
              "a card on the edge keeps its picture's matching part");
        const float part[4] = {.1f, .2f, .3f, .4f};
        check(cardRect(vpOf(eye, half), ahead, .2f, 2.f, -t, t, t, -t, 1000, 1000, part, rect, crop) &&
                  std::abs(crop[0] - .1f) < 1e-5f && std::abs(crop[3] - .4f) < 1e-5f,
              "the piece's own crop, whole when in view");
        check(!cardRect(vpOf(eye, half), ahead, .001f, 2.f, -t, t, t, -t, 1000, 1000, full, rect, crop),
              "under 8 pixels: none");
    }
    check(std::abs(wrapRadians(7.f) - (7.f - 6.2831853f)) < 1e-4f, "angles wrap");
    check(std::abs(yawOf(Quat::yaw(.5f)) - .5f) < 1e-4f, "the heading of a turn");
    std::printf("%d failure(s)\n", failures);
    return failures ? 1 : 0;
}
