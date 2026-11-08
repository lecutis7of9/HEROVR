#include "spidy/aer_schedule.hpp"
#include <cstdio>
using namespace spidy::aer;

int main() {
    int bad = 0;
    auto check = [&](bool ok, const char* name) {
        if (!ok) { std::fprintf(stderr, "FAIL: %s\n", name); ++bad; }
    };
    Input in{};
    auto p = schedule(in);
    check(!p.eligible() && p.reason == Reason::noGeneration, "no generation");

    in.generation = 101; // odd -> right eye renders, left eye synthesized
    p = schedule(in);
    check(p.renderEye == 1 && p.synthesizeEye == 0, "deterministic odd frame");
    check(p.reason == Reason::noTracking, "tracking required");

    in.trackingValid = true;
    in.sceneCut = true;
    check(schedule(in).reason == Reason::sceneCut, "cut rejects cached eye");
    in.sceneCut = false;
    check(schedule(in).reason == Reason::noCachedRealEye, "missing real source");

    in.lastRealEyeGeneration[0] = 100;
    in.realEyeReady[0] = true;
    check(schedule(in).reason == Reason::gpuNotComplete, "fence required");
    in.gpuFenceComplete[0] = true;
    check(schedule(in).reason == Reason::missingDepthReprojection, "rotational-only warp prohibited");
    in.depthReprojectionReady = true;
    check(schedule(in).reason == Reason::missedDeadline, "deadline required");
    in.beforeDeadline = true;
    check(schedule(in).eligible(), "ready source in safe conditions");

    in.generation = 102; // even -> left eye renders, right eye synthesized
    check(schedule(in).renderEye == 0 && schedule(in).synthesizeEye == 1, "deterministic even frame");
    check(schedule(in).reason == Reason::noCachedRealEye, "never reuse opposite-eye source");

    in.lastRealEyeGeneration[1] = 99;
    in.realEyeReady[1] = in.gpuFenceComplete[1] = true;
    check(schedule(in).reason == Reason::staleCachedRealEye, "age rejects stale source");
    in.lastRealEyeGeneration[1] = 101;
    check(schedule(in).eligible(), "right source now fresh");
    in.lastRealEyeGeneration[1] = 103;
    check(schedule(in).reason == Reason::noCachedRealEye, "future source invalid");
    std::puts(bad ? "AER schedule checks FAILED" : "AER schedule checks passed");
    return bad ? 1 : 0;
}
