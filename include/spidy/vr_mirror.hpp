#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
//
// Update 37: the MIRROR (wrist hologram page 2; live.txt mirror, mirror_distance,
// mirror_size, mirror_spot, mirror_anchor, mirror_style), for recording: the player sees
// their own head and hands move, reflected, floating in front with the city visible
// behind it (no background: only the figure and a thin frame are drawn).
//
// What it shows, honestly: the TRACKED poses - the headset and both controllers - drawn as a
// Spider-Man figure (head with lenses that turn with yours, a torso and arms placed from the
// head and hands, gloved hands that turn with the controllers), mirrored across the mirror's
// plane. It is NOT the game's own rendered body: that needs a third game view (a camera facing
// the player, rendered with a player-only mask), whose Spider-Man 2 addresses are not
// evidenced, and it would cost about as much again as one eye (at about 39 fps, a fall to
// the 20s). Faces are not tracked over Virtual Desktop here, so talking does not show.
//
// Update 38.3 MIRROR LOOK SUIT (the October 24 steering, "read what suit I have equipped and
// mirror my motions"), honestly: plan B. The motions are the native_body solved skeleton (16
// joints, as 38.2). The equipped suit's ID is NOT read: no address or save field for it is
// evidenced. Its COLOURS are: SuitColours learns them from the game's own rendered suit, the
// pixels at the hero's projected limbs in our left-eye snapshot copies (RGBA8, every 5 s),
// pooled in a hue histogram; until it has, the classic red / black with a white spider. The
// figure is a Spider-Man (V torso, web lines, spider emblem, lenses); a suit's complex pattern is
// reduced to two colours and the emblem. Plan A (the game's own body in a third view) stays
// future work, unevidenced as above.
//
// Placement (the October 24 steering): it never follows the gaze. RECENTRE puts it in front
// of where the head faces then (level), and there it stays:
// - PLAYSPACE (default): fixed in the player's tracking space, so it travels with the player
//   across the city (swinging, crawling) while turning the head does not move it;
// - WORLD: pinned to that spot in the city.
// Distance, size and spot (left/centre/right) apply live, about the placed facing.
// Cost: nothing at all while off (no vertex is made); on, a few hundred quads, measured
// each frame and shown on its holo button and under the mirror in milliseconds.
// Plan B: mirror_style 1 PANEL draws a dark see-through backing behind the figure (a
// picture-in-picture look, easier to read against a bright city).
#include "soft_shapes.hpp"
#include "math.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace spidy::vr_mirror {
enum Spot : uint32_t { left = 0, centre = 1, right = 2, spots = 3 };
enum Anchor : uint32_t { playspace = 0, world = 1, anchors = 2 };
enum Style : uint32_t { figure = 0, panel = 1, styles = 2 };
inline constexpr const char* spotNames[spots] = {"LEFT", "CENTRE", "RIGHT"};
inline constexpr const char* anchorNames[anchors] = {"PLAYSPACE", "WORLD"};
inline constexpr const char* styleNames[styles] = {"FIGURE", "PANEL"};
struct Config {
    bool on = false;              // mirror (default off: zero cost)
    uint32_t distanceCm = 120;    // mirror_distance, 50-300
    uint32_t sizePercent = 100;   // mirror_size, 25-200 (100: life size)
    uint32_t spot = centre;       // mirror_spot
    uint32_t anchor = playspace;  // mirror_anchor
    uint32_t style = figure;      // mirror_style (1 PANEL: plan B)
    static constexpr uint32_t minDistance = 50, maxDistance = 300, minSize = 25, maxSize = 200;
    static constexpr float spotSide = .6f; // metres aside for LEFT and RIGHT
};
struct SuitPalette {
    Vec3 primary{.62f, .03f, .04f};   // linear RGB; the default: red ...
    Vec3 secondary{.02f, .02f, .03f}; // ... and black ...
    Vec3 emblem{.85f, .85f, .88f};    // ... with a white spider, until the suit's are learned
    bool learned{};
};
// The player's frame this image: the playspace's place in the world.
struct Frame {
    Vec3 anchor{};     // the player's feet (GameTrackingRig::Output::anchor), world
    Quat turn{};       // tracking space -> world (Swing's trackingTurn)
    Pose head{};       // world
    bool handTracked[2]{};
    Pose hands[2]{};   // world grips
    // Update 38.2 MIRROR LOOK SUIT: the hero's own solved joints (native_body::suit, world,
    // in native_body::SuitJoint order); none (suit false): the FIGURE from the tracked poses.
    bool suit{};
    Vec3 joints[16]{};
    SuitPalette palette{}; // Update 38.3: the suit's colours (learned from the eye, or the default)
};
inline constexpr uint32_t suitJointCount = 16;
// Where it was put, in the anchor's frame (playspace: about the feet, tracking axes).
struct Placement {
    bool placed{};
    uint32_t anchor{};
    Vec3 eye{};        // the head at RECENTRE
    Vec3 ahead{};      // level forward at RECENTRE, unit
};
inline bool usable(const Frame& f) {
    return finite(f.anchor) && finite(f.head.position) && std::isfinite(f.turn.w);
}
inline Vec3 toWorld(const Placement& p, const Frame& f, Vec3 v, bool direction) {
    if (p.anchor == world)
        return v;
    return direction ? f.turn.rotate(v) : f.anchor + f.turn.rotate(v);
}
inline Vec3 fromWorld(uint32_t anchor, const Frame& f, Vec3 v, bool direction) {
    if (anchor == world)
        return v;
    const Quat back = f.turn.conjugate();
    return direction ? back.rotate(v) : back.rotate(v - f.anchor);
}
inline Placement recentre(const Frame& f, uint32_t anchor) {
    Placement p;
    if (!usable(f))
        return p;
    const Vec3 up = anchor == world ? Vec3{0, 1, 0} : f.turn.rotate({0, 1, 0});
    Vec3 forward = f.head.orientation.rotate({0, 0, -1});
    forward = forward - up * dot(forward, up);
    if (length(forward) < 1e-3f)
        forward = f.head.orientation.rotate({0, 1, 0}) - up * dot(f.head.orientation.rotate({0, 1, 0}), up);
    if (length(forward) < 1e-3f)
        return p;
    p.placed = true;
    p.anchor = anchor < anchors ? anchor : playspace;
    p.eye = fromWorld(p.anchor, f, f.head.position, false);
    p.ahead = normalized(fromWorld(p.anchor, f, normalized(forward), true));
    return p;
}
struct Plane {
    bool valid{};
    Vec3 centre{}, normal{}, up{}, right{}; // normal toward the player; right as the player sees it
    float scale = 1;
};
inline Plane plane(const Placement& p, const Frame& f, const Config& c) {
    Plane m;
    if (!p.placed || !usable(f))
        return m;
    const Vec3 eye = toWorld(p, f, p.eye, false), ahead = normalized(toWorld(p, f, p.ahead, true));
    const Vec3 up = p.anchor == world ? Vec3{0, 1, 0} : normalized(f.turn.rotate({0, 1, 0}));
    if (!finite(eye) || length(ahead) < .5f || length(up) < .5f)
        return m;
    const float distance =
        static_cast<float>(std::clamp(c.distanceCm, Config::minDistance, Config::maxDistance)) / 100.f;
    const Vec3 right = normalized(cross(ahead, up));
    const float side = c.spot == Spot::left ? -Config::spotSide : c.spot == Spot::right ? Config::spotSide : 0.f;
    m.valid = true;
    m.centre = eye + ahead * distance + right * side;
    m.normal = ahead * -1.f;
    m.up = up;
    m.right = right;
    m.scale = static_cast<float>(std::clamp(c.sizePercent, Config::minSize, Config::maxSize)) / 100.f;
    return m;
}
// A point reflected in the plane, then scaled about the mirror's centre.
inline Vec3 reflect(const Plane& m, Vec3 p) {
    const Vec3 r = p - m.normal * (2 * dot(p - m.centre, m.normal));
    return m.centre + (r - m.centre) * m.scale;
}
inline Vec3 reflectDirection(const Plane& m, Vec3 d) { return d - m.normal * (2 * dot(d, m.normal)); }

