// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
// Update 36: the calibration panel drawn from the art atlas (calibration_panel.hpp).
#include "spidy/calibration_panel.hpp"
#include "spidy/calibration_art.hpp"
#include "spidy/pixel_font.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace spidy::calibration_panel {
namespace art = calibration_art;
using body_calibration::Phase;
namespace {
constexpr float pi = 3.14159265f;
// Linear RGB.
constexpr Vec3 black{0, 0, 0}, frame{.035f, .035f, .04f}, yellow{1, .78f, .1f}, orange{1, .38f, .02f},
    gold{1, .66f, .12f}, goldDeep{.85f, .32f, .02f}, pale{.86f, .88f, .94f}, track{.012f, .012f, .016f},
    rim{.16f, .15f, .14f};
struct Panel {
    Vec3 centre, right, up;
    Vec3 at(float x, float y) const {
        return centre + right * x + up * y;
    }
    // A quad of atlas rect `r` over panel x0..x1, y0..y1 (y up), each corner
    // its colour: bottom-left, bottom-right, top-right, top-left.
    void quad(std::vector<Vertex>& v, float x0, float y0, float x1, float y1, const art::Rect& r,
              const Vec3 (&c)[4], const float (&a)[4]) const {
        const auto vert = [&](float x, float y, float s, float t, int i) {
            const auto u = art::uv(r, s, t);
            return Vertex{at(x, y), c[i], a[i], {u.u, u.v, 0, 0}};
        };
        const Vertex p = vert(x0, y0, 0, 1, 0), q = vert(x1, y0, 1, 1, 1), s = vert(x1, y1, 1, 0, 2),
                     t = vert(x0, y1, 0, 0, 3);
        v.insert(v.end(), {p, q, s, p, s, t});
    }
    void quad(std::vector<Vertex>& v, float x0, float y0, float x1, float y1, const art::Rect& r, Vec3 color,
              float alpha = 1) const {
        quad(v, x0, y0, x1, y1, r, {color, color, color, color}, {alpha, alpha, alpha, alpha});
    }
    void fill(std::vector<Vertex>& v, float x0, float y0, float x1, float y1, Vec3 color, float alpha = 1) const {
        quad(v, x0, y0, x1, y1, art::white, color, alpha);
    }
    // Pixel text centred on x at baseline y, shrunk to `width`; italic.
    void text(std::vector<Vertex>& v, const char* s, float x, float y, float height, Vec3 top, Vec3 bottom,
              float width, float alpha = 1) const {
        PixelTextStyle style;
        style.top = top;
        style.bottom = bottom;
        style.alpha = alpha;
        style.outline = {0, 0, .003f};
        style.outlineAlpha = 1;
        style.xScale = .9f;
        style.slant = .2f;
        style.height = height;
        const float w = pixelTextWidth(s, style);
        if (w > width && w > 0)
            style.height *= width / w;
        const float actual = pixelTextWidth(s, style) + style.slant * style.height;
        const auto first = v.size();
        appendPixelText(v, s, at(x - actual / 2, y), right, up, style);
        const auto white = art::uv(art::white, .5f, .5f);
        for (auto i = first; i < v.size(); ++i)
            v[i].shape = {white.u, white.v, 0, 0};
    }
};
float pulse(float clock, float rate = 1.3f) {
    return .5f + .5f * std::sin(2 * pi * rate * std::fmod(clock, 1000.f));
}
} // namespace

float glow(const body_calibration::View& view, float clock) {
    if (view.phase == Phase::idle)
        return 0;
    if (view.phase == Phase::done)
        return 1;
    const auto s = body_calibration::steps(view);
    const int met = s.done[0] + s.done[1] + s.done[2];
    const float beat = std::isfinite(clock) ? pulse(clock) : 0.f;
    if (view.phase == Phase::holding)
        return .85f + .15f * beat;
    return std::clamp(.3f + .15f * static_cast<float>(met) + .1f * beat, 0.f, 1.f);
}

