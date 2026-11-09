#include "spidy/vr_building_detail.hpp"
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <limits>
using namespace spidy::vr_building_detail;

int main() {
    int failures = 0;
    const auto check = [&](bool ok, const char* test) {
        if (!ok) { std::fprintf(stderr, "FAILED: %s\n", test); ++failures; }
    };
    check(textureRequest(700.f, 1000.f) == 1000.f, "reduced-eye request reaches display footprint");
    check(textureRequest(700.f, 2000.f) == 1050.f, "boost is capped at 1.5x");
    check(textureRequest(1200.f, 1000.f) == 1200.f, "never reduce texture request");
    check(textureRequest(1000.f, 1010.f) == 1000.f, "ignore tiny changes");
    check(textureRequest(0.f, 300.f) == 0.f, "reject zero request");
    check(textureRequest(std::numeric_limits<float>::infinity(), 300.f) ==
          std::numeric_limits<float>::infinity(), "reject invalid request");
    check(textureRequest(1000.f, std::numeric_limits<float>::quiet_NaN()) == 1000.f,
          "reject invalid new target");
    constexpr uintptr_t game = 0x1000, leftEye = 0x2000;
    check(primaryWindowOwner(true, false, true, game, game, 200), "primary game view eligible");
    check(!primaryWindowOwner(true, false, true, leftEye, game, 200), "eye views never eligible");
    check(!primaryWindowOwner(true, true, true, game, game, 200), "resting primary excluded");
    check(!primaryWindowOwner(true, false, false, game, game, 200), "unknown byte layout excluded");
    check(!primaryWindowOwner(true, false, true, game, game, 100), "FAR WINDOWS game disables writes");
    check(!primaryWindowOwner(false, false, true, game, game, 200), "outside VR excluded");
    check(windowReach(145.f, 200) == 217.5f, "2x request capped to 1.5x reach");
    check(windowReach(145.f, 150) == 217.5f, "1.5x request");
    check(windowReach(145.f, 100) == 145.f, "game reach preserved");
    check(windowReach(0.f, 200) == 0.f, "zero invalid distance");
    check(windowReach(std::numeric_limits<float>::quiet_NaN(), 200) !=
          windowReach(std::numeric_limits<float>::quiet_NaN(), 200),
          "NaN remains untouched, never written");
    std::puts(failures ? "Building detail test FAILED" : "Building detail tests passed");
    return failures ? 1 : 0;
}
