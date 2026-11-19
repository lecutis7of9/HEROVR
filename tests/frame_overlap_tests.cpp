// FRAME OVERLAP: the slot picker never hands back a fence that is still in flight,
// and it prefers a slot that has already retired so the caller does not wait.
#include "spidy/frame_overlap.hpp"

#include <cstdio>

namespace {
int fails = 0;
void check(bool ok, const char* what) {
    if (!ok) {
        std::fprintf(stderr, "FAIL %s\n", what);
        ++fails;
    }
}
} // namespace

int main() {
    using spidy::frame_overlap::pick;
    using spidy::frame_overlap::slots;
    check(slots == 3, "three slots");
    uint64_t fresh[3] = {0, 0, 0};
    auto c = pick(fresh, 3, 0);
    check(c.index == 0 && c.waitFor == 0, "an unused ring needs no wait");
    uint64_t one[3] = {5, 0, 0};
    c = pick(one, 3, 4);
    check(c.index == 1 && c.waitFor == 0, "the next free slot, not the one still at 5");
    uint64_t busy[3] = {9, 10, 11};
    c = pick(busy, 3, 8);
    check(c.index == 0 && c.waitFor == 9, "all busy: wait for the oldest");
    c = pick(busy, 3, 9);
    check(c.index == 0 && c.waitFor == 0, "the oldest has retired");
    c = pick(busy, 3, 10);
    check(c.index == 0 && c.waitFor == 0, "the first retired slot is taken");
    uint64_t later[3] = {12, 10, 11};
    c = pick(later, 3, 9);
    check(c.index == 1 && c.waitFor == 10, "the oldest is not slot 0");
    c = pick(nullptr, 3, 0);
    check(c.index == 0 && c.waitFor == 0, "no fences");
    // UINT64_MAX compares as "already retired". The renderer rejects it before pick.
    if (fails) {
        std::fprintf(stderr, "%d failed\n", fails);
        return 1;
    }
    std::puts("frame overlap slot picker ok");
    return 0;
}
