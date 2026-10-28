// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
// The wrist hologram (Update 14): wrist_hud.hpp.
#ifndef SPIDY_TEST_COMMIT
#define SPIDY_TEST_COMMIT "unknown"
#endif
#ifndef SPIDY_TEST_NUMBER
#define SPIDY_TEST_NUMBER "0"
#endif
#include "spidy/wrist_hud.hpp"
#include "spidy/version.hpp"
#include "spidy/extras385.hpp"
#include "spidy/gestures385.hpp"
#include "spidy/gesture_session.hpp"
#include "spidy/blood_splatter.hpp"
#include "spidy/combat_input.hpp"
#include "spidy/combat38.hpp"
#include "spidy/hero_modes.hpp"
#include "spidy/mirror_view.hpp"
#include "spidy/npc_fade.hpp"
#include "spidy/controls_guide.hpp"
#include "spidy/native_view.hpp"
#include "spidy/overlay_text.hpp"
#include "spidy/pixel_font.hpp"
#include "spidy/shadow_camera.hpp"
#include "spidy/shadow_distance.hpp"
#include "spidy/eye_taa.hpp"
#include "spidy/engine_taa.hpp"
#include "spidy/far_shadows.hpp"
#include "spidy/eye_bloom.hpp"
#include "spidy/eye_color.hpp"
#include "spidy/foveation.hpp"
#include "spidy/shared_lod.hpp"
#include "spidy/soft_shapes.hpp"
#include "spidy/tracking.hpp"
#include "spidy/web_visual.hpp"
#include <algorithm>
#include <atomic>
#include <initializer_list>
#include <cmath>
#include <cstdio>
#include <string>
#include <string_view>

static_assert(spidy::wrist_hud::combatControls == static_cast<int>(spidy::combat38::options),
              "a COMBAT row per combat38 option");

namespace spidy::wrist_hud {
namespace {
constexpr float pi = 3.14159265f;

// Draws in the panel's own measures: u to its right and v up from its
// middle, metres at scale 1.
// Update 17: in the theme's colours (c), and v shifted up by dv (a page's
// scroll) for what scrolls.
struct Painter {
    std::vector<Vertex>& out;
    Frame f;
    float s, soft;
    Vec3 normal;
    const Palette& c;
    float dv = 0;
    // Update 36: the "2000s Raimi" theme's pixel font (pixel_font.hpp) instead of the strokes.
    bool pixel = false;
    // Seconds on any clock, for the Raimi glow's faint flicker.
    float seconds = 0;
    Vec3 at(float u, float v) const {
        return f.centre + f.right * (u * s) + f.up * ((v + dv) * s);
    }
    void bar(float u0, float v0, float u1, float v1, float width, Vec3 color, float alpha) const {
        soft::softBar(out, at(u0, v0), at(u1, v1), normal, width * s / 2, color, alpha, soft);
    }
    // A line with a faint glow about it.
    void glowLine(float u0, float v0, float u1, float v1, Vec3 color, float width = .0011f) const {
        bar(u0, v0, u1, v1, width * 3.2f, color, .13f);
        bar(u0, v0, u1, v1, width, color, .9f);
    }
    void rect(float u0, float v0, float u1, float v1, Vec3 color, float alpha) const {
        soft::quad(out, {at(u0, v0), color, alpha}, {at(u1, v0), color, alpha}, {at(u1, v1), color, alpha},
                   {at(u0, v1), color, alpha});
    }
    // A ring (inner 0: a disc), squeezed to rx across by ry up.
    void ring(float u, float v, float rx, float ry, float inner, float outer, Vec3 color, float alpha,
              int segments = 48) const {
        soft::softArc(out, at(u, v), f.right * (rx / ry), f.up, inner * s, outer * s, 0, 2 * pi, color, alpha,
                      soft, segments);
    }
    void triangle(float u0, float v0, float u1, float v1, float u2, float v2, Vec3 color, float alpha) const {
        soft::softTriangle(out, {at(u0, v0), at(u1, v1), at(u2, v2)}, normal, color, alpha, 0, soft);
    }
    void text(const std::string& t, float u, float v, float height, Vec3 color) const {
        if (pixel) {
            pixelText(t, u, v, height, color);
            return;
        }
        appendText(out, t, at(u, v), f.right, f.up, TextStyle{height * s, .15f, color, false, soft});
    }
    // Update 36: the pixel font at the stroke font's width (textWidth), so
    // every layout, centring and overlap check holds; its pixels a little
    // narrower or wider than tall to fit. Warm colours shade from a lighter
    // yellow at the top to orange at the bottom, as the handheld game's did.
    void pixelText(const std::string& t, float u, float v, float height, Vec3 color) const {
        const int columns = pixelTextColumns(t);
        const float want = textWidth(t, height);
        if (columns <= 0 || !(want > 0) || !(height > 0))
            return;
        PixelTextStyle style;
        style.height = height * s;
        const float natural = static_cast<float>(columns) * height / 7;
        style.xScale = std::clamp(want / natural, .7f, 1.3f);
        const float lead = (want - natural * style.xScale) / 2;
        const bool warm = color.x > color.z + .3f && color.x > .5f;
        style.top = warm ? Vec3{std::min(1.f, color.x * 1.05f), std::min(1.f, color.y * 1.35f + .06f), color.z} : color;
        style.bottom = warm ? Vec3{color.x, color.y * .55f, color.z * .5f} : color * .85f;
        style.outline = Vec3{0, .0006f, .004f};
        textGlow(u + lead, v, natural * style.xScale, height, color);
        appendPixelText(out, t, at(u + lead, v), f.right, f.up, style);
    }
    // Update 36: the Raimi theme's hologram glow behind a line of text: a
    // soft haze of its colour, strongest behind the letters, fading to
    // nothing a little past them (9 quads, alpha only; no blur pass).
    void textGlow(float u, float v, float width, float height, Vec3 color) const {
        const float a = .15f * flicker();
        const float us[4] = {u - height * .7f, u + height * .15f, u + width - height * .15f, u + width + height * .7f};
        const float vs[4] = {v - height * .55f, v + height * .2f, v + height * .8f, v + height * 1.55f};
        const Vec3 haze = color * .8f + Vec3{0, .1f, .35f} * .2f;
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j) {
                const auto alpha = [&](int x, int y) { return x >= 1 && x <= 2 && y >= 1 && y <= 2 ? a : 0.f; };
                soft::quad(out, {at(us[i], vs[j]), haze, alpha(i, j)}, {at(us[i + 1], vs[j]), haze, alpha(i + 1, j)},
                           {at(us[i + 1], vs[j + 1]), haze, alpha(i + 1, j + 1)},
                           {at(us[i], vs[j + 1]), haze, alpha(i, j + 1)});
            }
    }
    // The glow's flicker: a slow breath and a faint quick shimmer, 0.9..1.
    float flicker() const {
        const float t = std::isfinite(seconds) ? std::fmod(seconds, 600.f) : 0.f;
        return .95f + .035f * std::sin(t * 2.1f) + .015f * std::sin(t * 23.f);
    }
    // Update 19: hard-edged shapes for the visualizers (no soft rims: a
    // few thousand vertices a frame at most).
    void flat(float u0, float v0, float u1, float v1, float u2, float v2, Vec3 color, float alpha) const {
        out.insert(out.end(), {Vertex{at(u0, v0), color, alpha}, Vertex{at(u1, v1), color, alpha},
                               Vertex{at(u2, v2), color, alpha}});
    }
    void flatQuad(float u0, float v0, float u1, float v1, float u2, float v2, float u3, float v3, Vec3 color,
                  float alpha) const {
        soft::quad(out, {at(u0, v0), color, alpha}, {at(u1, v1), color, alpha}, {at(u2, v2), color, alpha},
                   {at(u3, v3), color, alpha});
    }
    void line(float u0, float v0, float u1, float v1, float width, Vec3 color, float alpha) const {
        const float du = u1 - u0, dv_ = v1 - v0, n = std::sqrt(du * du + dv_ * dv_);
        if (!(n > 1e-7f))
            return;
        const float x = -dv_ / n * width / 2, y = du / n * width / 2;
        flatQuad(u0 + x, v0 + y, u1 + x, v1 + y, u1 - x, v1 - y, u0 - x, v0 - y, color, alpha);
    }
};

// The panel's outline: 18 by 22 cm (Update 17: every page; `top` is halfH), its corners cut.
constexpr float halfW = panelWidth / 2, halfH = panelHeight / 2, cut = .016f;
std::array<std::array<float, 2>, 8> outlineOf(float top) {
    return {{{-halfW + cut, top},
             {halfW - cut, top},
             {halfW, top - cut},
             {halfW, -halfH + cut},
             {halfW - cut, -halfH},
             {-halfW + cut, -halfH},
             {-halfW, -halfH + cut},
             {-halfW, top - cut}}};
}

void backing(const Painter& p, float top) {
    const auto outline = outlineOf(top);
    // Dark glass, so the lines read over a bright sky.
    for (size_t i = 0; i < outline.size(); ++i) {
        const auto& a = outline[i];
        const auto& b = outline[(i + 1) % outline.size()];
        const Vertex c{p.at(0, 0), p.c.backing, p.c.backingAlpha};
        p.out.insert(p.out.end(), {c, {p.at(a[0], a[1]), p.c.backing, p.c.backingAlpha}, {p.at(b[0], b[1]), p.c.backing, p.c.backingAlpha}});
    }
    // Scanlines, faint.
    for (float v = -halfH + .003f; v < top - .002f; v += .003f) {
        const float over = std::max(0.f, v > 0 ? v - (top - cut) : -v - (halfH - cut));
        const float h = halfW - over - .003f;
        p.rect(-h, v, h, v + .0006f, p.c.line, .045f);
    }
}

// Update 36: the 2000s Raimi theme's watermark, a faint glowing blue spider
// of our own drawing (body, head and eight bent legs) behind the page.
void spiderMark(const Painter& p, float u, float v, float size) {
    const Vec3 blue = p.c.accent;
    p.ring(u, v + size * .05f, 1, 1, 0, size * .55f, blue, .035f, 32); // the glow about it
    p.ring(u, v + size * .05f, 1, 1, 0, size * .32f, blue, .04f, 32);
    p.ring(u, v + size * .30f, .8f, 1, 0, size * .075f, blue, .17f, 20);        // head
    p.ring(u, v + size * .15f, .75f, 1, 0, size * .12f, blue, .17f, 24);        // thorax
    p.ring(u, v - size * .14f, .72f, 1, 0, size * .2f, blue, .15f, 28);          // abdomen
    // Each leg: from the thorax out to a knee, then on to the foot.
    constexpr float legs[4][4] = {{.36f, .42f, .40f, .62f},   // front: out and up, then up
                                  {.44f, .24f, .62f, .30f},   // out, a little up
                                  {.42f, .02f, .60f, -.12f},  // out, a little down
                                  {.32f, -.22f, .38f, -.52f}}; // back: out and down, then down
    for (const float side : {-1.f, 1.f})
        for (int i = 0; i < 4; ++i) {
            const float u0 = u + side * size * .06f, v0 = v + size * (.2f - .035f * static_cast<float>(i));
            const float u1 = u + side * size * legs[i][0], v1 = v + size * legs[i][1];
            const float u2 = u + side * size * legs[i][2], v2 = v + size * legs[i][3];
            p.bar(u0, v0, u1, v1, size * .07f, blue, .05f);
            p.bar(u1, v1, u2, v2, size * .06f, blue, .05f);
            p.bar(u0, v0, u1, v1, size * .026f, blue, .2f);
            p.bar(u1, v1, u2, v2, size * .018f, blue, .2f);
        }
}

// Update 36: the Raimi theme's hologram halo, a soft blue bloom about the
// panel's edges: wide faint bands under the glass (most of their inner half
// hidden by it), so the panel seems to shine. About 40 quads.
void raimiHalo(const Painter& p, float top) {
    const auto outline = outlineOf(top);
    const Vec3 blue{.12f, .42f, 1};
    const float k = p.flicker();
    constexpr float widths[] = {.042f, .026f, .014f}, alphas[] = {.035f, .06f, .1f};
    for (int layer = 0; layer < 3; ++layer)
        for (size_t i = 0; i < outline.size(); ++i) {
            const auto& a = outline[i];
            const auto& b = outline[(i + 1) % outline.size()];
            p.bar(a[0], a[1], b[0], b[1], widths[layer], blue, alphas[layer] * k);
        }
}

void frame(const Painter& p, float top) {
    const auto outline = outlineOf(top);
    if (p.pixel) // Update 36: the Raimi edge glows brighter, a hologram's rim.
        for (size_t i = 0; i < outline.size(); ++i) {
            const auto& a = outline[i];
            const auto& b = outline[(i + 1) % outline.size()];
            p.bar(a[0], a[1], b[0], b[1], .013f, p.c.frame, .09f * p.flicker());
        }
    for (size_t i = 0; i < outline.size(); ++i) {
        const auto& a = outline[i];
        const auto& b = outline[(i + 1) % outline.size()];
        p.bar(a[0], a[1], b[0], b[1], .007f, p.c.frame, .12f); // glow
        p.bar(a[0], a[1], b[0], b[1], .0017f, p.c.frame, .95f);
    }
    // An inner accent line, and frame-coloured brackets at the cut corners.
    const float in = .005f;
    for (size_t i = 0; i < outline.size(); ++i) {
        const auto& a = outline[i];
        const auto& b = outline[(i + 1) % outline.size()];
        const auto inward = [&](const std::array<float, 2>& q) {
            return std::array<float, 2>{q[0] - std::copysign(in, q[0]), q[1] - std::copysign(in, q[1])};
        };
        const auto ia = inward(a), ib = inward(b);
        p.bar(ia[0], ia[1], ib[0], ib[1], .0006f, p.c.accent, .35f);
    }
    // A tick inside each cut corner.
    for (const float x : {-1.f, 1.f})
        for (const float y : {-1.f, 1.f})
        {
            const float edge = y > 0 ? top : halfH;
            p.bar(x * (halfW - cut + .002f), y * (edge - .0045f), x * (halfW - .0045f), y * (edge - cut + .002f),
                  .0012f, p.c.frame, .75f);
        }
    // The spider on the top edge, on a dark disc.
    const float ev = top;
    p.ring(0, ev, 1, 1, 0, .0125f, p.c.backing, .95f);
    p.ring(0, ev, 1, 1, .011f, .0128f, p.c.frame, .95f);
    p.ring(0, ev - .0012f, .0028f, .0042f, 0, .0042f, p.c.frame, 1, 24);
    p.ring(0, ev + .0047f, 1, 1, 0, .0021f, p.c.frame, 1, 16);
    for (const float side : {-1.f, 1.f})
        for (int leg = 0; leg < 4; ++leg) {
            const float v0 = ev + .0035f - leg * .0022f;
            const float knee = v0 + (leg < 2 ? .0028f : -.0010f);
            const float footV = knee + (leg < 2 ? .0012f : -.0034f);
            p.bar(side * .002f, v0, side * .0055f, knee, .0007f, p.c.frame, 1);
            p.bar(side * .0055f, knee, side * .0078f, footV, .0007f, p.c.frame, 1);
        }
}

// A standing Spider-Man in accent-coloured wire, about 15 cm tall, its middle at (cu, cv).
void figure(const Painter& p, float cu, float cv) {
    // The right side's outline from the neck round to the crotch; the left mirrors it.
    const std::array<std::array<float, 2>, 19> side{{{.006f, .042f},
                                                     {.022f, .037f},
                                                     {.028f, .030f},
                                                     {.031f, .006f},
                                                     {.032f, -.019f},
                                                     {.030f, -.029f},
                                                     {.024f, -.027f},
                                                     {.024f, -.017f},
                                                     {.021f, .004f},
                                                     {.018f, .020f},
                                                     {.014f, -.002f},
                                                     {.017f, -.020f},
                                                     {.016f, -.050f},
                                                     {.013f, -.077f},
                                                     {.017f, -.085f},
                                                     {.004f, -.085f},
                                                     {.005f, -.077f},
                                                     {.005f, -.050f},
                                                     {0, -.026f}}};
    for (const float m : {-1.f, 1.f})
        for (size_t i = 0; i + 1 < side.size(); ++i)
            p.glowLine(cu + m * side[i][0], cv + side[i][1], cu + m * side[i + 1][0], cv + side[i + 1][1], p.c.accent);
    p.glowLine(cu - .006f, cv + .042f, cu + .006f, cv + .042f, p.c.accent);
    // The head, its eyes, and a web line or two across it.
    p.ring(cu, cv + .056f, .0105f, .013f, .0118f, .0130f, p.c.accent, .9f);
    p.ring(cu, cv + .056f, .0105f, .013f, .0105f, .0150f, p.c.accent, .12f);
    for (const float m : {-1.f, 1.f})
        p.triangle(cu + m * .0016f, cv + .056f, cu + m * .0078f, cv + .0605f, cu + m * .0060f, cv + .0525f, p.c.text,
                   .85f);
    p.bar(cu, cv + .069f, cu, cv + .044f, .0005f, p.c.accent, .4f);
    p.bar(cu - .009f, cv + .051f, cu + .009f, cv + .051f, .0005f, p.c.accent, .35f);
    // The suit's lines: chest, the belt, the knees, the arms' web pattern.
    for (const float m : {-1.f, 1.f}) {
        p.bar(cu, cv + .026f, cu + m * .016f, cv + .033f, .0006f, p.c.accent, .5f);
        p.bar(cu + m * .016f, cv + .033f, cu + m * .019f, cv + .018f, .0006f, p.c.accent, .5f);
        p.bar(cu + m * .0105f, cv - .050f, cu + m * .0055f, cv - .050f, .0006f, p.c.accent, .45f);
        p.bar(cu + m * .025f, cv + .020f, cu + m * .027f, cv + .002f, .0005f, p.c.accent, .35f);
        p.bar(cu + m * .010f, cv - .030f, cu + m * .010f, cv - .070f, .0005f, p.c.accent, .3f);
    }
    p.bar(cu - .014f, cv - .004f, cu + .014f, cv - .004f, .0007f, p.c.accent, .55f);
    p.bar(cu, cv + .040f, cu, cv - .024f, .0005f, p.c.accent, .35f);
    // The spider on the chest, in the frame's colour.
    p.ring(cu, cv + .020f, .0022f, .0034f, 0, .0034f, p.c.frame, .95f, 16);
    for (const float m : {-1.f, 1.f}) {
        p.bar(cu, cv + .021f, cu + m * .0065f, cv + .026f, .0005f, p.c.frame, .9f);
        p.bar(cu, cv + .019f, cu + m * .0065f, cv + .013f, .0005f, p.c.frame, .9f);
    }
    // The light he stands in.
    p.ring(cu, cv - .086f, .026f, .005f, .0040f, .0050f, p.c.line, .6f);
    p.ring(cu, cv - .086f, .026f, .005f, 0, .0050f, p.c.accent, .12f);
}

void icon(const Painter& p, Gadget g, float u, float v) {
    // Its box, the corners cut as the panel's are.
    const float h = .0085f, c = .003f;
    const float box[6][2] = {{-h + c, h}, {h, h}, {h, -h + c}, {h - c, -h}, {-h, -h}, {-h, h - c}};
    for (int i = 0; i < 6; ++i)
        p.bar(u + box[i][0], v + box[i][1], u + box[(i + 1) % 6][0], v + box[(i + 1) % 6][1], .0007f, p.c.accent, .8f);
    p.rect(u - h, v - h, u + h, v + h, p.c.accent, .1f);
    switch (g) {
    case Gadget::impactWeb:
    case Gadget::webShooter:
    case Gadget::gameWebShooter:
        // A small web: spokes and a ring.
        for (int k = 0; k < 4; ++k) {
            const float a = k * pi / 4;
            p.bar(u - std::cos(a) * .0055f, v - std::sin(a) * .0055f, u + std::cos(a) * .0055f,
                  v + std::sin(a) * .0055f, .0006f, p.c.line, .95f);
        }
        p.ring(u, v, 1, 1, .0031f, .0039f, p.c.line, .95f, 24);
        break;
    case Gadget::webBomb:
    case Gadget::concussionBlast:
        // A ball with its burst.
        p.ring(u, v, 1, 1, 0, .0033f, p.c.line, .95f, 24);
        for (int k = 0; k < 8; ++k) {
            const float a = k * pi / 4;
            p.bar(u + std::cos(a) * .0045f, v + std::sin(a) * .0045f, u + std::cos(a) * .0062f,
                  v + std::sin(a) * .0062f, .0006f, p.c.line, .95f);
        }
        break;
    case Gadget::tripMine:
        // A diamond with its eye.
        p.bar(u, v + .006f, u + .006f, v, .0007f, p.c.line, .95f);
        p.bar(u + .006f, v, u, v - .006f, .0007f, p.c.line, .95f);
        p.bar(u, v - .006f, u - .006f, v, .0007f, p.c.line, .95f);
        p.bar(u - .006f, v, u, v + .006f, .0007f, p.c.line, .95f);
        p.ring(u, v, 1, 1, 0, .0018f, p.c.frame, 1, 16);
        break;
    default:
        p.ring(u, v, 1, 1, .0035f, .0045f, p.c.line, .95f, 24);
        p.ring(u, v, 1, 1, 0, .0015f, p.c.line, .95f, 16);
        break;
    }
}

void readouts(const Painter& p, const WristHudData& d) {
    const float u0 = -.012f, u1 = .082f;
    // HEALTH, its bar in the frame's colour, and how much in numbers.
    p.text("HEALTH", u0, .071f, .0062f, p.c.text);
    const float health = std::isfinite(d.health) ? std::clamp(d.health, 0.f, 1.f) : 0.f;
    const float b0 = u0, b1 = .056f, bv0 = .056f, bv1 = .065f;
    p.rect(b0, bv0, b1, bv1, p.c.frame, .2f);
    p.rect(b0, bv0, b0 + (b1 - b0) * health, bv1, p.c.frame, .9f);
    p.bar(b0, bv0 - .0008f, b1, bv0 - .0008f, .0005f, p.c.frame, .7f);
    char n[16];
    std::snprintf(n, sizeof n, "%d%%", static_cast<int>(std::lround(health * 100)));
    const float nh = fittedHeight(n, .0058f, u1 - b1 - .003f);
    p.text(n, u1 - textWidth(n, nh), bv0 + .0012f, nh, p.c.value);
    // Three gadgets: an icon, the name, the charges in accent segments, n/m.
    for (size_t i = 0; i < d.gadgets.size(); ++i) {
        const auto& row = d.gadgets[i];
        const float v = .030f - static_cast<float>(i) * .029f;
        icon(p, row.gadget, u0 + .0085f, v);
        const float lu = u0 + .021f;
        const char* name = gadgetName(row.gadget);
        const std::string label = name && *name ? name : "GADGET";
        p.text(label, lu, v + .0015f, fittedHeight(label, .0050f, u1 - lu), p.c.text);
        const int capacity = std::clamp(row.capacity, 1, 8);
        const int ammo = std::clamp(row.ammo, 0, capacity);
        const float s0 = lu, s1 = .062f, gapU = .0012f;
        const float w = (s1 - s0 - gapU * static_cast<float>(capacity - 1)) / static_cast<float>(capacity);
        for (int k = 0; k < capacity; ++k) {
            const float a = s0 + static_cast<float>(k) * (w + gapU);
            p.rect(a, v - .0085f, a + w, v - .0035f, p.c.accent, k < ammo ? .9f : .15f);
        }
        std::snprintf(n, sizeof n, "%d/%d", std::clamp(row.ammo, 0, 99), std::clamp(row.capacity, 0, 99));
        const float ch = fittedHeight(n, .0050f, u1 - s1 - .003f);
        p.text(n, u1 - textWidth(n, ch), v - .0085f, ch, p.c.line);
    }
    // A thin rule between the figure and the readouts.
    p.bar(-.018f, .078f, -.018f, -.040f, .0005f, p.c.accent, .3f);
    if (d.demo) // the values are placeholders, and the panel says so
        p.text("DEMO", -halfW + .010f, halfH - .019f, .0050f, p.c.warn);
    // Public version (0.0.1 = build 38.5, 0.0.2 = build 38.6), top right of STATUS (the about line).
    const float vh = fittedHeight(spidy::version::statusLine, .0042f, halfW - .040f);
    p.text(spidy::version::statusLine, halfW - .010f - textWidth(spidy::version::statusLine, vh), halfH - .019f, vh, p.c.line);
    // Version 0.0.6: the credit under it.
    const float mh = fittedHeight(spidy::version::madeBy, .0034f, halfW - .040f);
    p.text(spidy::version::madeBy, halfW - .010f - textWidth(spidy::version::madeBy, mh), halfH - .019f - vh - .002f, mh, p.c.line);
}

// The round map: its streets turn with the head, its arrow the player.
void minimap(const Painter& p, float cu, float cv, float heading) {
    const float r = .025f;
    p.ring(cu, cv, 1, 1, 0, r, p.c.backing, .75f);
    p.ring(cu, cv, 1, 1, 0, r, p.c.accent, .08f);
    const float t = std::isfinite(heading) ? -heading : 0.f;
    const float cs = std::cos(t), sn = std::sin(t);
    const auto turn = [&](float x, float y) {
        return std::array<float, 2>{cu + x * cs + y * sn, cv - x * sn + y * cs};
    };
    const float inner = r - .0025f;
    for (int k = -3; k <= 3; ++k) {
        const float o = static_cast<float>(k) * .0075f + .002f;
        if (std::abs(o) >= inner)
            continue;
        const float half = std::sqrt(inner * inner - o * o);
        const bool avenue = k == 0 || k == 2;
        for (const bool across : {false, true}) {
            const auto a = across ? turn(o, -half) : turn(-half, o);
            const auto b = across ? turn(o, half) : turn(half, o);
            p.bar(a[0], a[1], b[0], b[1], avenue ? .0014f : .0006f, p.c.line, avenue ? .45f : .28f);
        }
    }
    p.ring(cu, cv, 1, 1, r - .0006f, r + .0006f, p.c.line, .9f);
    p.ring(cu, cv, 1, 1, r, r + .004f, p.c.accent, .12f);
    for (int k = 0; k < 8; ++k) {
        const float a = t + static_cast<float>(k) * pi / 4;
        const float len = k % 2 ? .0018f : .0032f;
        p.bar(cu + std::sin(a) * r, cv + std::cos(a) * r, cu + std::sin(a) * (r - len), cv + std::cos(a) * (r - len),
              .0006f, p.c.line, .9f);
    }
    // North: the world's -z, where the head's heading counts from.
    const float nh = .0042f;
    const float nu = cu + std::sin(t) * (r + .0055f), nv = cv + std::cos(t) * (r + .0055f);
    p.text("N", nu - textWidth("N", nh) / 2, nv - nh / 2, nh, p.c.text);
    // The player, in the frame's colour, pointing where the head looks.
    p.triangle(cu, cv + .0075f, cu - .0048f, cv - .0050f, cu, cv - .0020f, p.c.frame, 1);
    p.triangle(cu, cv + .0075f, cu, cv - .0020f, cu + .0048f, cv - .0050f, p.c.frame, 1);
}

// The light on the wrist and its rays fanned up to the panel.
void light(const Painter& p, Vec3 emitter, float seconds) {
    const float pulse = .85f + .15f * std::sin(2 * pi * .8f * (std::isfinite(seconds) ? seconds : 0.f));
    const Vec3 r = p.f.right, u = p.f.up;
    // A faint cone, then the rays.
    const Vec3 left = p.at(-.072f, -halfH), rightEnd = p.at(.072f, -halfH);
    p.out.insert(p.out.end(), {{emitter, p.c.accent, .10f * pulse}, {left, p.c.accent, .01f}, {rightEnd, p.c.accent, .01f}});
    for (const float a : {-.062f, -.034f, -.008f, .02f, .046f, .068f}) {
        const Vec3 at0 = p.at(a - .0018f, -halfH), at1 = p.at(a + .0018f, -halfH);
        p.out.insert(p.out.end(), {{emitter, p.c.line, .42f * pulse}, {at0, p.c.line, .03f}, {at1, p.c.line, .03f}});
    }
    soft::softArc(p.out, emitter, r, u, 0, .0026f, 0, 2 * pi, p.c.text, .95f, p.soft, 24);
    soft::softArc(p.out, emitter, r, u, .0026f, .0045f, 0, 2 * pi, p.c.line, .45f * pulse, .009f, 32);
}

// Update 15: the pages' controls.
struct Box {
    Control control;
    float u0, v0, u1, v1;
    bool in(float u, float v) const {
        return u >= u0 && u <= u1 && v >= v0 && v <= v1;
    }
};
// The arrow strip under the panel (its bottom edge stays put on every page).
constexpr float stripTop = -halfH - .004f, stripBottom = -halfH - .024f, stripHalf = .075f;
constexpr Box previousBox{Control::previous, -stripHalf, stripBottom, -.045f, stripTop},
    nextBox{Control::next, .045f, stripBottom, stripHalf, stripTop};
constexpr float trackLeft = -.066f, trackRight = .066f, sliderReach = .009f;
const char* const nextLaunchHeading = "NEXT LAUNCH - RESTART REQUIRED";
const char* const restartNotice = "RESTART THE GAME TO APPLY";
const char* const scrollFooter = "STICK UP/DOWN OR DRAG TO SCROLL";
// Update 39.0.1's hero tabs (Update 39.0.2: no longer on any page; their Controls stay).
const char* const heroTabsHelp = "Opens a hero's own page: their powers, flying and INVINCIBLE.";
// Update 39.0.2: POWERS' heading over the hero sections, its banner, and a section's subtext.
const char* const heroProfilesHeading = "HERO PROFILES - ONE HERO AT A TIME";
const char* const activeHeroWidest = "ACTIVE HERO: SPIDER-MAN";
const char* const activeHeroHelp = "Turn a hero ON in its profile below; only one hero is on at a time.";
const char* const sectionHelp = "Opens this group of settings and closes the page's other open group.";
const char* const restartTagText = "RESTART";
// The hero being played as View has it (hero_modes::fromToggles' order).
int heroOf(const View& view) {
    return view.combat38[combat38::shooterType] == 1 ? 1
           : view.combat38[combat38::daredevil] != 0 ? 2
           : view.combat38[combat38::hulk] != 0      ? 3
           : view.combat38[combat38::batman] != 0    ? 4 // Version 0.0.7
           : view.combat38[combat38::wolverine] != 0 ? 5
           : view.combat38[combat38::superman] != 0  ? 6
           : view.combat38[combat38::punisher] != 0  ? 7
           : view.combat38[combat38::captainAmerica] != 0 ? 8 // Version 0.0.7 CAPTAIN AMERICA
           : view.combat38[combat38::thor] != 0           ? 9 // Version 0.0.72
           : view.combat38[combat38::greenLantern] != 0   ? 10
                                                     : 0;
}
// No hero toggle on: Spider-Man (hero_modes.hpp).
bool spiderActive(const View& view) {
    for (const auto o : combat38::heroToggles)
        if (view.combat38[o] != 0)
            return false;
    return true;
}
constexpr float titleHeight = .0068f, labelHeight = .0046f, headingHeight = .0042f, noticeHeight = .0044f,
                footerHeight = .0040f, fpsHeight = .0056f;
// Update 16: the FPS monitor's widest readout (Update 17: with its band's
// word), and its sparkline's left edge when it sits beside the readout.
const char* const fpsWidest = "FPS 888 / 888 HZ POOR";
// Update 19: the MUSIC page's widest time and state word, and its visualizer's height.
const char* const clockWidest = "88:88:88";
const char* const stateWidest = "STOPPED";
constexpr float visualHeight = .046f;
const char* const gpuWidest = "EYE PASSES 88.88 MS GPU";
constexpr float sparkLeft = .030f;
// The scrollbar's line (its track's bottom above the cut corner), and its
// thumb's least length.
constexpr float scrollbarU = (scrollbarU0 + scrollbarU1) / 2, scrollTrackTop = viewTop - .002f,
                scrollTrackBottom = viewBottom + .012f, thumbLeast = .016f;
bool slider(Control c) {
    return c == Control::thickness || c == Control::speed || c == Control::history || c == Control::reach ||
           c == Control::monitor || c == Control::supersample || c == Control::sharpen ||
           c == Control::hudHeight || c == Control::hudScale || c == Control::hudText || c == Control::musicVolume;
}
// Update 16: the ON/OFF buttons (AA's FXAA/OFF and RETICLES'): a press flips them.
bool toggle(Control c) {
    return c == Control::aa || c == Control::reticles || c == Control::eyeTest;
}
bool setting(Control c) {
    return slider(c) || toggle(c) || cycles(c);
}
bool off(const View& view, Control c) {
    // Update 38.5: 90s ANIMATED's own rows do nothing under another FILTER (or with its shader failed).
    if (c == Control::bloodVisor) // Update 38.5: VISOR DROPS is EXTREME's own
        return !blood_splatter::isExtreme(view.blood);
    if (c == Control::invincible) // Update 38.5: only on the confirmed health record
        return view.invincibleNa;
    if (c == Control::flyVignette || c == Control::flyRoll) // Update 38.5: FLY MODE's own
        return view.flyMode == 0;
    if (c == Control::flyRollSpeed || c == Control::flyAutoLevel) // and FLY ROLL's
        return view.flyMode == 0 || view.flyRoll == extras385::fly::rollOff;
    if (c == Control::trainMove) // Version 0.0.2: TRAIN MOVE only does something in TRAINING
        return view.gestureMoves != gestures385::modeTraining;
    // Version 0.0.2: AIM STRENGTH and BALL HIT are BALL LOOK OURS's; BALL AIM the game ball's
    if (c == Control::combatAimStrength || c == Control::combatBallHit)
        return view.combat38[combat38::ballLook] != combat38::lookOurs;
    if (c == Control::combatBallAim)
        return view.combat38[combat38::ballLook] == combat38::lookOurs;
    // Version 0.0.3: ROPE STRENGTH is ROPE GRAB's; TARGET FPS, QUALITY FLOOR and SHOW STATUS are
    // AUTO PERFORMANCE's (all OFF by default)
    if (c == Control::combatRopeStrength)
        return view.combat38[combat38::ropeGrab] == 0;
    if (c == Control::combatPerfTarget || c == Control::combatPerfFloor || c == Control::combatPerfStatus)
        return view.combat38[combat38::autoPerf] == 0;
    // Version 0.0.5: SUPERSONIC is FLY MODE's; SUPERSONIC SPEED is SUPERSONIC's
    if (c == Control::combatSupersonic)
        return view.flyMode == 0;
    if (c == Control::combatSupersonicSpeed)
        return view.flyMode == 0 || view.combat38[combat38::supersonic] != 0;
    // Version 0.0.6: DAREDEVIL PUNCH is DAREDEVIL's
    if (c == Control::combatDaredevilPunch)
        return view.combat38[combat38::daredevil] == 0;
    if (c == Control::gestureKeep) // Update 38.5 GESTURE REMAP: only in the popup, never for a safety gesture
        return !view.remap || !view.remap->popup || view.remap->safety;
    if (c == Control::gestureAgain)
        return !view.remap || !view.remap->popup;
    if (c == Control::gestureCancel)
        return !view.remap || !(view.remap->popup || view.remap->recording);
    if (c == Control::promptHandling) // Update 38.5: AUTO needs the game's prompts hooked (9699 in SM2)
        return !extras385::prompts::hooked;
    if (c == Control::colorCartoon || c == Control::colorOutlines || c == Control::colorBands)
        return (view.color >> 8 & 0xff) != eye_color::animated90s || (view.color & 0x1000000) != 0;
    if (c == Control::taaJitter) // Update 34.2: greyed (N/A) while it would do nothing
        return !engine_taa::jitterActive(view.engineTaa, (view.eyeAa & 0xff) == eye_taa::on);
    if (c == Control::shadowDistance) // Update 33: locked until the search finds its value
        return !shadow_distance::sliderMoves(view.shadowDistance); // Update 34.2: and while searching
    const auto i = static_cast<unsigned>(c);
    return i < maskedControls && (view.disabled >> i & 1);
}
// Update 37.2: the COLOR page's live sliders (eye_color::Config's).
bool colorSlider(Control c) {
    return (c >= Control::colorStrength && c <= Control::vignette) || c == Control::colorCartoon;
}
// Update 33: the sliders a pointer drags: the settings' and SHADOW DISTANCE (live, past
// the mask, never a setting: Applier never sees it).
bool dragged(Control c) {
    return slider(c) || c == Control::shadowDistance || c == Control::eyeTrail || colorSlider(c) ||
           c == Control::foveaInner || c == Control::flyRollSpeed;
}
// Update 24: 0 for the presses past the mask (fpsReset).
uint32_t bit(Control c) {
    const auto i = static_cast<unsigned>(c);
    return i < maskedControls ? 1u << i : 0u;
}
const char* labelOf(Control c) {
    if (const int o = combatOption(c); o >= 0)
        return combat38::optionInfo[o].label;
    switch (c) {
    case Control::thickness:
        return "WEB THICKNESS";
    case Control::speed:
        return "WEB SPEED";
    case Control::aa:
        return "ANTI-ALIASING";
    case Control::reticles:
        return "RETICLES";
    case Control::eyeTest:
        return "EYE TEST (SAME PICTURE)";
    case Control::addressFinder:
        return "ADDRESS FINDER";
    case Control::eyeLod:
        return "EYE LOD";
    case Control::shadowCamera:
        return "SHADOW CAMERA";
    case Control::shadowDistance:
        return "SHADOW DISTANCE";
    case Control::eyeAa:
        return "EYE AA";
    case Control::engineTaa:
        return "ENGINE TAA";
    case Control::eyeTrail:
        return "EYE AA TRAIL";
    case Control::taaJitter:
        return "TAA JITTER";
    case Control::farShadows:
        return "FAR SHADOWS";
    case Control::eyeBloom:
        return "EYE BLOOM";
    case Control::sharedLod:
        return "SHARED LOD";
    case Control::mirror:
        return "MIRROR";
    case Control::mirrorRecentre:
        return "RECENTRE MIRROR";
    case Control::worldLog:
        return "WORLD LOG";
    case Control::blood:
        return "BLOOD SPLATTER";
    case Control::bloodVisor:
        return "VISOR DROPS";
    case Control::invincible:
        return "INVINCIBLE";
    case Control::flyMode:
        return "FLY MODE";
    case Control::flyVignette:
        return "FLY VIGNETTE";
    case Control::flyRoll:
        return "FLY ROLL";
    case Control::flyRollSpeed:
        return "ROLL SPEED";
    case Control::flyAutoLevel:
        return "AUTO LEVEL";
    case Control::symbiote:
        return "SYMBIOTE LOOK";
    case Control::padMode:
        return "PAD MODE";
    case Control::promptHandling:
        return "PROMPT HANDLING";
    case Control::playOnMonitor:
        return "PLAY ON MONITOR";
    case Control::gestureMoves:
        return "GESTURE MOVES";
    case Control::trainMove:
        return "TRAIN MOVE";
    case Control::heroTabSpider:
        return "SPIDER-MAN";
    case Control::heroTabIron:
        return "IRON MAN";
    case Control::heroTabDaredevil:
        return "DAREDEVIL";
    case Control::heroTabHulk:
        return "HULK";
    case Control::heroPlaySpider:
        return "PLAY AS SPIDER-MAN";
    case Control::heroRoster:
        return "ROSTER: HEROES AND SUITS";
    case Control::section0:
    case Control::section1:
    case Control::section2:
    case Control::section3:
    case Control::section4:
    case Control::section5:
    case Control::section6:
    case Control::section7:
    case Control::section8:
    case Control::section9:
    case Control::section10:
    case Control::section11:
        return "SECTION";
    case Control::gestureRemapMove:
        return "REMAP MOVE";
    case Control::gestureRecord:
        return "RECORD MY MOVE";
    case Control::gestureKeep:
        return "KEEP ANYWAY";
    case Control::gestureAgain:
        return "TRY AGAIN";
    case Control::gestureCancel:
        return "CANCEL";
    case Control::gestureResetMove:
        return "RESET MOVE";
    case Control::gestureResetAll:
        return "RESET ALL MOVES";
    case Control::gestureProfile:
        return "GESTURE PROFILE";
    case Control::mirrorAnchor:
        return "MIRROR ANCHOR";
    case Control::npcFade:
        return "NPC NEAR FADE";
    case Control::slotTest:
        return "SLOT TEST";
    case Control::webBallTest:
        return "WEB BALL TEST";
    case Control::colorFx:
        return "COLOR FX";
    case Control::colorFilter:
        return "FILTER";
    case Control::colorStrength:
        return "STRENGTH";
    case Control::exposure:
        return "EXPOSURE";
    case Control::contrast:
        return "CONTRAST";
    case Control::saturation:
        return "SATURATION";
    case Control::vibrance:
        return "VIBRANCE";
    case Control::warmth:
        return "WARMTH";
    case Control::tint:
        return "TINT";
    case Control::vignette:
        return "VIGNETTE";
    case Control::colorCartoon:
        return "CARTOON STRENGTH";
    case Control::colorOutlines:
        return "OUTLINES";
    case Control::colorBands:
        return "BANDS";
    case Control::foveated:
        return "FOVEATED (EXPERIMENTAL)";
    case Control::foveaInner:
        return "INNER SIZE";
    case Control::foveaDebug:
        return "FOVEATED DEBUG";
    case Control::foveaStats:
        return "FOVEATED FPS";
    case Control::history:
        return "HISTORY BLEND";
    case Control::reach:
        return "BUILDING REACH";
    case Control::monitor:
        return "WINDOW FPS";
    case Control::sharpen:
        return "SHARPNESS";
    case Control::supersample:
        return "EYE SUPERSAMPLE";
    case Control::hudHeight:
        return "HOLOGRAM HEIGHT";
    case Control::hudScale:
        return "HOLOGRAM SCALE";
    case Control::hudText:
        return "TEXT SIZE";
    case Control::hudTheme:
        return "COLOR THEME";
    case Control::hudTextColor:
        return "TEXT COLOR";
    case Control::webStyle:
        return "WEB STYLE";
    case Control::webColor:
        return "WEB COLOR";
    case Control::musicVolume:
        return "MUSIC VOLUME";
    case Control::musicVisualizer:
        return "VISUALIZER";
    case Control::musicTrack:
        return "SONG";
    default:
        return "";
    }
}
// The widest number a slider prints: its room on the row is kept for it.
const char* widestNumber(Control c) {
    switch (c) {
    case Control::monitor:
        return "EVERY FRAME";
    case Control::thickness:
        return "8.8 CM";
    case Control::speed:
        return "888 M/S";
    case Control::hudHeight:
        return "88 CM";
    case Control::hudScale:
        return "8.88X";
    case Control::shadowDistance:
        return shadow_distance::widestValue();
    case Control::eyeTrail:
        return "100%";
    case Control::exposure:
        return "+8.8 EV";
    case Control::warmth:
    case Control::tint:
        return "-100";
    default:
        return "888%";
    }
}

// Update 16/17: where everything on a page goes, laid out from the top of
// the inner area down for one text size, each control's explanation wrapped
// under it. v at scroll 0; what runs below viewBottom is reached by scrolling.
struct Help {
    float v = 0; // the first line's bottom
    float height = helpHeight, pitch = helpPitch;
    std::vector<std::string> lines;
    int rows = 1; // Update 38.5: the lines it shows (helpRowsShown at most); more scroll
};
// Update 19: dropdown (the song list's button), visual (the visualizer's
// box), progress (the times and the seek bar), transport (a player
// button), musicStatus (the note and the folder).
enum class Kind : uint8_t {
    title, fps, wide, slider, toggle, cycle, mirror, heading, notice, footer,
    dropdown, visual, progress, transport, musicStatus,
    fpsStats, // Update 24: the session HIGH / LOW / AVG boxes, RESET its box
    guide,    // Update 37: a CONTROLS line (Item::line), its how-to as its help
    trail,    // Update 38.5 GESTURE REMAP: the recorded move's trail and the popup's line
    section,  // Update 39.0.2: a section's header (a press opens or closes it), its summary on the right
    banner    // Update 39.0.2: POWERS' ACTIVE HERO line
};
struct Item {
    Kind kind = Kind::title;
    Control control = Control::none;
    const char* text = "";
    Box box{Control::none, 0, 0, 0, 0}; // a button; the FPS sparkline; the mirror
    float v = 0, h = 0;                 // the text's bottom and height (a slider's label)
    float track = 0;                    // a slider's track; a heading's line
    float numberH = 0, numberW = 0;     // a slider's number: its height, the width kept for it
    float v2 = 0, h2 = 0;               // the FPS monitor's GPU line
    Help help;
    uint32_t line = 0;                  // Update 37: a guide item's controls_guide::Line
    // Update 39.0.2: a section header's summary (sum0 / sum1's values between pre and post, else
    // "count SETTINGS"), its hero (-1 none: the ACTIVE badge), and a row's RESTART tag (tagH 0: none).
    Control sum0 = Control::none, sum1 = Control::none;
    const char* pre = "";
    const char* post = "";
    int count = 0, hero = -1;
    bool open = false;
    float tagU = 0, tagH = 0;
};
Item itemOf(Kind kind, Control control = Control::none) {
    Item it;
    it.kind = kind;
    it.control = control;
    return it;
}
struct Layout {
    float text = 1;
    std::vector<Item> items;
    float bottom = viewTop; // the content's lowest edge, scroll 0
    std::vector<Area> areas;
};

// Update 39.0.2: the pages' sections (the approved 0.0.6 layout). A header's summary: sum0 (and
// sum1) between pre and post; else, with a word, how many rows ("4 SETTINGS"); else pre alone.
struct SectionDef {
    const char* name;
    Control sum0, sum1;
    const char* pre;
    const char* post;
    const char* word;
    int hero; // hero_modes::Hero, -1: not a hero's profile
    Control rows[12]; // Version 0.0.72: PUNISHER has ten (the van); 0.0.73 test 3: eleven (VAN SCREEN), twelve (VAN PEDALS)
};
using C = Control;
constexpr SectionDef powersSections[] = {
    {"FLYING", C::flyMode, C::none, "FLY ", "", nullptr, -1,
     {C::flyMode, C::combatSupersonic, C::combatSupersonicSpeed, C::flyVignette, C::flyRoll, C::flyRollSpeed,
      C::flyAutoLevel, C::combatFlyPose}},
    {"SPIDER-MAN", C::none, C::none, "", "", nullptr, 0, {C::heroPlaySpider, C::combatHeroDamage, C::symbiote}},
    {"IRON MAN", C::none, C::none, "", "", nullptr, 1, {C::combatShooterType, C::combatIronHud, C::combatPalmBlast}},
    {"DAREDEVIL", C::none, C::none, "", "", nullptr, 2,
     {C::combatDaredevil, C::combatDaredevilPunch, C::combatBeaconsVision, C::combatBatonHolsters, C::combatBatonLook,
      C::combatDdGrapple, C::combatDdParry, C::combatDdBatonTip}},
    {"HULK", C::none, C::none, "", "", nullptr, 3, {C::combatHulk, C::combatHeroHeight, C::combatHulkRage}},
    // Version 0.0.7 HERO PROFILES
    {"BATMAN", C::none, C::none, "", "", nullptr, 4, {C::combatBatman, C::combatBatarangChain, C::combatBatarangStyle, C::combatSmokeBlind,
      C::combatPerchTakedown, C::combatTakedownLight}},
    {"WOLVERINE", C::none, C::none, "", "", nullptr, 5, {C::combatWolverine, C::combatClawClimb}},
    {"SUPERMAN", C::none, C::none, "", "", nullptr, 6, {C::combatSuperman}},
    {"PUNISHER", C::none, C::none, "", "", nullptr, 7, {C::combatPunisher, C::combatEasyReload, C::combatHolsterHands, C::combatGunModels, C::combatPunisherRage, C::combatPunisherSuit,
      C::combatRageBlood, C::combatRageTint, C::combatPunisherHook, C::combatPunisherVan,
      C::combatVanScreen, C::combatVanPedals}},
    {"CAPTAIN AMERICA", C::none, C::none, "", "", nullptr, 8,
     {C::combatCaptainAmerica, C::combatShieldArm, C::combatShieldDamage, C::combatShieldChain, C::combatShieldBlock,
      C::combatShieldLook, C::combatShieldSlam, C::combatCapSprint}},
    // Version 0.0.72 (TEST)
    {"THOR", C::none, C::none, "", "", nullptr, 9,
     {C::combatThor, C::combatThorLightning, C::combatThorStorm, C::combatThorChain, C::combatThorHammer, C::combatThorHammerModel, C::combatThorFlight,
      C::combatNaturalInvincible}},
    {"GREEN LANTERN", C::none, C::none, "", "", nullptr, 10,
     {C::combatGreenLantern, C::combatGlFlight, C::combatGlFist, C::combatGlBeam, C::combatGlBubble, C::combatGlChain,
      C::combatGlCharge}},
};
constexpr SectionDef vrSections[] = {
    {"BODY AND VIEW", C::none, C::none, "", "", "SETTINGS", -1,
     {C::combatCrawlCap, C::combatWallTilt, C::combatHandFlips, C::npcFade, C::playOnMonitor, C::combatSpiderClimbOnly,
      C::combatGameBody, C::combatCalibLook, C::combatWeaponCollision}},
    {"WEBS", C::webStyle, C::webColor, "", "", nullptr, -1,
     {C::thickness, C::speed, C::webStyle, C::webColor, C::reticles, C::combatWebRope}},
    {"HOLOGRAM", C::hudHeight, C::hudScale, "", "", nullptr, -1,
     {C::hudHeight, C::hudScale, C::hudText, C::hudTheme, C::hudTextColor, C::combatHoloOpen, C::combatRingAnchor,
      C::combatRingLayer}},
    {"MIRROR", C::mirror, C::none, "MIRROR ", "", nullptr, -1,
     {C::mirror, C::mirrorRecentre, C::mirrorAnchor, C::combatMirrorLook, C::combatMirrorView}},
    {"SOUND", C::combatSfxVolume, C::none, "SFX ", "", nullptr, -1, {C::combatSfxVolume}},
    {"ADVANCED - TESTS AND LOGS", C::none, C::none, "", "", "SETTINGS", -1, {C::worldLog, C::slotTest}},
};
constexpr SectionDef combatSections[] = {
    {"PUNCHES", C::combatPunchMode, C::none, "", "", nullptr, -1,
     {C::combatPunchMode, C::combatPunchSense, C::combatPunchAngle, C::combatMeleeAnim, C::combatZipMode,
      C::combatHitIndicator, C::combatPunchHits, C::combatUppercut}},
    {"WEB BALL", C::combatBallAim, C::combatBallLook, "", "", nullptr, -1,
     {C::combatBallAim, C::combatBallLook, C::combatBallFire, C::combatBallLead, C::combatAimStrength,
      C::combatBallHit}},
    {"LUNGE AND LOCK-ON", C::combatLunge, C::none, "", "", nullptr, -1,
     {C::combatLunge, C::combatLungeFrom, C::combatLungeSpeed, C::combatLungeAim, C::combatLookLock,
      C::combatStrikeSteer, C::combatAimAxis, C::combatFoeTrack}},
    {"GADGETS AND GRABS", C::combatGadgetKeys, C::none, "", " KEYS", nullptr, -1,
     {C::combatGadgetKeys, C::combatGadgetFeel, C::combatGadgetHint, C::combatGrabWeb, C::combatRopeGrab,
      C::combatRopeStrength, C::combatWallStick}},
    {"BLOOD", C::blood, C::combatBloodLook, "", "", nullptr, -1, {C::blood, C::combatBloodLook, C::bloodVisor}},
    // Version 0.0.72 (TEST)
    {"SURVIVAL AND REPUTATION", C::none, C::none, "", "", "SETTINGS", -1, {C::combatTestUnlocks, C::combatSurvival, C::combatReputation, C::combatHeroMenu, C::combatRepLog, C::combatHeroSuit, C::combatOwnSuit, C::combatSuitBody, C::combatPerkBranch,
      C::combatUltimates}},
    {"FIRES, SHOVES AND FACTIONS", C::none, C::none, "", "", nullptr, -1, {C::combatFireDouse, C::combatNpcShove, C::combatFaction}},
};
constexpr SectionDef graphicsSections[] = {
    {"QUALITY", C::aa, C::sharpen, "", "", nullptr, -1,
     {C::combatImage, C::aa, C::sharpen, C::farShadows, C::reach, C::combatFarWindows, C::combatCullMargin, C::combatShadowMatch, C::eyeAa, C::eyeTrail, C::history}},
    {"PERFORMANCE", C::combatAutoPerf, C::none, "AUTO ", "", nullptr, -1,
     {C::combatAutoPerf, C::combatPerfTarget, C::combatPerfFloor, C::combatPerfStatus, C::foveated, C::foveaInner}},
    {"DISPLAY AND DETAIL", C::monitor, C::none, "WINDOW ", "", nullptr, -1,
     {C::monitor, C::eyeBloom, C::combatLightHold, C::combatLensFlares, C::sharedLod, C::eyeLod}},
    {"ADVANCED - TESTS AND ENGINE", C::none, C::none, "", "", "SETTINGS", -1,
     {C::engineTaa, C::taaJitter, C::eyeTest, C::addressFinder, C::webBallTest, C::foveaDebug, C::foveaStats}},
};
constexpr SectionDef colorSections[] = {
    {"ADJUST", C::none, C::none, "", "", "SLIDERS", -1,
     {C::exposure, C::contrast, C::saturation, C::vibrance, C::warmth, C::tint, C::vignette}},
    {"90s ANIMATED", C::none, C::none, "ONLY WITH 90s FILTER", "", nullptr, -1,
     {C::colorCartoon, C::colorOutlines, C::colorBands}},
};
constexpr SectionDef controlsSections[] = {
    {"HOW TO PLAY", C::none, C::none, "", "", "MOVES", -1, {}},
    {"GESTURES", C::gestureMoves, C::none, "", "", nullptr, -1, {C::gestureMoves, C::trainMove}},
    {"GESTURE REMAP", C::gestureProfile, C::none, "", "", nullptr, -1,
     {C::gestureProfile, C::gestureRemapMove, C::gestureRecord, C::gestureKeep, C::gestureAgain, C::gestureCancel,
      C::gestureResetMove, C::gestureResetAll}},
    {"GAMEPAD", C::padMode, C::none, "PAD MODE ", "", nullptr, -1, {C::padMode, C::promptHandling}},
};
struct PageSections {
    const SectionDef* list;
    int count;
};
template <size_t N> constexpr PageSections of(const SectionDef (&a)[N]) { return {a, static_cast<int>(N)}; }
PageSections pageSections(int page) {
    switch (page) {
    case powersPage:
        return of(powersSections);
    case vrPage:
        return of(vrSections);
    case combatPage:
        return of(combatSections);
    case graphicsPage:
        return of(graphicsSections);
    case colorPage:
        return of(colorSections);
    case controlsPage:
        return of(controlsSections);
    default:
        return {nullptr, 0};
    }
}
const SectionDef* sectionsOf(int page) { return pageSections(page).list; }
int rowsIn(const SectionDef& d) {
    int n = 0;
    for (const Control c : d.rows)
        n += c != Control::none;
    return n;
}
// The rows outside every section, per page (STATUS and the arrows: none).
constexpr Control alwaysShown[] = {C::invincible, C::heroRoster, C::recalibrate, C::fpsReset, C::supersample,
                                   C::colorFx,    C::colorFilter, C::colorStrength};
constexpr int alwaysShownPage[] = {powersPage, powersPage, vrPage, graphicsPage, graphicsPage, colorPage, colorPage, colorPage};
static_assert(std::size(alwaysShown) == std::size(alwaysShownPage));
bool isSliderRow(Control c) {
    switch (c) {
    case Control::eyeTrail:
    case Control::foveaInner:
    case Control::colorStrength:
    case Control::exposure:
    case Control::contrast:
    case Control::saturation:
    case Control::vibrance:
    case Control::warmth:
    case Control::tint:
    case Control::vignette:
    case Control::colorCartoon:
    case Control::flyRollSpeed:
    case Control::shadowDistance:
        return true;
    default:
        return slider(c);
    }
}
bool isCycleRow(Control c) {
    return c == Control::webStyle || c == Control::webColor || c == Control::hudTheme || c == Control::hudTextColor ||
           c == Control::musicVisualizer;
}
// The open sections, a nibble a page: 0xF the page's default, 0xE none, else the section.
std::atomic<uint32_t> openSections{0xFFFFFFFFu};
std::atomic<int> heroPlayed{0};
} // namespace

