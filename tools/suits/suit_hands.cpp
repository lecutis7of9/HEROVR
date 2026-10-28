// Test build 12 HANDS: each suit's rest hand frames (the converter's handBack and suit_wear's own) and the worn
// hand against a real controller grip (OpenXR /input/grip/pose, Touch): the hand bone's way and its back.
//   spidy_suit_hands <Models/>
#include "spidy/mesh_model.hpp"
#include "spidy/suit_wear.hpp"

#include <cstdio>
#include <cstdlib>
#include <string>

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
        if (!m || !sw::makeRig(*m, rig, mm::suitFolders[k])) {
            std::printf("%s: no rig\n", mm::suitFolders[k]);
            ++bad;
            continue;
        }
        std::printf("%-24s", mm::suitFolders[k]);
        for (int s = 0; s < 2; ++s) {
            const Vec3 a{m->suitHandBack[s * 3], m->suitHandBack[s * 3 + 1], m->suitHandBack[s * 3 + 2]}, b = rig.skeleton.handBack[s];
            std::printf(" %c model(%5.2f %5.2f %5.2f) now(%5.2f %5.2f %5.2f) thumb %+.0f", s ? 'R' : 'L', a.x, a.y, a.z, b.x, b.y, b.z, rig.handThumb[s]);
        }
        std::printf("\n");
        if (const char* dump = std::getenv("SUIT_HANDS_DUMP")) { // the rest hands' points (x y z bone), for a look
            std::FILE* f = std::fopen((std::string(dump) + mm::suitFolders[k] + ".txt").c_str(), "w");
            if (f) {
                for (int s = 0; s < 2; ++s) {
                    const Vec3 w = rig.skeleton.at[s ? sw::wristR : sw::wristL], t = rig.skeleton.at[s ? sw::tipR : sw::tipL], b = rig.skeleton.handBack[s];
                    std::fprintf(f, "E %d %f %f %f %f %f %f %f %f %f\n", s, w.x, w.y, w.z, t.x, t.y, t.z, b.x, b.y, b.z);
                }
                for (size_t tr = 0; tr < rig.triangleBone.size(); ++tr) {
                    const uint32_t best = rig.triangleBone[tr];
                    if (best != sw::boneHandL && best != sw::boneHandR && best != sw::boneForeL && best != sw::boneForeR)
                        continue;
                    std::fprintf(f, "T %u", best);
                    for (int c = 0; c < 3; ++c) {
                        const Vec3 q = rig.rest[rig.corner[tr * 3 + c]];
                        std::fprintf(f, " %f %f %f", q.x, q.y, q.z);
                    }
                    std::fprintf(f, "\n");
                }
                std::fclose(f);
            }
        }
    }
    return bad;
}
