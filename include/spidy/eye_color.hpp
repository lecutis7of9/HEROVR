// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR (Ilya Mezerowsky).
#pragma once
// Update 37.2: COLOR / ATMOSPHERE (the hologram's COLOR page; live.txt color_fx, color_filter,
// color_strength, exposure, contrast, saturation, vibrance, warmth, tint, vignette).
//
// Where it runs: inside the eye sharpening pass (native_eye_gpu.cpp, Update 16), the one
// fullscreen draw that already writes both staged eyes into the headset's images. The same
// shader and the same constants draw both eyes, per pixel, from colour alone (the vignette from
// each eye image's own centre), so the two eyes match exactly and nothing turns with the head
// (unlike the old bloom bug, which came from the game's head-locked lens effects). When COLOR
// FX is OFF the constants say so and the shader skips it: the pass is as in 37/37.1; with
// sharpening at 0 and COLOR FX ON the pass runs with no sharpening (peak 0).
// The HUD, webs and hologram are drawn over it afterwards: never tinted. The desktop window
// shows the eye before the pass (no grading there, as with sharpening).
//
// The game's own tonemap / colour grading / exposure: no confirmed path was found in this build
// (no address is used or claimed; see docs/color-atmosphere.md). Plan B if the pass ever fails
// to build: the pass stops for the session (sharpenFault, as since Update 16) and the eyes are
// copied as before - no grade, no crash, no flat screen.
//
// This file is the CPU copy of the shader's arithmetic (the tests and the preview pictures use it):
// the HLSL in native_eye_gpu.cpp is the same steps in the same order.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace spidy::eye_color {
// Update 38.5: ANIMATED90S, the 1994 animated series' look (cel bands, ink lines, painted flats).
enum Filter : uint32_t { none = 0, vivid, goldenHour, nightNeon, raimi2000s, noir, animated90s, filters };
inline constexpr const char* filterNames[filters] = {"NONE", "VIVID", "GOLDEN HOUR", "NIGHT NEON", "2000s RAIMI",
                                                     "NOIR", "90s ANIMATED"};
inline const char* filterName(uint32_t f) { return f < filters ? filterNames[f] : "NONE"; }
inline constexpr const char* filterKeys[filters] = {"none", "vivid", "golden", "neon", "raimi", "noir", "animated"};

// The player's settings (whole numbers, as the sliders and live.txt hold them).
struct Config {
    bool on = false;          // COLOR FX, default OFF (the user picks after the previews)
    uint32_t filter = none;   // FILTER
    uint32_t strength = 100;  // STRENGTH 0-100: how much of the filter
    int32_t exposureTenths{}; // EXPOSURE -20..20 (-2.0 to +2.0 EV), 0 the game's
    uint32_t contrast = 100;  // CONTRAST 50-150, 100 the game's
    uint32_t saturation = 100; // SATURATION 0-200, 100 the game's
    uint32_t vibrance{};      // VIBRANCE 0-100
    int32_t warmth{};         // WARMTH -100..100 (blue .. amber)
    int32_t tint{};           // TINT -100..100 (green .. magenta)
    uint32_t vignette{};      // VIGNETTE 0-100, default off
    // Update 38.5: 90s ANIMATED's own three (used only with that FILTER; the hologram shows them then).
    uint32_t cartoon = 60;    // CARTOON STRENGTH 0-100: painted flats, ink width and band hardness
    bool outlines = true;     // OUTLINES
    uint32_t bands = 4;       // BANDS 3, 4 or 5
    // Version 0.0.6 DAREDEVIL SONAR: on while DAREDEVIL ABILITIES is on (game_xr sets it, with on);
    // it takes the place of the FILTER and the grade (sonarPixel, the toon pipeline's sonar mode).
    bool sonar = false;
};
struct Limits {
    int lo, hi, step;
};
inline constexpr Limits strengthLimits{0, 100, 5}, exposureLimits{-20, 20, 1}, contrastLimits{50, 150, 5},
    saturationLimits{0, 200, 5}, vibranceLimits{0, 100, 5}, warmthLimits{-100, 100, 5}, tintLimits{-100, 100, 5},
    vignetteLimits{0, 100, 5}, cartoonLimits{0, 100, 10}, bandLimits{3, 5, 1};

