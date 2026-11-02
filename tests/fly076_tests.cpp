// HERO VR 0.0.76 FLIGHT (include/spidy/fly076.hpp): the take-off, the steer, SUPERSONIC, the brakes, the body roll
// and the wall push-off, pure and frame by frame. Made by Agent Mulder.
#include "spidy/fly076.hpp"

#include <cmath>
#include <cstdio>
#include <limits>
#include <string>

using namespace spidy;
using namespace spidy::fly076;

namespace {
int failures = 0;
void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++failures;
    }
}
constexpr float dt = 1.f / 90;

void startTests() {
    Start s;
    int frames = 0;
    while (!s.update(dt, true, false, false, true) && frames < 200)
        ++frames;
    check(frames >= 34 && frames <= 37, "A held 0.4 s asks the take-off");
    for (int i = 0; i < 90; ++i)
        s.update(dt, true, true, false, true);
    check(s.descends() == 0, "the same A press never also comes down");
    s.update(dt, true, true, false, false);
    for (int i = 0; i < 40; ++i)
        s.update(dt, true, true, false, true);
    check(s.descends() == 1, "a new A held in flight comes down");
    Start arms;
    bool asked = false;
    for (int i = 0; i < 30; ++i)
        asked = arms.update(dt, true, false, true, false) || asked;
    check(asked, "both arms up take off");
    Start off;
    bool none = false;
    for (int i = 0; i < 90; ++i)
        none = off.update(dt, false, false, true, true) || none;
    check(!none, "not a flyer: nothing takes off");
    const Vec3 up{0, 1, 0};
    const Vec3 high[2] = {{-.1f, .3f, 0}, {.1f, .3f, 0}}, low[2] = {{-.3f, -.4f, -.2f}, {.3f, -.4f, -.2f}};
    const bool both[2] = {true, true}, one[2] = {true, false};
    check(armsUp(high, both, up) && !armsUp(low, both, up) && !armsUp(high, one, up), "arms straight up, both");
    const Vec3 ahead[2] = {{-.1f, -.2f, -.6f}, {.1f, -.2f, -.6f}};
    const Vec3 w = steerWay(ahead, both, up);
    check(w.z < -.9f, "both arms out ahead steer ahead");
    check(length(steerWay(low, both, up)) < .5f || steerWay(low, both, up).z < 0, "arms down never steer up");
}

void flightTests() {
    Flight f;
    In in;
    in.seconds = dt;
    in.speed = 2;
    in.top = 16;
    in.grounded = true;
    in.look = {0, 0, -1};
    in.enter = true;
    Out o = f.update(in);
    check(o.tookOff && f.flying() && o.velocity.y > 0, "takes off, rising");
    in.enter = false;
    in.grounded = false;
    in.airborne = true;
    for (int i = 0; i < 90 * 3; ++i)
        o = f.update((in.velocity = o.velocity, in));
    check(o.velocity.z < -14 && std::fabs(o.velocity.y) < 1.5f, "flies on by itself, continuous");
    in.want = {1, 0, 0};
    for (int i = 0; i < 90 * 4; ++i)
        o = f.update((in.velocity = o.velocity, in));
    check(o.velocity.x > 14, "the arms steer");
    const Vec3 before = normalized(o.velocity);
    in.boost = 2.5f;
    in.sonicTop = 40;
    in.want = {};
    float lowest = 1e9f;
    for (int i = 0; i < 90 * 3; ++i) {
        o = f.update((in.velocity = o.velocity, in));
        lowest = std::min(lowest, o.velocity.y);
    }
    check(o.supersonic && length(o.velocity) > 38 && dot(normalized(o.velocity), before) > .999f,
          "SUPERSONIC: faster along the same path");
    check(lowest > -.01f, "SUPERSONIC never lowers you");
    in.grounded = true;
    o = f.update((in.velocity = o.velocity, in));
    check(f.flying() && !o.landed, "SUPERSONIC never grounds you");
    in.grounded = false;
    in.boost = 1;
    in.brake = true;
    for (int i = 0; i < 90 * 3; ++i)
        o = f.update((in.velocity = o.velocity, in));
    check(f.flying() && length(o.velocity) < .5f, "B: the air brake stops, still flying");
    in.brake = false;
    in.throttle = -1;
    for (int i = 0; i < 90 * 2; ++i)
        o = f.update((in.velocity = o.velocity, in));
    check(length(o.velocity) < .5f, "stick back: a hover");
    in.throttle = 0;
    in.drop = true;
    o = f.update((in.velocity = o.velocity, in));
    in.drop = false;
    for (int i = 0; i < 60; ++i)
        o = f.update((in.velocity = o.velocity, in));
    check(f.descending() && o.velocity.y < -3, "A held: comes down");
    in.grounded = true;
    for (int i = 0; i < 30 && f.flying(); ++i)
        o = f.update((in.velocity = o.velocity, in));
    check(!f.flying() && o.landed, "and lands");
    Flight g;
    In bad = in;
    bad.enter = true;
    bad.grounded = false;
    bad.velocity = {std::numeric_limits<float>::quiet_NaN(), 0, 0};
    bad.want = {std::numeric_limits<float>::infinity(), 0, 0};
    bad.throttle = std::numeric_limits<float>::quiet_NaN();
    for (int i = 0; i < 30; ++i)
        o = g.update(bad);
    check(finite(o.velocity), "NaN never spreads");
}

