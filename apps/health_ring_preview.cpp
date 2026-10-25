// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
//
// Update 36: draws the health and focus ring (health_ring.hpp) without a
// headset, 35 cm from the eyes at about a headset's pixel density (18 a
// degree, so the ring is about as many pixels across as in the headset),
// over a dusk street: one image a state, written as <prefix>-<state>.bmp.
//   full      health and focus full
//   h75-f50   health 75 percent, focus half spent in slow motion (glowing)
//   h25-f15   health low (its beat), focus nearly out
//   warning   focus empty and the refusal's red flash
//   na        health unread (N/A, empty crescent), focus 60 percent
//   focus-drain  health unread (N/A) while slow motion drains focus: 35
//                percent left, held at the thin left tip
//
//   spidy_health_ring_preview <prefix>
#include "spidy/calibration_art.hpp"
#include "spidy/health_ring.hpp"
#include "preview_image.hpp"
#include <cmath>
#include <cstdio>
#include <limits>
#include <string>
using namespace spidy;
using namespace spidy::preview;

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: spidy_health_ring_preview <prefix>\n");
        return 2;
    }
    calibration_art::Image atlas;
    if (!calibration_art::load(atlas)) {
        std::fprintf(stderr, "the embedded atlas did not decode\n");
        return 1;
    }
    const auto texture = Texture::from(calibration_art::mips(atlas, 5));
    const int W = 360, H = 300;
    const float f = 18 * 57.2958f;
    const Camera camera{f, W / 2.f, H / 2.f};
    const Vec3 centre{0, 0, -.35f};
    const Pose hand{centre - health_ring::fromHand, {}};
    const float nan = std::numeric_limits<float>::quiet_NaN();
    struct State {
        const char* name;
        float health, focus, blend, warning;
        bool active;
        float clock;
    };
    const State states[] = {
        {"full", 1, 1, 0, 0, false, 0},
        {"h75-f50", .75f, .5f, 1, 0, true, .2618f},
        {"h25-f15", health_ring::healthShare(27.5f, 110), .15f, .6f, 0, true, .156f},
        {"warning", .6f, 0, 0, 1, false, 0},
        {"na", nan, .6f, 0, 0, false, 0},
        {"focus-drain", nan, .35f, 1, 0, true, .2618f},
    };
    int failed = 0;
    for (const auto& s : states) {
        Image image(W, H);
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x) {
                const float v = static_cast<float>(y) / H;
                const bool lit = ((x / 9 + y / 13) % 7 == 0) && v > .35f;
                float* o = image.at(x, y);
                o[0] = decode(lit ? .55f : .30f - .18f * v);
                o[1] = decode(lit ? .42f : .26f - .16f * v);
                o[2] = decode(lit ? .25f : .36f - .18f * v);
            }
        SlowMotionView slow;
        slow.meter = 1;
        slow.focus = s.focus;
        slow.blend = s.blend;
        slow.warning = s.warning;
        slow.active = s.active;
        std::vector<Vertex> art;
        health_ring::append(art, slow, s.health, hand, {0, 0, 0}, 1 / f, s.clock);
        drawArt(image, art, texture, camera, 0, 0, 0, 0, W, H);
        if (!save(image, std::string(argv[1]) + "-" + s.name + ".bmp"))
            ++failed;
    }
    return failed ? 1 : 0;
}