// A filter's look at full strength (NONE: all neutral).
struct Look {
    float ev = 0, contrast = 1, saturation = 1, vibrance = 0, warmth = 0, tint = 0, crush = 0, mono = 0,
          vignette = 0;
    std::array<float, 3> shadow{}, high{}; // split toning: added in the shadows / highlights
};
inline Look look(uint32_t f) {
    Look l;
    switch (f) {
    case vivid:
        l.ev = .1f; l.contrast = 1.10f; l.saturation = 1.30f; l.vibrance = .35f;
        break;
    case goldenHour:
        l.ev = .15f; l.contrast = 1.05f; l.saturation = 1.10f; l.warmth = .6f; l.tint = .05f; l.vignette = .25f;
        l.shadow = {.02f, 0, -.03f};
        l.high = {.06f, .03f, -.02f};
        break;
    case nightNeon:
        l.ev = -.25f; l.contrast = 1.15f; l.saturation = 1.35f; l.vibrance = .4f; l.warmth = -.35f; l.tint = .25f;
        l.vignette = .3f;
        l.shadow = {0, .01f, .05f};
        l.high = {.05f, -.01f, .04f};
        break;
    case raimi2000s: // warm, filmic, slightly crushed
        l.ev = .05f; l.contrast = 1.12f; l.saturation = .92f; l.vibrance = .1f; l.warmth = .35f; l.crush = .04f;
        l.vignette = .2f;
        l.shadow = {.01f, 0, -.02f};
        l.high = {.04f, .03f, 0};
        break;
    case noir:
        l.ev = -.1f; l.contrast = 1.35f; l.crush = .05f; l.mono = 1; l.vignette = .45f;
        break;
    case animated90s: // bold primaries, deep blue shadows, warm orange highlights (the cel pass adds the rest)
        l.ev = .06f; l.contrast = 1.10f; l.saturation = 1.30f; l.vibrance = .30f;
        l.shadow = {-.04f, 0, .08f};
        l.high = {.05f, .025f, -.03f};
        break;
    default:
        break;
    }
    return l;
}

// The shader's constants after the sharpening's four (24 floats in all; native_eye_gpu.cpp).
struct Params {
    float on = 0, gain = 1, contrast = 1, crush = 0;      // gain = 2^EV (in light)
    float saturation = 1, vibrance = 0, mono = 0, vignette = 0;
    std::array<float, 3> balance{1, 1, 1};                // warmth and tint, per channel
    std::array<float, 3> shadow{}, high{};
};
inline float fin(float v, float d) { return std::isfinite(v) ? v : d; }
inline Params params(const Config& c) {
    Params p;
    if (!c.on)
        return p;
    if (c.sonar) { // DAREDEVIL SONAR draws its own look (sonarPixel): no grade
        p.on = 1;
        return p;
    }
    const Look l = look(c.filter);
    const float s = static_cast<float>(std::min(c.strength, 100u)) / 100.f;
    const float ev = static_cast<float>(std::clamp(c.exposureTenths, exposureLimits.lo, exposureLimits.hi)) / 10.f +
                     l.ev * s;
    p.on = 1;
    p.gain = std::exp2(ev);
    p.contrast = static_cast<float>(std::clamp<uint32_t>(c.contrast, 50, 150)) / 100.f * (1 + (l.contrast - 1) * s);
    p.crush = l.crush * s;
    p.saturation = static_cast<float>(std::min(c.saturation, 200u)) / 100.f * (1 + (l.saturation - 1) * s);
    p.vibrance = static_cast<float>(std::min(c.vibrance, 100u)) / 100.f + l.vibrance * s;
    p.mono = l.mono * s;
    p.vignette = std::clamp(static_cast<float>(std::min(c.vignette, 100u)) / 100.f + l.vignette * s, 0.f, 1.f);
    const float w = std::clamp(static_cast<float>(std::clamp(c.warmth, -100, 100)) / 100.f + l.warmth * s, -1.f, 1.f);
    const float t = std::clamp(static_cast<float>(std::clamp(c.tint, -100, 100)) / 100.f + l.tint * s, -1.f, 1.f);
    p.balance = {1 + .10f * w + .05f * t, 1 - .08f * t, 1 - .10f * w + .05f * t};
    for (int i = 0; i < 3; ++i) {
        p.shadow[static_cast<size_t>(i)] = l.shadow[static_cast<size_t>(i)] * s;
        p.high[static_cast<size_t>(i)] = l.high[static_cast<size_t>(i)] * s;
    }
    return p;
}
// True when the settings change nothing (the shader may skip them).
inline bool neutral(const Params& p) {
    return p.on == 0 || (p.gain == 1 && p.contrast == 1 && p.crush == 0 && p.saturation == 1 && p.vibrance == 0 &&
                     p.mono == 0 && p.vignette == 0 && p.balance == std::array<float, 3>{1, 1, 1} &&
                     p.shadow == std::array<float, 3>{} && p.high == std::array<float, 3>{});
}

