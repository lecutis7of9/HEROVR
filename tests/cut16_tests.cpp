// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
// HERO VR 0.0.78 cut16: cut16::BossPick and cut16::BossGate on the public 0.0.77 and cut8 Sandman timelines.
#include "spidy/cut16.hpp"

#include <cstdio>

using namespace spidy::cut16;
static int failed = 0;
#define CHECK(x)                                                                                                       \
    do {                                                                                                               \
        if (!(x)) {                                                                                                    \
            std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #x);                                                    \
            ++failed;                                                                                                  \
        }                                                                                                              \
    } while (0)

int main() {
    // BossPick: two SandmanBot records (public 0.0.77): A 250/250 never hit, B the one the throws hit.
    {
        BossPick p;
        const uint64_t A = 0x265176412c0ull, B = 0x265179cdcc0ull;
        BossPick::In in[2] = {{A, 90.f, true, 250.f, 250.f}, {B, 110.f, true, 250.f, 250.f}};
        CHECK(p.pick(in, 2, 0.0) == 0); // nobody hit yet: the nearest
        in[1].hp = 245.5f;              // B takes a debris hit
        CHECK(p.pick(in, 2, 2.0) == 1);
        in[0].metres = 80.f; // A nearer still: B, the hit one, stays
        CHECK(p.pick(in, 2, 4.0) == 1);
        CHECK(p.pick(in, 2, 21.0) == 1);  // 19 s since his hit
        CHECK(p.pick(in, 2, 23.0) == 0);  // 21 s: no recent hit, the nearest again
        in[1].hp = 233.3f;                // hit again
        CHECK(p.pick(in, 2, 24.0) == 1);
        in[1].hp = 0.f; // dead: never over a living one
        CHECK(p.pick(in, 2, 25.0) == 0);
        BossPick::In one[1] = {{B, 50.f, true, 0.f, 250.f}};
        CHECK(p.pick(one, 1, 26.0) == 0); // only a dead one: still him (the old read; game_xr skips hp <= 0)
        CHECK(p.pick(in, 0, 27.0) == -1);
        BossPick::In noHp[2] = {{A, 30.f, false}, {B, 20.f, false}};
        CHECK(p.pick(noHp, 2, 28.0) == 1); // no HP read: the nearest
    }
    // BossGate on public 0.0.77 (throws 17-30): 239.5 -> 235.0 -> 233.3, then nothing.
    {
        BossGate g;
        CHECK(!g.thrown(true, 239.5f, 235.0f, 250.f));        // 17: a hit
        CHECK(!g.thrown(true, 235.0f, 235.0f, 250.f, false)); // 18: cut short, no count
        CHECK(!g.thrown(true, 235.0f, 233.3f, 250.f));        // 19: the last hit (clipped 1.7)
        for (int i = 0; i < 5; ++i)
            CHECK(!g.thrown(true, 233.3f, 233.3f, 250.f, false)); // 20-24 cut short
        CHECK(!g.thrown(true, 233.3f, 233.3f, 250.f)); // 25: one
        CHECK(!g.holding());
        CHECK(g.thrown(true, 233.3f, 233.3f, 250.f)); // 30: two in a row - the gate
        CHECK(g.holding() && g.heldHp > 233.2f && g.heldHp < 233.4f && g.gates == 1);
        CHECK(!g.thrown(true, 233.3f, 233.3f, 250.f)); // once per gate
        g.hp(true, 233.3f);
        CHECK(g.holding());
        // A Y in the gate that lands (cut8: 233.3 -> 1).
        BossGate::YReport r;
        CHECK(!g.yPressed(true, 233.3f, 121.f, true, r));
        CHECK(g.y.gated && g.y.plain && g.y.no == 1);
        bool done = false;
        float hp = 233.3f;
        for (int f = 0; f < 400 && !done; ++f) {
            if (f == 60)
                hp = 1.f;
            g.hp(true, hp);
            done = g.yUpdate(1.f / 90, true, hp, r);
        }
        CHECK(done && BossGate::landed(r) && r.lowest == 1.f && r.gated);
        CHECK(!g.holding()); // his HP fell: the gate is over
        // One that does not land.
        CHECK(!g.yPressed(true, 1.f, 100.f, false, r));
        done = false;
        for (int f = 0; f < 400 && !done; ++f)
            done = g.yUpdate(1.f / 90, true, 1.f, r);
        CHECK(done && !BossGate::landed(r) && !r.gated && !r.plain);
        // A second press inside the watch reports the first one cut short.
        CHECK(!g.yPressed(true, 1.f, 100.f, true, r));
        CHECK(g.yPressed(true, 1.f, 100.f, true, r) && r.no == 3);
    }
    // cut8 (throws 22-27): 241 -> 236.5 (22), 23 cut, 24 -> 233.3, 25 none, 26 cut, 27 none: the gate at 27.
    {
        BossGate g;
        CHECK(!g.thrown(true, 241.f, 236.5f, 250.f));
        CHECK(!g.thrown(true, 236.5f, 236.5f, 250.f, false));
        CHECK(!g.thrown(true, 236.5f, 233.3f, 250.f));
        CHECK(!g.thrown(true, 233.3f, 233.3f, 250.f));
        CHECK(!g.thrown(true, 233.3f, 233.3f, 250.f, false));
        CHECK(g.thrown(true, 233.3f, 233.3f, 250.f));
        g.lost();
        CHECK(!g.holding());
    }
    // Not hurt (250/250) or no HP read: never a gate.
    {
        BossGate g;
        for (int i = 0; i < 5; ++i) {
            CHECK(!g.thrown(true, 250.f, 250.f, 250.f));
            CHECK(!g.thrown(false, -1.f, -1.f, 0.f));
        }
        CHECK(!g.holding());
    }
    if (failed)
        std::printf("%d cut16 check(s) failed\n", failed);
    else
        std::printf("cut16 tests passed\n");
    return failed ? 1 : 0;
}
