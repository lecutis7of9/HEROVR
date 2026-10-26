#include "spidy/web_visual.hpp"
#include "spidy/gadget_wheel.hpp"
#include "spidy/overlay_text.hpp"
#include "spidy/soft_shapes.hpp"
#include "spidy/tech_marks.hpp"
#include "spidy/web_shading.hpp"
#include <algorithm>
#include <string>
#include <utility>

namespace spidy {
namespace {
using soft::perpendicular, soft::quad, soft::softArc, soft::softBar, soft::softTriangle;
constexpr float pi = 3.14159265f;
Vec3 mix(Vec3 a, Vec3 b, float t) {
    return a + (b - a) * t;
}
float halfWidth(const WebLook& look, Vec3 viewer, Vec3 p, float pixelAngle, float radius) {
    return std::max(radius, .5f * look.minPixels * pixelAngle * length(viewer - p));
}
// Deterministic 0..1 value per strand, so a splat keeps its shape.
float hash(float seed, int i) {
    const float s = std::sin(seed * 12.9898f + i * 78.233f) * 43758.547f;
    return s - std::floor(s);
}
void glob(std::vector<Vertex>& out, Vec3 center, Vec3 viewer, float radius, Vec3 color) {
    const Vec3 v = normalized(viewer - center);
    const Vec3 a = perpendicular(v), b = cross(v, a);
    constexpr int sides = 8;
    for (int i = 0; i < sides; ++i) {
        const float t0 = 2 * pi * i / sides, t1 = 2 * pi * (i + 1) / sides;
        const Vec3 p0 = center + (a * std::cos(t0) + b * std::sin(t0)) * radius;
        const Vec3 p1 = center + (a * std::cos(t1) + b * std::sin(t1)) * radius;
        out.insert(out.end(), {{center, color}, {p0, color * .8f}, {p1, color * .8f}});
    }
}
// Each hand's aim markers in its own colour (aimColors, Update 16: Spider-Man
// red for the left hand, blue for the right). A miss is the same colour dimmed.
constexpr const Vec3* handColors = aimColors;
constexpr Vec3 shadowColor{.01f, .012f, .018f};
// Tapered camera-facing strand for the impact splat.
void strand(std::vector<Vertex>& out, Vec3 from, Vec3 to, Vec3 viewer, float pixelAngle, const WebLook& look,
            float rootRadius) {
    constexpr int pieces = 4;
    for (int i = 0; i < pieces; ++i) {
        const float u0 = float(i) / pieces, u1 = float(i + 1) / pieces;
        const Vec3 p0 = mix(from, to, u0), p1 = mix(from, to, u1);
        const Vec3 side0 = normalized(cross(to - from, viewer - p0)), side1 = normalized(cross(to - from, viewer - p1));
        const float w0 = halfWidth(look, viewer, p0, pixelAngle, rootRadius * (1 - .7f * u0));
        const float w1 = halfWidth(look, viewer, p1, pixelAngle, rootRadius * (1 - .7f * u1));
        const Vec3 c0 = mix(look.core, look.edge, .25f + .3f * u0), c1 = mix(look.core, look.edge, .25f + .3f * u1);
        quad(out, {p0 - side0 * w0, c0 * .75f}, {p1 - side1 * w1, c1 * .75f}, {p1 + side1 * w1, c1},
             {p0 + side0 * w0, c0});
    }
}
// The combat marks are printed, as the graphics of Spidy's videos are (their
// style system's plates, keylines and timing): flat colour, every shape keyed
// in ink, a hard drop to one side where a shadow would be, and nothing soft
// but an edge's last pixel. They move on twos: a mark is a pose held a
// twelfth of a second, then the next; nothing eases, slides or fades.
namespace plate {
// The style's colour tokens in linear RGB.
constexpr Vec3 ink{.0033f, .0030f, .0070f};   // #0b0a14: every keyline
constexpr Vec3 paper{.9301f, .9131f, .8550f}; // #f7f5ee
constexpr Vec3 red{.7682f, .0194f, .0284f};   // #e3262f, the brand's
constexpr Vec3 yellow{1.f, .7529f, .0742f};   // #ffe14d: a burst, a finisher
constexpr Vec3 cyan{.0060f, .6724f, 1.f};     // #12d6ff: the misprint's left ghost
constexpr Vec3 magenta{1.f, .0273f, .2918f};  // #ff2e93: its right ghost
constexpr float pose = 1.f / 12, degree = pi / 180;
// How far the misprint's ghosts lie to either side on the poses a mark
// lands, as a share of its size.
constexpr float misprint = .11f;
} // namespace plate
// How a mark lands, the style's "pop": three held poses (small and turned,
// past its size, just short of it), then at rest. `hit`: the poses on which
// its misprint is wide.
struct Landing {
    float scale = 1, turn{};
    bool hit{};
};
Landing landing(float age) {
    switch (static_cast<int>(age / plate::pose)) {
    case 0:
        return {.3f, 10 * plate::degree, true};
    case 1:
        return {1.16f, -5 * plate::degree, true};
    case 2:
        return {.95f, 2 * plate::degree, false};
    default:
        return {};
    }
}
struct Flat {
    float x{}, y{};
    Flat operator+(Flat b) const {
        return {x + b.x, y + b.y};
    }
    Flat operator-(Flat b) const {
        return {x - b.x, y - b.y};
    }
    Flat operator*(float s) const {
        return {x * s, y * s};
    }
};
// One printed mark, flat across the line of sight. Its shapes are placed in
// its own plane (x to the viewer's right, y up, metres from its centre),
// sized and turned as a whole by `place`, and laid down a plate at a time by
// `layers`.
class Print {
  public:
    Print(std::vector<Vertex>& out, Vec3 centre, Vec3 right, Vec3 up, Vec3 view, float px)
        : out_(out), centre_(centre), right_(right), up_(up), view_(view), px_(px) {
        place(1, 0);
    }
    // From here on the shapes are `scale` their size and turned by `turn`
    // (radians, anticlockwise) about a centre `moved` from the mark's.
    void place(float scale, float turn, Flat moved = {}) {
        scale_ = scale;
        across_ = right_ * std::cos(turn) + up_ * std::sin(turn);
        along_ = up_ * std::cos(turn) - right_ * std::sin(turn);
        moved_ = moved;
    }
    // Draws what `draw` draws as print lays it down. Under everything the
    // misprint's two ghosts, cyan `misprint` to the left and magenta as far
    // to the right (0: none); then the hard drop in `dropColor`, `drop` to
    // the right and down (0: none); the keyline, ink `key` wide around every
    // shape; and the shapes in their own colours.
    template <class Draw> void layers(float key, float drop, Vec3 dropColor, float misprint, Draw&& draw) {
        const float grow = std::max(key * scale_, 1.1f * px_);
        const auto layer = [&](Flat shift, const Vec3* tinted, float grown) {
            shift_ = shift * scale_;
            plate_ = tinted;
            grow_ = grown;
            draw();
        };
        // What lies under the keyline is a pixel smaller than it, so that an
        // edge both share shows none of it.
        const float under = std::max(grow - px_, 0.f);
        if (misprint > 0) {
            layer({-misprint, 0}, &plate::cyan, under);
            layer({misprint, 0}, &plate::magenta, under);
        }
        if (drop > 0)
            layer({drop, -drop}, &dropColor, under);
        layer({}, &plate::ink, grow);
        layer({}, nullptr, 0);
    }
    void disc(Flat at, float radius, Vec3 color) {
        dot(world(at), radius * scale_ + grow_, tint(color));
    }
    // A ring `half` to either side of `radius`, or its arc from angle `from`
    // to `to` (anticlockwise from the mark's right).
    void ring(Flat at, float radius, float half, Vec3 color, float from = 0, float to = 2 * pi) {
        const float r = radius * scale_, h = half * scale_ + grow_;
        const float beyond = to - from > 2 * pi - 1e-3f ? 0.f : grow_ / std::max(r, 1e-6f);
        softArc(out_, world(at), across_, along_, std::max(r - h, 0.f), r + h, from - beyond, to + beyond,
                tint(color), 1, soft(), segments(r + h));
    }
    // A stroke with round ends, as a marker draws it. `both` false: one
    // that goes on from the end of the stroke before, whose round end is its
    // start.
    void line(Flat p, Flat q, float half, Vec3 color, bool both = true) {
        const Vec3 a = world(p), b = world(q), c = tint(color);
        const float h = half * scale_ + grow_;
        if (length(b - a) > 1e-6f) {
            const Vec3 side = normalized(cross(b - a, view_));
            const float across[4] = {-h - soft(), -h, h, h + soft()}, alpha[4] = {0, 1, 1, 0};
            for (int i = 0; i < 3; ++i)
                quad(out_, {a + side * across[i], c, alpha[i]}, {b + side * across[i], c, alpha[i]},
                     {b + side * across[i + 1], c, alpha[i + 1]}, {a + side * across[i + 1], c, alpha[i + 1]});
        }
        if (both)
            dot(a, h, c);
        dot(b, h, c);
    }
    void triangle(Flat a, Flat b, Flat c, Vec3 color) {
        softTriangle(out_, {world(a), world(b), world(c)}, view_, tint(color), 1, grow_, soft());
    }
    // A burst: a star of `points` spikes from radius `inner` out to `outer`,
    // each its own length and lean by `seed`.
    void star(Flat at, float inner, float outer, int points, float seed, Vec3 color) {
        disc(at, inner, color);
        const float step = 2 * pi / static_cast<float>(points);
        const auto on = [&](float angle, float radius) {
            return Flat{at.x + std::cos(angle) * radius, at.y + std::sin(angle) * radius};
        };
        for (int i = 0; i < points; ++i) {
            const float middle = seed + step * (static_cast<float>(i) + .3f * (hash(seed, i) - .5f));
            const float tip = inner + (outer - inner) * (.55f + .45f * hash(seed, i + 23));
            triangle(on(middle - step * .56f, inner * .96f),
                     on(middle + step * .3f * (hash(seed, i + 47) - .5f), tip),
                     on(middle + step * .56f, inner * .96f), color);
        }
    }
    // Lettering in the overlay's stroke font, leaning as italics do, from
    // the left end of its baseline.
    static constexpr float lean = .18f;
    void text(std::string_view words, Flat origin, float height, float weight, Vec3 color) {
        TextStyle style;
        style.height = height * scale_;
        style.weight = weight + 2 * grow_ / std::max(style.height, 1e-6f);
        style.color = tint(color);
        style.round = true;
        style.soft = soft();
        appendText(out_, words, world(origin), across_, along_ + across_ * lean, style);
    }