int sectionCount(int page) { return pageSections(page).count; }
const char* sectionName(int page, int section) {
    const PageSections ps = pageSections(page);
    return section >= 0 && section < ps.count ? ps.list[section].name : "";
}
int openSection(int page) {
    const int count = sectionCount(page);
    if (count == 0)
        return -1;
    const uint32_t n = openSections.load(std::memory_order_relaxed) >> (4 * page) & 0xFu;
    if (n == 0xFu) // the default
        return page == colorPage    ? -1
               : page == powersPage ? 1 + std::clamp(heroPlayed.load(std::memory_order_relaxed), 0, 10)
                                    : 0;
    return static_cast<int>(n) < count ? static_cast<int>(n) : -1;
}
void setOpenSection(int page, int section) {
    if (page < 0 || page >= pages)
        return;
    const uint32_t n = section >= 0 && section < sectionCount(page) ? static_cast<uint32_t>(section) : 0xEu;
    uint32_t was = openSections.load(std::memory_order_relaxed);
    while (!openSections.compare_exchange_weak(was, (was & ~(0xFu << (4 * page))) | n << (4 * page),
                                               std::memory_order_relaxed)) {
    }
}
void resetSections() { openSections.store(0xFFFFFFFFu, std::memory_order_relaxed); }
void setActiveHero(int hero) { heroPlayed.store(hero >= 0 && hero <= 10 ? hero : 0, std::memory_order_relaxed); } // 0.0.7: 9 (CAPTAIN AMERICA)
int activeHero() { return heroPlayed.load(std::memory_order_relaxed); }
const char* activeHeroName(int hero) {
    static_assert(sizeof(hero_modes::heroNames) / sizeof(hero_modes::heroNames[0]) == hero_modes::heroes,
                  "every hero has a name");
    if (hero < 0 || hero >= static_cast<int>(hero_modes::heroes) || !hero_modes::heroNames[hero])
        return hero_modes::heroNames[hero_modes::spiderMan];
    return hero_modes::heroNames[hero];
}
Place placeOf(Control c) {
    if (c == Control::none)
        return {};
    for (size_t i = 0; i < std::size(alwaysShown); ++i)
        if (alwaysShown[i] == c)
            return {alwaysShownPage[i], -1};
    const int ci = static_cast<int>(c);
    if (ci >= static_cast<int>(Control::musicTrack) && ci <= static_cast<int>(Control::musicPick))
        return {musicPage, -1};
    for (int page = 0; page < pages; ++page) {
        const PageSections ps = pageSections(page);
        for (int s = 0; s < ps.count; ++s)
            for (const Control r : ps.list[s].rows)
                if (r == c && r != Control::none)
                    return {page, s};
    }
    return {};
}
bool restartTag(Control c) {
    return c == Control::supersample || c == Control::combatMirrorView || c == Control::combatLensFlares; // 0.0.62
}

