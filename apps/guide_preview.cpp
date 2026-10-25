// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
//
// Version 0.0.2: draws TRAINING's path guide and the BALL LOOK OURS web ball without a headset, from
// the player's eyes, over a sky (made-up hand motions, not a recording). Six cells:
//   1 PUNCH, RED: idle, the start ring pulsing, the arrows and the ghost hand on the path
//   2 PUNCH, BLUE: the right hand half way along it (the followed part lighter)
//   3 PUNCH, SUCCESS (green)
//   4 PUNCH, FAIL: the hand left the path (orange, the reason under the name)
//   5 FLY STOP, a pose: both hands on its end rings, BLUE
//   6 OURS: one press, one ball from the right wrist bending (AIM STRENGTH STRONG) onto an enemy
//     20 degrees off the aim, drawn at 5 moments, and the splash where it hit
//
//   spidy_guide_preview <path.bmp>
#include "spidy/our_ball.hpp"
#include "spidy/pixel_font.hpp"
#include "spidy/soft_shapes.hpp"
#include "spidy/training_guide.hpp"
#include "spidy/web_visual.hpp"
#include "preview_image.hpp"
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>
using namespace spidy;
using namespace spidy::preview;

namespace {
void sky(Image& image, int x0, int size, int height) {
    for (int y = 0; y < height; ++y)
        for (int x = x0; x < x0 + size; ++x) {
            const float v = static_cast<float>(y) / static_cast<float>(height);
            float* o = image.at(x, y);
            const bool below = v > .70f;
            o[0] = decode(below ? .30f : .20f + .22f * v);
            o[1] = decode(below ? .32f : .26f + .20f * v);
            o[2] = decode(below ? .36f : .46f + .10f * v);
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
void hand(std::vector<Vertex>& out, Vec3 at, Vec3 eye, Vec3 color) {
    soft::softBar(out, at - Vec3{0, .015f, 0}, at + Vec3{0, .015f, 0}, normalized(at - eye), .03f, color, .9f, .01f);
}
training_guide::Draw drawOf(const training_guide::Guide& g, float pulse) {
    training_guide::Draw d;
    d.on = g.on();
    d.state = static_cast<uint8_t>(g.state());
    d.pose = g.path().pose;
    d.head = g.path().head;
    d.armed = g.armed();
    d.hands = g.path().hands;
    d.ghostAlpha = g.ghostAlpha();
    d.pulse = pulse;
    for (int h = 0; h < 2; ++h) {
        d.progress[h] = g.progress(h);
        d.ghost[h] = g.ghost(h);
        for (int k = 0; k < training_guide::points; ++k)
            d.p[h][k] = g.point(h, k);
    }
    return d;
}
void label(std::vector<Vertex>& out, const std::string& a, const std::string& b, Vec3 at, Vec3 eye, Vec3 color) {
    const Vec3 back = normalized(eye - at);
    const Vec3 right = normalized(cross(Vec3{0, 1, 0}, back));
    const Vec3 up = cross(back, right);
    const float k = length(at - eye) / .6f; // the same size on screen near or far
    PixelTextStyle s;
    s.height = .022f * k;
    s.top = s.bottom = color;
    appendPixelText(out, a, at - right * (pixelTextWidth(a, s) * .5f) + up * (.03f * k), right, up, s);
    s.height = .016f * k;
    s.top = s.bottom = Vec3{1, 1, 1};
    appendPixelText(out, b, at - right * (pixelTextWidth(b, s) * .5f), right, up, s);
}
Vec3 stateColor(uint32_t s) {
    return s == training_guide::blue ? Vec3{.3f, .6f, 1} : s == training_guide::success ? Vec3{.3f, 1, .4f}
         : s == training_guide::fail ? Vec3{1, .55f, .15f} : Vec3{1, .25f, .2f};
}
} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: spidy_guide_preview <path.bmp>\n");
        return 2;
    }
    constexpr float pixelAngle = .0016f;
    constexpr int size = 900, height = 900, cells = 6;
    Image image(size * cells, height);
    const Vec3 head{0, 1.6f, 0};
    for (int cell = 0; cell < cells; ++cell) {
        const int x0 = cell * size;
        sky(image, x0, size, height);
        std::vector<Vertex> world;
        Vec3 eye = head + Vec3{-.45f, .20f, .60f}, target = head + Vec3{.10f, -.30f, -.35f}; // just behind the player's left shoulder
        if (cell < 5) {
            using namespace training_guide;
            const int move = cell == 4 ? static_cast<int>(flyStop) : static_cast<int>(gestures385::abilityPunch);
            Guide g;
            g.select(move, pathFor(move, nullptr));
            training_guide::Input in;
            in.seconds = 1 / 90.f;
            in.live = true;
            in.head = head;
            in.look = {0, 0, -1};
            in.tracked[0] = in.tracked[1] = true;
            in.arm = .62f;
            in.hands[0] = {-.25f, 1.05f, -.15f};
            in.hands[1] = {.25f, 1.05f, -.15f};
            for (int i = 0; i < 30; ++i)
                g.update(in); // anchored, RED, the ghost riding
            if (cell == 1 || cell == 2 || cell == 3) {
                in.hands[1] = g.point(1, 0);
                g.update(in);
                const int upTo = cell == 1 ? points / 2 : cell == 2 ? points : 10;
                for (int i = 0; i < upTo; ++i) {
                    in.hands[1] = g.point(1, i);
                    g.update(in);
                }
                if (cell == 2)
                    g.result(true, "");
                if (cell == 3) {
                    in.hands[1] = in.hands[1] + Vec3{.35f, .30f, 0};
                    for (int i = 0; i < 30; ++i)
                        g.update(in);
                }
            }
            if (cell == 4) {
                in.hands[0] = g.point(0, points - 1);
                in.hands[1] = g.point(1, points - 1);
                g.update(in);
            }
            if (cell == 4) { // a pose: both hands - from straight behind
                eye = head + Vec3{0, .45f, 1.5f};
                target = head + Vec3{0, -.30f, -.30f};
            }
            const Draw d = drawOf(g, cell == 0 ? 1.f : .5f);
            appendTrainingGuide(world, d, {}, eye, pixelAngle);
            hand(world, in.hands[0], eye, {.7f, .1f, .1f});
            hand(world, in.hands[1], eye, {.7f, .1f, .1f});
            const std::string name = std::string(moveName(move));
            const std::string line = g.state() == fail ? std::string("FAIL: ") + g.reason() : g.instruction();
            label(world, name, line, head + Vec3{.05f, .02f, -.55f}, eye, stateColor(g.state()));
            std::printf("cell %d: %s %s, %zu vertices\n", cell, name.c_str(), stateName(g.state()), world.size());
        } else {
            using namespace our_ball;
            eye = {-4.3f, 2.6f, -6.6f}; // side on, so the bend shows
            target = {1.65f, 1.1f, -4.35f};
            Pool pool;
            Enemies e;
            const float r = 20 * .0174533f;
            e.list[0].feet = Vec3{std::sin(r) * 9, 0, -std::cos(r) * 9};
            e.list[0].id = 7;
            e.count = 1;
            const Vec3 wrist{.22f, 1.3f, -.25f};
            pool.fire(1, wrist, {0, -.03f, -1}, e, 0, strong);
            // the enemy: a dark figure
            const Vec3 f = e.list[0].feet;
            soft::softBar(world, f + Vec3{0, .1f, 0}, f + Vec3{0, 1.5f, 0}, normalized(f - eye), .22f, {.08f, .08f, .1f}, 1, .002f);
            soft::softBar(world, f + Vec3{0, 1.55f, 0}, f + Vec3{0, 1.8f, 0}, normalized(f - eye), .13f, {.08f, .08f, .1f}, 1, .002f);
            hand(world, wrist, eye, {.7f, .1f, .1f});
            soft::softBar(world, wrist, wrist + normalized(Vec3{0, -.03f, -1}) * 9.f, normalized(wrist - eye), .01f,
                          {1, 1, 1}, .25f, .01f); // where the hand aimed
            Splash splash;
            int frames = 0;
            for (int i = 0; i < 90 && pool.flying(); ++i) {
                const Step s = pool.update(1 / 90.f, e);
                ++frames;
                if (i % 6 == 2)
                    for (int b = 0; b < Pool::balls; ++b)
                        if (pool.ball(b).alive)
                            appendWebBall(world, pool.ball(b).at, pool.ball(b).tail(), eye, pixelAngle, .9f);
                for (int h = 0; h < s.hitCount; ++h)
                    splash.start(s.hits[h].at);
            }
            splash.update(.05f);
            if (splash.fade() > 0 && splash.age < Splash::life)
                appendWebSplash(world, splash.at, eye, pixelAngle, splash.fade(), 1.f);
            label(world, "OURS - AIM STRENGTH STRONG", "1 PRESS = 1 BALL, ENEMY 20 DEGREES OFF THE AIM: HIT", Vec3{1.65f, 2.5f, -4.35f}, eye,
                  {.95f, .95f, .95f});
            std::printf("cell 5: OURS ball, %llu fired, %llu hit after %d frames, %zu vertices\n", static_cast<unsigned long long>(pool.fired()),
                        static_cast<unsigned long long>(pool.hits()), frames, world.size());
        }
        const Camera camera{1 / pixelAngle, size * .5f, height * .5f};
        draw(image, look(world, eye, target), camera, 0, 0, x0, 0, size, height);
    }
    return save(image, argv[1]) ? 0 : 1;
}
