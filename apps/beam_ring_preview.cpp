// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
//
// Version 0.0.5 BEAM RING: the chest beam's cooldown on the right wrist, drawn by the game's own
// appendBeamRing without a headset (a made-up forearm, not a recording). Four cells, the player
// looking at the right wrist: just fired (empty), 15 s (half), 27 s (nine tenths), READY (the flash).
//
//   spidy_beam_ring_preview <path.bmp>
#include "spidy/pixel_font.hpp"
#include "spidy/repulsor.hpp"
#include "spidy/soft_shapes.hpp"
#include "spidy/web_visual.hpp"
#include "preview_image.hpp"
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>
using namespace spidy;
using namespace spidy::preview;

namespace {
std::vector<Vertex> look(const std::vector<Vertex>& world, Vec3 eye, Vec3 target) {
    const Vec3 back = normalized(eye - target);
    const Vec3 right = normalized(cross(Vec3{0, 1, 0}, back));
    const Vec3 up = cross(back, right);
    auto out = world;
    for (auto& v : out) {
        const Vec3 d = v.position - eye;
        v.position = {dot(d, right), dot(d, up), dot(d, back)};
    }
    return out;
}
void text(std::vector<Vertex>& out, const std::string& a, Vec3 at, Vec3 eye, Vec3 color, float h) {
    const Vec3 back = normalized(eye - at);
    const Vec3 right = normalized(cross(Vec3{0, 1, 0}, back));
    const Vec3 up = cross(back, right);
    PixelTextStyle s;
    s.height = h;
    s.top = s.bottom = color;
    appendPixelText(out, a, at - right * (pixelTextWidth(a, s) * .5f), right, up, s);
}
} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: spidy_beam_ring_preview <path.bmp>\n");
        return 2;
    }
    constexpr float pixelAngle = .0012f;
    constexpr int size = 640, height = 640, cells = 4;
    Image image(size * cells, height);
    const Vec3 eye{0, 1.6f, 0};
    const float shares[cells] = {0.01f, .5f, .9f, 1.f};
    const float flashes[cells] = {0, 0, 0, .55f};
    const char* titles[cells] = {"JUST FIRED", "15 OF 30 S", "27 OF 30 S", "READY"};
    const char* lines[cells] = {"EMPTY, FILLS CLOCKWISE", "HALF", "ALMOST", "CHIME + BUZZ + FLASH"};
    for (int cell = 0; cell < cells; ++cell) {
        const int x0 = cell * size;
        for (int y = 0; y < height; ++y)
            for (int x = x0; x < x0 + size; ++x) {
                const float v = static_cast<float>(y) / static_cast<float>(height);
                float* o = image.at(x, y);
                o[0] = decode(.10f + .22f * v);
                o[1] = decode(.09f + .10f * v);
                o[2] = decode(.18f + .08f * v);
            }
        std::vector<Vertex> world;
        // The right hand held up in front, its back (+y) toward the eyes.
        Pose hand{{.08f, 1.32f, -.34f}, {}};
        const Vec3 face = hand.position + Vec3{0, .045f, .065f};
        const Vec3 target = face;
        // the forearm and glove (dark red), behind the ring
        soft::softBar(world, hand.position + Vec3{.1f, -.22f, .1f}, hand.position + Vec3{0, .02f, .04f},
                      normalized(face - eye), .04f, {.35f, .06f, .06f}, 1, .002f);
        soft::softBar(world, hand.position + Vec3{0, .02f, .04f}, hand.position + Vec3{0, .03f, -.06f},
                      normalized(face - eye), .045f, {.55f, .08f, .08f}, 1, .002f);
        appendBeamRing(world, shares[cell], flashes[cell], hand, eye, pixelAngle, 1.f);
        text(world, titles[cell], face + Vec3{0, .075f, 0}, eye, cell == 3 ? Vec3{.55f, .88f, 1} : Vec3{1, .6f, .2f}, .014f);
        text(world, lines[cell], face + Vec3{0, -.06f, 0}, eye, {1, 1, 1}, .008f);
        std::printf("cell %d: share %.2f flash %.2f, %zu vertices\n", cell, static_cast<double>(shares[cell]),
                    static_cast<double>(flashes[cell]), world.size());
        const Camera camera{1 / pixelAngle, size * .5f, height * .5f};
        draw(image, look(world, eye, target), camera, 0, 0, x0, 0, size, height);
    }
    return save(image, argv[1]) ? 0 : 1;
}