inline float toLinear(float c) { return c <= .04045f ? c / 12.92f : std::pow((c + .055f) / 1.055f, 2.4f); }
inline float toDisplay(float l) {
    return l <= .0031308f ? l * 12.92f : 1.055f * std::pow(l, 1 / 2.4f) - .055f;
}
inline float saturate(float v) { return std::clamp(fin(v, 0), 0.f, 1.f); }
// Highlights roll off above .8 instead of clipping (EXPOSURE up keeps the sky's shape).
inline float shoulder(float x) { return x <= .8f ? x : .8f + .2f * (1 - std::exp(-(x - .8f) / .2f)); }

// One pixel, display-encoded rgb 0-1 (as the eyes are); (u, v) 0-1 across the eye image.
// The steps, in this order: EXPOSURE (in light), CONTRAST and the crush, SATURATION and
// VIBRANCE, WARMTH / TINT, the split toning, NOIR's grey, VIGNETTE.
inline std::array<float, 3> apply(const Params& p, std::array<float, 3> c, float u, float v) {
    for (auto& x : c)
        x = saturate(x);
    if (!(p.on > .5f))
        return c;
    if (p.gain != 1)
        for (auto& x : c)
            x = shoulder(toDisplay(toLinear(x) * p.gain));
    for (auto& x : c) {
        x = (x - .5f) * p.contrast + .5f;
        x = (x - p.crush) / (1 - p.crush);
        x = saturate(x);
    }
    const auto luma = [](const std::array<float, 3>& k) { return .2126f * k[0] + .7152f * k[1] + .0722f * k[2]; };
    float y = luma(c);
    const float mx = std::max({c[0], c[1], c[2]}), mn = std::min({c[0], c[1], c[2]});
    // Skin (red over green over blue) gets 30% of the vibrance: dull colours most, faces least.
    const float skin = saturate((c[0] - c[1]) * 4) * saturate((c[1] - c[2]) * 4);
    const float boost = p.saturation * (1 + p.vibrance * (1 - (mx - mn)) * (1 - .7f * skin));
    for (auto& x : c)
        x = saturate(y + (x - y) * boost);
    for (size_t i = 0; i < 3; ++i)
        c[i] = saturate(c[i] * p.balance[i]);
    y = luma(c);
    for (size_t i = 0; i < 3; ++i)
        c[i] = saturate(c[i] + p.shadow[i] * (1 - y) * (1 - y) + p.high[i] * y * y);
    y = luma(c);
    for (auto& x : c)
        x = x + (y - x) * p.mono;
    const float du = fin(u, .5f) - .5f, dv = fin(v, .5f) - .5f;
    const float d2 = (du * du + dv * dv) * 2; // 0 at the centre, 1 at the corners
    const float dim = saturate(1 - p.vignette * .7f * d2);
    for (auto& x : c)
        x = saturate(x * dim);
    return c;
}

