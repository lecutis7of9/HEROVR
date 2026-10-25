// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
// Update 36: the health and focus ring (health_ring.hpp).
#include "spidy/health_ring.hpp"
#include "spidy/calibration_art.hpp"
#include "spidy/pixel_font.hpp"
#include "spidy/soft_shapes.hpp"
#include <limits>
#include <algorithm>
#include <cmath>
#include <functional>

namespace spidy::health_ring {
namespace art = calibration_art;
namespace {
constexpr float pi = 3.14159265f;
// Linear RGB.
constexpr Vec3 black{0, 0, 0}, glass{.012f, .013f, .02f}, red{.78f, .02f, .02f}, redLit{1, .2f, .14f},
    blue{.025f, .09f, .8f}, blueLit{.28f, .45f, 1}, bright{.75f, .9f, 1}, alarm{1, .16f, .1f},
    discDeep{.3f, .005f, .005f}, discLit{.56f, .02f, .015f};
struct Flat {
    float x, y;
};
struct Ring {
    Vec3 centre, right, up;
    float opacity;
    art::Uv white;
    Vec3 at(Flat p) const {
        return centre + right * p.x + up * p.y;
    }
    Vertex vert(Flat p, Vec3 color, float alpha) const {
        return {at(p), color, alpha * opacity, {white.u, white.v, 0, 0}};
    }
    // A band from angle t0 to t1 between inner(t) and outer(t), coloured per
    // edge and angle.
    void band(std::vector<Vertex>& out, float t0, float t1, int segments, const std::function<Flat(float)>& inner,
              const std::function<Flat(float)>& outer, const std::function<Vec3(float, bool)>& color,
              float innerAlpha, float outerAlpha) const {
        const int n = std::max(1, static_cast<int>(std::ceil(std::abs(t1 - t0) / (2 * pi) * segments)));
        for (int i = 0; i < n; ++i) {
            const float a = t0 + (t1 - t0) * static_cast<float>(i) / static_cast<float>(n);
            const float b = t0 + (t1 - t0) * static_cast<float>(i + 1) / static_cast<float>(n);
            const Vertex p = vert(inner(a), color(a, true), innerAlpha), q = vert(outer(a), color(a, false), outerAlpha),
                         r = vert(outer(b), color(b, false), outerAlpha), s = vert(inner(b), color(b, true), innerAlpha);
            out.insert(out.end(), {p, q, r, p, r, s});
        }
    }
    void disc(std::vector<Vertex>& out, Flat c, float radius, Vec3 middle, Vec3 edge, float alpha,
              int segments) const {
        for (int i = 0; i < segments; ++i) {
            const float a = 2 * pi * static_cast<float>(i) / static_cast<float>(segments);
            const float b = 2 * pi * static_cast<float>(i + 1) / static_cast<float>(segments);
            out.push_back(vert(c, middle, alpha));
            out.push_back(vert({c.x + radius * std::cos(a), c.y + radius * std::sin(a)}, edge, alpha));
            out.push_back(vert({c.x + radius * std::cos(b), c.y + radius * std::sin(b)}, edge, alpha));
        }
    }
    void rect(std::vector<Vertex>& out, Flat a, Flat b, Vec3 color, float alpha) const {
        const Vertex p = vert(a, color, alpha), q = vert({b.x, a.y}, color, alpha), r = vert(b, color, alpha),
                     s = vert({a.x, b.y}, color, alpha);
        out.insert(out.end(), {p, q, r, p, r, s});
    }
};
Vec3 mix(Vec3 a, Vec3 b, float t) {
    return a + (b - a) * t;
}
Flat circle(Flat c, float r, float t) {
    return {c.x + r * std::cos(t), c.y + r * std::sin(t)};
}
// Chrome lit from the upper left: bright toward 135 degrees, dark opposite.
Vec3 chrome(float t, float lift) {
    const float k = .5f + .5f * std::cos(t - .75f * pi);
    const float l = std::clamp(.07f + .75f * k * k + lift, 0.f, 1.f);
    return {l, l * 1.01f, l * 1.04f};
}
float unit(float v) {
    return std::isfinite(v) ? std::clamp(v, 0.f, 1.f) : 0.f;
}
} // namespace

float healthShare(float hitPoints, float maxHitPoints) {
    if (!std::isfinite(hitPoints) || !std::isfinite(maxHitPoints) || !(maxHitPoints > 0) || maxHitPoints > 1e6f ||
        hitPoints < 0 || hitPoints > maxHitPoints * 1.001f)
        return std::numeric_limits<float>::quiet_NaN();
    return std::min(hitPoints / maxHitPoints, 1.f);
}

namespace {
void draw(std::vector<Vertex>& out, const SlowMotionView& slow, float health, Vec3 c, Vec3 right, Vec3 up,
          float opacity, float distance, float pixelAngle, float seconds);
}

void append(std::vector<Vertex>& out, const SlowMotionView& slow, float health, Pose hand, Vec3 viewer,
            float pixelAngle, float seconds) {
    const float opacity = unit(slow.meter);
    if (opacity <= 0 || !finite(hand.position) || !finite(viewer) || !std::isfinite(pixelAngle) || pixelAngle <= 0)
        return;
    const Vec3 c = hand.position + hand.orientation.rotate(fromHand);
    const float distance = length(viewer - c);
    if (!finite(c) || !(distance > .08f && distance < 3))
        return;
    // Level and upright across the line of sight, as the meter was.
    const Vec3 view = (viewer - c) / distance;
    Vec3 right = cross(Vec3{0, 1, 0}, view);
    right = length(right) > .1f ? normalized(right) : soft::perpendicular(view);
    const Vec3 up = cross(view, right);
    draw(out, slow, health, c, right, up, opacity, distance, pixelAngle, seconds);
}

Placed placed(Pose hand, const Place& place, Vec3 viewer) {
    Placed p;
    if (!finite(hand.position) || !finite(viewer))
        return p;
    const float along = std::isfinite(place.offsetMm)
                            ? std::clamp(place.offsetMm, Place::minOffset, Place::maxOffset) * 1e-3f
                            : fromHand.z;
    const float tilt = (std::isfinite(place.tiltDegrees)
                            ? std::clamp(place.tiltDegrees, -Place::maxTilt, Place::maxTilt)
                            : 0.f) * pi / 180;
    // In the glove's frame: flush on the wrist's underside (the palm's side, -y) at the web
    // shooter, `along` toward the forearm (+z); its top toward the fingers (-z). TILT turns
    // it about the wrist's width (x), its top lifting toward the palm.
    const Vec3 normal{0, -std::cos(tilt), std::sin(tilt)}, top{0, -std::sin(tilt), -std::cos(tilt)};
    p.centre = hand.position + hand.orientation.rotate(Vec3{0, fromHand.y + Place::skin, along});
    p.normal = normalized(hand.orientation.rotate(normal));
    p.up = normalized(hand.orientation.rotate(top));
    p.right = cross(p.up, p.normal);
    const Vec3 toViewer = viewer - p.centre;
    const float distance = length(toViewer);
    if (!finite(p.centre) || !finite(p.normal) || !(distance > .08f && distance < 3))
        return p;
    p.distance = distance;
    p.facing = dot(p.normal, toViewer / distance);
    // Fades out as the wrist turns away: whole when it faces the eyes within ~55 degrees,
    // gone past ~80 (seen edge-on, or from the back).
    p.fade = std::clamp((p.facing - Place::fadeGone) / (Place::fadeWhole - Place::fadeGone), 0.f, 1.f);
    p.valid = finite(p.right) && finite(p.up);
    return p;
}

void append(std::vector<Vertex>& out, const SlowMotionView& slow, float health, Pose hand, const Place& place,
            Vec3 viewer, float pixelAngle, float seconds) {
    if (!place.flat) {
        append(out, slow, health, hand, viewer, pixelAngle, seconds);
        return;
    }
    const float opacity = unit(slow.meter);
    if (opacity <= 0 || !std::isfinite(pixelAngle) || pixelAngle <= 0)
        return;
    const Placed p = placed(hand, place, viewer);
    if (!p.valid || p.fade <= 0)
        return;
    draw(out, slow, health, p.centre, p.right, p.up, opacity * p.fade, p.distance, pixelAngle, seconds);
}

namespace {
void draw(std::vector<Vertex>& out, const SlowMotionView& slow, float health, Vec3 c, Vec3 right, Vec3 up,
          float opacity, float distance, float pixelAngle, float seconds) {
    const Ring g{c, right, up, opacity, art::uv(art::white, .5f, .5f)};
    const float px = pixelAngle * distance, t = std::isfinite(seconds) ? seconds : 0.f;
    const bool known = std::isfinite(health) && health >= 0 && health <= 1.001f;
    const float hp = known ? std::min(health, 1.f) : 0.f, focus = unit(slow.focus), blend = unit(slow.blend),
                warning = unit(slow.warning);
    constexpr Flat o{0, 0}, d{-discShift, 0};
    constexpr float rimIn = .0232f, gap = .0006f, discIn = discRadius - .0016f;
    const float ro = outerRadius, edge = std::max(1.2f * px, .0002f);
    constexpr int segments = 96;
    const auto at = [](Flat c0, float r) { return [c0, r](float a) { return circle(c0, r, a); }; };
    const auto flat = [](Vec3 col) { return [col](float, bool) { return col; }; };
    // A soft shadow, then the rim with its outer edge feathered.
    g.band(out, 0, 2 * pi, segments, at(o, 0), at(o, ro + 2.5f * px), flat(black), .5f, .5f);
    g.band(out, 0, 2 * pi, segments, at(o, ro + 2.5f * px), at(o, ro + 2.5f * px + 2 * edge), flat(black), .5f, 0);
    g.band(out, 0, 2 * pi, segments, at(o, ro), at(o, ro + edge), [](float a, bool) { return chrome(a, 0); }, 1, 0);
    g.band(out, 0, 2 * pi, segments, at(o, rimIn), at(o, ro),
           [](float a, bool inner) { return chrome(a, inner ? -.08f : .1f); }, 1, 1);
    // The crescents: the outer edge the rim's inside, the inner the disc's
    // rim, so about 2 mm across at the left tip and 16 mm at the right.
    const float ci = rimIn - gap, cd = discRadius + gap;
    const auto inner = at(d, cd), outer = at(o, ci);
    const auto crescent = [&](float t0, float t1, Vec3 base, Vec3 lit, float alpha) {
        g.band(out, t0, t1, segments, inner, outer,
               [&](float a, bool in) {
                   const float gloss = .5f + .5f * std::sin(a); // the upper side catches the light
                   return in ? base * (.62f + .1f * gloss) : mix(base, lit, .35f + .4f * gloss);
               },
               alpha, alpha);
    };
    // Health above: empty glass, then the fill held at the thin left tip,
    // reaching right as far as its share: it drains from the wide right end
    // toward the left (angle pi is the left tip, 0 the right end), smoothly,
    // the edge where the share puts it. Low health beats.
    crescent(0, pi, mix(glass, red, .1f), mix(glass, red, .18f), .92f);
    if (known && hp > .002f) {
        const float beat = hp < .3f ? .5f + .5f * std::sin(t * 2 * pi * 1.6f) : 0.f;
        crescent(pi * (1 - hp), pi, mix(red, redLit, .25f * beat), mix(redLit, Vec3{1, .7f, .6f}, .3f * beat), 1);
    }
    // Focus below, held at the left tip as well (angle -pi), reaching
    // round under toward the right end: it drains right to left too.
    {
        const float pulse = slow.active ? .5f + .5f * std::sin(t * 6.f) : 0.f;
        const float flash = warning * (.55f + .45f * std::cos(t * 28.f));
        crescent(-pi, 0, mix(mix(glass, blue, .1f), alarm, warning * .5f),
                 mix(mix(glass, blue, .18f), alarm, warning * .5f), .92f);
        if (focus > .002f) {
            Vec3 base = mix(blue, bright, blend * (.15f + .25f * pulse)),
                 lit = mix(blueLit, bright, blend * (.3f + .3f * pulse));
            crescent(-pi, -pi * (1 - focus), mix(base, alarm, flash), mix(lit, alarm, flash), 1);
        }
    }
    // The split between them, a chrome sliver each side.
    g.rect(out, {-ci, -.00045f}, {-discShift - cd + gap * .5f, .00045f}, chrome(pi, .05f), 1);
    g.rect(out, {-discShift + cd - gap * .5f, -.00045f}, {ci, .00045f}, chrome(0, .05f), 1);
    // The disc's rim (lit from below, so it reads as set into the face), the
    // red disc, Spidy's spider.
    g.band(out, 0, 2 * pi, segments, at(d, discIn), at(d, discRadius),
           [](float a, bool inside) { return chrome(a + pi, inside ? -.05f : .05f); }, 1, 1);
    g.disc(out, {d.x - .002f, d.y + .003f}, discIn * .25f, discLit, discLit, 1, 32);
    g.band(out, 0, 2 * pi, segments, at({d.x - .002f, d.y + .003f}, discIn * .25f), at(d, discIn),
           [](float a, bool inside) { return inside ? discLit : mix(discDeep, discLit, .35f + .35f * std::sin(a)); }, 1,
           1);
    {
        const float s = discIn * .8f;
        const auto uv = [](float u, float v) { return art::uv(art::emblem, u, v); };
        const Vec3 tint{.55f, .08f, .06f};
        const auto e = [&](Flat p, float u, float v) {
            const auto q = uv(u, v);
            return Vertex{g.at(p), tint, .95f * opacity, {q.u, q.v, 0, 0}};
        };
        const Vertex p = e({d.x - s, d.y - s}, 0, 1), q = e({d.x + s, d.y - s}, 1, 1), r = e({d.x + s, d.y + s}, 1, 0),
                     w = e({d.x - s, d.y + s}, 0, 0);
        out.insert(out.end(), {p, q, r, p, r, w});
    }
    // Unread health says so, in its crescent's wide part.
    if (!known) {
        PixelTextStyle style;
        style.height = .0042f;
        style.top = {1, .55f, .5f};
        style.bottom = {.85f, .3f, .25f};
        style.alpha = opacity;
        style.outline = {0, 0, .0006f};
        style.outlineAlpha = opacity;
        style.slant = .2f;
        const float w = pixelTextWidth("N/A", style) + style.slant * style.height;
        const auto first = out.size();
        appendPixelText(out, "N/A", g.at({(ci + cd - discShift) / 2 - w / 2, .0104f - style.height / 2}), right, up, style);
        for (auto i = first; i < out.size(); ++i)
            out[i].shape = {g.white.u, g.white.v, 0, 0};
    }
}
} // namespace
} // namespace spidy::health_ring

namespace spidy::health_ring {
float Eased::update(float target, float seconds, float tau) {
    if (!std::isfinite(target) || !std::isfinite(shown) || !std::isfinite(seconds) || seconds < 0 || !(tau > 0) ||
        std::abs(target - shown) < .001f)
        return shown = target;
    shown += (target - shown) * (1 - std::exp(-std::min(seconds, 1.f) / tau));
    return shown;
}
} // namespace spidy::health_ring
