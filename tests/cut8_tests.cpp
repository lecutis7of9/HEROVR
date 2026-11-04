// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
// HERO VR 0.0.77 CUT8: cut8::handleParts (the registry handle form) and cut8::ThrowWatch (the per-throw log).
#include "spidy/cut8.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>

using namespace spidy::cut8;
static int failed = 0;
#define CHECK(x)                                                                                                       \
    do {                                                                                                               \
        if (!(x)) {                                                                                                    \
            std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #x);                                                    \
            ++failed;                                                                                                  \
        }                                                                                                              \
    } while (0)

int main() {
    uint32_t i{}, g{};
    // cut7 POI DUMP words: +98 6b72c101 (index 0x2c101, generation 0x6b7), +88 6b70ffff (index 0xffff: the form
    // fits; only the registry entry's generation and own handle say whether it is live).
    CHECK(handleParts(0x6b72c101u, 0x100000u, i, g) && i == 0x2c101u && g == 0x6b7u);
    CHECK(handleParts(0x6b70ffffu, 0x100000u, i, g) && i == 0xffffu);
    CHECK(!handleParts(0x6b7fffffu, 0x100000u, i, g)); // index all ones: none
    CHECK(!handleParts(0x00012345u, 0x100000u, i, g)); // generation 0
    CHECK(!handleParts(0x6b72c101u, 0x2c101u, i, g));  // past the registry's count
    CHECK(handleParts(0x00216e00u, 0x100000u, i, g) && i == 0x16e00u && g == 2u);

    ThrowWatch w;
    ThrowWatch::Report r;
    CHECK(!w.start(true, true, 1000.f, 1000.f, 12.f, 1350, 19613, true, 5.f, r));
    CHECK(w.active && w.r.throwNo == 1 && w.r.hp0 == 1000.f && w.r.stickX == 1350 && w.r.stickY == 19613);
    bool done = false;
    float hp = 1000.f;
    for (int f = 0; f < 300 && !done; ++f) {
        if (f == 40) // about 0.44 s in, the debris lands
            hp = 900.f;
        done = w.update(1.f / 90, true, true, hp, 11.f, r);
    }
    CHECK(done && !w.active);
    CHECK(r.hpAt[0] == 900.f && r.hpAt[1] == 900.f && r.hpAt[2] == 900.f && r.lowest == 900.f);
    CHECK(r.metres0 == 12.f && r.metresEnd == 11.f && r.bossSeen > 150);
    // No boss read: -1 stays, the frame count says so.
    CHECK(!w.start(false, false, 0.f, 0.f, 0.f, 0, 0, false, 0.f, r));
    CHECK(w.r.hp0 == -1.f && w.r.metres0 == -1.f);
    // A second throw while the first is watched: the first is reported cut short.
    ThrowWatch::Report cut;
    CHECK(w.start(true, true, 50.f, 100.f, 3.f, 0, 0, false, 0.f, cut) && cut.throwNo == 2 && w.r.throwNo == 3);
    // Bad dt never runs away.
    CHECK(!w.update(NAN, true, true, 50.f, 3.f, r) && w.t == 0.f);
    if (failed)
        return 1;
    std::printf("cut8 tests OK\n");
    return 0;
}