// The subtexts: one sentence each.
inline constexpr const char* help = "Live colour grading for both eyes in one cheap pass; OFF is the game's look.";
inline constexpr const char* filterHelp = "A look: VIVID, GOLDEN HOUR, NIGHT NEON, 2000s RAIMI, NOIR or 90s ANIMATED.";
inline constexpr const char* strengthHelp = "How much of the FILTER is used: 0 none, 100 all of it.";
inline constexpr const char* exposureHelp = "Brightens or darkens the picture in EV steps: 0 is the game's.";
inline constexpr const char* contrastHelp = "Darker darks and brighter brights above 100, flatter below.";
inline constexpr const char* saturationHelp = "How strong every colour is: 100 is the game's, 0 black and white.";
inline constexpr const char* vibranceHelp = "Boosts dull colours most and leaves skin tones nearly alone.";
inline constexpr const char* warmthHelp = "Warmer (amber) to the right, cooler (blue) to the left.";
inline constexpr const char* tintHelp = "Magenta to the right, green to the left.";
inline constexpr const char* vignetteHelp = "Darkens the edges of each eye a little: 0 is off.";
inline constexpr const char* cartoonHelp = "90s ANIMATED: how flat and painted it looks, and how bold the ink lines are.";
inline constexpr const char* outlinesHelp = "90s ANIMATED: dark ink lines around shapes, the same in both eyes.";
inline constexpr const char* bandsHelp = "90s ANIMATED: how many flat shades each colour gets (3 is the most cartoony).";