namespace {
Layout build(int page, float k, int open) {
    Layout l;
    l.text = k;
    if (page < 1 || page >= pages)
        return l; // STATUS: drawn as it always was, nothing scrolls
    const float gapV = .0065f * k;
    float c = viewTop;
    // Update 38.5: wrapped clear of the scroll gutter; a control's shows
    // helpRowsShown lines at most and scrolls the rest (capped false: all shown).
    auto helpUnder = [&](const char* text, Help& h, bool capped = true) {
        h.height = helpHeight * k;
        h.pitch = helpPitch * k;
        h.lines = wrapText(text, h.height, helpWidth - helpGutter);
        const int all = static_cast<int>(h.lines.size());
        h.rows = capped ? std::min(all, helpRowsShown) : all;
        h.v = c - .0022f * k - h.height;
        c = h.v - h.pitch * static_cast<float>(h.rows - 1);
    };
    {
        Item t = itemOf(Kind::title);
        t.text = pageName(page);
        t.h = fittedHeight(t.text, titleHeight * k, .13f);
        t.v = c - .006f - t.h;
        c = t.v - .0045f - .002f * k;
        l.items.push_back(std::move(t));
    }
#if defined(SPIDY_TEST_BUILD) && SPIDY_TEST_BUILD
    // Build 5: which build runs, on the wrist (the console's first line says it too).
    {
        Item f = itemOf(Kind::footer);
        f.text = "TEST " SPIDY_TEST_NUMBER " " SPIDY_TEST_COMMIT " - HERO VR 0.0.72 test"; // Test build 11
        f.h = fittedHeight(f.text, footerHeight * k, .14f);
        f.v = c - f.h;
        c = f.v - .002f * k;
        l.items.push_back(std::move(f));
    }
#else
    // The public release: its version where the test builds said TEST <n> <commit>.
    {
        Item f = itemOf(Kind::footer);
        f.text = "HERO VR " SPIDY_PUBLIC_VERSION " - MADE BY AGENT MULDER";
        f.h = fittedHeight(f.text, footerHeight * k, .14f);
        f.v = c - f.h;
        c = f.v - .002f * k;
        l.items.push_back(std::move(f));
    }
#endif
    auto sliderItem = [&](Control control) {
        Item s = itemOf(Kind::slider, control);
        s.text = labelOf(control);
        s.numberH = fittedHeight(widestNumber(control), .0050f * k, .060f);
        s.numberW = textWidth(widestNumber(control), s.numberH);
        s.h = fittedHeight(s.text, labelHeight * k, trackRight - trackLeft - s.numberW - .006f);
        s.v = c - gapV - std::max(s.h, s.numberH);
        s.track = s.v - std::max(.008f * k, .0075f);
        c = s.track - .005f;
        helpUnder(helpText(control), s.help);
        l.items.push_back(std::move(s));
    };
    auto toggleItem = [&](Control control) {
        Item t = itemOf(Kind::toggle, control);
        t.text = labelOf(control);
        const float bh = std::max(.020f * k, .016f);
        t.box = {control, .030f, c - gapV - bh, .074f, c - gapV};
        t.h = fittedHeight(t.text, labelHeight * k, .030f - .004f - trackLeft);
        t.v = (t.box.v0 + t.box.v1) / 2 - t.h / 2;
        c = t.box.v0;
        helpUnder(helpText(control), t.help);
        l.items.push_back(std::move(t));
    };
    auto cycleItem = [&](Control control) {
        Item y = itemOf(Kind::cycle, control);
        y.text = labelOf(control);
        y.h = fittedHeight(y.text, labelHeight * k, trackRight - trackLeft);
        y.v = c - gapV - y.h;
        const float bh = std::max(.018f * k, .015f);
        y.box = {control, trackLeft, y.v - .003f * k - bh, trackRight, y.v - .003f * k};
        c = y.box.v0;
        helpUnder(helpText(control), y.help);
        l.items.push_back(std::move(y));
    };
    auto footerItem = [&](const char* text) {
        Item f = itemOf(Kind::footer);
        f.text = text;
        f.h = fittedHeight(f.text, footerHeight * k, .14f);
        f.v = c - gapV - f.h;
        c = f.v;
        l.items.push_back(std::move(f));
    };
    auto heading = [&](const char* text) {
        Item h = itemOf(Kind::heading);
        h.text = text;
        h.track = c - gapV;
        h.h = fittedHeight(h.text, headingHeight * k, trackRight - trackLeft);
        h.v = h.track - .0035f * k - h.h;
        c = h.v;
        l.items.push_back(std::move(h));
    };
    // Update 39.0.2 (owner: "the holo menus need to be cleaner and better organized"): the rows in
    // sections, a header each (a press opens it and closes the page's other one); a closed
    // section's rows are not laid out at all.
    auto row = [&](Control control) {
        if (isSliderRow(control))
            sliderItem(control);
        else if (isCycleRow(control))
            cycleItem(control);
        else
            toggleItem(control);
    };
    auto sectionHeader = [&](int s) -> bool {
        const SectionDef& d = sectionsOf(page)[s];
        Item h = itemOf(Kind::section, sectionControl(s));
        h.text = d.name;
        h.line = static_cast<uint32_t>(s);
        h.open = s == open;
        h.sum0 = d.sum0;
        h.sum1 = d.sum1;
        h.pre = d.pre;
        h.post = d.post;
        h.hero = d.hero;
        h.count = d.word ? (page == controlsPage && s == 0 ? static_cast<int>(controls_guide::lines) : rowsIn(d)) : 0;
        const float bh = std::max(.019f * k, .016f);
        h.box = {h.control, trackLeft, c - gapV - bh, trackRight, c - gapV};
        h.h = std::min(fittedHeight(h.text, labelHeight * k, (trackRight - trackLeft) * .55f), bh * .5f);
        h.v = (h.box.v0 + h.box.v1) / 2 - h.h / 2;
        h.h2 = std::min(.0036f * k, bh * .42f); // the summary's (or the ACTIVE badge's) height, fitted when drawn
        c = h.box.v0;
        l.items.push_back(std::move(h));
        return s == open;
    };
    auto section = [&](int s) {
        if (!sectionHeader(s))
            return;
        const SectionDef& d = sectionsOf(page)[s];
        if (page == controlsPage && s == 0) {
            // Update 37: one line per control and its how-to under it; marked when drawn.
            for (uint32_t i = 0; i < controls_guide::lines; ++i) {
                Item g = itemOf(Kind::guide);
                g.line = i;
                g.text = controls_guide::texts[i].name;
                g.h = fittedHeight(g.text, labelHeight * k, .085f);
                g.v = c - gapV - g.h;
                c = g.v;
                helpUnder(controls_guide::texts[i].how, g.help, false);
                l.items.push_back(std::move(g));
            }
            return;
        }
        for (const Control control : d.rows) {
            if (control == Control::none)
                break;
            row(control);
            if (control == Control::gestureRecord) {
                // Update 38.5 GESTURE REMAP: the trail (RED / BLUE / GREEN) with the popup's
                // CONFLICTS WITH line in it, under RECORD.
                Item t = itemOf(Kind::trail);
                const float th = .046f * k;
                t.box = {Control::none, trackLeft, c - gapV - th, trackRight, c - gapV};
                t.h = std::min(.0042f * k, .0060f);
                c = t.box.v0;
                l.items.push_back(std::move(t));
            }
        }
    };
    auto sections = [&](int from, int to) {
        for (int s = from; s < to; ++s)
            section(s);
    };
    switch (page) {
    case powersPage: {
        // ACTIVE HERO, INVINCIBLE and FLYING for every hero, then a profile per hero (the one played open).
        Item b = itemOf(Kind::banner);
        b.text = activeHeroWidest;
        b.h = fittedHeight(activeHeroWidest, .0046f * k, trackRight - trackLeft);
        b.v = c - gapV - b.h;
        c = b.v;
        helpUnder(activeHeroHelp, b.help);
        l.items.push_back(std::move(b));
        row(Control::invincible);
        row(Control::heroRoster); // Build 8: the roster page (pictures, stories, PLAY AS, suits)
        section(0);
        heading(heroProfilesHeading);
        sections(1, sectionCount(powersPage)); // Version 0.0.7: nine heroes (CAPTAIN AMERICA)
        break;
    }
    case vrPage: {
        Item r = itemOf(Kind::wide, Control::recalibrate);
        r.text = "RECALIBRATE";
        r.box = {Control::recalibrate, trackLeft, c - gapV - .024f * k, trackRight, c - gapV};
        c = r.box.v0;
        helpUnder(helpText(Control::recalibrate), r.help);
        l.items.push_back(std::move(r));
        sections(0, 6);
        break;
    }
    case combatPage:
        sections(0, sectionCount(combatPage)); // Version 0.0.72: + SURVIVAL AND REPUTATION
        break;
    case graphicsPage: {
        // Update 16: the FPS monitor. Update 17: its sparkline beside the
        // readout while the readout fits there near its size, else under it.
        Item f = itemOf(Kind::fps);
        const float beside = sparkLeft - trackLeft - .004f;
        const float near = fittedHeight(fpsWidest, fpsHeight * k, beside);
        const bool fits = near >= .85f * fpsHeight * k;
        f.h = fits ? near : fittedHeight(fpsWidest, fpsHeight * k, trackRight - trackLeft);
        f.v = c - gapV - f.h;
        f.h2 = fittedHeight(gpuWidest, helpHeight * k, fits ? beside : trackRight - trackLeft);
        f.v2 = f.v - .003f * k - f.h2;
        if (fits) {
            f.box = {Control::none, sparkLeft, f.v2, trackRight, f.v + f.h};
        } else {
            const float top = f.v2 - .004f * k;
            f.box = {Control::none, trackLeft, top - .012f * k, trackRight, top};
        }
        c = f.box.v0;
        helpUnder(fpsHelp, f.help, false);
        l.items.push_back(std::move(f));
        // Update 24: the session's HIGH, LOW and AVG, and RESET at the row's end.
        Item st = itemOf(Kind::fpsStats, Control::fpsReset);
        const float sh = std::max(.017f * k, .014f);
        st.box = {Control::fpsReset, trackRight - .028f, c - gapV - sh, trackRight, c - gapV};
        st.h = fittedHeight("AVG", .0030f * k, .009f);       // a box's name
        st.h2 = fittedHeight("888.8", .0048f * k, .026f);    // its number
        c = st.box.v0;
        helpUnder(helpText(Control::fpsReset), st.help);
        l.items.push_back(std::move(st));
        sections(0, 4);
        // The launch-only part, set apart and labelled, always shown.
        heading(nextLaunchHeading);
        sliderItem(Control::supersample);
        Item n = itemOf(Kind::notice);
        n.text = restartNotice;
        n.h = fittedHeight(n.text, noticeHeight * k, .12f);
        n.v = c - .004f * k - n.h;
        c = n.v;
        l.items.push_back(std::move(n));
        break;
    }
    case colorPage:
        // Update 37.2: COLOR / ATMOSPHERE (eye_color.hpp): live, both eyes alike, default OFF.
        row(Control::colorFx);
        row(Control::colorFilter);
        row(Control::colorStrength);
        sections(0, 2);
        break;
    case controlsPage:
        sections(0, 4);
        break;
    default: { // musicPage
        // Update 19, from the top as in the concept: the song list, the
        // visualizer, the progress, the buttons, the volume.
        Item d = itemOf(Kind::dropdown, Control::musicTrack);
        const float dh = std::max(.020f * k, .016f);
        d.box = {Control::musicTrack, trackLeft, c - gapV - dh, trackRight, c - gapV};
        c = d.box.v0;
        helpUnder(helpText(Control::musicTrack), d.help);
        l.items.push_back(std::move(d));
        Item v = itemOf(Kind::visual);
        v.box = {Control::none, trackLeft, c - gapV - visualHeight, trackRight, c - gapV};
        c = v.box.v0;
        l.items.push_back(std::move(v));
        Item g = itemOf(Kind::progress, Control::musicSeek);
        g.h = fittedHeight(clockWidest, .0042f * k, .030f);
        g.v = c - gapV - g.h;
        const float bh = std::max(.0075f * k, .007f);
        g.box = {Control::musicSeek, trackLeft, g.v - .0025f * k - bh, trackRight, g.v - .0025f * k};
        c = g.box.v0;
        helpUnder(helpText(Control::musicSeek), g.help);
        l.items.push_back(std::move(g));
        {
            const Control row[6] = {Control::musicPrevious, Control::musicRewind, Control::musicPlay,
                                    Control::musicStop,     Control::musicForward, Control::musicNext};
            const float space = .003f, w = (trackRight - trackLeft - 5 * space) / 6;
            const float th = std::max(.018f * k, .016f), top = c - gapV;
            for (int i = 0; i < 6; ++i) {
                Item b = itemOf(Kind::transport, row[i]);
                const float u0 = trackLeft + static_cast<float>(i) * (w + space);
                b.box = {row[i], u0, top - th, u0 + w, top};
                l.items.push_back(std::move(b));
            }
            c = top - th;
            helpUnder(helpText(Control::musicPlay), l.items.back().help); // one line for the row
        }
        sliderItem(Control::musicVolume);
        cycleItem(Control::musicVisualizer);
        Item m = itemOf(Kind::mirror); // SM2TODO: greyed until the game's music can be muted live
        m.text = gameMusicText;
        m.box = {Control::none, trackLeft, c - gapV - .018f * k, trackRight, c - gapV};
        m.h = fittedHeight(m.text, .0046f * k, .12f);
        c = m.box.v0;
        helpUnder(gameMusicHelp, m.help, false);
        l.items.push_back(std::move(m));
        Item st = itemOf(Kind::musicStatus);
        st.h = .0040f * k;
        st.v = c - gapV - st.h;
        st.h2 = .0036f * k;
        st.v2 = st.v - .0025f * k - st.h2;
        c = st.v2;
        l.items.push_back(std::move(st));
        break;
    }
    }
    // Update 39.0.2: EYE SUPERSAMPLE's and MIRROR VIEW's gold RESTART tag, after the name when it fits.
    for (Item& it : l.items) {
        if (!restartTag(it.control) || (it.kind != Kind::slider && it.kind != Kind::toggle))
            continue;
        const float limit = it.kind == Kind::slider ? trackRight - it.numberW - .003f : it.box.u0 - .002f;
        const float u = trackLeft + textWidth(it.text, it.h) + .003f;
        for (float th = std::min(.0032f * k, it.h * .85f); th >= .0022f; th -= .0004f)
            if (u + textWidth(restartTagText, th) + .003f <= limit) {
                it.tagU = u;
                it.tagH = th;
                break;
            }
    }
    footerItem(scrollFooter);
    l.bottom = c - .007f * k;

    // Everything drawn, as rectangles (tests: none overlaps another).
    auto area = [&](float u0, float v0, float u1, float v1, const char* what) {
        l.areas.push_back({u0, v0, u1, v1, what});
    };
    auto centred = [&](const char* t, float v, float h, const char* what) {
        const float w = textWidth(t, h);
        area(-w / 2, v, w / 2, v + h, what);
    };
    auto help = [&](const Help& h) {
        // Update 38.5: the rows shown, at their widest line (it scrolls through them).
        float widest = 0;
        for (const std::string& line : h.lines)
            widest = std::max(widest, helpLineWidth(line, h.height));
        for (int i = 0; i < h.rows && i < static_cast<int>(h.lines.size()); ++i) {
            const float v = h.v - h.pitch * static_cast<float>(i);
            area(trackLeft, v, trackLeft + widest, v + h.height, "an explanation line");
        }
    };
    auto box = [&](const Box& b, const char* what) {
        area(b.u0, b.v0, b.u1, b.v1, what);
    };
    for (const Item& it : l.items) {
        switch (it.kind) {
        case Kind::title:
            centred(it.text, it.v, it.h, "the title");
            break;
        case Kind::guide:
            area(trackLeft, it.v, trackLeft + textWidth(it.text, it.h), it.v + it.h, "a control's name");
            area(trackRight - textWidth("N/A", it.h), it.v, trackRight, it.v + it.h, "a control's mark");
            break;
        case Kind::fpsStats:
            area(trackLeft, it.box.v0, it.box.u0 - .003f, it.box.v1, "the session FPS boxes");
            box(it.box, "RESET");
            break;
        case Kind::fps:
            area(trackLeft, it.v, trackLeft + textWidth(fpsWidest, it.h), it.v + it.h, "the FPS readout");
            area(trackLeft, it.v2, trackLeft + textWidth(gpuWidest, it.h2), it.v2 + it.h2, "the eye GPU line");
            box(it.box, "the FPS sparkline");
            break;
        case Kind::trail:
            box(it.box, "the gesture trail");
            break;
        case Kind::wide:
        case Kind::mirror:
            box(it.box, it.kind == Kind::wide ? "RECALIBRATE" : std::string_view(it.text) == gameMusicText ? "the game music mute" : "the mirror");
            break;
        case Kind::dropdown:
            box(it.box, "the song list");
            break;
        case Kind::visual:
            box(it.box, "the visualizer");
            break;
        case Kind::progress:
            area(trackLeft, it.v, trackLeft + textWidth(clockWidest, it.h), it.v + it.h, "the time played");
            centred(stateWidest, it.v, it.h, "the player's state");
            area(trackRight - textWidth(clockWidest, it.h), it.v, trackRight, it.v + it.h, "the song's length");
            box(it.box, "the progress bar");
            break;
        case Kind::transport:
            box(it.box, "a player button");
            break;
        case Kind::section:
            box(it.box, "a section header");
            break;
        case Kind::banner:
            centred(activeHeroWidest, it.v, it.h, "the ACTIVE HERO line");
            break;
        case Kind::musicStatus:
            area(trackLeft, it.v, trackRight, it.v + it.h, "the music note");
            area(trackLeft, it.v2, trackRight, it.v2 + it.h2, "the music folder");
            break;
        case Kind::slider:
            area(trackLeft, it.v, trackLeft + textWidth(it.text, it.h), it.v + it.h, "a control's name");
            area(trackRight - it.numberW, it.v, trackRight, it.v + it.numberH, "a slider's number");
            area(trackLeft - .005f, it.track - .005f, trackRight + .005f, it.track + .005f, "a slider's track");
            if (it.tagH > 0)
                area(it.tagU, it.v, it.tagU + textWidth(restartTagText, it.tagH) + .003f, it.v + it.h, "a RESTART tag");
            break;
        case Kind::toggle:
            area(trackLeft, it.v, trackLeft + textWidth(it.text, it.h), it.v + it.h, "a control's name");
            box(it.box, "a button");
            if (it.tagH > 0)
                area(it.tagU, it.v, it.tagU + textWidth(restartTagText, it.tagH) + .003f, it.v + it.h, "a RESTART tag");
            break;
        case Kind::cycle:
            area(trackLeft, it.v, trackLeft + textWidth(it.text, it.h), it.v + it.h, "a control's name");
            box(it.box, "a button");
            break;
        case Kind::heading:
            area(trackLeft, it.track - .0004f, trackRight, it.track + .0004f, "the NEXT LAUNCH line");
            centred(it.text, it.v, it.h, "the NEXT LAUNCH heading");
            break;
        case Kind::notice:
            centred(it.text, it.v, it.h, "the restart notice");
            break;
        case Kind::footer:
            centred(it.text, it.v, it.h, "the footer");
            break;
        }
        help(it.help);
    }
    return l;
}
// Per thread (the overlay and the input stage each keep their own), rebuilt
// only when the text size or the open section changes: no allocation in a steady frame.
// Update 39.0.2: a page past the eight is STATUS.
const Layout& layoutOf(int page, float text) {
    const int p = page >= 0 && page < pages ? page : 0;
    const float k = textFactor(text);
    thread_local std::array<Layout, pages> cache;
    thread_local std::array<long, pages> key = [] {
        std::array<long, pages> a{};
        a.fill(-1);
        return a;
    }();
    const int open = openSection(p); // read once: the key and the build agree
    const long want = std::lround(k * 1000) * 16 + (open + 1);
    if (key[static_cast<size_t>(p)] != want) {
        cache[static_cast<size_t>(p)] = build(p, k, open);
        key[static_cast<size_t>(p)] = want;
    }
    return cache[static_cast<size_t>(p)];
}
const Item* itemOf(const Layout& l, Control c) {
    for (const Item& it : l.items)
        if (it.control == c && c != Control::none)
            return &it;
    return nullptr;
}

// 0.0.73 test 4 HERO SETTINGS (the user: "those are still part of the hero page and they should be gone it
// should just be like the tiles with subpages not all the dropdowns its too much you know cluttered"): with
// the holo tiles (HERO MENU HOLO TILES) the old POWERS list - a dropdown per hero - is gone; every row it
// had lives on the hero's tile page, SETTINGS sub-tab, a short page at a time: first the hero's own rows
// (his profile's, but its ON switch: PLAY AS is that), then EVERY HERO's (INVINCIBLE and FLYING), each
// group starting a fresh page. HERO MENU CLASSIC keeps the old list as it was.
constexpr float heroRowPitch = .0165f, heroRowH = .0135f, heroHeadingH = .0095f;
constexpr float heroBoxU0 = .028f, heroBoxU1 = .074f;
constexpr uint32_t heroPageUnits = 6; // a row 1, a slider 2
constexpr Control everyHeroRows[] = {C::invincible,  C::flyMode,      C::combatSupersonic, C::combatSupersonicSpeed,
                                     C::flyVignette, C::flyRoll,      C::flyRollSpeed,     C::flyAutoLevel,
                                     C::combatFlyPose};
struct HeroSettingsPage {
    std::vector<Control> rows;
    bool everyHero{};
};
// A hero's own rows: his old profile's, without its first (the hero's ON switch / PLAY AS SPIDER-MAN).
std::vector<Control> heroOwnRows(uint32_t hero) {
    std::vector<Control> out;
    const SectionDef* def = nullptr;
    for (int s = 0; s < sectionCount(powersPage); ++s)
        if (sectionsOf(powersPage)[s].hero == static_cast<int>(hero))
            def = &sectionsOf(powersPage)[s];
    if (!def)
        return out;
    bool first = true;
    for (const Control c : def->rows) {
        if (c == Control::none)
            break;
        if (first) {
            first = false;
            continue;
        }
        out.push_back(c);
    }
    return out;
}
const std::vector<HeroSettingsPage>& heroPagesOf(uint32_t hero) {
    thread_local std::array<std::vector<HeroSettingsPage>, hero_modes::heroes> cache;
    thread_local std::array<bool, hero_modes::heroes> made{};
    const uint32_t h = std::min<uint32_t>(hero, hero_modes::heroes - 1);
    if (made[h])
        return cache[h];
    auto& pagesOut = cache[h];
    const auto group = [&](const std::vector<Control>& rows, bool every) {
        HeroSettingsPage pg;
        pg.everyHero = every;
        uint32_t units = 0;
        for (const Control c : rows) {
            const uint32_t u = isSliderRow(c) ? 2u : 1u;
            if (units + u > heroPageUnits && !pg.rows.empty()) {
                pagesOut.push_back(pg);
                pg.rows.clear();
                units = 0;
            }
            pg.rows.push_back(c);
            units += u;
        }
        if (!pg.rows.empty())
            pagesOut.push_back(pg);
    };
    group(heroOwnRows(h), false);
    group(std::vector<Control>(std::begin(everyHeroRows), std::end(everyHeroRows)), true);
    made[h] = true;
    return pagesOut;
}
// The SETTINGS page's rows as Items (content() draws them): a heading, then a row each - its name at the
// left, its value button at the right (a press steps it, as the old rows did); a slider its track under it.
Layout heroSettingsLayout(uint32_t hero, uint32_t page) {
    Layout l;
    const auto& all = heroPagesOf(hero);
    if (all.empty())
        return l;
    const HeroSettingsPage& pg = all[std::min<size_t>(page, all.size() - 1)];
    float c = holo_roster::settingsTop;
    {
        Item h = itemOf(Kind::heading);
        static const std::array<std::string, hero_modes::heroes> only = [] {
            std::array<std::string, hero_modes::heroes> a;
            for (uint32_t i = 0; i < hero_modes::heroes; ++i)
                a[i] = std::string(hero_modes::heroNames[i]) + " ONLY";
            return a;
        }();
        h.text = pg.everyHero ? "EVERY HERO: INVINCIBLE AND FLYING"
                              : only[std::min<uint32_t>(hero, hero_modes::heroes - 1)].c_str();
        h.track = c - .001f;
        h.h = fittedHeight(h.text, .0040f, trackRight - trackLeft);
        h.v = h.track - .0030f - h.h;
        c -= heroHeadingH;
        l.items.push_back(std::move(h));
    }
    for (const Control control : pg.rows) {
        if (isSliderRow(control)) {
            Item s = itemOf(Kind::slider, control);
            s.text = labelOf(control);
            s.numberH = fittedHeight(widestNumber(control), .0040f, .050f);
            s.numberW = textWidth(widestNumber(control), s.numberH);
            s.h = fittedHeight(s.text, .0040f, trackRight - trackLeft - s.numberW - .006f);
            s.v = c - .002f - std::max(s.h, s.numberH);
            s.track = s.v - .0085f;
            c -= 2 * heroRowPitch;
            l.items.push_back(std::move(s));
        } else {
            Item t = itemOf(Kind::toggle, control);
            t.text = labelOf(control);
            t.box = {control, heroBoxU0, c - .0015f - heroRowH, heroBoxU1, c - .0015f};
            t.h = fittedHeight(t.text, .0040f, heroBoxU0 - .003f - trackLeft);
            t.v = (t.box.v0 + t.box.v1) / 2 - t.h / 2;
            c -= heroRowPitch;
            l.items.push_back(std::move(t));
        }
    }
    l.bottom = c;
    return l;
}
// The setting row under (u, v) on that page (a toggle's box or its name; a slider's band), else none.
Control heroSettingUnder(uint32_t hero, uint32_t page, float u, float v) {
    if (!std::isfinite(u) || !std::isfinite(v))
        return Control::none;
    for (const Item& it : heroSettingsLayout(hero, page).items) {
        if (it.kind == Kind::toggle) {
            if (u >= trackLeft - .006f && u <= it.box.u1 && v >= it.box.v0 && v <= it.box.v1)
                return it.control;
        } else if (it.kind == Kind::slider) {
            if (u >= trackLeft - .008f && u <= trackRight + .008f && v >= it.track - .0065f && v <= it.v + it.h + .001f)
                return it.control;
        }
    }
    return Control::none;
}

// Update 38.5: `shown`'s lines in `room`'s rows (the layout's), scrolled by
// `scroll` lines: they slide up through the rows and are clipped to them
// (clipBand), so nothing moves outside its rows and nothing pops. When more
// lies below or above, the row at that edge dims (the fade) and a small arrow
// and a bar in the gutter say so. Nothing is dropped.
void helpLines(const Painter& p, const Help& room, const Help& shown, Vec3 color, float scroll = 0) {
    const int all = static_cast<int>(shown.lines.size());
    const int rows = std::max(1, room.rows);
    if (all <= rows) { // it fits: every line, as it always was
        for (int k = 0; k < all; ++k)
            p.text(shown.lines[static_cast<size_t>(k)], trackLeft, room.v - room.pitch * static_cast<float>(k),
                   shown.height, color);
        return;
    }
    const float most = static_cast<float>(all - rows);
    const float off = std::isfinite(scroll) ? std::clamp(scroll, 0.f, most) : 0.f;
    const int first = static_cast<int>(std::floor(off));
    const bool above = off > .01f, below = off < most - .01f;
    const float rowsTop = room.v + room.height * 1.15f;
    const float rowsBottom = room.v - room.pitch * static_cast<float>(rows - 1) - room.height * .3f;
    const size_t from = p.out.size();
    for (int k = first; k <= first + rows && k < all; ++k) {
        const float row = static_cast<float>(k) - off; // 0 the top row
        float w = 1;
        if (below && row > static_cast<float>(rows) - 2)
            w = 1 - .45f * std::min(1.f, row - static_cast<float>(rows - 2)); // the fade toward the bottom
        if (above && row < 1)
            w = std::min(w, 1 - .45f * std::min(1.f, 1 - row)); // and toward the top
        p.text(shown.lines[static_cast<size_t>(k)], trackLeft, room.v - room.pitch * row, shown.height, color * w);
    }
    clipBand(p.out, from, p.f, p.s, rowsBottom + p.dv, rowsTop + p.dv);
    // The gutter: arrows and a bar with its thumb.
    const float u = trackRight - helpGutter * .45f;
    const float top = room.v + room.height, bottom = room.v - room.pitch * static_cast<float>(rows - 1);
    const float a = room.height * .45f;
    if (above)
        p.triangle(u - a, top - a, u + a, top - a, u, top, color, .9f);
    if (below)
        p.triangle(u - a, bottom + a, u + a, bottom + a, u, bottom, color, .9f);
    const float t0 = top - a * 1.4f, t1 = bottom + a * 1.4f;
    if (t0 > t1) {
        p.bar(u, t0, u, t1, .0004f, color, .35f);
        const float span = (t0 - t1) * static_cast<float>(rows) / static_cast<float>(all);
        const float at = t0 - (t0 - t1 - span) * (off / most);
        p.bar(u, at, u, at - span, .0011f, color, .9f);
    }
}
// The FPS monitor's bands (fpsBand), in the theme's colours; unknown in its line colour.
Vec3 bandColor(const Palette& c, int band) {
    return band == 2 ? c.good : band == 1 ? c.warn : band == 0 ? c.bad : c.line;
}
std::string whole(float value) {
    if (!std::isfinite(value) || value < 0 || value > 999)
        return "--";
    return std::to_string(static_cast<int>(std::lround(value)));
}
// Update 16: page 2's FPS monitor: "FPS 44 / 90 HZ" in its band's colour
// (Update 17: and its band's word, OK, LOW or POOR), the eye passes' GPU
// time, and the last 5 s as a sparkline with the refresh as a line.
// Everything not known shows "--"; no number is made up.
void fpsMonitor(const Painter& p, const Item& it, const View& view) {
    const int band = fpsBand(view.fps, view.refreshHz);
    std::string readout = "FPS " + whole(view.fps) + " / " + whole(view.refreshHz) + " HZ";
    if (*fpsWord(band))
        readout += std::string(" ") + fpsWord(band);
    p.text(readout, trackLeft, it.v, it.h, bandColor(p.c, band));
    char gpu[48];
    if (view.eyeGpuMs == 0)
        std::snprintf(gpu, sizeof gpu, "EYE PASSES OFF"); // no AA, history or sharpening on
    else if (std::isfinite(view.eyeGpuMs) && view.eyeGpuMs > 0 && view.eyeGpuMs < 100)
        std::snprintf(gpu, sizeof gpu, "EYE PASSES %.2f MS GPU", view.eyeGpuMs);
    else
        std::snprintf(gpu, sizeof gpu, "EYE PASSES -- MS GPU");
    p.text(gpu, trackLeft, it.v2, it.h2, p.c.help);
    const Box& b = it.box;
    p.rect(b.u0, b.v0, b.u1, b.v1, p.c.backing, .7f);
    p.bar(b.u0, b.v0, b.u1, b.v0, .0005f, p.c.accent, .5f);
    float top = std::isfinite(view.refreshHz) && view.refreshHz > 0 ? view.refreshHz * 1.15f : 0.f;
    for (const float f : view.fpsTrail)
        if (std::isfinite(f))
            top = std::max(top, f * 1.05f);
    if (!(top > 0))
        return; // nothing measured yet: an empty box
    const auto y = [&](float f) {
        return b.v0 + .0008f + (b.v1 - b.v0 - .0016f) * std::clamp(f / top, 0.f, 1.f);
    };
    if (std::isfinite(view.refreshHz) && view.refreshHz > 0)
        p.bar(b.u0, y(view.refreshHz), b.u1, y(view.refreshHz), .0004f, p.c.text, .35f);
    const size_t n = view.fpsTrail.size();
    const float step = (b.u1 - b.u0) / static_cast<float>(n - 1);
    for (size_t k = 1; k < n; ++k) {
        const float a = view.fpsTrail[k - 1], c = view.fpsTrail[k];
        if (!std::isfinite(a) || !std::isfinite(c))
            continue;
        p.bar(b.u0 + step * static_cast<float>(k - 1), y(a), b.u0 + step * static_cast<float>(k), y(c), .0007f,
              bandColor(p.c, fpsBand(c, view.refreshHz)), .95f);
    }
}

void button(const Painter& p, const Box& b, std::string label, const View& view, float k = 1) {
    const bool hover = view.hover == b.control, held = view.held == b.control;
    if (off(view, b.control))
        label = "N/A";
    p.rect(b.u0, b.v0, b.u1, b.v1, p.c.backing, .75f);
    p.rect(b.u0, b.v0, b.u1, b.v1, held ? p.c.line : p.c.accent, held ? .5f : hover ? .3f : .12f);
    const Vec3 edge = hover || held ? p.c.text : p.c.accent;
    const float w = hover || held ? .0011f : .0007f, a = hover || held ? .95f : .8f;
    p.bar(b.u0, b.v0, b.u1, b.v0, w, edge, a);
    p.bar(b.u1, b.v0, b.u1, b.v1, w, edge, a);
    p.bar(b.u1, b.v1, b.u0, b.v1, w, edge, a);
    p.bar(b.u0, b.v1, b.u0, b.v0, w, edge, a);
    if (hover) // a glow round it
        p.bar(b.u0, (b.v0 + b.v1) / 2, b.u1, (b.v0 + b.v1) / 2, (b.v1 - b.v0) + .006f, p.c.line, .06f);
    if (label.empty())
        return;
    const float h = std::min(fittedHeight(label, .0052f * k, (b.u1 - b.u0) - .006f), (b.v1 - b.v0) * .62f);
    p.text(label, (b.u0 + b.u1) / 2 - textWidth(label, h) / 2, (b.v0 + b.v1) / 2 - h / 2, h, p.c.text);
}

// Update 24: the session's HIGH, LOW and AVG in three boxes, each its name
// over its number ("--" until a whole second has been tracked), and RESET.
void fpsStats(const Painter& p, const Item& it, const View& view, float k) {
    const Box& r = it.box;
    const float space = .003f, right = r.u0 - space, w = (right - trackLeft - 2 * space) / 3;
    const char* names[3] = {"HIGH", "LOW", "AVG"};
    const float values[3] = {view.fpsHigh, view.fpsLow, view.fpsAvg};
    for (int i = 0; i < 3; ++i) {
        const float u0 = trackLeft + static_cast<float>(i) * (w + space), u1 = u0 + w;
        p.rect(u0, r.v0, u1, r.v1, p.c.backing, .7f);
        p.bar(u0, r.v0, u1, r.v0, .0005f, p.c.accent, .5f);
        const float mid = (u0 + u1) / 2;
        p.text(names[i], mid - textWidth(names[i], it.h) / 2, r.v1 - .0015f - it.h, it.h, p.c.label);
        char n[16];
        if (std::isfinite(values[i]) && values[i] > 0)
            std::snprintf(n, sizeof n, "%.1f", static_cast<double>(std::min(values[i], 999.f)));
        else
            std::snprintf(n, sizeof n, "--");
        const int band = fpsBand(values[i], view.refreshHz);
        p.text(n, mid - textWidth(n, it.h2) / 2, r.v0 + .0012f, it.h2, band < 0 ? p.c.text : bandColor(p.c, band));
    }
    button(p, r, "RESET", view, k);
}

// Update 18: the < > of a stepping button (arrows), at its two ends.
void arrowsOn(const Painter& p, const Box& b, Vec3 color, float alpha) {
    const float h = (b.v1 - b.v0) * .34f, w = h * .8f, mid = (b.v0 + b.v1) / 2, inset = .0035f;
    p.triangle(b.u0 + inset, mid, b.u0 + inset + w, mid + h / 2, b.u0 + inset + w, mid - h / 2, color, alpha);
    p.triangle(b.u1 - inset, mid, b.u1 - inset - w, mid - h / 2, b.u1 - inset - w, mid + h / 2, color, alpha);
}
void sliderRow(const Painter& p, const Item& it, const View& view) {
    const float value = view.settings[it.control];
    const Range r = range(it.control);
    const bool hover = view.hover == it.control, held = view.held == it.control;
    const float track = it.track;
    p.text(it.text, trackLeft, it.v, it.h, hover || held ? p.c.text : p.c.label);
    const bool disabled = off(view, it.control);
    const std::string number =
        it.control == Control::shadowDistance
            ? shadow_distance::valueText(view.shadowDistance, value, view.shadowDistanceGame) // Update 33
        : disabled ? "UNAVAILABLE"
                   : valueText(it.control, value);
    // Never wider than the room kept for the widest (UNAVAILABLE shrinks).
    const float nh = fittedHeight(number, it.numberH, it.numberW);
    p.text(number, trackRight - textWidth(number, nh), it.v, nh, p.c.value);
    const float f = std::isfinite(value) ? std::clamp((value - r.low) / (r.high - r.low), 0.f, 1.f) : 0.f;
    const float knob = trackLeft + (trackRight - trackLeft) * f;
    p.rect(trackLeft, track - .0011f, trackRight, track + .0011f, p.c.accent, .3f);
    p.rect(trackLeft, track - .0011f, knob, track + .0011f, disabled ? p.c.backing : p.c.accent, .9f);
    for (const float end : {trackLeft, trackRight})
        p.bar(end, track - .003f, end, track + .003f, .0006f, p.c.line, .7f);
    if (hover || held)
        p.ring(knob, track, 1, 1, 0, .0085f, p.c.line, held ? .3f : .16f);
    p.ring(knob, track, 1, 1, 0, held ? .0050f : .0042f, p.c.backing, .95f);
    p.ring(knob, track, 1, 1, .0030f, held ? .0050f : .0042f, held || hover ? p.c.text : p.c.line, .95f);
    p.ring(knob, track, 1, 1, 0, .0016f, p.c.frame, 1, 16);
}

// Update 19: the MUSIC page.
// Text the panel can draw: what it has no glyph for becomes a space ('_' a
// dash, '&' a plus).
std::string panelText(const std::string& t) {
    std::string out;
    out.reserve(t.size());
    for (const char ch : t)
        out += ch == '_' ? '-' : ch == '&' ? '+' : drawable(ch) ? ch : ' ';
    return out;
}
Vec3 hueColor(float h, float value, bool mono) {
    h = std::isfinite(h) ? h - std::floor(h) : 0.f;
    Vec3 c{std::clamp(std::abs(h * 6 - 3) - 1, 0.f, 1.f), std::clamp(2 - std::abs(h * 6 - 2), 0.f, 1.f),
           std::clamp(2 - std::abs(h * 6 - 4), 0.f, 1.f)};
    if (mono) {
        const float g = (c.x + c.y + c.z) / 3 * .5f + .45f;
        c = {g, g, g};
    }
    return c * std::clamp(value, 0.f, 1.f);
}
float unit(float x) {
    return std::isfinite(x) ? std::clamp(x, 0.f, 1.f) : 0.f;
}
float fract(float x) {
    return x - std::floor(x);
}
// The five visualizers (music::visualizerName), inside the box: only flat
// shapes, a few thousand vertices at most, whatever the spectrum holds.
void visualize(const Painter& p, int kind, float u0, float v0, float u1, float v1, const music::Spectrum& sp,
               float seconds, bool mono) {
    using music::bands;
    using music::wavePoints;
    const float cu = (u0 + u1) / 2, cv = (v0 + v1) / 2, hw = (u1 - u0) / 2 - .0015f, hh = (v1 - v0) / 2 - .0015f;
    if (!(hw > 0 && hh > 0) || !std::isfinite(cu) || !std::isfinite(cv))
        return;
    std::array<float, bands> b{}, pk{};
    for (int i = 0; i < bands; ++i) {
        b[static_cast<size_t>(i)] = unit(sp.band[static_cast<size_t>(i)]);
        pk[static_cast<size_t>(i)] = unit(sp.peak[static_cast<size_t>(i)]);
    }
    const auto band = [&](int i) {
        return b[static_cast<size_t>(std::clamp(i, 0, bands - 1))];
    };
    const float level = unit(sp.level), bass = unit(sp.bass);
    const float travel = std::isfinite(sp.travel) ? std::fmod(sp.travel, 1000.f) : 0.f;
    const float t = std::isfinite(seconds) ? std::fmod(seconds, 1000.f) : 0.f;
    constexpr float tau = 6.2831853f;
    p.rect(u0, v0, u1, v1, p.c.backing, .88f);
    switch (kind) {
    case 0: { // ORIGINAL: red/blue bars mirrored from the middle, the waveform over them
        const float w = hw / bands;
        for (int i = 0; i < bands; ++i) {
            const float x0 = w * (static_cast<float>(i) + .18f), x1 = w * (static_cast<float>(i) + .82f);
            const float h = std::max(.0005f, band(i) * hh * .9f);
            const Vec3 col = p.c.frame + (p.c.accent - p.c.frame) * (static_cast<float>(i) / (bands - 1));
            p.rect(cu + x0, cv - h, cu + x1, cv + h, col, .85f);
            p.rect(cu - x1, cv - h, cu - x0, cv + h, col, .85f);
        }
        const auto y = [&](int k) {
            const float w_ = sp.wave[static_cast<size_t>(k)];
            return cv + (std::isfinite(w_) ? std::clamp(w_, -1.f, 1.f) : 0.f) * hh * .8f;
        };
        const float step = 2 * hw / (wavePoints - 1);
        for (int k = 1; k < wavePoints; ++k)
            p.line(cu - hw + step * static_cast<float>(k - 1), y(k - 1), cu - hw + step * static_cast<float>(k), y(k),
                   .0006f, p.c.text, .75f);
        break;
    }
    case 1: { // BARS: LED segments, a falling peak on each
        constexpr int segs = 12;
        const float w = 2 * hw / bands, sh = 2 * hh / segs, base = cv - hh;
        for (int i = 0; i < bands; ++i) {
            const float x0 = cu - hw + w * (static_cast<float>(i) + .12f), x1 = x0 + w * .76f;
            const int lit = static_cast<int>(std::lround(band(i) * segs));
            for (int g = 0; g < lit; ++g) {
                const float y0 = base + sh * (static_cast<float>(g) + .15f), f = static_cast<float>(g) / (segs - 1);
                const Vec3 col = f < .5f ? p.c.accent + (p.c.line - p.c.accent) * (f * 2)
                                         : p.c.line + (p.c.frame - p.c.line) * ((f - .5f) * 2);
                p.rect(x0, y0, x1, y0 + sh * .7f, col, .9f);
            }
            const float y = std::min(base + pk[static_cast<size_t>(i)] * 2 * hh, cv + hh - .0005f);
            if (pk[static_cast<size_t>(i)] > .02f)
                p.rect(x0, y - .0004f, x1, y + .0004f, p.c.text, .9f);
        }
        break;
    }
    case 2: { // FRACTAL BLOOM: petals with smaller blooms on their tips, hues cycling
        const auto petal = [&](float x, float y, float a, float r0, float r1, float half, Vec3 col, float al) {
            const float rm = r0 + (r1 - r0) * .45f;
            p.flatQuad(x + std::cos(a) * r0, y + std::sin(a) * r0, x + std::cos(a - half) * rm,
                       y + std::sin(a - half) * rm, x + std::cos(a) * r1, y + std::sin(a) * r1,
                       x + std::cos(a + half) * rm, y + std::sin(a + half) * rm, col, al);
        };
        const auto bloom = [&](float x, float y, float R, float spin, int from, float hue) {
            constexpr int P = 8;
            for (int layer = 0; layer < 2; ++layer) {
                for (int j = 0; j < P; ++j) {
                    const float a = spin * (layer ? -1.f : 1.f) +
                                    (static_cast<float>(j) + .5f * static_cast<float>(layer)) * tau / P;
                    const float bj = band((from + j * 3 + layer * 2) % bands);
                    const float r1 = R * (layer ? .55f : 1.f) * (.45f + .55f * bj) * (1 + .12f * bass);
                    petal(x, y, a, R * .12f, r1, 3.14159f / P * (layer ? .6f : .8f),
                          hueColor(hue + static_cast<float>(j) / P * .5f + .33f * static_cast<float>(layer) + t * .07f,
                                   .6f + .4f * bj, mono),
                          .55f);
                    if (layer)
                        continue;
                    const float sx = x + std::cos(a) * r1, sy = y + std::sin(a) * r1, rs = r1 * .3f;
                    for (int q = 0; q < 5; ++q) {
                        const float a2 = a - spin * 2 + static_cast<float>(q) * tau / 5;
                        petal(sx, sy, a2, 0, rs * (.5f + .5f * band((from + j * 4 + q * 7) % bands)), 3.14159f / 5 * .7f,
                              hueColor(hue + .5f + static_cast<float>(q) * .1f + t * .1f, .9f, mono), .6f);
                    }
                }
            }
            p.ring(x, y, 1, 1, 0, R * (.1f + .15f * bass), hueColor(hue + t * .2f, 1, mono), .8f, 16);
        };
        const float spin = travel * .5f + t * .05f;
        bloom(cu, cv, hh / 1.46f, spin, 0, 0);
        bloom(cu - hw * .62f, cv, hh * .7f / 1.46f, -spin * 1.3f, 11, .33f);
        bloom(cu + hw * .62f, cv, hh * .7f / 1.46f, -spin * 1.3f, 21, .66f);
        break;
    }
    case 3: { // TUNNEL: rings rushing toward you, twisting, lit by the bands
        constexpr int N = 9, K = 24;
        for (int i = 0; i < N; ++i) {
            const float d = fract(static_cast<float>(i) / N + travel * .3f), r = d * d;
            const float inner = r, outer = std::min(1.f, r + .03f + .1f * r);
            const float twist = travel * .4f + (1 - d) * 1.5f, fade = std::min(1.f, d * 4);
            for (int k = 0; k < K; ++k) {
                const float a0 = twist + static_cast<float>(k) * tau / K, a1 = a0 + tau / K * .82f;
                const float bk = band((k < K / 2 ? k : K - 1 - k) * bands / (K / 2));
                const Vec3 col = hueColor(t * .05f + static_cast<float>(i) * .11f + static_cast<float>(k) / K * .3f,
                                          .35f + .65f * bk, mono);
                p.flatQuad(cu + std::cos(a0) * hw * inner, cv + std::sin(a0) * hh * inner, cu + std::cos(a0) * hw * outer,
                           cv + std::sin(a0) * hh * outer, cu + std::cos(a1) * hw * outer, cv + std::sin(a1) * hh * outer,
                           cu + std::cos(a1) * hw * inner, cv + std::sin(a1) * hh * inner, col, (.25f + .75f * bk) * fade);
            }
        }
        p.ring(cu, cv, 1, 1, 0, hh * (.08f + .12f * bass), p.c.text, .5f, 16);
        break;
    }
    default: { // STARBURST: rays by the bands, sparks flying out, a core that beats with the bass
        constexpr int R = 48, sparks = 40;
        for (int k = 0; k < R; ++k) {
            const float a = static_cast<float>(k) * tau / R + travel * .12f;
            const float bk = band((k < R / 2 ? k : R - 1 - k) * bands / (R / 2));
            const float L = .18f + .82f * bk;
            p.flat(cu + std::cos(a + .05f) * hw * .1f, cv + std::sin(a + .05f) * hh * .1f, cu + std::cos(a) * hw * L,
                   cv + std::sin(a) * hh * L, cu + std::cos(a - .05f) * hw * .1f, cv + std::sin(a - .05f) * hh * .1f,
                   hueColor(t * .1f + static_cast<float>(k) / R, .8f + .2f * bk, mono), .8f);
        }
        for (int j = 0; j < sparks; ++j) {
            const float h1 = fract(std::sin(static_cast<float>(j) * 12.9898f) * 43758.5453f);
            const float h2 = fract(std::sin(static_cast<float>(j) * 78.233f) * 12543.123f);
            const float h3 = fract(std::sin(static_cast<float>(j) * 39.425f) * 24634.634f);
            const float ph = fract(h2 + travel * (.3f + .5f * h3));
            const float a = tau * h1 + t * .02f, r = .1f + .82f * ph;
            const float x = cu + std::cos(a) * hw * r, y = cv + std::sin(a) * hh * r;
            const float sz = .0005f + .0012f * ph * (.5f + level);
            p.flatQuad(x - sz, y, x, y + sz, x + sz, y, x, y - sz, hueColor(h1 + t * .1f, 1, mono), (1 - ph) * .9f);
        }
        const float core = hh * (.12f + .25f * bass);
        p.ring(cu, cv, 1, 1, 0, core, p.c.text, .9f, 24);
        p.ring(cu, cv, 1, 1, core, core + hh * (.04f + .05f * bass), p.c.frame, .6f, 24);
        break;
    }
    }
    p.bar(u0, v0, u1, v0, .0005f, p.c.accent, .45f);
    p.bar(u0, v1, u1, v1, .0005f, p.c.accent, .45f);
}
void dropdown(const Painter& p, const Item& it, const View& view, float k) {
    const MusicView& m = view.music;
    std::string label = !m.available ? "MUSIC OFF"
                        : m.tracks <= 0 ? "NO SONGS YET"
                        : m.track >= 0 && !m.song.empty()
                            ? std::to_string(m.track + 1) + "/" + std::to_string(m.tracks) + "  " + panelText(m.song)
                            : "PICK A SONG (" + std::to_string(m.tracks) + ")";
    button(p, it.box, "", view, k);
    const Box& b = it.box;
    const float mid = (b.v0 + b.v1) / 2;
    const float th = std::min(fittedHeight(label, .0050f * k, b.u1 - b.u0 - .016f), (b.v1 - b.v0) * .55f);
    const bool hover = view.hover == it.control || view.held == it.control;
    p.text(label, b.u0 + .004f, mid - th / 2, th, hover ? p.c.text : p.c.value);
    const float c = b.u1 - .0065f, sz = std::min(.0030f, (b.v1 - b.v0) * .2f);
    const Vec3 col = hover ? p.c.text : p.c.line;
    if (m.listOpen) // open: points up
        p.triangle(c - sz, mid - sz * .6f, c + sz, mid - sz * .6f, c, mid + sz * .8f, col, .95f);
    else
        p.triangle(c - sz, mid + sz * .6f, c + sz, mid + sz * .6f, c, mid - sz * .8f, col, .95f);
}
void progress(const Painter& p, const Item& it, const View& view) {
    const MusicView& m = view.music;
    const bool known = std::isfinite(m.total) && m.total > 0;
    const std::string el = music::clockText(m.elapsed), tot = known ? music::clockText(m.total) : "--:--";
    const char* state = !m.available ? "OFF" : m.playing ? "PLAYING" : m.paused ? "PAUSED" : "STOPPED";
    p.text(el, trackLeft, it.v, it.h, p.c.value);
    p.text(state, -textWidth(state, it.h) / 2, it.v, it.h, m.playing ? p.c.good : p.c.label);
    p.text(tot, trackRight - textWidth(tot, it.h), it.v, it.h, p.c.value);
    const Box& b = it.box;
    const float mid = (b.v0 + b.v1) / 2, half = std::min(.0011f, (b.v1 - b.v0) / 4);
    const float f = known ? unit(m.elapsed / m.total) : 0.f;
    const float knob = trackLeft + (trackRight - trackLeft) * f;
    const bool hover = view.hover == it.control || view.held == it.control;
    p.rect(trackLeft, mid - half, trackRight, mid + half, p.c.accent, hover ? .45f : .3f);
    p.rect(trackLeft, mid - half, knob, mid + half, p.c.frame, .9f);
    if (known) {
        if (hover)
            p.ring(knob, mid, 1, 1, 0, .0060f, p.c.line, .2f, 24);
        p.ring(knob, mid, 1, 1, 0, .0028f, hover ? p.c.text : p.c.line, .95f, 16);
    }
}
void transport(const Painter& p, const Item& it, const View& view, float k, Vec3 grey) {
    button(p, it.box, "", view, k);
    const Box& b = it.box;
    const float cx = (b.u0 + b.u1) / 2, cy = (b.v0 + b.v1) / 2;
    const float s = std::min((b.v1 - b.v0) * .26f, (b.u1 - b.u0) * .2f);
    const bool hover = view.hover == it.control || view.held == it.control;
    const Vec3 col = off(view, it.control) || !view.music.available ? grey : hover ? p.c.text : p.c.line;
    const auto right = [&](float x) {
        p.triangle(x - s * .8f, cy + s, x - s * .8f, cy - s, x + s * .8f, cy, col, .95f);
    };
    const auto left = [&](float x) {
        p.triangle(x + s * .8f, cy + s, x - s * .8f, cy, x + s * .8f, cy - s, col, .95f);
    };
    const auto upright = [&](float x) {
        p.rect(x - s * .2f, cy - s, x + s * .2f, cy + s, col, .95f);
    };
    switch (it.control) {
    case Control::musicPrevious:
        upright(cx - s * 1.f);
        left(cx + s * .25f);
        break;
    case Control::musicRewind:
        left(cx - s * .7f);
        left(cx + s * .7f);
        break;
    case Control::musicPlay:
        if (view.music.playing) {
            upright(cx - s * .45f);
            upright(cx + s * .45f);
        } else {
            right(cx + s * .1f);
        }
        break;
    case Control::musicStop:
        p.rect(cx - s * .8f, cy - s * .8f, cx + s * .8f, cy + s * .8f, col, .95f);
        break;
    case Control::musicForward:
        right(cx - s * .7f);
        right(cx + s * .7f);
        break;
    default: // next
        right(cx - s * .25f);
        upright(cx + s * 1.f);
        break;
    }
}
void musicStatus(const Painter& p, const Item& it, const View& view) {
    const MusicView& m = view.music;
    const bool noted = !m.note.empty();
    const std::string line = noted         ? panelText(m.note)
                             : !m.available ? std::string("MUSIC OFF")
                             : m.tracks <= 0 ? std::string("NO SONGS YET: ADD MP3, WAV, FLAC OR M4A FILES")
                                             : std::to_string(m.tracks) + (m.tracks == 1 ? " SONG" : " SONGS") +
                                                   " IN YOUR MUSIC FOLDER";
    const float w = trackRight - trackLeft;
    p.text(line, trackLeft, it.v, fittedHeight(line, it.h, w), noted ? p.c.warn : p.c.help);
    // The font has no backslash: C:\sm2-port\Music shows as C:/SM2-PORT/MUSIC.
    std::string path = m.folder;
    std::replace(path.begin(), path.end(), '\\', '/');
    const std::string folder =
        "FOLDER: " + (path.empty() ? std::string("MUSIC, BESIDE THE LAUNCHER") : panelText(path));
    p.text(folder, trackLeft, it.v2, fittedHeight(folder, it.h2, w), p.c.help);
}

// Update 27: ADDRESS FINDER button text from View (not Settings).
std::string addressFinderValue(const View& view) {
    char n[32];
    switch (view.addressFinderStatus) {
    case 1:
        std::snprintf(n, sizeof n, "ON %u S", view.addressFinderSeconds);
        return n;
    case 2:
        // Update 28: the confirmed count alone read as just "1"; the near ones too.
        // Update 30: and how many depth-only (likely shadow) passes bound camera data.
        if (view.addressFinderShadow)
            std::snprintf(n, sizeof n, "DONE %u +%u, %u SHADOW", view.addressFinderCandidates,
                          view.addressFinderAlmost, view.addressFinderShadow);
        else if (view.addressFinderAlmost)
            std::snprintf(n, sizeof n, "DONE %u +%u ALMOST", view.addressFinderCandidates,
                          view.addressFinderAlmost);
        else
            std::snprintf(n, sizeof n, "DONE %u", view.addressFinderCandidates);
        return n;
    case 3:
        return "OFF (FAILED)";
    default:
        return "START";
    }
}
// Update 28: EYE LOD button text from View (SpidyEyeLodNow).
std::string eyeLodValue(const View& view) {
    return view.eyeLodStatus == 2 ? "OFF (FAILED)" : view.eyeLodStatus == 1 ? "ON" : "OFF";
}
// Update 33: EYE AA button text from View (eye_taa.hpp).
std::string eyeAaValue(const View& view) {
    char t[32];
    eye_taa::valueText(view.eyeAa, view.eyeAaMs, t, sizeof t);
    return t;
}
// Update 34: ENGINE TAA button text from View (engine_taa.hpp).
std::string engineTaaValue(const View& view) {
    char t[32];
    engine_taa::valueText(view.engineTaa, t, sizeof t);
    return t;
}
const Help& engineTaaOffLines(const Help& help, uint32_t reason) {
    thread_local Help cached;
    thread_local uint32_t cachedReason = ~0u;
    thread_local float cachedHeight = -1;
    if (cachedReason != reason || cachedHeight != help.height) {
        cached = help;
        cached.lines = wrapText(engineTaaOffHelp(reason), help.height, helpWidth - helpGutter);
        cachedReason = reason;
        cachedHeight = help.height;
    }
    cached.v = help.v;
    cached.pitch = help.pitch;
    return cached;
}
const Help& eyeAaFailedLines(const Help& help, uint32_t reason) {
    thread_local Help cached;
    thread_local uint32_t cachedReason = ~0u;
    thread_local float cachedHeight = -1;
    if (cachedReason != reason || cachedHeight != help.height) {
        cached = help;
        cached.lines = wrapText(eyeAaFailedHelp(reason), help.height, helpWidth - helpGutter);
        cachedReason = reason;
        cachedHeight = help.height;
    }
    cached.v = help.v;
    cached.pitch = help.pitch;
    return cached;
}
// Update 31: SHADOW CAMERA button text from View (SpidyShadowCameraNow).
std::string shadowCameraValue(const View& view) {
    const uint32_t mode = view.shadowCamera & 0xff;
    return !mode && (view.shadowCamera >> 8) ? "OFF (REVERTED)" : shadow_camera::modeName(mode);
}
const Help& shadowCameraRevertedLines(const Help& help, uint32_t reason) {
    thread_local Help cached;
    thread_local uint32_t cachedReason = ~0u;
    thread_local float cachedHeight = -1;
    if (cachedReason != reason || cachedHeight != help.height) {
        cached = help;
        cached.lines = wrapText(shadowCameraRevertedHelp(reason), help.height, helpWidth - helpGutter);
        cachedReason = reason;
        cachedHeight = help.height;
    }
    cached.v = help.v;
    cached.pitch = help.pitch;
    return cached;
}
// Update 35: FAR SHADOWS' button text and its subtext for its state (once per state
// and text size, per thread).
std::string farShadowsValue(const View& view) {
    char text[32];
    far_shadows::valueText(view.farShadows, text, sizeof text);
    return text;
}
// Update 36: EYE BLOOM's and SHARED LOD's button text.
std::string eyeBloomValue(const View& view) {
    char text[32];
    eye_bloom::valueText(view.eyeBloom, text, sizeof text);
    return text;
}
std::string mirrorValue(const View& view) {
    if (!(view.mirror & 1))
        return view.mirror & 4 ? "AUTO OFF" : "OFF"; // Test build 10: it turned itself off (the note says why)
    char text[32];
    std::snprintf(text, sizeof text, view.mirror & 2 ? "45HZ %.2f MS" : "ON %.2f MS", static_cast<double>(view.mirror >> 8) / 1000.0);
    return text;
}
std::string worldLogValue(const View& view) {
    if (view.worldLog & 2)
        return "STOPPED";
    if (!(view.worldLog & 1))
        return "OFF";
    return "ON " + std::to_string(view.worldLog >> 8);
}
std::string mirrorAnchorValue(const View& view) { return view.mirrorAnchor == 1 ? "WORLD" : "PLAYSPACE"; }
std::string npcFadeValue(const View& view) {
    char text[32];
    npc_fade::valueText(view.npcFade & 0xff, view.npcFade >> 8 & 0xff, text, sizeof text);
    return text;
}
// SLOT TEST / WEB BALL TEST: the input picked, and whether it waits or was sent.
std::string testValue(uint32_t packed) {
    const uint32_t step = packed & 0xff;
    if (!step || step > combat_input::testCount)
        return "PRESS";
    return std::string(combat_input::testSteps[step - 1].name) + (packed & 0x100 ? " IN 1.5S" : packed & 0x200 ? " SENT" : "");
}
std::string webBallValue(const View& view) {
    const char* bloom = (view.webBall >> 16) == eye_bloom::copy ? "COPY" : (view.webBall >> 16) == eye_bloom::off ? "OFF" : "GATE";
    if (!(view.webBall & 0xffff))
        return std::string("BLOOM ") + bloom;
    return std::string("BLOOM ") + bloom + ((view.webBall & 0x100) ? " IN 1.5S" : " SHOT");
}
// Update 37.2: COLOR FX / FILTER / FOVEATED's buttons.
std::string colorValue(const View& view) {
    if (view.color & 0x10000)
        return "REFUSED";
    return view.color & 1 ? "ON" : "OFF";
}
std::string foveaValue(const View& view) {
    const uint32_t level = view.fovea & 0xff, fault = view.fovea >> 8 & 0xff;
    if (fault)
        return "OFF: FAILED";
    if (level && !(view.fovea & 0x20000))
        return std::string(foveation::levelName(level)) + " (WAIT)";
    return foveation::levelName(level);
}
std::string foveaStatsValue(const View& view) {
    if (!(view.foveaFps[2] > 0))
        return "NO DATA";
    char text[48];
    if (view.foveaGpuMs > 0)
        std::snprintf(text, sizeof text, "%.0f/%.0f/%.0f %.1fMS", static_cast<double>(view.foveaFps[0]),
                      static_cast<double>(view.foveaFps[1]), static_cast<double>(view.foveaFps[2]),
                      static_cast<double>(view.foveaGpuMs));
    else
        std::snprintf(text, sizeof text, "%.0f/%.0f/%.0f", static_cast<double>(view.foveaFps[0]),
                      static_cast<double>(view.foveaFps[1]), static_cast<double>(view.foveaFps[2]));
    return text;
}
std::string bloodValue(const View& view) {
    if (!view.blood)
        return "OFF";
    return std::string(blood_splatter::stepName(view.blood)) + " " + std::to_string(view.blood) + "%";
}
std::string sharedLodValue(const View& view) {
    char text[32];
    shared_lod::valueText(view.sharedLod, text, sizeof text);
    return text;
}
// Update 36: their subtext for a state (once per state and text size, per thread).
const Help& stateLines(const Help& help, uint32_t key, const char* text) {
    thread_local Help cached;
    thread_local uint32_t cachedKey = ~0u;
    thread_local float cachedHeight = -1;
    if (cachedKey != key || cachedHeight != help.height) {
        cached = help;
        cached.lines = wrapText(text, help.height, helpWidth - helpGutter);
        cachedKey = key;
        cachedHeight = help.height;
    }
    return cached;
}
const Help& farShadowsLines(const Help& help, uint32_t packed) {
    thread_local Help cached;
    thread_local uint32_t cachedPacked = ~0u;
    thread_local float cachedHeight = -1;
    if (cachedPacked != packed || cachedHeight != help.height) {
        cached = help;
        cached.lines = wrapText(farShadowsHelp(packed), help.height, helpWidth - helpGutter);
        cachedPacked = packed;
        cachedHeight = help.height;
    }
    cached.v = help.v;
    cached.pitch = help.pitch;
    return cached;
}
// Update 36: COLOR THEME's subtext while 2000s Raimi is on (raimiHelp).
const Help& raimiLines(const Help& help) {
    thread_local Help cached;
    thread_local float cachedHeight = -1;
    if (cachedHeight != help.height) {
        cached = help;
        cached.lines = wrapText(raimiHelp, help.height, helpWidth - helpGutter);
        cachedHeight = help.height;
    }
    cached.v = help.v;
    cached.pitch = help.pitch;
    return cached;
}
// Update 28.1: EYE LOD's failed subtext, wrapped like its help (once per
// reason and text size, per thread: no allocation in a steady frame).
const Help& eyeLodFailedLines(const Help& help, uint32_t reason) {
    thread_local Help cached;
    thread_local uint32_t cachedReason = ~0u;
    thread_local float cachedHeight = -1;
    if (cachedReason != reason || cachedHeight != help.height) {
        cached = help;
        cached.lines = wrapText(eyeLodFailedHelp(reason), help.height, helpWidth - helpGutter);
        cachedReason = reason;
        cachedHeight = help.height;
    }
    cached.v = help.v;
    cached.pitch = help.pitch;
    return cached;
}

// Update 34.2: TAA JITTER's subtext while inactive (once per size, per thread).
const Help& taaJitterInactiveLines(const Help& help) {
    thread_local Help cached;
    thread_local float cachedHeight = -1;
    if (cachedHeight != help.height) {
        cached = help;
        cached.lines = wrapText(engine_taa::jitterInactiveHelp, help.height, helpWidth - helpGutter);
        cachedHeight = help.height;
    }
    cached.v = help.v;
    cached.pitch = help.pitch;
    return cached;
}
// Update 33: SHADOW DISTANCE's subtext follows its state (once per text and size, per thread).
const Help& shadowDistanceLines(const Help& help, const View& view) {
    thread_local Help cached;
    thread_local std::string cachedText;
    thread_local float cachedHeight = -1;
    std::string text = shadow_distance::helpText(view.shadowDistance, view.shadowDistanceLive, view.shadowDistanceGame);
    if (text != cachedText || cachedHeight != help.height) {
        cached = help;
        cached.lines = wrapText(text, help.height, helpWidth - helpGutter);
        cachedText = std::move(text);
        cachedHeight = help.height;
    }
    cached.v = help.v;
    cached.pitch = help.pitch;
    return cached;
}

// A page's content, drawn by its layout (the Painter scrolled), then clipped.
// Update 38.5: the subtext an item shows for `view`'s state (its own help,
// or a state's text wrapped the same; one place for drawing and helpInfo).
const Help& shownHelp(const Item& it, const View& view) {
    // MIRROR TEST: why MIRROR VIEW turned itself OFF, under it, while it stays OFF.
    if (view.mirrorViewWhy && view.mirrorViewWhy < mirror_view::whys && combatOption(it.control) == combat38::mirrorView &&
        view.combat38[combat38::mirrorView] == 0) {
        thread_local std::string text;
        text = std::string("Turned itself OFF: ") + mirror_view::whyNames[view.mirrorViewWhy] + ". Restart the game to try again.";
        return stateLines(it.help, 0x4d000000u | view.mirrorViewWhy, text.c_str());
    }
    return it.control == Control::eyeLod && it.kind == Kind::toggle && view.eyeLodStatus == 2
                      ? eyeLodFailedLines(it.help, view.eyeLodReason)
                  : it.control == Control::shadowCamera && it.kind == Kind::toggle && !(view.shadowCamera & 0xff) &&
                          (view.shadowCamera >> 8)
                      ? shadowCameraRevertedLines(it.help, view.shadowCamera >> 8)
                  : it.control == Control::eyeAa && it.kind == Kind::toggle && (view.eyeAa & 0xff) == eye_taa::failed
                      ? eyeAaFailedLines(it.help, view.eyeAa >> 8)
                  : it.control == Control::engineTaa && it.kind == Kind::toggle &&
                          engine_taa::stateOf(view.engineTaa) != engine_taa::on && engine_taa::reasonOf(view.engineTaa)
                      ? engineTaaOffLines(it.help, engine_taa::reasonOf(view.engineTaa))
                  : it.control == Control::shadowDistance && it.kind == Kind::slider
                      ? shadowDistanceLines(it.help, view)
                  : it.control == Control::taaJitter && off(view, it.control)
                      ? taaJitterInactiveLines(it.help)
                  : it.control == Control::farShadows && it.kind == Kind::toggle &&
                          far_shadows::reasonOf(view.farShadows)
                      ? farShadowsLines(it.help, view.farShadows)
                  : it.control == Control::eyeBloom && it.kind == Kind::toggle && eye_bloom::reasonOf(view.eyeBloom)
                      ? stateLines(it.help, view.eyeBloom, eyeBloomHelp(view.eyeBloom))
                  : it.control == Control::hudTheme && view.theme == raimiTheme ? raimiLines(it.help)
                  : it.control == Control::sharedLod && it.kind == Kind::toggle &&
                          shared_lod::reasonOf(view.sharedLod)
                      ? stateLines(it.help, view.sharedLod | 1u << 16, sharedLodHelp(view.sharedLod))
                      : it.help;
}
// Update 38.5 GESTURE REMAP: its rows' values.
bool remapRow(Control c) {
    return c >= Control::gestureRemapMove && c <= Control::gestureProfile;
}
std::string remapValue(Control c, const View& view) {
    const gesture_session::View none{};
    const gesture_session::View& r = view.remap ? *view.remap : none;
    switch (c) {
    case Control::gestureRemapMove:
        return std::string(gestures385::gestureName(r.move)) + (r.customMask >> r.move & 1 ? " (MINE)" : "");
    case Control::gestureRecord:
        return r.recording ? "STOP" : r.popup ? "ANSWER BELOW" : "PRESS";
    case Control::gestureKeep:
    case Control::gestureAgain:
    case Control::gestureCancel:
        return "PRESS";
    case Control::gestureResetMove:
        return r.customMask >> r.move & 1 ? "PRESS" : "BUILT-IN";
    case Control::gestureResetAll:
        return r.resetAllArmed ? "PRESS AGAIN" : r.customMask ? "PRESS" : "BUILT-IN";
    case Control::gestureProfile:
        return "PROFILE " + std::to_string(r.profile + 1);
    default:
        return "";
    }
}
// The trail box: each hand's path (front view), RED / BLUE / GREEN, the step and the reason, or
// the popup's CONFLICTS WITH line.
void remapTrail(const Painter& p, const Item& it, const View& view, Vec3 grey) {
    const Box& b = it.box;
    p.rect(b.u0, b.v0, b.u1, b.v1, p.c.backing, .6f);
    p.bar(b.u0, b.v0, b.u1, b.v0, .0006f, p.c.accent, .3f);
    p.bar(b.u0, b.v1, b.u1, b.v1, .0006f, p.c.accent, .3f);
    const gesture_session::View* r = view.remap;
    const float h = it.h;
    if (!r || (!r->recording && !r->popup && r->step == 0)) {
        const char* t = "PICK A MOVE, THEN RECORD";
        p.text(t, -textWidth(t, h) / 2, (b.v0 + b.v1) / 2 - h / 2, h, grey);
        return;
    }
    const Vec3 red{.95f, .18f, .16f}, blue{.25f, .55f, 1.f}, green{.25f, .92f, .35f};
    const Vec3 colour = r->trail == 3 ? green : r->trail == 2 ? blue : r->trail == 1 ? red : grey;
    // Left half: the path, the body's middle at the centre, an arm's length to the edge.
    const float cu = b.u0 + (b.v1 - b.v0) * .55f, cv = (b.v0 + b.v1) / 2, s = (b.v1 - b.v0) * .38f;
    p.bar(cu - s, cv, cu + s, cv, .0004f, grey, .35f);
    p.bar(cu, cv - s, cu, cv + s, .0004f, grey, .35f);
    for (int hand = 0; hand < 2 && r->trailPoints == gesture_session::points; ++hand)
        for (int i = 1; i < gesture_session::points; ++i)
            p.line(cu + r->u[hand][i - 1] * s, cv + (r->v[hand][i - 1] + .5f) * s, cu + r->u[hand][i] * s,
                   cv + (r->v[hand][i] + .5f) * s, .0011f, colour, .95f);
    // Right: what to do now.
    const float tu = cu + s + .006f, width = b.u1 - tu - .003f;
    std::string first = r->stepText, second, third = r->reasonText;
    Vec3 firstColour = r->trail ? colour : p.c.text;
    if (r->popup) {
        first = std::string(r->safety ? "BLOCKED: " : "CONFLICTS WITH: ") + r->conflictName;
        second = r->safety ? "TRY AGAIN OR CANCEL" : "KEEP ANYWAY, TRY AGAIN OR CANCEL";
        third.clear();
        firstColour = p.c.warn;
    } else if (!third.empty()) {
        second = "DIDN'T MATCH, TRY AGAIN:"; // and why, on its own line so it stays readable
    }
    const float h1 = fittedHeight(first, h, width);
    const float h2 = fittedHeight(second.empty() ? " " : second, h * .85f, width);
    const float h3 = fittedHeight(third.empty() ? " " : third, h * .8f, width);
    const float top = third.empty() ? cv + .002f : cv + .002f + h3 * .7f;
    p.text(first, tu, top, h1, firstColour);
    if (!second.empty())
        p.text(second, tu, top - .004f - h2, h2, r->popup ? p.c.warn : p.c.text);
    if (!third.empty())
        p.text(third, tu, top - .007f - h2 - h3, h3, p.c.label);
}

// A toggle's or cycle's button value (Update 39.0.2: one place, also a section header's summary).
std::string buttonValue(const View& view, Control c) {
    return c == Control::addressFinder ? addressFinderValue(view)
           : c == Control::eyeLod      ? eyeLodValue(view)
           : c == Control::eyeAa       ? eyeAaValue(view)
           : c == Control::engineTaa   ? engineTaaValue(view)
           : c == Control::taaJitter   ? std::string(engine_taa::jitterName(view.taaJitter))
           : c == Control::shadowCamera ? shadowCameraValue(view)
           : c == Control::farShadows   ? farShadowsValue(view)
           : c == Control::eyeBloom     ? eyeBloomValue(view)
           : c == Control::sharedLod    ? sharedLodValue(view)
           : c == Control::mirror       ? mirrorValue(view)
           : c == Control::mirrorRecentre ? std::string("PRESS")
           : c == Control::worldLog     ? worldLogValue(view)
           : c == Control::blood        ? bloodValue(view)
           : c == Control::bloodVisor   ? std::string(view.bloodVisor ? "ON" : "OFF")
           : c == Control::invincible   ? std::string(view.invincible ? "ON" : "OFF")
           : c == Control::flyMode      ? std::string(extras385::fly::speedName(view.flyMode))
           : c == Control::flyVignette  ? std::string(view.flyVignette ? "ON" : "OFF")
           : c == Control::flyRoll      ? std::string(extras385::fly::rollName(view.flyRoll))
           : c == Control::flyAutoLevel ? std::string(view.flyAutoLevel ? "ON" : "OFF")
           : c == Control::symbiote     ? std::string(view.symbiote ? "ON" : "OFF")
           : c == Control::padMode      ? std::string(!view.padMode ? "OFF" : view.padByPrompt ? "ON (PROMPT)" : "ON")
           : c == Control::promptHandling ? std::string(view.promptHandling ? "AUTO" : "MANUAL")
           : c == Control::playOnMonitor ? std::string(view.monitorArmed ? extras385::view::confirm : "PRESS")
           : c == Control::gestureMoves ? std::string(gestures385::modeName(view.gestureMoves))
           : c == Control::trainMove    ? std::string(view.trainMove) + (view.trainMine ? " (MINE)" : "")
           : c == Control::heroRoster   ? std::string("OPEN")
           : c == Control::heroPlaySpider ? std::string(spiderActive(view) ? "ACTIVE" : "PRESS")
           : remapRow(c)                ? remapValue(c, view)
           : c == Control::mirrorAnchor ? mirrorAnchorValue(view)
           : c == Control::npcFade      ? npcFadeValue(view)
           : c == Control::colorFx      ? colorValue(view)
           : c == Control::colorFilter  ? std::string(eye_color::filterName(view.color >> 8 & 0xff))
           : c == Control::colorOutlines ? std::string(view.color & 0x20000 ? "ON" : "OFF")
           : c == Control::colorBands   ? std::to_string(std::clamp<uint32_t>(view.color >> 20 & 0xf, 3, 5))
           : c == Control::foveated     ? foveaValue(view)
           : c == Control::foveaDebug   ? std::string(view.fovea & 0x10000 ? "ON" : "OFF")
           : c == Control::foveaStats   ? foveaStatsValue(view)
           : c == Control::slotTest     ? testValue(view.slotTest)
           : c == Control::webBallTest  ? webBallValue(view)
           : combatOption(c) >= 0
               ? std::string(combat38::valueName(static_cast<uint32_t>(combatOption(c)),
                                                 view.combat38[combatOption(c)]))
                                                : valueText(c, view.settings);
}
// A row's value as drawn: a slider's number, else its button's.
std::string rowValue(const View& view, Control c) {
    if (!isSliderRow(c))
        return off(view, c) ? std::string("N/A") : buttonValue(view, c);
    if (off(view, c))
        return "UNAVAILABLE";
    return c == Control::shadowDistance
               ? shadow_distance::valueText(view.shadowDistance, view.settings[c], view.shadowDistanceGame)
               : valueText(c, view.settings[c]);
}
// Update 39.0.2: a section's header: + or - at the left, its name, and on the right its summary (a
// hero's profile: a gold ACTIVE badge while that hero is played).
void sectionRow(const Painter& p, const Item& it, const View& view) {
    const Box& b = it.box;
    const bool hover = view.hover == it.control, held = view.held == it.control;
    p.rect(b.u0, b.v0, b.u1, b.v1, p.c.backing, .8f);
    p.rect(b.u0, b.v0, b.u1, b.v1, held ? p.c.line : p.c.accent, held ? .45f : hover ? .28f : it.open ? .16f : .08f);
    const Vec3 edge = hover || held ? p.c.text : p.c.accent;
    p.bar(b.u0, b.v1, b.u1, b.v1, .0007f, edge, it.open ? .9f : .55f);
    p.bar(b.u0, b.v0, b.u1, b.v0, .0007f, edge, it.open ? .9f : .55f);
    p.bar(b.u0, b.v0, b.u0, b.v1, .0013f, it.open ? p.c.line : edge, .9f);
    // The sign: - open, + closed.
    const float mid = (b.v0 + b.v1) / 2, s = std::min((b.v1 - b.v0) * .2f, .0026f), cx = b.u0 + .0055f;
    const Vec3 sign = hover || held || it.open ? p.c.text : p.c.line;
    p.bar(cx - s, mid, cx + s, mid, .0008f, sign, .95f);
    if (!it.open)
        p.bar(cx, mid - s, cx, mid + s, .0008f, sign, .95f);
    const float nameU = b.u0 + .011f;
    p.text(it.text, nameU, it.v, it.h, hover || held || it.open ? p.c.text : p.c.label);
    const float left = nameU + textWidth(it.text, it.h) + .006f, right = b.u1 - .004f;
    if (right <= left)
        return;
    if (it.hero >= 0) {
        if (heroOf(view) != it.hero)
            return;
        const float h = fittedHeight("ACTIVE", it.h2, right - left - .004f);
        const float w = textWidth("ACTIVE", h) + .004f, u0 = right - w;
        p.rect(u0, mid - h * .9f, right, mid + h * .9f, p.c.warn, .18f);
        p.bar(u0, mid - h * .9f, right, mid - h * .9f, .0006f, p.c.warn, .9f);
        p.bar(u0, mid + h * .9f, right, mid + h * .9f, .0006f, p.c.warn, .9f);
        p.text("ACTIVE", u0 + .002f, mid - h / 2, h, p.c.warn);
        return;
    }
    std::string summary = it.pre;
    if (it.sum0 != Control::none) {
        summary += rowValue(view, it.sum0);
        if (it.sum1 != Control::none)
            summary += " / " + rowValue(view, it.sum1);
        summary += it.post;
    } else if (it.count > 0) {
        const SectionDef& d = sectionsOf(view.page)[it.line];
        summary = std::to_string(it.count) + " " + (d.word ? d.word : "");
    }
    if (summary.empty())
        return;
    const float h = fittedHeight(summary, it.h2, right - left);
    p.text(summary, right - textWidth(summary, h), mid - h / 2, h, p.c.value);
}
// Update 39.0.2: POWERS' ACTIVE HERO line, the hero in gold.
void bannerRow(const Painter& p, const Item& it, const View& view) {
    // Version 0.0.72 build 6 THOR CRASH (crash-t876, 0x85e36ea0 / build 4's 0x8309fa70: both strlen's
    // byte loop): this was names[9], nine heroes, and heroOf is 9 for THOR and 10 for GREEN LANTERN, so
    // turning THOR on read past the table into the "ACTIVE HERO: " text and strlen'd it as a pointer.
    // The name now comes from hero_modes::heroNames (every hero) and any other number reads SPIDER-MAN.
    const char* lead = "ACTIVE HERO: ";
    const char* name = activeHeroName(heroOf(view));
    const float w = textWidth(lead, it.h) + textWidth(name, it.h);
    p.text(lead, -w / 2, it.v, it.h, p.c.label);
    p.text(name, -w / 2 + textWidth(lead, it.h), it.v, it.h, p.c.warn);
}
// Update 39.0.2: the gold RESTART tag after a row's name.
void restartTagOn(const Painter& p, const Item& it) {
    if (it.tagH <= 0)
        return;
    const float mid = it.v + it.h / 2, w = textWidth(restartTagText, it.tagH) + .003f;
    p.rect(it.tagU, mid - it.tagH * .85f, it.tagU + w, mid + it.tagH * .85f, p.c.warn, .16f);
    p.text(restartTagText, it.tagU + .0015f, mid - it.tagH / 2, it.tagH, p.c.warn);
}
void content(const Painter& p, const Layout& l, const View& view) {
    const float k = l.text;
    const Vec3 grey{.45f, .5f, .58f};
    for (const Item& it : l.items) {
        Vec3 help = p.c.help;
        switch (it.kind) {
        case Kind::title: {
            p.text(it.text, -textWidth(it.text, it.h) / 2, it.v, it.h, p.c.text);
            p.bar(-.05f, it.v - .004f, .05f, it.v - .004f, .0007f, p.c.frame, .8f);
            break;
        }
        case Kind::fps:
            fpsMonitor(p, it, view);
            break;
        case Kind::fpsStats:
            fpsStats(p, it, view, k);
            break;
        case Kind::wide:
            button(p, it.box, it.text, view, k);
            break;
        case Kind::slider:
            sliderRow(p, it, view);
            restartTagOn(p, it);
            break;
        case Kind::toggle:
        case Kind::cycle:
            restartTagOn(p, it);
            p.text(it.text, trackLeft, it.v, it.h,
                   view.hover == it.control || view.held == it.control ? p.c.text : p.c.label);
            button(p, it.box, buttonValue(view, it.control), view, k);
            if (arrows(it.control))
                arrowsOn(p, it.box, view.hover == it.control || view.held == it.control ? p.c.text : p.c.accent,
                         off(view, it.control) ? .25f : .9f);
            break;
        case Kind::mirror: {
            const Box& m = it.box;
            p.rect(m.u0, m.v0, m.u1, m.v1, p.c.backing, .6f);
            p.bar(m.u0, m.v0, m.u1, m.v0, .0006f, p.c.accent, .3f);
            p.bar(m.u0, m.v1, m.u1, m.v1, .0006f, p.c.accent, .3f);
            p.text(it.text, -textWidth(it.text, it.h) / 2, (m.v0 + m.v1) / 2 - it.h / 2, it.h, grey);
            help = grey;
            break;
        }
        case Kind::heading:
            p.bar(trackLeft, it.track, trackRight, it.track, .0006f, p.c.warn, .7f);
            p.text(it.text, -textWidth(it.text, it.h) / 2, it.v, it.h, p.c.warn);
            break;
        case Kind::notice:
            if (std::lround(view.settings.supersample) != std::lround(view.supersampleNow))
                p.text(it.text, -textWidth(it.text, it.h) / 2, it.v, it.h, p.c.warn);
            break;
        case Kind::footer:
            p.text(it.text, -textWidth(it.text, it.h) / 2, it.v, it.h, p.c.accent);
            break;
        case Kind::dropdown:
            dropdown(p, it, view, k);
            break;
        case Kind::visual: {
            const Box& b = it.box;
            const int kind = std::isfinite(view.settings.musicVisualizer)
                                 ? std::clamp(static_cast<int>(std::lround(view.settings.musicVisualizer)), 0,
                                              music::visualizers - 1)
                                 : 0;
            visualize(p, kind, b.u0, b.v0, b.u1, b.v1, view.music.spectrum, view.seconds, view.theme == 4);
            if (!view.music.available) {
                const float h = .0050f * k;
                p.text("MUSIC OFF", -textWidth("MUSIC OFF", h) / 2, (b.v0 + b.v1) / 2 - h / 2, h, grey);
            }
            break;
        }
        case Kind::progress:
            progress(p, it, view);
            break;
        case Kind::transport:
            transport(p, it, view, k, grey);
            break;
        case Kind::section:
            sectionRow(p, it, view);
            help = grey;
            break;
        case Kind::banner:
            bannerRow(p, it, view);
            break;
        case Kind::musicStatus:
            musicStatus(p, it, view);
            break;
        case Kind::trail:
            remapTrail(p, it, view, grey);
            break;
        case Kind::guide: {
            const auto state = controls_guide::stateOf(static_cast<controls_guide::Line>(it.line), view.guideOff,
                                                       view.guideNa);
            const Vec3 color = state == controls_guide::on ? p.c.text : grey;
            p.text(it.text, trackLeft, it.v, it.h, color);
            if (const char* m = controls_guide::mark(state); *m)
                p.text(m, trackRight - textWidth(m, it.h), it.v, it.h, state == controls_guide::na ? p.c.warn : grey);
            if (state != controls_guide::on)
                help = grey;
            break;
        }
        }
        helpLines(p, it.help, shownHelp(it, view), help,
                  it.control != Control::none && it.control == view.helpFor ? view.helpScroll : 0.f);
    }
}

// Update 19: the open song list, over what is under the dropdown (drawn
// after the page, inside its clip).
void songList(const Painter& p, const Layout& l, const View& view) {
    const MusicView& m = view.music;
    if (!m.listOpen || !m.available)
        return;
    const Item* d = itemOf(l, Control::musicTrack);
    if (!d)
        return;
    const float h = listRowHeight(l.text);
    const int shown = std::min(listRows, std::max(0, m.tracks - m.listFirst));
    if (shown <= 0)
        return;
    const float top = d->box.v0, bottom = top - h * static_cast<float>(shown);
    p.rect(trackLeft, bottom, trackRight, top, p.c.backing, 1); // opaque: nothing under it shows through
    p.bar(trackLeft, bottom, trackRight, bottom, .0007f, p.c.accent, .8f);
    p.bar(trackLeft, bottom, trackLeft, top, .0007f, p.c.accent, .8f);
    p.bar(trackRight, bottom, trackRight, top, .0007f, p.c.accent, .8f);
    for (int r = 0; r < shown; ++r) {
        const int index = m.listFirst + r;
        const float y1 = top - h * static_cast<float>(r), y0 = y1 - h;
        const bool hover = index == m.listHover;
        if (hover)
            p.rect(trackLeft + .0008f, y0 + .0004f, trackRight - .0008f, y1 - .0004f, p.c.accent, .45f);
        if (r > 0)
            p.bar(trackLeft + .002f, y1, trackRight - .002f, y1, .0003f, p.c.accent, .3f);
        const std::string name =
            std::to_string(index + 1) + ". " + panelText(static_cast<size_t>(r) < m.rows.size() ? m.rows[r] : "");
        const float th = std::min(fittedHeight(name, .0044f * l.text, trackRight - trackLeft - .010f), h * .6f);
        p.text(name, trackLeft + .003f, (y0 + y1) / 2 - th / 2, th,
               hover ? p.c.text : index == m.track ? p.c.value : p.c.label);
    }
    if (m.tracks > listRows) { // where in the list these rows are
        const float u = trackRight - .0025f, room = top - bottom;
        const float len = std::max(.004f, room * static_cast<float>(listRows) / static_cast<float>(m.tracks));
        const float most = static_cast<float>(m.tracks - listRows);
        const float y = top - (room - len) * std::clamp(static_cast<float>(m.listFirst) / most, 0.f, 1.f);
        p.bar(u, bottom + .001f, u, top - .001f, .0006f, p.c.accent, .35f);
        p.bar(u, y - len, u, y, .0016f, p.c.line, .9f);
    }
}

// Update 17: the slim scrollbar on the right, while the page scrolls.
void scrollbar(const Painter& p, int page, float scroll, float k, bool dragging) {
    const Thumb t = scrollThumb(page, scroll, k);
    if (!t.shown)
        return;
    p.bar(scrollbarU, scrollTrackBottom, scrollbarU, scrollTrackTop, .0010f, p.c.accent, .35f);
    p.bar(scrollbarU, t.v0, scrollbarU, t.v1, dragging ? .0034f : .0026f, dragging ? p.c.text : p.c.line, .9f);
}

// The arrows and the page between them.
void strip(const Painter& p, const View& view, int page) {
    p.rect(-stripHalf, stripBottom, stripHalf, stripTop, p.c.backing, p.c.backingAlpha);
    p.bar(-stripHalf, stripBottom, stripHalf, stripBottom, .0008f, p.c.frame, .8f);
    p.bar(-stripHalf, stripTop, stripHalf, stripTop, .0006f, p.c.accent, .5f);
    button(p, previousBox, "", view);
    button(p, nextBox, "", view);
    const float mid = (stripTop + stripBottom) / 2;
    for (const Box* b : {&previousBox, &nextBox}) {
        const float c = (b->u0 + b->u1) / 2, d = b == &previousBox ? -1.f : 1.f;
        const Vec3 color = view.hover == b->control ? p.c.text : p.c.line;
        p.triangle(c + d * .0060f, mid, c - d * .0040f, mid + .0060f, c - d * .0040f, mid - .0060f, color, .95f);
    }
    char label[48];
    std::snprintf(label, sizeof label, "%d/%d %s", page + 1, pages, view.heroTiles && page == powersPage ? "HEROES" : pageName(page)); // 0.0.73 test 4: the tiles
    // A refused setting's note takes the page's place for a while, in red.
    const std::string t = view.note.empty() ? std::string(label) : view.note;
    const float h = fittedHeight(t, .0046f, .084f);
    p.text(t, -textWidth(t, h) / 2, mid - h / 2, h, view.note.empty() ? p.c.text : p.c.bad);
}

// Where the other controller points: a thin beam from its tip, a dot there.
void pointerMark(const Painter& p, const View& view) {
    const Vec3 at = p.at(view.pointU, view.pointV);
    if (!finite(view.tip) || !finite(at))
        return;
    soft::softBar(p.out, view.tip, at, p.normal, .0007f, p.c.line, .35f, p.soft);
    p.ring(view.pointU, view.pointV, 1, 1, 0, .0030f, p.c.text, .95f, 16);
    p.ring(view.pointU, view.pointV, 1, 1, .0030f, .0055f, p.c.line, .35f, 24);
}

// Update 17: the themes. Each is checked (tests) for its text standing out
// on its glass and for bands that differ in more than red against green.
constexpr std::array<Palette, themes> palettes{{
    // Spider-Man: red frame, blue lines (Update 14's colours).
    {{1, .06f, .05f}, {.1f, .5f, 1}, {.5f, .85f, 1}, {.85f, .95f, 1}, {.5f, .85f, 1}, {1, .72f, .66f},
     {.62f, .74f, .88f}, {.004f, .016f, .05f}, {.35f, .95f, .45f}, {1, .55f, .08f}, {1, .32f, .28f}, .58f},
    // High contrast: white and yellow on near-black glass.
    {{1, 1, 1}, {1, .8f, 0}, {1, .9f, .25f}, {1, 1, 1}, {1, 1, 1}, {1, .85f, 0}, {.9f, .9f, .85f},
     {0, 0, 0}, {1, 1, 1}, {1, .8f, 0}, {1, .3f, 0}, .85f},
    // Deuteranopia/protanopia safe: blue and orange, never red against green.
    {{1, .35f, 0}, {.05f, .3f, 1}, {.45f, .7f, 1}, {.95f, .95f, 1}, {.55f, .78f, 1}, {1, .6f, .12f},
     {.7f, .76f, .88f}, {.004f, .01f, .04f}, {.25f, .55f, 1}, {1, .85f, .2f}, {1, .35f, 0}, .62f},
    // Tritanopia safe: red and teal, never blue against yellow.
    {{.9f, .04f, .06f}, {0, .55f, .5f}, {.3f, .9f, .82f}, {.95f, .98f, .98f}, {.4f, .92f, .85f}, {1, .55f, .6f},
     {.72f, .82f, .8f}, {.01f, .02f, .02f}, {.15f, .9f, .8f}, {1, .6f, .75f}, {.95f, .08f, .08f}, .62f},
    // Monochrome: greys only; the bands differ in brightness and by word.
    {{.85f, .85f, .85f}, {.5f, .5f, .5f}, {.75f, .75f, .75f}, {1, 1, 1}, {.85f, .85f, .85f}, {1, 1, 1},
     {.7f, .7f, .7f}, {.01f, .01f, .01f}, {1, 1, 1}, {.6f, .6f, .6f}, {.35f, .35f, .35f}, .65f},
    // Update 36: 2000s Raimi, after the 2004 Spider-Man 2 handheld pause
    // menu: near-black navy glass, blue lines (its glowing spider), titles
    // and the pointed-at item bright yellow, items orange, headings gold,
    // values and help pale; drawn in the pixel font. The bands differ by
    // blue (good) against red (bad).
    {{.021f, .155f, 1}, {.045f, .223f, 1}, {.262f, .515f, 1}, {1, .80f, .06f}, {1, .25f, .01f}, {.85f, .88f, 1},
     {.62f, .68f, .85f}, {.0012f, .0018f, .006f}, {.10f, .58f, 1}, {1, .52f, .02f}, {1, .06f, .03f}, .93f},
}};
constexpr std::array<Vec3, textColors> textColorValues{{{0, 0, 0}, {1, 1, 1}, {1, .85f, .1f}, {.2f, .95f, 1}}};
} // namespace
bool greyed(const View& view, Control c) {
    return off(view, c);
}

