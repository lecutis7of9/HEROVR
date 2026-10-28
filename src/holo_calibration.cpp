// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
// Version 0.0.73 test 2 HOLO CALIBRATION (holo_calibration.hpp).
#include "spidy/holo_calibration.hpp"
#include "spidy/calibration_art.hpp"
#include "spidy/pixel_font.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace spidy::holo_calibration {
namespace art = calibration_art;
using body_calibration::Phase;

const char* touchText(uint32_t hint) {
    switch (hint) {
    case touchSqueeze: return "PUT A HAND ON THE SPOT, SQUEEZE GRIP";
    case touchLower: return "A LITTLE LOWER";
    case touchHigher: return "A LITTLE HIGHER";
    case touchBehind: return "REACH OVER A SHOULDER, BEHIND YOU";
    case touchSides: return "LEFT HAND LEFT HIP, RIGHT HAND RIGHT HIP";
    case touchConfirm: return "PRESS A OR BOTH TRIGGERS";
    case touchTracking: return "KEEP THE CONTROLLERS IN VIEW";
    default: return "";
    }
}

bool plausible(uint32_t step, int hand, holster::BodyPoint p, uint32_t& hint) {
    hint = touchNone;
    if (!std::isfinite(p.below) || !std::isfinite(p.side) || !std::isfinite(p.ahead)) {
        hint = touchTracking;
        return false;
    }
    switch (step) {
    case belt:
        if (p.below < .30f)
            return hint = touchLower, false;
        if (p.below > .85f)
            return hint = touchHigher, false;
        if (std::abs(p.side) > .30f || p.ahead < -.10f)
            return hint = touchSqueeze, false;
        return true;
    case back:
        if (p.ahead > -.02f || std::abs(p.side) > .40f)
            return hint = touchBehind, false;
        if (p.below < -.25f)
            return hint = touchLower, false;
        if (p.below > .60f)
            return hint = touchHigher, false;
        return true;
    case holsters: {
        const float out = hand == 0 ? -p.side : p.side;
        if (out < .05f || out > .55f || p.ahead < -.30f || p.ahead > .35f)
            return hint = touchSides, false;
        if (p.below < .35f)
            return hint = touchLower, false;
        if (p.below > 1.05f)
            return hint = touchHigher, false;
        return true;
    }
    default:
        return false;
    }
}

void Flow::start(float eyeHeight) {
    active_ = true;
    skipped_ = false;
    step_ = belt;
    hint_ = touchSqueeze;
    scale_ = holster::scaleFor(eyeHeight);
    touches_ = {};
    // A grip or button already down when the steps begin is not a touch: it must open first.
    gripHeld_ = {true, true};
    triggerHeld_ = {true, true};
    aHeld_ = true;
}

void Flow::advance() {
    ++step_;
    hint_ = step_ == confirm ? touchConfirm : touchSqueeze;
}

uint32_t Flow::doneMask() const {
    uint32_t mask = (1u << stand) | (1u << tpose);
    if (touches_.belt)
        mask |= 1u << belt;
    if (touches_.back)
        mask |= 1u << back;
    if (touches_.hips[0] && touches_.hips[1])
        mask |= 1u << holsters;
    if (step_ == finished && !skipped_)
        mask |= 1u << confirm;
    return mask;
}