void hoverTests() {
    // HOVER FLIGHT (pt1616u): no stick, no arm - exactly still; the sticks move, strafe, climb; SUPERSONIC never lowers
    Flight f;
    In in;
    in.hover = true;
    in.seconds = dt;
    in.speed = 2;
    in.top = 16;
    in.grounded = true;
    in.look = {0, 0, -1};
    in.enter = true;
    Out o = f.update(in);
    check(o.tookOff && o.velocity.y > 0 && std::fabs(o.velocity.z) < .01f, "hover: takes off straight up");
    in.enter = false;
    in.grounded = false;
    in.airborne = true;
    for (int i = 0; i < 90 * 2; ++i)
        o = f.update((in.velocity = o.velocity, in));
    check(f.flying() && length(o.velocity) == 0 && std::string(o.how) == "HOVER", "hover: then hangs exactly still");
    in.velocity = {3, -2, 5}; // the game nudged him: it still comes to rest
    for (int i = 0; i < 90; ++i)
        o = f.update((in.velocity = o.velocity, in));
    check(length(o.velocity) == 0, "hover: any leftover speed dies out, no drift");
    in.move = {.7f, 0, -.7f};
    for (int i = 0; i < 90 * 2; ++i)
        o = f.update((in.velocity = o.velocity, in));
    check(o.velocity.x > 9 && o.velocity.z < -9 && std::fabs(o.velocity.y) < .01f, "hover: the stick moves and strafes");
    in.move = {};
    in.rise = 1;
    for (int i = 0; i < 90 * 2; ++i)
        o = f.update((in.velocity = o.velocity, in));
    check(o.velocity.y > 9 && std::fabs(o.velocity.x) < .5f, "hover: the climb stick climbs");
    in.rise = -1;
    in.wall = {0, 1, 0}; // the ground just under: it never pushes into it
    for (int i = 0; i < 90; ++i)
        o = f.update((in.velocity = o.velocity, in));
    check(o.velocity.y > -.01f, "hover: never into a wall");
    in.wall = {};
    in.rise = 0;
    in.want = {0, 0, -1};
    for (int i = 0; i < 90 * 2; ++i)
        o = f.update((in.velocity = o.velocity, in));
    check(o.velocity.z < -14, "hover: an arm pointed flies along it");
    in.want = {0, -.6f, -.8f};
    in.boost = 2.5f;
    in.sonicTop = 40;
    float lowest = 1e9f;
    for (int i = 0; i < 90 * 2; ++i) {
        o = f.update((in.velocity = o.velocity, in));
        lowest = std::min(lowest, o.velocity.y);
    }
    check(o.supersonic && o.velocity.z < -38 && lowest > -.01f, "hover SUPERSONIC: fast, never lower");
    in.grounded = true;
    o = f.update((in.velocity = o.velocity, in));
    check(f.flying() && !o.landed, "hover SUPERSONIC never grounds you");
    in.boost = 1;
    in.want = {};
    for (int i = 0; i < 90 * 3 && f.flying(); ++i)
        o = f.update((in.velocity = o.velocity, in));
    check(!f.flying() && o.landed, "hover: let go on the ground, he lands");
    // the word: hover and the climb stick ride the free bits; an older word has neither
    const Word w = unpack(pack(true, false, false, .5f, true, -1));
    check(w.on && w.hover && std::fabs(w.rise + 1) < .01f && std::fabs(w.throttle - .5f) < .01f, "the word carries hover");
    const Word old = unpack(pack(true, true, false, 0));
    check(old.on && old.brake && !old.hover && old.rise == 0, "an older word: no hover, no climb");
    check((pack(true, true, true, 1, true, 1) & 7u) == 0, "never fly_feel's bits 0-2");
}

