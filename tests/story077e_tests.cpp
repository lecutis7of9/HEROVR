// HERO VR 0.0.77 X IS THE GAME'S X (include/spidy/story077e.hpp), pure.
#include "spidy/story077e.hpp"

#include <cstdio>

using namespace spidy::story077e;

namespace {
int failures = 0;
void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++failures;
    }
}
constexpr float dt = 1.f / 90;
}  // namespace

int main() {
    // One tap: one press of about 0.07 s, then up.
    {
        XPulse p;
        check(!p.update(dt), "idle up");
        p.tap();
        int down = 0;
        for (int i = 0; i < 60; ++i) down += p.update(dt);
        check(down >= 6 && down <= 8, "one tap ~0.07 s");
        check(p.taps == 1 && p.sent == 1 && !p.busy(), "one tap counted");
    }
    // Mashing: 8 taps 0.12 s apart give 8 separate presses, each with a release between.
    {
        XPulse p;
        int edges = 0;
        bool was = false;
        for (int i = 0; i < 300; ++i) {
            if (i % 11 == 0 && i < 88) p.tap();
            const bool d = p.update(dt);
            if (d && !was) ++edges;
            was = d;
        }
        check(edges == 8 && p.sent == 8, "mash 8 presses");
    }
    // Taps faster than presses queue (at most 4) and still come out separate.
    {
        XPulse p;
        for (int i = 0; i < 3; ++i) p.tap();
        int edges = 0;
        bool was = false;
        for (int i = 0; i < 200; ++i) {
            const bool d = p.update(dt);
            if (d && !was) ++edges;
            was = d;
        }
        check(edges == 3, "queued 3 separate");
        XPulse q;
        for (int i = 0; i < 9; ++i) q.tap();
        for (int i = 0; i < 400; ++i) q.update(dt);
        check(q.sent == XPulse::queueMax && q.taps == 9, "queue capped");
    }
    // The menu button tapped: one 0.1 s start on release, no View.
    {
        MenuHold m;
        bool start = false, view = false;
        for (int i = 0; i < 20; ++i) {
            auto o = m.update(dt, true, true);
            start |= o.start, view |= o.view;
        }
        check(!start && !view, "no start while held");
        int startFrames = 0;
        for (int i = 0; i < 40; ++i) {
            auto o = m.update(dt, true, false);
            startFrames += o.start, view |= o.view;
        }
        check(startFrames >= 8 && startFrames <= 10 && !view, "tap pauses on release");
        check(m.pauses == 1 && m.views == 0, "tap counted");
    }
    // Held 0.5 s: the View tap once with a buzz, never a start, however long it is held.
    {
        MenuHold m;
        int viewFrames = 0, buzz = 0, start = 0;
        for (int i = 0; i < 270; ++i) {
            auto o = m.update(dt, true, true);
            viewFrames += o.view, buzz += o.buzz, start += o.start;
            if (i == 40) check(viewFrames == 0, "no View before 0.5 s");
        }
        for (int i = 0; i < 40; ++i) {
            auto o = m.update(dt, true, false);
            viewFrames += o.view, start += o.start;
        }
        check(viewFrames >= 8 && viewFrames <= 10, "View 0.1 s");
        check(buzz == 1 && start == 0, "one buzz, never pauses");
        check(m.views == 1 && m.pauses == 0, "hold counted");
    }
    // A press already down when VR play starts waits for its release; off sends nothing.
    {
        MenuHold m;
        for (int i = 0; i < 10; ++i) m.update(dt, false, true);
        bool any = false;
        for (int i = 0; i < 100; ++i) {
            auto o = m.update(dt, true, true);
            any |= o.start || o.view;
        }
        for (int i = 0; i < 20; ++i) {
            auto o = m.update(dt, true, false);
            any |= o.start || o.view;
        }
        check(!any && m.pauses == 0 && m.views == 0, "held-in press ignored");
        for (int i = 0; i < 5; ++i) m.update(dt, true, true);
        bool start = false;
        for (int i = 0; i < 5; ++i) start |= m.update(dt, true, false).start;
        check(start && m.pauses == 1, "next tap pauses");
    }
    if (failures) {
        std::printf("story077e: %d failures\n", failures);
        return 1;
    }
    std::printf("story077e: all passed\n");
    return 0;
}