float& Settings::operator[](Control c) {
    switch (c) {
    case Control::thickness:
        return widthCm;
    case Control::speed:
        return speed;
    case Control::aa:
        return aa;
    case Control::history:
        return history;
    case Control::reach:
        return reach;
    case Control::monitor:
        return monitor;
    case Control::supersample:
        return supersample;
    case Control::sharpen:
        return sharpen;
    case Control::reticles:
        return reticles;
    case Control::eyeTest:
        return eyeTest;
    case Control::shadowDistance:
        return shadowDistance;
    case Control::eyeTrail:
        return eyeTrail;
    case Control::colorStrength:
        return colorStrength;
    case Control::exposure:
        return exposure;
    case Control::contrast:
        return contrast;
    case Control::saturation:
        return saturation;
    case Control::vibrance:
        return vibrance;
    case Control::warmth:
        return warmth;
    case Control::tint:
        return tint;
    case Control::vignette:
        return vignette;
    case Control::colorCartoon:
        return colorCartoon;
    case Control::flyRollSpeed:
        return flyRollSpeed;
    case Control::foveaInner:
        return foveaInner;
    case Control::hudHeight:
        return hudHeight;
    case Control::hudScale:
        return hudScale;
    case Control::hudText:
        return hudText;
    case Control::hudTheme:
        return hudTheme;
    case Control::hudTextColor:
        return hudTextColor;
    case Control::webStyle:
        return webStyle;
    case Control::musicVolume:
        return musicVolume;
    case Control::musicVisualizer:
        return musicVisualizer;
    case Control::webColor:
        return webColors[std::isfinite(webStyle) ? std::clamp(static_cast<int>(std::lround(webStyle)), 0, webStyles - 1)
                                                 : 0];
    default: {
        static thread_local float none;
        none = 0;
        return none;
    }
    }
}
Range range(Control c) {
    switch (c) {
    case Control::thickness:
        return {minWidthCm, maxWidthCm, widthStep};
    case Control::speed:
        return {minSpeed, maxSpeed, speedStep};
    case Control::history:
        return {0, 95, 5}; // Update 21: was 0-80
    case Control::reach:
        return {50, 400, 10};
    case Control::monitor:
        return {0, 240, 5};
    case Control::supersample:
        return {100, 200, 10};
    case Control::sharpen:
        return {0, 200, 10}; // Update 21: was 0-100 in 5s; 101-200 the strong mode
    case Control::shadowDistance:
        return {0, static_cast<float>(shadow_distance::maxCars), 1}; // Update 33: car lengths, 0 the game's
    case Control::eyeTrail:
        return {0, 100, 5}; // Update 34.2: percent of the history kept at swing speed
    case Control::colorStrength: // Update 37.2: the COLOR page (eye_color::Config's ranges)
        return {0, 100, 5};
    case Control::exposure:
        return {-2, 2, .1f};
    case Control::contrast:
        return {50, 150, 5};
    case Control::saturation:
        return {0, 200, 5};
    case Control::vibrance:
    case Control::vignette:
        return {0, 100, 5};
    case Control::flyRollSpeed:
        return {extras385::fly::rollSpeedLow, extras385::fly::rollSpeedHigh, extras385::fly::rollSpeedStep};
    case Control::colorCartoon:
        return {static_cast<float>(eye_color::cartoonLimits.lo), static_cast<float>(eye_color::cartoonLimits.hi),
                static_cast<float>(eye_color::cartoonLimits.step)};
    case Control::warmth:
    case Control::tint:
        return {-100, 100, 5};
    case Control::foveaInner:
        return {static_cast<float>(foveation::innerMin), static_cast<float>(foveation::innerMax),
                static_cast<float>(foveation::innerStep)};
    case Control::hudHeight:
        return {0, 25, 1};
    case Control::hudScale:
        return {.5f, 2, .05f};
    case Control::hudText:
        return {75, 175, 5};
    case Control::hudTheme:
        return {0, themes - 1, 1};
    case Control::hudTextColor:
        return {0, textColors - 1, 1};
    case Control::webStyle:
        return {0, web_style::count - 1, 1};
    case Control::webColor: // the longest list; each style's own: shadeCount
        return {0, web_style::maxShades - 1, 1};
    case Control::musicVolume:
        return {0, 100, 5};
    case Control::musicVisualizer:
        return {0, music::visualizers - 1, 1};
    default:
        return {0, 1, 1};
    }
}
const char* pageName(int page) {
    switch (page) {
    case powersPage:
        return "POWERS";
    case vrPage:
        return "VR SETTINGS";
    case combatPage:
        return "COMBAT";
    case graphicsPage:
        return "GRAPHICS";
    case colorPage:
        return "COLOR";
    case controlsPage:
        return "CONTROLS";
    case musicPage:
        return "MUSIC";
    default:
        return "STATUS";
    }
}
float textFactor(float textScale) {
    return std::isfinite(textScale) ? std::clamp(textScale, .75f, 1.75f) : 1.f;
}
bool onPanel(int page, float u, float v) {
    if (!std::isfinite(u) || !std::isfinite(v))
        return false;
    return (std::abs(u) <= halfW && v >= -halfH && v <= panelTop(page)) ||
           (std::abs(u) <= stripHalf && v >= stripBottom && v <= stripTop + .004f);
}
float panelTop(int) {
    return halfH;
}
float contentHeight(int page, float text) {
    return viewTop - layoutOf(page, text).bottom;
}
float maxScroll(int page, float text) {
    return std::max(0.f, contentHeight(page, text) - (viewTop - viewBottom));
}
float clampScroll(int page, float scroll, float text) {
    return std::isfinite(scroll) ? std::clamp(scroll, 0.f, maxScroll(page, text)) : 0.f;
}
Thumb scrollThumb(int page, float scroll, float text) {
    const float most = maxScroll(page, text);
    if (!(most > 0))
        return {false, 0, 0};
    const float track = scrollTrackTop - scrollTrackBottom;
    const float length = std::max(thumbLeast, track * (viewTop - viewBottom) / contentHeight(page, text));
    const float top = scrollTrackTop - (track - length) * clampScroll(page, scroll, text) / most;
    return {true, top - length, top};
}
bool onScrollbar(int page, float u, float v, float text) {
    return std::isfinite(u) && std::isfinite(v) && u >= scrollbarU0 && u <= scrollbarU1 && v >= viewBottom &&
           v <= viewTop && maxScroll(page, text) > 0;
}
Control controlAt(int page, float u, float v, float scroll, float text) {
    if (!onPanel(page, u, v))
        return Control::none;
    for (const Box& b : {previousBox, nextBox})
        if (b.in(u, v))
            return b.control;
    // Only what shows in the inner area can be pressed, where it shows.
    if (v < viewBottom || v > viewTop || onScrollbar(page, u, v, text))
        return Control::none;
    const float cv = v - clampScroll(page, scroll, text);
    for (const Item& it : layoutOf(page, text).items) {
        if (it.control == Control::none)
            continue;
        if (it.kind == Kind::slider) {
            if (std::abs(cv - it.track) <= sliderReach && u >= trackLeft - .008f && u <= trackRight + .008f)
                return it.control;
        } else if (it.box.in(u, cv)) {
            return it.control;
        }
    }
    return Control::none;
}
// Update 38.5: a control's subtext as drawn now, and where its rows are.
HelpInfo helpInfo(const View& view, int page, Control c) {
    HelpInfo info;
    if (c == Control::none)
        return info;
    for (const Item& it : layoutOf(page, view.textScale).items) {
        if (it.control != c || it.help.lines.empty())
            continue;
        const Help& shown = shownHelp(it, view);
        info.found = true;
        info.lines = static_cast<int>(shown.lines.size());
        info.rows = std::max(1, it.help.rows);
        info.height = shown.height;
        info.pitch = it.help.pitch;
        for (const std::string& line : shown.lines)
            info.widest = std::max(info.widest, helpLineWidth(line, shown.height));
        return info;
    }
    return info;
}
int helpOverflow(const View& view, int page, Control c) {
    const HelpInfo info = helpInfo(view, page, c);
    return info.found ? std::max(0, info.lines - info.rows) : 0;
}
Control helpAt(int page, float u, float v, float scroll, float text) {
    if (!std::isfinite(u) || !std::isfinite(v) || v < viewBottom || v > viewTop || u < trackLeft || u > trackRight)
        return Control::none;
    const float cv = v - clampScroll(page, scroll, text);
    for (const Item& it : layoutOf(page, text).items) {
        if (it.control == Control::none || it.help.lines.empty())
            continue;
        const Help& h = it.help;
        const float top = h.v + h.height + h.pitch * .25f;
        const float bottom = h.v - h.pitch * (static_cast<float>(h.rows - 1) + .25f);
        if (cv <= top && cv >= bottom)
            return it.control;
    }
    return Control::none;
}
uint32_t heroSettingsPages(uint32_t hero) { return static_cast<uint32_t>(std::max<size_t>(1, heroPagesOf(hero).size())); }
std::vector<Control> heroSettingsRows(uint32_t hero, uint32_t page) {
    const auto& all = heroPagesOf(hero);
    return page < all.size() ? all[page].rows : std::vector<Control>{};
}
bool heroSettingsEveryHero(uint32_t hero, uint32_t page) {
    const auto& all = heroPagesOf(hero);
    return page < all.size() && all[page].everyHero;
}
Control heroSettingAt(uint32_t hero, uint32_t page, float u, float v) { return heroSettingUnder(hero, page, u, v); }
Spot heroSettingSpot(uint32_t hero, uint32_t page, Control c) {
    for (const Item& it : heroSettingsLayout(hero, page).items) {
        if (it.control != c || c == Control::none)
            continue;
        if (it.kind == Kind::toggle)
            return {true, (it.box.u0 + it.box.u1) / 2, (it.box.v0 + it.box.v1) / 2};
        if (it.kind == Kind::slider)
            return {true, (trackLeft + trackRight) / 2, it.track};
    }
    return {false, 0, 0, false};
}
Spot controlSpot(int page, Control c, float scroll, float text) {
    const int p = page >= 0 && page < pages ? page : 0;
    if (c == Control::previous || c == Control::next) {
        const Box& b = c == Control::previous ? previousBox : nextBox;
        return {true, (b.u0 + b.u1) / 2, (b.v0 + b.v1) / 2, true};
    }
    const Item* it = itemOf(layoutOf(p, text), c);
    if (!it)
        return {false, 0, 0, false};
    const float dv = clampScroll(p, scroll, text);
    Spot s = it->kind == Kind::slider ? Spot{true, 0, it->track + dv}
                                      : Spot{true, (it->box.u0 + it->box.u1) / 2, (it->box.v0 + it->box.v1) / 2 + dv};
    s.visible = s.v >= viewBottom && s.v <= viewTop;
    return s;
}
std::vector<Area> pageAreas(int page, float text) {
    return layoutOf(page, text).areas;
}