bool Flow::update(const Input& in) {
    if (!active_)
        return false;
    // Edges: a press counts once, from open.
    std::array<bool, 2> gripEdge{}, triggerEdge{};
    for (int i = 0; i < 2; ++i) {
        const float g = in.tracked[i] && std::isfinite(in.grips[i]) ? in.grips[i] : 0.f;
        const float t = in.tracked[i] && std::isfinite(in.triggers[i]) ? in.triggers[i] : 0.f;
        gripEdge[i] = !gripHeld_[i] && g > gripPressed;
        if (g > gripPressed)
            gripHeld_[i] = true;
        else if (g < gripReleased)
            gripHeld_[i] = false;
        triggerEdge[i] = !triggerHeld_[i] && t > gripPressed;
        if (t > gripPressed)
            triggerHeld_[i] = true;
        else if (t < gripReleased)
            triggerHeld_[i] = false;
    }
    const bool aEdge = in.a && !aHeld_;
    aHeld_ = in.a;
    if (step_ == confirm) {
        const bool bothTriggers = (triggerEdge[0] && triggerHeld_[1]) || (triggerEdge[1] && triggerHeld_[0]);
        if (aEdge || bothTriggers) {
            step_ = finished;
            active_ = false;
            hint_ = touchNone;
            return true;
        }
        return false;
    }
    if (!in.headTracked || !finite(in.head.position)) {
        hint_ = touchTracking;
        return false;
    }
    const Vec3 look = in.head.orientation.rotate({0, 0, -1});
    holster::BodyYaw body;
    body.update(0, look);
    body.scale = scale_;
    if (!body.valid)
        return false;
    for (int i = 0; i < 2; ++i) {
        if (!gripEdge[i] || !finite(in.hands[i]))
            continue;
        const auto p = holster::bodyPoint(in.hands[i], in.head.position, body);
        uint32_t why = touchNone;
        if (!plausible(step_, i, p, why)) {
            hint_ = why;
            continue;
        }
        if (step_ == belt) {
            touches_.belt = true, touches_.beltAt = p;
            advance();
            break;
        }
        if (step_ == back) {
            touches_.back = true, touches_.backAt = p;
            advance();
            break;
        }
        if (step_ == holsters) {
            touches_.hips[i] = true, touches_.hipAt[i] = p;
            if (touches_.hips[0] && touches_.hips[1]) {
                advance();
                break;
            }
            hint_ = touchSides;
        }
    }
    return false;
}

holster::Fit Flow::fit() const {
    holster::Fit f;
    if (skipped_)
        return f;
    if (touches_.belt)
        f.beltBelow = holster::fitShift(touches_.beltAt.below - refBeltBelow, holster::fitLimit);
    if (touches_.back)
        f.backBelow = holster::fitShift(touches_.backAt.below - refBackBelow, holster::fitLimit);
    if (touches_.hips[0] && touches_.hips[1]) {
        const float below = (touches_.hipAt[0].below + touches_.hipAt[1].below) / 2;
        const float out = (-touches_.hipAt[0].side + touches_.hipAt[1].side) / 2;
        f.hipBelow = holster::fitShift(below - refHipBelow, holster::fitLimit);
        f.hipSide = holster::fitShift(out - refHipSide, holster::fitSideLimit);
    }
    return f;
}

Figure tPoseFigure() {
    Figure f;
    // The figure box: y -1 (the feet) to 1 (the head's top). The arms are level with the shoulders.
    const float shoulderY = .52f, span = .92f;
    f.head = {0, .80f, 0};
    f.neck = {0, .62f, 0};
    f.chest = {0, .36f, 0};
    f.waist = {0, .02f, 0};
    for (int i = 0; i < 2; ++i) {
        const float s = i == 0 ? -1.f : 1.f;
        f.shoulders[i] = {s * .20f, shoulderY, 0};
        f.elbows[i] = {s * (.20f + (span - .20f) * .5f), shoulderY, 0};
        f.hands[i] = {s * span, shoulderY, 0};
        f.hips[i] = {s * .13f, -.08f, 0};
        f.knees[i] = {s * .15f, -.52f, 0};
        f.feet[i] = {s * .17f, -.96f, 0};
    }
    return f;
}

