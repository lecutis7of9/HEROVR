// Test build 12 HANDS: each suit's rest hand frames (the converter's handBack and suit_wear's own) and the worn
// hand against a real controller grip (OpenXR /input/grip/pose, Touch): the hand bone's way and its back.
//   spidy_suit_hands <Models/>
#include "spidy/mesh_model.hpp"
#include "spidy/suit_wear.hpp"

#include <cstdio>

using namespace spidy;
namespace mm = spidy::mesh_model;
namespace sw = spidy::suit_wear;

int main(int argc, char** argv) {
    if (argc < 2)
        return 2;
    mm::Library lib;
    for (uint32_t s = mm::slotSuitFirst; s < mm::slots; ++s)
        lib.load(s, argv[1]);
    int bad = 0;
    for (uint32_t k = 0; k < mm::suitSlots; ++k) {
        const mm::Mesh* m = lib.model(mm::slotSuitFirst + k);
        sw::Rig rig;
        if (!m || !sw::makeRig(*m, rig)) {
            std::printf("%s: no rig\n", mm::suitFolders[k]);
            ++bad;
            continue;
        }
        std::printf("%-24s", mm::suitFolders[k]);
        for (int s = 0; s < 2; ++s) {
            const Vec3 a{m->suitHandBack[s * 3], m->suitHandBack[s * 3 + 1], m->suitHandBack[s * 3 + 2]}, b = rig.skeleton.handBack[s];
            std::printf(" %c model(%5.2f %5.2f %5.2f) now(%5.2f %5.2f %5.2f)", s ? 'R' : 'L', a.x, a.y, a.z, b.x, b.y, b.z);
        }
        std::printf("\n");
    }
    return bad;
}
