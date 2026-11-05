#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
// HERO VR 0.0.78 cut21 PROMPT LAYER (CANDIDATE). The game's own button prompts, waypoint and markers as small VR cards.
// CONFIRMED from the cut19 UI DUMP (r19 report, ui-dump pictures): SM2 draws its cohtml HUD - the waypoint diamond and
// its metres, the threat triangle, the reticle, the gadget wheel, the prompts - into a render target of its own, the
// frame's size (1280x720 RGBA8 there, #1: 3 RENDER_TARGET -> readable a present, last state 0xac3), clear but for those
// pieces (alpha 0.35% / 1.40%), its colour premultiplied (no pixel's colour above its alpha). The composed frame (#2,
// alpha 100%) is not it. So: find that target once (native_eye_gpu: the UI DUMP's hunt - no pictures, no wait - then
// each of its best candidates' alpha read back like the cards' own, no wait, the first that isLayer kept), copy it each present on the game's queue, read its alpha back every maskEveryMs into a
// coarse grid, and draw each lit piece (regions) as its own small card at its place in view (cardRect), keyed by the
// layer's own alpha. Never the whole layer: a layer lit over maxCoverPercent (a menu, the map, a fade) gives no cards, a
// piece wider or taller than maxRegionFrac is left out, every card is capped (maxCardTan) and shrunk (shrink).
// Pure and allocation-free; nothing here reads or writes the game.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace spidy::prompt_layer {
// live.txt prompt_layer (SpidyPromptLayer 5646): 0 on (the default), 1 off.
enum Mode : uint32_t { layerOn = 0, layerOff = 1 };
inline constexpr uint32_t cell = 16;                  // layer pixels per grid cell, each way
inline constexpr uint32_t maxCols = 160, maxRows = 90; // up to 2560x1440
inline constexpr uint32_t maxCards = 8;
inline constexpr uint8_t alphaOn = 12;                 // a cell is lit when a sampled alpha is over this (of 255)
inline constexpr uint64_t maskEveryMs = 100;           // the alpha read back this often (no wait: a later present)
inline constexpr uint64_t maskMaxAgeMs = 400;          // an older grid draws nothing
inline constexpr double maxCoverPercent = 20;          // lit cells over this part of the layer: no cards at all
inline constexpr double maxLocatePercent = 25;         // isLayer: alpha over this is not a HUD layer
inline constexpr float maxRegionFrac = .4f;            // a piece wider or taller than this part of the layer: left out
inline constexpr uint32_t padPixels = 4;               // around each piece's cells
inline constexpr uint32_t bridgeCells = 2;             // lit cells this close (Chebyshev) are one piece
// Placement, in tangent units of the eye (x right, y up). The layer as a whole would span +-fieldTan across (the
// game's own view, about 72 degrees), its height by its aspect; each card is the piece at its own place, shrunk about
// its centre, then capped.
inline constexpr float fieldTan = .72f;
inline constexpr float shrink = .6f;
inline constexpr float maxCardTan[2] = {.30f, .16f};  // a card's width, height at most (about 17 x 9 degrees)
inline constexpr uint64_t locateFirstMs = 3000, locateMaxMs = 30000; // a failed find is tried again after this, doubling
inline constexpr uint32_t maxLocates = 40;             // a session
inline constexpr uint64_t quietLostMs = 3000;          // the found target not drawn to this long: found again
inline constexpr uint32_t maxProbes = 4;               // a find checks at most this many of the hunt's best targets
inline constexpr uint64_t probeMs = 1500;              // a target whose alpha is not read back in this long: the next

// DXGI formats (dxgiformat.h): 8-bit with alpha.
inline bool eightBitAlpha(uint32_t f) {
    return (f >= 27 && f <= 29) || f == 87 || f == 90 || f == 91;
}
// A hunt's target worth checking: 8-bit with alpha, the back buffer's own size, drawn and then read this hunt.
inline bool candidate(uint32_t format, uint32_t w, uint32_t h, uint32_t backW, uint32_t backH, uint64_t toRead) {
    return eightBitAlpha(format) && w && h && w == backW && h == backH && toRead;
}
// The find: an 8-bit target with alpha of the back buffer's own size, lit somewhere but mostly clear.
inline bool isLayer(uint32_t format, uint32_t w, uint32_t h, uint32_t backW, uint32_t backH, uint64_t alphaPixels,
                    uint64_t pixels) {
    if (!eightBitAlpha(format) || !w || !h || w != backW || h != backH || !pixels || !alphaPixels)
        return false;
    return static_cast<double>(alphaPixels) * 100.0 <= maxLocatePercent * static_cast<double>(pixels);
}

