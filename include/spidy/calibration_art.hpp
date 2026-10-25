#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace spidy::calibration_art {
// Update 36: the calibration panel's baked art (and the health ring's
// emblem), one RGBA8 atlas, display-encoded (sRGB) with straight alpha,
// built by tools/calibration_art/make_atlas.py into
// assets/calibration_art.qoi and compiled into Spidy (CMake embeds the file).
// The figure is the crouching Spider-Man the player supplied (hero.png); the
// rest is Spidy's own drawing; the titles were set in Orbitron (SIL OFL 1.1),
// which is not shipped.
inline constexpr int width = 2048, height = 1280;
struct Rect {
    int x, y, w, h;
};
// Kept in step with make_atlas.py's LAYOUT.
// figure: the hero, aspect kept, centred with a margin; figureGlow: his gold
// halo at half the resolution, framed the same (drawn over the same quad).
inline constexpr Rect city{0, 0, 1024, 512}, figure{1024, 0, 1024, 1024}, figureGlow{0, 512, 512, 512},
    emblem{512, 512, 512, 512}, titleCalibration{0, 1024, 1024, 128}, titleCalibrated{0, 1152, 1024, 128}, white{1024, 1024, 8, 8};
// A point of `r`, s across and t down (0..1), as texture coordinates of the
// atlas; kept half a texel inside so filtering stays within `r`.
struct Uv {
    float u, v;
};
Uv uv(const Rect& r, float s, float t);
struct Image {
    int width{}, height{};
    std::vector<std::uint8_t> rgba; // rows from the top
};
// Decodes a QOI file (qoiformat.org). False, `out` empty, on anything malformed
// or larger than `maxPixels`.
bool decodeQoi(std::span<const std::uint8_t> file, Image& out, std::size_t maxPixels = std::size_t{1} << 24);
// The embedded atlas file (empty in a build without it).
std::span<const std::uint8_t> embedded();
// The atlas decoded and checked against the layout's size; false if not.
bool load(Image& out);
// The image's mip chain from `base` (included) to `levels` levels or 1 x 1,
// each a 2 x 2 average in linear light (alpha-weighted, so empty texels add
// no colour), rounded back to sRGB.
std::vector<Image> mips(const Image& base, int levels);
} // namespace spidy::calibration_art