namespace {
constexpr float pi = 3.14159265f;
// Linear RGB: the hologram's blues and whites.
constexpr Vec3 deep{.004f, .018f, .05f}, blue{.05f, .32f, 1.f}, cyan{.25f, .75f, 1.f}, white{.92f, .97f, 1.f},
    dim{.10f, .26f, .52f}, good{.35f, 1.f, .85f};
float pulse(float clock, float rate = 1.2f) {
    return .5f + .5f * std::sin(2 * pi * rate * std::fmod(clock, 1000.f));
}
struct Panel {
    Vec3 centre, right, up;
    Vec3 at(float x, float y) const { return centre + right * x + up * y; }
    Shape white_() const {
        const auto u = art::uv(art::white, .5f, .5f);
        return {u.u, u.v, 0, 0};
    }
    void tri4(std::vector<Vertex>& v, Vec3 a, Vec3 b, Vec3 c, Vec3 d, Vec3 color, float alpha) const {
        const Shape s = white_();
        const Vertex p{a, color, alpha, s}, q{b, color, alpha, s}, r{c, color, alpha, s}, t{d, color, alpha, s};
        v.insert(v.end(), {p, q, r, p, r, t});
    }
    void fill(std::vector<Vertex>& v, float x0, float y0, float x1, float y1, Vec3 color, float alpha) const {
        tri4(v, at(x0, y0), at(x1, y0), at(x1, y1), at(x0, y1), color, alpha);
    }
    // A line, `width` wide, with a soft glow under it.
    void line(std::vector<Vertex>& v, float x0, float y0, float x1, float y1, float width, Vec3 color, float alpha,
              float glow = 2.6f) const {
        const float dx = x1 - x0, dy = y1 - y0, l = std::sqrt(dx * dx + dy * dy);
        if (!(l > 1e-6f))
            return;
        const float nx = -dy / l, ny = dx / l;
        const auto band = [&](float w, Vec3 c, float a) {
            const float hx = nx * w / 2, hy = ny * w / 2;
            tri4(v, at(x0 - hx, y0 - hy), at(x1 - hx, y1 - hy), at(x1 + hx, y1 + hy), at(x0 + hx, y0 + hy), c, a);
        };
        if (glow > 0)
            band(width * glow, cyan, alpha * .22f);
        band(width, color, alpha);
    }
    void frame(std::vector<Vertex>& v, float x0, float y0, float x1, float y1, float width, Vec3 color,
               float alpha) const {
        line(v, x0, y0, x1, y0, width, color, alpha);
        line(v, x1, y0, x1, y1, width, color, alpha);
        line(v, x1, y1, x0, y1, width, color, alpha);
        line(v, x0, y1, x0, y0, width, color, alpha);
    }
    void ring(std::vector<Vertex>& v, float cx, float cy, float rx, float ry, float width, Vec3 color, float alpha,
              int segments = 20) const {
        for (int i = 0; i < segments; ++i) {
            const float a0 = 2 * pi * i / segments, a1 = 2 * pi * (i + 1) / segments;
            line(v, cx + rx * std::cos(a0), cy + ry * std::sin(a0), cx + rx * std::cos(a1), cy + ry * std::sin(a1),
                 width, color, alpha, 2.f);
        }
    }
    // Text, left-aligned at x (or centred), shrunk to `width`.
    void text(std::vector<Vertex>& v, const char* s, float x, float y, float height, Vec3 color, float width,
              float alpha = 1, bool centred = false) const {
        PixelTextStyle style;
        style.top = color;
        style.bottom = color * .78f;
        style.alpha = alpha;
        style.outline = {0, 0, .002f};
        style.outlineAlpha = .8f;
        style.xScale = .9f;
        style.slant = 0;
        style.height = height;
        const float w = pixelTextWidth(s, style);
        if (w > width && w > 0)
            style.height *= width / w;
        const float actual = pixelTextWidth(s, style);
        const auto first = v.size();
        appendPixelText(v, s, at(centred ? x - actual / 2 : x, y), right, up, style);
        const Shape blank = white_();
        for (auto i = first; i < v.size(); ++i)
            v[i].shape = blank;
    }
};
} // namespace