struct Grid {
    uint32_t cols{}, rows{}, width{}, height{}, lit{};
    uint64_t samples{}, alphaSamples{}; // the pixels read, those of them lit: the find's check (isLayer)
    std::array<uint8_t, maxCols * maxRows> on{};
    bool begin(uint32_t w, uint32_t h) {
        width = height = cols = rows = lit = 0;
        samples = alphaSamples = 0;
        on.fill(0);
        if (!w || !h)
            return false;
        const uint32_t c = (w + cell - 1) / cell, r = (h + cell - 1) / cell;
        if (c > maxCols || r > maxRows)
            return false;
        width = w;
        height = h;
        cols = c;
        rows = r;
        return true;
    }
    // One row of 4-byte pixels (RGBA8 or BGRA8: alpha the 4th byte), every second pixel.
    void row(const uint8_t* pixels, uint32_t y) {
        if (!pixels || y >= height)
            return;
        uint8_t* line = on.data() + static_cast<size_t>(y / cell) * cols;
        for (uint32_t x = 0; x < width; x += 2) {
            ++samples;
            if (pixels[static_cast<size_t>(x) * 4 + 3] > alphaOn) {
                ++alphaSamples;
                uint8_t& c = line[x / cell];
                if (!c) {
                    c = 1;
                    ++lit;
                }
            }
        }
    }
    bool at(uint32_t x, uint32_t y) const {
        return x < cols && y < rows && on[static_cast<size_t>(y) * cols + x];
    }
};

// One lit piece of the layer, in its pixels [x0, x1) x [y0, y1).
struct Region {
    uint32_t x0{}, y0{}, x1{}, y1{}, cells{};
};
// The pieces, largest first, at most maxCards; none when the layer is lit over maxCoverPercent.
inline uint32_t regions(const Grid& g, Region out[maxCards]) {
    if (!g.cols || !g.rows || !g.lit)
        return 0;
    if (static_cast<double>(g.lit) * 100.0 > maxCoverPercent * static_cast<double>(g.cols) * g.rows)
        return 0;
    std::array<uint8_t, maxCols * maxRows> seen{};
    std::array<uint16_t, maxCols * maxRows> stack{};
    std::array<Region, maxCards + 1> best{};
    uint32_t kept = 0;
    for (uint32_t sy = 0; sy < g.rows; ++sy)
        for (uint32_t sx = 0; sx < g.cols; ++sx) {
            const size_t start = static_cast<size_t>(sy) * g.cols + sx;
            if (!g.on[start] || seen[start])
                continue;
            Region r{sx, sy, sx, sy, 0};
            size_t top = 0;
            stack[top++] = static_cast<uint16_t>(start);
            seen[start] = 1;
            while (top) {
                const uint32_t i = stack[--top];
                const uint32_t x = i % g.cols, y = i / g.cols;
                ++r.cells;
                r.x0 = std::min(r.x0, x);
                r.y0 = std::min(r.y0, y);
                r.x1 = std::max(r.x1, x);
                r.y1 = std::max(r.y1, y);
                const uint32_t ny0 = y > bridgeCells ? y - bridgeCells : 0, nx0 = x > bridgeCells ? x - bridgeCells : 0;
                for (uint32_t ny = ny0; ny <= y + bridgeCells && ny < g.rows; ++ny)
                    for (uint32_t nx = nx0; nx <= x + bridgeCells && nx < g.cols; ++nx) {
                        const size_t j = static_cast<size_t>(ny) * g.cols + nx;
                        if (g.on[j] && !seen[j]) {
                            seen[j] = 1;
                            stack[top++] = static_cast<uint16_t>(j);
                        }
                    }
            }
            // Cells to pixels, padded, inside the layer.
            Region p;
            p.cells = r.cells;
            p.x0 = r.x0 * cell > padPixels ? r.x0 * cell - padPixels : 0;
            p.y0 = r.y0 * cell > padPixels ? r.y0 * cell - padPixels : 0;
            p.x1 = std::min(g.width, (r.x1 + 1) * cell + padPixels);
            p.y1 = std::min(g.height, (r.y1 + 1) * cell + padPixels);
            if (p.x1 <= p.x0 || p.y1 <= p.y0 || static_cast<float>(p.x1 - p.x0) > maxRegionFrac * g.width ||
                static_cast<float>(p.y1 - p.y0) > maxRegionFrac * g.height)
                continue; // a panel, not a prompt or marker
            uint32_t at = kept < maxCards ? kept++ : maxCards; // insertion by size into best[0..kept)
            best[at] = p;
            while (at > 0 && best[at].cells > best[at - 1].cells) {
                std::swap(best[at], best[at - 1]);
                --at;
            }
        }
    for (uint32_t i = 0; i < kept; ++i)
        out[i] = best[i];
    return kept;
}