// Sutherland-Hodgman against v >= low and v <= high, in the panel's plane:
// a clipped triangle becomes a fan (up to 5 points), its colour and alpha,
// shape coordinates interpolated along.
void clipBand(std::vector<Vertex>& out, size_t from, const Frame& f, float scale, float low, float high) {
    if (from >= out.size() || !(scale > 0))
        return;
    const size_t end = from + (out.size() - from) / 3 * 3;
    const auto vOf = [&](const Vertex& x) {
        return dot(x.position - f.centre, f.up) / scale;
    };
    const auto mix = [](const Vertex& a, const Vertex& b, float t) {
        Vertex r = a;
        r.position = a.position + (b.position - a.position) * t;
        r.color = a.color + (b.color - a.color) * t;
        r.alpha = a.alpha + (b.alpha - a.alpha) * t;
        r.shape = {a.shape.x + (b.shape.x - a.shape.x) * t, a.shape.y + (b.shape.y - a.shape.y) * t,
                   a.shape.z + (b.shape.z - a.shape.z) * t, a.shape.w + (b.shape.w - a.shape.w) * t};
        return r;
    };
    thread_local std::vector<Vertex> kept;
    kept.clear();
    for (size_t i = from; i < end; i += 3) {
        const float v0 = vOf(out[i]), v1 = vOf(out[i + 1]), v2 = vOf(out[i + 2]);
        if (!std::isfinite(v0) || !std::isfinite(v1) || !std::isfinite(v2))
            continue;
        if (std::min({v0, v1, v2}) >= low && std::max({v0, v1, v2}) <= high) {
            kept.insert(kept.end(), {out[i], out[i + 1], out[i + 2]});
            continue;
        }
        if (std::max({v0, v1, v2}) < low || std::min({v0, v1, v2}) > high)
            continue;
        std::array<Vertex, 8> a{out[i], out[i + 1], out[i + 2]}, b{};
        size_t n = 3;
        for (int side = 0; side < 2; ++side) {
            const auto inside = [&](const Vertex& x) {
                return side == 0 ? vOf(x) >= low : vOf(x) <= high;
            };
            const float edge = side == 0 ? low : high;
            size_t m = 0;
            for (size_t k = 0; k < n; ++k) {
                const Vertex& p = a[k];
                const Vertex& q = a[(k + 1) % n];
                const bool pin = inside(p), qin = inside(q);
                if (pin)
                    b[m++] = p;
                if (pin != qin) {
                    const float vp = vOf(p), vq = vOf(q);
                    b[m++] = mix(p, q, (edge - vp) / (vq - vp));
                }
            }
            a = b;
            n = m;
            if (n < 3)
                break;
        }
        for (size_t k = 1; k + 1 < n; ++k)
            kept.insert(kept.end(), {a[0], a[k], a[k + 1]});
    }
    out.resize(from);
    out.insert(out.end(), kept.begin(), kept.end());
}