// Update 38.5: 90s ANIMATED's cel pass (native_eye_gpu.cpp toonShader is the same steps in the
// same order). Per pixel, from the eye image alone, so both eyes get the same treatment at the same
// pixel scale (the outline width is a whole number of pixels, the same for both eyes and every
// frame, so lines neither shimmer between the eyes nor crawl):
//  1. PAINT: an anisotropic-weighted Kuwahara over a 5x5 grid of taps at `stride` pixels (4 corner
//     quadrants of 3x3; each quadrant weighted 1/(variance)^2 so flat areas win and edges stay
//     sharp). Bricks, cloth wrinkles and noise turn into flat painted areas.
//  2. the FILTER's grade (apply(), above), then teal lights (bright blue-greens a little greener).
//  3. CEL BANDS: the brightness snapped to `bands` soft steps (smooth edges, so a sky does not band
//     harshly), the colour kept.
//  4. INK: the edge from the quadrant means (a smoothed Sobel: texture does not draw lines), plus
//     the colour difference across it; stronger silhouettes get darker, wider lines (the game's depth
//     is not available to this pass - no confirmed address - so contrast stands in for it).
//  5. PAPER: a slight lift of the blacks and a warm paper flatness.
struct Toon {
    float on = 0, paint = 0, bands = 4, ink = 0; // ink 0 = OUTLINES OFF
    int stride = 1;                              // 1 or 2 pixels (CARTOON STRENGTH 50+)
    float sonar = 0;                             // Version 0.0.6 DAREDEVIL SONAR (1: on)
};
inline Toon toon(const Config& c) {
    Toon t;
    if (c.on && c.sonar) {
        t.on = 1;
        t.sonar = 1;
        return t;
    }
    if (!c.on || c.filter != animated90s)
        return t;
    const float k = static_cast<float>(std::min(c.cartoon, 100u)) / 100.f;
    t.on = 1;
    t.paint = k;
    t.bands = static_cast<float>(std::clamp<uint32_t>(c.bands, 3, 5));
    t.ink = c.outlines ? .55f + .45f * k : 0.f;
    t.stride = k >= .5f ? 2 : 1;
    return t;
}
inline float smoothstep(float a, float b, float x) {
    const float t = std::clamp((x - a) / (b - a), 0.f, 1.f);
    return t * t * (3 - 2 * t);
}
// One output pixel; fetch(x, y) gives the eye's display-encoded rgb, clamped to the image.
template <class Fetch>
std::array<float, 3> cartoonPixel(const Params& p, const Toon& t, Fetch&& fetch, int x, int y, float u, float v) {
    using C = std::array<float, 3>;
    const auto luma = [](const C& k) { return .2126f * k[0] + .7152f * k[1] + .0722f * k[2]; };
    C grid[5][5];
    for (int j = 0; j < 5; ++j)
        for (int i = 0; i < 5; ++i)
            grid[j][i] = fetch(x + (i - 2) * t.stride, y + (j - 2) * t.stride);
    C mean[4]{};
    float var[4]{};
    for (int q = 0; q < 4; ++q) {
        const int i0 = (q & 1) * 2, j0 = (q >> 1) * 2;
        float s = 0, s2 = 0;
        for (int j = j0; j < j0 + 3; ++j)
            for (int i = i0; i < i0 + 3; ++i) {
                for (size_t c = 0; c < 3; ++c)
                    mean[q][c] += grid[j][i][c] / 9.f;
                const float l = luma(grid[j][i]);
                s += l;
                s2 += l * l;
            }
        var[q] = std::max(s2 / 9.f - (s / 9.f) * (s / 9.f), 0.f);
    }
    C painted{};
    float total = 0;
    for (int q = 0; q < 4; ++q) {
        const float w = 1.f / ((var[q] * 400.f + 1e-3f) * (var[q] * 400.f + 1e-3f));
        total += w;
        for (size_t c = 0; c < 3; ++c)
            painted[c] += mean[q][c] * w;
    }
    C col = grid[2][2];
    for (size_t c = 0; c < 3; ++c)
        col[c] = saturate(col[c] + (painted[c] / total - col[c]) * t.paint);
    // 2. the grade, then the show's deep blue darks and teal lights
    col = apply(p, col, u, v);
    float yl = luma(col);
    const float dark = .65f * smoothstep(.45f, .08f, yl);
    const C night{.55f, .72f, 1.35f};
    for (size_t c = 0; c < 3; ++c)
        col[c] = saturate(col[c] * (1 + (night[c] - 1) * dark));
    yl = luma(col);
    const float teal = saturate((col[2] - col[0]) * 3.f) * saturate((yl - .45f) * 3.f);
    col[1] = saturate(col[1] + .10f * teal);
    // 3. cel bands (soft steps)
    yl = luma(col);
    const float f = yl * t.bands;
    const float base = std::floor(f), fr = f - base;
    const float hard = .30f - .18f * t.paint; // the step's softness: softer at low strength
    const float banded = (base + smoothstep(.5f - hard, .5f + hard, fr)) / t.bands;
    const float mix = .55f + .4f * t.paint;
    const float target = yl + (std::min(banded, 1.f) - yl) * mix;
    const float scale = target / std::max(yl, 1e-3f);
    for (auto& c : col)
        c = saturate(c * scale);
    // 4. ink from the quadrant means (0 top-left, 1 top-right, 2 bottom-left, 3 bottom-right)
    if (t.ink > 0) {
        float gx = 0, gy = 0, chroma = 0;
        const float l0 = luma(mean[0]), l1 = luma(mean[1]), l2 = luma(mean[2]), l3 = luma(mean[3]);
        gx = (l1 + l3) - (l0 + l2);
        gy = (l2 + l3) - (l0 + l1);
        for (size_t c = 0; c < 3; ++c)
            chroma = std::max({chroma, std::abs((mean[1][c] + mean[3][c]) - (mean[0][c] + mean[2][c])),
                               std::abs((mean[2][c] + mean[3][c]) - (mean[0][c] + mean[1][c]))});
        const float e = std::sqrt(gx * gx + gy * gy) + .35f * chroma;
        const float lo = .16f - .05f * t.paint, hi = lo + .10f;
        const float ink = smoothstep(lo, hi, e) * t.ink * (.75f + .25f * smoothstep(hi, hi + .3f, e));
        const C inkColour{.05f, .04f, .10f};
        for (size_t c = 0; c < 3; ++c)
            col[c] = col[c] + (inkColour[c] - col[c]) * ink;
    }
    // 5. paper
    const C paper{1.f, .97f, .90f};
    for (size_t c = 0; c < 3; ++c)
        col[c] = saturate(.025f * t.paint + col[c] * (1 - .025f * t.paint) * (1 - .06f * t.paint + .06f * t.paint * paper[c]));
    return col;
}