void rollTests() {
    BodyRoll r;
    const float pi = 3.14159265f;
    float a = 0;
    for (int i = 0; i < 90 * 4; ++i)
        a = r.update(dt, true, 1, 180, false);
    check(std::fabs(a) < pi + 1e-3f, "the roll wraps, full turns");
    const float held = r.update(dt, true, 0, 180, false);
    for (int i = 0; i < 90; ++i)
        a = r.update(dt, true, 0, 180, false);
    check(std::fabs(a - held) < 1e-4f, "let go: it holds where it is, no snap back");
    for (int i = 0; i < 90 * 3; ++i)
        a = r.update(dt, true, 0, 180, true);
    check(std::fabs(a) < 1e-3f, "Y held: rights you");
    // 0.0.76 roll fix: let go away from level it holds (above); let go near level it IS level, exactly
    r.reset();
    for (int i = 0; i < 9; ++i) // 90 Hz, 180 deg/s: 18 deg
        a = r.update(dt, true, 1, 180, false);
    check(a > 15 * pi / 180 && a < 20 * pi / 180, "a little roll");
    const float little = a;
    for (int i = 0; i < 90; ++i)
        a = r.update(dt, true, 0, 180, false);
    check(a == little, "let go at 18 deg: it holds (only within snapDegrees does it level)");
    for (int i = 0; i < 90 && r.angle() > 4 * pi / 180; ++i)
        a = r.update(dt, true, -.3f, 180, false); // back toward level, slowly
    a = r.update(dt, true, 0, 180, false);
    check(a == 0.f, "let go within snapDegrees of level: exactly 0");
    // landing: level at once, not at 180 deg/s
    for (int i = 0; i < 60; ++i)
        r.update(dt, true, 1, 180, false);
    check(std::fabs(r.angle()) > 1, "rolled well over");
    check(r.update(dt, false, 0, 180, false) == 0.f && !r.rolling(), "landed: exactly level the same frame");
}

bool near(Vec3 a, Vec3 b, float e = 1e-4f) {
    return std::fabs(a.x - b.x) < e && std::fabs(a.y - b.y) < e && std::fabs(a.z - b.z) < e;
}