// Update 38.3 SUIT COLOURS: the suit's colours as the game itself drew it. What is read,
// honestly: NOT the equipped suit's identity - no address or save field holding it is
// evidenced for Spider-Man 2, so none is read or guessed. Instead the session report's own
// copy of the left eye (EyeSnapshotSchedule, every 5 s, already taken and read back on the
// CPU since Update 19) is sampled at the hero's own forearms, upper arms, thighs, shins
// and belly, projected with that eye's own view-projection: the pixels the game rendered of
// the equipped suit. A colour histogram over many copies keeps what stays the same (the
// suit) and outvotes what changes (the city behind a limb that was off screen or hidden).
inline float toLinear(unsigned char c) {
    const float v = static_cast<float>(c) / 255.f;
    return v <= .04045f ? v / 12.92f : std::pow((v + .055f) / 1.055f, 2.4f);
}
inline unsigned char toSrgb(float v) {
    v = std::clamp(v, 0.f, 1.f);
    const float e = v <= .0031308f ? v * 12.92f : 1.055f * std::pow(v, 1 / 2.4f) - .055f;
    return static_cast<unsigned char>(std::lround(e * 255));
}
inline uint32_t packSrgb(Vec3 c) {
    return static_cast<uint32_t>(toSrgb(c.x)) << 16 | static_cast<uint32_t>(toSrgb(c.y)) << 8 | toSrgb(c.z);
}
// The median colour of the 5 x 5 pixels about (x, y) in RGBA8 rows, linear; false off the
// image (a 4 pixel margin) or across an edge (the patch's brightness spread over .3), where
// the limb's outline or a seam would mix in what is behind it.
inline bool samplePatch(const unsigned char* rows, unsigned width, unsigned height, unsigned pitch, float x,
                        float y, Vec3& colour) {
    if (!rows || !std::isfinite(x) || !std::isfinite(y) || x < 4 || y < 4 || x >= static_cast<float>(width) - 4.f || y >= static_cast<float>(height) - 4.f)
        return false;
    const int cx = static_cast<int>(x), cy = static_cast<int>(y);
    float r[25], g[25], b[25], luma[25];
    int n = 0;
    for (int dy = -2; dy <= 2; ++dy)
        for (int dx = -2; dx <= 2; ++dx, ++n) {
            const unsigned char* p = rows + static_cast<size_t>(cy + dy) * pitch + static_cast<size_t>(cx + dx) * 4;
            r[n] = toLinear(p[0]);
            g[n] = toLinear(p[1]);
            b[n] = toLinear(p[2]);
            luma[n] = .2126f * r[n] + .7152f * g[n] + .0722f * b[n];
        }
    const auto [lo, hi] = std::minmax_element(luma, luma + 25);
    if (*hi - *lo > .3f)
        return false;
    std::nth_element(r, r + 12, r + 25);
    std::nth_element(g, g + 12, g + 25);
    std::nth_element(b, b + 12, b + 25);
    colour = {r[12], g[12], b[12]};
    return true;
}
// Where on the solved skeleton (native_body::SuitJoint order) the suit is sampled: the
// middle of each upper arm, thigh and shin, each forearm 40% from the elbow (clear of the
// wrist ring and the hand's blood), and the belly; with their weights.
inline constexpr uint32_t suitSamplePoints = 9;
inline void samplePoints(const Vec3 (&j)[16], Vec3 (&out)[suitSamplePoints]) {
    enum { head, neck, chest, pelvis, shL, elL, haL, shR, elR, haR, hipL, knL, ftL, hipR, knR, ftR };
    out[0] = (j[shL] + j[elL]) * .5f;
    out[1] = (j[shR] + j[elR]) * .5f;
    out[2] = j[elL] + (j[haL] - j[elL]) * .4f;
    out[3] = j[elR] + (j[haR] - j[elR]) * .4f;
    out[4] = (j[hipL] + j[knL]) * .5f;
    out[5] = (j[hipR] + j[knR]) * .5f;
    out[6] = (j[knL] + j[ftL]) * .5f;
    out[7] = (j[knR] + j[ftR]) * .5f;
    out[8] = (j[chest] + j[pelvis]) * .5f;
}
// The histogram: 12 hues x 2 brightnesses for coloured pixels, and dark / mid / light greys.
class SuitColours {
  public:
    static constexpr int hues = 12, bins = hues * 2 + 3;
    static constexpr float keep = .85f;      // each new copy: older counts kept at 85%
    static constexpr float learnAt = 24.f;   // samples (weighted) before the colours are trusted
    static constexpr float primaryShare = .3f, secondaryShare = .1f;
    static int bin(Vec3 c) {
        const float hi = std::max({c.x, c.y, c.z}), lo = std::min({c.x, c.y, c.z});
        const float sat = hi > 1e-4f ? (hi - lo) / hi : 0;
        if (sat < .3f || hi < .02f)
            return hues * 2 + (hi < .05f ? 0 : hi < .4f ? 1 : 2);
        float h;
        if (hi == c.x)
            h = std::fmod((c.y - c.z) / (hi - lo) + 6, 6.f);
        else if (hi == c.y)
            h = (c.z - c.x) / (hi - lo) + 2;
        else
            h = (c.x - c.y) / (hi - lo) + 4;
        const int hue = std::clamp(static_cast<int>(h / 6 * hues + .5f) % hues, 0, hues - 1);
        return hue * 2 + (hi < .25f ? 0 : 1);
    }
    // A new eye copy: older counts fade so a suit change is followed within about a minute.
    void beginCopy() {
        for (int i = 0; i < bins; ++i) {
            weight_[i] *= keep;
            sum_[i] = sum_[i] * keep;
        }
        ++copies_;
    }
    void add(Vec3 linear) {
        if (!finite(linear))
            return;
        const int b = bin(linear);
        weight_[b] += 1;
        sum_[b] = sum_[b] + linear;
        ++samples_;
    }
    // Update 38.5: another hero (a Peter / Miles switch): his suit is learned afresh, at once,
    // instead of fading the last one's out over a minute.
    void forget() {
        for (int i = 0; i < bins; ++i) {
            weight_[i] = 0;
            sum_[i] = {};
        }
        ++forgets_;
    }
    uint64_t forgets() const { return forgets_; }
    uint64_t samples() const { return samples_; }
    uint64_t copies() const { return copies_; }
    SuitPalette palette() const {
        SuitPalette p;
        float total = 0;
        for (int i = 0; i < bins; ++i)
            total += weight_[i];
        if (total < learnAt)
            return p;
        int first = 0;
        for (int i = 1; i < bins; ++i)
            if (weight_[i] > weight_[first])
                first = i;
        if (weight_[first] < primaryShare * total)
            return p;
        // The second: the heaviest bin of another hue (or another grey).
        const auto hueOf = [](int b) { return b < hues * 2 ? b / 2 : 100 + b; };
        int second = -1;
        for (int i = 0; i < bins; ++i)
            if (hueOf(i) != hueOf(first) && (second < 0 || weight_[i] > weight_[second]))
                second = i;
        p.learned = true;
        p.primary = sum_[first] / weight_[first];
        p.secondary = second >= 0 && weight_[second] >= secondaryShare * total ? sum_[second] / weight_[second]
                                                                              : p.primary * .2f;
        // The emblem: a light grey seen on the suit, else white on a dark second colour, else
        // the second colour darkened.
        const int light = hues * 2 + 2;
        const float luma2 = .2126f * p.secondary.x + .7152f * p.secondary.y + .0722f * p.secondary.z;
        if (light != first && weight_[light] >= .05f * total)
            p.emblem = sum_[light] / weight_[light];
        else
            p.emblem = luma2 < .08f ? Vec3{.85f, .85f, .88f} : p.secondary * .15f;
        return p;
    }