// Telemetry, SpidyColorData (tools/update37_records.py): the settings and how often it ran.
inline constexpr uint32_t dataMagic = 0x524c4f43; // COLR
struct Data {
    uint32_t magic = dataMagic, version = 2, bytes = 80, on{};
    uint32_t filter{}, strength{}, contrast{}, saturation{};
    int32_t exposureTenths{}, warmth{}, tint{};
    uint32_t vibrance{}, vignette{}, fault{};
    // Update 38.5 (version 2): 90s ANIMATED's CARTOON STRENGTH, and OUTLINES (bit 0) | BANDS << 8 |
    // its shader's fault << 16 (0 fine, 1 compile, 2 pipeline: 90s ANIMATED turned itself off).
    uint32_t cartoon{}, toonBits{};
    uint64_t gradedPairs{};
    uint64_t sequence{}; // odd while written (publish)
};
static_assert(sizeof(Data) == 80);
// Version 0.0.6 DAREDEVIL SONAR (the approved preview's look, preview004/make.py): a near-black world,
// red and orange outlines (a Sobel of the brightness over 3x3 taps 2 pixels apart: fine texture
// draws little), brightest near the bottom centre (where he stands), and three pulse rings that
// grow out from there and fade (one every 2 s, 0.75 image widths across). The toon pipeline's
// sonar mode (native_eye_gpu.cpp toonShader sonar()) is the same steps in the same order; both
// eyes alike (the rings at the same place in each eye). 9 Loads a pixel. phase: seconds.
inline float sonarRings(float r, float phase) {
    float ring = 0;
    for (int k = 0; k < 3; ++k) {
        float t = phase * .5f + static_cast<float>(k) / 3.f;
        t -= std::floor(t);
        const float R = t * .75f, d = (r - R) / .008f;
        ring += (1 - t) * std::exp(-d * d);
    }
    return ring;
}
template <class Fetch>
std::array<float, 3> sonarPixel(Fetch&& fetch, int x, int y, int w, int h, float phase) {
    const auto lum = [](const std::array<float, 3>& k) { return .299f * k[0] + .587f * k[1] + .114f * k[2]; };
    float l[9];
    for (int j = 0; j < 3; ++j)
        for (int i = 0; i < 3; ++i)
            l[j * 3 + i] = lum(fetch(x + (i - 1) * 2, y + (j - 1) * 2));
    const float gx = (l[2] + 2 * l[5] + l[8]) - (l[0] + 2 * l[3] + l[6]);
    const float gy = (l[6] + 2 * l[7] + l[8]) - (l[0] + 2 * l[1] + l[2]);
    const float e = std::pow(saturate(std::sqrt(gx * gx + gy * gy) * .8f), .8f);
    const float u = (static_cast<float>(x) + .5f) / static_cast<float>(std::max(w, 1));
    const float v = (static_cast<float>(y) + .5f) / static_cast<float>(std::max(h, 1));
    const float aspect = static_cast<float>(std::max(h, 1)) / static_cast<float>(std::max(w, 1));
    const float du = u - .5f, dv = (v - .92f) * 1.6f * aspect;
    const float r = std::sqrt(du * du + dv * dv);
    const float ring = sonarRings(r, fin(phase, 0));
    const float near = std::exp(-r / .55f);
    const float edge = e * (.35f + .65f * near) + e * ring * 1.5f;
    const float base = l[4] * .06f;
    return {saturate(base + edge + ring * .25f), saturate(base * .4f + edge * .38f + ring * .08f),
            saturate(base * .3f + edge * .08f)};
}

} // namespace spidy::eye_color
