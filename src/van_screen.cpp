// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
// Made by Agent Mulder.
// Version 0.0.73 test 3 VAN SCREEN (van_screen.hpp).
#include "spidy/van_screen.hpp"
#include "spidy/finger_ray074.hpp"
#include "spidy/overlay_text.hpp"
#include "spidy/soft_shapes.hpp"
#include "spidy/test_notice.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string_view>

namespace spidy::van_screen {

// Original short case files (the characters are Marvel's; every word here is ours).
const CaseFile caseFiles[files] = {
    {"JIGSAW", "BILLY RUSSO", "EXTREME", 5,
     "Ex-special forces. Ran a private army for hire. His face went through a window and came back stitched. "
     "Smiles while he pays the shooters."},
    {"KINGPIN", "WILSON FISK", "EXTREME", 5,
     "Owns the rackets, the judges and half the precinct. Never holds the gun himself. Cut off the money "
     "and the muscle follows."},
    {"BULLSEYE", "NAME UNKNOWN", "HIGH", 4,
     "Contract killer who does not miss. Anything in reach is a weapon: cards, pens, a paper clip. "
     "Works for pay, kills for fun."},
    {"BARRACUDA", "NAME ON FILE", "HIGH", 4,
     "Cartel muscle and fixer. Big, loud, hard to put down. Lost an eye and fingers and kept coming. "
     "Laughs in a firefight."},
    {"THE RUSSIAN", "NAME UNKNOWN", "MEDIUM", 3,
     "Enforcer built like a truck. Fights bare-handed and grins when he bleeds. Do not let him close "
     "the distance."},
    {"MA GNUCCI", "ISABELLA GNUCCI", "HIGH", 4,
     "Head of a crime family. Lost her sons and more and never her nerve. Has a price on Castle's head "
     "and pays it in cash."},
    {"MICROCHIP", "D. L. LIEBERMAN", "LOW", 1,
     "Hacker and armorer. Built the network, kept the van running. Friend once. Check his work anyway."},
};

namespace {
constexpr float tau = 6.28318530718f;
bool ok(float f) { return std::isfinite(f); }

// The palette (linear RGB): gunmetal, a black-green face, olive, khaki, bone, blood red, armor green.
// 0.0.74 test 1 BATMOBILE: the Batman holo theme swaps them (setTheme, per append; render thread's own copy):
// black-blue trim, a deep blue face, blue fills, cyan text, ice-white, a bat-signal amber, cyan lights.
struct Palette {
    Vec3 gunmetal, faceDark, olive, khaki, bone, blood, armor, dim;
};
constexpr Palette punisherPalette{{.030f, .031f, .027f}, {.010f, .014f, .009f}, {.060f, .075f, .028f}, {.34f, .34f, .16f},
                                  {.78f, .74f, .58f},    {.62f, .035f, .02f},   {.16f, .58f, .12f},    {.15f, .16f, .08f}};
constexpr Palette batmanPalette{{.012f, .016f, .026f}, {.002f, .010f, .022f}, {.016f, .050f, .100f}, {.10f, .46f, .68f},
                                {.72f, .90f, 1.00f},   {.85f, .58f, .04f},    {.06f, .72f, .95f},   {.04f, .14f, .24f}};
thread_local Vec3 gunmetal = punisherPalette.gunmetal, faceDark = punisherPalette.faceDark, olive = punisherPalette.olive,
                  khaki = punisherPalette.khaki, bone = punisherPalette.bone, blood = punisherPalette.blood,
                  armor = punisherPalette.armor, dim = punisherPalette.dim;
void setTheme(bool batman) {
    const Palette& t = batman ? batmanPalette : punisherPalette;
    gunmetal = t.gunmetal;
    faceDark = t.faceDark;
    olive = t.olive;
    khaki = t.khaki;
    bone = t.bone;
    blood = t.blood;
    armor = t.armor;
    dim = t.dim;
}

struct Painter {
    std::vector<Vertex>& out;
    Vec3 c, r, u, n;
    float a;
    Vec3 P(float x, float y, float z) const { return c + r * x + u * y + n * z; }
    void tri(Vec3 p, Vec3 q, Vec3 s, Vec3 col, float al) {
        out.push_back(Vertex{p, col, al * a});
        out.push_back(Vertex{q, col, al * a});
        out.push_back(Vertex{s, col, al * a});
    }
    void quad(float x0, float y0, float x1, float y1, float z, Vec3 col, float al = 1) {
        const Vec3 p00 = P(x0, y0, z), p10 = P(x1, y0, z), p11 = P(x1, y1, z), p01 = P(x0, y1, z);
        tri(p00, p10, p11, col, al);
        tri(p00, p11, p01, col, al);
    }
    void frame(float x0, float y0, float x1, float y1, float e, float z, Vec3 col, float al = 1) {
        quad(x0, y1 - e, x1, y1, z, col, al);
        quad(x0, y0, x1, y0 + e, z, col, al);
        quad(x0, y0 + e, x0 + e, y1 - e, z, col, al);
        quad(x1 - e, y0 + e, x1, y1 - e, z, col, al);
    }
    // A bar from (x0,y0) to (x1,y1), w wide.
    void bar(float x0, float y0, float x1, float y1, float w, float z, Vec3 col, float al = 1) {
        const float dx = x1 - x0, dy = y1 - y0, l = std::sqrt(dx * dx + dy * dy);
        if (!(l > 1e-6f))
            return;
        const float nx = -dy / l * w / 2, ny = dx / l * w / 2;
        const Vec3 p0 = P(x0 + nx, y0 + ny, z), p1 = P(x1 + nx, y1 + ny, z), p2 = P(x1 - nx, y1 - ny, z),
                   p3 = P(x0 - nx, y0 - ny, z);
        tri(p0, p1, p2, col, al);
        tri(p0, p2, p3, col, al);
    }
    void disc(float x, float y, float rad, float z, Vec3 col, float al = 1, int k = 16, float sy = 1) {
        for (int i = 0; i < k; ++i) {
            const float t0 = tau * i / k, t1 = tau * (i + 1) / k;
            tri(P(x, y, z), P(x + rad * std::cos(t0), y + rad * sy * std::sin(t0), z),
                P(x + rad * std::cos(t1), y + rad * sy * std::sin(t1), z), col, al);
        }
    }
    void ring(float x, float y, float rad, float w, float z, Vec3 col, float al = 1, int k = 32) {
        for (int i = 0; i < k; ++i) {
            const float t0 = tau * i / k, t1 = tau * (i + 1) / k;
            const float r0 = rad - w / 2, r1 = rad + w / 2;
            const Vec3 a0 = P(x + r0 * std::cos(t0), y + r0 * std::sin(t0), z), a1 = P(x + r0 * std::cos(t1), y + r0 * std::sin(t1), z),
                       b0 = P(x + r1 * std::cos(t0), y + r1 * std::sin(t0), z), b1 = P(x + r1 * std::cos(t1), y + r1 * std::sin(t1), z);
            tri(a0, b0, b1, col, al);
            tri(a0, b1, a1, col, al);
        }
    }
    void text(std::string_view s, float x, float y, float h, Vec3 col, float z = .0025f, float al = 1, float weight = .13f) {
        TextStyle ts;
        ts.height = h;
        ts.weight = weight;
        ts.color = col;
        ts.soft = h * .09f;
        const size_t mark = out.size();
        appendText(out, s, P(x, y, z), r, u, ts);
        for (size_t i = mark; i < out.size(); ++i)
            out[i].alpha *= al * a;
    }
    void textRight(std::string_view s, float x, float y, float h, Vec3 col, float z = .0025f, float al = 1) {
        text(s, x - textWidth(s, h), y, h, col, z, al);
    }
    void textCentre(std::string_view s, float x, float y, float h, Vec3 col, float z = .0025f, float al = 1) {
        text(s, x - textWidth(s, h) / 2, y, h, col, z, al);
    }
};

// Liang-Barsky: the part of (x0,y0)-(x1,y1) inside the box; false: none.
bool clip(float& x0, float& y0, float& x1, float& y1, float lx, float ly, float hx, float hy) {
    float t0 = 0, t1 = 1;
    const float dx = x1 - x0, dy = y1 - y0;
    const float p[4] = {-dx, dx, -dy, dy}, q[4] = {x0 - lx, hx - x0, y0 - ly, hy - y0};
    for (int i = 0; i < 4; ++i) {
        if (p[i] == 0) {
            if (q[i] < 0)
                return false;
            continue;
        }
        const float t = q[i] / p[i];
        if (p[i] < 0)
            t0 = std::max(t0, t);
        else
            t1 = std::min(t1, t);
        if (t0 > t1)
            return false;
    }
    const float ax = x0 + t0 * dx, ay = y0 + t0 * dy;
    x1 = x0 + t1 * dx;
    y1 = y0 + t1 * dy;
    x0 = ax;
    y0 = ay;
    return true;
}

// An original skull: round cranium, square jaw, two sockets, a nose notch, short teeth.
void skull(Painter& p, float x, float y, float s, Vec3 col, float z) {
    p.disc(x, y + .10f * s, .42f * s, z, col, 1, 18, 1.05f);
    p.quad(x - .27f * s, y - .40f * s, x + .27f * s, y - .08f * s, z, col);
    p.disc(x - .20f * s, y - .05f * s, .14f * s, z, col, 1, 10); // cheekbones
    p.disc(x + .20f * s, y - .05f * s, .14f * s, z, col, 1, 10);
    const float zz = z + .0004f;
    p.disc(x - .17f * s, y + .03f * s, .135f * s, zz, faceDark, 1, 12, 1.15f);
    p.disc(x + .17f * s, y + .03f * s, .135f * s, zz, faceDark, 1, 12, 1.15f);
    p.disc(x - .17f * s, y + .02f * s, .04f * s, zz + .0003f, blood, 1, 8);
    p.disc(x + .17f * s, y + .02f * s, .04f * s, zz + .0003f, blood, 1, 8);
    p.tri(p.P(x - .06f * s, y - .10f * s, zz), p.P(x + .06f * s, y - .10f * s, zz), p.P(x, y - .21f * s, zz), faceDark, 1);
    for (int k = -1; k <= 1; ++k)
        p.quad(x + k * .12f * s - .012f * s, y - .38f * s, x + k * .12f * s + .012f * s, y - .25f * s, zz, faceDark);
    p.quad(x - .25f * s, y - .26f * s, x + .25f * s, y - .24f * s, zz, faceDark);
}

// 0.0.74 test 1 BATMOBILE: an original bat emblem (wings, scalloped hem, ears) in an oval, holo-lit.
void batEmblem(Painter& p, float x, float y, float s, Vec3 col, float z) {
    p.disc(x, y, .5f * s, z, col * .9f, .9f, 20, .58f);
    p.disc(x, y, .44f * s, z + .0002f, faceDark, 1, 20, .56f);
    const Vec3 c = col * 1.2f;
    const float zz = z + .0004f;
    // the body and head
    p.quad(x - .05f * s, y - .13f * s, x + .05f * s, y + .10f * s, zz, c);
    p.tri(p.P(x - .05f * s, y + .10f * s, zz), p.P(x - .035f * s, y + .17f * s, zz), p.P(x - .01f * s, y + .10f * s, zz), c, 1);
    p.tri(p.P(x + .01f * s, y + .10f * s, zz), p.P(x + .035f * s, y + .17f * s, zz), p.P(x + .05f * s, y + .10f * s, zz), c, 1);
    for (int sx = -1; sx <= 1; sx += 2) {
        const float k = static_cast<float>(sx);
        // the wing: a top edge out to the tip, the hem in three scallops back to the body
        const Vec3 root = p.P(x + k * .05f * s, y + .06f * s, zz), tip = p.P(x + k * .40f * s, y + .02f * s, zz);
        const Vec3 h1 = p.P(x + k * .30f * s, y - .07f * s, zz), h2 = p.P(x + k * .18f * s, y - .05f * s, zz),
                   h3 = p.P(x + k * .10f * s, y - .12f * s, zz), low = p.P(x + k * .05f * s, y - .10f * s, zz);
        p.tri(root, tip, h1, c, 1);
        p.tri(root, h1, h2, c, 1);
        p.tri(root, h2, h3, c, 1);
        p.tri(root, h3, low, c, 1);
    }
}

const char* compassName(float deg) {
    static const char* names[8] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
    const int i = static_cast<int>(std::floor((deg + 22.5f) / 45.f)) & 7;
    return names[i];
}

constexpr float W = width / 2, H = height / 2;
constexpr float bodyTop = H - tabHeight - .004f, bodyBottom = -H + .015f;

void drawMap(Painter& p, const View& v) {
    const float lx = -W + .008f, hx = lx + .124f, hy = bodyTop - .002f, ly = hy - .124f;
    const float cx = (lx + hx) / 2, cy = (ly + hy) / 2;
    p.quad(lx, ly, hx, hy, .001f, {.012f, .03f, .012f});
    p.frame(lx, ly, hx, hy, .0015f, .002f, khaki);
    // Heading-up: screen up is the van's way; the street grid every 40 m scrolls with the van (STYLIZED).
    const float h = v.heading, fx = std::sin(h), fz = -std::cos(h), rx = std::cos(h), rz = std::sin(h);
    const float scale = .062f / 60.f, block = 40.f;
    const auto toScreen = [&](float wx, float wz, float& sx, float& sy) {
        const float dx = wx - v.gridX, dz = wz - v.gridZ;
        sx = cx + (dx * rx + dz * rz) * scale;
        sy = cy + (dx * fx + dz * fz) * scale;
    };
    const float reach = 90.f;
    const float bx = std::floor((v.gridX - reach) / block) * block, bz = std::floor((v.gridZ - reach) / block) * block;
    for (int k = 0; k <= static_cast<int>(2 * reach / block) + 1; ++k) {
        for (int axis = 0; axis < 2; ++axis) {
            float x0, y0, x1, y1;
            if (axis == 0) {
                const float wx = bx + k * block;
                toScreen(wx, v.gridZ - reach, x0, y0);
                toScreen(wx, v.gridZ + reach, x1, y1);
            } else {
                const float wz = bz + k * block;
                toScreen(v.gridX - reach, wz, x0, y0);
                toScreen(v.gridX + reach, wz, x1, y1);
            }
            if (clip(x0, y0, x1, y1, lx + .002f, ly + .002f, hx - .002f, hy - .002f))
                p.bar(x0, y0, x1, y1, .0035f, .0015f, {.05f, .11f, .04f});
        }
    }
    // range rings and the cardinal letters (N: world -z, CANDIDATE)
    p.ring(cx, cy, .030f, .0007f, .0018f, dim, .8f, 28);
    p.ring(cx, cy, .052f, .0009f, .0018f, khaki, .8f, 36);
    for (int t = 0; t < 36; ++t) {
        const float ang = tau * t / 36.f - h; // from screen up, clockwise
        const float sx = std::sin(ang), sy = std::cos(ang), l = t % 9 == 0 ? .007f : .0035f;
        p.bar(cx + sx * .052f, cy + sy * .052f, cx + sx * (.052f - l), cy + sy * (.052f - l), .0008f, .0019f, khaki);
    }
    static const char* card[4] = {"N", "E", "S", "W"};
    for (int k = 0; k < 4; ++k) {
        const float ang = tau * k / 4.f - h;
        const float sx = cx + std::sin(ang) * .043f, sy = cy + std::cos(ang) * .043f;
        p.textCentre(card[k], sx, sy - .003f, .006f, k == 0 ? blood * 1.4f : bone);
    }
    // the van: a red arrow, nose up
    p.tri(p.P(cx, cy + .009f, .003f), p.P(cx - .0055f, cy - .006f, .003f), p.P(cx + .0055f, cy - .006f, .003f), blood * 1.5f, 1);
    p.tri(p.P(cx - .0055f, cy - .006f, .0031f), p.P(cx, cy - .002f, .0031f), p.P(cx + .0055f, cy - .006f, .0031f), faceDark, 1);
    // readouts
    char line[32];
    float deg = std::fmod(h * 360.f / tau, 360.f);
    if (deg < 0)
        deg += 360.f;
    const float x = hx + .010f;
    p.text("HEADING", x, hy - .010f, .0062f, khaki);
    std::snprintf(line, sizeof line, "%03d %s", static_cast<int>(deg + .5f) % 360, compassName(deg));
    p.text(line, x, hy - .030f, .014f, bone, .0025f, 1, .13f);
    p.text("SPEED", x, hy - .048f, .0062f, khaki);
    std::snprintf(line, sizeof line, "%d MPH", static_cast<int>(std::abs(v.speed) * 2.23694f + .5f));
    p.text(line, x, hy - .068f, .014f, bone, .0025f, 1, .13f);
    p.text("GRID: STYLIZED", x, ly + .016f, .0052f, dim * 1.6f);
    p.text("NO GAME MAP", x, ly + .006f, .0052f, dim * 1.6f);
    // 0.0.74 test 2b CRIME GPS: the crimes (red; the tracked gold), past the map's reach pinned to its rim
    if (v.crimeOn) {
        const uint32_t n = std::min(v.crimes, View::mostCrimes);
        for (uint32_t k = 0; k < n; ++k) {
            if (!ok(v.crimeX[k]) || !ok(v.crimeZ[k]))
                continue;
            float sx, sy;
            toScreen(v.crimeX[k], v.crimeZ[k], sx, sy);
            float dx = sx - cx, dy = sy - cy;
            const float l = std::sqrt(dx * dx + dy * dy), rim = .056f;
            if (l > rim && l > 1e-6f)
                dx *= rim / l, dy *= rim / l;
            const bool tracked = v.crimeTracked == static_cast<int>(k);
            p.disc(cx + dx, cy + dy, tracked ? .0042f : .0032f, .0032f, tracked ? Vec3{1.f, .7f, .1f} : blood * 1.8f, 1, 10);
            if (tracked)
                p.ring(cx + dx, cy + dy, .0065f, .0009f, .0032f, Vec3{1.f, .7f, .1f}, .9f, 16);
        }
        if (v.crimeTracked >= 0 && ok(v.crimeMetres) && ok(v.crimeBearing)) {
            p.text("TRACKED CRIME", x, hy - .082f, .0058f, blood * 1.6f);
            std::snprintf(line, sizeof line, "%d M %s", static_cast<int>(v.crimeMetres + .5f), compassName(v.crimeBearing));
            p.text(line, x, hy - .097f, .0095f, bone, .0025f, 1, .13f);
        } else if (n && !(v.crimePopup && v.offer.crimePopup)) { // the popup covers this line
            std::snprintf(line, sizeof line, "%u CRIME%s NEAR", n, n == 1 ? "" : "S");
            p.text(line, x, hy - .090f, .0058f, blood * 1.6f);
        }
        if (v.crimePopup && v.offer.crimePopup) {
            const float px1 = W - .006f, py0 = ly - .001f, py1 = hy - .072f;
            p.quad(x - .004f, py0, px1, py1, .0034f, {.10f, .006f, .004f}, .96f);
            p.frame(x - .004f, py0, px1, py1, .0012f, .0036f, blood * 2.f, 1);
            p.text("CRIME IN PROGRESS", x, py1 - .010f, .0068f, bone, .0038f);
            std::snprintf(line, sizeof line, "PRESS OK TO TRACK  %ds", static_cast<int>(v.crimePopupLeft + .5f));
            p.text(line, x, py1 - .020f, .0050f, khaki, .0038f);
        }
    }
}

void meter(Painter& p, float x0, float y0, float x1, float h, float share, Vec3 col) {
    share = std::clamp(ok(share) ? share : 0.f, 0.f, 1.f);
    p.quad(x0, y0, x1, y0 + h, .0015f, olive);
    const int segs = 12;
    const float w = (x1 - x0) / segs;
    for (int k = 0; k < segs; ++k)
        if ((k + .5f) / segs <= share)
            p.quad(x0 + k * w + .0006f, y0 + .0008f, x0 + (k + 1) * w - .0006f, y0 + h - .0008f, .002f, k >= segs - 2 ? blood * 1.5f : col);
}

void drawStatus(Painter& p, const View& v) {
    char line[40];
    const float mph = std::abs(v.speed) * 2.23694f, kmh = std::abs(v.speed) * 3.6f;
    const float lx = -W + .010f, top = bodyTop;
    p.text("SPEED", lx, top - .010f, .0062f, khaki);
    std::snprintf(line, sizeof line, "%d", static_cast<int>(mph + .5f));
    p.text(line, lx, top - .050f, .028f, bone, .0025f, 1, .12f);
    p.text("MPH", lx + textWidth(line, .028f) + .005f, top - .050f, .008f, khaki);
    std::snprintf(line, sizeof line, "%d KM/H", static_cast<int>(kmh + .5f));
    p.text(line, lx, top - .063f, .0065f, khaki);
    meter(p, lx, top - .077f, lx + .115f, .009f, std::abs(v.speed) / std::max(1.f, v.topSpeed), armor);
    // the van: a top-down outline with its plates
    const float vx = lx + .058f, vy = bodyBottom + .022f;
    p.frame(vx - .03f, vy - .011f, vx + .03f, vy + .011f, .0012f, .002f, khaki);
    p.quad(vx - .03f, vy - .011f, vx - .022f, vy + .011f, .0019f, dim);
    for (int k = 0; k < 4; ++k)
        p.quad(vx - .016f + k * .011f, vy - .007f, vx - .008f + k * .011f, vy + .007f, .0021f, olive * 2.f);
    p.text("HULL", vx + .036f, vy + .001f, .0052f, khaki);
    p.text("PLATED", vx + .036f, vy - .008f, .0052f, armor * 1.3f);
    // the right column
    const float rx = .005f;
    float y = top - .010f;
    const auto row = [&](const char* label, const char* value, Vec3 col) {
        p.text(label, rx, y, .0062f, khaki);
        p.textRight(value, W - .010f, y, .0075f, col);
        y -= .0075f;
        p.quad(rx, y, W - .010f, y + .0006f, .0016f, olive * 1.6f);
        y -= .0105f;
    };
    row("ARMOR", "BULLETPROOF", armor * 1.3f);
    const char* engine = v.driving ? (v.rpm > .08f ? "RUNNING" : "IDLE") : "OFF";
    row("ENGINE", engine, v.driving ? bone : dim * 1.8f);
    std::snprintf(line, sizeof line, "%d%%", static_cast<int>(std::clamp(ok(v.rpm) ? v.rpm : 0.f, 0.f, 1.f) * 100 + .5f));
    p.text("LOAD", rx, y, .0055f, khaki);
    meter(p, rx + .022f, y - .001f, W - .036f, .0075f, v.driving ? v.rpm : 0.f, khaki * 1.6f);
    p.textRight(line, W - .010f, y, .0055f, bone);
    y -= .016f;
    const char* gear = !v.driving ? "P" : v.speed < -.2f ? "R" : v.speed > .2f ? "D" : "N";
    row("GEAR", gear, bone);
    row("STATE", v.driving ? "DRIVING" : "PARKED", v.driving ? bone : khaki);
}

void drawCase(Painter& p, const View& v) {
    const CaseFile& f = caseFiles[std::min(v.file, files - 1)];
    char line[48];
    const float top = bodyTop;
    // the mug-shot frame: a silhouette, a crosshair, the file number
    const float mx0 = -W + .010f, mx1 = mx0 + .056f, my1 = top - .004f, my0 = my1 - .068f;
    p.quad(mx0, my0, mx1, my1, .001f, {.02f, .025f, .015f});
    p.frame(mx0, my0, mx1, my1, .0013f, .002f, khaki);
    const float sx = (mx0 + mx1) / 2;
    p.disc(sx, my0 + .040f, .0115f, .0019f, dim * 1.4f, 1, 16, 1.2f);
    p.disc(sx, my0 + .014f, .022f, .0019f, dim * 1.4f, 1, 18, .55f);
    p.quad(mx0 + .0015f, my0 + .0015f, mx1 - .0015f, my0 + .014f, .00195f, dim * 1.4f);
        p.ring(sx, my0 + .038f, .016f, .0007f, .0022f, blood * 1.5f, .9f, 24);
    p.bar(sx - .022f, my0 + .038f, sx + .022f, my0 + .038f, .0006f, .0022f, blood * 1.5f, .9f);
    p.bar(sx, my0 + .016f, sx, my0 + .060f, .0006f, .0022f, blood * 1.5f, .9f);
    std::snprintf(line, sizeof line, "FILE %u/%u", std::min(v.file, files - 1) + 1, files);
    p.textCentre(line, sx, my0 - .010f, .0058f, khaki);
    // the threat bars
    p.text("THREAT", mx0, my0 - .022f, .0052f, khaki);
    for (uint32_t k = 0; k < 5; ++k)
        p.quad(mx0 + k * .0112f, my0 - .032f, mx0 + k * .0112f + .009f, my0 - .026f, .002f, k < f.level ? blood * 1.5f : olive * 1.5f);
    p.text(f.threat, mx0, my0 - .042f, .0055f, f.level >= 4 ? blood * 1.6f : bone);
    // the name, the alias, the bio
    const float tx = mx1 + .008f, most = W - .008f - tx;
    p.text(f.name, tx, top - .015f, std::min(.012f, fittedHeight(f.name, .012f, most - .03f)), bone, .0025f, 1, .15f);
    p.textRight("OPEN", W - .010f, top - .013f, .0065f, blood * 1.6f);
    p.frame(W - .012f - textWidth("OPEN", .0065f) - .003f, top - .0165f, W - .007f, top - .0035f, .0008f, .0024f, blood * 1.6f);
    std::snprintf(line, sizeof line, "ALIAS: %s", f.alias);
    p.text(line, tx, top - .027f, .0058f, khaki);
    p.quad(tx, top - .032f, W - .008f, top - .0313f, .0016f, olive * 1.8f);
    const float bh = .0066f;
    float y = top - .043f;
    int count = 0;
    for (const auto& l : test_notice::lines(f.bio, bh, most)) {
        if (++count > 8 || y < bodyBottom + .002f)
            break;
        p.text(l, tx, y, bh, bone * .92f);
        y -= bh + .0042f;
    }
}

void drawLocker(Painter& p, const View&) {
    struct Item {
        const char* name;
        const char* count;
    };
    static const Item items[] = {{"CARBINE", "RACKED"}, {"SHOTGUN 12 GA", "RACKED"}, {"SIDEARMS", "X2"},
                                 {"FRAG", "X6"},        {"BREACHING", "X3"},         {"KNIFE", "ON BELT"},
                                 {"PLATES", "LEVEL IV"}, {"AMMO", "FULL"}};
    const float top = bodyTop - .010f, colW = (2 * W - .030f) / 2;
    for (int i = 0; i < 8; ++i) {
        const float x0 = -W + .010f + (i / 4) * (colW + .010f), y = top - (i % 4) * .025f;
        p.quad(x0, y - .010f, x0 + colW, y + .009f, .0012f, olive);
        p.quad(x0, y - .010f, x0 + .0025f, y + .009f, .0016f, i < 6 ? khaki : armor);
        p.text(items[i].name, x0 + .006f, y - .0035f, .0064f, bone);
        p.textRight(items[i].count, x0 + colW - .004f, y - .0035f, .0058f, khaki * 1.3f);
    }
    p.text("LOCKER SEALED - REAR DOORS", -W + .010f, bodyBottom + .006f, .0052f, dim * 1.8f);
}

// 0.0.74 test 1: a button (its frame lit while the screen is hovered).
void drawButton(Painter& p, const Button& b, bool lit, Vec3 face) {
    p.quad(b.x0, b.y0, b.x1, b.y1, .0012f, face);
    p.frame(b.x0, b.y0, b.x1, b.y1, .0010f, .0016f, lit ? bone : khaki);
    const float h = std::min(.0085f, fittedHeight(b.label, .0085f, b.x1 - b.x0 - .006f));
    p.textCentre(b.label, (b.x0 + b.x1) / 2, (b.y0 + b.y1) / 2 - h / 2, h, bone, .0022f);
}

std::string clipped(const std::string& s, float h, float most) {
    if (textWidth(s, h) <= most)
        return s;
    std::string t = s;
    while (!t.empty() && textWidth(t + "...", h) > most)
        t.pop_back();
    return t + "...";
}

void drawMusic(Painter& p, const View& v) {
    const float lx = -W + .010f, top = bodyTop;
    p.text("MUSIC", lx, top - .010f, .0062f, khaki);
    char line[64];
    if (!v.offer.music) {
        p.text("MUSIC PLAYER OFF", lx, top - .032f, .011f, bone);
        p.text(v.offer.batman ? "TURN IT ON: WRIST MENU, BATMAN" : "TURN IT ON: WRIST MENU, PUNISHER", lx, top - .046f, .0058f, khaki);
        return;
    }
    const std::string song = !v.musicAvailable ? std::string("PLAYER UNAVAILABLE")
                             : v.musicTracks <= 0  ? std::string("NO SONGS")
                             : v.musicSong.empty() ? std::string("READY")
                                                   : v.musicSong;
    p.text(clipped(song, .0105f, 2 * W - .020f), lx, top - .027f, .0105f, bone, .0025f, 1, .14f);
    const char* state = !v.musicAvailable ? "OFF" : v.musicPlaying ? "PLAYING" : v.musicPaused ? "PAUSED" : "STOPPED";
    if (v.musicTracks > 0 && v.musicTrack >= 0)
        std::snprintf(line, sizeof line, "%s  %d/%d", state, v.musicTrack + 1, v.musicTracks);
    else
        std::snprintf(line, sizeof line, "%s", state);
    p.text(line, lx, top - .039f, .0060f, v.musicPlaying ? armor * 1.3f : khaki);
    if (v.musicTracks <= 0 || !v.musicAvailable) {
        p.text(v.musicNote.empty() ? "ADD MP3 / OGG / WAV TO DOCUMENTS/HERO VR/MUSIC" : clipped(v.musicNote, .0050f, 2 * W - .020f), lx,
               top - .049f, .0050f, dim * 1.8f);
    } else if (v.musicTotal > 0) {
        const float share = std::clamp(v.musicElapsed / v.musicTotal, 0.f, 1.f);
        p.quad(lx, top - .048f, W - .010f, top - .046f, .0015f, olive);
        p.quad(lx, top - .048f, lx + (W - .010f - lx) * (ok(share) ? share : 0.f), top - .046f, .0018f, khaki * 1.4f);
    }
    // the volume bar (right of VOL+)
    p.text("VOL", W - .060f, top - .010f, .0055f, khaki);
    meter(p, W - .046f, top - .0115f, W - .010f, .0065f, std::clamp(v.musicVolume, 0.f, 100.f) / 100.f, armor);
    Button b[8];
    const uint32_t n = buttons(music, v.offer, b);
    for (uint32_t k = 0; k < n; ++k) {
        if (b[k].action == Action::musicToggle)
            b[k].label = v.musicPlaying ? "PAUSE" : "PLAY";
        drawButton(p, b[k], v.pointer ? v.hotButton == static_cast<int>(k) : v.hover, b[k].action == Action::musicToggle && v.musicPlaying ? blood * .8f : olive);
    }
}

void drawHome(Painter& p, const View& v) {
    const float lx = -W + .010f, top = bodyTop;
    p.text(v.offer.batman && v.carName ? std::string("HOME - BATMOBILE ") + v.carName : std::string("HOME"), lx, top - .010f, .0062f, khaki);
    Button b[8];
    const uint32_t n = buttons(home, v.offer, b);
    for (uint32_t k = 0; k < n; ++k)
        drawButton(p, b[k], v.pointer ? v.hotButton == static_cast<int>(k) : v.hover, b[k].action == Action::goBatcave && !v.offer.batman ? Vec3{.025f, .03f, .05f} : olive);
    if (!n)
        p.text("HIDEOUTS AND SEATED DRIVING ARE OFF", lx, top - .032f, .0060f, bone);
    if (v.homeNote)
        p.text(v.homeNote, lx, bodyBottom + .003f, .0050f, blood * 1.6f);
    else if (v.offer.batman && v.carName && v.offer.car < batmobileCars &&
             std::string_view(batmobileLabels[v.offer.car]).substr(11) != std::string_view(v.carName))
        p.text("THE PICKED BATMOBILE COMES ON THE NEXT SUMMON (X)", lx, bodyBottom + .003f, .0046f, dim * 2.4f);
    else if (v.offer.batman && v.carCredit)
        p.text(clipped(v.carCredit, .0042f, 2 * W - .020f), lx, bodyBottom + .003f, .0042f, dim * 2.0f);
}
} // namespace

uint32_t buttons(uint32_t page, const Offer& offer, Button out[8]) {
    uint32_t n = 0;
    if (page == music && offer.music) {
        static const struct {
            Action a;
            const char* label;
        } row[5] = {{Action::musicPrevious, "<< PREV"}, {Action::musicToggle, "PLAY/PAUSE"}, {Action::musicNext, "NEXT >>"},
                    {Action::volumeDown, "VOL-"},   {Action::volumeUp, "VOL+"}};
        const float x0 = -W + .010f, gap = .004f, bw = (2 * W - .020f - 4 * gap) / 5, y1 = bodyTop - .057f, y0 = bodyBottom + .004f;
        for (int k = 0; k < 5; ++k)
            out[n++] = Button{row[k].a, x0 + k * (bw + gap), y0, x0 + k * (bw + gap) + bw, y1, row[k].label};
    } else if (page == map && offer.crimePopup) {
        // 0.0.74 test 2b CRIME GPS: over the map page's readouts (drawMap's popup panel)
        const float x0 = -W + .008f + .124f + .008f, x1 = W - .010f, y0 = bodyTop - .002f - .124f + .002f;
        out[n++] = Button{Action::crimeTrack, x0, y0, x1, y0 + .020f, "OK - TRACK"};
    } else if (page == home) {
        const float x0 = -W + .010f, x1 = W - .010f, h = .026f, gap = .005f;
        float y = bodyTop - .016f;
        // 0.0.74 test 1 BATMOBILE: the van goes to the safehouse, the Batmobile to the Batcave (moved from the van).
        if (offer.hideouts) {
            out[n++] = offer.batman ? Button{Action::goBatcave, x0, y - h, x1, y, "GO TO BATCAVE"}
                                    : Button{Action::goSafehouse, x0, y - h, x1, y, "GO TO SAFEHOUSE"};
            y -= h + gap;
        }
        if (offer.seat) {
            out[n++] = Button{offer.sat ? Action::stand : Action::sit, x0, y - h, x1, y, offer.sat ? "STAND" : "SIT (SEATED DRIVING)"};
            y -= h + gap;
        }
        // 0.0.74 test 2b: GET OUT shares the last row with the picker (the body holds 3 full rows + this).
        const float xs = offer.exit ? (offer.batman ? x0 + (x1 - x0) * .62f : x0) : x1;
        if (offer.batman)
            out[n++] = Button{Action::nextBatmobile, x0, y - h, offer.exit ? xs - gap : x1, y,
                              batmobileLabels[offer.car < batmobileCars ? offer.car : 0]};
        if (offer.exit)
            out[n++] = Button{Action::exitVehicle, xs, y - h, x1, y, "GET OUT"};
    }
    return n;
}

bool pageShown(uint32_t page, const Offer& offer) { return page < pages && (!offer.batman || (page != cases && page != locker)); }

uint32_t shownPages(const Offer& offer, uint32_t out[pages]) {
    uint32_t n = 0;
    for (uint32_t k = 0; k < pages; ++k)
        if (pageShown(k, offer))
            out[n++] = k;
    return n;
}

Mount mountLocal() { return mountLocal(mountAt); }

Mount mountLocal(Vec3 at) {
    Mount m;
    m.centre = at;
    // Flush in the dash: square to the van, whatever the seat (no turn toward the eye).
    m.right = {1, 0, 0};
    m.up = {0, 1, 0};
    m.normal = {0, 0, 1};
    return m;
}

Vec3 onFace(Vec3 local) { return onFace(local, mountAt, faceScale); }

Vec3 onFace(Vec3 local, Vec3 at, float scale) {
    const Mount m = mountLocal(at);
    const Vec3 d = local - m.centre;
    const float k = ok(scale) && scale >= .5f && scale <= 2.f ? scale : faceScale;
    return Vec3{dot(d, m.right), dot(d, m.up), dot(d, m.normal)} * (1 / k);
}

void Screen::reset() {
    page_ = file_ = 0;
    flash_ = 0;
    near_[0] = near_[1] = hover_ = false;
    action_ = Action::none;
    aimed_[0] = aimed_[1] = trigWas_[0] = trigWas_[1] = false;
}

bool Screen::next() {
    if (page_ == cases && file_ + 1 < files && pageShown(cases, offer_))
        ++file_;
    else {
        do
            page_ = (page_ + 1) % pages;
        while (!pageShown(page_, offer_));
        file_ = 0;
    }
    flash_ = 1;
    return true;
}

bool Screen::previousFile() {
    if (page_ != cases)
        return false;
    file_ = file_ == 0 ? files - 1 : file_ - 1;
    flash_ = 1;
    return true;
}

bool Screen::pick(uint32_t page) {
    if (!pageShown(page, offer_) || page == page_)
        return false;
    page_ = page;
    file_ = 0;
    flash_ = 1;
    return true;
}

bool Screen::pokeAt(Vec3 f) {
    if (f.y >= H - tabHeight) {
        const float tx0 = -W + tabLeft; // the skull's corner picks the first tab
        const float share = std::clamp((f.x - tx0) / (W - tx0), 0.f, .999f);
        uint32_t shown[pages];
        const uint32_t n = shownPages(offer_, shown);
        return pick(shown[std::min(n - 1, static_cast<uint32_t>(share * static_cast<float>(n)))]);
    }
    if (page_ == map && offer_.crimePopup) { // 0.0.74 test 2b CRIME GPS: OK - TRACK, else the next page
        Button b[8];
        const uint32_t n = buttons(page_, offer_, b);
        for (uint32_t k = 0; k < n; ++k)
            if (f.x >= b[k].x0 - .002f && f.x <= b[k].x1 + .002f && f.y >= b[k].y0 - .002f && f.y <= b[k].y1 + .002f) {
                action_ = b[k].action;
                flash_ = .6f;
                return true;
            }
    }
    if (page_ == music || page_ == home) { // 0.0.74: the buttons, nothing between them
        Button b[8];
        const uint32_t n = buttons(page_, offer_, b);
        for (uint32_t k = 0; k < n; ++k)
            if (f.x >= b[k].x0 - .002f && f.x <= b[k].x1 + .002f && f.y >= b[k].y0 - .002f && f.y <= b[k].y1 + .002f) {
                action_ = b[k].action;
                flash_ = .6f;
                return true;
            }
        return false;
    }
    if (page_ == cases && f.x < -W / 3)
        return previousFile();
    return next();
}

void Screen::targetAt(Vec3 f, uint32_t page, const Offer& offer, int& tab, int& button) {
    tab = button = -1;
    if (!van072::fine(f) || std::abs(f.x) > W + raySlack || std::abs(f.y) > H + raySlack)
        return;
    if (f.y >= H - tabHeight) {
        const float tx0 = -W + tabLeft;
        const float share = std::clamp((f.x - tx0) / (W - tx0), 0.f, .999f);
        uint32_t shown[pages];
        const uint32_t n = shownPages(offer, shown);
        if (n)
            tab = static_cast<int>(std::min(n - 1, static_cast<uint32_t>(share * static_cast<float>(n))));
        return;
    }
    if (page == music || page == home || (page == map && offer.crimePopup)) {
        Button b[8];
        const uint32_t n = buttons(page, offer, b);
        for (uint32_t k = 0; k < n; ++k)
            if (f.x >= b[k].x0 - .002f && f.x <= b[k].x1 + .002f && f.y >= b[k].y0 - .002f && f.y <= b[k].y1 + .002f) {
                button = static_cast<int>(k);
                return;
            }
    }
}

bool rayOnFace(Vec3 from, Vec3 dir, Vec3& hit) { return rayOnFace(from, dir, hit, mountAt, faceScale); }

bool rayOnFace(Vec3 from, Vec3 dir, Vec3& hit, Vec3 at, float scale) { return rayOnFace(from, dir, hit, at, scale, raySlack); }

Vec3 snapToButton(Vec3 f, uint32_t page, const Offer& offer) {
    if (!van072::fine(f))
        return f;
    f = {std::clamp(f.x, -W, W), std::clamp(f.y, -H, H), 0};
    if (f.y >= H - tabHeight || !(page == music || page == home || (page == map && offer.crimePopup)))
        return f;
    Button b[8];
    const uint32_t n = buttons(page, offer, b);
    finger_ray::Box box[8];
    for (uint32_t k = 0; k < n && k < 8; ++k)
        box[k] = {b[k].x0, b[k].y0, b[k].x1, b[k].y1};
    float cx = f.x, cy = f.y;
    if (finger_ray::snap(f.x, f.y, box, std::min<uint32_t>(n, 8), finger_ray::dashSnap, cx, cy) >= 0)
        f = {cx, cy, 0};
    return f;
}

bool rayOnFace(Vec3 from, Vec3 dir, Vec3& hit, Vec3 at, float scale, float slack) {
    if (!ok(slack) || slack < 0 || slack > .2f)
        slack = raySlack;
    hit = {};
    if (!van072::fine(at) || !ok(scale) || scale < .5f || scale > 2.f)
        return false;
    if (!van072::fine(from) || !van072::fine(dir))
        return false;
    const float n = length(dir);
    if (!ok(n) || n < .5f)
        return false;
    const Mount m = mountLocal(at);
    const Vec3 d = dir * (1 / n);
    const Vec3 f0 = onFace(from, at, scale);
    const Vec3 df{dot(d, m.right), dot(d, m.up), dot(d, m.normal)}; // unit, the face's axes
    if (!van072::fine(f0) || !van072::fine(df) || f0.z <= 0 || df.z > -rayFacing)
        return false; // behind the face, or pointing away / along it
    const float t = -f0.z / df.z; // design units along the ray
    if (!ok(t) || t * scale > rayReach)
        return false;
    const Vec3 h = f0 + df * t;
    if (!van072::fine(h) || std::abs(h.x) > W + slack || std::abs(h.y) > H + slack)
        return false;
    hit = {std::clamp(h.x, -W, W), std::clamp(h.y, -H, H), 0};
    return true;
}

bool Screen::step(const In& in) {
    if (ok(in.dt) && in.dt > 0)
        flash_ = std::max(0.f, flash_ - in.dt * 4.f);
    hover_ = false;
    pointer_ = in.pointer;
    cursorOn_ = beamOn_ = false;
    hotTab_ = hotButton_ = -1;
    offer_ = in.offer;
    if (van072::fine(in.at) && ok(in.scale) && in.scale >= .5f && in.scale <= 2.f) {
        at_ = in.at;
        scale_ = in.scale;
    }
    if (!pageShown(page_, offer_)) { // the Batmobile's screen has no CASES / LOCKER
        page_ = map;
        file_ = 0;
    }
    bool changed = false;
    if (!in.driving) {
        near_[0] = near_[1] = false;
        aimed_[0] = aimed_[1] = false;
        trigWas_[0] = trigWas_[1] = false;
        return false;
    }
    if (in.x)
        changed |= next();
    for (unsigned i = 0; i < 2; ++i) {
        // the trigger's press, with its own re-arm (a held trigger pages once)
        const float tr = ok(in.trigger[i]) ? in.trigger[i] : 0.f;
        const bool pressed = trigWas_[i] ? tr > rayRelease : tr > rayPress;
        const bool fresh = pressed && !trigWas_[i];
        trigWas_[i] = pressed;
        aimed_[i] = false;
        if (!in.tracked[i] || !van072::fine(in.hand[i])) {
            near_[i] = false;
            continue;
        }
        // 0.0.74 test 2 DASH POINTER: the fingertip, out along the aim from the grip pose.
        Vec3 touchAt = in.hand[i];
        if (in.pointer && !in.finger && van072::fine(in.aim[i])) { // FINGER RAY: the hand is the tip
            const float an = length(in.aim[i]);
            if (ok(an) && an > .5f)
                touchAt = in.hand[i] + in.aim[i] * (fingerReach / an);
        }
        Vec3 f = onFace(touchAt, at_, scale_);
        // In the face's box, from just in front to a little through it (a poke goes in).
        const bool near = van072::fine(f) && f.z <= pokeDepth && f.z >= -pokeThrough && std::abs(f.x) <= W + pokeSlack &&
                          std::abs(f.y) <= H + pokeSlack;
        // An open hand, off the wheel: a hand that grips (the wheel, a door) never pages it. With the pointer
        // a pointing hand (the index out: trigger released) is open too.
        const bool pointing = in.pointer && tr < rayRelease;
        const bool open = ok(in.grip[i]) && (in.grip[i] < pokeGripMost || pointing) && !in.onWheel[i];
        hover_ |= near && open;
        if (near && in.finger) // FINGER RAY: the touch snaps to the nearest button
        {
            const Vec3 sn = snapToButton(f, page_, offer_);
            f = Vec3{sn.x, sn.y, f.z};
        }
        if (near && open && in.pointer) {
            cursorOn_ = true;
            cursor_ = {std::clamp(f.x, -W, W), std::clamp(f.y, -H, H), 0};
            targetAt(cursor_, page_, offer_, hotTab_, hotButton_);
        }
        if (near && !near_[i] && open)
            changed |= pokeAt(f);
        near_[i] = near; // a hand already in (or gripping in) must come out first
        // RAY POKE: the free hand pointed at the face (not already touching it); a fresh trigger pages it there.
        Vec3 hit{};
        if (in.ray && !near && !in.onWheel[i] &&
            rayOnFace(in.hand[i], in.aim[i], hit, at_, scale_, in.finger ? finger_ray::dashSlack : raySlack)) {
            if (in.finger) // FINGER RAY: the cursor (and the press) snap to the nearest button
                hit = snapToButton(hit, page_, offer_);
            aimed_[i] = true;
            hover_ = true;
            if (in.pointer && !cursorOn_) {
                cursorOn_ = beamOn_ = true;
                cursor_ = hit;
                beamFrom_ = in.hand[i];
                targetAt(hit, page_, offer_, hotTab_, hotButton_);
            }
            if (fresh)
                changed |= pokeAt(hit);
        }
    }
    return changed;
}

float headingOf(const van072::Frame& frame) {
    const Vec3 f = frame.forward();
    float h = std::atan2(f.x, -f.z);
    if (h < 0)
        h += tau;
    return h;
}

View view(const van072::Frame& frame, const Screen& s, float speed, float topSpeed, float rpm, bool driving) {
    return view(frame, s, speed, topSpeed, rpm, driving, mountAt, faceScale);
}

View view(const van072::Frame& frame, const Screen& s, float speed, float topSpeed, float rpm, bool driving, Vec3 at, float scale) {
    View v;
    if (!van072::fine(frame.at) || !ok(frame.yaw) || !van072::fine(at) || !ok(scale) || scale < .5f || scale > 2.f)
        return v;
    const Mount m = mountLocal(at);
    v.centre = frame.world(m.centre);
    v.right = van072::worldDir(frame, m.right);
    v.up = van072::worldDir(frame, m.up);
    v.scale = scale;
    v.page = s.page();
    v.file = s.file();
    v.speed = ok(speed) ? speed : 0.f;
    v.topSpeed = ok(topSpeed) && topSpeed > 1 ? topSpeed : 16.f;
    v.heading = headingOf(frame);
    v.rpm = ok(rpm) ? std::clamp(rpm, 0.f, 1.f) : 0.f;
    v.gridX = frame.at.x;
    v.gridZ = frame.at.z;
    v.driving = driving;
    v.flash = s.flash();
    v.hover = s.hover();
    if (s.pointer()) { // 0.0.74 test 2 DASH POINTER
        v.pointer = true;
        v.cursorOn = s.cursorOn();
        v.cursorX = s.cursor().x;
        v.cursorY = s.cursor().y;
        v.beamOn = s.beamOn() && van072::fine(s.beamFrom());
        v.beamFrom = v.beamOn ? frame.world(s.beamFrom()) : Vec3{};
        v.hotTab = s.hotTab();
        v.hotButton = s.hotButton();
    }
    v.on = true;
    return v;
}

bool append(std::vector<Vertex>& out, const View& v, Vec3 viewer) {
    if (!v.on || !van072::fine(v.centre) || !van072::fine(v.right) || !van072::fine(v.up) || !ok(v.scale) || v.scale < .5f || v.scale > 2.f ||
        !ok(v.speed) || !ok(v.heading) || !ok(v.rpm) || !ok(v.gridX) || !ok(v.gridZ) || !ok(v.flash) || v.page >= pages)
        return false;
    const Vec3 r = normalized(v.right), u = normalized(v.up), n = cross(r, u);
    if (!(std::abs(length(r) - 1) < .01f && std::abs(length(u) - 1) < .01f && std::abs(dot(r, u)) < .02f))
        return false;
    if (!van072::fine(viewer) || dot(viewer - v.centre, n) <= 0)
        return false; // behind it (or no eye): nothing
    if (std::abs(v.gridX) > 1e6f || std::abs(v.gridZ) > 1e6f)
        return false;
    const size_t from = out.size();
    try {
        setTheme(v.offer.batman);
        Painter p{out, v.centre, r * v.scale, u * v.scale, n * v.scale, v.driving ? 1.f : .7f};
        const float b = .010f; // the bezel
        // built into the dash: a flat trim plate flush on the fascia round the bezel (no bracket, no tilt)
        p.quad(-W - b * 1.8f, -H - b * 1.8f, W + b * 1.8f, H + b * 1.8f, -bezelDepth, gunmetal * .7f, 1);
        p.quad(-W - b, -H - b, W + b, H + b, -bezelDepth + .0005f, gunmetal, 1);
        p.quad(-W - b * .6f, -H - b * .6f, W + b * .6f, H + b * .6f, -.006f, gunmetal * 1.6f, 1);
        p.frame(-W - b, -H - b, W + b, H + b, .0016f, -.0058f, olive * 1.4f, 1);
        for (int sx = -1; sx <= 1; sx += 2) // the bolts
            for (int sy = -1; sy <= 1; sy += 2)
                p.disc(sx * (W + b * .5f), sy * (H + b * .5f), .0024f, -.0054f, khaki * .7f, 1, 8);
        p.text("MADE BY AGENT MULDER", W + b * .2f - textWidth("MADE BY AGENT MULDER", .0034f), -H - b * .82f, .0034f, dim * 1.5f, -.005f);
        p.quad(-W, -H, W, H, -.004f, faceDark, .97f);
        for (float y = -H + .002f; y < H; y += .0045f) // scanlines
            p.quad(-W, y, W, y + .0008f, -.0035f, olive, .35f);
        // the tab row with the skull
        const float ty = H - tabHeight;
        p.quad(-W, ty, W, H, .0005f, v.offer.batman ? Vec3{.004f, .014f, .03f} : Vec3{.02f, .022f, .013f});
        if (v.offer.batman)
            batEmblem(p, -W + .017f, ty + tabHeight * .5f, .030f, blood, .001f);
        else
            skull(p, -W + .016f, ty + tabHeight * .52f, .030f, bone, .001f);
        uint32_t shown[pages];
        const uint32_t tabs = shownPages(v.offer, shown);
        uint32_t at = 0;
        const float tx0 = -W + .034f, tw = (2 * W - .034f) / static_cast<float>(tabs);
        for (uint32_t j = 0; j < tabs; ++j) {
            const uint32_t k = shown[j];
            const float x0 = tx0 + j * tw, x1 = x0 + tw - .002f;
            const bool on = k == v.page;
            if (on)
                at = j;
            p.quad(x0, ty + .003f, x1, H - .003f, .0008f, on ? blood : olive);
            if (on)
                p.quad(x0, ty + .003f, x1, ty + .0045f, .0011f, bone);
            if (v.pointer && v.hotTab == static_cast<int>(j)) // 0.0.74 test 2 DASH POINTER: the tab under it
                p.frame(x0, ty + .003f, x1, H - .003f, .0011f, .0019f, bone);
            const float th = std::min(.0090f, fittedHeight(pageNames[k], .0090f, x1 - x0 - .004f)); // 0.0.74: six tabs
            p.textCentre(pageNames[k], (x0 + x1) / 2, ty + tabHeight / 2 - th / 2, th, on ? bone : khaki, .0015f);
        }
        p.quad(-W, ty - .0012f, W, ty, .0012f, khaki * .8f);
        // the corner ticks (a sight's frame)
        for (int sx = -1; sx <= 1; sx += 2)
            for (int sy = -1; sy <= 1; sy += 2) {
                const float x = sx * (W - .003f), y = sy > 0 ? ty - .003f : -H + .003f;
                p.quad(std::min(x, x - sx * .010f), y - (sy > 0 ? .0009f : 0), std::max(x, x - sx * .010f), y + (sy > 0 ? 0 : .0009f), .0013f, khaki * .9f);
                p.quad(x - (sx > 0 ? .0009f : 0), std::min(y, y - sy * .010f), x + (sx > 0 ? 0 : .0009f), std::max(y, y - sy * .010f), .0013f, khaki * .9f);
            }
        switch (v.page) {
        case map: drawMap(p, v); break;
        case status: drawStatus(p, v); break;
        case cases: drawCase(p, v); break;
        case music: drawMusic(p, v); break;
        case home: drawHome(p, v); break;
        default: drawLocker(p, v); break;
        }
        // 0.0.74 test 2b CRIME GPS: the popup's OK button, and the arrow over the dash to the tracked crime (lying
        // flat over the screen's top edge, pointing its way from the car's nose; its distance and compass point)
        if (v.crimeOn && v.page == map && v.crimePopup && v.offer.crimePopup) {
            Button b[8];
            const uint32_t n = buttons(map, v.offer, b);
            for (uint32_t k = 0; k < n; ++k)
                drawButton(p, b[k], v.pointer ? v.hotButton == static_cast<int>(k) : v.hover, blood * .9f);
        }
        if (v.crimeOn && v.crimeTracked >= 0 && ok(v.crimeMetres) && ok(v.crimeBearing)) {
            float rel = v.crimeBearing - v.heading * 360.f / tau;
            rel = std::fmod(rel, 360.f);
            if (rel > 180.f)
                rel -= 360.f;
            if (rel < -180.f)
                rel += 360.f;
            const float a = rel * tau / 360.f, sx = std::sin(a), sz = -std::cos(a); // x right, -z ahead (the face's normal is toward him)
            const float y = H + .034f, L = .028f, w = .010f;
            const Vec3 gold{1.f, .70f, .12f};
            const Vec3 tip = p.P(sx * L, y, .02f + sz * L), tail = p.P(-sx * L * .6f, y, .02f - sz * L * .6f);
            const Vec3 side = p.P(-sz * w, y, .02f + sx * w) - p.P(0, y, .02f);
            const Vec3 mid = p.P(sx * L * .1f, y, .02f + sz * L * .1f);
            p.tri(tip, mid + side, mid - side, gold, .95f);
            p.tri(mid + side * .45f, tail + side * .45f, tail - side * .45f, gold, .8f);
            p.tri(mid + side * .45f, tail - side * .45f, mid - side * .45f, gold, .8f);
            char cl[40];
            std::snprintf(cl, sizeof cl, "CRIME %d M %s", static_cast<int>(v.crimeMetres + .5f), compassName(v.crimeBearing));
            p.textCentre(cl, 0, H + .046f, .0075f, gold, .02f);
        }
        // the footer
        p.quad(-W, -H, W, -H + .012f, .0005f, v.offer.batman ? Vec3{.004f, .014f, .03f} : Vec3{.02f, .022f, .013f});
        p.text(v.page == cases ? "X: NEXT  TAP LEFT: BACK" : v.page == music || v.page == home ? "TAP A BUTTON  X: NEXT PAGE" : "X OR TAP: NEXT PAGE",
               -W + .006f, -H + .0035f, .0050f, khaki);
        char line[16];
        std::snprintf(line, sizeof line, "%u/%u", at + 1, tabs);
        p.textRight(line, W - .006f, -H + .0035f, .0050f, khaki);
        if (v.hover)
            p.frame(-W, -H, W, H, .0012f, .003f, bone, .6f);
        if (v.flash > 0)
            p.frame(-W, -H, W, H, .0025f, .0032f, blood * 2.f, std::min(1.f, v.flash));
        // 0.0.74 test 2 DASH POINTER: the beam from the hand and the cursor (the holo roster's look)
        if (v.pointer && v.cursorOn && ok(v.cursorX) && ok(v.cursorY)) {
            const float cx = std::clamp(v.cursorX, -W, W), cy = std::clamp(v.cursorY, -H, H);
            if (v.beamOn && van072::fine(v.beamFrom)) {
                const Vec3 to = p.P(cx, cy, .004f);
                const Vec3 mid = (v.beamFrom + to) * .5f;
                const Vec3 look = viewer - mid;
                if (length(to - v.beamFrom) > .02f && length(look) > 1e-3f)
                    soft::softBar(out, v.beamFrom, to, normalized(look), .0012f, bone, .45f * p.a, .0015f);
            }
            p.disc(cx, cy, .0026f, .0045f, bone, .95f, 12);
            p.ring(cx, cy, .0048f, .0012f, .0045f, v.offer.batman ? Vec3{.3f, .7f, 1.f} : khaki * 1.6f, .7f, 20);
        }
        for (size_t i = from; i < out.size(); ++i)
            if (!van072::fine(out[i].position) || !van072::fine(out[i].color) || !ok(out[i].alpha)) {
                out.resize(from);
                return false;
            }
        return out.size() > from;
    } catch (...) {
        out.resize(from);
        return false;
    }
}

} // namespace spidy::van_screen