  private:
    Vec3 world(Flat at) const {
        return centre_ + across_ * (at.x * scale_) + along_ * (at.y * scale_) + right_ * (moved_.x + shift_.x) +
               up_ * (moved_.y + shift_.y);
    }
    Vec3 tint(Vec3 color) const {
        return plate_ ? *plate_ : color;
    }
    // An edge's last pixel.
    float soft() const {
        return .9f * px_;
    }
    // Enough sides for a circle to stay round to a third of a pixel.
    int segments(float radius) const {
        return std::clamp(static_cast<int>(3.4f * std::sqrt(radius / px_)), 8, 48);
    }
    // A disc with a soft rim.
    void dot(Vec3 centre, float radius, Vec3 color) {
        const int sides = segments(radius);
        const float rim = radius + soft();
        Vec3 from = across_;
        for (int i = 1; i <= sides; ++i) {
            const float angle = 2 * pi * static_cast<float>(i) / static_cast<float>(sides);
            const Vec3 to = across_ * std::cos(angle) + along_ * std::sin(angle);
            const Vertex p{centre + from * radius, color}, q{centre + to * radius, color};
            out_.insert(out_.end(), {{centre, color}, p, q});
            quad(out_, p, q, {centre + to * rim, color, 0}, {centre + from * rim, color, 0});
            from = to;
        }
    }
    std::vector<Vertex>& out_;
    Vec3 centre_, right_, up_, view_, across_{}, along_{};
    float px_, scale_ = 1, grow_{};
    Flat moved_{}, shift_{};
    const Vec3* plate_{};
};
} // namespace

// The impact splat at a landed web's end: a central glob with strands
// spread across the surface, in look's core and edge.
static void appendSplat(std::vector<Vertex>& out, const WebLine& web, Vec3 viewer, float pixelAngle,
                        const WebLook& look) {
    // The surface normal is unknown here; face the strands back along the web.
    const Vec3 normal = normalized(web.start - web.end);
    const Vec3 a = perpendicular(normal), b = cross(normal, a);
    const Vec3 center = web.end + normal * .004f;
    // Update 15: the splat spreads from the tip as it lands.
    const float impact = std::isfinite(web.impact) ? std::clamp(web.impact, 0.f, 1.f) : 1.f;
    const float spread = .3f + .7f * impact;
    constexpr int strands = 7;
    for (int i = 0; i < strands; ++i) {
        const float angle = 2 * pi * (i + .35f * hash(web.seed, i)) / strands + web.seed;
        const Vec3 direction = a * std::cos(angle) + b * std::sin(angle);
        const float reach = (.09f + .13f * hash(web.seed, i + 17)) * spread;
        const Vec3 curl = (a * -std::sin(angle) + b * std::cos(angle)) * (reach * .18f * (hash(web.seed, i + 31) - .5f));
        strand(out, center, center + direction * reach + curl, viewer, pixelAngle, look, look.radius * .8f);
    }
    glob(out, center + normal * .002f, viewer,
         halfWidth(look, viewer, center, pixelAngle, look.radius * (3 + 1.5f * (1 - impact))), look.core);
}
// Update 20: a translucent disc turned to the eye, `alpha` at its centre
// fading to nothing at its rim: MESSY's and MODERN's tip and splat, where the
// 90s rope has its opaque blob.
static void softGlob(std::vector<Vertex>& out, Vec3 center, Vec3 viewer, float radius, Vec3 color, float alpha) {
    const Vec3 v = normalized(viewer - center);
    const Vec3 a = perpendicular(v), b = cross(v, a);
    constexpr int sides = 10;
    for (int i = 0; i < sides; ++i) {
        const float t0 = 2 * pi * i / sides, t1 = 2 * pi * (i + 1) / sides;
        const Vec3 e0 = a * std::cos(t0) + b * std::sin(t0), e1 = a * std::cos(t1) + b * std::sin(t1);
        const Vertex c{center, color, alpha}, m0{center + e0 * (radius * .55f), color, alpha * .7f},
            m1{center + e1 * (radius * .55f), color, alpha * .7f};
        out.insert(out.end(), {c, m0, m1});
        quad(out, m0, m1, {center + e1 * radius, color, 0}, {center + e0 * radius, color, 0});
    }
}
// Update 20: MESSY's and MODERN's splat: fine translucent threads, tapering
// and fading out over the surface, round a soft translucent centre; no blob
// or beads.
static void appendFineSplat(std::vector<Vertex>& out, const WebLine& web, Vec3 viewer, float pixelAngle,
                            const WebLook& look, Vec3 color, float alpha) {
    const Vec3 normal = normalized(web.start - web.end);
    const Vec3 a = perpendicular(normal), b = cross(normal, a);
    const Vec3 center = web.end + normal * .004f;
    const float impact = std::isfinite(web.impact) ? std::clamp(web.impact, 0.f, 1.f) : 1.f;
    const float spread = .3f + .7f * impact;
    constexpr int threads = 11;
    for (int i = 0; i < threads; ++i) {
        const float angle = 2 * pi * (i + .5f * hash(web.seed, i)) / threads + web.seed;
        const Vec3 direction = a * std::cos(angle) + b * std::sin(angle);
        const float reach = (.07f + .15f * hash(web.seed, i + 17)) * spread;
        const Vec3 bend = (a * -std::sin(angle) + b * std::cos(angle)) * (reach * .25f * (hash(web.seed, i + 31) - .5f));
        const Vec3 pts[3] = {center, center + direction * (reach * .5f) + bend * .6f, center + direction * reach + bend};
        for (int k = 0; k < 2; ++k) {
            const Vec3 side = normalized(cross(pts[k + 1] - pts[k], viewer - pts[k]));
            const float w0 = halfWidth(look, viewer, pts[k], pixelAngle, look.radius * (.32f - .12f * k));
            const float w1 = halfWidth(look, viewer, pts[k + 1], pixelAngle, look.radius * (.2f - .12f * k));
            const float a0 = alpha * (1 - .5f * k), a1 = alpha * (.5f - .5f * k);
            quad(out, {pts[k] - side * w0, color, a0}, {pts[k + 1] - side * w1, color, a1},
                 {pts[k + 1] + side * w1, color, a1}, {pts[k] + side * w0, color, a0});
        }
    }
    softGlob(out, center + normal * .002f, viewer,
             halfWidth(look, viewer, center, pixelAngle, look.radius * (1.8f + 1.f * (1 - impact))), color, alpha * .8f);
}
// The 90s rope (web_style::nineties): Updates 12-15's cream cartoon tube
// with its inked coil rings, unchanged.
// Update 24: the rope's loose fibres (the October 23 player's reference: a
// fine twisted strand with wispy fibres, frayed loops and tufts standing
// off it irregularly). A few 3D threads near the eye, each a little loop
// whose two ends sit in the rope or a wisp come loose from it; seeded by
// the web, so they hold still. A thread is a camera-facing strip floored to
// half a pixel at its true width's share of opacity, and they all fade out
// as the rope narrows on screen (seen), long before they could shimmer.
struct RopeAt {
    Vec3 centre, tangent, a, b;
    float radius, seen;
};
struct Strays {
    float perMetre = 6, loopShare = .4f;  // how many a metre near the eye, and how many are loops
    float reach = 1, width = .05f;        // size (in the rope's radii; width a half width)
    float alpha = .5f, taper = .6f;       // opacity; how much thinner the tip of a wisp is
    Vec3 color{1, 1, 1}, tip{1, 1, 1};    // root and end colours
    float drift = 0;                      // turns a second round the rope (ENERGY's arcs)
    float curl = .7f;                     // how much a wisp winds round as it goes (radians)
    float seed = 0;
};
inline constexpr int strayLimit = 64, strayPoints = 8;
// The 90s rope's plies: a turn every ninetiesTwist metres, each ply's face
// leaning by up to ninetiesLean of its normal along the rope.
inline constexpr float ninetiesTwist = .14f, ninetiesLean = .45f;
template <class Axis>
void appendStrays(std::vector<Vertex>& out, const Strays& s, Vec3 viewer, float pixelAngle, float nearEnd,
                  float seconds, Axis&& at, size_t room = static_cast<size_t>(-1)) {
    if (!(nearEnd > .05f) || !std::isfinite(nearEnd))
        return;
    // No more than `room` vertices (they are the first thing to go).
    constexpr size_t each = (strayPoints - 1) * 6;
    const int count = std::min({strayLimit, static_cast<int>(s.perMetre * nearEnd + .5f),
                                static_cast<int>(std::min(room / each, size_t{1000}))});
    for (int j = 0; j < count; ++j) {
        const float h0 = hash(s.seed + 31.7f, j), h1 = hash(s.seed + 37.3f, j), h2 = hash(s.seed + 41.9f, j),
                    h3 = hash(s.seed + 43.1f, j);
        // Thickest near the hand, where they are biggest on screen.
        const float u = (static_cast<float>(j) + h0) / static_cast<float>(count);
        const float m0 = .04f + (nearEnd - .04f) * std::pow(u, 1.7f);
        const RopeAt root = at(m0);
        if (root.seen <= .02f)
            continue;
        const float r = root.radius;
        const bool loop = h1 < s.loopShare;
        const float turn = 2 * pi * (h2 + s.drift * seconds * (h3 - .5f) * 2);
        const float way = h3 > .5f ? 1.f : -1.f;
        Vec3 pts[strayPoints];
        float width[strayPoints], alpha[strayPoints];
        for (int k = 0; k < strayPoints; ++k) {
            const float t = static_cast<float>(k) / (strayPoints - 1);
            float m, angle, lift;
            if (loop) { // both ends in the rope, bowed out between
                const float span = r * s.reach * (4 + 6 * h0);
                m = m0 + span * t;
                angle = turn + way * (.8f + .6f * h1) * t + .5f * std::sin(pi * t);
                lift = r * (.85f + s.reach * (1.1f + 2.2f * h2) * std::sin(pi * t));
            } else { // a wisp: from the rope along it, lifting away and curling
                const float stretch = r * s.reach * (4 + 8 * h0);
                m = m0 + way * stretch * .75f * t;
                angle = turn + way * ((1.6f * h1 - .8f) * t + s.curl * std::sin(2.5f * pi * t));
                lift = r * (.85f + s.reach * (.6f + 3.4f * h2) * std::pow(t, 1.4f));
            }
            const RopeAt here = at(std::max(m, 0.f));
            pts[k] = here.centre + (here.a * std::cos(angle) + here.b * std::sin(angle)) * lift;
            const float trueWidth = r * s.width * (loop ? 1 : 1 - s.taper * t);
            const float floored = std::max(trueWidth, .5f * pixelAngle * length(viewer - pts[k]));
            width[k] = floored;
            alpha[k] = s.alpha * root.seen * (trueWidth / floored) * (loop ? 1 : 1 - .45f * t);
        }
        for (int k = 0; k + 1 < strayPoints; ++k) {
            Vertex v[2][2];
            for (int e = 0; e < 2; ++e) {
                const int i = k + e;
                const Vec3 dir = pts[std::min(i + 1, strayPoints - 1)] - pts[std::max(i - 1, 0)];
                Vec3 side = cross(dir, viewer - pts[i]);
                side = length(side) > 1e-9f ? normalized(side) : perpendicular(normalized(viewer - pts[i]));
                const float t = static_cast<float>(i) / (strayPoints - 1);
                const Vec3 c = mix(s.color, s.tip, loop ? std::sin(pi * t) : t);
                v[e][0] = {pts[i] - side * width[i], c, alpha[i]};
                v[e][1] = {pts[i] + side * width[i], c, alpha[i]};
            }
            quad(out, v[0][0], v[1][0], v[1][1], v[0][1]);
        }
    }
}
// How much of a rope's fine structure (plies, strays) to show: none under
// about `from` pixels across, all of it at twice that.
inline float structureSeen(float radius, float range, float pixelAngle, float from) {
    return std::clamp((2 * radius / (pixelAngle * std::max(range, 1e-3f)) - from) / from, 0.f, 1.f);
}
static void appendNineties(std::vector<Vertex>& out, const WebLine& web, Vec3 viewer, float pixelAngle,
                           const WebLook& look) {
    const Vec3 chord = web.end - web.start;
    const float distance = length(chord);
    if (!finite(web.start) || !finite(web.end) || !finite(viewer) || distance < .05f || distance > 1000 ||
        !std::isfinite(pixelAngle) || pixelAngle <= 0 || !std::isfinite(web.extended) || web.extended <= 0)
        return;
    const float extended = std::min(web.extended, 1.f);
    const size_t start = out.size();
    // Parabolic approximation: arc length ~= chord + 8 h^2 / (3 chord).
    const float slack = std::isfinite(web.slack) ? std::max(web.slack, 0.f) : 0.f;
    const float sag = std::min(std::sqrt(3 * distance * slack / 8), distance * .5f);
    const Vec3 down{0, -1, 0};
    // Update 37: the sprung bend (RopeBend), never more than a quarter of the rope across.
    const Vec3 bow = finite(web.bow) && length(web.bow) < distance * .25f ? web.bow : Vec3{};
    // Update 15: a shot rope ripples across its line in flight, palm and tip
    // held, and twangs once as it snaps taut; across the eye's view of it.
    const float wave = std::isfinite(web.wave) && std::isfinite(web.wavePhase)
                           ? std::clamp(web.wave, -.5f, .5f) * std::min(1.f, distance / 2)
                           : 0.f;
    Vec3 waveSide = cross(chord, viewer - web.start);
    waveSide = length(waveSide) > 1e-5f ? normalized(waveSide) : perpendicular(normalized(chord));
    const auto ripple = [&](float u) {
        const float s = std::clamp(u / extended, 0.f, 1.f);
        return std::sin(pi * s) * std::sin(3 * pi * s - web.wavePhase);
    };
    auto point = [&](float u) {
        const Vec3 p = web.start + chord * u + down * (sag * 4 * u * (1 - u)) + bow * (4 * u * (1 - u));
        return wave != 0 ? p + waveSide * (wave * ripple(u)) : p;
    };
    auto tangent = [&](float u) {
        const Vec3 t = chord + down * (sag * 4 * (1 - 2 * u)) + bow * (4 * (1 - 2 * u));
        if (wave == 0)
            return normalized(t);
        const float h = 1e-3f * extended + 1e-5f;
        const float slope = (ripple(u + h) - ripple(u - h)) / (2 * h);
        return normalized(t + waveSide * (wave * slope));
    };
    // Update 12: a real tube, no longer a strip turned to the eye. Rings of
    // vertices round the rope at steps along it, placed in the world, so
    // each eye sees it from its own side and at its own depth; the coil rings
    // (web_shading.hpp: in groups of three that crowd the wrist and spread
    // along the line) are raised bulges of the mesh, inked where two rings
    // meet. Each pixel is lit in the three cel tones from its normal; the
    // silhouette's ink is a dark band behind the tube, a little wider.
    namespace ws = web_shading;
    const float reachMetres = extended * distance;
    const auto range = [&](float m) { return std::max(length(viewer - point(m / distance)), 1e-3f); };
    // Rings fade in where one is over about two and a half pixels long, as
    // the Update 11 shading faded them.
    const auto ringSeen = [&](float m) {
        return ws::saturate(ws::ringLength / (pixelAngle * range(m)) / 1.5f - 1);
    };
    const auto groupStart = [](float coils) {
        return ws::firstSpacing * (std::exp(ws::spacingGrowth * coils) - 1) / ws::spacingGrowth;
    };
    const auto ringShare = [](float group) {
        return std::min(ws::ringLength / (ws::firstSpacing * std::exp(ws::spacingGrowth * group)),
                        ws::groupShare / ws::wraps);
    };
    const auto bump = [&](float m) {
        const float c = ws::coilsAt(m), f = ws::fract(c), ring = ringShare(std::floor(c));
        if (f >= ring * ws::wraps)
            return 0.f;
        return std::sqrt(std::max(std::sin(pi * ws::fract(f / ring)), 0.f));
    };
    // The rope's own radius, the same all the way (Update 13: no taper toward
    // the hand or the anchor), widened by the pixel floor only far off where
    // it would be under it; and the outline with its coils.
    const auto bare = [&](float) { return look.radius; };
    const auto floored = [&](float m) { return halfWidth(look, viewer, point(m / distance), pixelAngle, bare(m)); };
    // The bulges themselves only where a ring is over about ten pixels long
    // (full at twenty): smaller, they would be a pixel or two high, and the
    // inked edges alone carry the rings.
    const auto bulgeSeen = [&](float m) {
        return ws::saturate(ws::ringLength / (pixelAngle * range(m)) / 10 - 1);
    };
    const auto radius = [&](float m) { return floored(m) * (1 + ws::tubeBulge * bulgeSeen(m) * bump(m)); };
    // Where to put rings of vertices: steps that follow the curve, and on
    // rings big enough to see, their edges (inked) and two points across.
    struct Step {
        float m, groove;
    };
    // Round the rope: 12 sides where it is wide on screen, down to 8 (where
    // it comes nearest the eye: at the wrist, or beside it).
    float wristPixels = floored(0) / (pixelAngle * range(0));
    for (float m = .05f; m < std::min(reachMetres, 3.f) && wristPixels <= 12; m += .05f)
        wristPixels = std::max(wristPixels, floored(m) / (pixelAngle * range(m)));
    const int sides = wristPixels > 12 ? 12 : wristPixels > 4 ? 10 : 8;
    // Update 24: the plies show (and twist) on a 12-sided rope over ten
    // pixels across; their steps are close enough for the twist there.
    const auto plySeenAt = [&](float m) {
        return sides == 12 ? structureSeen(floored(m), range(m), pixelAngle, 10) : 0.f;
    };
    std::vector<Step> steps;
    for (float m = 0;;) {
        steps.push_back({m, 0});
        if (m >= reachMetres)
            break;
        m = std::min(m + std::clamp(.06f + .05f * range(m), .06f, 6.f), reachMetres);
    }
    for (int group = 0; groupStart(static_cast<float>(group)) < reachMetres && group < 400; ++group) {
        const float ring = ringShare(static_cast<float>(group));
        if (ringSeen(groupStart(static_cast<float>(group))) <= 0 &&
            ringSeen(groupStart(static_cast<float>(group) + 3 * ring)) <= 0)
            continue;
        for (int j = 0; j <= 3; ++j) {
            // Two points across a bulging ring; one where it is flat, so the
            // inked edges either side stay lines and do not meet.
            const bool bulging =
                bulgeSeen(groupStart(static_cast<float>(group) + static_cast<float>(j) * ring)) > 0 ||
                bulgeSeen(groupStart(static_cast<float>(group) + static_cast<float>(j + 1) * ring)) > 0;
            for (const float t : {0.f, .3f, .5f, .7f}) {
                if (j == 3 && t > 0)
                    break;
                if (t > 0 && bulging == (t == .5f))
                    continue;
                const float m = groupStart(static_cast<float>(group) + (static_cast<float>(j) + t) * ring);
                if (m > 0 && m < reachMetres)
                    steps.push_back({m, t == 0 ? ringSeen(m) : 0});
            }
        }
    }
    std::sort(steps.begin(), steps.end(), [](const Step& a, const Step& b) { return a.m < b.m; });
    {
        std::vector<Step> kept;
        for (const auto& step : steps)
            if (kept.empty() || step.m - kept.back().m > 1e-4f) {
                // Two inked edges in a row (one group's last and the next's
                // first) get a plain step between, or the ink would fill the
                // rope from one to the other.
                if (!kept.empty() && step.groove > 0 && kept.back().groove > 0)
                    kept.push_back({(kept.back().m + step.m) * .5f, 0});
                kept.push_back(step);
            } else
                kept.back().groove = std::max(kept.back().groove, step.groove);
        steps.swap(kept);
    }
    // Update 24: where the plies twist, no gap longer than an eighth of a
    // turn (the coil rings' steps already close many); no more than
    // plyStepLimit added, for the vertex budget.
    {
        constexpr int plyStepLimit = 32;
        int added = 0;
        std::vector<Step> kept;
        for (size_t i = 0; i < steps.size(); ++i) {
            if (i > 0) {
                const float from = steps[i - 1].m, gap = steps[i].m - from;
                const float most = ninetiesTwist / (8 * std::max(plySeenAt(from), .01f));
                const int more = std::min(static_cast<int>(std::ceil(gap / most)) - 1, plyStepLimit - added);
                for (int k = 1; k <= more; ++k)
                    kept.push_back({from + gap * static_cast<float>(k) / static_cast<float>(more + 1), 0});
                added += std::max(more, 0);
            }
            kept.push_back(steps[i]);
        }
        steps.swap(kept);
    }
    // One frame along it (the sag stays in the plane of the chord and down),
    // so the sides do not twist but with the plies.
    Vec3 across = cross(chord, down);
    across = length(across) > 1e-4f * distance ? normalized(across) : perpendicular(normalized(chord));
    struct Ring {
        Vec3 centre, tangent, toViewer;
        float m, groove, coverage, outline, push, inkSeen, plySeen;
        // Normals as the rope leaves the ring backwards [0] and forwards [1]:
        // they differ at a ring's edge, where the bulge meets the rope.
        Vec3 point[12], normal[2][12];
    };
    std::vector<Ring> rings(steps.size());
    float spin = 0;
    for (size_t i = 0; i < steps.size(); ++i) {
        Ring& r = rings[i];
        r.m = steps[i].m;
        r.groove = steps[i].groove;
        const float u = r.m / distance;
        r.centre = point(u);
        r.tangent = tangent(u);
        r.toViewer = normalized(viewer - r.centre);
        const Vec3 a = normalized(across - r.tangent * dot(across, r.tangent)), b = cross(r.tangent, a);
        const float h = .002f, rad = radius(r.m);
        const float before = std::max(r.m - h, 0.f), after = std::min(r.m + h, reachMetres);
        const float slope[2] = {(rad - radius(before)) / std::max(r.m - before, 1e-5f),
                                (radius(after) - rad) / std::max(after - r.m, 1e-5f)};
        // Update 24: the rope is laid from three plies twisting round it (a
        // turn every ninetiesTwist metres): the rings' vertices turn with
        // them, so the plies' seams are lines of vertices.
        r.plySeen = plySeenAt(r.m);
        // The twist winds as fast as the plies are seen (none where they are
        // not, so a far rope's few rings never pinch it).
        if (i > 0)
            spin += std::min(pi * (r.m - rings[i - 1].m) * (r.plySeen + rings[i - 1].plySeen) / ninetiesTwist,
                             pi / 4); // an eighth of a turn a step at most, or the tube would pinch
        for (int k = 0; k < sides; ++k) {
            const float angle = 2 * pi * static_cast<float>(k) / static_cast<float>(sides) + spin;
            const Vec3 e = a * std::cos(angle) + b * std::sin(angle);
            r.point[k] = r.centre + e * rad;
            for (int s = 0; s < 2; ++s)
                r.normal[s][k] = normalized(e - r.tangent * slope[s]);
        }
        // A strand widened by the pixel floor is drawn at its true width's
        // share of opacity (Update 9), never below farOpacity.
        r.coverage = std::clamp(bare(r.m) / floored(r.m), look.farOpacity, 1.f);
        // The ink band: behind the tube's far side, reaching inkPixels past
        // its outline as seen from there; it fades out on a rope under about
        // two pixels across, as the strip's ink did.
        const float far = range(r.m);
        r.push = rad * 1.25f + .001f;
        const float pixel = pixelAngle * (far + r.push);
        r.outline = rad * (far + r.push) / far + ws::inkPixels * pixel;
        r.inkSeen = ws::saturate(floored(r.m) / (pixelAngle * far) / 2.5f - .6f);
    }
    const Vec3 core = look.core;
    const auto ropeAt = [&](float m) {
        m = std::clamp(m, 0.f, reachMetres);
        const float u = m / distance;
        const Vec3 t = tangent(u), a = normalized(across - t * dot(across, t));
        return RopeAt{point(u), t, a, cross(t, a), look.radius,
                      structureSeen(look.radius, range(m), pixelAngle, 8)};
    };
    // The ink band first: the tube drawn over it covers all but its edges.
    for (size_t i = 0; i + 1 < rings.size(); ++i) {
        // None where it would not show: it would still hide what lies behind.
        if (rings[i].inkSeen <= 0 && rings[i + 1].inkSeen <= 0)
            continue;
        Vertex band[2][2];
        for (int j = 0; j < 2; ++j) {
            const Ring& r = rings[i + j];
            Vec3 side = cross(r.tangent, r.toViewer);
            side = length(side) > 1e-4f ? normalized(side) : perpendicular(r.tangent);
            const Vec3 back = r.centre - r.toViewer * r.push;
            const float pixels = r.outline / (pixelAngle * (range(r.m) + r.push));
            const float reach = 1 + 1 / std::max(pixels, .5f);
            band[j][0] = {back - side * (r.outline * reach), core, r.coverage, ws::inkShape(-reach, r.inkSeen)};
            band[j][1] = {back + side * (r.outline * reach), core, r.coverage, ws::inkShape(reach, r.inkSeen)};
        }
        quad(out, band[0][0], band[1][0], band[1][1], band[0][1]);
    }
    // The tube's sides that face the viewer, with some to spare for the
    // eyes either side of it.
    for (size_t i = 0; i + 1 < rings.size(); ++i) {
        const Ring &r0 = rings[i], &r1 = rings[i + 1];
        for (int k = 0; k < sides; ++k) {
            const int l = (k + 1) % sides;
            const Vec3 middle = (r0.point[k] + r0.point[l] + r1.point[k] + r1.point[l]) * .25f;
            const Vec3 facing = r0.normal[1][k] + r0.normal[1][l] + r1.normal[0][k] + r1.normal[0][l];
            if (dot(normalized(facing), normalized(viewer - middle)) < -.3f)
                continue;
            // Update 24: each ply's face leans along the rope with its twist
            // (the cel tones step across it in a spiral) and the seams
            // between plies are inked, near the eye only.
            const int plyStart = k / 4 * 4;
            const auto vertex = [&](const Ring& r, int way, int side, int along) {
                Vec3 normal = r.normal[way][side];
                float groove = r.groove;
                if (r.plySeen > 0) {
                    normal = normalized(normal + r.tangent * (ninetiesLean * r.plySeen * (.5f * along - 1)));
                    if (along % 4 == 0)
                        groove = std::max(groove, r.plySeen);
                }
                return Vertex{r.point[side], core, r.coverage, ws::tubeShape(normal, groove)};
            };
            const int ak = k - plyStart, al = ak + 1;
            quad(out, vertex(r0, 1, k, ak), vertex(r1, 0, k, ak), vertex(r1, 0, l, al), vertex(r0, 1, l, al));
        }
    }
    // Its ends closed, where they face the viewer.
    for (const bool wrist : {true, false}) {
        if (rings.size() < 2)
            break;
        const Ring& r = wrist ? rings.front() : rings.back();
        const Vec3 normal = wrist ? r.tangent * -1.f : r.tangent;
        if (dot(normal, r.toViewer) < -.3f)
            continue;
        const Vertex centre{r.centre, core, r.coverage, ws::tubeShape(normal, 0)};
        for (int k = 0; k < sides; ++k) {
            const int l = (k + 1) % sides;
            out.insert(out.end(), {centre, {r.point[k], core, r.coverage, ws::tubeShape(normal, 0)},
                                   {r.point[l], core, r.coverage, ws::tubeShape(normal, 0)}});
        }
    }
    // Update 24: a few cream fibres come loose near the eye, drawn flat in
    // the rope's shadow tone (an inked cartoon has no soft fibres).
    {
        float nearEnd = 0;
        for (const Ring& r : rings)
            if (r.m <= reachMetres && structureSeen(look.radius, range(r.m), pixelAngle, 8) > 0)
                nearEnd = r.m;
        Strays strays;
        strays.perMetre = 6;
        strays.loopShare = .45f;
        strays.width = .09f;
        strays.alpha = 1;
        strays.taper = .4f;
        strays.color = core * .62f;
        strays.tip = core * .7f;
        strays.seed = web.seed + 2.3f;
        // Within the vertex budget, with room left for the splat.
        const size_t used = out.size() - start, budget = web_style::vertexBudget - 1500;
        if (!look.rope) // Update 38 WEB ROPE: no loose ends
            appendStrays(out, strays, viewer, pixelAngle, std::min(nearEnd, reachMetres * .97f), 0,
                         [&](float m) { return ropeAt(m); }, used < budget ? budget - used : 0);
    }
    if (extended < 1) {
        glob(out, point(extended), viewer, halfWidth(look, viewer, point(extended), pixelAngle, look.radius * 1.4f),
             look.core);
        return;
    }
    if (web.splat)
        appendSplat(out, web, viewer, pixelAngle, look);
}
namespace web_style {
namespace {
constexpr const char* names[count] = {"90s", "MESSY", "MODERN", "SYMBIOTE", "ENERGY"};
constexpr const char* keys[count] = {"90s", "messy", "modern", "symbiote", "energy"};
struct ShadeEntry {
    const char *name, *key;
    Shade colors;
};
// Linear RGB. The 90s default is WebLook's own cream, so 90s CREAM is the
// rope as it always was.
constexpr Vec3 splatEdge{.3f, .34f, .42f};
constexpr ShadeEntry nineties4[] = {
    {"CREAM", "cream", {{.807f, .763f, .631f}, splatEdge}},
    {"IVORY", "ivory", {{.90f, .86f, .66f}, splatEdge}},
    {"PARCHMENT", "parchment", {{.78f, .66f, .44f}, {.34f, .3f, .3f}}},
    {"WARM GREY", "warm_grey", {{.56f, .53f, .48f}, {.26f, .27f, .3f}}},
};
constexpr ShadeEntry messy4[] = {
    // Update 20: SILVER and PEARL brighter, near white: silk is lit darker.
    {"SILVER", "silver", {{.88f, .91f, .96f}, {.66f, .70f, .78f}}},
    {"PEARL", "pearl", {{.96f, .86f, .95f}, {.74f, .68f, .76f}}},
    {"SMOKE", "smoke", {{.44f, .46f, .50f}, {.3f, .32f, .36f}}},
    {"FROST", "frost", {{.64f, .72f, .84f}, {.5f, .62f, .78f}}},
};
constexpr ShadeEntry modern4[] = {
    {"BRIGHT WHITE", "bright_white", {{1.f, 1.f, 1.f}, {.7f, .74f, .82f}}},
    {"PLATINUM", "platinum", {{.74f, .72f, .67f}, {.55f, .53f, .5f}}},
    {"ICE", "ice", {{.50f, .88f, 1.f}, {.45f, .62f, .8f}}},
    {"CHROME", "chrome", {{.30f, .33f, .38f}, {.2f, .22f, .26f}}},
};
constexpr ShadeEntry symbiote4[] = {
    {"BLUE SHEEN", "blue_sheen", {{.24f, .31f, 1.f}, {.09f, .08f, .2f}}},
    {"PURPLE SHEEN", "purple_sheen", {{.62f, .18f, 1.f}, {.14f, .06f, .2f}}},
    {"CRIMSON SHEEN", "crimson_sheen", {{1.f, .08f, .10f}, {.2f, .04f, .05f}}},
    {"MATTE", "matte", {{.10f, .10f, .11f}, {.06f, .06f, .07f}}},
};
constexpr EnergyPalette palettes[6] = {
    {{.02f, .30f, 1.f}, {.25f, .62f, 1.f}, {.0f, .16f, .85f}}, // BLUE: electric
    {{1.f, .04f, .03f}, {1.f, .34f, .22f}, {.80f, .0f, .02f}}, // RED
    {{.06f, 1.f, .12f}, {.40f, 1.f, .42f}, {.0f, .70f, .08f}}, // GREEN
    {{1.f, .22f, .0f}, {1.f, .48f, .16f}, {.85f, .1f, .0f}},   // ORANGE
    {{.55f, .08f, 1.f}, {.75f, .45f, 1.f}, {.34f, .0f, .80f}}, // PURPLE
    {{1.f, .62f, .04f}, {1.f, .82f, .34f}, {.8f, .42f, .0f}},  // GOLD
};
constexpr ShadeEntry energy6[] = {
    {"BLUE", "blue", {palettes[0].core, palettes[0].rim}},     {"RED", "red", {palettes[1].core, palettes[1].rim}},
    {"GREEN", "green", {palettes[2].core, palettes[2].rim}},   {"ORANGE", "orange", {palettes[3].core, palettes[3].rim}},
    {"PURPLE", "purple", {palettes[4].core, palettes[4].rim}}, {"GOLD", "gold", {palettes[5].core, palettes[5].rim}},
};
struct List {
    const ShadeEntry* at;
    int n;
};
List list(int style) {
    switch (style) {
    case messy:
        return {messy4, 4};
    case modern:
        return {modern4, 4};
    case symbiote:
        return {symbiote4, 4};
    case energy:
        return {energy6, 6};
    default:
        return {nineties4, 4};
    }
}
const ShadeEntry& entry(int style, int i) {
    const List l = list(style);
    return l.at[i >= 0 && i < l.n ? i : 0];
}
} // namespace
const char* name(int style) {
    return names[style >= 0 && style < count ? style : 0];
}
const char* key(int style) {
    return keys[style >= 0 && style < count ? style : 0];
}
int shades(int style) {
    return list(style).n;
}
const char* shadeName(int style, int i) {
    return entry(style, i).name;
}
const char* shadeKey(int style, int i) {
    return entry(style, i).key;
}
Shade shade(int style, int i) {
    return entry(style, i).colors;
}
EnergyPalette energyPalette(int i) {
    return palettes[i >= 0 && i < 6 ? i : 0];
}
Vec3 swatch(int style, int i) {
    const Vec3 color = shade(style, i).color;
    if (style < messy || style >= count)
        return color; // the 90s rope's lit cel tone is its colour
    // The facing half of a tube lit from the sky, seen square on: the
    // overlay's shading at nine points across it, averaged.
    namespace ws = web_shading;
    const int kind = style == messy ? ws::glossMessy : style == modern ? ws::glossModern
                     : style == symbiote ? ws::glossSymbiote : ws::glossEnergy;
    const Vec3 view{0, 0, 1}, sky = normalized(Vec3{-.35f, 1, .45f}), half = normalized(sky + view);
    Vec3 sum{};
    for (int k = 0; k < 9; ++k) {
        const float a = -1.4f + 2.8f * static_cast<float>(k) / 8;
        const Vec3 e{std::sin(a), 0, std::cos(a)};
        Vec3 c = color;
        // MODERN/MESSY's z is the twist's phase: a quarter turn, the stripe's mean.
        ws::shadeGloss(c, ws::glossShape(kind, dot(e, sky), dot(e, half), kind <= ws::glossMessy ? .25f : dot(e, view)),
                       1);
        sum = sum + Vec3{std::min(c.x, 1.f), std::min(c.y, 1.f), std::min(c.z, 1.f)};
    }
    return sum * (1.f / 9);
}
bool sane(const std::vector<Vertex>& out, size_t from) {
    if (from > out.size() || out.size() - from > vertexBudget)
        return false;
    for (size_t i = from; i < out.size(); ++i) {
        const Vertex& v = out[i];
        if (!finite(v.position) || !finite(v.color) || !std::isfinite(v.alpha) || !std::isfinite(v.shape.x) ||
            !std::isfinite(v.shape.y) || !std::isfinite(v.shape.z) || !std::isfinite(v.shape.w))
            return false;
    }
    return true;
}
} // namespace web_style

// Update 18: MESSY, MODERN, SYMBIOTE and ENERGY. The same rope as the 90s
// one (its curve, sag, flight ripple, length and the far pixel floor), as
// smooth lit tubes (web_shading.hpp glossShape) instead of the inked cel
// tube; colours of their own, no textures. Returns whether it drew.
static void appendStyled(std::vector<Vertex>& out, const WebLine& web, Vec3 viewer, float pixelAngle,
                         const WebLook& look) {
    namespace ws = web_shading;
    using namespace web_style;
    const Vec3 chord = web.end - web.start;
    const float distance = length(chord);
    if (!finite(web.start) || !finite(web.end) || !finite(viewer) || distance < .05f || distance > 1000 ||
        !std::isfinite(pixelAngle) || pixelAngle <= 0 || !std::isfinite(web.extended) || web.extended <= 0)
        return;
    const int style = look.style;
    const float seconds = std::isfinite(look.seconds) ? std::fmod(std::fabs(look.seconds), 3600.f) : 0.f;
    const float extended = std::min(web.extended, 1.f);
    // The curve exactly as the 90s rope's.
    const float slack = std::isfinite(web.slack) ? std::max(web.slack, 0.f) : 0.f;
    const float sag = std::min(std::sqrt(3 * distance * slack / 8), distance * .5f);
    const Vec3 down{0, -1, 0};
    const Vec3 bow = finite(web.bow) && length(web.bow) < distance * .25f ? web.bow : Vec3{};
    const float wave = std::isfinite(web.wave) && std::isfinite(web.wavePhase)
                           ? std::clamp(web.wave, -.5f, .5f) * std::min(1.f, distance / 2)
                           : 0.f;
    Vec3 waveSide = cross(chord, viewer - web.start);
    waveSide = length(waveSide) > 1e-5f ? normalized(waveSide) : perpendicular(normalized(chord));
    const auto ripple = [&](float u) {
        const float s = std::clamp(u / extended, 0.f, 1.f);
        return std::sin(pi * s) * std::sin(3 * pi * s - web.wavePhase);
    };
    const auto point = [&](float u) {
        const Vec3 p = web.start + chord * u + down * (sag * 4 * u * (1 - u)) + bow * (4 * u * (1 - u));
        return wave != 0 ? p + waveSide * (wave * ripple(u)) : p;
    };
    const auto tangent = [&](float u) {
        const Vec3 t = chord + down * (sag * 4 * (1 - 2 * u)) + bow * (4 * (1 - 2 * u));
        if (wave == 0)
            return normalized(t);
        const float h = 1e-3f * extended + 1e-5f;
        const float slope = (ripple(u + h) - ripple(u - h)) / (2 * h);
        return normalized(t + waveSide * (wave * slope));
    };
    const float reachMetres = extended * distance;
    const auto range = [&](float m) { return std::max(length(viewer - point(m / distance)), 1e-3f); };
    const auto floored = [&](float m) { return halfWidth(look, viewer, point(m / distance), pixelAngle, look.radius); };
    // Steps a little finer than the 90s rope's (no coil rings to add theirs).
    std::vector<float> steps;
    for (float m = 0;;) {
        steps.push_back(m);
        if (m >= reachMetres || steps.size() > 900)
            break;
        m = std::min(m + std::clamp(.045f + .045f * range(m), .045f, 6.f), reachMetres);
    }
    if (steps.back() < reachMetres)
        steps.back() = reachMetres;
    Vec3 across = cross(chord, down);
    across = length(across) > 1e-4f * distance ? normalized(across) : perpendicular(normalized(chord));
    const Vec3 sky = normalized(Vec3{-.35f, 1, .45f});
    struct Frame {
        float m, range, floored, coverage;
        Vec3 centre, tangent, a, b, toViewer;
    };
    const auto frameAt = [&](float m) {
        Frame f;
        f.m = m;
        const float u = m / distance;
        f.centre = point(u);
        f.tangent = tangent(u);
        f.a = normalized(across - f.tangent * dot(across, f.tangent));
        f.b = cross(f.tangent, f.a);
        f.toViewer = normalized(viewer - f.centre);
        f.range = range(m);
        f.floored = floored(m);
        f.coverage = std::clamp(look.radius / f.floored, look.farOpacity, 1.f);
        return f;
    };
    std::vector<Frame> frames(steps.size());
    for (size_t i = 0; i < steps.size(); ++i)
        frames[i] = frameAt(steps[i]);
    // A tube round `centre(i)`, `radius(i)` wide, through frames [first, last]
    // of `fs`: `sides` round, its viewer-facing quads (some to spare for the
    // other eye) and its two ends, each vertex lit for its normal. Update 24:
    // the twist's phase rides in z (MODERN, MESSY) or x (SYMBIOTE, ENERGY);
    // round the whole rope three plies a turn, round a ply its own fibres.
    struct Ring {
        Vec3 centre;
        float radius, alpha;
        Vec3 color;
    };
    const auto tube = [&](const std::vector<Frame>& fs, size_t first, size_t last, int sides, int kind, float twist,
                          bool ply, auto&& ring) {
        if (last <= first || last >= fs.size())
            return;
        const bool inZ = kind <= ws::glossMessy;
        const auto vertex = [&](const Frame& f, const Ring& r, int k) {
            const float angle = 2 * pi * static_cast<float>(k) / static_cast<float>(sides);
            const Vec3 e = f.a * std::cos(angle) + f.b * std::sin(angle);
            const Vec3 at = r.centre + e * r.radius;
            const Vec3 v = normalized(viewer - at), half = normalized(sky + v);
            const float turns = f.m / twist + static_cast<float>(k) / sides;
            const float phase = ply ? turns : 3 * turns;
            return Vertex{at, r.color, r.alpha,
                          ws::glossShape(kind, inZ ? dot(e, sky) : phase, dot(e, half), inZ ? phase : dot(e, v), ply)};
        };
        Ring r0 = ring(first);
        for (size_t i = first; i < last; ++i) {
            const Ring r1 = ring(i + 1);
            const Frame &f0 = fs[i], &f1 = fs[i + 1];
            for (int k = 0; k < sides; ++k) {
                const float angle = 2 * pi * (static_cast<float>(k) + .5f) / static_cast<float>(sides);
                const Vec3 e = (f0.a + f1.a) * std::cos(angle) + (f0.b + f1.b) * std::sin(angle);
                const Vec3 middle = (r0.centre + r1.centre) * .5f + normalized(e) * ((r0.radius + r1.radius) * .5f);
                if (dot(normalized(e), normalized(viewer - middle)) < -.3f)
                    continue;
                quad(out, vertex(f0, r0, k), vertex(f1, r1, k), vertex(f1, r1, k + 1), vertex(f0, r0, k + 1));
            }
            r0 = r1;
        }
        for (const bool start : {true, false}) {
            const Frame& f = fs[start ? first : last];
            const Ring r = ring(start ? first : last);
            const Vec3 normal = start ? f.tangent * -1.f : f.tangent;
            if (dot(normal, f.toViewer) < -.3f)
                continue;
            const Vec3 v = normalized(viewer - r.centre), half = normalized(sky + v);
            const Shape flat = ws::glossShape(kind, dot(normal, sky), dot(normal, half), dot(normal, v), ply);
            for (int k = 0; k < sides; ++k) {
                Vertex p = vertex(f, r, k), q = vertex(f, r, k + 1);
                p.shape = q.shape = flat;
                out.insert(out.end(), {{r.centre, r.color, r.alpha, flat}, p, q});
            }
        }
    };
    const size_t last = frames.size() - 1;
    const float wristPixels = frames[0].floored / (pixelAngle * frames[0].range);
    const int sides = wristPixels > 12 ? 12 : wristPixels > 4 ? 10 : 8;
    // Update 24: every style is a twisted fibrous strand (the October 23
    // player's reference photo: a fine twisted core of many thin fibres with
    // loose wisps and small frayed loops), each in its own colour and
    // character. Near the eye the rope is laid from `plies` real 3D strands
    // twisting round its line inside its own radius (the width stays
    // web_width's, the same all along); they close up into the one tube as it
    // narrows on screen (none under fibrePixels across, all at twice that),
    // and further off the tube carries the twist in its shading alone, which
    // fades out in turn before it could shimmer. Loose fibres (appendStrays)
    // stand off it near the eye only.
    struct Lay {
        int plies;
        float twist, offset; // metres a turn; a ply's centre off the rope's line, in its radii
    };
    const Lay lay = style == messy      ? Lay{3, .16f, .54f}
                    : style == modern   ? Lay{3, .075f, .5f}
                    : style == symbiote ? Lay{3, .24f, .56f}
                                        : Lay{3, .12f, .52f};
    const auto plySeen = [&](float m) { return structureSeen(look.radius, range(m), pixelAngle, fibrePixels); };
    size_t split = 0; // coarse frames [0, split] show plies
    while (split < last && plySeen(frames[split + 1].m) > 0)
        ++split;
    const bool plied = plySeen(0) > 0;
    const float nearEnd = !plied ? 0.f : split < last ? frames[split + 1].m : reachMetres;
    // The plies' own frames: fine enough for the twist near the eye (seven a
    // turn), coarser where they are small, never more than 150.
    std::vector<Frame> nearFrames;
    if (plied) {
        for (float m = 0;;) {
            nearFrames.push_back(frameAt(m));
            if (m >= nearEnd || nearFrames.size() > 150)
                break;
            const float step = std::max(lay.twist / 7 * std::max(1.f, range(m) / .6f), nearEnd / 150);
            m = std::min(m + step, nearEnd);
        }
        nearFrames.back() = frameAt(nearEnd);
    }
    // The trunk: the single tube beyond the plies.
    std::vector<Frame> trunk;
    if (plied && nearEnd < reachMetres) {
        trunk.push_back(frameAt(nearEnd));
        for (const Frame& f : frames)
            if (f.m > nearEnd + 1e-4f)
                trunk.push_back(f);
    } else if (!plied)
        trunk = frames;
    const auto rope = [&](const std::vector<Frame>& fs, int kind, float alpha, Vec3 color, auto&& wobble) {
        if (fs.size() >= 2)
            tube(fs, 0, fs.size() - 1, sides, kind, lay.twist, false, [&](size_t i) {
                const Frame& f = fs[i];
                const auto w = wobble(f);
                return Ring{f.centre + w.shift, f.floored * w.swell, std::min(1.f, alpha * f.coverage), color};
            });
    };
    // The plies: each `alpha` where they stand apart, sharing it where they
    // close up (so the joint with the trunk does not brighten).
    const auto plies = [&](int kind, float trunkAlpha, float plyAlpha, auto&& tint, auto&& wobble) {
        if (nearFrames.size() < 2)
            return;
        const int plySides = wristPixels > 40 ? 8 : wristPixels > 16 ? 6 : 5;
        for (int j = 0; j < lay.plies; ++j)
            tube(nearFrames, 0, nearFrames.size() - 1, plySides, kind, lay.twist, true, [&, j](size_t i) {
                const Frame& f = nearFrames[i];
                const float w = plySeen(f.m);
                const auto wb = wobble(f, j);
                const float angle = 2 * pi * (static_cast<float>(j) / lay.plies + f.m / lay.twist) + wb.turn;
                const float envelope = f.floored * wb.swell, d = envelope * lay.offset * w;
                const Vec3 c = f.centre + wb.shift + (f.a * std::cos(angle) + f.b * std::sin(angle)) * d;
                const float alpha = std::min(1.f, (trunkAlpha + (plyAlpha - trunkAlpha) * w) * f.coverage);
                const float share = 1 - std::pow(std::max(1 - alpha, 0.f),
                                                  1 / (lay.plies - (lay.plies - 1) * w));
                return Ring{c, std::max(envelope - d, envelope * .3f), share, tint(j)};
            });
    };
    struct Still {
        Vec3 shift{};
        float swell = 1, turn = 0;
    };
    const auto still = [](const Frame&, int = 0) { return Still{}; };
    const auto ropeAt = [&](float m) {
        const Frame f = frameAt(std::clamp(m, 0.f, reachMetres));
        return RopeAt{f.centre, f.tangent, f.a, f.b, look.radius,
                      structureSeen(look.radius, f.range, pixelAngle, 8)};
    };
    const float strayEnd = std::min(nearEnd, reachMetres * .97f);
    // The tip in flight and the splat, in the style's colours.
    WebLook tipLook = look;
    if (style == messy) {
        // A silvery-white strand: three plies of silk fibres (each with its
        // own glints along the twist, web_shading.hpp), translucent, and the
        // most frayed - loops and wisps standing off it all along the near
        // part. Update 20 had five smooth fibres round a core; Update 24's
        // reference photo is fibrous all through.
        const Shade tone = shade(messy, look.color);
        const Vec3 silver = tone.color;
        // Update 38 WEB ROPE: opaque, a solid rope.
        const float messyAlpha = look.rope ? .96f : .62f;
        rope(trunk, ws::glossMessy, messyAlpha, silver, still);
        plies(ws::glossMessy, messyAlpha, look.rope ? .96f : .8f, [&](int j) { return mix(silver, Vec3{1, 1, 1}, .06f * static_cast<float>(j)); }, still);
        Strays strays;
        strays.perMetre = 26;
        strays.loopShare = .45f;
        strays.width = .05f;
        strays.alpha = .55f;
        strays.color = silver;
        strays.tip = mix(silver, Vec3{1, 1, 1}, .3f);
        strays.seed = web.seed;
        if (!look.rope) // Update 38 WEB ROPE: no loose ends
            appendStrays(out, strays, viewer, pixelAngle, strayEnd, seconds, ropeAt);
        // Update 37.2 (the SM1 look): a faint white glow round the rope with jagged edges -
        // a band turned to the eye behind it, its width broken up along the line (fixed per
        // web, so it does not crawl), brightest at the rope and gone at its ragged edge.
        if (!look.rope) { // Update 38 WEB ROPE: no glow band
            const float across5[5] = {-1, -.35f, 0, .35f, 1}, alpha5[5] = {0, .16f, .24f, .16f, 0};
            const Vec3 glow = mix(silver, Vec3{1, 1, 1}, .7f);
            Vertex band[2][5];
            bool glowOn[2]{};
            for (size_t i = 0; i <= last; ++i) {
                const Frame& f = frames[i];
                Vec3 side = cross(f.tangent, f.toViewer);
                side = length(side) > 1e-4f ? normalized(side) : perpendicular(f.tangent);
                const float push = f.floored * 1.1f + .001f;
                const Vec3 back = f.centre - f.toViewer * push;
                const float jag = .75f + .25f * std::sin(f.m * 37.f + 11 * web.seed) * std::sin(f.m * 13.f + 3.1f) +
                                  .2f * std::sin(f.m * 71.f + 5 * web.seed);
                const float half = std::max(f.floored * 2.2f * jag, 1.5f * pixelAngle * (f.range + push));
                // Only where the fibres show (fibrePixels; far off it is plain, as since Update 24:
                // no glow to shimmer on a thin far line).
                const float fade = std::min(1.f, f.coverage * 1.5f) *
                                   structureSeen(look.radius, f.range, pixelAngle, fibrePixels);
                for (int k = 0; k < 5; ++k) {
                    const float edge = k == 0 || k == 4 ? jag : 1.f;
                    band[i & 1][k] = {back + side * (half * across5[k] * edge), glow, alpha5[k] * fade};
                }
                glowOn[i & 1] = fade > 0;
                if (i > 0 && (glowOn[0] || glowOn[1]))
                    for (int k = 0; k < 4; ++k)
                        quad(out, band[(i - 1) & 1][k], band[i & 1][k], band[i & 1][k + 1], band[(i - 1) & 1][k + 1]);
            }
        }
        tipLook.core = mix(tone.color, Vec3{1, 1, 1}, .2f);
        tipLook.edge = tone.edge;
    } else if (style == modern) {
        // Tight and clean: a firm twist, fine even fibres, few loose ends.
        const Shade tone = shade(modern, look.color);
        const Vec3 white = tone.color * .9f;
        rope(trunk, ws::glossModern, .88f, white, still);
        plies(ws::glossModern, .88f, .88f, [&](int) { return white; }, still);
        Strays strays;
        strays.perMetre = 4;
        strays.loopShare = .1f;
        strays.reach = .6f;
        strays.curl = .3f;
        strays.width = .04f;
        strays.alpha = .4f;
        strays.color = strays.tip = white;
        strays.seed = web.seed;
        if (!look.rope) // Update 38 WEB ROPE: no loose ends
            appendStrays(out, strays, viewer, pixelAngle, strayEnd, seconds, ropeAt);
        tipLook.core = tone.color;
        tipLook.edge = tone.edge;
    } else if (style == symbiote) {
        // Black and slick: three tendrils twisting round each other, the
        // whole rope alive between hand and anchor, tapering tendrils
        // reaching off it.
        const Shade tone = shade(symbiote, look.color);
        const Vec3 black = tone.color; // the sheen: the shader keeps the rope near black
        const float writhe = seconds * 1.6f + 6 * web.seed;
        const auto alive = [&](const Frame& f, int j = 0) {
            const float m = f.m;
            // Held still at the hand and at the end, alive between.
            const float hold = std::clamp(m / .15f, 0.f, 1.f) * std::clamp((reachMetres - m) / .15f, 0.f, 1.f);
            Still w;
            w.shift = (f.a * std::sin(m * 5.3f + writhe) + f.b * std::sin(m * 3.7f + 1.9f + writhe * .8f)) *
                      (look.radius * .4f * hold);
            w.swell = 1 + .17f * hold * std::sin(m * 9.7f + writhe * 1.3f);
            w.turn = .5f * hold * std::sin(m * 4.1f + writhe * 1.1f + 2.1f * j);
            return w;
        };
        rope(trunk, ws::glossSymbiote, 1, black, alive);
        plies(ws::glossSymbiote, 1, 1, [&](int) { return black; }, alive);
        Strays strays;
        strays.perMetre = 7;
        strays.loopShare = .15f;
        strays.reach = 1.4f;
        strays.width = .2f;
        strays.taper = 1;
        strays.curl = 1.4f;
        strays.alpha = .95f;
        strays.color = black * .05f;
        strays.tip = black * .22f;
        strays.drift = .04f;
        strays.seed = web.seed;
        if (!look.rope) // Update 38 WEB ROPE: no loose ends
            appendStrays(out, strays, viewer, pixelAngle, strayEnd, seconds, ropeAt);
        tipLook.core = tone.color * .03f;
        tipLook.edge = tone.edge;
    } else { // energy
        // Glowing plasma fibres: three bright filaments twisting round each
        // other, plasma lines along them, small arcs crawling round the rope.
        const EnergyPalette pal = energyPalette(look.color);
        const auto thin = [](const Frame&, int = 0) {
            Still w;
            w.swell = .8f;
            return w;
        };
        rope(trunk, ws::glossEnergy, 1.4f, pal.core, thin);
        plies(ws::glossEnergy, 1.4f, 1.4f, [&](int j) { return j == 1 ? mix(pal.core, pal.rim, .35f) : pal.core; }, thin);
        Strays strays;
        strays.perMetre = 14;
        strays.loopShare = .7f;
        strays.reach = .8f;
        strays.curl = 1.2f;
        strays.width = .06f;
        strays.alpha = .8f;
        strays.color = pal.rim;
        strays.tip = mix(pal.core, Vec3{1, 1, 1}, .6f);
        strays.drift = .15f;
        strays.seed = web.seed;
        if (!look.rope) // Update 38 WEB ROPE: no loose ends
            appendStrays(out, strays, viewer, pixelAngle, strayEnd, seconds, ropeAt);
        // The glow: a soft band turned to the eye behind the core, fading
        // out to its edges, breathing a little.
        const float across5[5] = {-1, -.4f, 0, .4f, 1}, alpha5[5] = {0, .32f, .55f, .32f, 0};
        Vertex band[2][5];
        for (size_t i = 0; i <= last; ++i) {
            const Frame& f = frames[i];
            Vec3 side = cross(f.tangent, f.toViewer);
            side = length(side) > 1e-4f ? normalized(side) : perpendicular(f.tangent);
            const float push = f.floored * 1.25f + .001f;
            const Vec3 back = f.centre - f.toViewer * push;
            const float half = std::max(f.floored * 3.2f, 3 * pixelAngle * (f.range + push));
            const float pulse = (.85f + .15f * std::sin(seconds * 9 + f.m * 2.2f)) * std::min(1.f, f.coverage * 1.5f);
            for (int k = 0; k < 5; ++k)
                band[i & 1][k] = {back + side * (half * across5[k]), k == 0 || k == 4 ? pal.glow : pal.rim,
                                  alpha5[k] * pulse};
            if (i > 0)
                for (int k = 0; k < 4; ++k)
                    quad(out, band[(i - 1) & 1][k], band[i & 1][k], band[i & 1][k + 1], band[(i - 1) & 1][k + 1]);
        }
        tipLook.core = mix(pal.core, Vec3{1, 1, 1}, .55f);
        tipLook.edge = pal.rim;
    }
    // Update 20: MESSY and MODERN end soft and fine (no blob, no beads).
    const bool fine = style == messy || style == modern;
    if (extended < 1) {
        const float r = halfWidth(look, viewer, point(extended), pixelAngle, look.radius * (fine ? 1.1f : 1.4f));
        if (fine)
            softGlob(out, point(extended), viewer, r, tipLook.core, .75f);
        else
            glob(out, point(extended), viewer, r, tipLook.core);
        return;
    }
    if (web.splat && fine)
        appendFineSplat(out, web, viewer, pixelAngle, look, tipLook.core, style == messy ? .6f : .75f);
    else if (web.splat)
        appendSplat(out, web, viewer, pixelAngle, tipLook);
}
void appendWebBall(std::vector<Vertex>& out, Vec3 ball, Vec3 tail, Vec3 viewer, float pixelAngle, float fade) {
    // Version 0.0.3: SpidyVR's web ball look (the game's own ball in Remastered): a tight wad of
    // white web, fibres wrapped across it, and two thin silk threads twisting back to the wrist;
    // no glowing halo (0.0.2's OURS glob read as a light, not web).
    if (!finite(ball) || !finite(tail) || !finite(viewer) || !std::isfinite(pixelAngle) || !std::isfinite(fade))
        return;
    const float a = std::clamp(fade, 0.f, 1.f);
    const float range = length(ball - viewer);
    if (a < .01f || range < .05f)
        return;
    const size_t from = out.size();
    // 7 cm across near, never under 4 pixels far off.
    const float radius = std::max(.035f, 2.f * pixelAngle * range);
    const Vec3 white{1, 1, 1}, silk{.86f, .88f, .92f}, shade{.62f, .65f, .72f};
    softGlob(out, ball, viewer, radius * 1.35f, shade, .35f * a); // the wad's soft edge
    softGlob(out, ball, viewer, radius, white, .95f * a);         // the wad
    const Vec3 v = normalized(viewer - ball);
    const Vec3 s1 = perpendicular(v), s2 = cross(v, s1);
    constexpr float seed = 0; // the fibres hold still on the ball
    const float w = std::max(.0035f, .55f * pixelAngle * range);
    constexpr int fibres = 7; // chords across the wad, a little beyond its edge
    for (int i = 0; i < fibres; ++i) {
        const float t0 = 2 * pi * (static_cast<float>(i) + .37f * hash(seed + 3.1f, i)) / static_cast<float>(fibres);
        const float t1 = t0 + pi * (.55f + .35f * hash(seed + 5.3f, i));
        const Vec3 p0 = ball + (s1 * std::cos(t0) + s2 * std::sin(t0)) * (radius * 1.25f);
        const Vec3 p1 = ball + (s1 * std::cos(t1) + s2 * std::sin(t1)) * (radius * 1.25f);
        const Vec3 m = ball + (p0 + p1 - ball * 2) * .2f + v * (radius * .3f);
        for (const auto& [q0, q1] : {std::pair{p0, m}, std::pair{m, p1}}) {
            Vec3 side = cross(q1 - q0, viewer - q0);
            if (!(length(side) > 1e-6f))
                continue;
            side = normalized(side) * w;
            quad(out, {q0 - side, silk, .9f * a}, {q1 - side, white, .9f * a}, {q1 + side, white, .9f * a},
                 {q0 + side, silk, .9f * a});
        }
    }
    // Two thin threads twisting back toward the wrist, fading.
    const Vec3 along = ball - tail;
    const float span = length(along);
    if (span > .05f) {
        const Vec3 dir = along / span;
        const Vec3 x1 = perpendicular(dir), x2 = cross(dir, x1);
        constexpr int pieces = 10;
        for (int t = 0; t < 2; ++t)
            for (int i = 0; i < pieces; ++i) {
                const float u0 = float(i) / pieces, u1 = float(i + 1) / pieces;
                const auto at = [&](float u) {
                    const float turn = 2 * pi * (1.5f * u) + pi * static_cast<float>(t), r = radius * .45f * u;
                    return mix(tail, ball, u) + (x1 * std::cos(turn) + x2 * std::sin(turn)) * r;
                };
                const Vec3 p0 = at(u0), p1 = at(u1);
                Vec3 side = cross(p1 - p0, viewer - p0);
                if (!(length(side) > 1e-6f))
                    continue;
                side = normalized(side);
                const float w0 = std::max(.002f, .5f * pixelAngle * length(viewer - p0));
                const float w1 = std::max(.002f, .5f * pixelAngle * length(viewer - p1));
                quad(out, {p0 - side * w0, silk, .75f * a * u0}, {p1 - side * w1, silk, .75f * a * u1},
                     {p1 + side * w1, silk, .75f * a * u1}, {p0 + side * w0, silk, .75f * a * u0});
            }
    }
    if (!web_style::sane(out, from))
        out.resize(from);
}
void appendWebThread(std::vector<Vertex>& out, Vec3 a, Vec3 b, Vec3 viewer, float pixelAngle, float fade) {
    if (!finite(a) || !finite(b) || !finite(viewer) || !std::isfinite(pixelAngle) || !std::isfinite(fade))
        return;
    const float alpha = std::clamp(fade, 0.f, 1.f);
    if (alpha < .01f || length(b - a) < .005f)
        return;
    const Vec3 white{.97f, .97f, .98f}, silk{.8f, .82f, .87f};
    const Vec3 m = mix(a, b, .5f) + Vec3{0, -.012f, 0}; // a little sag
    const Vec3 pts[3] = {a, m, b};
    for (int i = 0; i < 2; ++i) {
        const Vec3 p0 = pts[i], p1 = pts[i + 1];
        Vec3 side = cross(p1 - p0, viewer - p0);
        if (!(length(side) > 1e-6f))
            continue;
        side = normalized(side);
        const float w0 = std::max(.004f, .6f * pixelAngle * length(viewer - p0));
        const float w1 = std::max(.004f, .6f * pixelAngle * length(viewer - p1));
        quad(out, {p0 - side * w0, silk, .9f * alpha}, {p1 - side * w1, white, .9f * alpha},
             {p1 + side * w1, white, .9f * alpha}, {p0 + side * w0, silk, .9f * alpha});
    }
}
void appendWebSplash(std::vector<Vertex>& out, Vec3 at, Vec3 viewer, float pixelAngle, float fade, float seed) {
    if (!finite(at) || !finite(viewer) || !std::isfinite(pixelAngle) || !std::isfinite(fade) || !std::isfinite(seed))
        return;
    const float a = std::clamp(fade, 0.f, 1.f);
    const float range = length(at - viewer);
    if (a < .01f || range < .05f)
        return;
    const size_t from = out.size();
    const Vec3 white{1, 1, 1}, silk{.86f, .88f, .92f};
    const float grow = .4f + .6f * (1 - a); // the splash opens as it fades
    const float radius = std::max(.12f * grow, 3.f * pixelAngle * range);
    softGlob(out, at, viewer, radius * 1.6f, silk, .3f * a);
    softGlob(out, at, viewer, radius * .45f, white, .9f * a);
    const Vec3 v = normalized(viewer - at);
    const Vec3 s1 = perpendicular(v), s2 = cross(v, s1);
    constexpr int strands = 8;
    for (int i = 0; i < strands; ++i) {
        const float angle = 2 * pi * (static_cast<float>(i) + .4f * hash(seed, i)) / strands + seed;
        const Vec3 d = s1 * std::cos(angle) + s2 * std::sin(angle);
        const Vec3 side = s1 * -std::sin(angle) + s2 * std::cos(angle);
        const float reach = radius * (1.2f + .8f * hash(seed, i + 9));
        const float w = std::max(.006f, .8f * pixelAngle * range);
        const Vec3 p0 = at + d * (radius * .3f), p1 = at + d * reach;
        out.insert(out.end(), {Vertex{p0 - side * w, white, .85f * a}, Vertex{p0 + side * w, white, .85f * a},
                               Vertex{p1, silk, 0}});
    }
    // Version 0.0.3: the splat's web ring, strand to strand (SpidyVR's impact web).
    for (int ring = 1; ring <= 2; ++ring) {
        const float rr = radius * (.55f + .35f * static_cast<float>(ring));
        const float w = std::max(.004f, .6f * pixelAngle * range);
        for (int i = 0; i < strands; ++i) {
            const float t0 = 2 * pi * (static_cast<float>(i) + .4f * hash(seed, i)) / strands + seed;
            const float t1 = 2 * pi * (static_cast<float>(i + 1) + .4f * hash(seed, (i + 1) % strands)) / strands + seed;
            const Vec3 q0 = at + (s1 * std::cos(t0) + s2 * std::sin(t0)) * rr;
            const Vec3 q1 = at + (s1 * std::cos(t1) + s2 * std::sin(t1)) * rr + v * (.01f * static_cast<float>(ring));
            Vec3 side = cross(q1 - q0, v);
            if (!(length(side) > 1e-6f))
                continue;
            side = normalized(side) * w;
            quad(out, {q0 - side, silk, .7f * a}, {q1 - side, silk, .7f * a}, {q1 + side, silk, .7f * a},
                 {q0 + side, silk, .7f * a});
        }
    }
    if (!web_style::sane(out, from))
        out.resize(from);
}
// Version 0.0.4 IRON MAN ABILITIES (REPULSOR): the palm blast, its flash and the chest beam.
namespace {
const Vec3 repCore{1, 1, 1}, repHot{.75f, .93f, 1}, repGlow{.35f, .72f, 1};
// A glowing bar p-q turned to the eye: a wide faint glow, a brighter middle, a white core.
void glowBar(std::vector<Vertex>& out, Vec3 p, Vec3 q, Vec3 viewer, float pixelAngle, float width, float a) {
    const Vec3 mid = (p + q) * .5f;
    const float range = std::max(length(mid - viewer), .05f);
    const Vec3 view = normalized(viewer - mid);
    const float floor = 1.5f * pixelAngle * range;
    softBar(out, p, q, view, std::max(width, floor * 2), repGlow, .28f * a, std::max(width * .9f, floor * 2));
    softBar(out, p, q, view, std::max(width * .45f, floor), repHot, .7f * a, std::max(width * .3f, floor));
    softBar(out, p, q, view, std::max(width * .16f, floor * .6f), repCore, a, std::max(width * .1f, floor * .5f));
}
} // namespace
void appendRepulsorBolt(std::vector<Vertex>& out, Vec3 bolt, Vec3 tail, Vec3 viewer, float pixelAngle, float fade) {
    if (!finite(bolt) || !finite(tail) || !finite(viewer) || !std::isfinite(pixelAngle) || !std::isfinite(fade))
        return;
    const float a = std::clamp(fade, 0.f, 1.f);
    const float range = length(bolt - viewer);
    if (a < .01f || range < .05f)
        return;
    const size_t from = out.size();
    const float r = std::max(.09f, 3.f * pixelAngle * range);
    softGlob(out, bolt, viewer, r * 3.2f, repGlow, .3f * a);
    softGlob(out, bolt, viewer, r * 1.5f, repHot, .75f * a);
    softGlob(out, bolt, viewer, r * .7f, repCore, a);
    if (length(tail - bolt) > .02f)
        glowBar(out, tail, bolt, viewer, pixelAngle, r * 1.1f, .85f * a);
    if (!web_style::sane(out, from))
        out.resize(from);
}
void appendRepulsorFlash(std::vector<Vertex>& out, Vec3 at, Vec3 viewer, float pixelAngle, float fade, float size) {
    if (!finite(at) || !finite(viewer) || !std::isfinite(pixelAngle) || !std::isfinite(fade) || !std::isfinite(size))
        return;
    const float a = std::clamp(fade, 0.f, 1.f);
    const float range = length(at - viewer);
    if (a < .01f || range < .05f)
        return;
    const size_t from = out.size();
    const float grow = .5f + .5f * (1 - a);
    const float r = std::max(std::clamp(size, .02f, 4.f) * grow, 3.f * pixelAngle * range);
    softGlob(out, at, viewer, r * 2.2f, repGlow, .35f * a);
    softGlob(out, at, viewer, r, repHot, .8f * a);
    softGlob(out, at, viewer, r * .45f, repCore, a);
    const Vec3 v = normalized(viewer - at);
    const Vec3 s1 = perpendicular(v), s2 = cross(v, s1);
    const float ring = r * (1.1f + .9f * (1 - a));
    softArc(out, at + v * .005f, s1, s2, ring * .9f, ring, 0, 2 * pi, repHot, .6f * a, ring * .15f, 24);
    if (!web_style::sane(out, from))
        out.resize(from);
}
void appendChestBeam(std::vector<Vertex>& out, Vec3 chest, Vec3 end, Vec3 viewer, float pixelAngle, float fade) {
    if (!finite(chest) || !finite(end) || !finite(viewer) || !std::isfinite(pixelAngle) || !std::isfinite(fade))
        return;
    const float a = std::clamp(fade, 0.f, 1.f);
    if (a < .01f || length(end - chest) < .1f)
        return;
    const size_t from = out.size();
    // In pieces, so near ones turn to the eye correctly and the far end tapers a little.
    constexpr int pieces = 8;
    for (int i = 0; i < pieces; ++i) {
        const float t0 = static_cast<float>(i) / pieces, t1 = static_cast<float>(i + 1) / pieces;
        const Vec3 p = chest + (end - chest) * t0, q = chest + (end - chest) * t1;
        glowBar(out, p, q, viewer, pixelAngle, .30f * (1 - .35f * t0), a);
    }
    softGlob(out, chest, viewer, .55f, repGlow, .45f * a);
    softGlob(out, chest, viewer, .25f, repHot, .9f * a);
    softGlob(out, chest, viewer, .11f, repCore, a);
    softGlob(out, end, viewer, .9f, repGlow, .4f * a);
    softGlob(out, end, viewer, .4f, repCore, .8f * a);
    if (!web_style::sane(out, from))
        out.resize(from);
}
void appendBaton(std::vector<Vertex>& out, Vec3 grip, Vec3 tip, Vec3 viewer, float pixelAngle) {
    if (!finite(grip) || !finite(tip) || !finite(viewer) || !std::isfinite(pixelAngle) || length(tip - grip) < .05f)
        return;
    const Vec3 mid = (grip + tip) * .5f;
    const float range = length(mid - viewer);
    if (range < .05f || range > 60.f)
        return;
    const size_t from = out.size();
    const Vec3 view = normalized(viewer - mid);
    const float hw = std::max(.016f, 1.5f * pixelAngle * range);
    softBar(out, grip, tip, view, hw, Vec3{.42f, .03f, .04f}, .97f, hw * .3f);
    softBar(out, grip, grip + (tip - grip) * .3f, view, hw * 1.12f, Vec3{.05f, .035f, .035f}, .97f, hw * .3f);
    const Vec3 side = normalized(cross(tip - grip, view)) * (hw * .4f);
    softBar(out, grip + (tip - grip) * .34f + side, tip + side, view, hw * .22f, Vec3{.86f, .32f, .30f}, .6f, hw * .1f);
    if (!web_style::sane(out, from))
        out.resize(from);
}
void appendBeamRing(std::vector<Vertex>& out, float share, float flash, Pose hand, Vec3 viewer, float pixelAngle,
                    float seconds) {
    if (!std::isfinite(share) || !std::isfinite(flash) || !finite(hand.position) || !finite(viewer) ||
        !std::isfinite(pixelAngle) || pixelAngle <= 0)
        return;
    const Vec3 c = hand.position + hand.orientation.rotate({0, .045f, .065f});
    const float distance = length(viewer - c);
    if (!finite(c) || !(distance > .08f && distance < 3))
        return;
    const size_t from = out.size();
    const Vec3 view3 = (viewer - c) / distance;
    Vec3 right = cross(Vec3{0, 1, 0}, view3);
    right = length(right) > .1f ? normalized(right) : perpendicular(view3);
    const Vec3 up = cross(view3, right);
    const float px = pixelAngle * distance, soft = std::max(.8f * px, .0003f);
    constexpr float radius = .02f, half = .003f;
    constexpr Vec3 orange{1.f, .55f, .15f}, hot{1.f, .82f, .45f}, ready{.55f, .88f, 1.f}, white{.9f, .97f, 1.f};
    const float s = std::clamp(share, 0.f, 1.f), f = std::clamp(flash, 0.f, 1.f);
    const bool full = s >= 1.f;
    const float t = std::isfinite(seconds) ? seconds : 0.f;
    softArc(out, c, right, up, radius - half - 1.2f * px, radius + half + 1.6f * px, 0, 2 * pi, shadowColor, .55f,
            1.8f * px, 72, 1.1f * px);
    softArc(out, c, right, up, radius - half, radius + half, 0, 2 * pi, mix(orange, {0, 0, 0}, .65f), .5f, soft, 72);
    constexpr float top = pi / 2;
    if (full) {
        const Vec3 col = mix(ready, white, f);
        softArc(out, c, right, up, radius - half, radius + half, 0, 2 * pi, col, 1.f, soft, 72);
        if (f > 0) { // the ready flash: a ring swelling out and fading
            const float r = radius * (1.f + .9f * (1.f - f));
            softArc(out, c + view3 * .002f, right, up, r - half * .6f, r + half * .6f, 0, 2 * pi, white, .85f * f, soft, 72);
        }
    } else if (s > .002f) {
        // The arc's leading edge glows a little as it charges.
        const float breathe = .85f + .15f * std::sin(t * 5.f);
        softArc(out, c, right, up, radius - half, radius + half, top - 2 * pi * s, top, orange, breathe, soft, 72);
        const float edge = top - 2 * pi * s;
        softArc(out, c, right, up, radius - half, radius + half, edge, std::min(top, edge + .18f), hot, 1.f, soft, 8);
    }
    // The centre: the chest beam's emblem, a small disc (blue-white when ready, dim orange while cooling).
    softArc(out, c, right, up, 0, .0065f, 0, 2 * pi, full ? mix(ready, white, f) : mix(orange, {0, 0, 0}, .35f),
            full ? .95f : .7f, soft, 36);
    if (!web_style::sane(out, from))
        out.resize(from);
}
namespace {
Vec3 guideColor(uint8_t state, bool done) {
    switch (state) {
    case training_guide::blue: return done ? Vec3{.55f, .85f, 1.f} : Vec3{.2f, .55f, 1.f};
    case training_guide::success: return Vec3{.25f, 1.f, .4f};
    case training_guide::fail: return Vec3{1.f, .5f, .1f};
    default: return done ? Vec3{1.f, .55f, .45f} : Vec3{1.f, .16f, .1f};
    }
}
// A strip turned to the eye from p0 to p1, `w` half-wide, colour and alpha at each end.
void facingStrip(std::vector<Vertex>& out, Vec3 p0, Vec3 p1, Vec3 viewer, float w, Vec3 c, float a0, float a1) {
    Vec3 side = cross(p1 - p0, viewer - p0);
    if (!(length(side) > 1e-6f))
        return;
    side = normalized(side) * w;
    quad(out, {p0 - side, c, a0}, {p1 - side, c, a1}, {p1 + side, c, a1}, {p0 + side, c, a0});
}
} // namespace
void appendShockwave(std::vector<Vertex>& out, Vec3 eye, Vec3 way, float age, float pixelAngle) {
    if (!finite(eye) || !finite(way) || !(length(way) > .5f) || !std::isfinite(age) || !(age >= 0) ||
        age >= shockwaveSeconds || !std::isfinite(pixelAngle) || pixelAngle <= 0)
        return;
    const size_t from = out.size();
    const float u = age / shockwaveSeconds, fade = std::pow(1.f - u, 1.5f);
    const Vec3 w = normalized(way), a = normalized(perpendicular(w)), b = cross(w, a);
    constexpr Vec3 pale{.85f, .93f, 1.f}, white{1, 1, 1};
    // 1. The ring: from 3 m ahead to 2 m behind, 0.8 m wide to 3.5 m, thinning out as it goes.
    const float d = 3.f - 5.f * u, r = .8f + 2.7f * u, half = .05f + .09f * u;
    softArc(out, eye + w * d, a, b, std::max(r - half, 0.f), r + half, 0, 2 * pi, pale, .7f * fade, half * .9f, 96);
    softArc(out, eye + w * (d + .4f), a, b, std::max(r * .8f - half * .5f, 0.f), r * .8f + half * .5f, 0, 2 * pi, white,
            .35f * fade, half * .6f, 96);
    // 2. The vapour cone: faint streaks from just ahead flaring back past the eye.
    const Vec3 apex = eye + w * 1.6f;
    constexpr int streaks = 28;
    for (int k = 0; k < streaks; ++k) {
        const float t = 2 * pi * (static_cast<float>(k) + .5f * u) / streaks;
        const Vec3 base = eye - w * 3.f + (a * std::cos(t) + b * std::sin(t)) * (2.4f + .6f * u);
        // in two pieces split just ahead of the eyes, so the front piece is never clipped by the near plane
        const float s = (1.6f - .2f) / 4.6f, a0 = .3f * fade, a1 = 0.f, am = a0 + (a1 - a0) * s;
        const Vec3 mid = apex + (base - apex) * s;
        facingStrip(out, apex, mid, eye, .008f + .008f * u, pale, a0, am);
        facingStrip(out, mid, base, eye, .008f + .008f * u, pale, am, a1);
    }
    if (!web_style::sane(out, from))
        out.resize(from);
}
void appendTrainingGuide(std::vector<Vertex>& out, const training_guide::Draw& g, Vec3 travel, Vec3 viewer,
                         float pixelAngle) {
    if (!g.on || !finite(travel) || !finite(viewer) || !std::isfinite(pixelAngle) || !std::isfinite(g.pulse) ||
        !std::isfinite(g.ghostAlpha))
        return;
    constexpr int n = training_guide::points;
    const size_t from = out.size();
    const float pulse = std::clamp(g.pulse, 0.f, 1.f);
    for (int h = 0; h < 2; ++h) {
        const bool drawn = g.head ? h == 0 : (g.hands & (1u << h)) != 0;
        if (!drawn)
            continue;
        Vec3 p[n];
        bool ok = true;
        for (int k = 0; k < n; ++k) {
            p[k] = g.p[h][k] + travel;
            ok = ok && finite(p[k]);
        }
        if (!ok || length(p[0] - viewer) < .05f)
            continue;
        const float progress = std::isfinite(g.progress[h]) ? std::clamp(g.progress[h], 0.f, 1.f) : 0.f;
        const int doneTo = static_cast<int>(progress * (n - 1) + .5f);
        // The ribbon: a wide soft glow and a bright core; the part already followed is lighter.
        for (int k = 0; k + 1 < n; ++k) {
            const bool done = k < doneTo;
            const Vec3 c = guideColor(g.state, done);
            const float range = length(p[k] - viewer);
            const float core = std::max(.008f, 1.2f * pixelAngle * range);
            const float glow = std::max(.03f, 3.5f * pixelAngle * range);
            const float a = done ? .95f : .7f + .2f * pulse;
            facingStrip(out, p[k], p[k + 1], viewer, glow, c, .22f * a, .22f * a);
            facingStrip(out, p[k], p[k + 1], viewer, core, mix(c, Vec3{1, 1, 1}, .35f), a, a);
        }
        // Arrows along it (chevrons pointing the way), every 6 points.
        for (int k = 4; k + 1 < n; k += 6) {
            const Vec3 d = p[k + 1] - p[k];
            Vec3 side = cross(d, viewer - p[k]);
            if (!(length(d) > 1e-4f) || !(length(side) > 1e-6f))
                continue;
            const float range = length(p[k] - viewer);
            const float s = std::max(.035f, 4.f * pixelAngle * range);
            const Vec3 f = normalized(d) * s;
            side = normalized(side) * (s * .8f);
            const Vec3 c = mix(guideColor(g.state, false), Vec3{1, 1, 1}, .6f);
            out.insert(out.end(), {Vertex{p[k] + f, c, .95f}, Vertex{p[k] - f * .4f + side, c, .8f},
                                   Vertex{p[k] - f * .4f - side, c, .8f}});
        }
        // Start marker (a ring that pulses until the hand is on it) and end marker (a bright dot).
        const float r0 = std::max(.05f, 5.f * pixelAngle * length(p[0] - viewer));
        const Vec3 startColor = g.armed ? Vec3{1, 1, 1} : guideColor(g.state, false);
        softGlob(out, p[0], viewer, r0 * (g.armed ? 1.f : 1.f + .35f * pulse), startColor, g.armed ? .35f : .55f);
        softGlob(out, p[0], viewer, r0 * .35f, Vec3{1, 1, 1}, .9f);
        const float r1 = std::max(.045f, 4.5f * pixelAngle * length(p[n - 1] - viewer)) * (g.pose ? 1.6f : 1.f);
        softGlob(out, p[n - 1], viewer, r1, guideColor(g.state, false), .6f);
        softGlob(out, p[n - 1], viewer, r1 * .4f, mix(guideColor(g.state, false), Vec3{1, 1, 1}, .7f), .95f);
        // The ghost hand riding the path, showing the speed.
        const Vec3 ghost = g.ghost[h] + travel;
        const float ga = std::clamp(g.ghostAlpha, 0.f, 1.f);
        if (finite(ghost) && ga > .01f) {
            const float rg = std::max(.04f, 4.f * pixelAngle * length(ghost - viewer));
            softGlob(out, ghost, viewer, rg * 1.8f, Vec3{.7f, .9f, 1.f}, .3f * ga);
            softGlob(out, ghost, viewer, rg, Vec3{.9f, .97f, 1.f}, .85f * ga);
        }
    }
    if (!web_style::sane(out, from))
        out.resize(from);
}
void appendWeb(std::vector<Vertex>& out, const WebLine& web, Vec3 viewer, float pixelAngle, const WebLook& look) {
    // Update 18: a style that fails (anything not finite, over the vertex
    // budget, or a throw) is drawn as 90s instead; a web is never lost to it.
    if (look.style > web_style::nineties && look.style < web_style::count) {
        const size_t from = out.size();
        bool drawn = false;
        try {
            appendStyled(out, web, viewer, pixelAngle, look);
            drawn = web_style::sane(out, from);
        } catch (...) {
            drawn = false;
        }
        if (drawn)
            return;
        out.resize(std::min(from, out.size()));
        WebLook plain = look; // the 90s rope as given
        plain.style = plain.color = 0;
        appendNineties(out, web, viewer, pixelAngle, plain);
        return;
    }
    // 90s: CREAM is the look as given; its other shades only recolour it.
    if (look.color > 0 && look.color < web_style::shades(web_style::nineties)) {
        WebLook shaded = look;
        shaded.core = web_style::shade(web_style::nineties, look.color).color;
        shaded.edge = web_style::shade(web_style::nineties, look.color).edge;
        appendNineties(out, web, viewer, pixelAngle, shaded);
        return;
    }
    appendNineties(out, web, viewer, pixelAngle, look);
}
void appendAimMarker(std::vector<Vertex>& out, const AimMarker& marker, Vec3 viewer, float pixelAngle,
                     UiStyle style) {
    if (style == UiStyle::tech)
        return tech::appendAimMarker(out, marker, viewer, pixelAngle);
    const float distance = length(viewer - marker.point);
    const float opacity = std::isfinite(marker.opacity) ? std::clamp(marker.opacity, 0.f, 1.f) : 0.f;
    if (!finite(marker.point) || !finite(viewer) || !std::isfinite(pixelAngle) || pixelAngle <= 0 ||
        !(distance >= .3f && distance <= 1000) || opacity <= 0)
        return;
    // Level and upright across the line of sight, so the marker never rolls.
    const Vec3 view = (viewer - marker.point) / distance;
    Vec3 right = cross(Vec3{0, 1, 0}, view);
    right = length(right) > .1f ? normalized(right) : perpendicular(view);
    const Vec3 up = cross(view, right);
    // Metres per pixel at the marker, and how far the grip has closed toward a shot.
    const float px = pixelAngle * distance;
    const float squeeze = std::isfinite(marker.squeeze) ? std::clamp(marker.squeeze / .65f, 0.f, 1.f) : 0.f;
    const Vec3 c = marker.point;
    const Vec3 color = handColors[marker.hand ? 1 : 0], light = mix(color, {1, 1, 1}, .6f);
    // Every shape is drawn twice, first all of a marker's shadows, then its
    // colours: a soft dark shadow reaching `shade` past the shape keeps it
    // readable on the sky and on a lit wall.
    const float shade = 1.3f * px, shadowSoft = 1.8f * px, soft = .8f * px, shadowAlpha = .6f;
    const auto segments = [&](float radius) { return std::clamp(static_cast<int>(radius / px) + 12, 16, 72); };
    // An arc's shadow lies mostly outside it, a thin edge within, and
    // reaches past its ends by `lengthen` metres.
    const auto arc = [&](bool shadow, Vec3 tint, float alpha, float r, float halfWidth, float from = 0,
                         float to = 2 * pi, float lengthen = 0) {
        const float grow = shadow ? shade : 0, beyond = shadow ? lengthen / r : 0;
        softArc(out, c, right, up, std::max(r - halfWidth - grow * .4f, 0.f), r + halfWidth + grow, from - beyond,
                to + beyond, shadow ? shadowColor : tint, alpha * opacity * (shadow ? shadowAlpha : 1),
                shadow ? shadowSoft : soft, segments(r), shadow ? shadowSoft * .6f : soft);
    };
    // A dot's shadow is a thin dark edge: a wider one would read as a second ring.
    const auto dot = [&](bool shadow, Vec3 tint, float alpha, float r) {
        softArc(out, c, right, up, 0, r + (shadow ? shade * .5f : 0), 0, 2 * pi, shadow ? shadowColor : tint,
                alpha * opacity * (shadow ? shadowAlpha : 1), shadow ? shadowSoft * .7f : soft, segments(r));
    };
    switch (marker.kind) {
    case AimMark::anchor: {
        const float r = 10.5f * px * (1 - .4f * squeeze);
        for (const bool shadow : {true, false}) {
            arc(shadow, color, 1, r, 1.15f * px);
            dot(shadow, light, 1, 2 * px);
        }
        break;
    }
    case AimMark::air: {
        // Faint: short translucent dashes, their shadows lighter and no
        // longer than they are, so the gaps stay open on a bright sky.
        const float r = 8.5f * px * (1 - .4f * squeeze);
        constexpr int dashes = 8;
        for (const bool shadow : {true, false})
            for (int i = 0; i < dashes; ++i) {
                const float from = 2 * pi * i / dashes;
                arc(shadow, color, shadow ? .7f : .75f, r, .9f * px, from, from + 2 * pi / dashes * .45f);
            }
        break;
    }
    case AimMark::blocked: {
        const Vec3 rising = (right + up) * (5.5f * px * .7071f), falling = (right - up) * (5.5f * px * .7071f);
        for (const bool shadow : {true, false})
            for (const Vec3 arm : {rising, falling})
                softBar(out, c - arm, c + arm, view, .95f * px + (shadow ? shade : 0),
                        shadow ? shadowColor : color * missBrightness, opacity * (shadow ? shadowAlpha : .85f),
                        shadow ? shadowSoft : soft);
        break;
    }
    case AimMark::target: {
        // Three arcs around the target, turning, each with a claw pointing in
        // at it; while the ring locks on it closes in from wider. The right
        // hand's ring is a little wider and turned between the left's arcs,
        // so both hands' rings on one target stay apart.
        const float lock = std::isfinite(marker.lock) ? std::clamp(marker.lock, 0.f, 1.f) : 1.f;
        const float closing = lock * lock * (3 - 2 * lock);
        const float size = std::max(std::isfinite(marker.radius) ? marker.radius * 1.15f : 0.f, 13 * px);
        const float r = size * (marker.hand ? 1.15f : 1.f) * (1 - .25f * squeeze) * (1 + .8f * (1 - closing));
        const float spin = (std::isfinite(marker.spin) ? marker.spin : 0.f) + (marker.hand ? pi / 3 : 0.f);
        const float halfWidth = 1.2f * px, claw = std::clamp(r * .2f, 4.5f * px, 10 * px);
        constexpr float span = 75 * pi / 180;
        for (const bool shadow : {true, false}) {
            for (int i = 0; i < 3; ++i) {
                const float middle = pi / 2 + spin + 2 * pi * i / 3;
                arc(shadow, color, 1, r, halfWidth, middle - span / 2, middle + span / 2, shade);
                const Vec3 outward = right * std::cos(middle) + up * std::sin(middle);
                const Vec3 across = right * -std::sin(middle) + up * std::cos(middle);
                const Vec3 base = c + outward * (r - halfWidth);
                softTriangle(out, {base + across * (claw * .55f), base - outward * claw, base - across * (claw * .55f)},
                             view, shadow ? shadowColor : color, opacity * (shadow ? shadowAlpha : 1),
                             shadow ? shade : 0, shadow ? shadowSoft : soft);
            }
            dot(shadow, light, 1, 1.8f * px);
        }
        break;
    }
    }
}
Vec3 onAimLine(Vec3 point, Vec3 normal, Vec3 origin, Vec3 direction) {
    const Vec3 n = normalized(normal);
    const float facing = dot(direction, n);
    if (!finite(point) || !finite(origin) || !finite(direction) || !finite(n) || length(n) < .5f ||
        !(std::abs(facing) >= .2f))
        return point;
    const float along = dot(point - origin, n) / facing;
    const Vec3 crossing = origin + direction * along;
    return along > 0 && finite(crossing) && length(crossing - point) <= .25f * length(point - origin) + .25f
               ? crossing
               : point;
}
void appendSlowMotionMeter(std::vector<Vertex>& out, const SlowMotionView& view, Pose hand, Vec3 viewer,
                           float pixelAngle, float seconds, UiStyle style) {
    if (style == UiStyle::tech)
        return tech::appendSlowMotionMeter(out, view, hand, viewer, pixelAngle, seconds);
    const float opacity = std::isfinite(view.meter) ? std::clamp(view.meter, 0.f, 1.f) : 0.f;
    if (opacity <= 0 || !finite(hand.position) || !finite(viewer) || !std::isfinite(pixelAngle) || pixelAngle <= 0)
        return;
    // Over the back of the wrist, toward the forearm, like a watch's face.
    const Vec3 c = hand.position + hand.orientation.rotate({0, .045f, .065f});
    const float distance = length(viewer - c);
    if (!finite(c) || !(distance > .08f && distance < 3))
        return;
    // Level and upright across the line of sight, as the aim markers are.
    const Vec3 view3 = (viewer - c) / distance;
    Vec3 right = cross(Vec3{0, 1, 0}, view3);
    right = length(right) > .1f ? normalized(right) : perpendicular(view3);
    const Vec3 up = cross(view3, right);
    const float px = pixelAngle * distance, soft = std::max(.8f * px, .0003f);
    constexpr float radius = .02f, half = .0026f;
    constexpr Vec3 cyan{.3f, .85f, 1.f}, bright{.78f, .97f, 1.f}, red{1.f, .16f, .1f}, glass{.88f, .94f, 1.f};
    const auto unit = [](float v) { return std::isfinite(v) ? std::clamp(v, 0.f, 1.f) : 0.f; };
    const float focus = unit(view.focus), blend = unit(view.blend), warning = unit(view.warning);
    const float t = std::isfinite(seconds) ? seconds : 0.f;
    // In slow motion it glows and breathes; a warning flashes it red.
    const float pulse = view.active ? .5f + .5f * std::sin(t * 6.f) : 0.f;
    Vec3 fill = mix(cyan, bright, blend * (.3f + .3f * pulse));
    fill = mix(fill, red, warning * (.55f + .45f * std::cos(t * 28.f)));
    const Vec3 track = mix(mix(cyan, {0, 0, 0}, .62f), red, warning * .5f);
    // A soft dark shadow keeps it readable on the sky and on a lit wall; the
    // empty track under the fill.
    softArc(out, c, right, up, radius - half - 1.2f * px, radius + half + 1.6f * px, 0, 2 * pi, shadowColor,
            .55f * opacity, 1.8f * px, 72, 1.1f * px);
    softArc(out, c, right, up, radius - half, radius + half, 0, 2 * pi, track, .45f * opacity, soft, 72);
    if (focus > .002f) {
        constexpr float top = pi / 2;
        softArc(out, c, right, up, radius - half, radius + half, top - 2 * pi * focus, top, fill, opacity, soft,
                72);
    }
    // The hourglass: two triangles tip to tip, its shadow first.
    constexpr float width = .0062f, height = .0088f, neck = .0007f;
    const std::array<Vec3, 3> upper{c + (up * height) - right * width, c + (up * height) + right * width,
                                    c + up * neck};
    const std::array<Vec3, 3> lower{c - (up * height) + right * width, c - (up * height) - right * width,
                                    c - up * neck};
    for (const bool shadow : {true, false})
        for (const auto& shape : {upper, lower})
            softTriangle(out, shape, view3, shadow ? shadowColor : mix(glass, fill, .35f),
                         opacity * (shadow ? .55f : .9f), shadow ? 1.2f * px : 0.f, shadow ? 1.6f * px : soft);
}
void appendImpact(std::vector<Vertex>& out, const ImpactView& impact, Vec3 viewer, float pixelAngle,
                  UiStyle style) {
    if (style == UiStyle::tech)
        return tech::appendImpact(out, impact, viewer, pixelAngle);
    const float distance = length(viewer - impact.point);
    if (!finite(impact.point) || !finite(viewer) || !std::isfinite(pixelAngle) || pixelAngle <= 0 ||
        !std::isfinite(impact.age) || impact.age < 0 || impact.age > ImpactView::seconds ||
        !(distance >= .12f && distance <= 60))
        return;
    // Level and upright across the line of sight, as the aim markers are.
    const Vec3 view = (viewer - impact.point) / distance;
    Vec3 right = cross(Vec3{0, 1, 0}, view);
    right = length(right) > .1f ? normalized(right) : perpendicular(view);
    const Vec3 up = cross(view, right);
    const float strength = std::isfinite(impact.strength) ? std::clamp(impact.strength, 0.f, 1.f) : 0.f;
    const bool finisher = impact.kind == BlowKind::Finisher, heavy = impact.kind != BlowKind::Hit;
    // How far the burst reaches; a jab's is the size of the fist, a
    // finisher's of his chest.
    const float reach = finisher ? .26f : heavy ? .16f : .07f + .08f * strength;
    const int pose = static_cast<int>(impact.age / plate::pose);
    // Each blow's burst is its own drawing, by where it landed.
    const float seed = impact.point.x * 3.1f + impact.point.y * 5.3f + impact.point.z * 7.7f;
    Print print(out, impact.point, right, up, view, pixelAngle * distance);
    // The star: small, then whole; a finisher's a third pose, wider yet. Each
    // pose is drawn anew. A called blow's is out of register. A finisher's
    // star is the smaller share of its reach: an arm's length away it must
    // not fill the view; its lines carry the rest.
    if (pose < (finisher ? 3 : 2)) {
        const float outer = reach * (finisher ? .6f : .75f) * (pose == 0 ? .6f : pose == 1 ? 1.f : 1.15f);
        const int points = finisher ? 13 : heavy ? 11 : 9;
        const float turned = seed + static_cast<float>(pose);
        const Vec3 body = heavy ? plate::red : plate::yellow, heart = heavy ? plate::yellow : plate::paper;
        print.layers(outer * .045f, 0, {}, impact.called ? outer * plate::misprint : 0, [&] {
            print.star({}, outer * .5f, outer, points, turned, body);
            print.star({}, outer * .26f, outer * .56f, points, turned, heart);
        });
    }
    // Then its lines, thrown clear of where the star was: a pose for a jab,
    // two for a harder blow and three for a finisher, each farther out and
    // thinner.
    const int lines = pose - 2;
    if (lines >= 0 && lines < (finisher ? 3 : heavy ? 2 : 1)) {
        const int count = finisher ? 14 : heavy ? 10 : 7;
        const float from = reach * ((finisher ? .8f : .7f) + .4f * static_cast<float>(lines));
        const float width = reach * (.05f - .014f * static_cast<float>(lines));
        print.layers(reach * .018f, 0, {}, 0, [&] {
            for (int i = 0; i < count; ++i) {
                const float angle = seed + 2 * pi * (static_cast<float>(i) + .5f * hash(seed, i + 60)) /
                                               static_cast<float>(count);
                const Flat way{std::cos(angle), std::sin(angle)}, side{-way.y * width, way.x * width};
                const float to = from + reach * (.3f + .35f * hash(seed, i + 80));
                print.triangle(way * from + side, way * to, way * from - side, plate::paper);
            }
        });
    }
    // The count: lettered in over the blow as a sound word is. It lands in
    // three poses, off level; holds; and is gone in two, a pose large and
    // out of register, a pose small.
    if (impact.count >= 2) {
        const int last = static_cast<int>(ImpactView::seconds / plate::pose + .5f);
        Landing land = landing(impact.age);
        if (pose == last - 2)
            land = {1.12f, 0, true};
        else if (pose >= last - 1)
            land.scale = .6f;
        const std::string text = "X" + std::to_string(std::min(impact.count, 999));
        const float height = (finisher ? .06f : .042f) * std::clamp(distance / .7f, .8f, 2.5f);
        const float width = textWidth(text, height) + Print::lean * height;
        print.place(land.scale, land.turn + 8 * plate::degree, {0, reach * .6f + .05f + height * .5f});
        print.layers(height * .06f, height * .1f, finisher ? plate::red : plate::ink,
                     land.hit ? height * plate::misprint : 0,
                     [&] { print.text(text, {width * -.5f, height * -.5f}, height, .2f, plate::yellow); });
    }
}
void appendCue(std::vector<Vertex>& out, const CueView& cue, Vec3 viewer, float pixelAngle, UiStyle style) {
    if (style == UiStyle::tech)
        return tech::appendCue(out, cue, viewer, pixelAngle);
    const float distance = length(viewer - cue.point);
    if (!finite(cue.point) || !finite(viewer) || !std::isfinite(pixelAngle) || pixelAngle <= 0 ||
        !std::isfinite(cue.left) || cue.left <= 0 || !std::isfinite(cue.age) || cue.age < 0 ||
        !(distance >= .15f && distance <= 60))
        return;
    // Level and upright across the line of sight, as the bursts are.
    const Vec3 view = (viewer - cue.point) / distance;
    Vec3 right = cross(Vec3{0, 1, 0}, view);
    right = length(right) > .1f ? normalized(right) : perpendicular(view);
    const Vec3 up = cross(view, right);
    const bool called = cue.way != CueWay::None, finisher = called && cue.finisher;
    // The ring's radius: an arm's length away the mark is a hand wide;
    // farther off it keeps most of its size on screen.
    const float size = (finisher ? .13f : .1f) * std::clamp(distance / .9f, 1.f, 5.f);
    const int pose = static_cast<int>(cue.age / plate::pose);
    Print print(out, cue.point, right, up, view, pixelAngle * distance);
    if (!called) {
        // Nothing called: a target and no more. A look's mark is a thin
        // ring; a lock's is bolder, with four ticks in from it, a little off
        // level. Either snaps on, a pose wide and a pose short.
        print.place(pose == 0 ? 1.16f : pose == 1 ? .95f : 1.f, -12 * plate::degree);
        const float half = size * (cue.locked ? .022f : .014f);
        print.layers(size * .022f, 0, {}, 0, [&] {
            print.ring({}, size, half, plate::paper);
            for (int i = 0; cue.locked && i < 4; ++i) {
                const float angle = pi / 4 + pi / 2 * static_cast<float>(i);
                const Flat way{std::cos(angle), std::sin(angle)};
                print.line(way * (size * .72f), way * size, half, plate::paper);
            }
        });
        return;
    }
    const Landing land = landing(cue.age);
    const Vec3 fill = finisher ? plate::yellow : plate::paper, second = finisher ? plate::red : plate::cyan;
    // The ring: what is left of the flow's time, running down clockwise from
    // the top a notch at a time, over a thin ink track that stays whole.
    const float left = std::clamp(cue.left, 0.f, 1.f);
    print.place(land.scale, land.turn);
    if (finisher) {
        // The finisher's is a burst's edge: a zigzag, a stroke to a notch.
        constexpr int strokes = 28;
        const auto corner = [&](int i) {
            const float angle = pi / 2 - 2 * pi * static_cast<float>(i) / strokes;
            return Flat{std::cos(angle), std::sin(angle)} * (size * (i % 2 ? 1.14f : .96f));
        };
        const int shown = static_cast<int>(std::ceil(left * strokes));
        print.layers(size * .03f, 0, {}, 0, [&] {
            for (int i = shown; i < strokes; ++i)
                print.line(corner(i), corner(i + 1), size * .012f, plate::ink, i == shown);
            for (int i = 0; i < shown; ++i)
                print.line(corner(i), corner(i + 1), size * .045f, second, i == 0);
        });
    } else {
        const float turn = 2 * pi * std::ceil(left * 48) / 48;
        print.layers(size * .03f, 0, {}, 0, [&] {
            print.ring({}, size, size * .012f, plate::ink);
            // Angles run from the mark's right toward its top: clockwise
            // from the top is downward from pi / 2.
            print.ring({}, size, size * .05f, second, pi / 2 - turn, pi / 2);
        });
    }
    // The arrow: a marker's stroke with a hard drop, pointing the way the
    // fist must travel. It comes in from outside the ring, on the side the
    // fist comes from, and ends at his middle. It lands out of register, and
    // then boils as a held drawing does: drawn in three slightly different
    // places, two poses each.
    Flat way{0, -1};
    switch (cue.way) {
    case CueWay::Left:
        way = {-1, 0};
        break;
    case CueWay::Right:
        way = {1, 0};
        break;
    case CueWay::Up:
        way = {0, 1};
        break;
    default:
        break;
    }
    const Flat across{-way.y, way.x};
    struct Boil {
        float turn;
        Flat shift;
    };
    static constexpr Boil boils[3] = {{0, {}}, {-.8f, {.015f, .022f}}, {.7f, {-.022f, -.015f}}};
    const Boil& boil = boils[pose < 3 ? 0 : (pose / 2) % 3];
    print.place(land.scale, land.turn + boil.turn * plate::degree, boil.shift * size);
    print.layers(size * .04f, size * .06f, second, land.hit ? size * plate::misprint : 0, [&] {
        const Flat tip = way * (size * .3f), tail = way * (size * -1.3f), neck = way * (size * -.11f);
        print.line(tail, tip, size * .085f, fill);
        print.line(tip, neck + across * (size * .4f), size * .085f, fill, false);
        print.line(tip, neck - across * (size * .4f), size * .085f, fill, false);
    });
}
void appendKnockout(std::vector<Vertex>& out, const KnockoutView& knockout, Vec3 viewer, float pixelAngle,
                    UiStyle style) {
    if (style == UiStyle::tech)
        return tech::appendKnockout(out, knockout, viewer, pixelAngle);
    const float distance = length(viewer - knockout.point);
    if (!finite(knockout.point) || !finite(viewer) || !std::isfinite(pixelAngle) || pixelAngle <= 0 ||
        !std::isfinite(knockout.age) || knockout.age < 0 || knockout.age >= KnockoutView::seconds ||
        !(distance >= .15f && distance <= 60))
        return;
    // Level and upright across the line of sight, as the bursts are.
    const Vec3 view = (viewer - knockout.point) / distance;
    Vec3 right = cross(Vec3{0, 1, 0}, view);
    right = length(right) > .1f ? normalized(right) : perpendicular(view);
    const Vec3 up = cross(view, right);
    // It lands as a sound word does, holds three poses, and leaves in two.
    const int pose = static_cast<int>(knockout.age / plate::pose);
    Landing land = landing(knockout.age);
    if (pose == 6)
        land = {1.12f, 0, true};
    else if (pose >= 7)
        land.scale = .6f;
    constexpr std::string_view text = "KO!";
    const float height = .055f * std::clamp(distance / .7f, .8f, 2.5f);
    const float width = textWidth(text, height) + Print::lean * height;
    const float seed = knockout.point.x * 2.3f + knockout.point.y * 4.1f + knockout.point.z * 6.7f;
    Print print(out, knockout.point, right, up, view, pixelAngle * distance);
    print.place(land.scale, land.turn + 6 * plate::degree);
    print.layers(height * .07f, 0, {}, 0, [&] {
        print.star({}, width * .42f, width * .8f, 12, seed, plate::red);
        print.star({}, width * .34f, width * .64f, 12, seed, plate::paper);
    });
    print.layers(height * .06f, height * .1f, plate::red, land.hit ? height * plate::misprint : 0,
                 [&] { print.text(text, {width * -.5f, height * -.5f}, height, .2f, plate::yellow); });
}
namespace {
// What a Spider-Sense warning asks for, as its plate.
Vec3 senseColor(SenseAnswer answer) {
    switch (answer) {
    case SenseAnswer::Dodge:
        return plate::red;
    case SenseAnswer::Jump:
        return plate::yellow;
    default:
        return plate::cyan;
    }
}
// The comic's Spider-Sense lines about the mark's centre, `s` its size: five
// jagged strokes fanned up and out from a point under them, the middle one
// the longest, each kinked twice. `width` scales the strokes; `reach` how far
// out they go.
void senseLines(Print& print, float s, Vec3 color, float width = 1, float reach = 1) {
    static constexpr float angles[5] = {-56, -28, 0, 28, 56};
    const Flat from{0, -.62f * s};
    for (int i = 0; i < 5; ++i) {
        const float a = angles[i] * plate::degree;
        const Flat way{std::sin(a), std::cos(a)}, across{way.y, -way.x};
        const float r0 = .42f * s * reach, r1 = (i == 2 ? 1.18f : .98f) * s * reach, kink = .1f * s;
        Flat p[4];
        for (int k = 0; k < 4; ++k)
            p[k] = from + way * (r0 + (r1 - r0) * static_cast<float>(k) / 3) +
                   across * (k == 1 ? kink : k == 2 ? -kink : 0.f);
        const float half = .062f * s * width;
        print.line(p[0], p[1], half, color);
        print.line(p[1], p[2], half, color, false);
        print.line(p[2], p[3], half, color, false);
    }
}
// The ring that closes on the blow, `left` 1..0 of the warning's time; in its
// last notch a burst's edge, yellow: now.
void senseRing(Print& print, float s, float left) {
    if (left <= 1.f / 8 + 1e-4f) {
        constexpr int strokes = 22;
        const auto corner = [&](int i) {
            const float angle = pi / 2 - 2 * pi * static_cast<float>(i) / strokes;
            return Flat{std::cos(angle), std::sin(angle)} * (s * (i % 2 ? 1.16f : .98f));
        };
        for (int i = 0; i < strokes; ++i)
            print.line(corner(i), corner(i + 1), .05f * s, plate::yellow, i == 0);
        return;
    }
    print.ring({}, s * (1.02f + .55f * left), .035f * s, plate::paper);
}
// The held drawing's boil: three places, two poses each, once it has landed.
struct SenseBoil {
    float turn;
    Flat shift;
};
SenseBoil senseBoil(int pose) {
    static constexpr SenseBoil boils[3] = {{0, {}}, {-1.2f, {.02f, .03f}}, {1, {-.03f, -.02f}}};
    return boils[pose < 3 ? 0 : (pose / 2) % 3];
}
// One Spider-Sense mark: its ring, its lines (a jump's under a chevron), or
// as a counter breaks it.
void senseMark(Print& print, SenseAnswer answer, float s, float left, bool timed, float age, float broken) {
    const Vec3 color = senseColor(answer);
    if (broken >= 0) {
        switch (static_cast<int>(broken / plate::pose)) {
        case 0:
            print.place(1.2f, 0);
            print.layers(.035f * s, 0, {}, plate::misprint * s, [&] { senseLines(print, s, plate::paper); });
            break;
        case 1:
            print.place(1, 0);
            print.layers(.025f * s, 0, {}, 0, [&] { senseLines(print, s, color, .5f, 1.6f); });
            break;
        default:
            print.place(.6f, 0);
            print.layers(.035f * s, 0, {}, 0, [&] { senseLines(print, s, color); });
            break;
        }
        return;
    }
    const Landing land = landing(age);
    // The ring stands still while the lines boil.
    print.place(land.scale, land.turn);
    if (timed)
        print.layers(.03f * s, 0, {}, 0, [&] { senseRing(print, s, std::clamp(left, 0.f, 1.f)); });
    const SenseBoil boil = senseBoil(static_cast<int>(age / plate::pose));
    print.place(land.scale, land.turn + boil.turn * plate::degree, boil.shift * s);
    print.layers(.035f * s, .055f * s, plate::ink, land.hit ? plate::misprint * s : 0, [&] {
        senseLines(print, s, color);
        if (answer == SenseAnswer::Jump) {
            print.line({-.22f * s, .66f * s}, {0, .86f * s}, .06f * s, plate::paper);
            print.line({0, .86f * s}, {.22f * s, .66f * s}, .06f * s, plate::paper, false);
        }
    });
}
bool senseShows(SenseAnswer answer, float age, float left, float broken, float pixelAngle) {
    return answer != SenseAnswer::None && std::isfinite(age) && age >= 0 && std::isfinite(left) &&
           std::isfinite(broken) && broken < SenseView::brokenSeconds && std::isfinite(pixelAngle) &&
           pixelAngle > 0;
}
} // namespace
void appendSense(std::vector<Vertex>& out, const SenseView& sense, Vec3 viewer, float pixelAngle,
                 UiStyle style) {
    if (style == UiStyle::tech)
        return tech::appendSense(out, sense, viewer, pixelAngle);
    const float away = length(viewer - sense.point);
    if (!finite(sense.point) || !finite(viewer) || !(away >= .3f && away <= 60) ||
        !senseShows(sense.answer, sense.age, sense.left, sense.broken, pixelAngle))
        return;
    // An arm's length away the mark is a hand wide; farther off it keeps
    // most of its size on screen (six degrees across). It stands clear over
    // his head.
    const float s = .055f * std::clamp(away / .9f, 1.4f, 9.f);
    const Vec3 centre = sense.point + Vec3{0, .14f + .5f * s, 0};
    const float distance = length(viewer - centre);
    if (!(distance >= .15f))
        return;
    // Level and upright across the line of sight, as the bursts are.
    const Vec3 view = (viewer - centre) / distance;
    Vec3 right = cross(Vec3{0, 1, 0}, view);
    right = length(right) > .1f ? normalized(right) : perpendicular(view);
    const Vec3 up = cross(view, right);
    Print print(out, centre, right, up, view, pixelAngle * distance);
    senseMark(print, sense.answer, s, sense.left, sense.timed, sense.age, sense.broken);
}
void appendSenseSight(std::vector<Vertex>& out, const SenseSightView& view, Pose head, float pixelAngle,
                      const SenseSightConfig& config, UiStyle style) {
    if (style == UiStyle::tech)
        return tech::appendSenseSight(out, view, head, pixelAngle, config);
    const float across = std::sqrt(view.sight.x * view.sight.x + view.sight.y * view.sight.y);
    if (!view.sight.shown || !finite(head.position) || !(across > .5f && across < 2) ||
        !senseShows(view.answer, view.age, view.left, view.broken, pixelAngle))
        return;
    const Quat q = head.orientation;
    const Vec3 forward = q.rotate({0, 0, -1}), right = q.rotate({1, 0, 0}), up = q.rotate({0, 1, 0});
    if (!finite(forward) || !finite(right) || !finite(up))
        return;
    const Vec3 way = (right * view.sight.x + up * view.sight.y) / across;
    // A metre off, upright as the view is.
    constexpr float distance = 1, s = .032f;
    const auto at = [&](float angle) {
        return head.position + (forward * std::cos(angle) + way * std::sin(angle)) * distance;
    };
    {
        const Vec3 centre = at(config.angle), facing = normalized(head.position - centre);
        const Vec3 r = normalized(cross(up, facing)), u = cross(facing, r);
        Print print(out, centre, r, u, facing, pixelAngle * distance);
        senseMark(print, view.answer, s, view.left, view.timed, view.age, view.broken);
        if (view.broken < 0) {
            // The arrowhead beyond the ring, pointing to him.
            const float fx = dot(way, r), fy = dot(way, u), f = std::sqrt(fx * fx + fy * fy);
            const Flat ahead{fx / std::max(f, 1e-4f), fy / std::max(f, 1e-4f)}, side{-ahead.y, ahead.x};
            const Landing land = landing(view.age);
            print.place(land.scale, 0);
            print.layers(.035f * s, .055f * s, plate::ink, 0, [&] {
                print.triangle(ahead * (1.95f * s) + side * (.5f * s), ahead * (2.75f * s),
                               ahead * (1.95f * s) - side * (.5f * s), senseColor(view.answer));
            });
        }
    }
    if (view.broken >= 0)
        return;
    // The rim's lines: three, wavy, out toward him, boiling.
    const Vec3 centre = at(config.rim), facing = normalized(head.position - centre);
    const Vec3 radial = normalized(way - facing * dot(way, facing));
    const Vec3 tangent = normalized(cross(radial, facing));
    if (!finite(radial) || !finite(tangent))
        return;
    Print print(out, centre, tangent, radial, facing, pixelAngle * distance);
    const SenseBoil boil = senseBoil(static_cast<int>(view.age / plate::pose));
    const Landing land = landing(view.age);
    print.place(land.scale, boil.turn * plate::degree, boil.shift * .02f);
    const Vec3 color = senseColor(view.answer);
    print.layers(.004f, .006f, plate::ink, 0, [&] {
        for (int i = -1; i <= 1; ++i) {
            const float tilt = static_cast<float>(i) * 14 * plate::degree;
            const Flat dir{std::sin(tilt), std::cos(tilt)}, side{dir.y, -dir.x};
            const Flat base{static_cast<float>(i) * .06f, -.04f};
            Flat before = base;
            for (int k = 1; k <= 6; ++k) {
                const float t = static_cast<float>(k) / 6;
                const Flat p = base + dir * (.085f * t) + side * (.009f * std::sin(t * 3 * pi));
                print.line(before, p, .0055f, color, k == 1);
                before = p;
            }
        }
    });
}
namespace {
// A gadget's picture on its badge, `s` the badge's radius, in strokes of
// `color`: what the gadget does, as a sign painter would draw it.
void gadgetPicture(Print& print, Gadget g, Flat at, float s, Vec3 color) {
    const auto p = [&](float x, float y) { return Flat{at.x + x * s, at.y + y * s}; };
    const auto round = [&](float r, float a) { return p(r * std::cos(a), r * std::sin(a)); };
    const float h = .085f * s; // a stroke's half width
    switch (g) {
    case Gadget::webShooter:
        // A web: six spokes and two rounds strung between them.
        for (int k = 0; k < 6; ++k) {
            const float a = pi / 2 + pi / 3 * static_cast<float>(k), b = a + pi / 3;
            print.line(at, round(.68f, a), h * .7f, color);
            print.line(round(.34f, a), round(.34f, b), h * .55f, color);
            print.line(round(.62f, a), round(.62f, b), h * .55f, color);
        }
        break;
    case Gadget::impactWeb:
        // The burst of a web ball that lands hard.
        print.star(at, .3f * s, .68f * s, 8, .4f, color);
        break;
    case Gadget::webBomb:
        // A round bomb, its fuse alight.
        print.disc(p(-.08f, -.12f), .42f * s, color);
        print.line(p(.2f, .2f), p(.34f, .4f), h, color);
        print.star(p(.4f, .48f), .06f * s, .18f * s, 6, 1.1f, color);
        break;
    case Gadget::electricWeb:
        // A bolt.
        print.line(p(.26f, .66f), p(-.18f, .06f), h * 1.4f, color);
        print.line(p(-.18f, .06f), p(.18f, .06f), h * 1.4f, color, false);
        print.line(p(.18f, .06f), p(-.26f, -.66f), h * 1.4f, color, false);
        break;
    case Gadget::tripMine:
        // A mine on a wall, its line across to the next.
        print.disc(p(-.36f, 0), .22f * s, color);
        print.line(p(-.36f, 0), p(.56f, 0), h * .7f, color);
        print.line(p(.56f, -.28f), p(.56f, .28f), h, color);
        break;
    case Gadget::concussionBlast:
        // A blast of air, its rings going out from where it went off.
        print.disc(p(-.44f, 0), .12f * s, color);
        for (int k = 0; k < 3; ++k)
            print.ring(p(-.44f, 0), (.34f + .22f * static_cast<float>(k)) * s, h * .75f, color, -.9f, .9f);
        break;
    case Gadget::spiderDrone: {
        // A spider: its body, its head and eight legs.
        print.disc(p(0, -.1f), .2f * s, color);
        print.disc(p(0, .16f), .12f * s, color);
        static constexpr Flat knees[4] = {{.3f, .24f}, {.36f, .1f}, {.36f, -.1f}, {.28f, -.26f}};
        static constexpr Flat feet[4] = {{.44f, .52f}, {.62f, .16f}, {.6f, -.24f}, {.44f, -.56f}};
        for (const float side : {-1.f, 1.f})
            for (int k = 0; k < 4; ++k) {
                const Flat knee = p(side * knees[k].x, knees[k].y), foot = p(side * feet[k].x, feet[k].y);
                print.line(p(side * .1f, -.04f), knee, h * .5f, color);
                print.line(knee, foot, h * .5f, color, false);
            }
        break;
    }
    case Gadget::suspensionMatrix:
        // The field that lifts what it catches: an orb, two orbits crossing
        // round it.
        print.disc(at, .2f * s, color);
        for (const float tilt : {.45f, -.45f}) {
            constexpr int pieces = 18;
            Flat from{};
            for (int k = 0; k <= pieces; ++k) {
                const float t = 2 * pi * static_cast<float>(k) / pieces;
                const float x = .64f * std::cos(t), y = .24f * std::sin(t);
                const Flat to =
                    p(x * std::cos(tilt) - y * std::sin(tilt), x * std::sin(tilt) + y * std::cos(tilt));
                if (k)
                    print.line(from, to, h * .55f, color, k == 1);
                from = to;
            }
        }
        break;
    default:
        break;
    }
}
} // namespace
void appendGadgetWheel(std::vector<Vertex>& out, const GadgetWheelView& view, Vec3 viewer, float pixelAngle,
                       UiStyle style) {
    if (style == UiStyle::tech)
        return tech::appendGadgetWheel(out, view, viewer, pixelAngle);
    const WheelFrame& f = view.frame;
    const float distance = length(viewer - f.centre);
    if (!view.open || !finite(f.centre) || !finite(f.right) || !finite(f.up) || !finite(f.out) ||
        !finite(viewer) || !std::isfinite(pixelAngle) || pixelAngle <= 0 || !std::isfinite(view.age) ||
        view.age < 0 || !(distance >= .05f && distance <= 5))
        return;
    // Sizes in metres in the wheel's plane: the places' ring, a badge, the lit
    // one, a bead where a place is empty, a charge's pip.
    constexpr float radius = .13f, badge = .03f, litBadge = .038f, bead = .006f, pip = .0042f;
    const float px = pixelAngle * distance;
    const int lit = view.lit >= 0 && view.lit < gadgetPlaces &&
                            view.places[static_cast<size_t>(view.lit)] != Gadget::none
                        ? view.lit
                        : -1;
    const auto place = [](int p, float r) {
        return Flat{r * std::sin(gadgetPlaceAngle(p)), r * std::cos(gadgetPlaceAngle(p))};
    };
    const auto spent = [&](int p) {
        return view.most[static_cast<size_t>(p)] && !view.ammo[static_cast<size_t>(p)];
    };
    // A badge's charges round its outer side, from `centre` (its own `s` the
    // badge's radius): yellow left, grey spent; eight at most.
    const auto charges = [&](Print& print, int p, Flat centre, float s) {
        const int most = std::min<int>(view.most[static_cast<size_t>(p)], 8);
        if (!most)
            return;
        const float outward = pi / 2 - gadgetPlaceAngle(p);
        const float step =
            std::min(21 * plate::degree, 160 * plate::degree / static_cast<float>(std::max(most - 1, 1)));
        for (int k = 0; k < most; ++k) {
            const float a = outward + step * (static_cast<float>(k) - .5f * static_cast<float>(most - 1));
            const Flat at = centre + Flat{std::cos(a), std::sin(a)} * (s + .0105f);
            if (k < view.ammo[static_cast<size_t>(p)])
                print.disc(at, pip, plate::yellow);
            else
                print.disc(at, pip * .75f, mix(plate::paper, plate::ink, .5f));
        }
    };
    // The wheel lands as the marks do, out of register on its first poses.
    const Landing land = landing(view.age);
    const float misprint = land.hit ? .006f : 0;
    Print wheel(out, f.centre, f.right, f.up, f.out, px);
    wheel.place(land.scale, land.turn);
    // The ring the places are on, a bead where one is empty, and the middle
    // while nothing is lit.
    wheel.layers(.0022f, 0, {}, misprint, [&] {
        wheel.ring({}, radius, .0016f, plate::paper);
        for (int p = 0; p < gadgetPlaces; ++p)
            if (view.places[static_cast<size_t>(p)] == Gadget::none)
                wheel.disc(place(p, radius), bead, plate::paper);
        if (lit < 0)
            wheel.disc({}, bead, plate::paper);
    });
    // The badges but the lit one, keyed with a hard drop; their charges,
    // keyed finer.
    wheel.layers(.0028f, .0045f, plate::ink, misprint, [&] {
        for (int p = 0; p < gadgetPlaces; ++p) {
            if (view.places[static_cast<size_t>(p)] == Gadget::none || p == lit)
                continue;
            const Vec3 plateColor = spent(p) ? mix(plate::paper, plate::ink, .5f)
                                    : p == view.equipped ? plate::yellow
                                                         : plate::paper;
            wheel.disc(place(p, radius), badge, plateColor);
        }
    });
    wheel.layers(.0012f, .002f, plate::ink, misprint, [&] {
        for (int p = 0; p < gadgetPlaces; ++p)
            if (view.places[static_cast<size_t>(p)] != Gadget::none && p != lit)
                charges(wheel, p, place(p, radius), badge);
    });
    for (int p = 0; p < gadgetPlaces; ++p)
        if (view.places[static_cast<size_t>(p)] != Gadget::none && p != lit)
            gadgetPicture(wheel, view.places[static_cast<size_t>(p)], place(p, radius), badge, plate::ink);
    if (lit < 0)
        return;
    const Gadget chosen = view.places[static_cast<size_t>(lit)];
    // The lit badge: red and larger. It snaps on each time another gadget
    // lights - a pose wide and out of register, a pose short - and then
    // boils, as a held drawing does: drawn in three places by turns, two
    // poses each.
    const float litFor = std::isfinite(view.litAge) ? std::max(view.litAge, 0.f) : 1.f;
    const int pose = static_cast<int>(litFor / plate::pose);
    const Landing pop = pose == 0 ? Landing{1.16f, -4 * plate::degree, true}
                        : pose == 1 ? Landing{.95f, 2 * plate::degree, false}
                                    : Landing{};
    struct Boil {
        float turn;
        Flat shift;
    };
    static constexpr Boil boils[3] = {{0, {}}, {-.8f, {.015f, .022f}}, {.7f, {-.022f, -.015f}}};
    const Boil& boil = boils[pose < 3 ? 0 : (pose / 2) % 3];
    // Where the wheel's landing has its place.
    const float c = std::cos(land.turn), s = std::sin(land.turn);
    const Flat at = place(lit, radius);
    const Vec3 centre =
        f.centre + (f.right * (at.x * c - at.y * s) + f.up * (at.x * s + at.y * c)) * land.scale;
    Print badgePrint(out, centre, f.right, f.up, f.out, px);
    badgePrint.place(land.scale * pop.scale, land.turn + pop.turn + boil.turn * plate::degree,
                     boil.shift * (litBadge * land.scale));
    badgePrint.layers(.0034f, .0055f, plate::ink, pop.hit ? .006f : 0, [&] {
        badgePrint.disc({}, litBadge, plate::red);
        if (lit == view.equipped)
            badgePrint.ring({}, litBadge + .0048f, .0024f, plate::yellow);
    });
    const float pipsFrom = litBadge + (lit == view.equipped ? .0065f : 0.f);
    badgePrint.layers(.0012f, .002f, plate::ink, pop.hit ? .006f : 0,
                      [&] { charges(badgePrint, lit, {}, pipsFrom); });
    badgePrint.layers(.0016f, 0, {}, 0,
                      [&] { gadgetPicture(badgePrint, chosen, {}, litBadge, plate::paper); });
    // Its name in a yellow caption across the middle, leaning as its
    // lettering does, snapping on with the badge.
    const char* name = gadgetName(chosen);
    const float height = fittedHeight(name, .0135f, .15f);
    const float width = textWidth(name, height) + Print::lean * height;
    const float halfWidth = width * .5f + .008f, halfHeight = height * .5f + .0075f;
    const float slant = Print::lean * halfHeight;
    Print caption(out, f.centre, f.right, f.up, f.out, px);
    caption.place(land.scale * pop.scale, land.turn + pop.turn + 2 * plate::degree);
    caption.layers(.003f, .0045f, plate::ink, pop.hit ? .005f : 0, [&] {
        const Flat a{-halfWidth - slant, -halfHeight}, b{halfWidth - slant, -halfHeight},
            d{halfWidth + slant, halfHeight}, e{-halfWidth + slant, halfHeight};
        caption.triangle(a, b, d, plate::yellow);
        caption.triangle(a, d, e, plate::yellow);
    });
    caption.text(name, {width * -.5f, height * -.5f}, height, .17f, plate::ink);
}
void AimMarkerMotion::reset() {
    *this = {};
}
void AimMarkerMotion::begin(int64_t timeNs) {
    seconds_ = 0;
    if (timeNs <= time_)
        return; // the same image again
    if (time_ && static_cast<double>(timeNs - time_) * 1e-9 > gapSeconds)
        reset();
    else if (time_)
        seconds_ = static_cast<float>(static_cast<double>(timeNs - time_) * 1e-9);
    time_ = timeNs;
}
Vec3 AimMarkerMotion::aim(Vec3 direction, float yaw) {
    if (!std::isfinite(yaw))
        return direction;
    return aim(direction, Quat::yaw(yaw));
}
Vec3 AimMarkerMotion::aim(Vec3 direction, Quat turn) {
    const float n = turn.x * turn.x + turn.y * turn.y + turn.z * turn.z + turn.w * turn.w;
    if (!finite(direction) || length(direction) < .5f || !std::isfinite(n) || std::abs(n - 1) > .01f)
        return direction;
    const Vec3 raw = turn.conjugate().rotate(normalized(direction));
    if (!filtering_) {
        steady_ = raw;
        speed_ = {};
        filtering_ = true;
    } else if (seconds_ > 0) {
        // The share of the way to the raw direction a low-pass at `hz` takes
        // in this image's time; the cutoff rises with the aim's speed.
        const auto share = [dt = seconds_](float hz) { return dt / (dt + 1 / (2 * pi * hz)); };
        speed_ += ((raw - steady_) / seconds_ - speed_) * share(speedCutoffHz);
        const float cutoff = minCutoffHz + speedCutoff * std::max(length(speed_) - speedDeadband, 0.f);
        steady_ = normalized(steady_ + (raw - steady_) * share(cutoff));
    }
    return turn.rotate(steady_);
}
void AimMarkerMotion::markers(const AimMarker* wanted, Vec3 origin, Vec3 direction, std::vector<AimMarker>& out) {
    const bool ray = finite(origin) && finite(direction) && length(direction) > .5f;
    if (wanted && (wanted->kind > AimMark::target || !finite(wanted->point) || (wanted->kind != AimMark::target && !ray)))
        wanted = nullptr;
    direction = normalized(direction);
    const float dt = seconds_;
    // The share of the way to a new value an ease over `seconds` takes now.
    const auto ease = [dt](float seconds) { return 1 - std::exp(-dt / seconds); };
    for (unsigned k = 0; k < kinds_.size(); ++k) {
        auto& shown = kinds_[k];
        const bool target = k == static_cast<unsigned>(AimMark::target);
        if (wanted && static_cast<unsigned>(wanted->kind) == k) {
            const bool fresh = shown.weight <= 0;
            const AimMarker before = shown.marker;
            shown.marker = *wanted;
            if (target && !fresh) {
                // The ring slides to where the target is now, or to the next one.
                shown.marker.point = mix(before.point, wanted->point, ease(targetSeconds));
                shown.marker.radius = before.radius + (wanted->radius - before.radius) * ease(targetSeconds);
            } else if (!target) {
                // A marker on the ray eases to a new distance (its depth).
                const float distance = std::max(length(wanted->point - origin), .01f);
                shown.distance = fresh || !(shown.distance > 0)
                                     ? distance
                                     : std::exp(std::log(shown.distance) +
                                                (std::log(distance) - std::log(shown.distance)) * ease(distanceSeconds));
                shown.marker.point = origin + direction * shown.distance;
            }
            // A new marker shows at once, faintly, and is whole within fadeInSeconds.
            shown.weight = fresh ? .35f : std::min(1.f, shown.weight + dt / fadeInSeconds);
            shown.lock = fresh ? 0.f : std::min(1.f, shown.lock + dt / lockSeconds);
        } else if (shown.weight > 0) {
            shown.weight = std::max(0.f, shown.weight - dt / fadeOutSeconds);
            shown.lock = std::min(shown.lock, shown.weight); // a target's ring widens as it goes
        }
        if (shown.weight <= 0)
            continue;
        // A marker fading out stays where it was last wanted: where the web
        // just went, or where the aim left it.
        AimMarker marker = shown.marker;
        marker.opacity = shown.weight;
        marker.lock = target ? shown.lock : 1.f;
        marker.spin = spin_;
        out.push_back(marker);
    }
    // The target's ring turns, faster as the grip closes.
    const float squeeze = kinds_[static_cast<unsigned>(AimMark::target)].marker.squeeze;
    spin_ = std::fmod(spin_ + dt * (.8f + 2.4f * std::clamp(std::isfinite(squeeze) ? squeeze : 0.f, 0.f, 1.f)),
                      2 * pi);
}
} // namespace spidy
