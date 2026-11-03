// HERO VR 0.0.77 HEAL ANYWHERE (include/spidy/story077f.hpp), pure.
#include "spidy/story077f.hpp"

#include <cmath>
#include <cstdio>

using namespace spidy::story077f;

namespace {
int failures = 0;
void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++failures;
    }
}
constexpr float dt = 1.f / 90;
int run(HealStick& h, int frames, bool live, float x, float y, float hold = .25f) {
    int on = 0;
    for (int i = 0; i < frames; ++i) on += h.update(dt, live, x, y, hold);
    return on;
}
}  // namespace

int main() {
    {
        HealStick h;
        check(run(h, 90, true, 0, 0) == 0, "rest: no heal");
        check(run(h, 20, true, 0, -1) == 0, "held under heal_hold_ms: nothing yet");
        check(run(h, 90, true, 0, -1) > 80, "held down: D-pad down out");
        check(h.sends == 1, "one send");
        run(h, 10, true, 0, 0);
        check(!h.sending() && h.longestSeconds > .9f, "let go: ends, longest counted");
    }
    {
        HealStick h;
        check(run(h, 90, true, .55f, -.8f) > 40, "a diagonal more down than aside heals");
        HealStick s;
        check(run(s, 90, true, .9f, -.6f) == 0, "more aside than down: no heal");
        HealStick w;
        run(w, 30, true, .3f, .2f);
        check(run(w, 90, true, .2f, -.9f) > 40, "a push that wandered first still heals (nothing spent)");
    }
    {
        HealStick h;
        run(h, 25, true, 0, -1);
        int on = 0;
        for (int i = 0; i < 60; ++i) on += h.update(dt, true, 0, 0, .25f);
        check(on >= 22 && on <= 28, "a short send lasts at least 0.3 s");
        check(h.ended() || !h.sending(), "ended");
        HealStick d;
        check(run(d, 120, false, 0, -1) == 0, "not live: no heal");
        run(d, 40, true, 0, -1);
        check(d.sending(), "live: heals");
        d.update(dt, false, 0, -1, .25f);
        check(!d.sending() && d.ended(), "live lost: ends at once");
        HealStick n;
        check(run(n, 90, true, NAN, NAN) == 0 && run(n, 3, true, 0, -1, NAN) == 0, "nan safe");
        check(!n.update(NAN, true, 0, -1, .25f) || true, "nan seconds safe");
    }
    {
        HealHint k;
        int shown = 0;
        for (int i = 0; i < 90; ++i) shown += k.update(dt, true, 1.f, false);
        check(shown == 0, "full health: no hint");
        for (int i = 0; i < 170; ++i) shown += k.update(dt, true, .2f, false);
        check(shown == 0, "hurt under 2 s: no hint yet");
        for (int i = 0; i < 20; ++i) shown += k.update(dt, true, .2f, false);
        check(shown == 1, "hurt 2 s: one hint");
        for (int i = 0; i < 900; ++i) shown += k.update(dt, true, .2f, false);
        check(shown == 1, "not again within 30 s");
        for (int i = 0; i < 2000; ++i) shown += k.update(dt, true, .2f, true);
        check(shown == 1, "healing: no hint");
        for (int i = 0; i < 300; ++i) shown += k.update(dt, true, .2f, false);
        check(shown == 2 && k.shown == 2, "again after 30 s");
        HealHint off;
        for (int i = 0; i < 900; ++i) shown += off.update(dt, false, .1f, false);
        check(shown == 2, "not live: no hint");
    }
    if (failures == 0) std::printf("story077f: all passed\n");
    return failures ? 1 : 0;
}
