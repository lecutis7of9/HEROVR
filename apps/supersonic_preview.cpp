// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
//
// Version 0.0.5 SUPERSONIC: the sonic boom's shockwave (ring + cone) drawn by the game's own
// appendShockwave without a headset. Two cells from the player's eyes flying forward (0.05 and
// 0.3 s after the boom), one from a chase camera beside and behind him (0.3 s, the vapour cone
// around him), and a fourth with the holo settings and their subtexts.
//
//   spidy_supersonic_preview <path.bmp>
#include "spidy/combat38.hpp"
#include "spidy/extras385.hpp"
#include "spidy/fly_boost.hpp"
#include "spidy/pixel_font.hpp"
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
void text(std::vector<Vertex>& out, const std::string& a, Vec3 at, Vec3 eye, Vec3 color, float h, bool left = false) {
    // flat to the camera (it looks down -z), so left-aligned rows stay level
    (void)eye;
    const Vec3 right{1, 0, 0}, up{0, 1, 0};
    PixelTextStyle s;
    s.height = h;
    s.top = s.bottom = color;
    appendPixelText(out, a, left ? at : at - right * (pixelTextWidth(a, s) * .5f), right, up, s);
}
std::string upper(std::string a) {
    for (auto& c : a)
        if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 32);
    return a;
}
} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: spidy_supersonic_preview <path.bmp>\n");
        return 2;
    }
    constexpr float pixelAngle = .0026f; // about 95 degrees across a cell
    constexpr int size = 640, height = 640, cells = 4;
    Image image(size * cells, height);
    const Vec3 eye{0, 1.6f, 0};
    const Vec3 way = normalized(Vec3{0, .05f, -1});
    const float ages[3] = {.05f, .3f, .3f};
    char titles[3][64];
    for (int i = 0; i < 3; ++i)
        std::snprintf(titles[i], sizeof titles[i], i == 2 ? "%.2f S, CHASE CAMERA" : "%.2f S AFTER THE BOOM",
                      static_cast<double>(ages[i]));
    for (int cell = 0; cell < cells; ++cell) {
        const int x0 = cell * size;
        for (int y = 0; y < height; ++y)
            for (int x = x0; x < x0 + size; ++x) {
                const float v = static_cast<float>(y) / static_cast<float>(height);
                float* o = image.at(x, y);
                o[0] = decode(.30f - .18f * v);
                o[1] = decode(.45f - .25f * v);
                o[2] = decode(.70f - .35f * v);
            }
        std::vector<Vertex> world;
        // the chase camera: 7 m behind, 1.5 m up, a little to the right (kept looking down -z)
        const Vec3 camera = cell == 2 ? eye + Vec3{1.2f, 1.5f, 7.f} : eye;
        const Vec3 target = camera + way * 4.f;
        if (cell < 3) {
            appendShockwave(world, eye, way, ages[cell], pixelAngle);
            if (cell == 2) // where the hero's eyes are
                text(world, "YOU", eye + Vec3{0, .1f, 0}, camera, {1, .3f, .3f}, .12f);
            text(world, titles[cell], camera + Vec3{0, .55f, -1.4f}, camera, {1, .85f, .3f}, .05f);
            char line[96];
            std::snprintf(line, sizeof line, "FADE %.2f  DASH %.0f M/S (2X OF FAST %.0f)",
                          static_cast<double>(fly_boost::shock(ages[cell])),
                          static_cast<double>(fly_boost::speedOf(0)), static_cast<double>(extras385::fly::speeds[3]));
            text(world, line, camera + Vec3{0, -.6f, -1.4f}, camera, {1, 1, 1}, .03f);
        } else {
            const Vec3 at = eye + Vec3{-.62f, .5f, -1.2f};
            float y = 0;
            auto row = [&](const std::string& name, const std::string& value, const std::string& sub) {
                text(world, name + "  " + value, at + Vec3{0, y, 0}, eye, {.55f, .88f, 1}, .04f, true);
                y -= .06f;
                text(world, upper(sub), at + Vec3{0, y, 0}, eye, {1, 1, 1}, .017f, true);
                y -= .11f;
            };
            row("FLY SPEED", "FAST (40 M/S)", extras385::fly::speedHelp);
            const auto& s = combat38::optionInfo[combat38::supersonic];
            const auto& k = combat38::optionInfo[combat38::supersonicSpeed];
            row(s.label, "ON", s.help);
            row(k.label, "2X (80 M/S)", k.help);
            text(world, "1.5X 60 / 2X 80 / 2.5X 100 / 3X 120 M/S", at + Vec3{0, y, 0}, eye, {1, .85f, .3f}, .02f, true);
            y -= .1f;
            text(world, "BOTH HANDS OUT + LEFT STICK FORWARD = DASH", at + Vec3{0, y, 0}, eye, {1, .6f, .2f}, .022f, true);
        }
        std::printf("cell %d: %zu vertices\n", cell, world.size());
        const Camera lens{1 / pixelAngle, size * .5f, height * .5f};
        draw(image, look(world, camera, target), lens, 0, 0, x0, 0, size, height);
    }
    return save(image, argv[1]) ? 0 : 1;
}