const char* fpsWord(int band) {
    switch (band) {
    case 2:
        return "OK";
    case 1:
        return "LOW";
    case 0:
        return "POOR";
    default:
        return "";
    }
}
const char* themeName(int theme) {
    switch (theme) {
    case 1:
        return "HIGH CONTRAST";
    case 2:
        return "DEUTAN/PROTAN SAFE";
    case 3:
        return "TRITAN SAFE";
    case 4:
        return "MONOCHROME";
    case raimiTheme:
        return "2000s Raimi";
    default:
        return "SPIDER-MAN RED/BLUE";
    }
}
const char* themeKey(int theme) {
    switch (theme) {
    case 1:
        return "contrast";
    case 2:
        return "deutan";
    case 3:
        return "tritan";
    case 4:
        return "mono";
    case raimiTheme:
        return "raimi";
    default:
        return "spiderman";
    }
}
const char* textColorName(int color) {
    switch (color) {
    case 1:
        return "WHITE";
    case 2:
        return "YELLOW";
    case 3:
        return "CYAN";
    default:
        return "THEME";
    }
}
const char* textColorKey(int color) {
    switch (color) {
    case 1:
        return "white";
    case 2:
        return "yellow";
    case 3:
        return "cyan";
    default:
        return "theme";
    }
}
Palette palette(int theme, int textColor) {
    Palette p = palettes[static_cast<size_t>(theme >= 0 && theme < themes ? theme : 0)];
    if (textColor > 0 && textColor < textColors) {
        const Vec3 t = textColorValues[static_cast<size_t>(textColor)];
        p.text = p.label = p.value = t;
        p.help = t * .8f;
    }
    return p;
}
const char* helpText(Control c) {
    if (const int o = combatOption(c); o >= 0)
        return combat38::optionInfo[o].help;
    switch (c) {
    case Control::fpsReset:
        return fpsStatsHelp;
    case Control::recalibrate:
        return "Runs the T-pose again to re-measure your height and arm length.";
    case Control::thickness:
        return "How thick your webs look. Only the look: swinging is the same.";
    case Control::speed:
        return "How fast a shot web flies out to where it sticks; it still grabs at once.";
    case Control::aa:
        return "Smooths jagged edges; FXAA is cheap but softens fine detail a little.";
    case Control::history:
        return "Mixes in the last frame: 20-40 calms shimmer, 80-95 is heavy (trails behind motion).";
    case Control::reach:
        return "How far buildings keep full detail; more costs some frame rate and video memory.";
    case Control::monitor:
        return "Limits desktop window updates; lower leaves more for the headset. 0: no cap.";
    case Control::supersample:
        // Update 23: OFF (100%) is the default; the cost said plainly (the October 23 player).
        return "OFF is fastest; higher calms shimmer but costs much frame rate (needs a restart).";
    case Control::sharpen:
        return "Up to 100% is normal CAS; 110-200% is extra strong, with halos and more shimmer.";
    case Control::reticles:
        return "Shows where each hand's web will stick.";
    case Control::eyeTest:
        return "ON shows both eyes one picture (flat): if far flicker stops, the eyes disagree.";
    case Control::addressFinder:
        return addressFinderHelp;
    case Control::eyeLod:
        return eyeLodHelp;
    case Control::shadowCamera:
        return shadow_camera::help;
    case Control::shadowDistance:
        return shadow_distance::help;
    case Control::eyeAa:
        return eye_taa::help;
    case Control::engineTaa:
        return engine_taa::help;
    case Control::eyeTrail:
        return "EYE AA's smear while swinging: 0 none (sharpest motion), 100 the full blend.";
    case Control::colorFx:
        return eye_color::help;
    case Control::colorFilter:
        return eye_color::filterHelp;
    case Control::colorStrength:
        return eye_color::strengthHelp;
    case Control::exposure:
        return eye_color::exposureHelp;
    case Control::contrast:
        return eye_color::contrastHelp;
    case Control::saturation:
        return eye_color::saturationHelp;
    case Control::vibrance:
        return eye_color::vibranceHelp;
    case Control::warmth:
        return eye_color::warmthHelp;
    case Control::tint:
        return eye_color::tintHelp;
    case Control::vignette:
        return eye_color::vignetteHelp;
    case Control::colorCartoon:
        return eye_color::cartoonHelp;
    case Control::colorOutlines:
        return eye_color::outlinesHelp;
    case Control::colorBands:
        return eye_color::bandsHelp;
    case Control::foveated:
        return foveation::help;
    case Control::foveaInner:
        return foveation::innerHelp;
    case Control::foveaDebug:
        return foveation::debugHelp;
    case Control::foveaStats:
        return foveation::statsHelp;
    case Control::taaJitter:
        return engine_taa::jitterHelp;
    case Control::farShadows:
        return far_shadows::help;
    case Control::eyeBloom:
        return eye_bloom::help;
    case Control::sharedLod:
        return shared_lod::help;
    case Control::mirror:
        return "Shows your body in the hero suit you wear; press again to close, no restart.";
    case Control::mirrorRecentre:
        return "Puts the mirror in front of where you face now; it then stays put when you look away.";
    case Control::worldLog:
        return "Quietly notes NPC talk, spawns and missions for future mods; changes nothing in game.";
    case Control::blood:
        return blood_splatter::help;
    case Control::bloodVisor:
        return blood_splatter::visorHelp;
    case Control::invincible:
        return extras385::invincible::help;
    case Control::flyMode:
        return extras385::fly::help;
    case Control::flyVignette:
        return extras385::fly::vignetteHelp;
    case Control::flyRoll:
        return extras385::fly::rollHelp;
    case Control::flyRollSpeed:
        return extras385::fly::rollSpeedHelp;
    case Control::flyAutoLevel:
        return extras385::fly::autoLevelHelp;
    case Control::symbiote:
        return extras385::symbiote::help;
    case Control::padMode:
        return extras385::prompts::padHelp;
    case Control::promptHandling:
        return extras385::prompts::help;
    case Control::playOnMonitor:
        return extras385::view::help;
    case Control::gestureMoves:
        return gestures385::help;
    case Control::trainMove:
        return "Picks the move TRAINING draws as a glowing path in the air to follow.";
    case Control::heroTabSpider:
    case Control::heroTabIron:
    case Control::heroTabDaredevil:
    case Control::heroTabHulk:
        return heroTabsHelp;
    case Control::section0:
    case Control::section1:
    case Control::section2:
    case Control::section3:
    case Control::section4:
    case Control::section5:
    case Control::section6:
    case Control::section7:
    case Control::section8:
    case Control::section9:
    case Control::section10:
    case Control::section11:
        return sectionHelp;
    case Control::heroPlaySpider:
        return "Turns every other hero off so you swing as Spider-Man again, live.";
    case Control::heroRoster:
        return "Opens the roster: each hero's picture, story and suits. No restart needed.";
    case Control::gestureRemapMove:
        return gesture_session::helpMove;
    case Control::gestureRecord:
        return gesture_session::helpRecord;
    case Control::gestureKeep:
        return gesture_session::helpKeep;
    case Control::gestureAgain:
        return gesture_session::helpAgain;
    case Control::gestureCancel:
        return gesture_session::helpCancel;
    case Control::gestureResetMove:
        return gesture_session::helpResetMove;
    case Control::gestureResetAll:
        return gesture_session::helpResetAll;
    case Control::gestureProfile:
        return gesture_session::helpProfile;
    case Control::mirrorAnchor:
        return "PLAYSPACE: the mirror travels with you; WORLD: it stays pinned in the city.";
    case Control::npcFade:
        return npc_fade::help;
    case Control::slotTest:
        return "Each press tries the next gadget input 1.5 s later and logs it.";
    case Control::webBallTest:
        return "Swaps EYE BLOOM OFF and COPY, then shoots a web ball: watch for gold squares.";
    case Control::hudHeight:
        return "How far above your wrist the hologram floats.";
    case Control::hudScale:
        return "How big the whole hologram is.";
    case Control::hudText:
        return "Bigger text is easier to read; the page scrolls to fit.";
    case Control::hudTheme:
        return "Colour sets for colour-blind players; FPS also says OK/LOW.";
    case Control::hudTextColor:
        return "The colour of names and values; THEME uses the set's own.";
    case Control::webStyle:
        return "Twisted fibre strands: MESSY (default) frays most; 90s keeps its coils.";
    case Control::webColor:
        return "Shade of the selected web style.";
    case Control::musicTrack:
        return "Pick a song from your Music folder; drop MP3, WAV, FLAC or M4A files there.";
    case Control::musicPick:
        return "A song in your Music folder; pull the trigger to play it.";
    case Control::musicSeek:
        return "How far into the song you are; point and pull the trigger to jump there.";
    case Control::musicPrevious:
    case Control::musicRewind:
    case Control::musicPlay:
    case Control::musicStop:
    case Control::musicForward:
    case Control::musicNext:
        return "Previous song, back 10 s, play or pause, stop, ahead 10 s, next song.";
    case Control::musicVolume:
        return "Music loudness only; the game's own sound is not changed.";
    case Control::musicVisualizer:
        return "The picture that moves with your music; all are light on frame rate.";
    default:
        return "";
    }
}
const char* shadowCameraRevertedHelp(uint32_t reason) {
    static const std::array<std::string, 3> texts = {
        std::string("Back to OFF; the launcher's console and report say why."),
        std::string("Back to OFF: ") + shadow_camera::reasonText(1) + ".",
        std::string("Back to OFF: ") + shadow_camera::reasonText(2) + ".",
    };
    return texts[reason < texts.size() ? reason : 0].c_str();
}
// Update 35: FAR SHADOWS' subtext: its help, or why it changed on its own (one
// sentence, <= 80).
// Update 36: EYE BLOOM's and SHARED LOD's subtexts (one sentence, <= 80).
const char* eyeBloomHelp(uint32_t packed) {
    return eye_bloom::helpText(packed);
}
const char* sharedLodHelp(uint32_t packed) {
    return shared_lod::helpText(packed);
}
const char* farShadowsHelp(uint32_t packed) {
    namespace fs = far_shadows;
    static const auto texts = [] {
        std::array<std::string, fs::reasons * 2> t; // [reason * 2 + (mode != off)]
        for (uint32_t r = 0; r < fs::reasons; ++r) {
            t[r * 2] = r ? std::string("Back to OFF: ") + fs::reasonText(r) + "." : std::string(fs::help);
            t[r * 2 + 1] = r ? std::string("Plan B on: ") + fs::reasonText(r) + " with EYES." : std::string(fs::help);
        }
        t[fs::noHook * 2] = "N/A: this game build differs, so shadows are left as they are.";
        return t;
    }();
    const uint32_t r = fs::reasonOf(packed);
    return texts[(r < fs::reasons ? r : 0) * 2 + (fs::modeOf(packed) != fs::off ? 1 : 0)].c_str();
}
// Update 34: ENGINE TAA's subtext when off with a reason (one sentence, <= 80).
const char* engineTaaOffHelp(uint32_t reason) {
    static const auto texts = [] {
        std::array<std::string, engine_taa::reasons> t;
        t[0] = engine_taa::help;
        for (uint32_t i = 1; i < engine_taa::reasons; ++i)
            t[i] = std::string("Off: ") + engine_taa::reasonText(i) + ".";
        return t;
    }();
    return texts[reason < engine_taa::reasons ? reason : 0].c_str();
}
// Update 33: a failed EYE AA's subtext (one sentence, <= 80).
const char* eyeAaFailedHelp(uint32_t reason) {
    static const auto texts = [] {
        std::array<std::string, eye_taa::reasons> t;
        for (uint32_t i = 0; i < eye_taa::reasons; ++i)
            t[i] = std::string("Failed: ") + eye_taa::reasonText(i) + ".";
        return t;
    }();
    return texts[reason < eye_taa::reasons ? reason : 0].c_str();
}
const char* eyeLodFailedHelp(uint32_t reason) {
    static const auto texts = [] {
        std::array<std::string, native_view::eyeLodReasons> t;
        for (uint32_t i = 0; i < t.size(); ++i)
            t[i] = i ? std::string("Failed: ") + native_view::eyeLodReasonText(i) + "."
                     : std::string("Failed; the launcher's console and report say why.");
        return t;
    }();
    return texts[reason < texts.size() ? reason : 0].c_str();
}
float helpLineWidth(const std::string& line, float height) {
    if (line.empty() || !(height > 0))
        return 0;
    // pixelText: its natural width (columns * height / 7) squeezed to no less than 0.7.
    const float pixel = static_cast<float>(pixelTextColumns(line)) * height / 7 * .7f;
    return std::max(textWidth(line, height), pixel);
}
std::vector<std::string> wrapText(const std::string& text, float height, float width) {
    std::vector<std::string> lines;
    std::string line;
    size_t at = 0;
    while (at < text.size()) {
        size_t end = text.find(' ', at);
        if (end == std::string::npos)
            end = text.size();
        std::string word = text.substr(at, end - at);
        at = end + 1;
        if (word.empty())
            continue;
        // Update 38.5: a word wider than a whole line is split where it fills one.
        while (helpLineWidth(word, height) > width && word.size() > 1) {
            if (!line.empty()) {
                lines.push_back(line);
                line.clear();
            }
            size_t fits = 1;
            while (fits < word.size() && helpLineWidth(word.substr(0, fits + 1), height) <= width)
                ++fits;
            lines.push_back(word.substr(0, fits));
            word.erase(0, fits);
        }
        const std::string longer = line.empty() ? word : line + " " + word;
        if (!line.empty() && helpLineWidth(longer, height) > width) {
            lines.push_back(line);
            line = word;
        } else {
            line = longer;
        }
    }
    if (!line.empty() || lines.empty())
        lines.push_back(line);
    return lines;
}
float sliderValue(Control c, float u) {
    const Range r = range(c);
    const float f = std::isfinite(u) ? std::clamp((u - trackLeft) / (trackRight - trackLeft), 0.f, 1.f) : 0.f;
    const float steps = std::round(f * (r.high - r.low) / r.step);
    // Whole steps: 1.3, not 1.2999 (hundredths for the hologram's 1.05X).
    const float value = std::clamp(r.low + steps * r.step, r.low, r.high);
    return r.step < 1 ? std::round(value * 100) / 100 : value;
}
std::string valueText(Control c, float value) {
    char n[32];
    const float v = std::isfinite(value) ? value : 0.f;
    switch (c) {
    case Control::thickness:
        std::snprintf(n, sizeof n, "%.1f CM", v);
        break;
    case Control::speed:
        std::snprintf(n, sizeof n, "%d M/S", static_cast<int>(std::lround(v)));
        break;
    case Control::aa:
        return v > .5f ? "FXAA" : "OFF";
    case Control::reticles:
    case Control::eyeTest:
        return v > .5f ? "ON" : "OFF";
    case Control::history:
        if (v < .5f)
            return "OFF";
        std::snprintf(n, sizeof n, "%d%%", static_cast<int>(std::lround(v)));
        break;
    case Control::reach:
        std::snprintf(n, sizeof n, "%d%%", static_cast<int>(std::lround(v)));
        break;
    case Control::monitor:
        if (v < .5f)
            return "EVERY FRAME";
        std::snprintf(n, sizeof n, "%d FPS", static_cast<int>(std::lround(v)));
        break;
    case Control::supersample:
        if (std::lround(v) <= 100)
            return "OFF";  // Update 23: 100% is no supersampling at all
        std::snprintf(n, sizeof n, "%d%%", static_cast<int>(std::lround(v)));
        break;
    case Control::sharpen:
        if (v < .5f)
            return "OFF";
        std::snprintf(n, sizeof n, "%d%%", static_cast<int>(std::lround(v)));
        break;
    case Control::eyeTrail: // Update 34.2
        std::snprintf(n, sizeof n, "%d%%", static_cast<int>(std::lround(v)));
        break;
    case Control::flyRollSpeed: // Update 38.5
        std::snprintf(n, sizeof n, "%d DEG/S", static_cast<int>(std::lround(v)));
        break;
    case Control::exposure: // Update 37.2: EV, 0 the game's
        if (std::abs(v) < .05f)
            return "GAME";
        std::snprintf(n, sizeof n, "%+.1f EV", static_cast<double>(v));
        break;
    case Control::warmth:
    case Control::tint:
        std::snprintf(n, sizeof n, "%+d", static_cast<int>(std::lround(v)));
        if (std::lround(v) == 0)
            return "0";
        break;
    case Control::vignette:
    case Control::vibrance:
        if (v < .5f)
            return "OFF";
        std::snprintf(n, sizeof n, "%d", static_cast<int>(std::lround(v)));
        break;
    case Control::colorStrength:
    case Control::contrast:
    case Control::saturation:
    case Control::foveaInner:
    case Control::colorCartoon:
        std::snprintf(n, sizeof n, "%d%%", static_cast<int>(std::lround(v)));
        break;
    case Control::shadowDistance: // Update 33: the reach unknown here (the row uses View's)
        if (v < .5f)
            return "GAME";
        std::snprintf(n, sizeof n, "%d M, %d CARS",
                      static_cast<int>(std::lround(static_cast<float>(std::lround(v)) * shadow_distance::carMetres)),
                      static_cast<int>(std::lround(v)));
        break;
    case Control::hudHeight:
        std::snprintf(n, sizeof n, "%d CM", static_cast<int>(std::lround(v)));
        break;
    case Control::hudScale:
        std::snprintf(n, sizeof n, "%.2fX", v);
        break;
    case Control::hudText:
        std::snprintf(n, sizeof n, "%d%%", static_cast<int>(std::lround(v)));
        break;
    case Control::hudTheme:
        return themeName(static_cast<int>(std::lround(v)));
    case Control::hudTextColor:
        return textColorName(static_cast<int>(std::lround(v)));
    case Control::webStyle:
        return web_style::name(static_cast<int>(std::lround(v)));
    case Control::webColor: // without its style: valueText(Control, Settings) names it
        std::snprintf(n, sizeof n, "SHADE %d", static_cast<int>(std::lround(v)) + 1);
        break;
    case Control::musicVolume:
        std::snprintf(n, sizeof n, "%d%%", static_cast<int>(std::lround(v)));
        break;
    case Control::musicVisualizer:
        return music::visualizerName(static_cast<int>(std::lround(v)));
    default:
        return {};
    }
    return n;
}

