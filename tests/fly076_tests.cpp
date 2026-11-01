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
} // namespace

int main() {
    startTests();
    flightTests();
    hoverTests();
    rollTests();
    pushOffTests();
    if (failures) {
        std::printf("%d fly076 checks failed\n", failures);
        return 1;
    }
    std::printf("fly076 tests passed\n");
    return 0;
}
