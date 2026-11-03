// HERO VR 0.0.77 PROMPT WINDOW and BOSS DAMAGE (include/spidy/story077c.hpp, world_trace's isBossClass), pure.
#include "spidy/story077c.hpp"
#include "spidy/world_trace.hpp"

#include <cstdio>

using namespace spidy;
using namespace spidy::story077c;

namespace {
int failures = 0;
void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++failures;
    }
}
} // namespace

int main() {
    // A Quest 3 eye's tangents (about 52 left / 45 right / 47 up / 55 down degrees), 2064 x 2208 pixels.
    float r[4];
    const float tl = -1.28f, tr = 1.f, tu = 1.07f, td = -1.43f;
    check(promptRect(promptOn, tl, tr, tu, td, 2064, 2208, 16.f / 9, r), "on: a window");
    check(r[0] > 0 && r[2] < 2064 && r[2] - r[0] > 400 && r[2] - r[0] < 800, "on: about a quarter of the width");
    check(r[1] > 2208 * .5f && r[3] < 2208, "on: low in the view, inside it");
    const float mid = (r[1] + r[3]) * .5f, wantMid = (tu + .36f) / (tu - td) * 2208;
    check(mid > wantMid - 1 && mid < wantMid + 1, "on: centred at the tangent -0.36");
    const float aspect = (r[2] - r[0]) / (r[3] - r[1]) * ((tr - tl) / 2064) / ((tu - td) / 2208);
    check(aspect > 16.f / 9 - .02f && aspect < 16.f / 9 + .02f, "on: the game image's shape (tangent space)");
    float big[4];
    check(promptRect(promptLarge, tl, tr, tu, td, 2064, 2208, 16.f / 9, big), "large: a window");
    check(big[2] - big[0] > r[2] - r[0], "large: wider");
    check(!promptRect(promptOff, tl, tr, tu, td, 2064, 2208, 16.f / 9, r), "off: none");
    check(!promptRect(7, tl, tr, tu, td, 2064, 2208, 16.f / 9, r), "bad mode: none");
    check(!promptRect(promptOn, tl, tr, tu, td, 0, 2208, 16.f / 9, r), "no width: none");
    check(!promptRect(promptOn, NAN, tr, tu, td, 2064, 2208, 16.f / 9, r), "NaN lens: none");
    check(!promptRect(promptOn, tl, tr, tu, td, 2064, 2208, 0.f, r), "no image: none");
    check(!promptRect(promptOn, tl, tr, tu, td, 8, 8, 16.f / 9, r), "under 8 pixels: none");
    // A narrow lens: clamped inside the image.
    check(promptRect(promptLarge, -.3f, .3f, .3f, -.3f, 1000, 1000, 16.f / 9, r) || true, "narrow: no crash");
    check(r[0] >= 0 && r[2] <= 1000 && r[1] >= 0 && r[3] <= 1000, "narrow: clamped");
    check(promptModeName(promptOn)[0] == 'O' && promptModeName(promptLarge)[0] == 'L', "names");

    // BOSS DAMAGE: facing.
    check(facing({0, 30, -200}, {0, 0, -1}), "boss ahead and high: facing");
    check(facing({60, 0, -200}, {0, -.5f, -1}), "17 degrees off: facing");
    check(!facing({200, 0, -200}, {0, 0, -1}), "45 degrees off: not");
    check(!facing({0, 0, 200}, {0, 0, -1}), "behind: not");
    check(!facing({0, 0, .1f}, {0, 0, -1}), "on top of him: not");
    check(!facing({0, 0, -200}, {0, 1, 0}), "looking straight up: not");
    check(!facing({NAN, 0, -200}, {0, 0, -1}), "NaN: not");
    check(bossStrongScale == 4.f && bossProxyMetres > 2 && bossProxyMetres < 30, "constants");

    // Boss classes (world_trace).
    check(world_trace::isBossClass(".?AVSandmanBot@@"), "Sandman");
    check(world_trace::isBossClass(".?AVLizardBot@@"), "Lizard");
    check(!world_trace::isBossClass(".?AVHumanoidBot@@"), "a thug: not");
    check(!world_trace::isBossClass(""), "empty: not");
    check(!world_trace::isBossClass(nullptr), "null: not");

    if (failures)
        return 1;
    std::printf("story077c: all passed\n");
    return 0;
}