// A piece's card in tangent units {x0, y0 (lower), x1, y1 (upper)}; false when the layer or piece is unusable.
inline bool cardTangents(const Region& r, uint32_t layerW, uint32_t layerH, float out[4]) {
    out[0] = out[1] = out[2] = out[3] = 0;
    if (!layerW || !layerH || r.x1 <= r.x0 || r.y1 <= r.y0 || r.x1 > layerW || r.y1 > layerH)
        return false;
    const float w = static_cast<float>(layerW), h = static_cast<float>(layerH);
    const float halfW = fieldTan, halfH = fieldTan * h / w;
    const float cu = (static_cast<float>(r.x0 + r.x1) * .5f) / w, cv = (static_cast<float>(r.y0 + r.y1) * .5f) / h;
    const float cx = (cu * 2 - 1) * halfW, cy = (1 - cv * 2) * halfH;
    float tw = static_cast<float>(r.x1 - r.x0) / w * 2 * halfW * shrink;
    float th = static_cast<float>(r.y1 - r.y0) / h * 2 * halfH * shrink;
    const float fit = std::min({1.f, maxCardTan[0] / tw, maxCardTan[1] / th}); // capped, its shape kept
    tw *= fit;
    th *= fit;
    out[0] = cx - tw * .5f;
    out[1] = cy - th * .5f;
    out[2] = cx + tw * .5f;
    out[3] = cy + th * .5f;
    return true;
}
// The card's rectangle {x0, y0, x1, y1} in one eye's pixels (y down) from that eye's tangents (left and down
// negative), and the piece's crop {u0, v0, u1, v1} of the layer; false when under 8 pixels or off the view.
inline bool cardRect(const Region& r, uint32_t layerW, uint32_t layerH, float tanLeft, float tanRight, float tanUp,
                     float tanDown, unsigned width, unsigned height, float rect[4], float crop[4]) {
    rect[0] = rect[1] = rect[2] = rect[3] = 0;
    crop[0] = crop[1] = 0;
    crop[2] = crop[3] = 1;
    float t[4];
    if (!width || !height || !std::isfinite(tanLeft) || !std::isfinite(tanRight) || !std::isfinite(tanUp) ||
        !std::isfinite(tanDown) || tanRight - tanLeft < .1f || tanUp - tanDown < .1f ||
        !cardTangents(r, layerW, layerH, t))
        return false;
    const float w = static_cast<float>(width), h = static_cast<float>(height);
    const float x0 = (t[0] - tanLeft) / (tanRight - tanLeft) * w, x1 = (t[2] - tanLeft) / (tanRight - tanLeft) * w;
    const float y0 = (tanUp - t[3]) / (tanUp - tanDown) * h, y1 = (tanUp - t[1]) / (tanUp - tanDown) * h;
    if (x1 <= 0 || y1 <= 0 || x0 >= w || y0 >= h)
        return false;
    // A card cut by the view's edge keeps its picture's matching part.
    const float cx0 = std::clamp(x0, 0.f, w), cy0 = std::clamp(y0, 0.f, h);
    const float cx1 = std::clamp(x1, 0.f, w), cy1 = std::clamp(y1, 0.f, h);
    if (cx1 - cx0 < 8.f || cy1 - cy0 < 8.f)
        return false;
    const float u0 = static_cast<float>(r.x0) / layerW, u1 = static_cast<float>(r.x1) / layerW;
    const float v0 = static_cast<float>(r.y0) / layerH, v1 = static_cast<float>(r.y1) / layerH;
    crop[0] = u0 + (cx0 - x0) / (x1 - x0) * (u1 - u0);
    crop[2] = u0 + (cx1 - x0) / (x1 - x0) * (u1 - u0);
    crop[1] = v0 + (cy0 - y0) / (y1 - y0) * (v1 - v0);
    crop[3] = v0 + (cy1 - y0) / (y1 - y0) * (v1 - v0);
    rect[0] = cx0;
    rect[1] = cy0;
    rect[2] = cx1;
    rect[3] = cy1;
    return crop[2] > crop[0] && crop[3] > crop[1];
}
// The next find's delay after `failed` finds in a row: locateFirstMs, doubling, at most locateMaxMs.
inline uint64_t locateDelay(uint32_t failed) {
    uint64_t d = locateFirstMs;
    for (uint32_t i = 0; i < failed && d < locateMaxMs; ++i)
        d *= 2;
    return std::min(d, locateMaxMs);
}
} // namespace spidy::prompt_layer
