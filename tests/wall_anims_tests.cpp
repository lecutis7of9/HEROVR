#include "spidy/wall_anims.hpp"
#include <cstdio>
#include <cstdlib>
#define CHECK(c) do { if(!(c)) { std::fprintf(stderr, "FAILED %s:%d %s\n", __FILE__, __LINE__, #c); std::exit(1); } } while(0)
using namespace spidy::wall_anims;
struct Run {
    Watch w;
    std::vector<Note> notes;
    double t = 0;
    Action step(Input in, float dt = .05f) {
        t += dt;
        in.session = t;
        return w.update(dt, in, notes);
    }
    bool has(Kind k) const {
        for (const auto& n : notes)
            if (n.kind == k)
                return true;
        return false;
    }
};
int main() {
    CHECK(sizeof(Data) == 512);
    CHECK(classify(1, false, false) == ownWall && classify(2, true, false) == gameWall && classify(3, false, true) == floorCrawl);
    CHECK(classify(0, true, false) == air && classify(0, false, true) == ground);
    { // contact, hand-over, leave with intent: no drop
        Run r;
        Input in{};
        in.enabled = true;
        in.airborne = true;
        r.step(in);
        in.surface = 1; in.airborne = false; in.speed = 5;
        r.step(in);
        CHECK(r.has(contact) && r.w.data().contacts[ownWall] == 1);
        in.surface = 2;
        r.step(in);
        CHECK(r.has(handOver) && r.w.data().handOvers == 1);
        in.intent = true;
        r.step(in);
        in.intent = false; in.surface = 0; in.airborne = true; in.vy = -9;
        r.step(in);
        CHECK(r.has(leave));
        for (int i = 0; i < 20; ++i) r.step(in);
        CHECK(!r.has(drop) && r.w.data().drops == 0);
    }
    { // a drop off the game's wall with no intent: DROP, recover; the third: auto-off
        Run r;
        Input in{};
        in.enabled = true;
        Action last;
        for (int k = 1; k <= 3; ++k) {
            in.surface = 2; in.airborne = false; in.vy = 0; in.y = 50;
            r.step(in);
            in.surface = 0; in.airborne = true; in.vy = -8;
            bool recover = false, off = false;
            for (int i = 0; i < 12; ++i) {
                in.y -= .4f;
                last = r.step(in);
                recover |= last.recover; off |= last.autoOff;
            }
            CHECK(recover);
            CHECK(off == (k == 3));
            CHECK(r.w.data().dropsGame == static_cast<uint32_t>(k));
        }
    }
    { // OFF: a drop is logged, nothing acts
        Run r;
        Input in{};
        in.surface = 1;
        r.step(in);
        in.surface = 0; in.airborne = true; in.vy = -8;
        for (int i = 0; i < 12; ++i) CHECK(!r.step(in).recover);
        CHECK(r.w.data().drops == 1 && r.w.data().recoveries == 0);
    }
    { // a web held after leaving is no drop
        Run r;
        Input in{};
        in.enabled = true;
        in.surface = 2;
        r.step(in);
        in.surface = 0; in.airborne = true; in.vy = -8; in.webbed = true;
        for (int i = 0; i < 12; ++i) r.step(in);
        CHECK(r.w.data().drops == 0);
    }
    { // 79.7 the yaw jump the tracking space put back: counted and noted, with its size
        Run r;
        Input in{};
        in.enabled = true; in.surface = 2;
        for (int i = 0; i < 10; ++i) r.step(in);
        CHECK(r.w.data().flips == 0 && !r.has(flip));
        in.yawJumpDegrees = -165.f;
        r.step(in);
        in.yawJumpDegrees = 0.f;
        r.step(in);
        CHECK(r.w.data().flips == 1 && r.has(flip) && r.w.data().maxYawJumpDegrees > 164.f);
    }
    { // a far fall is logged
        Run r;
        Input in{};
        in.surface = 2; in.y = 300;
        r.step(in);
        in.surface = 0; in.airborne = true; in.vy = -30;
        for (int i = 0; i < 40; ++i) { in.y -= 2.5f; r.step(in); }
        CHECK(r.has(farFall) && r.w.data().farFalls == 1);
    }
    { // the ring keeps the latest events; text and summary say '79.7 WALL'
        Run r;
        Input in{};
        for (int i = 0; i < 40; ++i) { in.surface = i % 2 ? 0 : 1; in.airborne = !(i % 2); r.step(in); }
        CHECK(r.w.data().eventCount == ringSize && r.w.data().eventTotal > ringSize);
        for (const auto& n : r.notes) CHECK(text(n).rfind("79.7 WALL", 0) == 0);
        CHECK(summary(r.w.data()).rfind("79.7 WALL:", 0) == 0);
    }
    std::puts("wall_anims tests passed");
}