const char* shortName(Control c) {
    if (const int o = combatOption(c); o >= 0)
        return combat38::optionInfo[o].shortName;
    switch (c) {
    case Control::thickness:
        return "THICKNESS";
    case Control::speed:
        return "WEB SPEED";
    case Control::aa:
        return "AA";
    case Control::history:
        return "HISTORY";
    case Control::reach:
        return "REACH";
    case Control::monitor:
        return "WINDOW FPS";
    case Control::supersample:
        return "SUPERSAMPLE";
    case Control::sharpen:
        return "SHARPNESS";
    case Control::reticles:
        return "RETICLES";
    case Control::eyeTest:
        return "EYE TEST";
    case Control::addressFinder:
        return "ADDRESS FINDER";
    case Control::eyeLod:
        return "EYE LOD";
    case Control::shadowCamera:
        return "SHADOW CAMERA";
    case Control::shadowDistance:
        return "SHADOW DISTANCE";
    case Control::eyeAa:
        return "EYE AA";
    case Control::engineTaa:
        return "ENGINE TAA";
    case Control::eyeTrail:
        return "EYE AA TRAIL";
    case Control::taaJitter:
        return "TAA JITTER";
    case Control::farShadows:
        return "FAR SHADOWS";
    case Control::eyeBloom:
        return "EYE BLOOM";
    case Control::sharedLod:
        return "SHARED LOD";
    case Control::mirror:
        return "MIRROR";
    case Control::mirrorRecentre:
        return "RECENTRE";
    case Control::worldLog:
        return "WORLD LOG";
    case Control::blood:
        return "BLOOD";
    case Control::bloodVisor:
        return "VISOR";
    case Control::invincible:
        return "INVINCIBLE";
    case Control::flyMode:
        return "FLY";
    case Control::flyVignette:
        return "VIGNETTE";
    case Control::flyRoll:
        return "ROLL";
    case Control::flyRollSpeed:
        return "ROLL SPEED";
    case Control::flyAutoLevel:
        return "LEVEL";
    case Control::symbiote:
        return "SYMBIOTE";
    case Control::padMode:
        return "PAD MODE";
    case Control::promptHandling:
        return "PROMPTS";
    case Control::playOnMonitor:
        return "MONITOR";
    case Control::gestureMoves:
        return "GESTURES";
    case Control::trainMove:
        return "TRAIN";
    case Control::heroTabSpider:
        return "SPIDER-MAN";
    case Control::heroTabIron:
        return "IRON MAN";
    case Control::heroTabDaredevil:
        return "DAREDEVIL";
    case Control::heroTabHulk:
        return "HULK";
    case Control::heroPlaySpider:
        return "SPIDER-MAN";
    case Control::heroRoster:
        return "ROSTER";
    case Control::section0:
    case Control::section1:
    case Control::section2:
    case Control::section3:
    case Control::section4:
    case Control::section5:
    case Control::section6:
    case Control::section7:
    case Control::section8:
    case Control::section9:
    case Control::section10:
    case Control::section11:
        return "SECTION";
    case Control::gestureRemapMove:
        return "REMAP";
    case Control::gestureRecord:
        return "RECORD";
    case Control::gestureKeep:
        return "KEEP";
    case Control::gestureAgain:
        return "AGAIN";
    case Control::gestureCancel:
        return "CANCEL";
    case Control::gestureResetMove:
        return "RESET";
    case Control::gestureResetAll:
        return "RESET ALL";
    case Control::gestureProfile:
        return "PROFILE";
    case Control::mirrorAnchor:
        return "ANCHOR";
    case Control::npcFade:
        return "NPC FADE";
    case Control::slotTest:
        return "SLOT TEST";
    case Control::webBallTest:
        return "WEB BALL";
    case Control::colorFx:
        return "COLOR FX";
    case Control::colorFilter:
        return "FILTER";
    case Control::colorStrength:
        return "STRENGTH";
    case Control::exposure:
        return "EXPOSURE";
    case Control::contrast:
        return "CONTRAST";
    case Control::saturation:
        return "SATURATION";
    case Control::vibrance:
        return "VIBRANCE";
    case Control::warmth:
        return "WARMTH";
    case Control::tint:
        return "TINT";
    case Control::vignette:
        return "VIGNETTE";
    case Control::colorCartoon:
        return "CARTOON";
    case Control::colorOutlines:
        return "OUTLINES";
    case Control::colorBands:
        return "BANDS";
    case Control::foveated:
        return "FOVEATED";
    case Control::foveaInner:
        return "INNER SIZE";
    case Control::foveaDebug:
        return "FOV DEBUG";
    case Control::foveaStats:
        return "FOV FPS";
    case Control::recalibrate:
        return "RECALIBRATE";
    case Control::fpsReset:
        return "FPS RESET";
    case Control::hudHeight:
        return "HEIGHT";
    case Control::hudScale:
        return "SCALE";
    case Control::hudText:
        return "TEXT SIZE";
    case Control::hudTheme:
        return "THEME";
    case Control::hudTextColor:
        return "TEXT COLOR";
    case Control::webStyle:
        return "WEB STYLE";
    case Control::webColor:
        return "WEB COLOR";
    case Control::musicVolume:
        return "MUSIC VOLUME";
    case Control::musicVisualizer:
        return "VISUALIZER";
    default:
        return "";
    }
}
bool onRelease(Control c) {
    return c == Control::hudHeight || c == Control::hudScale || c == Control::hudText;
}
bool cycles(Control c) {
    return c == Control::hudTheme || c == Control::hudTextColor || arrows(c);
}
bool arrows(Control c) {
    return c == Control::webStyle || c == Control::webColor || c == Control::musicVisualizer;
}
bool musicButton(Control c) {
    return (c >= Control::musicPrevious && c <= Control::musicNext) || c == Control::musicSeek;
}
float listRowHeight(float text) {
    return std::max(.012f * textFactor(text), .010f);
}
int listRowAt(float u, float v, float scroll, float text, int first, int tracks) {
    if (!std::isfinite(u) || !std::isfinite(v) || v < viewBottom || v > viewTop || u < trackLeft || u > trackRight)
        return -1;
    const float k = textFactor(text);
    const Item* d = itemOf(layoutOf(musicPage, k), Control::musicTrack);
    const int shown = std::min(listRows, tracks - std::max(first, 0));
    if (!d || shown <= 0)
        return -1;
    const float row = (d->box.v0 - (v - clampScroll(musicPage, scroll, k))) / listRowHeight(k);
    if (!(row >= 0) || row >= static_cast<float>(shown))
        return -1;
    return static_cast<int>(row);
}
float seekFraction(float u) {
    return std::isfinite(u) ? std::clamp((u - trackLeft) / (trackRight - trackLeft), 0.f, 1.f) : 0.f;
}
static_assert(Settings::webStyles == web_style::count);
int webStyleOf(const Settings& s) {
    return std::isfinite(s.webStyle) ? std::clamp(static_cast<int>(std::lround(s.webStyle)), 0, web_style::count - 1)
                                     : 0;
}
int shadeCount(const Settings& s) {
    return web_style::shades(webStyleOf(s));
}
std::string valueText(Control c, const Settings& s) {
    if (c == Control::webColor) {
        const float v = s[c];
        return web_style::shadeName(webStyleOf(s), std::isfinite(v) ? static_cast<int>(std::lround(v)) : 0);
    }
    return valueText(c, s[c]);
}
bool restartRequired(Control c) {
    return c == Control::supersample;
}
bool gpuSide(Control c) {
    return c == Control::aa || c == Control::history || c == Control::reach || c == Control::monitor ||
           c == Control::sharpen || c == Control::eyeTest;
}
float clampTo(Control c, float value) {
    const Range r = range(c);
    return std::isfinite(value) ? std::clamp(value, r.low, r.high) : r.low;
}

void Applier::start(const Settings& live) {
    last_.fill(-1e9);
    for (unsigned i = 0; i < controls; ++i) {
        const auto c = static_cast<Control>(i);
        if (setting(c))
            good_[i] = wanted_[i] = clampTo(c, live[c]);
    }
    dirty_ = 0;
}
void Applier::want(Control c, float value) {
    const auto i = static_cast<unsigned>(c);
    if (i >= controls || !setting(c) || (disabled_ >> i & 1))
        return;
    wanted_[i] = clampTo(c, value);
    dirty_ |= 1u << i;
}
uint32_t Applier::due(double now, Control held) const {
    uint32_t out = 0;
    for (unsigned i = 0; i < maskedControls; ++i) {
        if (!(dirty_ >> i & 1))
            continue;
        const auto c = static_cast<Control>(i);
        if (onRelease(c) && held == c)
            continue; // the panel would move under the pointer dragging it
        if (!gpuSide(c) || held != c || !(now - last_[i] < gpuInterval))
            out |= 1u << i;
    }
    return out;
}
bool Applier::result(Control c, bool ok, float actual, double now) {
    const auto i = static_cast<unsigned>(c);
    if (i >= maskedControls)
        return false;
    last_[i] = now;
    dirty_ &= ~(1u << i);
    const float tolerance = range(c).step / 2;
    if (ok && std::isfinite(actual) && std::abs(actual - wanted_[i]) <= tolerance) {
        good_[i] = clampTo(c, actual);
        failures_[i] = 0;
        return true;
    }
    wanted_[i] = good_[i];
    char text[48];
    if (++failures_[i] >= failuresToDisable) {
        disabled_ |= 1u << i;
        std::snprintf(text, sizeof text, "%s OFF: REFUSED", shortName(c));
    } else {
        std::snprintf(text, sizeof text, "%s REFUSED", shortName(c));
    }
    fault(text, now);
    return false;
}
Settings Applier::shown(const Settings& live) const {
    Settings out = live;
    for (unsigned i = 0; i < controls; ++i) {
        const auto c = static_cast<Control>(i);
        if (setting(c))
            out[c] = clampTo(c, dirty_ >> i & 1 ? wanted_[i] : live[c]);
    }
    return out;
}
std::string Applier::note(double now) const {
    return now - noteAt_ >= 0 && now - noteAt_ < noteSeconds ? note_ : std::string();
}

Pointer point(const Frame& f, float scale, const Pose& grip, unsigned hand) {
    Pointer out;
    if (!validTrackedPose(grip) || !finite(f.centre) || !finite(f.right) || !finite(f.up))
        return out;
    const float s = std::isfinite(scale) ? std::clamp(scale, .5f, 2.f) : 1.f;
    const Vec3 dir = webShooter(grip, hand).orientation.rotate({0, 0, -1});
    out.tip = grip.position + dir * .05f;
    const Vec3 n = cross(f.right, f.up); // toward the eyes
    const float depth = dot(out.tip - f.centre, n);
    if (std::abs(depth) <= touchDepth) {
        out.at = out.tip - n * depth;
    } else {
        const float facing = dot(dir, n);
        if (depth < 0 || facing > -1e-3f)
            return out;
        const float t = -depth / facing;
        if (!(t >= 0 && t <= pointReach))
            return out;
        out.at = out.tip + dir * t;
    }
    out.u = dot(out.at - f.centre, f.right) / s;
    out.v = dot(out.at - f.centre, f.up) / s;
    out.hit = std::isfinite(out.u) && std::isfinite(out.v);
    return out;
}

constexpr float listRowsPerSecond = 8;
void Menu::setScroll(int page, float scroll, float text) {
    const int p = page >= 0 && page < pages ? page : 0;
    scroll_[static_cast<size_t>(p)] = clampScroll(p, scroll, textFactor(text));
}
namespace {
// 0.0.73 test 4 SETTINGS: a press on a hero's setting row asks what the old POWERS row asked.
void pressHeroSetting(Control c, Menu::Result& r) {
    switch (c) {
    case Control::invincible: r.invincible = true; break;
    case Control::flyMode: r.flyMode = true; break;
    case Control::flyVignette: r.flyVignette = true; break;
    case Control::flyRoll: r.flyRoll = true; break;
    case Control::flyAutoLevel: r.flyAutoLevel = true; break;
    case Control::symbiote: r.symbiote = true; break;
    case Control::heroPlaySpider: r.heroSpider = true; break;
    default:
        if (combatOption(c) >= 0)
            r.combat38 = combatOption(c);
        break;
    }
}
} // namespace
Menu::Result Menu::update(const Input& in, Settings& settings) {
    Result r;
    const float t = std::isfinite(in.trigger) ? std::clamp(in.trigger, 0.f, 1.f) : 0.f;
    const bool touched = t > (touched_ ? Toggle::rest : Toggle::touch);
    const bool first = touched && !touched_;
    touched_ = touched;
    const bool down = t > (down_ ? Toggle::released : Toggle::pressed);
    const bool press = down && !down_;
    down_ = down;
    const float k = textFactor(in.text);
    if (in.hero >= 0)
        setActiveHero(in.hero); // Update 39.0.2: POWERS opens the profile of the hero played
    // Update 17: the update the panel's size, place or text changes, what
    // the pointer is over moves under it: hover nothing, press nothing.
    const bool relaid = layoutSeen_ && in.layout != layout_;
    layout_ = in.layout;
    layoutSeen_ = true;
    // 0.0.73 test 4 (the user: "it should just be like the tiles with subpages not all the dropdowns"):
    // HERO MENU HOLO TILES: POWERS is the tiles - open on it, closed off it; CLASSIC keeps the old list.
    const bool tiles = in.heroTiles && !in.heroMenuClassic;
    if (roster_.isOpen() && tiles && page_ != powersPage) {
        roster_.close();
        r.rosterClosed = true;
    } else if (!roster_.isOpen() && tiles && page_ == powersPage) {
        roster_.open(in.hero >= 0 ? static_cast<uint32_t>(in.hero) : static_cast<uint32_t>(activeHero()));
        hover_ = held_ = Control::none;
        helpFor_ = Control::none;
        drag_ = Drag::none;
        closeList();
    }
    // 0.0.73 test 3 HOLO ROSTER (fix 28): while the heroes are open on the hologram, the panel is theirs:
    // the pointer and a fresh pull work their tiles and tabs, the stick scrolls the REP log.
    if (roster_.isOpen()) {
        if (in.heroMenuClassic) { // switched to CLASSIC while open: back to the page
            roster_.close();
            r.rosterClosed = true;
        }
        // 0.0.73 test 4 SETTINGS: a hero's setting rows are the menu's own controls, worked here; a
        // slider held keeps following the pointer.
        holo_roster::State& rs = roster_.state();
        const bool settingsTab = rs.screen == holo_roster::Screen::hero && rs.tab == holo_roster::Tab::settings;
        const bool sliding = settingsTab && claim_ && down && in.active && !relaid && dragged(held_);
        const Control hoverWasRow = hover_;
        hover_ = Control::none;
        if (!sliding)
            held_ = Control::none;
        rs.settingsPages = rs.screen == holo_roster::Screen::hero ? heroSettingsPages(rs.hero) : 1u;
        rs.settingsPage = std::min(rs.settingsPage, rs.settingsPages - 1);
        rs.settingHelp = nullptr;
        helpFor_ = Control::none;
        drag_ = Drag::none;
        closeList();
        if (!touched)
            claim_ = false;
        const bool onIt = in.active && in.pointer.hit && std::isfinite(in.pointer.u) && std::isfinite(in.pointer.v) &&
                          std::abs(in.pointer.u) <= halfW && std::abs(in.pointer.v) <= halfH;
        const bool onStrip = tiles && in.active && in.pointer.hit && std::isfinite(in.pointer.u) &&
                             std::isfinite(in.pointer.v) &&
                             (previousBox.in(in.pointer.u, in.pointer.v) || nextBox.in(in.pointer.u, in.pointer.v));
        if (first && (onIt || onStrip))
            claim_ = true;
        if (!roster_.isOpen() || !in.active)
            return r;
        // The arrow strip under the panel: off POWERS (the tiles close).
        if (onStrip && claim_ && press && !relaid) {
            setPage(previousBox.in(in.pointer.u, in.pointer.v) ? (page_ + pages - 1) % pages : (page_ + 1) % pages);
            roster_.close();
            r.rosterClosed = r.pressed = true;
            hover_ = held_ = Control::none;
            return r;
        }
        Control row = settingsTab && onIt && !relaid && !sliding
                           ? heroSettingAt(rs.hero, rs.settingsPage, in.pointer.u, in.pointer.v)
                           : Control::none;
        const bool rowOff = row != Control::none && (bit(row) & in.disabled) != 0;
        const holo_roster::Data* d = in.rosterData;
        if (!d) {
            if (holo_roster::version() != rosterVersion_)
                rosterVersion_ = holo_roster::latest(rosterData_);
            d = &rosterData_;
        }
        holo_roster::Data shown = *d;
        if (in.hero >= 0 && in.hero < static_cast<int>(holo_roster::heroes))
            shown.playing = static_cast<uint32_t>(in.hero);
        const int hoverWas = roster_.state().hover;
        const float stick = std::isfinite(in.stick) && std::abs(in.stick) > stickDead ? in.stick : 0.f;
        const bool onRow = row != Control::none || sliding;
        const holo_roster::Actions a = roster_.update(onIt && !relaid && !onRow, in.pointer.u, in.pointer.v,
                                                      claim_ && press && !relaid && !onRow, stick, in.seconds, shown);
        r.stick = onIt && stick != 0;
        r.hovered = roster_.state().hover != holo_roster::none && roster_.state().hover != hoverWas;
        r.pressed = claim_ && press && hoverWas != holo_roster::none && !onRow;
        r.rosterPlayAs = a.playAs;
        r.rosterSuitHero = a.suitHero;
        r.rosterSuitIndex = a.suitIndex;
        r.rosterClosed = r.rosterClosed || a.closed;
        if (onRow) {
            hover_ = sliding ? held_ : row;
            r.hovered = r.hovered || (hover_ != hoverWasRow);
            roster_.state().settingHelp = helpText(hover_);
            if (claim_ && press && !relaid && row != Control::none) {
                r.pressed = true;
                if (!rowOff) {
                    if (dragged(row))
                        held_ = row;
                    else
                        pressHeroSetting(row, r);
                }
            }
            if (claim_ && down && dragged(held_) && in.pointer.hit && std::isfinite(in.pointer.u)) {
                const float value = sliderValue(held_, in.pointer.u);
                if (!(std::abs(value - settings[held_]) < 1e-4f)) {
                    settings[held_] = value;
                    r.changed |= bit(held_);
                    r.flyRollSpeed = r.flyRollSpeed || held_ == Control::flyRollSpeed;
                }
            }
        }
        // CLOSE on the tiles' POWERS: back to STATUS (the tiles would only open again).
        if (a.closed && tiles && page_ == powersPage)
            setPage(0);
        return r;
    }
    for (int i = 0; i < pages; ++i)
        scroll_[static_cast<size_t>(i)] = clampScroll(i, scroll_[static_cast<size_t>(i)], k);
    float& scroll = scroll_[static_cast<size_t>(page_)];
    const bool on = in.active && in.pointer.hit && onPanel(page_, in.pointer.u, in.pointer.v);
    Control now = on && !relaid && drag_ == Drag::none ? controlAt(page_, in.pointer.u, in.pointer.v, scroll, k) : Control::none;
    if ((bit(now) & in.disabled) || (now == Control::shadowDistance && in.shadowDistanceOff))
        now = Control::none; // a disabled control: the press is still the panel's, it does nothing
    // Update 19: the song list closes off the MUSIC page, with no songs, or
    // when the panel moves; open, it lies over what is under the dropdown.
    const int tracks = std::max(in.musicTracks, 0);
    if (page_ != musicPage || !in.active || relaid || tracks == 0)
        closeList();
    if (listOpen_) {
        listFirst_ = std::clamp(listFirst_, 0, std::max(0, tracks - listRows));
        const int row = on && !relaid && drag_ == Drag::none
                            ? listRowAt(in.pointer.u, in.pointer.v, scroll, k, listFirst_, tracks)
                            : -1;
        if (row >= 0)
            now = Control::musicPick;
        const int song = row >= 0 ? listFirst_ + row : -1;
        r.hovered = song >= 0 && song != listHover_;
        listHover_ = song;
    }
    r.hovered = r.hovered || (now != Control::none && now != hover_);
    hover_ = now;
    // A pull that began on the panel is the panel's until it is let go.
    if (!touched)
        claim_ = false;
    else if (first && on)
        claim_ = true;
    if (!down || !in.active || relaid) { // relaid: a held slider or drag is let go, not misread
        held_ = Control::none;
        drag_ = Drag::none;
    }
    // Update 38.5: whose subtext is being read - the control pointed at, or
    // the one whose text the pointer is on (or a finger is dragging) - and how
    // many lines past its rows it runs (the drawing side's count when it has
    // one for it: a state's text can be longer than the plain help).
    bool helpReading = false;
    if (!in.active || relaid) {
        helpFor_ = Control::none;
        helpScroll_ = 0;
    } else {
        Control reading = now;
        if (drag_ == Drag::help)
            reading = helpFor_;
        else if (reading == Control::none && on && drag_ == Drag::none)
            reading = helpAt(page_, in.pointer.u, in.pointer.v, scroll, k);
        if (reading != Control::none && reading != helpFor_) {
            helpFor_ = reading;
            helpScroll_ = helpIdle_ = helpHold_ = 0;
            helpManual_ = helpBack_ = false;
        }
        helpReading = reading != Control::none && reading == helpFor_;
    }
    int helpMost = 0;
    if (helpFor_ != Control::none) {
        if (in.helpControl == helpFor_ && in.helpOverflow >= 0) {
            helpMost = in.helpOverflow;
        } else {
            for (const Item& it : layoutOf(page_, in.text).items)
                if (it.control == helpFor_ && !it.help.lines.empty()) {
                    helpMost = std::max(0, static_cast<int>(it.help.lines.size()) - it.help.rows);
                    break;
                }
        }
    }
    const float helpTop = static_cast<float>(helpMost);
    helpScroll_ = std::isfinite(helpScroll_) ? std::clamp(helpScroll_, 0.f, helpTop) : 0.f;
    if (!in.active)
        return r;
    if (claim_ && press && !relaid) {
        if (listOpen_ && hover_ != Control::musicPick && hover_ != Control::musicTrack &&
            hover_ != Control::previous && hover_ != Control::next) {
            closeList(); // a press off the open list only closes it
            r.pressed = true;
        } else if (hover_ != Control::none) {
            r.pressed = true;
            held_ = hover_;
            switch (hover_) {
            case Control::previous: // the arrows wrap (MUSIC <-> STATUS)
                setPage((page_ + pages - 1) % pages);
                closeList();
                break;
            case Control::next:
                setPage((page_ + 1) % pages);
                closeList();
                break;
            case Control::section0:
            case Control::section1:
            case Control::section2:
            case Control::section3:
            case Control::section4:
            case Control::section5:
            case Control::section6:
            case Control::section7:
            case Control::section8:
            case Control::section10:
            case Control::section11:
            case Control::section9: {
                // Update 39.0.2: open it (closing the page's other one), or close it; the header
                // stays where it is under the pointer (the rows above may have closed).
                const int s = sectionIndex(hover_);
                const Spot before = controlSpot(page_, hover_, 0, in.text);
                setOpenSection(page_, openSection(page_) == s ? -1 : s);
                const Spot after = controlSpot(page_, hover_, 0, in.text);
                if (before.found && after.found)
                    scroll += before.v - after.v;
                scroll = clampScroll(page_, scroll, k);
                closeList();
                r.section = true;
                break;
            }
            case Control::musicPick:
                if (listHover_ >= 0) {
                    r.music = Music::pick;
                    r.musicValue = static_cast<float>(listHover_);
                }
                closeList();
                break;
            case Control::musicTrack:
                if (listOpen_) {
                    closeList();
                } else {
                    r.music = Music::rescan; // opening looks for new songs
                    if (tracks > 0) {
                        listOpen_ = true;
                        listFirst_ = std::clamp(in.musicTrack - 2, 0, std::max(0, tracks - listRows));
                        listScroll_ = static_cast<float>(listFirst_);
                        listHover_ = -1;
                    }
                }
                break;
            case Control::musicPrevious:
                r.music = Music::previous;
                break;
            case Control::musicRewind:
                r.music = Music::rewind;
                break;
            case Control::musicPlay:
                r.music = Music::toggle;
                break;
            case Control::musicStop:
                r.music = Music::stop;
                break;
            case Control::musicForward:
                r.music = Music::forward;
                break;
            case Control::musicNext:
                r.music = Music::next;
                break;
            case Control::musicSeek:
                r.music = Music::seek;
                r.musicValue = seekShown_ = seekFraction(in.pointer.u);
                break;
            case Control::recalibrate:
                r.recalibrate = true;
                break;
            case Control::fpsReset:
                r.fpsReset = true;
                break;
            case Control::addressFinder:
                r.addressFinder = true;
                break;
            case Control::eyeLod:
                r.eyeLod = true;
                break;
            case Control::eyeAa:
                r.eyeAa = true;
                break;
            case Control::engineTaa:
                r.engineTaa = true;
                break;
            case Control::taaJitter:
                r.taaJitter = true;
                break;
            case Control::farShadows:
                r.farShadows = true;
                break;
            case Control::eyeBloom:
                r.eyeBloom = true;
                break;
            case Control::sharedLod:
                r.sharedLod = true;
                break;
            case Control::mirror:
                r.mirror = true;
                break;
            case Control::mirrorRecentre:
                r.mirrorRecentre = true;
                break;
            case Control::worldLog:
                r.worldLog = true;
                break;
            case Control::blood:
                r.blood = true;
                break;
            case Control::bloodVisor:
                r.bloodVisor = true;
                break;
            case Control::invincible:
                r.invincible = true;
                break;
            case Control::flyMode:
                r.flyMode = true;
                break;
            case Control::flyVignette:
                r.flyVignette = true;
                break;
            case Control::flyRoll:
                r.flyRoll = true;
                break;
            case Control::flyAutoLevel:
                r.flyAutoLevel = true;
                break;
            case Control::symbiote:
                r.symbiote = true;
                break;
            case Control::padMode:
                r.padMode = true;
                break;
            case Control::promptHandling:
                r.promptHandling = true;
                break;
            case Control::playOnMonitor:
                r.playOnMonitor = true;
                break;
            case Control::gestureMoves:
                r.gestureMoves = true;
                break;
            case Control::trainMove:
                r.trainMove = true;
                break;
            case Control::heroPlaySpider:
                r.heroSpider = true;
                break;
            case Control::heroRoster:
                // 0.0.73 test 3 HOLO ROSTER (fix 28): the heroes open on the hologram; only HERO MENU
                // CLASSIC asks for the old big popup.
                if (in.heroMenuClassic) {
                    r.heroRoster = true;
                } else {
                    roster_.open(in.hero >= 0 ? static_cast<uint32_t>(in.hero) : 0u);
                    hover_ = held_ = Control::none;
                    helpFor_ = Control::none;
                }
                break;
            case Control::gestureRemapMove:
                r.remapMove = true;
                break;
            case Control::gestureRecord:
                r.remapRecord = true;
                break;
            case Control::gestureKeep:
                r.remapKeep = true;
                break;
            case Control::gestureAgain:
                r.remapAgain = true;
                break;
            case Control::gestureCancel:
                r.remapCancel = true;
                break;
            case Control::gestureResetMove:
                r.remapResetMove = true;
                break;
            case Control::gestureResetAll:
                r.remapResetAll = true;
                break;
            case Control::gestureProfile:
                r.remapProfile = true;
                break;
            case Control::mirrorAnchor:
                r.mirrorAnchor = true;
                break;
            case Control::npcFade:
                r.npcFade = true;
                break;
            case Control::slotTest:
                r.slotTest = true;
                break;
            case Control::webBallTest:
                r.webBallTest = true;
                break;
            case Control::combatCrawlCap:
            case Control::combatZipMode:
            case Control::combatBallAim:
            case Control::combatBallLook:
            case Control::combatPunchMode:
            case Control::combatRingAnchor:
            case Control::combatHoloOpen:
            case Control::combatWallTilt:
            case Control::combatWebRope:
            case Control::combatLunge:
            case Control::combatBallFire:
            case Control::combatAimAxis:
            case Control::combatRingLayer:
            case Control::combatLungeFrom:
            case Control::combatLungeSpeed:
            case Control::combatLungeAim:
            case Control::combatPunchAngle:
            case Control::combatBallLead:
            case Control::combatBloodLook:
            case Control::combatMeleeAnim:
            case Control::combatGadgetKeys:
            case Control::combatGrabWeb:
            case Control::combatMirrorLook:
            case Control::combatGadgetFeel:
            case Control::combatLookLock:
            case Control::combatStrikeSteer:
            case Control::combatFarWindows:
            case Control::combatGadgetHint:
            case Control::combatAimStrength:
            case Control::combatBallHit:
            case Control::combatRopeGrab:
            case Control::combatRopeStrength:
            case Control::combatWallStick:
            case Control::combatAutoPerf:
            case Control::combatPerfTarget:
            case Control::combatPerfFloor:
            case Control::combatPerfStatus:
            case Control::combatShooterType:
            case Control::combatMirrorView:
            case Control::combatHeroHeight:
            case Control::combatHeroDamage:
            case Control::combatPunchSense:
            case Control::combatSfxVolume:
            case Control::combatSupersonic:
            case Control::combatSupersonicSpeed:
            case Control::combatDaredevil:
            case Control::combatHulk:
            case Control::combatDaredevilPunch:
            case Control::combatHitIndicator:
            case Control::combatHandFlips:
            case Control::combatLensFlares:
            case Control::combatPunchHits:
            case Control::combatFoeTrack:
            case Control::combatBeaconsVision:
            case Control::combatBatman: // Version 0.0.7
            case Control::combatWolverine:
            case Control::combatSuperman:
            case Control::combatPunisher:
            case Control::combatBatarangChain:
            case Control::combatEasyReload:
            case Control::combatHolsterHands:
            case Control::combatBatonHolsters:
            case Control::combatBatonLook:
            case Control::combatBatarangStyle:
            case Control::combatGunModels:
            case Control::combatCaptainAmerica: // Version 0.0.7 CAPTAIN AMERICA
            case Control::combatShieldArm:
            case Control::combatShieldDamage:
            case Control::combatShieldChain:
            case Control::combatShieldBlock:
            case Control::combatShieldLook:
            case Control::combatThor:
            case Control::combatThorLightning:
            case Control::combatThorStorm:
            case Control::combatThorChain:
            case Control::combatGreenLantern:
            case Control::combatGlFlight:
            case Control::combatGlFist:
            case Control::combatGlBeam:
            case Control::combatGlBubble:
            case Control::combatGlChain:
            case Control::combatGlCharge:
            case Control::combatSpiderClimbOnly:
            case Control::combatUppercut:
            case Control::combatSmokeBlind:
            case Control::combatDdGrapple:
            case Control::combatDdParry:
            case Control::combatIronHud:
            case Control::combatPunisherRage:
            case Control::combatReputation:
            case Control::combatSurvival:
            case Control::combatPunisherSuit:
            case Control::combatNaturalInvincible:
            case Control::combatHulkRage:
            case Control::combatRageBlood:
            case Control::combatRageTint:
            case Control::combatFireDouse:
            case Control::combatShieldSlam:
            case Control::combatPunisherHook:
            case Control::combatClawClimb:
            case Control::combatNpcShove:
            case Control::combatFaction:
            case Control::combatHeroSuit:
            case Control::combatTestUnlocks:
            case Control::combatUltimates:
            case Control::combatPerkBranch:
            case Control::combatOwnSuit:
            case Control::combatPerchTakedown:
            case Control::combatTakedownLight:
            case Control::combatPunisherVan:
            case Control::combatThorHammer:
            case Control::combatGameBody:
            case Control::combatThorHammerModel:
            case Control::combatThorFlight:
            case Control::combatSuitBody:
            case Control::combatCapSprint:
            case Control::combatCullMargin:
            case Control::combatShadowMatch:
            case Control::combatCalibLook:
            case Control::combatFlyPose:
            case Control::combatWeaponCollision:
            case Control::combatLightHold:
            case Control::combatVanScreen:
            case Control::combatPalmBlast:
            case Control::combatHeroMenu:
            case Control::combatRepLog:
            case Control::combatVanPedals:
            case Control::combatImage:
            case Control::combatDdBatonTip:
                r.combat38 = combatOption(hover_);
                break;
            case Control::colorFx:
                r.colorFx = true;
                break;
            case Control::colorFilter:
                r.colorFilter = true;
                break;
            case Control::colorOutlines:
                r.colorOutlines = true;
                break;
            case Control::colorBands:
                r.colorBands = true;
                break;
            case Control::foveated:
                r.foveated = true;
                break;
            case Control::foveaDebug:
                r.foveaDebug = true;
                break;
            case Control::foveaStats:
                r.foveaStats = true;
                break;
            case Control::shadowCamera:
                r.shadowCamera = true;
                break;
            case Control::aa:
            case Control::reticles:
            case Control::eyeTest:
                settings[hover_] = settings[hover_] > .5f ? 0.f : 1.f;
                r.changed |= bit(hover_);
                break;
            case Control::hudTheme:
            case Control::hudTextColor:
            case Control::webStyle:
            case Control::webColor:
            case Control::musicVisualizer: {
                const Range range_ = range(hover_);
                // WEB COLOR: the style's own list (Update 18).
                const int n = hover_ == Control::webColor ? shadeCount(settings) : static_cast<int>(range_.high) + 1;
                const float v = settings[hover_];
                const int at = std::isfinite(v) ? static_cast<int>(std::lround(v)) : 0;
                // Update 18: < on the left half steps back, > on the right forward.
                const int step = arrows(hover_) && in.pointer.u < (trackLeft + trackRight) / 2 ? -1 : 1;
                settings[hover_] = static_cast<float>(((at + step) % n + n) % n);
                r.changed |= bit(hover_);
                break;
            }
            default:
                break;
            }
        } else if (on && helpMost > 0 && helpReading &&
                   helpAt(page_, in.pointer.u, in.pointer.v, scroll, k) == helpFor_) {
            // Update 38.5: a finger on a long subtext drags its lines.
            drag_ = Drag::help;
            dragV_ = in.pointer.v;
            dragScroll_ = helpScroll_;
            helpManual_ = true;
        } else if (on && maxScroll(page_, k) > 0 && in.pointer.v >= viewBottom && in.pointer.v <= viewTop &&
                   std::abs(in.pointer.u) <= halfW) {
            // Empty panel or the scrollbar: a drag that scrolls.
            drag_ = onScrollbar(page_, in.pointer.u, in.pointer.v, k) ? Drag::bar : Drag::content;
            dragV_ = in.pointer.v;
            dragScroll_ = scroll;
        }
    }
    float& shown = scroll_[static_cast<size_t>(page_)]; // the page may have changed
    if (claim_ && down && drag_ == Drag::help && in.pointer.hit && std::isfinite(in.pointer.v)) {
        // The lines follow the finger: up shows the later ones.
        const float pitch = helpPitch * k;
        const float want = std::clamp(dragScroll_ + (in.pointer.v - dragV_) / pitch, 0.f, helpTop);
        if (std::abs(want - helpScroll_) > 1e-4f) {
            helpScroll_ = want;
            r.scrolled = true;
        }
    } else if (claim_ && down && drag_ != Drag::none && in.pointer.hit && std::isfinite(in.pointer.v)) {
        float want = dragScroll_ + (in.pointer.v - dragV_); // the content follows the pointer up
        if (drag_ == Drag::bar) {
            const Thumb th = scrollThumb(page_, shown, k);
            const float room = scrollTrackTop - scrollTrackBottom - (th.v1 - th.v0);
            want = room > 1e-4f ? dragScroll_ + (dragV_ - in.pointer.v) * maxScroll(page_, k) / room : dragScroll_;
        }
        want = clampScroll(page_, want, k);
        if (std::abs(want - shown) > 1e-6f) {
            shown = want;
            r.scrolled = true;
        }
    }
    // The stick: the panel's while the pointer is on it; it scrolls while
    // nothing is held or dragged.
    if (on) {
        r.stick = true;
        const float y = std::isfinite(in.stick) ? std::clamp(in.stick, -1.f, 1.f) : 0.f;
        const float dt = std::isfinite(in.seconds) ? std::clamp(in.seconds, 0.f, .1f) : 0.f;
        if (std::abs(y) > stickDead && held_ == Control::none && drag_ == Drag::none && listOpen_) {
            // Update 19: the open song list scrolls instead of the page.
            const float tilt = (std::abs(y) - stickDead) / (1 - stickDead) * (y > 0 ? 1.f : -1.f);
            const float most = static_cast<float>(std::max(0, tracks - listRows));
            listScroll_ = std::clamp(listScroll_ - tilt * listRowsPerSecond * dt, 0.f, most);
            const int firstRow = static_cast<int>(std::lround(listScroll_));
            if (firstRow != listFirst_) {
                listFirst_ = firstRow;
                r.scrolled = true;
            }
        } else if (std::abs(y) > stickDead && held_ == Control::none && drag_ == Drag::none && helpReading &&
                   helpMost > 0 && (y > 0 ? helpScroll_ > 1e-3f : helpScroll_ < helpTop - 1e-3f)) {
            // Update 38.5: a long subtext pointed at scrolls first; at its end the page does.
            const float tilt = (std::abs(y) - stickDead) / (1 - stickDead) * (y > 0 ? 1.f : -1.f);
            helpScroll_ = std::clamp(helpScroll_ - tilt * helpStickLines * dt, 0.f, helpTop);
            helpManual_ = true;
            r.scrolled = true;
        } else if (std::abs(y) > stickDead && held_ == Control::none && drag_ == Drag::none) {
            const float tilt = (std::abs(y) - stickDead) / (1 - stickDead) * (y > 0 ? 1.f : -1.f);
            const float want = clampScroll(page_, shown - tilt * stickSpeed * dt, k); // up: toward the top
            if (std::abs(want - shown) > 1e-6f) {
                shown = want;
                r.scrolled = true;
            }
        }
    }
    // Update 38.5: the fallback - pointed at for helpAutoDelay seconds with no
    // stick or finger on it, a long subtext steps a line at a time (each eased
    // over half a second, held helpAutoHold), holds at the end, then glides
    // back to the top. Smooth, so nothing flickers.
    if (on && helpReading && helpMost > 0 && !helpManual_ && drag_ == Drag::none) {
        const float dt = std::isfinite(in.seconds) ? std::clamp(in.seconds, 0.f, .1f) : 0.f;
        helpIdle_ += dt;
        if (helpIdle_ >= helpAutoDelay) {
            const float before = helpScroll_;
            if (helpBack_) {
                helpScroll_ = std::max(0.f, helpScroll_ - helpBackLines * dt);
                if (helpScroll_ <= 0) {
                    helpBack_ = false;
                    helpHold_ = 0;
                    helpIdle_ = 0; // the first line read again before the next step
                }
            } else {
                const float line = std::floor(helpScroll_ + 1e-3f);
                if (helpScroll_ - line > 1e-3f) { // between lines: on to the next
                    helpScroll_ = std::min(line + 1, helpScroll_ + helpAutoLines * dt);
                } else if ((helpHold_ += dt) >= helpAutoHold) {
                    helpHold_ = 0;
                    if (helpScroll_ >= helpTop - 1e-3f)
                        helpBack_ = true;
                    else
                        helpScroll_ = std::min(helpTop, helpScroll_ + helpAutoLines * dt);
                }
            }
            r.scrolled = r.scrolled || std::abs(helpScroll_ - before) > 1e-6f;
        }
    } else if (!helpReading) {
        helpIdle_ = helpHold_ = 0;
    }
    // A held slider follows the pointer along its track, on the panel or off it.
    if (claim_ && down && dragged(held_) && in.pointer.hit) {
        const float value = sliderValue(held_, in.pointer.u);
        if (!(std::abs(value - settings[held_]) < 1e-4f)) {
            settings[held_] = value;
            r.changed |= bit(held_);
            r.shadowDistance = r.shadowDistance || held_ == Control::shadowDistance;
            r.eyeTrail = r.eyeTrail || held_ == Control::eyeTrail;
            r.colorSlider = r.colorSlider || colorSlider(held_);
            r.foveaInner = r.foveaInner || held_ == Control::foveaInner;
            r.flyRollSpeed = r.flyRollSpeed || held_ == Control::flyRollSpeed;
        }
    }
    // Update 19: a held progress bar seeks as it is dragged (not every hair).
    if (claim_ && down && held_ == Control::musicSeek && in.pointer.hit && r.music == Music::none) {
        const float f = seekFraction(in.pointer.u);
        if (std::abs(f - seekShown_) > .01f) {
            r.music = Music::seek;
            r.musicValue = seekShown_ = f;
        }
    }
    return r;
}