void rollFrameTests() {
    const float pi = 3.14159265f;
    const Vec3 head{1, 2, 3}, chest{1, 1.7f, 3}; // the chest 30 cm under the head
    const Vec3 ahead{0, 0, -1};
    RollFrame f;
    // 90 deg: the chest swings out to the side, level with the head; the head never moves
    Quat q = f.update(pi / 2, {}, ahead);
    Vec3 c = RollFrame::aboutHead(head, chest, q);
    check(near(c - head, {.3f, 0, 0}) || near(c - head, {-.3f, 0, 0}), "90 deg: the chest beside the head");
    check(near(RollFrame::aboutHead(head, head, q), head), "90 deg: the head stays where it is");
    check(f.nearClip() > 0, "90 deg: the near plane steps out");
    // 180 deg: upside down (facing the clouds), the chest ABOVE the head - out of the view looking up
    q = f.update(pi, {}, {0, .9f, -.1f}); // the look moved: the axis stays locked
    c = RollFrame::aboutHead(head, chest, q);
    check(near(c - head, {0, .3f, 0}), "180 deg: the chest above the head");
    check(near(f.axis(), ahead), "the axis is locked from the roll's start (no swimming with the head)");
    check(f.nearClip() == RollFrame::clipMetres, "180 deg: the near plane out");
    // quaternions: past 90 deg nothing flips - a smooth sweep keeps every step small
    Vec3 last = chest;
    float most = 0;
    for (int i = 0; i <= 360; ++i) {
        const Vec3 now = RollFrame::aboutHead(head, chest, f.update(i * pi / 180, {}, ahead));
        most = std::max(most, length(now - last));
        last = now;
    }
    check(most < .3f * pi / 180 * 1.01f, "0-360 deg: one smooth circle, no gimbal jump");
    // 360 deg: home again exactly as at 0
    q = f.update(2 * pi, {}, ahead);
    check(near(RollFrame::aboutHead(head, chest, q), chest), "360 deg: the chest back under the head");
    check(near(q.rotate({0, 1, 0}), {0, 1, 0}), "360 deg: up is up");
    // hysteresis: the near plane comes back only under clipOffDegrees
    f.update(pi, {}, ahead);
    f.update(70 * pi / 180, {}, ahead);
    check(f.nearClip() > 0, "70 deg on the way back: still out");
    f.update(50 * pi / 180, {}, ahead);
    check(f.nearClip() == 0, "50 deg: NEAR CLIP's own again");
    // the return to 0: a clean identity, the axis cleared, nothing left over
    q = f.update(0, {}, ahead);
    check(q.x == 0 && q.y == 0 && q.z == 0 && q.w == 1, "0: the identity, exactly");
    check(!f.active() && f.nearClip() == 0 && near(RollFrame::aboutHead(head, chest, f.turn()), chest),
          "0: axis cleared, near plane back, the body under the head");
    // a new roll after 0 takes the look then; fast flight takes the flight's way
    f.update(.5f, {}, {1, 0, 0});
    check(near(f.axis(), {1, 0, 0}), "a new roll: the look at its start");
    f.update(.5f, {0, 0, 20}, {1, 0, 0});
    check(near(f.axis(), {0, 0, 1}), "fast: about the flight's way");
    f.update(.5f, {0, 0, 2}, {1, 0, 0});
    check(near(f.axis(), {0, 0, 1}), "slowing: the last way kept");
    // BodyRoll + RollFrame: landing clears both the same frame
    BodyRoll r;
    for (int i = 0; i < 90; ++i)
        f.update(r.update(dt, true, 1, 180, false), {}, ahead);
    check(f.active(), "rolled");
    q = f.update(r.update(dt, false, 0, 180, false), {}, ahead);
    check(q.w == 1 && !f.active() && f.nearClip() == 0, "landed: identity the same frame");
    // the right stick: roll or climb, not both
    check(climbStick(.5f, .32f) == 0 && rollStick(.5f, .32f) == .5f, "rolling: no climb");
    check(climbStick(.1f, .8f) == .8f && rollStick(.1f, .8f) == 0, "climbing: no roll");
}

void pushOffTests() {
    PushOff p;
    float right = 0, forward = 0;
    check(!p.update(dt, {0, 0, -1}, right, forward), "no kick, no push");
    p.kicked({0, 0, 1}); // a wall ahead (its normal back toward the player)
    check(p.update(dt, {0, 0, -1}, right, forward) && forward < -.5f, "after a kick: away from the wall");
    for (int i = 0; i < 90; ++i)
        p.update(dt, {0, 0, -1}, right, forward);
    check(!p.pushing(), "only a moment");
}

