// HERO VR 0.0.82: DIVE FIX (AirYield web press, freefall as air), SWING ASSIST / ANCHOR RANGE (Swing::shot), and the
// retired WRISTS webline mode. Standalone: only spidy/swing.hpp + src/swing.cpp and header-only game_swing / webline_modes.
// (The same cases also live in tests/core_tests.cpp.)
#include "spidy/game_swing.hpp"
#include "spidy/swing.hpp"
#include "spidy/webline_modes.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <utility>
#include <cstdlib>
#include <optional>

using namespace spidy;
static int failures = 0;
#define CHECK(x) do { if (!(x)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #x); ++failures; } } while (0)

namespace {
struct Building : World {
    Vec3 low{5, 0, -20}, high{15, 30, 20};
    std::optional<RayHit> raycast(Vec3 o, Vec3 d, float distance) const override {
        const float from[3]{o.x, o.y, o.z}, way[3]{d.x, d.y, d.z}, lo[3]{low.x, low.y, low.z}, hi[3]{high.x, high.y, high.z};
        float enter = 0, leave = distance, normal[3]{};
        for (int axis = 0; axis < 3; ++axis) {
            if (std::abs(way[axis]) < 1e-8f) {
                if (from[axis] < lo[axis] || from[axis] > hi[axis])
                    return {};
                continue;
            }
            float first = (lo[axis] - from[axis]) / way[axis], second = (hi[axis] - from[axis]) / way[axis];
            float out = -1;
            if (first > second) { std::swap(first, second); out = 1; }
            if (first > enter) { enter = first; normal[0] = normal[1] = normal[2] = 0; normal[axis] = out; }
            leave = std::min(leave, second);
            if (enter > leave)
                return {};
        }
        if (enter <= 0)
            return {};
        return RayHit{o + d * enter, {normal[0], normal[1], normal[2]}, 7, true};
    }
    MoveResult sweep(Vec3, Vec3 to, float) const override { return {to, {}, false}; }
    bool exists(std::uint64_t id) const override { return id == 7; }
};
Pose aimAt(Vec3 from, Vec3 at) { return {from, arc({0, 0, -1}, normalized(at - from))}; }
bool near(float a, float b, float e) { return std::fabs(a - b) <= e; }
} // namespace

