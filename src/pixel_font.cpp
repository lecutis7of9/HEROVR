// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
// Update 36: the pixel font (pixel_font.hpp). The glyphs are our own, drawn
// for this mod (tools source: none needed, the bits are below).
#include "spidy/pixel_font.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>

namespace spidy {
namespace {
constexpr int rows = 7, space = 4;
// Each row's bits, the leftmost pixel the highest of `width` bits.
struct Glyph {
    char c;
    unsigned char width;
    unsigned char bits[rows];
};
constexpr Glyph glyphs[] = {
    {'A', 6, {0x1e, 0x33, 0x33, 0x3f, 0x33, 0x33, 0x33}},
    {'B', 6, {0x3e, 0x33, 0x33, 0x3e, 0x33, 0x33, 0x3e}},
    {'C', 6, {0x1e, 0x33, 0x30, 0x30, 0x30, 0x33, 0x1e}},
    {'D', 6, {0x3c, 0x36, 0x33, 0x33, 0x33, 0x36, 0x3c}},
    {'E', 6, {0x3f, 0x30, 0x30, 0x3e, 0x30, 0x30, 0x3f}},
    {'F', 6, {0x3f, 0x30, 0x30, 0x3e, 0x30, 0x30, 0x30}},
    {'G', 6, {0x1e, 0x33, 0x30, 0x3f, 0x33, 0x33, 0x1f}},
    {'H', 6, {0x33, 0x33, 0x33, 0x3f, 0x33, 0x33, 0x33}},
    {'I', 4, {0x0f, 0x06, 0x06, 0x06, 0x06, 0x06, 0x0f}},
    {'J', 6, {0x0f, 0x06, 0x06, 0x06, 0x36, 0x36, 0x1c}},
    {'K', 6, {0x33, 0x36, 0x3c, 0x38, 0x3c, 0x36, 0x33}},
    {'L', 6, {0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x3f}},
    {'M', 6, {0x33, 0x3f, 0x3f, 0x3f, 0x33, 0x33, 0x33}},
    {'N', 6, {0x33, 0x3b, 0x3f, 0x37, 0x33, 0x33, 0x33}},
    {'O', 6, {0x1e, 0x33, 0x33, 0x33, 0x33, 0x33, 0x1e}},
    {'P', 6, {0x3e, 0x33, 0x33, 0x3e, 0x30, 0x30, 0x30}},
    {'Q', 6, {0x1e, 0x33, 0x33, 0x33, 0x3f, 0x36, 0x1f}},
    {'R', 6, {0x3e, 0x33, 0x33, 0x3e, 0x3c, 0x36, 0x33}},
    {'S', 6, {0x1f, 0x30, 0x30, 0x1e, 0x03, 0x03, 0x3e}},
    {'T', 6, {0x3f, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c}},
    {'U', 6, {0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x1e}},
    {'V', 6, {0x33, 0x33, 0x33, 0x33, 0x33, 0x1e, 0x0c}},
    {'W', 6, {0x33, 0x33, 0x33, 0x3f, 0x3f, 0x3f, 0x33}},
    {'X', 6, {0x33, 0x33, 0x1e, 0x0c, 0x1e, 0x33, 0x33}},
    {'Y', 6, {0x33, 0x33, 0x1e, 0x0c, 0x0c, 0x0c, 0x0c}},
    {'Z', 6, {0x3f, 0x03, 0x06, 0x0c, 0x18, 0x30, 0x3f}},
    {'0', 6, {0x1e, 0x33, 0x37, 0x3f, 0x3b, 0x33, 0x1e}},
    {'1', 4, {0x06, 0x0e, 0x06, 0x06, 0x06, 0x06, 0x0f}},
    {'2', 6, {0x1e, 0x33, 0x03, 0x06, 0x0c, 0x18, 0x3f}},
    {'3', 6, {0x3e, 0x03, 0x03, 0x1e, 0x03, 0x03, 0x3e}},
    {'4', 6, {0x06, 0x0e, 0x1e, 0x36, 0x3f, 0x06, 0x06}},
    {'5', 6, {0x3f, 0x30, 0x3e, 0x03, 0x03, 0x33, 0x1e}},
    {'6', 6, {0x1e, 0x30, 0x30, 0x3e, 0x33, 0x33, 0x1e}},
    {'7', 6, {0x3f, 0x03, 0x06, 0x0c, 0x18, 0x18, 0x18}},
    {'8', 6, {0x1e, 0x33, 0x33, 0x1e, 0x33, 0x33, 0x1e}},
    {'9', 6, {0x1e, 0x33, 0x33, 0x1f, 0x03, 0x03, 0x1e}},
    {'!', 2, {0x03, 0x03, 0x03, 0x03, 0x03, 0x00, 0x03}},
    {'%', 6, {0x3b, 0x3e, 0x06, 0x0c, 0x18, 0x1f, 0x37}},
    {'\'', 2, {0x03, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {'(', 3, {0x03, 0x06, 0x06, 0x06, 0x06, 0x06, 0x03}},
    {')', 3, {0x06, 0x03, 0x03, 0x03, 0x03, 0x03, 0x06}},
    {'+', 6, {0x00, 0x0c, 0x0c, 0x3f, 0x0c, 0x0c, 0x00}},
    {',', 3, {0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x06}},
    {'-', 5, {0x00, 0x00, 0x00, 0x1f, 0x00, 0x00, 0x00}},
    {'.', 2, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03}},
    {'/', 6, {0x03, 0x06, 0x06, 0x0c, 0x18, 0x18, 0x30}},
    {':', 2, {0x00, 0x00, 0x03, 0x00, 0x00, 0x03, 0x00}},
    {';', 3, {0x00, 0x00, 0x03, 0x00, 0x00, 0x03, 0x06}},
    {'<', 5, {0x03, 0x06, 0x0c, 0x18, 0x0c, 0x06, 0x03}},
    {'=', 6, {0x00, 0x00, 0x3f, 0x00, 0x3f, 0x00, 0x00}},
    {'>', 5, {0x18, 0x0c, 0x06, 0x03, 0x06, 0x0c, 0x18}},
    {'?', 6, {0x1e, 0x33, 0x03, 0x0e, 0x0c, 0x00, 0x0c}},
};
const Glyph* glyph(char c) {
    const char upper = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    for (const auto& g : glyphs)
        if (g.c == upper)
            return &g;
    return nullptr;
}
} // namespace

int pixelTextColumns(std::string_view text) {
    int n = 0;
    for (const char c : text) {
        const auto* g = glyph(c);
        n += g ? g->width + 1 : space;
    }
    const auto* last = text.empty() ? nullptr : glyph(text.back());
    return last ? n - 1 : n; // no gap after the last letter
}
float pixelTextWidth(std::string_view text, const PixelTextStyle& style) {
    return static_cast<float>(pixelTextColumns(text)) * style.height / rows * style.xScale;
}
void appendPixelText(std::vector<Vertex>& out, std::string_view text, Vec3 origin, Vec3 right, Vec3 up,
                     const PixelTextStyle& style) {
    const float py = style.height / rows, px = py * style.xScale;
    if (text.empty() || !finite(origin) || !finite(right) || !finite(up) || !(py > 0) || !(px > 0) ||
        !std::isfinite(style.slant) || !std::isfinite(style.alpha))
        return;
    // The lit pixels, a margin of one about them for the outline: row 0 the top.
    const int cols = pixelTextColumns(text) + 2, height = rows + 2;
    if (cols > 4096)
        return;
    std::vector<unsigned char> lit(static_cast<size_t>(cols * height));
    int x = 1;
    for (const char c : text) {
        const auto* g = glyph(c);
        if (!g) {
            x += space;
            continue;
        }
        for (int r = 0; r < rows; ++r)
            for (int i = 0; i < g->width; ++i)
                if (g->bits[r] >> (g->width - 1 - i) & 1)
                    lit[static_cast<size_t>((r + 1) * cols + x + i)] = 1;
        x += g->width + 1;
    }
    // Column c, row r (from the top of the margin) to a point.
    const auto at = [&](float c, float r) {
        const float v = (static_cast<float>(rows + 1) - r) * py; // up from the baseline
        return origin + right * ((c - 1) * px + style.slant * v) + up * v;
    };
    const auto run = [&](int r, int c0, int c1, Vec3 color, float alpha) {
        const Vertex a{at(static_cast<float>(c0), static_cast<float>(r + 1)), color, alpha},
            b{at(static_cast<float>(c1), static_cast<float>(r + 1)), color, alpha},
            cc{at(static_cast<float>(c1), static_cast<float>(r)), color, alpha},
            d{at(static_cast<float>(c0), static_cast<float>(r)), color, alpha};
        out.insert(out.end(), {a, b, cc, a, cc, d});
    };
    if (style.outlineAlpha > 0) {
        const float oa = std::min(style.outlineAlpha, 1.f) * std::clamp(style.alpha, 0.f, 1.f);
        for (int r = 0; r < height; ++r) {
            int start = -1;
            for (int c = 0; c <= cols; ++c) {
                bool on = false;
                if (c < cols)
                    for (int dr = -1; dr <= 1 && !on; ++dr)
                        for (int dc = -1; dc <= 1 && !on; ++dc) {
                            const int rr = r + dr, cc = c + dc;
                            on = rr >= 0 && rr < height && cc >= 0 && cc < cols &&
                                 lit[static_cast<size_t>(rr * cols + cc)];
                        }
                if (on && start < 0)
                    start = c;
                else if (!on && start >= 0) {
                    run(r, start, c, style.outline, oa);
                    start = -1;
                }
            }
        }
    }
    for (int r = 1; r <= rows; ++r) {
        const float t = static_cast<float>(r - 1) / (rows - 1);
        const Vec3 color = style.top + (style.bottom - style.top) * t;
        int start = -1;
        for (int c = 0; c <= cols; ++c) {
            const bool on = c < cols && lit[static_cast<size_t>(r * cols + c)];
            if (on && start < 0)
                start = c;
            else if (!on && start >= 0) {
                run(r, start, c, color, style.alpha);
                start = -1;
            }
        }
    }
}
} // namespace spidy
