// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
//
// Draws the web (web_visual.hpp's appendWeb, shaded as the overlay's pixel
// shader shades it, web_shading.hpp) without a headset, at about the
// headset's pixel density, over a plain drawn city: one sample a pixel with
// the overlay's blending and depth test. Update 12's web is a tube in the
// world, so it is drawn from more than one place:
//   <prefix>-front.bmp   from the head: a web held at the hand rising to its
//                        anchor (left), webs across the street at 10 to 60 m
//                        (right)
//   <prefix>-side.bmp    the held web's first metre or so, from beside it and
//                        a little above (left) and from below (right)
//   <prefix>-stereo.bmp  the held web as the left and the right eye see it
//                        (64 mm apart), the left eye's image on the left
//   <prefix>-travel.bmp  Update 15: a 25 m shot at web_speed 180, eight
//                        images left to right at 0, 15, 30, 50, 75 and 100 ms
//                        (in flight; it arrives at 110 ms), 120 ms (taut,
//                        splat spreading) and 200 ms (settled)
//   <prefix>-travel-short.bmp  the same for a 5 m shot (arrives at 30 ms):
//                        0, 10, 20, 30, 45, 70, 100 and 200 ms
//
//   spidy_web_preview <prefix>
//
// Update 18: the web styles (web_visual.hpp web_style), a row each in its
// default shade: the held web from the head, the held web from beside its
// first metre, and the webs across the street; and WEB COLOR's grid, a row
// a style and a column a shade, the held web from beside:
//   spidy_web_preview <prefix> styles   -> <prefix>-styles.bmp
//   spidy_web_preview <prefix> shades   -> <prefix>-shades.bmp
// Update 20: the web leaving a right hand (a plain box glove and forearm on
// the grip, as the headset tracks it; the web from webShooter/webFromGrip,
// as game_xr draws it), a 2 x 2 sheet: MESSY from the head (top left), MESSY
// close from beside the wrist (top right), 90s from the same place (bottom
// left), MESSY up close against the sky, from below (bottom right):
//   spidy_web_preview <prefix> hand     -> <prefix>-hand.bmp
#include "spidy/tracking.hpp"
#include "spidy/web_visual.hpp"
#include "preview_image.hpp"
#include <cmath>
#include <cstdio>
#include <limits>
#include <string>
#include <vector>
using namespace spidy;
using namespace spidy::preview;

