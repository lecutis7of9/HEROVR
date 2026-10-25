// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
//
// Version 0.0.4 IRON MAN ABILITIES (REPULSOR): draws the palm blast and the chest beam without a headset
// (made-up hands and enemies, not a recording), over a dusk sky. Three cells:
//   1 one press: the blast leaves the right wrist (the palm flash), bends (AIM STRENGTH STRONG) onto
//     an enemy 20 degrees off the aim, drawn at 5 moments, and its impact flash
//   2 the same from the player's eyes, the bolt 4 m out
//   3 the CHEST BEAM: both triggers held, one large beam from the chest straight ahead through two
//     enemies, and the wrist's cooldown line just after it fired
//
//   spidy_repulsor_preview <path.bmp>
#include "spidy/our_ball.hpp"
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
void dusk(Image& image, int x0, int size, int height) {
    for (int y = 0; y < height; ++y)
        for (int x = x0; x < x0 + size; ++x) {
            const float v = static_cast<float>(y) / static_cast<float>(height);
            float* o = image.at(x, y);
            const bool below = v > .70f;
            o[0] = decode(below ? .12f : .10f + .25f * v);
            o[1] = decode(below ? .12f : .08f + .12f * v);
            o[2] = decode(below ? .15f : .20f + .10f * v);
        }
}
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
void label(std::vector<Vertex>& out, const std::string& a, const std::string& b, Vec3 at, Vec3 eye, Vec3 color, float k = 0) {
    const Vec3 back = normalized(eye - at);
    const Vec3 right = normalized(cross(Vec3{0, 1, 0}, back));
    const Vec3 up = cross(back, right);
    if (k <= 0)
        k = length(at - eye) / .6f;
    PixelTextStyle s;
    s.height = .022f * k;
    s.top = s.bottom = color;
    appendPixelText(out, a, at - right * (pixelTextWidth(a, s) * .5f) + up * (.03f * k), right, up, s);
    s.height = .016f * k;
    s.top = s.bottom = Vec3{1, 1, 1};
    appendPixelText(out, b, at - right * (pixelTextWidth(b, s) * .5f), right, up, s);
}
void enemy(std::vector<Vertex>& world, Vec3 f, Vec3 eye) {
    soft::softBar(world, f + Vec3{0, .1f, 0}, f + Vec3{0, 1.5f, 0}, normalized(f - eye), .22f, {.05f, .05f, .07f}, 1, .002f);
    soft::softBar(world, f + Vec3{0, 1.55f, 0}, f + Vec3{0, 1.8f, 0}, normalized(f - eye), .13f, {.05f, .05f, .07f}, 1, .002f);
}
void hand(std::vector<Vertex>& out, Vec3 at, Vec3 eye) {
    soft::softBar(out, at - Vec3{0, .015f, 0}, at + Vec3{0, .015f, 0}, normalized(at - eye), .03f, {.7f, .1f, .1f}, .9f, .01f);
}
} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: spidy_repulsor_preview <path.bmp>\n");
        return 2;
    }
    constexpr float pixelAngle = .0016f;
    constexpr int size = 900, height = 900, cells = 3;
    Image image(size * cells, height);
    const Vec3 head{0, 1.6f, 0};
    for (int cell = 0; cell < cells; ++cell) {
        const int x0 = cell * size;
        dusk(image, x0, size, height);
        std::vector<Vertex> world;
        Vec3 eye, target;
        if (cell < 2) {
            using namespace our_ball;
            Pool pool;
            Enemies e;
            const float r = 20 * .0174533f;
            e.list[0].feet = Vec3{std::sin(r) * 9, 0, -std::cos(r) * 9};
            e.list[0].id = 7;
            e.count = 1;
            const Vec3 wrist{.22f, 1.3f, -.25f};
            if (cell == 0) {
                eye = {-5.2f, 2.3f, -6.8f}; // side on, so the bend shows
                target = {1.65f, 1.1f, -4.35f};
            } else {
                eye = head;
                target = head + Vec3{.2f, -.2f, -1};
            }
            pool.fire(1, wrist, {0, -.03f, -1}, e, 0, strong);
            enemy(world, e.list[0].feet, eye);
            hand(world, wrist, eye);
            appendRepulsorFlash(world, wrist + Vec3{0, 0, -.06f}, eye, pixelAngle, cell == 0 ? .8f : .45f,
                                cell == 0 ? .12f : .03f); // the palm
            Vec3 hitAt{};
            bool hit = false;
            for (int i = 0; i < 90 && pool.flying(); ++i) {
                const Step s = pool.update(1 / 90.f, e);
                const bool draw = cell == 0 ? i % 5 == 1 : i == 6;
                if (draw)
                    for (int b = 0; b < Pool::balls; ++b)
                        if (pool.ball(b).alive)
                            appendRepulsorBolt(world, pool.ball(b).at, pool.ball(b).tail(), eye, pixelAngle, .95f);
                for (int h = 0; h < s.hitCount; ++h) {
                    hit = true;
                    hitAt = s.hits[h].at;
                }
            }
            if (hit && cell == 0)
                appendRepulsorFlash(world, hitAt, eye, pixelAngle, .7f, .35f);
            if (cell == 0)
                label(world, "IRON MAN ABILITIES (REPULSOR)", "1 PRESS = 1 BLAST, NO WEB. STRONG AIM: HIT 20 DEGREES OFF",
                      Vec3{1.65f, 2.4f, -4.35f}, eye, {.55f, .85f, 1}, 9.f);
            else
                label(world, "FROM YOUR EYES", "THE BLAST ON ITS WAY, BENDING ONTO HIM", head + Vec3{.25f, .3f, -1.2f}, eye,
                      {.55f, .85f, 1});
            std::printf("cell %d: repulsor blast, hit %d, %zu vertices\n", cell, hit ? 1 : 0, world.size());
        } else {
            eye = head + Vec3{-1.3f, 1.f, 5.f}; // behind and above the player
            target = head + Vec3{.4f, -.8f, -9};
            Vec3 from, dir;
            repulsor::chestRay(head, {0, -.2f, -1}, from, dir);
            const Vec3 feet[3] = {{.3f, 0, -9}, {-.5f, 0, -16}, {4, 0, -10}};
            int hits[3]{};
            const int n = repulsor::beamHits(from, dir, feet, 3, hits, 3);
            for (const auto& f : feet)
                enemy(world, f, eye);
            // the player: a dark-red figure, both hands forward on the triggers
            soft::softBar(world, head - Vec3{0, 1.5f, 0}, head - Vec3{0, .15f, 0}, normalized(head - eye), .2f, {.35f, .05f, .05f}, 1, .002f);
            soft::softBar(world, head - Vec3{0, .1f, 0}, head + Vec3{0, .12f, 0}, normalized(head - eye), .12f, {.35f, .05f, .05f}, 1, .002f);
            hand(world, head + Vec3{-.2f, -.4f, -.4f}, eye);
            hand(world, head + Vec3{.2f, -.4f, -.4f}, eye);
            appendChestBeam(world, from, from + dir * 24.f, eye, pixelAngle, repulsor::beamFade(.12f));
            for (int i = 0; i < n; ++i)
                appendRepulsorFlash(world, feet[hits[i]] + Vec3{0, 1, 0}, eye, pixelAngle, .8f, .6f);
            repulsor::Trigger t;
            const bool press[2] = {true, true}, held[2] = {true, true};
            const Vec3 aim[2] = {dir, dir};
            t.update(0, press, aim, held);
            t.update(repulsor::beamHold, press, aim, held);
            t.update(repulsor::beamHold + .5f, press, aim, held);
            char line[32];
            repulsor::status(line, sizeof line, t);
            label(world, line, "ON THE WRIST, JUST AFTER", head + Vec3{.9f, .1f, -.4f}, eye, {.55f, .85f, 1}, 9.f);
            label(world, "CHEST BEAM - BOTH TRIGGERS HELD 0.35 S", "4X A BLAST, 30 S COOLDOWN, NEVER A WEB",
                  head + Vec3{0, 1.9f, -12}, eye, {.55f, .85f, 1});
            std::printf("cell 2: chest beam, %d of 3 enemies in it, status '%s', %zu vertices\n", n, line, world.size());
        }
        const Camera camera{1 / pixelAngle, size * .5f, height * .5f};
        draw(image, look(world, eye, target), camera, 0, 0, x0, 0, size, height);
    }
    return save(image, argv[1]) ? 0 : 1;
}
