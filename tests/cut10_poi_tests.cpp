#include "spidy/cut10_poi.hpp"
#include <cstdio>
using namespace spidy::cut10;
static int fails = 0;
#define CHECK(x) do { if (!(x)) { std::printf("FAIL %s\n", #x); ++fails; } } while (0)
int main() {
    const uint64_t img = 0x140000000ull;
    CHECK(layoutMatches(img, img + 0x53cf538, img + 0x53cf408));
    CHECK(!layoutMatches(img, img + 0x53cf540, img + 0x53cf408));
    CHECK(!layoutMatches(0, 0x53cf538, 0x53cf408));
    CHECK(poiViewRva + managerOffset == 0xb0f4cb8u);
    CHECK(countSane(0) && countSane(45) && !countSane(-1) && !countSane(5000));
    float a[3] = {1942.9f, 1.2f, -604.f}, z[3] = {0, 0, 0}, far_[3] = {9000.f, 0, 0}, nan_[3] = {NAN, 0, 0};
    CHECK(worldTriple(a, 1900.f, 0, -600.f));
    CHECK(!worldTriple(z, 0, 0, 0));
    CHECK(!worldTriple(far_, 0, 0, 0));
    CHECK(!worldTriple(nan_, 0, 0, 0));
    uint32_t w1[3] = {1, 2, 3}, w2[3] = {1, 2, 4};
    CHECK(signature(w1, 3) != signature(w2, 3) && signature(w1, 3) == signature(w1, 3));
    std::printf(fails ? "cut10_poi_tests FAILED\n" : "cut10_poi_tests ok\n");
    return fails ? 1 : 0;
}