void aimTests() {
    // 0.0.76 AIM FLIGHT (test 801ce57): look down and push forward - a dive; look up - a climb; strafe stays level
    const Vec3 down45 = normalized(Vec3{0, -1, -1}), up45 = normalized(Vec3{0, 1, -1});
    const Vec3 dive = aimedMove({0, 0, -1}, down45);
    check(dive.y < -.6f && dive.z < -.6f, "aim: forward while looking down dives forward and down");
    const Vec3 climb = aimedMove({0, 0, -1}, up45);
    check(climb.y > .6f && climb.z < -.6f, "aim: forward while looking up climbs");
    const Vec3 strafe = aimedMove({1, 0, 0}, down45);
    check(std::fabs(strafe.y) < 1e-4f && strafe.x > .99f, "aim: the strafe stays level");
    const Vec3 back = aimedMove({0, 0, 1}, down45);
    check(back.y > .6f && back.z > .6f, "aim: back while looking down backs up and away");
    check(length(aimedMove({0, 0, -1}, {0, -1, 0})) > .99f, "aim: a look straight down keeps the stick's way");
    check(length(aimedMove({0, 0, -1}, {NAN, 0, 0})) > .99f, "aim: a NaN look keeps the level stick");
    Flight f;
    In in;
    in.hover = true;
    in.aimPitch = true;
    in.seconds = dt;
    in.speed = 2;
    in.top = 16;
    in.grounded = true;
    in.look = {0, 0, -1};
    in.enter = true;
    Out o = f.update(in);
    in.enter = false;
    in.grounded = false;
    in.airborne = true;
    for (int i = 0; i < 90 * 2; ++i)
        o = f.update((in.velocity = o.velocity, in));
    check(length(o.velocity) == 0, "aim: nothing held, still");
    in.look = down45;
    in.move = {0, 0, -1};
    for (int i = 0; i < 90 * 2; ++i)
        o = f.update((in.velocity = o.velocity, in));
    check(o.velocity.y < -8 && o.velocity.z < -8, "aim: moving and looking down dives - no hover needed");
    in.look = up45;
    for (int i = 0; i < 90 * 2; ++i)
        o = f.update((in.velocity = o.velocity, in));
    check(o.velocity.y > 8 && o.velocity.z < -8, "aim: looking up climbs");
    in.look = {0, 0, -1};
    in.rise = -1;
    for (int i = 0; i < 90 * 2; ++i)
        o = f.update((in.velocity = o.velocity, in));
    check(o.velocity.y < -3 && o.velocity.z < -3, "aim: the climb stick still sinks while moving");
    in.rise = 0;
    in.look = down45;
    in.boost = 3;
    for (int i = 0; i < 90 * 2; ++i)
        o = f.update((in.velocity = o.velocity, in));
    check(o.supersonic && o.velocity.y < -10, "aim: SUPERSONIC follows the aim down in the air");
    in.grounded = true;
    o = f.update((in.velocity = o.velocity, in));
    check(o.velocity.y >= 0, "aim: SUPERSONIC never into the ground");
    in.boost = 0;
    in.grounded = false;
    // the old way without aimPitch: the stick stays level
    Flight g;
    In lv = in;
    lv.aimPitch = false;
    lv.enter = true;
    lv.grounded = false;
    lv.velocity = {};
    o = g.update(lv);
    lv.enter = false;
    for (int i = 0; i < 90 * 2; ++i)
        o = g.update((lv.velocity = o.velocity, lv));
    check(std::fabs(o.velocity.y) < .5f, "no aimPitch: the stick stays level (as before)");
}

void levelViewTests() {
    // 0.0.76 LEVEL VIEW: COMFORT keeps the view level, FULL rolls it, OFF rolls nothing
    check(levelView(1) && !levelView(2) && levelView(0) && levelView(7), "level view: COMFORT and unknown, not FULL");
    check(!rolls(0) && rolls(1) && rolls(2), "OFF never rolls");
    Vec3 up{0, 1, 0}, feet{0, 0, 0};
    const Vec3 eye{0, 1.7f, 0};
    rollBody(Quat::around({0, 0, -1}, 3.14159265f), eye, up, feet);
    check(up.y < -.99f && std::fabs(feet.y - 3.4f) < 1e-3f, "rollBody: half a turn - the body over the head, up down");
    Vec3 up2{0, 1, 0}, feet2{0, 0, 0};
    rollBody(Quat{}, eye, up2, feet2);
    check(up2.y > .99f && length(feet2) < 1e-5f, "rollBody: no roll changes nothing");
}
} // namespace

int main() {
    startTests();
    flightTests();
    hoverTests();
    rollTests();
    rollFrameTests();
    aimTests();
    levelViewTests();
    pushOffTests();
    if (failures) {
        std::printf("%d fly076 checks failed\n", failures);
        return 1;
    }
    std::printf("fly076 tests passed\n");
    return 0;
}
