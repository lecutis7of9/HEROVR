// 0.0.82 WEBLINE MODES: WRISTS and TWO HAND state machines (include/spidy/webline_modes.hpp).
#include "spidy/webline_modes.hpp"
#include <cstdio>
#include <cstdlib>
#define CHECK(c) do { if(!(c)) { std::fprintf(stderr, "FAILED %s:%d %s\n", __FILE__, __LINE__, #c); std::exit(1); } } while(0)
using namespace spidy;
using namespace spidy::webline_modes;
static WristsInput wi(float gap, bool valid = true, bool free = true, bool enabled = true) {
    WristsInput i{};
    i.seconds = .1f; i.enabled = enabled; i.tracked = true; i.free = free; i.valid = valid; i.holdSeconds = .4f;
    i.left = {0, 1, 0}; i.right = {gap, 1, 0};
    return i;
}
int main() {
    // WRISTS: together 0.4 s fires once, with a ring that fills
    {
        Wrists w;
        auto o = w.update(wi(.05f));
        CHECK(o.active && !o.commit && o.progress > .2f && o.progress < .3f);
        o = w.update(wi(.05f)); CHECK(!o.commit && o.progress > .45f);
        o = w.update(wi(.05f)); CHECK(!o.commit);
        o = w.update(wi(.05f)); CHECK(o.commit && o.progress >= 1.f);
        o = w.update(wi(.05f)); CHECK(!o.commit && !o.active); // once per meeting
        o = w.update(wi(.5f));  CHECK(!o.commit && !o.cancelled); // parted: re-armed
        o = w.update(wi(.05f)); CHECK(o.active && !o.commit);
    }
    // separating cancels; a grip or trigger down (a swing) never starts it
    {
        Wrists w;
        w.update(wi(.05f)); w.update(wi(.05f));
        auto o = w.update(wi(.4f));
        CHECK(o.cancelled && o.why == WristsWhy::separated && !o.commit);
        Wrists g;
        for (int i = 0; i < 10; ++i) { o = g.update(wi(.02f, true, false)); CHECK(!o.commit && !o.active); }
        Wrists d;
        for (int i = 0; i < 10; ++i) { o = d.update(wi(.02f, true, true, false)); CHECK(!o.commit && !o.active); }
    }
    // start at 12 cm, keep to 17 cm; 13 cm does not start it
    {
        Wrists w;
        CHECK(!w.update(wi(.13f)).active);
        CHECK(w.update(wi(.11f)).active);
        CHECK(w.update(wi(.16f)).active);
        CHECK(w.update(wi(.18f)).cancelled);
    }
    // no valid surface: the ring fills, waits 1.5 s, cancels; a surface appearing in time fires
    {
        Wrists w;
        WristsOutput o;
        for (int i = 0; i < 4; ++i) o = w.update(wi(.05f, false));
        CHECK(o.ready && !o.commit && o.progress >= 1.f);
        o = w.update(wi(.05f, true)); CHECK(o.commit);
        Wrists n;
        bool cancelled = false;
        for (int i = 0; i < 30 && !cancelled; ++i) { o = n.update(wi(.05f, false)); cancelled = o.cancelled; CHECK(!o.commit); }
        CHECK(cancelled && o.why == WristsWhy::noSurface);
    }
    CHECK(holdSeconds(400) > .39f && holdSeconds(400) < .41f && holdSeconds(5) == holdSeconds(150) && holdSeconds(9999) == holdSeconds(2000));
    // TWO HAND: A then the other hand's B commits; the same hand twice re-places A; too close cancels; A lapses after 3 s
    {
        TwoHand t;
        TwoHandInput i{}; i.enabled = true; i.seconds = .1f;
        auto o = t.update(i); CHECK(!o.anchored && !o.commit);
        i.shot[0] = true; i.point[0] = {0, 5, -10};
        o = t.update(i); CHECK(o.anchored && o.placed && o.hand == 0 && !o.commit);
        i.shot[0] = false;
        o = t.update(i); CHECK(o.anchored && o.remaining < 3.f);
        i.shot[1] = true; i.point[1] = {8, 5, -10};
        o = t.update(i); CHECK(o.commit && o.a.x == 0 && o.b.x == 8 && !o.anchored);
        o = t.update(i); CHECK(o.anchored); // the shot flag is still set in this test frame: it places a fresh A, not a line
        t.reset();
        i = {}; i.enabled = true; i.seconds = .1f;
        i.shot[1] = true; i.point[1] = {0, 0, 0};
        t.update(i);
        i.shot[1] = true; i.point[1] = {3, 0, 0}; // the same hand again
        o = t.update(i); CHECK(!o.commit && o.placed && o.a.x == 3);
        i.shot[1] = false; i.shot[0] = true; i.point[0] = {4, 0, 0}; // only 1 m from A
        o = t.update(i); CHECK(o.cancelled && o.why == TwoHandWhy::tooClose);
        t.reset();
        i = {}; i.enabled = true; i.seconds = .1f;
        i.shot[0] = true; i.point[0] = {0, 0, 0};
        t.update(i);
        i.shot[0] = false;
        bool lapsed = false;
        for (int k = 0; k < 40 && !lapsed; ++k) { o = t.update(i); lapsed = o.cancelled; }
        CHECK(lapsed && o.why == TwoHandWhy::timeout);
        // switched off drops A
        i.shot[0] = true; t.update(i); i.shot[0] = false; i.enabled = false;
        o = t.update(i); CHECK(o.cancelled && o.why == TwoHandWhy::disabled && !t.anchored());
    }
    std::puts("webline_modes tests passed");
    return 0;
}
