#pragma once
#include "vertex.hpp"
#include <string_view>
#include <vector>

namespace spidy {
// Update 36: a chunky bitmap font of our own drawing (no outside font, no
// licence), after the 2004 Spider-Man 2 handheld menus: each glyph 5 by 7
// pixels made bold (every pixel also lights its right neighbour, so the
// stems are two wide), with a one-pixel dark outline about the whole run of
// text. The same characters as the stroke font (overlay_text.hpp: capitals,
// digits and its punctuation; lower case drawn in capitals, anything else a
// space). Drawn as geometry, one quad per run of lit pixels in a row (the
// outline first, merged across letters, then the fill), so it needs no
// texture and can't fail to load. The wrist hologram's "2000s Raimi" theme
// uses it; the calibration panel's live text too.
struct PixelTextStyle {
    float height = .04f;          // the 7 pixel rows, metres
    Vec3 top{1, 1, 1};            // fill colour of the top row (linear RGB)
    Vec3 bottom{1, 1, 1};         // ...of the bottom row; rows between blend
    float alpha = 1;
    Vec3 outline{0, 0, .002f};    // the dark outline
    float outlineAlpha = .95f;    // 0: none
    float xScale = 1;             // pixel width over pixel height
    float slant = 0;              // italic: across per unit up
};
// Pixel columns `text` takes (each glyph its width plus one between, a space 4).
int pixelTextColumns(std::string_view text);
// How wide `text` is at `style.height` and `style.xScale`, metres (the outline not counted).
float pixelTextWidth(std::string_view text, const PixelTextStyle& style);
// Appends `text` standing on its baseline from `origin` along unit `right`,
// its rows up along unit `up`.
void appendPixelText(std::vector<Vertex>& out, std::string_view text, Vec3 origin, Vec3 right, Vec3 up,
                     const PixelTextStyle& style);
} // namespace spidy
