// HERO VR 0.0.77 STORY INPUTS (include/spidy/story077.hpp): the hero switch's presses, its head gesture
// and the two-hand web throw, pure and frame by frame. Made by Agent Mulder.
#include "spidy/story077.hpp"

#include <cstdio>

using namespace spidy;
using namespace spidy::story077;

namespace {
int failures = 0;
void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++failures;
    }
}
constexpr float dt = 1.f / 90;

struct Sent {
    float back{}, dpad{}, x{}, b{};
};
Sent run(SwapMacro& m, float seconds, uint64_t markerAt = 0, float changeAt = 1e9f) {
    Sent s;
    for (float t = 0; t < seconds && m.running(); t += dt) {
        const uint16_t out = m.update(dt, true, t >= changeAt ? markerAt + 1 : markerAt);
        s.back += (out & pad::back) ? dt : 0;
        s.dpad += (out & (pad::dpadLeft | pad::dpadRight)) ? dt : 0;
        s.x += (out & pad::x) ? dt : 0;
        s.b += (out & pad::b) ? dt : 0;
    }
    return s;
}

void macroTests() {
    SwapMacro m;
    check(!m.start(routeOff, 0), "OFF started a switch");
    check(m.start(viewHold, 5), "HOLD VIEW did not start");
    check(!m.start(viewHold, 5), "a second start while one runs");
    const Sent s = run(m, 20, 5);
    check(s.back > .95f && s.back < 1.05f, "View not held about 1 s");
    check(s.x > 2.1f && s.x < 2.3f, "X not held about 2.2 s");
    check(s.b > .1f && s.b < .2f, "no change did not close the app with B");
    check(!m.running() && m.last() == SwapMacro::closedNoChange && m.closedCount == 1, "not closed, no change");
    // The hero replaced while X is held: nothing more is sent, no B.
    check(m.start(viewHold, 9), "did not start again");
    const Sent t = run(m, 20, 9, 2.5f);
    check(t.b == 0 && t.x < 1, "B or more X sent after the hero went");
    check(m.last() == SwapMacro::switched && m.switchedCount == 1, "the switch not counted");
    // The D-pad shortcut: a tap, never View.
    check(m.start(dpadLeft, 0), "D-PAD LEFT did not start");
    const Sent d = run(m, 20);
    check(d.back == 0 && d.dpad > .1f && d.dpad < .2f, "the shortcut was not a short D-pad tap");
    // No session: cancelled at once.
    m.start(viewHold, 0);
    m.update(dt, true, 0);
    check(m.update(dt, false, 0) == 0 && !m.running() && m.last() == SwapMacro::cancelled, "not cancelled");
    m.start(viewHold, 0);
    m.update(dt, true, 0);
    m.cancel();
    check(!m.running() && m.last() == SwapMacro::cancelled, "a pause did not cancel it");
    // A change before X (opening, a map or a respawn) does not count as the switch.
    m.start(viewHold, 0);
    const Sent e = run(m, .5f, 0, 0);
    check(e.back > 0 && m.running(), "a change while opening ended it");
    run(m, 20, 0, 0);
}