void append(std::vector<Vertex>& out, const body_calibration::View& view, float clock) {
    if (view.phase == Phase::idle || !finite(view.panel.position) || !std::isfinite(clock))
        return;
    const auto& q = view.panel.orientation;
    const Panel p{view.panel.position, q.rotate({1, 0, 0}), q.rotate({0, 1, 0})};
    if (!finite(p.right) || !finite(p.up))
        return;
    const float w = body_calibration::panelWidth / 2, h = body_calibration::panelHeight / 2, edge = .007f;
    const bool done = view.phase == Phase::done, holding = view.phase == Phase::holding;
    const float beat = pulse(clock);
    // The frame, the street (brighter behind the figure, darker under the
    // text), the title's band darkened.
    p.fill(out, -w - edge, -h - edge, w + edge, h + edge, frame);
    p.fill(out, -w - edge * .45f, -h - edge * .45f, w + edge * .45f, h + edge * .45f, {.10f, .09f, .08f});
    p.fill(out, -w, -h, w, h, black);
    {
        constexpr int columns = 4;
        for (int i = 0; i < columns; ++i) {
            const float s0 = static_cast<float>(i) / columns, s1 = static_cast<float>(i + 1) / columns;
            const auto shade = [](float s) {
                const float l = .7f - .38f * std::clamp((s - .25f) / .75f, 0.f, 1.f);
                return Vec3{l, l, l};
            };
            const art::Rect strip{art::city.x + static_cast<int>(s0 * art::city.w), art::city.y,
                                  static_cast<int>((s1 - s0) * art::city.w), art::city.h};
            p.quad(out, -w + 2 * w * s0, -h, -w + 2 * w * s1, h, strip,
                   {shade(s0), shade(s1), shade(s1), shade(s0)}, {1, 1, 1, 1});
        }
    }
    p.quad(out, -w, h - .15f, w, h, art::white, {black, black, black, black}, {0, 0, .8f, .8f});
    // The title, 8:1.
    {
        const float th = .1f, tw = th * 8, ty = h - .063f;
        p.quad(out, -tw / 2, ty - th / 2, tw / 2, ty + th / 2, done ? art::titleCalibrated : art::titleCalibration,
               {1, 1, 1});
    }
    // The hero (the player's picture, its aspect kept by the atlas), over
    // his gold halo, which grows with the steps.
    {
        const float size = .52f, cx = -.335f, cy = -.055f;
        const float x0 = cx - size / 2, x1 = cx + size / 2, y0 = cy - size / 2, y1 = cy + size / 2;
        p.quad(out, x0, y0, x1, y1, art::figureGlow, {1, 1, 1}, glow(view, clock));
        p.quad(out, x0, y0, x1, y1, art::figure, {1, 1, 1});
    }
    // The right column: steps or the result, the hint, the bar, skip.
    const float cx = .245f, column = .6f;
    p.fill(out, -.07f, -h, w, h - .15f, black, .28f);
    if (done) {
        char line[64];
        std::snprintf(line, sizeof line, "EYE HEIGHT %.2f M", view.result.eyeHeight);
        p.text(out, line, cx, .085f, .036f, yellow, orange, column);
        std::snprintf(line, sizeof line, "ARM LENGTH %.2f M", view.result.armLength);
        p.text(out, line, cx, .005f, .036f, yellow, orange, column);
        p.text(out, "SPIDER-MAN NOW HAS YOUR SIZE", cx, -.085f, .026f, pale, pale * .8f, column);
    } else {
        const auto s = body_calibration::steps(view);
        const char* lines[3] = {"STAND TALL, LOOK AHEAD", "ARMS OUT TO THE SIDES", "HOLD BOTH TRIGGERS"};
        const float ys[3] = {.085f, .005f, -.075f};
        for (int i = 0; i < 3; ++i) {
            if (i == s.active) {
                const float glow = .8f + .2f * beat;
                p.text(out, lines[i], cx, ys[i], .036f * (1 + .04f * beat), Vec3{1, .93f, .45f} * glow,
                       orange * glow, column);
            } else if (s.done[i]) {
                p.text(out, lines[i], cx, ys[i], .034f, yellow * .32f, orange * .32f, column, .9f);
            } else {
                p.text(out, lines[i], cx, ys[i], .034f, yellow * .8f, orange * .8f, column);
            }
        }
        const char* hint = holding ? "HOLD STILL" : body_calibration::hintText(view.hint);
        p.text(out, hint, cx, -.145f, .022f, holding ? Vec3{1, .93f, .45f} : pale,
               holding ? yellow : pale * .8f, column);
    }
    {
        const float x0 = -.035f, x1 = .525f, y0 = -.205f, y1 = -.19f, r = .0025f;
        p.fill(out, x0 - r, y0 - r, x1 + r, y1 + r, rim);
        p.fill(out, x0, y0, x1, y1, track, .95f);
        const float share = done ? 1.f : std::clamp(std::isfinite(view.progress) ? view.progress : 0.f, 0.f, 1.f);
        if (share > 0) {
            const float xe = x0 + (x1 - x0) * share;
            p.quad(out, x0, y0, xe, y1, art::white, {goldDeep, goldDeep, gold, gold}, {1, 1, 1, 1});
            p.quad(out, x0, y1 - (y1 - y0) * .35f, xe, y1, art::white, Vec3{1, .95f, .7f}, .45f);
        }
    }
    if (done)
        p.text(out, "REDO IT IN SETTINGS, SPIDY VR", cx, -.258f, .019f, pale * .7f, pale * .55f, column);
    else
        p.text(out, "PRESS B TO SKIP", cx, -.258f, .02f, yellow, orange, column);
}
} // namespace spidy::calibration_panel