Vec3 wrist(const Pose& grip, unsigned hand) {
    // body::Config::wristFromGrip, a right hand's; a left mirrors x.
    const float back = hand == 0 ? -.02f : .02f;
    return compose(grip, Pose{{back, .09f, 0}, {}}).position;
}
Vec3 emitter(const Pose& grip, unsigned hand) {
    const float back = hand == 0 ? -.045f : .045f;
    return compose(grip, Pose{{back, .085f, 0}, {}}).position;
}

float armDistance(const Pose& point, unsigned pointHand, const Pose& arm, unsigned armHand) {
    // cos and sin of 30 degrees: the forearm from the grip's +y toward its +z.
    const Vec3 along = arm.orientation.rotate({0, .8660254f, .5f});
    // From the grip, 2 cm to the back of the hand as wrist() is.
    const Vec3 from = arm.position + arm.orientation.rotate({armHand == 0 ? -.02f : .02f, 0, 0});
    const auto toSegment = [&](Vec3 p) {
        const float t = std::clamp(dot(p - from, along), 0.f, Toggle::forearm);
        return length(p - (from + along * t));
    };
    return std::min(toSegment(point.position), toSegment(wrist(point, pointHand)));
}

float Toggle::zoneDistanceOf(const Pose& point, unsigned pointHand, const Pose& arm, unsigned armHand) {
    // From the hologram wrist joint back along the forearm (armDistance's line), zoneForearm long.
    const Vec3 along = arm.orientation.rotate({0, .8660254f, .5f});
    const Vec3 from = wrist(arm, armHand);
    const auto toSegment = [&](Vec3 p) {
        const float t = std::clamp(dot(p - from, along), 0.f, zoneForearm);
        return length(p - (from + along * t));
    };
    // The pointing hand: its grip, its wrist joint and its controller's tip (6 cm out along
    // the grip's -y, where the hand points).
    const Vec3 tip = compose(point, Pose{{0, -.06f, 0}, {}}).position;
    return std::min({toSegment(point.position), toSegment(wrist(point, pointHand)), toSegment(tip)});
}
void Toggle::look(const Pose& head, bool valid) {
    head_ = head;
    headValid_ = valid && validTrackedPose(head);
}
bool Toggle::palmLookOf(const Pose& arm, unsigned armHand, const Pose& head) {
    // The grip's +x is out of a left palm and into a right one (tracking.hpp, webShooter).
    const Vec3 palm = arm.orientation.rotate({armHand == 0 ? 1.f : -1.f, 0, 0});
    const Vec3 at = wrist(arm, armHand);
    const Vec3 toHead = head.position - at;
    if (!finite(palm) || !finite(toHead) || length(toHead) < .05f)
        return false;
    const bool facing = dot(normalized(palm), normalized(toHead)) >= palmFacing;
    const Vec3 forward = head.orientation.rotate({0, 0, -1});
    const float c = std::clamp(dot(normalized(forward), normalized(at - head.position)), -1.f, 1.f);
    return facing && std::acos(c) * 57.2957795f <= lookDegrees;
}
bool Toggle::update(float seconds, bool play, unsigned hudHand, const Pose grips[2], const bool tracked[2],
                    const float triggers[2], const float squeezes[2]) {
    const float dt = std::isfinite(seconds) ? std::clamp(seconds, 0.f, .1f) : 0.f;
    wait_ = std::max(0.f, wait_ - dt);
    const unsigned hud = hudHand ? 1u : 0u, other = 1u - hud;
    bool toggled = false;
    Diagnostics& d = diagnostics_;
    const bool legacy = mode_ > 4;
    const bool forearmMode = mode_ == 4;
    // Update 38.1: the zone and the palm-up look, both in the grips' own (tracking) space.
    const bool both = tracked[other] && tracked[hud] && validTrackedPose(grips[other]) && validTrackedPose(grips[hud]);
    float zoneD = both ? zoneDistanceOf(grips[other], other, grips[hud], hud) : -1.f;
    if (!std::isfinite(zoneD))
        zoneD = -1;
    const bool palm = !legacy && headValid_ && tracked[hud] && validTrackedPose(grips[hud]) &&
                      palmLookOf(grips[hud], hud, head_);
    const bool zoned = !legacy && mode_ != 3;
    // Update 38.2 FOREARM: the zone is the forearm itself; the pointing hand's speed relative
    // to the hologram hand (a punch or a swing past the arm is never a click).
    float forearmD = -1, pointSpeed = 0;
    if (forearmMode && both) {
        forearmD = armDistance(grips[other], other, grips[hud], hud);
        if (!std::isfinite(forearmD))
            forearmD = -1;
        const Vec3 rel = grips[other].position - grips[hud].position;
        if (lastPointValid_ && dt > 1e-4f)
            pointSpeed = length(rel - lastPoint_.position) / dt;
        lastPoint_.position = rel;
        lastPointValid_ = true;
    } else {
        lastPointValid_ = false;
    }
    const bool inZone = zoned && play &&
                        (forearmMode ? forearmD >= 0 && forearmD <= forearmZone : zoneD >= 0 && zoneD <= zone);
    linger_ = inZone ? zoneLinger : std::max(0.f, linger_ - dt);
    d.zoneDistance = play ? zoneD : -1.f;
    d.palmLook = play && palm;
    d.inZone = inZone;
    const auto toggle = [&] {
        on_ = !on_;
        ++toggles_;
        wait_ = rearm;
        toggled = true;
    };
    bool firstTouch[2]{}, pressedNow[2]{};
    for (unsigned i = 0; i < 2; ++i) {
        const float trigger = tracked[i] && std::isfinite(triggers[i]) ? triggers[i] : 0.f;
        // Touched: the trigger off its rest. The claim starts there, before
        // the pull is deep enough for anything of the game's to answer it.
        const bool touched = trigger > (touched_[i] ? rest : touch);
        const bool first = touched && !touched_[i];
        touched_[i] = touched;
        const bool down = trigger > (down_[i] ? released : pressed);
        const bool press = down && !down_[i];
        down_[i] = down;
        firstTouch[i] = first;
        pressedNow[i] = press;
        const uint32_t bit = 1u << i;
        float distance = -1;
        if (i == other && both) {
            distance = armDistance(grips[i], i, grips[hud], hud);
            if (!std::isfinite(distance))
                distance = -1;
        }
        const bool inReach = distance >= 0 && distance <= (forearmMode ? forearmReach : reach);
        if (i == other) {
            d.distance = play ? distance : -1;
            d.inReach = play && inReach;
            if (play && distance >= 0 && (d.nearest < 0 || distance < d.nearest))
                d.nearest = distance;
            if (press) {
                d.pressDistance = distance;
                if (!play)
                    ++d.pressesNotPlaying;
                else {
                    ++d.presses;
                    ++(inReach ? d.pressesInReach : d.pressesOutOfReach);
                    if (inReach && wait_ > 0)
                        ++d.pressesRearming;
                }
            }
        }
        if (!touched || !play) {
            hands_ &= ~bit;
            continue;
        }
        const bool opening = !on_;
        // 38.1 TAP / PALM LOOK: no click in reach; the zone (below) takes the pulls.
        if (mode_ == 0 || mode_ == 1)
            continue;
        // F8 ONLY: nothing claimed while the hologram is closed (closing is the click's).
        if (mode_ == 3 && opening) {
            if (press && inReach && i == other)
                ++d.leftToGame;
            hands_ &= ~bit;
            continue;
        }
        // CLICK: with the hologram hand's trigger pulled too (the Sonic Burst) an opening
        // pull never opens - and is still claimed to its release, never handed back.
        const bool sonic = tracked[hud] && std::isfinite(triggers[hud]) && triggers[hud] > touch;
        if ((mode_ == 2 || forearmMode) && opening && i == other && sonic && (first || press)) {
            if (inReach)
                hands_ |= bit;
            if (press && inReach)
                ++d.cancelled;
            continue;
        }
        // The pull is the toggle's from its start to its release, even one
        // too soon after the last toggle or drifting out of reach.
        if (first && inReach)
            hands_ |= bit;
        if (!press || !inReach)
            continue;
        // Update 20: a pull begun out of reach, pressed in reach, is the
        // toggle's from here on.
        hands_ |= bit;
        if (wait_ > 0)
            continue;
        if (forearmMode && pointSpeed > forearmSpeed) {
            ++d.forearmFast;
            continue;
        }
        toggle();
        if (forearmMode)
            ++d.forearmClicks;
    }
    // Update 38.1 ZONE: the pointing hand's trigger and grip pressed in it (or just out of
    // it) are swallowed to their release - never given back to the game.
    {
        const float squeeze =
            squeezes && tracked[other] && std::isfinite(squeezes[other]) ? squeezes[other] : 0.f;
        const bool squeezed = squeeze > (squeezed_[other] ? rest : touch);
        const bool firstSqueeze = squeezed && !squeezed_[other];
        squeezed_[other] = squeezed;
        squeezed_[hud] = false;
        const bool held = touched_[other] || squeezed;
        if (zoned && play && (inZone || linger_ > 0) && (firstTouch[other] || firstSqueeze) && !swallowing_[other]) {
            swallowing_[other] = true;
            ++d.zoneSwallowed;
        }
        if (!held || !zoned)
            swallowing_[other] = false;
        swallowing_[hud] = false;
        zoneHands_ = swallowing_[other] ? 1u << other : 0u;
    }
    // Update 38.1 TAP (every gesture mode): in the zone `tapDwell` with the palm-up look,
    // then out (or `tapHold` in it), or a click in the zone with the look, toggles once.
    // The hologram hand's own trigger pulled (the Sonic Burst's half): never a tap or a look.
    const bool sonicHeld = tracked[hud] && std::isfinite(triggers[hud]) && triggers[hud] > touch;
    if (zoned && play && !forearmMode) {
        const auto tap = [&] {
            if (sonicHeld) {
                if (!tapDone_)
                    ++d.cancelled;
            } else if (!tapDone_ && wait_ <= 0) {
                toggle();
                ++d.taps;
                lookOpened_ = false;
                lookSpent_ = palm; // PALM LOOK: break the look before it opens again
            }
            tapDone_ = true;
        };
        if (inZone) {
            if (palm)
                dwell_ += dt;
            if (dwell_ >= tapHold || (palm && pressedNow[other]))
                tap();
        } else {
            if (dwell_ >= tapDwell)
                tap();
            dwell_ = 0;
            tapDone_ = false;
        }
    } else {
        dwell_ = 0;
        tapDone_ = false;
    }
    // Update 38.1 PALM LOOK: the look held opens; broken a moment, closes what it opened.
    if (mode_ == 1 && play) {
        if (palm) {
            brokenSeconds_ = 0;
            lookSeconds_ += dt;
            if (!on_ && !lookSpent_ && !sonicHeld && lookSeconds_ >= lookHold && wait_ <= 0) {
                toggle();
                lookOpened_ = true;
                ++d.lookOpens;
            }
        } else {
            lookSeconds_ = 0;
            lookSpent_ = false;
            if (on_ && lookOpened_ && (brokenSeconds_ += dt) >= lookLetGo) {
                toggle();
                lookOpened_ = false;
                ++d.lookCloses;
            }
        }
    } else {
        lookSeconds_ = brokenSeconds_ = 0;
        lookOpened_ = lookSpent_ = false;
    }
    const float step = dt / fadeSeconds;
    opacity_ = std::clamp(opacity_ + (on_ ? step : -step), 0.f, 1.f);
    return toggled;
}
bool Toggle::flip() {
    if (wait_ > 0)
        return false;
    on_ = !on_;
    ++toggles_;
    ++diagnostics_.flips;
    wait_ = rearm;
    return true;
}

Frame place(const View& view, Vec3 viewer) {
    const float s = std::isfinite(view.scale) ? std::clamp(view.scale, .5f, 2.f) : 1.f;
    Vec3 up = view.headUp;
    up = finite(up) && length(up) > 1e-3f ? normalized(up) : Vec3{0, 1, 0};
    // Update 17: the height in cm whatever the scale; the panel's size scales.
    const float height = std::isfinite(view.heightCm) ? std::clamp(view.heightCm, 0.f, 25.f) / 100 : gap;
    const Vec3 centre = view.emitter + up * (height + panelHeight / 2 * s);
    Vec3 look = centre - viewer;
    look = length(look) > .02f ? normalized(look) : up * -1.f;
    Vec3 right = cross(look, up);
    right = length(right) > 1e-3f ? normalized(right) : soft::perpendicular(look);
    return {centre, right, normalized(cross(right, look))};
}

// 0.0.73 test 3 HOLO ROSTER (fix 28): the heroes on the hologram, holo_roster's pieces in the theme's colours.
void holoRoster(const Painter& p, const View& view) {
    namespace hr = holo_roster;
    const hr::Data* d = view.rosterData;
    thread_local hr::Data latest{};
    thread_local uint32_t seen = ~0u;
    if (!d) {
        if (hr::version() != seen)
            seen = hr::latest(latest);
        d = &latest;
    }
    thread_local std::vector<hr::Piece> ps;
    ps.clear();
    // 0.0.73 test 4: the tiles' POWERS before the menu has opened them: the grid of the hero played.
    hr::State st = view.roster;
    if (st.screen == hr::Screen::closed) {
        st.screen = hr::Screen::grid;
        st.hero = std::min(static_cast<uint32_t>(activeHero()), hr::heroes - 1);
        st.group = hr::groupOf(st.hero);
    }
    const bool settingsRows = st.screen == hr::Screen::hero && st.tab == hr::Tab::settings;
    if (st.screen == hr::Screen::hero) {
        st.settingsPages = heroSettingsPages(st.hero);
        st.settingsPage = std::min(st.settingsPage, st.settingsPages - 1);
    }
    hr::pieces(st, *d, ps);
    const auto colour = [&](const hr::Piece& x) {
        switch (x.role) {
        case hr::Role::text: return p.c.text;
        case hr::Role::label: return p.c.label;
        case hr::Role::value: return p.c.value;
        case hr::Role::help: return p.c.help;
        case hr::Role::accent: return p.c.accent;
        case hr::Role::frame: return p.c.frame;
        case hr::Role::line: return p.c.line;
        case hr::Role::good: return p.c.good;
        case hr::Role::warn: return p.c.warn;
        case hr::Role::bad: return p.c.bad;
        case hr::Role::backing: return p.c.backing;
        case hr::Role::custom: break;
        }
        return finite(x.color) ? x.color : p.c.text;
    };
    // 0.0.73 test 3 PORTRAITS: which heroes' portraits are drawn (their initials then are not).
    const float opacity = std::isfinite(view.opacity) ? std::clamp(view.opacity, 0.f, 1.f) : 0.f;
    bool drawn[hero_modes::heroes]{};
    const auto portrait = [&](const hr::Piece& x) {
        if (!view.rosterImages || !view.rosterPictures || opacity <= .55f || x.portrait < 0 ||
            x.portrait >= static_cast<int>(hero_modes::heroes) || x.portrait >= static_cast<int>(roster_page::pictureCount))
            return false;
        const roster_page::Picture& q = view.rosterPictures[x.portrait];
        const bool box = std::isfinite(q.u0) && std::isfinite(q.v0) && std::isfinite(q.u1) && std::isfinite(q.v1) &&
                         q.u0 >= -.01f && q.v0 >= -.01f && q.u1 <= 1.01f && q.v1 <= 1.01f && q.u1 - q.u0 > 1e-4f &&
                         q.v1 - q.v0 > 1e-4f;
        if (q.layer < 0 || q.layer > 4095 || !box)
            return false;
        // A square on the disc's centre (a touch wider than the disc), framed in the theme's colours.
        const float cu = (x.u0 + x.u1) / 2, cv = (x.v0 + x.v1) / 2, h = (x.u1 - x.u0) / 2 * 1.08f;
        const float u0 = cu - h, u1 = cu + h, v0 = cv - h, v1 = cv + h;
        const float layer = static_cast<float>(q.layer);
        const Vec3 tint{.93f, .98f, 1.f}; // the faint holo blue over the picture
        const Vec3 lift = p.normal * (.0006f * p.s); // a hair toward the eye, as the popup's pictures sit
        const auto at = [&](float u, float v) { return p.at(u, v) + lift; };
        const Vertex tl{at(u0, v1), tint, 1.f, {q.u0, q.v0, layer, 1}}, tr{at(u1, v1), tint, 1.f, {q.u1, q.v0, layer, 1}},
            br{at(u1, v0), tint, 1.f, {q.u1, q.v1, layer, 1}}, bl{at(u0, v0), tint, 1.f, {q.u0, q.v1, layer, 1}};
        for (const Vertex* y : {&tl, &tr, &br, &bl})
            if (!finite(y->position))
                return false;
        auto& out = *view.rosterImages;
        out.insert(out.end(), {tl, bl, br, tl, br, tr});
        // The frame just outside it (the translucent pass is drawn first: never under the picture).
        const float w = .0008f, e = w;
        p.bar(u0 - e, v1 + e, u1 + e, v1 + e, w, p.c.frame, 1);
        p.bar(u0 - e, v0 - e, u1 + e, v0 - e, w, p.c.frame, 1);
        p.bar(u0 - e, v0 - e, u0 - e, v1 + e, w, p.c.frame, 1);
        p.bar(u1 + e, v0 - e, u1 + e, v1 + e, w, p.c.frame, 1);
        p.rect(u0 - e * 3, v0 - e * 3, u1 + e * 3, v0 - e * 1.6f, p.c.accent, .35f); // a glow under it
        return true;
    };
    for (const hr::Piece& x : ps) {
        const Vec3 c = colour(x);
        const float a = std::isfinite(x.alpha) ? std::clamp(x.alpha, 0.f, 1.f) : 1.f;
        if (x.kind == hr::Piece::disc && x.portrait >= 0 && x.portrait < static_cast<int>(hero_modes::heroes)) {
            bool ok = false;
            try {
                ok = portrait(x);
            } catch (...) {
                ok = false;
            }
            if (ok) {
                drawn[x.portrait] = true;
                continue;
            }
        }
        if (x.fallback && x.portrait >= 0 && x.portrait < static_cast<int>(hero_modes::heroes) && drawn[x.portrait])
            continue;
        switch (x.kind) {
        case hr::Piece::fill: p.rect(x.u0, x.v0, x.u1, x.v1, c, a * (x.role == hr::Role::backing ? p.c.backingAlpha : 1.f)); break;
        case hr::Piece::outline: {
            const float w = .0007f;
            p.bar(x.u0, x.v0, x.u1, x.v0, w, c, a);
            p.bar(x.u0, x.v1, x.u1, x.v1, w, c, a);
            p.bar(x.u0, x.v0, x.u0, x.v1, w, c, a);
            p.bar(x.u1, x.v0, x.u1, x.v1, w, c, a);
            break;
        }
        case hr::Piece::disc: {
            const float r = (x.u1 - x.u0) / 2;
            p.ring((x.u0 + x.u1) / 2, (x.v0 + x.v1) / 2, r, r, 0, r, c, a * .9f);
            p.ring((x.u0 + x.u1) / 2, (x.v0 + x.v1) / 2, r, r, r * .86f, r, p.c.frame, a);
            break;
        }
        case hr::Piece::text: p.text(x.words, x.u0, x.v0, x.height, c); break;
        }
    }
    // 0.0.73 test 4 SETTINGS: the hero's setting rows (the old POWERS list's), the menu's own rows.
    if (settingsRows)
        content(p, heroSettingsLayout(st.hero, st.settingsPage), view);
}

void appendWristHud(std::vector<Vertex>& out, const View& view, Vec3 viewer, float pixelAngle) {
    const float opacity = std::isfinite(view.opacity) ? std::clamp(view.opacity, 0.f, 1.f) : 0.f;
    if (opacity < .002f || !finite(view.emitter) || !finite(viewer))
        return;
    const size_t from = out.size();
    const Frame f = place(view, viewer);
    const float s = std::isfinite(view.scale) ? std::clamp(view.scale, .5f, 2.f) : 1.f;
    const float distance = std::max(length(f.centre - viewer), .1f);
    const float pa = std::isfinite(pixelAngle) && pixelAngle > 0 ? pixelAngle : .001f;
    const Palette colors = palette(view.theme, view.textColor);
    Painter painter{out, f, s, pa * distance, cross(f.right, f.up), colors};
    painter.pixel = view.theme == raimiTheme;
    painter.seconds = view.seconds;
    const Painter& p = painter;
    light(p, view.emitter, view.seconds);
    const int page = view.page >= 0 && view.page < pages ? view.page : 0;
    const float top = panelTop(page);
    if (p.pixel)
        raimiHalo(p, top);
    backing(p, top);
    if (p.pixel)
        spiderMark(p, 0, (top - halfH) / 2 - .01f, .085f);
    const bool tilesHere = view.heroTiles && page == powersPage; // 0.0.73 test 4: never the old list
    if (view.roster.screen != holo_roster::Screen::closed || tilesHere) {
        // 0.0.73 test 3 HOLO ROSTER (fix 28): the heroes in place of the page, clipped to its inner area.
        const size_t clipFrom = out.size();
        holoRoster(p, view);
        clipBand(out, clipFrom, f, s, viewBottom, viewTop);
    } else if (page == 0) {
        figure(p, -.051f, -.003f);
        readouts(p, view.data);
        minimap(p, .034f, -.076f, view.heading);
    } else {
        // Update 17: the page's content, scrolled and clipped to the inner
        // area; the scrollbar beside it. The strip and frame stay put.
        const float k = textFactor(view.textScale);
        const float scroll = clampScroll(page, view.scroll, k);
        const size_t clipFrom = out.size();
        Painter scrolled = p;
        scrolled.dv = scroll;
        content(scrolled, layoutOf(page, k), view);
        if (page == musicPage)
            songList(scrolled, layoutOf(page, k), view);
        clipBand(out, clipFrom, f, s, viewBottom, viewTop);
        scrollbar(p, page, scroll, k, view.pointing && onScrollbar(page, view.pointU, view.pointV, k));
    }
    strip(p, view, page);
    frame(p, top);
    if (view.pointing)
        pointerMark(p, view);
    for (size_t i = from; i < out.size(); ++i)
        out[i].alpha *= opacity;
}

void appendVisualizer(std::vector<Vertex>& out, const View& view, Vec3 viewer, float pixelAngle, int kind, float u0,
                      float v0, float u1, float v1) {
    if (!finite(view.emitter) || !finite(viewer))
        return;
    const Frame f = place(view, viewer);
    const float s = std::isfinite(view.scale) ? std::clamp(view.scale, .5f, 2.f) : 1.f;
    const float distance = std::max(length(f.centre - viewer), .1f);
    const float pa = std::isfinite(pixelAngle) && pixelAngle > 0 ? pixelAngle : .001f;
    const Palette colors = palette(view.theme, view.textColor);
    const Painter p{out, f, s, pa * distance, cross(f.right, f.up), colors};
    visualize(p, std::clamp(kind, 0, music::visualizers - 1), u0, v0, u1, v1, view.music.spectrum, view.seconds,
              view.theme == 4);
}

bool sane(const std::vector<Vertex>& out, size_t from, Vec3 near, float radius) {
    if (from > out.size() || out.size() - from > maxVertices)
        return false;
    for (size_t i = from; i < out.size(); ++i) {
        const auto& v = out[i];
        if (!finite(v.position) || !finite(v.color) || !std::isfinite(v.alpha) || length(v.position - near) > radius)
            return false;
    }
    return true;
}
} // namespace spidy::wrist_hud