void headTests() {
    HeadTouch h;
    const Vec3 head{0, 1.6f, 0};
    Vec3 hands[2]{{0, 1.75f, .02f}, {.3f, 1.0f, .2f}};
    const bool tracked[2]{true, true};
    const float none[2]{}, trig[2]{.8f, 0};
    int frames = 0, cues = 0;
    bool fired = false;
    while (!fired && frames < 400) {
        const auto o = h.update(dt, true, head, hands, tracked, none, none);
        fired = o.fired;
        cues += o.cue;
        ++frames;
    }
    check(fired && frames >= 106 && frames <= 110 && cues == 1, "1.2 s on the head did not fire once, cued once");
    for (int i = 0; i < 400; ++i)
        check(!h.update(dt, true, head, hands, tracked, none, none).fired || i > 360, "fired again in the cooldown");
    HeadTouch t;
    for (int i = 0; i < 300; ++i)
        check(!t.update(dt, true, head, hands, tracked, trig, none).fired, "fired with the trigger pulled");
    Vec3 both[2]{{0, 1.75f, .02f}, {.05f, 1.78f, 0}};
    HeadTouch b;
    for (int i = 0; i < 300; ++i)
        check(!b.update(dt, true, head, both, tracked, none, none).fired, "both hands overhead fired");
    HeadTouch p;
    for (int i = 0; i < 300; ++i)
        check(!p.update(dt, false, head, hands, tracked, none, none).fired, "fired outside play");
    HeadTouch moving;
    for (int i = 0; i < 300; ++i) {
        Vec3 m[2]{{.1f * ((i / 9) % 2 ? 1.f : -1.f), 1.75f, 0}, hands[1]};
        check(!moving.update(dt, true, head, m, tracked, none, none).fired, "a moving hand fired");
    }
}

int yankRun(TwoHandYank& y, float from, float to, float speed, bool web, float leftDelay = 0, float rightTrig = 0) {
    const Vec3 head{0, 1.6f, 0}, fwd{0, -.3f, 1};
    const bool tracked[2]{true, true};
    const float trig[2]{0, rightTrig};
    int held = 0;
    for (int i = 0; i < 45; ++i) { // out in front first, held there
        Vec3 h[2]{{-.2f, 1.3f, from}, {.2f, 1.3f, from}};
        held += y.update(dt, true, head, fwd, h, tracked, trig, web);
    }
    float a = from, b = from, t = 0;
    for (int i = 0; i < 200; ++i, t += dt) {
        if (t >= leftDelay)
            a = std::max(to, a - speed * dt);
        b = std::max(to, b - speed * dt);
        Vec3 h[2]{{-.2f, 1.3f, a}, {.2f, 1.3f, b}};
        held += y.update(dt, true, head, fwd, h, tracked, trig, web);
    }
    return held;
}

void yankTests() {
    TwoHandYank y;
    const int held = yankRun(y, .45f, .12f, 2.2f, false);
    check(y.yanks == 1 && held >= 88 && held <= 92, "a two-hand yank did not hold LB + RB about 1 s");
    TwoHandYank slow;
    check(yankRun(slow, .45f, .12f, .5f, false) == 0, "a slow pull back threw");
    TwoHandYank web;
    check(yankRun(web, .45f, .12f, 2.2f, true) == 0, "a yank on an attached web threw (the swing's)");
    TwoHandYank near;
    check(yankRun(near, .2f, .05f, 2.2f, false) == 0, "hands never out in front threw");
    TwoHandYank apart;
    check(yankRun(apart, .45f, .12f, 2.2f, false, .6f) == 0, "hands yanked 0.6 s apart threw");
    // A double punch: out and straight back, no hold out there.
    TwoHandYank punch;
    {
        const Vec3 head{0, 1.6f, 0}, fwd{0, 0, 1};
        const bool tracked[2]{true, true};
        const float none[2]{};
        int held = 0;
        for (int k = 0; k < 4; ++k)
            for (int i = 0; i < 30; ++i) {
                const float z = i < 15 ? .1f + i * .035f : .1f + (30 - i) * .035f; // ~3 m/s out and back
                Vec3 h[2]{{-.2f, 1.3f, z}, {.2f, 1.3f, z}};
                held += punch.update(dt, true, head, fwd, h, tracked, none, false);
            }
        check(held == 0, "a double punch threw");
    }
    TwoHandYank trig;
    check(yankRun(trig, .45f, .12f, 2.2f, false, 0, .9f) == 0, "a yank with a trigger pulled threw");
}
} // namespace

int main() {
    macroTests();
    headTests();
    yankTests();
    std::printf("%s (%d failures)\n", failures ? "FAILED" : "story077 ok", failures);
    return failures ? 1 : 0;
}