namespace {
float hash(int a, int b) {
    unsigned h = static_cast<unsigned>(a) * 73856093u ^ static_cast<unsigned>(b) * 19349663u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return static_cast<float>(h & 0xffffu) / 65535.f;
}
// Rows of towers in muted blues, greens and tans, each with window bands.
void city(Image& image, int x0, int y0, int w, int h) {
    for (int y = y0; y < y0 + h; ++y)
        for (int x = x0; x < x0 + w; ++x) {
            float* o = image.at(x, y);
            const float v = static_cast<float>(y - y0) / static_cast<float>(h);
            o[0] = decode(.55f - .2f * v);
            o[1] = decode(.62f - .2f * v);
            o[2] = decode(.72f - .15f * v);
        }
    for (int row = 0; row < 4; ++row)
        for (int i = 0; i < 14; ++i) {
            const float r = hash(row, i);
            const int bw = static_cast<int>(60 + 60 * r) + row * 20;
            const int bx = x0 + static_cast<int>((i + .3f * hash(i, row + 9)) * w / 11) - 40;
            const int by = y0 + static_cast<int>(h * (.05f + .2f * row + .15f * hash(row + 3, i)));
            const float tint[3][3] = {{.36f, .45f, .55f}, {.35f, .52f, .44f}, {.6f, .56f, .45f}};
            const float* t = tint[static_cast<int>(hash(i, row * 7) * 2.99f)];
            const float dim = .55f + .15f * row;
            for (int y = std::max(by, y0); y < y0 + h; ++y)
                for (int x = std::max(bx, x0); x < std::min(bx + bw, x0 + w); ++x) {
                    const bool window = ((y - by) % 14) < 6 && ((x - bx) % 12) > 3 && x - bx > 6 && bx + bw - x > 6;
                    const float k = (window ? .7f : 1.f) * dim;
                    float* o = image.at(x, y);
                    o[0] = decode(t[0] * k);
                    o[1] = decode(t[1] * k);
                    o[2] = decode(t[2] * k);
                }
        }
}

// An eye at `eye` looking at `target`, level: world positions into its
// space (looking along -z). Normals stay in the world, as the shader lights
// them there.
struct View {
    Vec3 eye, right, up, back;
    View(Vec3 at, Vec3 target) : eye(at) {
        back = normalized(at - target);
        right = cross(Vec3{0, 1, 0}, back);
        right = length(right) > 1e-3f ? normalized(right) : Vec3{1, 0, 0};
        up = cross(back, right);
    }
    std::vector<Vertex> look(const std::vector<Vertex>& world) const {
        auto out = world;
        for (auto& v : out) {
            const Vec3 d = v.position - eye;
            v.position = {dot(d, right), dot(d, up), dot(d, back)};
        }
        return out;
    }
};
void cell(Image& image, const std::vector<Vertex>& world, const View& view, float pixelAngle, int x0, int size) {
    city(image, x0, 0, size, size);
    std::vector<float> depth(static_cast<size_t>(image.w) * image.h, std::numeric_limits<float>::infinity());
    const Camera camera{1 / pixelAngle, size * .5f, size * .5f};
    draw(image, view.look(world), camera, 0, 0, x0, 0, size, size, &depth);
}

// A box (centre, three unit axes and half sizes), lit flat from the sky.
void box(std::vector<Vertex>& out, Vec3 c, const Vec3 (&axis)[3], Vec3 half, Vec3 color) {
    const Vec3 sky = normalized(Vec3{-.35f, 1, .45f});
    const float h[3] = {half.x, half.y, half.z};
    for (int a = 0; a < 3; ++a)
        for (float sign : {-1.f, 1.f}) {
            const Vec3 n = axis[a] * sign, u = axis[(a + 1) % 3] * h[(a + 1) % 3], v = axis[(a + 2) % 3] * h[(a + 2) % 3];
            const Vec3 f = c + n * h[a];
            const Vec3 lit = color * (.3f + .7f * std::max(0.f, dot(n, sky)));
            const Vertex p0{f - u - v, lit}, p1{f + u - v, lit}, p2{f + u + v, lit}, p3{f - u + v, lit};
            out.insert(out.end(), {p0, p1, p2, p0, p2, p3});
        }
}
// A right hand on `grip` (OpenXR's grip: -y past the knuckles, +x the back
// of the right hand, the forearm up +y tilted 30 degrees toward +z).
std::vector<Vertex> rightHand(Pose grip) {
    std::vector<Vertex> out;
    const auto at = [&](Vec3 local) { return grip.position + grip.orientation.rotate(local); };
    const auto dir = [&](Vec3 local) { return grip.orientation.rotate(normalized(local)); };
    const Vec3 x = dir({1, 0, 0}), y = dir({0, 1, 0}), z = dir({0, 0, 1});
    const Vec3 forearm = dir({0, .866f, .5f}), side = normalized(cross(forearm, x));
    const Vec3 red{.5f, .03f, .03f}, blue{.04f, .07f, .3f}, black{.04f, .04f, .05f};
    box(out, at({-.008f, -.005f, -.004f}), {x, y, z}, {.034f, .045f, .042f}, red);        // the fist
    box(out, at({-.024f, -.03f, -.046f}), {x, y, z}, {.012f, .024f, .011f}, red);         // the thumb
    const Vec3 wrist = at({.0f, .0f, .0f}) + forearm * .07f;
    box(out, wrist, {x, forearm, side}, {.03f, .03f, .03f}, red);                         // the wrist
    box(out, wrist + forearm * .14f, {x, forearm, side}, {.036f, .11f, .036f}, blue);      // the forearm
    // The web shooter's nozzle, where Update 20's web leaves.
    const Pose shooter = webShooter(grip, 1);
    box(out, shooter.position - x * .004f, {x, forearm, side}, {.006f, .009f, .009f}, black);
    return out;
}
bool write(const Image& image, const std::string& path) {
    if (save(image, path))
        return true;
    std::fprintf(stderr, "could not write %s\n", path.c_str());
    return false;
}
} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: spidy_web_preview <prefix>\n");
        return 2;
    }
    const std::string prefix = argv[1];
    constexpr int size = 1024;
    constexpr float pixelAngle = .00065f; // the headset's eye, about 2688 pixels across its view
    const Vec3 head{0, 0, 0};
    // Held: from the hand low and left, up past the head to the right.
    WebLine held;
    held.start = {-.16f, -.3f, -.55f};
    held.end = {14, 26, -30};
    held.splat = false;
    if (argc > 2) {
        const std::string mode = argv[2];
        if (mode != "styles" && mode != "shades" && mode != "hand" && mode != "fibres") {
            std::fprintf(stderr, "usage: spidy_web_preview <prefix> [styles|shades|hand|fibres]\n");
            return 2;
        }
        if (mode == "fibres") {
            // Update 24: each style's fibres, a row each: up close (0.3 m
            // off the rope, a quarter of a metre of it), the held web about
            // a metre out, and a web 4 m away; all at the headset's pixels
            // (the last two drawn small and blown up 2x, pixel for pixel).
            constexpr int tile = 512, small = 256;
            Image sheet(3 * tile, web_style::count * tile);
            const Vec3 dir = normalized(held.end - held.start);
            const Vec3 side = normalized(cross(dir, Vec3{0, 1, 0}));
            const Vec3 close = held.start + dir * .8f;
            const Vec3 closeEye = close + side * .3f + Vec3{0, .04f, 0};
            const Vec3 metre = held.start + dir * 1.1f;
            WebLine across;
            across.start = {-2.2f, -.6f, -4};
            across.end = {2.6f, .9f, -4.4f};
            across.splat = false;
            across.seed = 1.7f;
            for (int row = 0; row < web_style::count; ++row) {
                WebLook look;
                look.style = row;
                look.seconds = 1.35f;
                const auto render = [&](int column, const WebLine& web, Vec3 eye, Vec3 target, float angle, int px) {
                    std::vector<Vertex> world;
                    appendWeb(world, web, eye, pixelAngle, look);
                    Image cellImage(px, px);
                    city(cellImage, 0, 0, px, px);
                    std::vector<float> depth(static_cast<size_t>(px) * px, std::numeric_limits<float>::infinity());
                    const Camera camera{1 / angle, px * .5f, px * .5f};
                    draw(cellImage, View(eye, target).look(world), camera, 0, 0, 0, 0, px, px, &depth);
                    const int scale = tile / px;
                    for (int y = 0; y < tile; ++y)
                        for (int x = 0; x < tile; ++x) {
                            const float* from = cellImage.at(x / scale, y / scale);
                            float* to = sheet.at(column * tile + x, row * tile + y);
                            to[0] = from[0], to[1] = from[1], to[2] = from[2];
                        }
                };
                // Up close: the eye's own pixels a little enlarged (0.3 m off).
                render(0, held, closeEye, close, pixelAngle * .5f, tile);
                render(1, held, head, metre, pixelAngle, small);
                render(2, across, head, Vec3{0, .1f, -4.2f}, pixelAngle, small);
            }
            return write(sheet, prefix + "-fibres.bmp") ? 0 : 1;
        }
        if (mode == "hand") {
            constexpr int tile = 768;
            // The right hand reaching forward and up, the web to a ledge.
            const Vec3 hand{.2f, -.22f, -.45f}, anchor{5, 12, -24};
            const Vec3 aim = normalized(anchor - hand);
            Quat q = arc({0, -1, 0}, aim);
            const Vec3 back = q.rotate({1, 0, 0});
            Vec3 want = Vec3{1, 0, 0} - aim * dot(Vec3{1, 0, 0}, aim);
            want = normalized(want);
            q = Quat::around(aim, std::atan2(dot(cross(back, want), aim), dot(back, want))) * q;
            const Pose grip{hand, q};
            const Vec3 shooter = webShooter(grip, 1).position;
            WebLine web;
            web.end = anchor;
            web.start = webFromGrip(shooter, grip.position, anchor);
            web.slack = .4f;
            web.seed = .6f;
            web.splat = false;
            const std::vector<Vertex> glove = rightHand(grip);
            Image sheet(2 * tile, 2 * tile);
            const auto at = [&](int column, int row, int style, Vec3 eye, Vec3 target, float angle) {
                WebLook look;
                look.style = style;
                look.seconds = 1.35f;
                std::vector<Vertex> world = glove;
                appendWeb(world, web, eye, angle, look);
                const View view(eye, target);
                city(sheet, column * tile, row * tile, tile, tile);
                std::vector<float> depth(static_cast<size_t>(sheet.w) * sheet.h,
                                         std::numeric_limits<float>::infinity());
                const Camera camera{1 / angle, tile * .5f, tile * .5f};
                draw(sheet, view.look(world), camera, 0, 0, column * tile, row * tile, tile, tile, &depth);
            };
            const float headAngle = pixelAngle * size / tile;
            at(0, 0, web_style::messy, head, hand + aim * .3f, headAngle * 1.3f);
            // From the right, square to the aim, the fist and the forearm in view.
            const Vec3 square = normalized(cross(aim, Vec3{0, 1, 0})) * -1.f;
            const Vec3 beside = hand + square * .5f + Vec3{0, -.08f, 0} - aim * .02f;
            const Vec3 look = hand - aim * .06f + aim * .1f;
            at(1, 0, web_style::messy, beside, look, headAngle * 1.1f);
            at(0, 1, web_style::nineties, beside, look, headAngle * 1.1f);
            // Up close, 0.6-0.9 m out: the strand itself (its drawn middle there).
            Vec3 strand = web.start + normalized(web.end - web.start) * .75f;
            {
                WebLook messyLook;
                messyLook.style = web_style::messy;
                std::vector<Vertex> probe;
                appendWeb(probe, web, head, headAngle, messyLook);
                Vec3 sum{};
                int n = 0;
                for (const Vertex& v : probe) {
                    const float d = dot(v.position - web.start, normalized(web.end - web.start));
                    if (d > .6f && d < .9f) {
                        sum = sum + v.position;
                        ++n;
                    }
                }
                if (n)
                    strand = sum * (1.f / static_cast<float>(n));
            }
            at(1, 1, web_style::messy, strand + square * .3f, strand, headAngle * .3f);
            return write(sheet, prefix + "-hand.bmp") ? 0 : 1;
        }
        if (mode == "shades") {
            constexpr int tile = 320;
            const float wide = pixelAngle * size / tile * .3f; // zoomed: the rope's own look
            Image grid(web_style::maxShades * tile, web_style::count * tile);
            const Vec3 along = held.start + normalized(held.end - held.start) * .5f;
            const Vec3 beside = along + Vec3{.45f, .15f, .12f};
            const View view(beside, along);
            for (int row = 0; row < web_style::count; ++row)
                for (int column = 0; column < web_style::maxShades; ++column) {
                    if (column >= web_style::shades(row)) { // no such shade: a plain dark tile
                        for (int y = row * tile; y < (row + 1) * tile; ++y)
                            for (int x = column * tile; x < (column + 1) * tile; ++x) {
                                float* o = grid.at(x, y);
                                o[0] = o[1] = o[2] = decode(.12f);
                            }
                        continue;
                    }
                    WebLook look;
                    look.style = row;
                    look.color = column;
                    look.seconds = 1.35f;
                    std::vector<Vertex> web;
                    appendWeb(web, held, beside, wide, look);
                    city(grid, column * tile, row * tile, tile, tile);
                    std::vector<float> depth(static_cast<size_t>(grid.w) * grid.h,
                                             std::numeric_limits<float>::infinity());
                    const Camera camera{1 / wide, tile * .5f, tile * .5f};
                    draw(grid, view.look(web), camera, 0, 0, column * tile, row * tile, tile, tile, &depth);
                }
            return write(grid, prefix + "-shades.bmp") ? 0 : 1;
        }
        const int rows = web_style::count;
        constexpr int tile = 448;
        // The same view as the full size images, a tile's pixels each wider;
        // the far webs at the headset's own pixels, so their floor shows true.
        const float wide = pixelAngle * size / tile;
        Image sheet(3 * tile, rows * tile);
        const Vec3 along = held.start + normalized(held.end - held.start) * .5f;
        const Vec3 beside = along + Vec3{.75f, .25f, .2f};
        for (int row = 0; row < rows; ++row) {
            WebLook look;
            look.style = row;
            look.seconds = 1.35f; // a moment with MESSY's glints lit
            std::vector<Vertex> a, b, c;
            appendWeb(a, held, head, wide, look);
            const auto at = [&](int column, const std::vector<Vertex>& world, const View& view, float angle) {
                city(sheet, column * tile, row * tile, tile, tile);
                std::vector<float> depth(static_cast<size_t>(sheet.w) * sheet.h,
                                         std::numeric_limits<float>::infinity());
                const Camera camera{1 / angle, tile * .5f, tile * .5f};
                draw(sheet, view.look(world), camera, 0, 0, column * tile, row * tile, tile, tile, &depth);
            };
            at(0, a, View(head, {0, 0, -1}), wide);
            appendWeb(b, held, beside, wide, look);
            at(1, b, View(beside, along), wide);
            // Across the street at 10 to 40 m, at the headset's pixels:
            // the middle of the street view, full size.
            for (int i = 0; i < 3; ++i) {
                WebLine web;
                const float z = -10.f - 14.f * static_cast<float>(i);
                web.start = {-.12f * -z, (-.12f + .1f * i) * -z, z};
                web.end = {.12f * -z, (.14f - .1f * i) * -z, z - 3};
                web.slack = i == 1 ? 1.5f : 0;
                web.seed = static_cast<float>(i);
                appendWeb(c, web, head, pixelAngle, look);
            }
            at(2, c, View(head, {0, 0, -1}), pixelAngle);
        }
        return write(sheet, prefix + "-styles.bmp") ? 0 : 1;
    }
    std::vector<Vertex> heldWeb;
    appendWeb(heldWeb, held, head, pixelAngle);
    const View ahead(head, {0, 0, -1});
    Image front(2 * size, size);
    cell(front, heldWeb, ahead, pixelAngle, 0, size);
    // Across the street, 10 to 60 m off, one slack.
    std::vector<Vertex> far;
    for (int i = 0; i < 4; ++i) {
        WebLine web;
        const float z = -10.f - 16.f * static_cast<float>(i);
        web.start = {-.4f * -z, (-.3f + .12f * i) * -z, z};
        web.end = {.45f * -z, (.32f - .1f * i) * -z, z - 4};
        web.slack = i == 1 ? 1.5f : 0;
        web.seed = static_cast<float>(i);
        appendWeb(far, web, head, pixelAngle);
    }
    cell(front, far, ahead, pixelAngle, size, size);
    if (!write(front, prefix + "-front.bmp"))
        return 1;
    // From beside the rope's first metre, and from under it. Each is built
    // for its own eye, as the overlay builds its webs for the viewer.
    const Vec3 along = held.start + normalized(held.end - held.start) * .5f;
    Image side(2 * size, size);
    const Vec3 beside = along + Vec3{.75f, .25f, .2f};
    std::vector<Vertex> fromBeside;
    appendWeb(fromBeside, held, beside, pixelAngle);
    cell(side, fromBeside, View(beside, along), pixelAngle, 0, size);
    const Vec3 below = along + Vec3{-.15f, -.6f, .35f};
    std::vector<Vertex> fromBelow;
    appendWeb(fromBelow, held, below, pixelAngle);
    cell(side, fromBelow, View(below, along), pixelAngle, size, size);
    if (!write(side, prefix + "-side.bmp"))
        return 1;
    // One web for both eyes, as in the headset (built for the point between
    // them), each eye drawing it from where it is.
    Image stereo(2 * size, size);
    const Vec3 target = held.start + normalized(held.end - held.start) * .7f;
    for (int eye = 0; eye < 2; ++eye) {
        const View straight(head, target);
        const Vec3 at = head + straight.right * (eye ? .032f : -.032f);
        View view = straight;
        view.eye = at;
        cell(stereo, heldWeb, view, pixelAngle, eye * size, size);
    }
    if (!write(stereo, prefix + "-stereo.bmp"))
        return 1;
    // Update 15: the shot in flight, as the overlay animates it (game_xr.cpp:
    // WebAnimation::flight at 90 images a second, slack while flying).
    auto travel = [&](Vec3 anchor, const float (&times)[8], const std::string& name) {
        constexpr int small = 512;
        constexpr float wide = pixelAngle * 3;
        Image strip(8 * small, small);
        const Vec3 palm{.18f, -.25f, -.45f};
        const float distance = length(anchor - palm);
        const View view(head, head + normalized(palm - head) + normalized(anchor - head)); // both ends in view
        for (int i = 0; i < 8; ++i) {
            WebLine shot;
            shot.start = palm;
            shot.end = anchor;
            shot.seed = 3;
            const auto f = WebAnimation::flight(times[i], distance, WebTravel{}, 1 / 90.f);
            shot.extended = f.extended;
            shot.wave = f.wave;
            shot.wavePhase = f.wavePhase;
            shot.impact = f.impact;
            if (f.flying)
                shot.slack = distance * .0015f;
            std::vector<Vertex> web;
            appendWeb(web, shot, head, wide);
            cell(strip, web, view, wide, i * small, small);
        }
        return write(strip, prefix + name);
    };
    const float longTimes[8] = {0, .015f, .03f, .05f, .075f, .1f, .12f, .2f};
    const float shortTimes[8] = {0, .01f, .02f, .03f, .045f, .07f, .1f, .2f};
    if (!travel({7, 5, -23}, longTimes, "-travel.bmp") || !travel({1.2f, 2.2f, -4.2f}, shortTimes, "-travel-short.bmp"))
        return 1;
    return 0;
}