  private:
    float weight_[bins]{};
    Vec3 sum_[bins]{};
    uint64_t samples_{}, copies_{}, forgets_{};
};

// Update 38.2 SUIT, redrawn in 38.3 as a Spider-Man: the hero's skeleton as the game's body
// solved it, reflected, every part a flat soft shape facing the player. A V torso in the
// suit's first colour over its second (the flanks), arms and legs the same way (the second
// colour down their sides), web lines over the first colour, the spider on the chest in the
// emblem colour, gloves and boots. The head (with its lenses) is drawn by append(). About
// 140 quads more than FIGURE's.
inline bool suitUsable(const Frame& f) {
    if (!f.suit)
        return false;
    for (uint32_t j = 0; j < suitJointCount; ++j)
        if (!finite(f.joints[j]))
            return false;
    return true;
}
// The web lines' colour over `c`: black over a light colour, lighter over a dark one.
inline Vec3 webLineColour(Vec3 c) {
    const float luma = .2126f * c.x + .7152f * c.y + .0722f * c.z;
    return luma < .03f ? Vec3{.16f, .16f, .18f} : c * .12f;
}
// The spider: a body and head, eight legs bent at the knee, centred at `at` facing `view`.
inline void appendSpider(std::vector<Vertex>& out, Vec3 at, Vec3 up, Vec3 right, Vec3 view, float size,
                         Vec3 colour, float soft) {
    soft::softBar(out, at - up * (size * .35f), at + up * (size * .2f), view, size * .09f, colour, .95f, soft);
    soft::softArc(out, at + up * (size * .3f), right, up, 0, size * .08f, 0, 6.2831853f, colour, .95f, soft, 10);
    static constexpr float legs[4][4] = {
        // knee (across, up), foot (across, up), in units of `size`
        {.30f, .30f, .38f, .62f}, {.34f, .10f, .50f, .25f}, {.34f, -.08f, .50f, -.22f}, {.28f, -.25f, .36f, -.58f}};
    for (const float side : {-1.f, 1.f})
        for (const auto& l : legs) {
            const Vec3 knee = at + (right * (side * l[0]) + up * l[1]) * size;
            const Vec3 foot = at + (right * (side * l[2]) + up * l[3]) * size;
            soft::softBar(out, at, knee, view, size * .035f, colour, .95f, soft);
            soft::softBar(out, knee, foot, view, size * .03f, colour, .95f, soft);
        }
}
inline void appendSuit(std::vector<Vertex>& out, const Plane& m, const Frame& f, float soft, Vec3 view) {
    const float s = m.scale;
    const SuitPalette& pal = f.palette;
    const Vec3 first = pal.primary, second = pal.secondary, lines = webLineColour(first);
    Vec3 j[suitJointCount];
    for (uint32_t i = 0; i < suitJointCount; ++i)
        j[i] = reflect(m, f.joints[i]);
    enum { head, neck, chest, pelvis, shL, elL, haL, shR, elR, haR, hipL, knL, ftL, hipR, knR, ftR };
    const auto bar = [&](Vec3 a, Vec3 b, float w, Vec3 colour, float alpha = .95f) {
        soft::softBar(out, a, b, view, w * s, colour, alpha, soft);
    };
    Vec3 up = j[neck] - j[pelvis];
    up = length(up) > 1e-3f ? normalized(up) : m.up;
    Vec3 across = j[shR] - j[shL];
    across = across - up * dot(across, up);
    across = length(across) > 1e-3f ? normalized(across) : m.right;
    const float shoulders = std::max(length(j[shR] - j[shL]) * .5f, .12f * s);
    // The torso: the second colour's flanks, the first colour's V over them.
    const Vec3 waistL = j[pelvis] - across * (.13f * s), waistR = j[pelvis] + across * (.13f * s);
    const Vec3 topL = j[chest] - across * (shoulders + .03f * s) + up * (.04f * s),
               topR = j[chest] + across * (shoulders + .03f * s) + up * (.04f * s);
    soft::softTriangle(out, {topL, topR, waistR}, view, second, .93f, .01f * s, soft);
    soft::softTriangle(out, {topL, waistR, waistL}, view, second, .93f, .01f * s, soft);
    const Vec3 vL = j[chest] - across * shoulders + up * (.04f * s), vR = j[chest] + across * shoulders + up * (.04f * s);
    const Vec3 vWaistL = j[pelvis] - across * (.07f * s), vWaistR = j[pelvis] + across * (.07f * s);
    soft::softTriangle(out, {vL, vR, vWaistR}, view, first, .96f, 0, soft);
    soft::softTriangle(out, {vL, vWaistR, vWaistL}, view, first, .96f, 0, soft);
    bar(j[neck], j[chest] + up * (.04f * s), .032f, first);
    // The belt and hips.
    bar(j[hipL], j[hipR], .075f, first);
    // Web lines over the chest: the centre seam and three bands curving down to the middle.
    bar(j[neck], j[pelvis], .005f, lines, .8f);
    for (int band = 1; band <= 3; ++band) {
        const float t = static_cast<float>(band) / 4.f;
        const Vec3 mid = j[chest] + (j[pelvis] - j[chest]) * t;
        const float half = shoulders * (1 - t * .45f);
        const Vec3 sag = up * (-.025f * s);
        bar(mid - across * half, mid + sag, .0035f, lines, .75f);
        bar(mid + sag, mid + across * half, .0035f, lines, .75f);
    }
    // The spider on the chest.
    appendSpider(out, j[chest] - up * (.06f * s), up, across, view, .16f * s, pal.emblem, soft);
    // Arms and legs: the second colour down the sides under a narrower bar of the first,
    // web lines across them; gloves and boots in the first colour.
    static constexpr int arms[2][3] = {{shL, elL, haL}, {shR, elR, haR}};
    static constexpr int legs[2][3] = {{hipL, knL, ftL}, {hipR, knR, ftR}};
    const auto limb = [&](const int (&l)[3], float upper, float lower) {
        bar(j[l[0]], j[l[1]], upper + .012f, second);
        bar(j[l[1]], j[l[2]], lower + .012f, second);
        bar(j[l[0]], j[l[1]], upper * .7f, first);
        bar(j[l[1]], j[l[2]], lower * .7f, first);
        for (int k = 0; k < 2; ++k) {
            const Vec3 a = j[l[k]], b = j[l[k + 1]];
            const Vec3 side = normalized(cross(b - a, view)) * ((k ? lower : upper) * .7f * s);
            for (const float t : {.3f, .65f}) {
                const Vec3 at = a + (b - a) * t;
                bar(at - side, at + side, .003f, lines, .7f);
            }
        }
    };
    for (const auto& arm : arms) {
        limb(arm, .05f, .042f);
        soft::softArc(out, j[arm[0]], across, up, 0, .055f * s, 0, 6.2831853f, first, .96f, soft, 14); // shoulder
    }
    for (const auto& leg : legs) {
        limb(leg, .07f, .055f);
        // The boot: the first colour from above the ankle to the foot.
        const Vec3 ankle = j[leg[1]] + (j[leg[2]] - j[leg[1]]) * .75f;
        bar(ankle, j[leg[2]], .05f, first);
    }
}
// The Spider-Man head, both looks: an oval in the suit's first colour, web lines radiating
// from between the eyes with two rings about them, and the big lenses - white, outlined in
// the second colour - turned with the headset's face.
inline void appendHead(std::vector<Vertex>& out, const Plane& m, const Frame& f, float soft) {
    const float s = m.scale;
    const Vec3 head = reflect(m, f.head.position);
    const Vec3 face = normalized(reflectDirection(m, f.head.orientation.rotate({0, 0, -1})));
    Vec3 headUp = normalized(reflectDirection(m, f.head.orientation.rotate({0, 1, 0})));
    Vec3 headRight = normalized(cross(face, headUp));
    if (!finite(headRight) || !finite(headUp)) {
        headRight = m.right;
        headUp = m.up;
    }
    // Drawn flat in the mirror's plane: its own axes, turned by the head's roll.
    Vec3 hr = headRight - m.normal * dot(headRight, m.normal), hu = headUp - m.normal * dot(headUp, m.normal);
    hr = length(hr) > .2f ? normalized(hr) : m.right;
    hu = length(hu) > .2f ? normalized(hu) : m.up;
    const Vec3 first = f.palette.primary, lines = webLineColour(first);
    const float w = .095f * s, h = .12f * s;
    soft::softArc(out, head, hr * (w / h), hu, 0, h, 0, 6.2831853f, first, .96f, soft, 28);
    const float facing = dot(face, m.normal); // 1: the face toward the player
    const Vec3 centre = head + hu * (.01f * s) + hr * (dot(face, m.right) * -.03f * s);
    // The web: 10 lines from between the eyes to the edge, and two rings.
    for (int i = 0; i < 10; ++i) {
        const float t = 6.2831853f * (static_cast<float>(i) + .5f) / 10;
        const Vec3 edge = head + (hr * (std::cos(t) * w) + hu * (std::sin(t) * h)) * .97f;
        soft::softBar(out, centre, edge, m.normal, .0025f * s, lines, .75f, soft);
    }
    for (const float k : {.45f, .75f})
        soft::softArc(out, centre, hr * (w / h), hu, h * k - .002f * s, h * k + .002f * s, 0, 6.2831853f, lines, .7f,
                      soft, 24);
    if (facing < -.2f)
        return; // the back of the head: no lenses
    const float squeeze = std::clamp(.35f + .65f * facing, .35f, 1.f);
    for (const float side : {-1.f, 1.f}) {
        const Vec3 x = hr * (side * squeeze), y = hu;
        const std::array<Vec3, 3> lens = {centre + (x * .016f + y * .018f) * s, centre + (x * .074f + y * .040f) * s,
                                          centre + (x * .040f - y * .036f) * s};
        soft::softTriangle(out, lens, m.normal, f.palette.secondary * .5f, .95f, .007f * s, soft);
        soft::softTriangle(out, lens, m.normal, {.92f, .93f, .95f}, .97f, 0, soft);
    }
}
// Appends the reflected figure (and the frame; PANEL its backing). Returns the vertices added.
inline size_t append(std::vector<Vertex>& out, const Plane& m, const Frame& f, const Config& c, Vec3 viewer) {
    if (!m.valid || !finite(viewer))
        return 0;
    const size_t from = out.size();
    const float s = m.scale, soft = .004f * s;
    const Vec3 red = f.palette.primary, blue = f.palette.secondary, frameColour{.55f, .75f, 1.f};
    const float half = .5f * s;   // the frame: about 1 m wide and 1.8 m tall at life size
    const float tall = .95f * s;
    const Vec3 c0 = m.centre - m.up * (.35f * s);
    // The backing (PANEL), behind everything else.
    if (c.style == panel) {
        const Vec3 a = c0 - m.right * half - m.up * tall, b = c0 + m.right * half - m.up * tall,
                   d = c0 + m.right * half + m.up * tall, e = c0 - m.right * half + m.up * tall;
        const Vec3 back = m.normal * -.002f;
        soft::quad(out, {a + back, {0, 0, .01f}, .55f}, {b + back, {0, 0, .01f}, .55f},
                   {d + back, {0, 0, .01f}, .55f}, {e + back, {0, 0, .01f}, .55f});
    }
    // The frame: four thin bars, so the mirror is found even with nothing in it.
    {
        const Vec3 corner[4] = {c0 - m.right * half - m.up * tall, c0 + m.right * half - m.up * tall,
                                c0 + m.right * half + m.up * tall, c0 - m.right * half + m.up * tall};
        for (int i = 0; i < 4; ++i)
            soft::softBar(out, corner[i], corner[(i + 1) % 4], m.normal, .004f * s, frameColour, .5f, soft);
    }
    const Vec3 view = m.normal; // the figure faces the player, flat across the line of sight
    const auto disc = [&](Vec3 at, float radius, Vec3 colour, float alpha) {
        soft::softArc(out, at, m.right, m.up, 0, radius, 0, 6.2831853f, colour, alpha, soft, 20);
    };
    // The head (Update 38.3: a Spider-Man's, web lines and lenses), turned with the headset.
    const Vec3 face = normalized(reflectDirection(m, f.head.orientation.rotate({0, 0, -1})));
    const Vec3 headUp = normalized(reflectDirection(m, f.head.orientation.rotate({0, 1, 0})));
    const Vec3 headRight = normalized(cross(face, headUp));
    const Vec3 head = reflect(m, f.head.position);
    // Update 38.2 SUIT: the hero's own skeleton, the gloves at its wrists.
    if (suitUsable(f)) {
        appendSuit(out, m, f, soft, view);
        for (unsigned i = 0; i < 2; ++i) {
            const Vec3 hand = reflect(m, f.joints[i == 0 ? 6 : 9]);
            disc(hand, .045f * s, red, .95f);
            if (f.handTracked[i] && finite(f.hands[i].position)) {
                const Vec3 fingers = normalized(reflectDirection(m, f.hands[i].orientation.rotate({0, 0, -1})));
                if (length(fingers) > .5f)
                    soft::softBar(out, hand, hand + fingers * (.07f * s), view, .02f * s, red, .95f, soft);
            }
        }
        appendHead(out, m, f, soft);
        return out.size() - from;
    }
    // The body from the head: the neck under it along the playspace's up, the shoulders
    // across the level facing (the head's yaw), the torso and the hips below.
    const Vec3 up = m.up;
    Vec3 bodyRight = headRight - up * dot(headRight, up);
    bodyRight = length(bodyRight) > 1e-3f ? normalized(bodyRight) : m.right;
    const Vec3 neck = head - up * (.20f * s);
    const Vec3 hips = neck - up * (.55f * s);
    const Vec3 shoulder[2] = {neck - bodyRight * (.18f * s) - up * (.04f * s),
                              neck + bodyRight * (.18f * s) - up * (.04f * s)};
    soft::softBar(out, neck, hips, view, .13f * s, blue, .9f, soft);
    soft::softBar(out, shoulder[0], shoulder[1], view, .05f * s, red, .9f, soft);
    soft::softBar(out, hips - bodyRight * (.1f * s), hips - bodyRight * (.12f * s) - up * (.75f * s), view, .06f * s,
                  blue, .85f, soft);
    soft::softBar(out, hips + bodyRight * (.1f * s), hips + bodyRight * (.12f * s) - up * (.75f * s), view, .06f * s,
                  blue, .85f, soft);
    // The arms: shoulder, an elbow bent down and out, the hand. A mirror swaps the sides:
    // the right hand's reflection is on the player's right as seen, which is the reflected
    // shoulder nearer to it.
    for (unsigned i = 0; i < 2; ++i) {
        if (!f.handTracked[i] || !finite(f.hands[i].position))
            continue;
        const Vec3 hand = reflect(m, f.hands[i].position);
        const Vec3 sh = length(hand - shoulder[0]) < length(hand - shoulder[1]) ? shoulder[0] : shoulder[1];
        const float reach = length(hand - sh), arm = .62f * s;
        const Vec3 mid = (sh + hand) * .5f;
        const float bend = reach < arm ? std::sqrt(std::max(arm * arm / 4 - reach * reach / 4, 0.f)) : 0.f;
        const Vec3 out1 = normalized(sh - neck - up * dot(sh - neck, up));
        const Vec3 elbow = mid + normalized(out1 - up) * bend;
        soft::softBar(out, sh, elbow, view, .04f * s, red, .9f, soft);
        soft::softBar(out, elbow, hand, view, .035f * s, red, .9f, soft);
        // The glove: a disc and the fingers along the grip's forward.
        const Vec3 fingers = normalized(reflectDirection(m, f.hands[i].orientation.rotate({0, 0, -1})));
        disc(hand, .045f * s, red, .95f);
        if (length(fingers) > .5f)
            soft::softBar(out, hand, hand + fingers * (.07f * s), view, .02f * s, red, .95f, soft);
    }
    appendHead(out, m, f, soft);
    return out.size() - from;
}
// The mirror's telemetry (SpidyMirrorData; tools/update37_records.py).
struct Data {
    uint32_t magic = 0x5252494d, version = 1, bytes = 72, on{}; // MIRR
    uint32_t anchor{}, spot{}, style{}, recentres{};
    uint64_t frames{}, vertices{};
    float lastMs{}, meanMs{}, peakMs{}, distance{};
    uint64_t sequence{};
};
static_assert(sizeof(Data) == 72);
} // namespace spidy::vr_mirror
