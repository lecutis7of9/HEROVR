#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
//
// The web strand's look, worked out for each pixel (Update 11), after the
// 1994 animated series: a thick cream rope, wrapped every so often by raised
// coils that sit close together by the hand and spread out along the line,
// cel-shaded in three flat tones with a dark ink line on the silhouette and
// around each coil.
//
// Update 10 lit the strand at five vertices across it, so a strand a few
// pixels wide could only show the two shades its columns interpolated to
// ("two shades of solid grey"). Now the strand is one strip whose vertices
// carry where they are across it and how many coil groups from the wrist
// (Vertex::shape), and the overlay's pixel shader (d3d12_renderer.cpp) draws
// the rope, its coils and its ink from that: no textures, a few dozen ALU
// instructions a strand pixel. Coils and ink shorter than about two pixels
// fade out (from the shape's change across one pixel, fwidth) instead of
// crawling as moire, and every hard cel edge is ramped over one pixel.
//
// This is the reference the shader follows, and the previews draw with it.
#include "vertex.hpp"
#include <algorithm>
#include <cmath>

namespace spidy::web_shading {
// The rope's half width as a share of the strip's (the coils bulge past it).
inline constexpr float rope = .7f, bulge = .26f;
// Coils come in groups of `wraps` rings, each `ringLength` metres along the
// rope. Groups are `firstSpacing` metres apart at the wrist, growing
// `spacingGrowth` metres a metre along the line, so the groups along the
// strand number log(1 + growth * metres / first) / growth.
inline constexpr float firstSpacing = .085f, spacingGrowth = .16f;
inline constexpr float ringLength = .024f, wraps = 3, groupShare = .85f;
// A ring's ink line slants across the rope and bows round it (metres), so
// the rings read as cord wound around the strand, not as stripes.
inline constexpr float ringSlant = .008f, ringBow = .012f;
inline constexpr float inkPixels = 1.6f; // the ink line's width
// Two finer lines along the rope between the coils, where its strands lie
// side by side, at these shares of its half width (drawn at `strandInk` of
// the ink, where the rope is at least 20 pixels across).
inline constexpr float strandLines[2] = {-.3f, .42f};
inline constexpr float strandInk = .55f;
// Tones against the lit one, per channel: the reference's lit #E8E2D0, its
// shadow side #B8B4A8 (both sRGB, compared in linear light), the midtone
// between, and the ink.
inline constexpr float midTone[3] = {.79f, .80f, .81f};
inline constexpr float shadowTone[3] = {.594f, .598f, .621f};
inline constexpr float inkTone = .05f;
// The lit tone itself, in linear light (#E8E2D0): WebLook::core.
inline constexpr float lit[3] = {.807f, .763f, .631f};

inline float coilsAt(float metres) {
    return std::log1p(spacingGrowth * std::max(metres, 0.f) / firstSpacing) / spacingGrowth;
}
// A strand vertex's shape: x across the strip (-1 to 1 its coil-tip edges,
// the rope inside +-rope; beyond them a pixel of soft margin), the coils
// counted from the wrist, and the sky light's direction against the strand's
// side and against the way to the eye. viewLight is stored +2 so a strand's
// w is never 0 (0: not a strand).
inline Shape strandShape(float x, float coils, float sideLight, float viewLight) {
    return {x, coils, sideLight, viewLight + 2};
}
inline bool isStrand(const Shape& s) {
    return s.w > .5f && s.w < 3.5f;
}
inline float saturate(float v) {
    return std::clamp(v, 0.f, 1.f);
}
inline float fract(float v) {
    return v - std::floor(v);
}
inline float smooth(float from, float to, float v) {
    return saturate((v - from) / (to - from));
}
// One pixel of strand: its interpolated colour, opacity and shape, and how
// much x and the coil count change across the pixel (fwidth on the GPU).
inline void shadeStrand(Vec3& color, float& alpha, const Shape& s, float xPerPixel, float coilsPerPixel) {
    const float x = s.x, ax = std::abs(x);
    const float fx = std::max(xPerPixel, 1e-4f), fc = std::max(coilsPerPixel, 1e-5f);
    // A group's spacing here, in metres, grows as exp(growth * groups).
    const float spacing = firstSpacing * std::exp(spacingGrowth * s.y);
    const float ring = std::min(ringLength / spacing, groupShare / wraps);
    // Rings fade out where one would be under about two pixels long.
    const float seen = saturate(ring / fc / 1.5f - 1);
    const float across = std::min(ax / rope, 1.f);
    const float round = std::sqrt(std::max(1 - across * across, 0.f));
    const float f = fract(s.y + (ringSlant * x + ringBow * round) / spacing);
    const bool grouped = f < ring * wraps;
    const float local = fract(f / ring);
    const float bump = grouped ? std::sqrt(std::max(std::sin(3.1415927f * local), 0.f)) : 0;
    const float half = rope + bulge * seen * bump;
    const float cover = saturate((half - ax) / fx + .5f);
    // Ink: along the silhouette, and between the rings and at each group's
    // ends, about 1.6 pixels wide, fading where the strand is too thin for it.
    const float inkSeen = saturate(rope / fx / 2.5f - .6f);
    const float rim = saturate((ax - (half - inkPixels * fx)) / fx + .5f);
    const float border = grouped ? std::min(local, 1 - local) * ring : std::min(f - ring * wraps, 1 - f);
    const float line = seen * saturate((inkPixels * .5f * fc - border) / fc + .5f) * (ax < half ? 1.f : 0.f);
    const float strandSeen = grouped ? 0 : saturate(rope / fx / 10 - 1);
    const float lengthwise = strandInk * strandSeen *
                             std::max(saturate(1.3f - std::abs(x - strandLines[0] * rope) / fx),
                                      saturate(1.3f - std::abs(x - strandLines[1] * rope) / fx));
    const float ink = std::max(inkSeen * std::max(rim, line), lengthwise);
    // Flat cel light: lit toward the sky's side of the rope, a midtone band
    // and the shadow side; each ring lit on its crown and shaded on its far
    // half. Steps are ramped over about a pixel and a half.
    const float nx = std::clamp(x / half, -1.f, 1.f);
    const float sideLight = s.z, viewLight = s.w - 2;
    const float toward = (sideLight < 0 ? -1.f : 1.f) * std::max(std::abs(sideLight), .7f);
    const float light = .62f + .5f * nx * toward + .1f * viewLight +
                        (grouped ? seen * (.12f * bump - .45f * smooth(.55f, .7f, local)) : 0);
    const float step = std::max(fx * 1.5f, 1e-3f);
    const float toMid = saturate((light - .3f) / step + .5f), toLit = saturate((light - .6f) / step + .5f);
    Vec3 tone{shadowTone[0] + (midTone[0] - shadowTone[0]) * toMid + (1 - midTone[0]) * toLit,
              shadowTone[1] + (midTone[1] - shadowTone[1]) * toMid + (1 - midTone[1]) * toLit,
              shadowTone[2] + (midTone[2] - shadowTone[2]) * toMid + (1 - midTone[2]) * toLit};
    tone = tone + (Vec3{inkTone, inkTone, inkTone} - tone) * ink;
    color = {color.x * tone.x, color.y * tone.y, color.z * tone.z};
    alpha = alpha * cover;
}
// Update 12: the rope is a real tube (web_visual.cpp's appendWeb): a ring
// of 8 to 12 vertices round it at each step along it, placed in the world,
// so each eye sees it from its own side and it stands at its own depth in
// the stereo image, and its coil rings are raised bulges of the mesh itself.
// A tube vertex carries its normal (world) and how near a ring's edge it
// is; its pixel is lit in the three cel tones by the normal against the sky
// light, the edges between rings inked. The silhouette's ink is a dark band
// set behind the tube, a little wider than it (inkShape), so its edge shows
// round the tube's whole outline, coils included.
//
// The sky light (WebLook::light's default, normalised) and where its tones
// step: lit above litEdge, the midtone down to shadowEdge, shadow below.
inline constexpr float skyLight[3] = {.239988f, .9499525f, .19999f};
inline constexpr float litEdge = .35f, shadowEdge = -.35f;
// The bulge of a coil ring on the tube, as a share of its radius.
inline constexpr float tubeBulge = .3f; // Update 13: 30% (it was bulge / rope, 37%)
// groove: 1 on the edge between two rings, falling to 0 by the next step
// along the tube (0 elsewhere); below 1 where the rings are too small to see.
inline Shape tubeShape(Vec3 normal, float groove) {
    return {normal.x, normal.y, normal.z, 4 + std::clamp(groove, 0.f, 1.f)};
}
inline bool isTube(const Shape& s) {
    return s.w > 3.5f && s.w < 6.5f; // Update 18: 8 and up are glossShape's
}
// The ink band: x across it (+-1 its edges, a pixel's soft margin beyond),
// seen how much of the ink shows (it fades out on a rope too thin for it).
inline Shape inkShape(float x, float seen) {
    return {x, std::clamp(seen, 0.f, 1.f), 0, -1};
}
inline bool isInk(const Shape& s) {
    return s.w < -.5f;
}
inline float tubeLight(const Shape& s) {
    const float n = std::sqrt(s.x * s.x + s.y * s.y + s.z * s.z);
    return n > 1e-6f ? (s.x * skyLight[0] + s.y * skyLight[1] + s.z * skyLight[2]) / n : 0.f;
}
// One pixel of tube: lightPerPixel and groovePerPixel are how much
// tubeLight(s) and s.w change across the pixel (fwidth on the GPU).
inline void shadeTube(Vec3& color, const Shape& s, float lightPerPixel, float groovePerPixel) {
    const float light = tubeLight(s);
    const float step = std::max(lightPerPixel * 1.5f, 1e-3f);
    const float toMid = saturate((light - shadowEdge) / step + .5f), toLit = saturate((light - litEdge) / step + .5f);
    Vec3 tone{shadowTone[0] + (midTone[0] - shadowTone[0]) * toMid + (1 - midTone[0]) * toLit,
              shadowTone[1] + (midTone[1] - shadowTone[1]) * toMid + (1 - midTone[1]) * toLit,
              shadowTone[2] + (midTone[2] - shadowTone[2]) * toMid + (1 - midTone[2]) * toLit};
    const float fg = std::max(groovePerPixel, 1e-4f);
    const float line = saturate((s.w - 5) / fg + inkPixels * .5f + .5f);
    tone = tone + (Vec3{inkTone, inkTone, inkTone} - tone) * line;
    color = {color.x * tone.x, color.y * tone.y, color.z * tone.z};
}
inline void shadeInk(Vec3& color, float& alpha, const Shape& s, float xPerPixel) {
    const float cover = saturate((1 - std::abs(s.x)) / std::max(xPerPixel, 1e-4f) + .5f);
    color = color * inkTone;
    alpha = alpha * cover * s.y;
}
// Update 18: the other web styles (web_visual.hpp web_style): MODERN, MESSY,
// SYMBIOTE and ENERGY are tubes too, but smooth and lit, not cel-shaded or
// inked. Their vertices carry the light per vertex as it falls on the
// tube's normal there (interpolating a dot product is interpolating the
// normal): x its side to the sky light, y to the half-way vector between
// the light and the eye (the gloss), z a style's own (MODERN and MESSY: the
// twist's phase along and round the rope, a turn a unit; SYMBIOTE and ENERGY:
// how squarely it faces the eye), and w = glossBase + the style's kind,
// the same at every vertex of a triangle. Still no textures: a few ALU
// instructions a pixel, the same here and in the overlay's shader.
inline constexpr float glossBase = 8;
enum GlossKind : int { glossModern, glossMessy, glossSymbiote, glossEnergy };
// Update 24: a ply - one of the two to four twisted strands a near rope is
// laid from (web_visual.cpp) - is its style's kind + plyOffset: lit the same,
// its own fine fibres on it but not the seams between plies.
inline constexpr float plyOffset = 4;
inline Shape glossShape(int kind, float side, float half, float extra, bool ply = false) {
    return {side, half, extra, glossBase + static_cast<float>(std::clamp(kind, 0, 3)) + (ply ? plyOffset : 0.f)};
}
inline bool isGloss(const Shape& s) {
    return s.w > 7.5f;
}
// Update 24: the rope's fibres, from the twist's phase p (three plies round,
// a turn a unit): the seam between two plies (1 on it) and the fine fibres
// along each ply (1 on a fibre's lit crest), each faded out before it would
// be under about four pixels a line (no shimmer or moire far off).
struct Fibres {
    float seam, fibre, seamSeen, fibreSeen;
};
inline constexpr float fibreLines = 7; // fine fibres per ply
inline Fibres fibres(float p, float perPixel) {
    const float pp = std::max(perPixel, 1e-4f);
    const float c = std::cos(6.2831853f * p), f = std::cos(6.2831853f * fibreLines * p + 1.3f * c);
    return {std::pow(.5f + .5f * c, 6.f), .5f + .5f * f, saturate(.25f / pp - 1),
            saturate(.25f / (fibreLines * pp) - 1)};
}
// One pixel of a styled tube: zPerPixel and xPerPixel how much s.z and s.x
// change across it (fwidth on the GPU). MODERN and MESSY carry the twist's
// phase in z, SYMBIOTE and ENERGY in x (z is how squarely they face the eye).
inline void shadeGloss(Vec3& color, const Shape& s, float zPerPixel, float xPerPixel = 1) {
    const bool ply = s.w > glossBase + 3.5f;
    const float w = ply ? s.w - plyOffset : s.w;
    const float d = saturate(.5f + .5f * s.x), h = std::max(saturate(s.y), 1e-4f);
    const Fibres z = fibres(s.z, zPerPixel), x = fibres(s.x, xPerPixel);
    const float seamZ = ply ? 0.f : z.seam * z.seamSeen, seamX = ply ? 0.f : x.seam * x.seamSeen;
    if (w < glossBase + .5f) { // MODERN: bright and clean, tight fine fibres, a soft gloss
        const float tone = (.58f + .42f * d) * (1 - .2f * seamZ) * (1 - .2f * z.fibreSeen * (1 - z.fibre));
        const float spec = std::pow(h, 48.f) * .65f + std::pow(h, 14.f) * .12f * z.fibreSeen * z.fibre;
        color = color * tone + Vec3{spec, spec, spec};
    } else if (w < glossBase + 1.5f) {
        // MESSY: silvery silk fibres - darker in the seams, each fibre with
        // its own glint along the twist (a hair's highlight).
        const float tone = (.5f + .42f * d) * (1 - .35f * seamZ) * (1 - .4f * z.fibreSeen * (1 - z.fibre));
        const float glint = 1 + z.fibreSeen * (1.3f * z.fibre - .65f);
        const float spec = (std::pow(h, 40.f) * .6f + std::pow(h, 8.f) * .08f) * glint +
                           std::pow(h, 12.f) * .2f * z.fibreSeen * z.fibre;
        color = color * tone + Vec3{spec, spec, spec};
    } else if (w < glossBase + 2.5f) { // SYMBIOTE: near black, slick, its colour a sheen at the rim and in the gloss
        const float f = 1 - saturate(s.z), rim = f * f * f * (1 - .5f * seamX);
        const float spec = std::pow(h, 60.f) * .8f * (1 + x.fibreSeen * (1.2f * x.fibre - .6f));
        const float most = std::max({color.x, color.y, color.z});
        color = color * (.03f + .42f * rim + .55f * spec) + Vec3{1, 1, 1} * (.3f * most * spec);
    } else { // ENERGY: its own light, white-hot where it faces the eye, plasma fibres along the twist
        const float f = saturate(s.z), hot = f * f * f * f * .92f;
        const float line = x.fibre * x.fibre * x.fibre * x.fibre;
        const float glow = std::min(1.f, hot + .55f * x.fibreSeen * line) * (1 - .4f * seamX);
        color = color + (Vec3{1, 1, 1} - color) * glow;
    }
}
} // namespace spidy::web_shading
