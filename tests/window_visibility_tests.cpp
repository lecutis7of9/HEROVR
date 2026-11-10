#include "spidy/window_visibility.hpp"
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <limits>

using namespace spidy::window_visibility;
int main() {
    int bad = 0;
    const auto check = [&](bool ok, const char* text) {
        if (!ok) { ++bad; std::fprintf(stderr, "FAILED: %s\n", text); }
    };
    constexpr uintptr_t main = 0x1000, alias = 0x2000, leftEye = 0x3000, otherView = 0x4000;
    check(owner(main, main, 0, false) == direct, "direct main view recognized");
    check(owner(alias, main, main, false) == parent, "main query context recognized");
    check(owner(alias, main, main, true) == other, "eye query excluded even if context aliases");
    check(owner(leftEye, main, 0, true) == other, "left eye excluded");
    check(owner(otherView, main, 0, false) == other, "unrelated view excluded");
    check(owner(0, main, 0, false) == other, "null view");
    check(owner(main, 0, 0, false) == other, "null main view");
    check(eligible(direct, true, false, true, 200), "main direct may write");
    check(eligible(parent, true, false, true, 150), "main context may write");
    check(!eligible(other, true, false, true, 200), "unrelated never writes");
    check(!eligible(direct, false, false, true, 200), "VR off never writes");
    check(!eligible(direct, true, true, true, 200), "resting primary never writes");
    // Regression: the live hook must pass `rested == 0` as the third arg.
    // Before the Nov 10 fix it passed `rested != 0`, so an active primary
    // view could never be eligible, despite the standalone gate passing.
    const auto liveEligible = [&](bool rested) {
        return eligible(direct, true, rested == false, true, 150);
    };
    check(liveEligible(false), "active primary query must be eligible");
    check(!liveEligible(true), "resting primary query must stay excluded");
    check(!eligible(direct, true, false, false, 200), "unknown bytes never writes");
    check(!eligible(direct, true, false, true, 100), "game-distance rollback switch");
    check(reach(145.f, 200) == 217.5f, "normal 2x request capped at 1.5x");
    check(reach(145.f, 300) == 217.5f, "high request still capped");
    check(reach(145.f, 150) == 217.5f, "1.5x reach request");
    check(reach(145.f, 100) == 145.f, "game-distance rollback no change");
    check(reach(0.f, 200) == 0.f, "invalid zero distance no change");
    check(reach(6000.f, 200) == 6000.f, "implausible distance no change");
    const auto nan = std::numeric_limits<float>::quiet_NaN();
    check(std::isnan(reach(nan, 200)), "NaN stays unchanged and cannot pass positive compare");
    check(std::puts(bad ? "Window visibility gate FAILED" : "Window visibility gate tests passed") >= 0,
          "print status");
    return bad ? 1 : 0;
}