int main() {
    { // AirYield: a new web press ends the hand-over
        using game_swing::AirYield;
        uint64_t steps = 100, air = 200, now = 1000;
        AirYield y;
        auto step = [&](bool asked, bool airRuns, bool press = false) {
            ++steps; now += 5; if (airRuns) air += 2;
            return y.update(now, asked, true, true, false, steps, air, false, press);
        };
        for (int i = 0; i < 5; ++i) step(true, true);
        step(true, false);
        CHECK(step(false, false) && y.began());
        CHECK(step(false, false, true));
        now += AirYield::pressMinMs;
        CHECK(!step(false, false, true) && y.pressedOut() == 1);
        CHECK(!step(false, false));
        // the old way still works: no press keeps the hand-over through the move
        AirYield z; uint64_t s = 1, a = 1, t = 1000;
        z.update(t, true, true, true, false, s, a); s += 3; t += 5;
        CHECK(z.update(t, false, true, true, false, s, a)); t += 500; ++s;
        CHECK(z.update(t, false, true, true, false, s, a, false, false));
    }
    { // freefall is air
        game_swing::Data d;
        d.status = 1; d.qpc = 1000; d.grounded = 0; d.collisionFlags = 0; d.velocity = {0, -12, 0};
        CHECK(game_swing::freefall(d) && game_swing::airborne(d, 1001, 1000));
        d.velocity = {0, -2, 0};
        CHECK(!game_swing::airborne(d, 1001, 1000));
        d.velocity = {0, -12, 0}; d.wall = static_cast<uint32_t>(game_swing::Surface::game);
        CHECK(!game_swing::airborne(d, 1001, 1000));
        d.wall = 0; d.grounded = 1;
        CHECK(!game_swing::airborne(d, 1001, 1000));
        d.grounded = 0; d.collisionFlags = 0x10; d.velocity = {0, 0, 0};
        CHECK(game_swing::airborne(d, 1001, 1000)); // the old air flag still counts
    }
    { // settings v9
        game_swing::Settings s;
        CHECK(s.version == 9 && s.bytes == 60 && game_swing::validSettings(s));
        s.anchorRange = 10; CHECK(!game_swing::validSettings(s));
        s.anchorRange = 140; s.assistDegrees = 31; CHECK(!game_swing::validSettings(s));
        s.assistDegrees = 10; s.assist = 2; CHECK(!game_swing::validSettings(s));
        s.assist = 1; s.version = 8; s.bytes = game_swing::settingsBytesV8; s.anchorRange = 0; CHECK(game_swing::validSettings(s));
    }
    { // assist
        Building w;
        const Vec3 from{0, 10, 0};
        SwingConfig c; c.assistDegrees = 10;
        const Swing swing(c);
        const Pose nearMiss = aimAt(from, from + Vec3{1, 0, -6});
        const auto plain = swing.shot(nearMiss, from, w);
        CHECK(plain.web && plain.web->airAnchor && !plain.assisted);
        CHECK(plain.web && near(length(plain.web->anchor - nearMiss.position), 100.f, .01f));
        const auto snapped = swing.shot(nearMiss, from, w, true);
        CHECK(snapped.web && !snapped.web->airAnchor && snapped.web->surface == 7 && snapped.assisted);
        const auto far = swing.shot(aimAt(from, from + Vec3{0, 0, -1}), from, w, true);
        CHECK(far.web && far.web->airAnchor && !far.assisted);
        const Swing off{SwingConfig{}};
        const auto none = off.shot(nearMiss, from, w, true);
        CHECK(none.web && none.web->airAnchor && !none.assisted);
        const auto direct = swing.shot(aimAt(from, {5, 10, 0}), from, w, true);
        CHECK(direct.web && direct.web->surface == 7 && !direct.assisted);
        SwingConfig bare; bare.airAnchors = false;
        CHECK(!Swing(bare).shot(nearMiss, from, w, true).web);
        SwingConfig noAir = c; noAir.airAnchors = false;
        CHECK(Swing(noAir).shot(nearMiss, from, w, true).web.has_value());
    }
    { // range
        Building w; w.low = {120, 0, -20}; w.high = {130, 60, 20};
        const Vec3 from{0, 10, 0};
        Swing swing{SwingConfig{}};
        const Pose at = aimAt(from, {120, 10, 0});
        const auto before = swing.shot(at, from, w);
        CHECK(before.web && before.web->airAnchor);
        swing.setAnchorRange(140);
        const auto after = swing.shot(at, from, w);
        CHECK(after.web && !after.web->airAnchor && after.web->surface == 7);
        const Pose away = aimAt(from, {-10, 10, 0});
        const auto sky = swing.shot(away, from, w);
        CHECK(sky.web && sky.web->airAnchor && near(length(sky.web->anchor - away.position), 100.f, .01f));
        swing.setAnchorRange(1000);
        CHECK(swing.config().maxRange == 250.f);
    }
    { // webline modes: WRISTS retired
        using namespace webline_modes;
        CHECK(effectiveMode(modeGrip) == modeGrip && effectiveMode(modeWrists) == modeGrip && effectiveMode(modeTwoHand) == modeTwoHand);
        CHECK(effectiveMode(7) == modeGrip);
        CHECK(nextMode(modeGrip) == modeTwoHand && nextMode(modeTwoHand) == modeGrip && nextMode(modeWrists) == modeTwoHand);
    }
    if (failures) { std::printf("%d dive_fix_082 failures\n", failures); return 1; }
    std::puts("dive_fix_082 tests passed");
    return 0;
}