void append(std::vector<Vertex>& out, const body_calibration::View& view, float clock) {
    if (view.phase == Phase::idle || !finite(view.panel.position) || !std::isfinite(clock))
        return;
    const auto& q = view.panel.orientation;
    const Panel p{view.panel.position, q.rotate({1, 0, 0}), q.rotate({0, 1, 0})};
    if (!finite(p.right) || !finite(p.up))
        return;
    const float w = body_calibration::panelWidth / 2, h = body_calibration::panelHeight / 2;
    const float beat = pulse(clock);
    // Which steps are met and which is active.
    uint32_t done = 0, active = stand;
    if (view.phase == Phase::done) {
        done = view.holoDone | (1u << stand) | (1u << tpose);
        active = view.holoStep;
    } else {
        const auto s = body_calibration::steps(view);
        if (s.done[0])
            done |= 1u << stand;
        active = s.done[0] ? tpose : stand;
    }
    // The glass: deep blue, a lighter band at the top, scan lines, the frame.
    p.fill(out, -w, -h, w, h, deep, .78f);
    p.fill(out, -w, h - .09f, w, h, blue * .35f, .55f);
    for (int i = 0; i < 24; ++i) {
        const float y = -h + (2 * h) * (i + .5f) / 24;
        p.fill(out, -w, y - .0012f, w, y + .0012f, blue, .07f);
    }
    p.frame(out, -w, -h, w, h, .006f, cyan, .95f);
    const float c = .03f; // corner ticks
    for (int sx = -1; sx <= 1; sx += 2)
        for (int sy = -1; sy <= 1; sy += 2) {
            const float x = sx * (w + .012f), y = sy * (h + .012f);
            p.line(out, x, y, x - sx * c * 2, y, .008f, white, 1);
            p.line(out, x, y, x, y - sy * c * 2, .008f, white, 1);
        }
    p.text(out, "BODY CALIBRATION", -w + .04f, h - .065f, .04f, white, .55f);
    if (active != tpose || view.phase == Phase::done) // the T-pose caption takes its place
        p.text(out, "HOLO", w - .14f, h - .065f, .03f, cyan, .1f, .9f);
    // The checklist.
    const float x0 = -w + .05f, rowH = .056f, top = h - .145f; // six rows clear the hint line
    for (uint32_t i = 0; i < steps; ++i) {
        const float y = top - rowH * static_cast<float>(i);
        const bool met = (done >> i) & 1u, now = i == active && view.phase != Phase::idle && !met;
        if (now)
            p.fill(out, x0 - .02f, y - .026f, -.04f, y + .03f, blue, .28f + .18f * beat);
        // The box and its tick.
        const float b = .02f;
        p.frame(out, x0, y - b, x0 + 2 * b, y + b, .004f, met ? good : (now ? white : dim), 1);
        if (met) {
            p.line(out, x0 + .008f, y, x0 + .018f, y - .012f, .006f, good, 1, 2.f);
            p.line(out, x0 + .018f, y - .012f, x0 + .036f, y + .014f, .006f, good, 1, 2.f);
        }
        char label[48];
        std::snprintf(label, sizeof label, "%u  %s", i + 1, stepNames[i]);
        p.text(out, label, x0 + .065f, y - .014f, .03f, met ? good * .85f : (now ? white : dim * 1.6f), .43f,
               now ? .85f + .15f * beat : 1.f);
    }
    // The hint line and the bar (the T-pose hold).
    {
        const char* hint = "";
        if (view.phase == Phase::holding)
            hint = "HOLD THE T-POSE STILL";
        else if (view.phase == Phase::waiting)
            hint = body_calibration::hintText(view.hint);
        else if (view.holoStep == finished)
            hint = "CALIBRATED - REDO IT IN SETTINGS, SPIDY VR";
        else
            hint = touchText(view.holoHint);
        p.text(out, hint, x0, -h + .085f, .022f, white, .5f, .95f);
        const float share = view.phase == Phase::done ? 1.f
                            : std::clamp(std::isfinite(view.progress) ? view.progress : 0.f, 0.f, 1.f);
        const float bx0 = x0, bx1 = -.06f, by = -h + .05f;
        p.fill(out, bx0, by - .005f, bx1, by + .005f, dim, .6f);
        if (share > 0)
            p.fill(out, bx0, by - .005f, bx0 + (bx1 - bx0) * share, by + .005f, cyan, 1);
        if (view.phase == Phase::done && view.holoStep == finished) {
            char line[64];
            std::snprintf(line, sizeof line, "EYE %.2f M   ARM %.2f M", view.result.eyeHeight, view.result.armLength);
            p.text(out, line, .3f, -h + .03f, .022f, good, .5f, 1, true);
        } else {
            p.text(out, "B SKIPS", w - .14f, -h + .03f, .02f, cyan, .12f, .9f);
        }
    }
    // The wireframe body in a TRUE T-pose, in the right half.
    {
        const float cx = .3f, cy = -.02f, sx = .25f, sy = .215f;
        const auto F = tPoseFigure();
        const float lw = .0045f, a = .95f;
        const auto L = [&](Vec3 a0, Vec3 a1, Vec3 col, float al) {
            p.line(out, cx + a0.x * sx, cy + a0.y * sy, cx + a1.x * sx, cy + a1.y * sy, lw, col, al);
        };
        // The holo platform and its rings.
        p.ring(out, cx, cy - .985f * sy, .12f, .018f, .003f, cyan, .8f);
        p.ring(out, cx, cy - .985f * sy, .17f, .026f, .0025f, blue, .6f);
        // Head, neck.
        p.ring(out, cx + F.head.x * sx, cy + F.head.y * sy, .1f * sx * 1.25f, .17f * sy, lw, white, a, 16);
        p.ring(out, cx + F.head.x * sx, cy + F.head.y * sy, .1f * sx * .5f, .17f * sy, lw * .6f, cyan, .6f, 12);
        L(F.neck, {0, F.head.y - .17f, 0}, white, a);
        // The shoulders' line and both arms, straight out and level (upper and lower edge of each arm).
        L(F.shoulders[0], F.shoulders[1], white, a);
        for (int i = 0; i < 2; ++i) {
            const Vec3 up{0, .045f, 0};
            L(F.shoulders[i] + up, F.hands[i] + up, white, a);
            L(F.shoulders[i] - up, F.hands[i] - up, white, a);
            L(F.elbows[i] + up, F.elbows[i] - up, cyan, .8f);
            L(F.hands[i] + up, F.hands[i] - up, white, a);
            // The hand: three short fingers on the arm's line.
            const float s = i == 0 ? -1.f : 1.f;
            for (int k = -1; k <= 1; ++k)
                L(F.hands[i] + Vec3{0, .025f * k, 0}, F.hands[i] + Vec3{s * .06f, .03f * k, 0}, white, a);
        }
        // Torso: outline and the mesh's rings.
        const Vec3 lsh = F.shoulders[0], rsh = F.shoulders[1];
        L(lsh, F.hips[0] + Vec3{-.02f, 0, 0}, white, a);
        L(rsh, F.hips[1] + Vec3{.02f, 0, 0}, white, a);
        L(F.hips[0] + Vec3{-.02f, 0, 0}, F.hips[1] + Vec3{.02f, 0, 0}, white, a);
        L(F.neck, F.waist, cyan, .55f);
        for (int k = 1; k <= 3; ++k) {
            const float t = k / 4.f, y = lsh.y + (F.hips[0].y - lsh.y) * t, hw = .20f + (.15f - .20f) * t;
            L({-hw, y, 0}, {hw, y, 0}, cyan, .45f);
        }
        // Legs: two lines each, knee ticks, feet.
        for (int i = 0; i < 2; ++i) {
            const float s = i == 0 ? -1.f : 1.f;
            L(F.hips[i] + Vec3{s * .07f, 0, 0}, F.feet[i] + Vec3{s * .045f, 0, 0}, white, a);
            L(F.hips[i] - Vec3{s * .05f, 0, 0}, F.feet[i] - Vec3{s * .02f, 0, 0}, white, a);
            L(F.knees[i] - Vec3{.05f, 0, 0}, F.knees[i] + Vec3{.06f, 0, 0}, cyan, .6f);
            L(F.feet[i] - Vec3{s * .03f, 0, 0}, F.feet[i] + Vec3{s * .12f, 0, 0}, white, a);
        }
        // The spots to touch: belt, back (between the shoulders), each hip.
        const auto spot = [&](float fx, float fy, uint32_t which) {
            const bool met = (done >> which) & 1u, now = active == which && !met;
            if (!met && !now)
                return;
            const float r = now ? .022f + .008f * beat : .018f;
            p.ring(out, cx + fx * sx, cy + fy * sy, r, r, .004f, met ? good : white, now ? .7f + .3f * beat : .9f, 14);
        };
        if (active == belt || ((done >> belt) & 1u)) {
            const bool met = (done >> belt) & 1u;
            L({-.17f, F.waist.y, 0}, {.17f, F.waist.y, 0}, met ? good : white, met ? .9f : .6f + .4f * beat);
        }
        spot(0, F.chest.y + .08f, back);
        spot(F.hips[0].x - .06f, F.hips[0].y - .04f, holsters);
        spot(F.hips[1].x + .06f, F.hips[1].y - .04f, holsters);
        spot(0, F.waist.y, belt);
        const char* caption = view.phase == Phase::done ? "" : "ARMS STRAIGHT OUT, LEVEL";
        if (active == tpose && *caption)
            p.text(out, caption, cx, cy + .9f * sy + .06f, .02f, cyan, .45f, .85f + .15f * beat, true);
    }
}
} // namespace spidy::holo_calibration
