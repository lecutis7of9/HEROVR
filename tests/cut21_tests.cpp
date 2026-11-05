// HERO VR 0.0.78 cut21 PROMPT LAYER tests (include/spidy/prompt_layer.hpp): the find, the alpha grid, its pieces and
// their small cards - never the whole layer. The grid below is the cut19 UI DUMP's VR-play layer (TARGET #1, 1280x720,
// 16-pixel cells, every second pixel's alpha over 12): the waypoint, the reticle, the threat marker, the gadget wheel and
// the piece in the lower right corner.
#include "spidy/prompt_layer.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

using namespace spidy::prompt_layer;

namespace {
int failures = 0;
void check(bool ok, const char* what) {
    std::printf("%s: %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok)
        ++failures;
}
const char* dump[] = {
    "................................................................................",
    "................................................................................",
    "................................................................................",
    "................................................................................",
    "................................................................................",
    "................................................................................",
    "................................................................................",
    "................................................................................",
    "................................................................................",
    "................................................................................",
    "................................................................................",
    "................................................................................",
    "................................................................................",
    "................................................................................",
    ".................................................#..............................",
    "................................................###.............................",
    "................................................###.............................",
    "................................................###.............................",
    "................................................................................",
    "................................................................................",
    "................................................................................",
    "................................................................................",
    "................................................................................",
    "................................................................................",
    ".......................................##.......................................",
    ".......................................###......................................",
    ".......................................###......................................",
    "................................................................................",
    "................................................................................",
    "................................................................................",
    "................................................................................",
    ".......................................#............................##..........",
    "......................................###.......................#######.........",
    "......................................###....................###########........",
    ".............................................................############.......",
    "..............................................................#############.....",
    "................................................................###########....#",
    "................................................................#######........#",
    "..............................................................................##",
    ".............................................................................###",
    "..............................................................................##",
    ".........................................................................###...#",
    "...........................................................................##...",
    "............................................................................###.",
    "..............................................................................##",
};
// A 1280x720 RGBA8 layer with each '#' cell's pixels lit (premultiplied white at alpha 200).
void fill(Grid& g, const char* const* cells, uint32_t rowsN) {
    g.begin(1280, 720);
    std::vector<uint8_t> row(1280 * 4);
    for (uint32_t y = 0; y < 720; ++y) {
        std::memset(row.data(), 0, row.size());
        const uint32_t cy = y / cell;
        if (cy < rowsN)
            for (uint32_t x = 0; x < 1280; ++x)
                if (cells[cy][x / cell] == '#')
                    row[x * 4] = row[x * 4 + 1] = row[x * 4 + 2] = row[x * 4 + 3] = 200;
        g.row(row.data(), y);
    }
}
} // namespace

int main() {
    // The find: the frame's size, 8-bit with alpha, lit but mostly clear (#1 0.35%: yes; #2 100%: no).
    check(isLayer(28, 1280, 720, 1280, 720, 3226, 921600), "TARGET #1 (RGBA8, 0.35% alpha) is the layer");
    check(!isLayer(28, 1280, 720, 1280, 720, 921600, 921600), "the composed frame (alpha 100%) is not");
    check(!isLayer(28, 1280, 720, 1280, 720, 0, 921600), "a clear target says nothing (found again later)");
    check(!isLayer(28, 1920, 1080, 1280, 720, 3000, 2073600), "another size (TARGET #6, 1920x1080) is not");
    check(!isLayer(10, 1280, 720, 1280, 720, 3000, 921600), "a float target is not");
    check(isLayer(87, 1280, 720, 1280, 720, 3000, 921600), "BGRA8 is");
    // The grid.
    Grid g;
    check(g.begin(1280, 720) && g.cols == 80 && g.rows == 45, "1280x720: 80 x 45 cells");
    check(!g.begin(4096, 2160), "a layer past 2560x1440 gives no grid");
    fill(g, dump, sizeof(dump) / sizeof(dump[0]));
    uint32_t litWant = 0;
    for (const char* r : dump)
        for (const char* c = r; *c; ++c)
            litWant += *c == '#';
    check(g.lit == litWant, "every lit cell of the dump read");
    Region r[maxCards];
    const uint32_t n = regions(g, r);
    std::printf("      %u pieces:", n);
    for (uint32_t i = 0; i < n; ++i)
        std::printf(" (%u,%u)-(%u,%u)", r[i].x0, r[i].y0, r[i].x1, r[i].y1);
    std::printf("\n");
    check(n == 5, "the dump's layer: 5 pieces (waypoint, reticle, marker, wheel, corner)");
    check(n && r[0].x0 >= 960 && r[0].x1 <= 1280 && r[0].y0 >= 480 && r[0].y1 <= 620, "the wheel the largest");
    bool waypoint = false;
    for (uint32_t i = 0; i < n; ++i)
        waypoint |= r[i].x0 <= 768 && r[i].x1 >= 800 && r[i].y0 <= 224 && r[i].y1 >= 272 && r[i].x1 - r[i].x0 <= 64;
    check(waypoint, "the waypoint diamond and its metres one small piece");
    // Cards from the real eye of a Quest 3 (tangents about -1.38 .. 0.96 across, 1.07 .. -1.21 up).
    const float L = -1.38f, R = .96f, U = 1.07f, D = -1.21f;
    const unsigned W = 2064, H = 2208;
    float areaAll = 0;
    bool capped = true, inView = true;
    for (uint32_t i = 0; i < n; ++i) {
        float t[4], rect[4], crop[4];
        check(cardTangents(r[i], 1280, 720, t), "a piece has a card");
        capped &= t[2] - t[0] <= maxCardTan[0] + 1e-5f && t[3] - t[1] <= maxCardTan[1] + 1e-5f;
        if (cardRect(r[i], 1280, 720, L, R, U, D, W, H, rect, crop)) {
            areaAll += (rect[2] - rect[0]) * (rect[3] - rect[1]);
            inView &= rect[0] >= 0 && rect[2] <= W && rect[1] >= 0 && rect[3] <= H;
            inView &= crop[0] >= 0 && crop[2] <= 1 && crop[1] >= 0 && crop[3] <= 1 && crop[2] > crop[0];
        }
    }
    check(capped, "every card within its cap");
    check(inView, "every card and its crop inside the view and the layer");
    std::printf("      cards cover %.2f%% of the eye\n", 100.0 * areaAll / (W * H));
    check(areaAll < .04f * W * H, "all the dump's cards together under 4% of the eye (small cards)");
    // The waypoint keeps its place: right of and above the view's centre, as on the layer.
    {
        Region wp{752, 208, 832, 304, 12};
        float t[4];
        cardTangents(wp, 1280, 720, t);
        check(t[0] > 0 && t[1] > 0 && (t[2] - t[0]) < .06f, "the waypoint small, up and right of centre");
    }
    // Never the whole layer: a layer lit all over (a menu, the map, a fade) gives no cards.
    {
        Grid full;
        full.begin(1280, 720);
        std::vector<uint8_t> row(1280 * 4, 255);
        for (uint32_t y = 0; y < 720; ++y)
            full.row(row.data(), y);
        check(full.lit == 80 * 45 && regions(full, r) == 0, "a full layer: no cards");
    }
    // A large panel among small pieces is left out, the small ones kept.
    {
        Grid p;
        p.begin(1280, 720);
        std::vector<uint8_t> row(1280 * 4);
        for (uint32_t y = 0; y < 720; ++y) {
            std::memset(row.data(), 0, row.size());
            if (y >= 100 && y < 300) // a 540x200 panel (more than 40% of the width)
                for (uint32_t x = 100; x < 640; ++x)
                    row[x * 4 + 3] = 255;
            if (y >= 600 && y < 640) // a prompt, 160x40
                for (uint32_t x = 1000; x < 1160; ++x)
                    row[x * 4 + 3] = 255;
            p.row(row.data(), y);
        }
        const uint32_t m = regions(p, r);
        check(m == 1 && r[0].x0 >= 980 && r[0].x1 <= 1180, "a panel left out, the prompt kept");
        float t[4];
        cardTangents(r[0], 1280, 720, t);
        check(t[2] - t[0] < .2f && t[3] - t[1] < .06f, "a prompt's card about 10 x 3 degrees");
    }
    // At most maxCards, the largest.
    {
        Grid s;
        s.begin(1280, 720);
        std::vector<uint8_t> row(1280 * 4);
        for (uint32_t y = 0; y < 720; ++y) {
            std::memset(row.data(), 0, row.size());
            if ((y / 16) % 6 == 1)
                for (uint32_t x = 0; x < 1280; ++x)
                    if ((x / 16) % 8 == 1)
                        row[x * 4 + 3] = 255;
            s.row(row.data(), y);
        }
        check(regions(s, r) == maxCards, "at most 8 cards");
    }
    // Faint alpha (a fade's edge) is not lit.
    {
        Grid f;
        f.begin(1280, 720);
        std::vector<uint8_t> row(1280 * 4, 10);
        f.row(row.data(), 0);
        check(f.lit == 0, "alpha 10 of 255 is not lit");
    }
    // A card cut by the view's edge crops its picture to match.
    {
        Region edge{1200, 300, 1280, 340, 4};
        float rect[4], crop[4];
        const bool ok = cardRect(edge, 1280, 720, -.7f, .7f, .7f, -.7f, 1000, 1000, rect, crop);
        check(ok && rect[2] == 1000.f && crop[2] < 1.f && crop[0] > 1200.f / 1280.f - 1e-4f, "cut at the edge, crop cut");
    }
    // Bad input.
    {
        Region bad{10, 10, 5, 20, 1};
        float rect[4], crop[4];
        check(!cardRect(bad, 1280, 720, L, R, U, D, W, H, rect, crop), "an empty piece: no card");
        Region ok{10, 10, 50, 50, 1};
        check(!cardRect(ok, 1280, 720, NAN, R, U, D, W, H, rect, crop), "a NaN lens: no card");
        check(!cardRect(ok, 0, 720, L, R, U, D, W, H, rect, crop), "no layer: no card");
    }
    // The find's back-off.
    check(locateDelay(0) == 3000 && locateDelay(1) == 6000 && locateDelay(3) == 24000 && locateDelay(9) == 30000,
          "a failed find again after 3, 6, 12, 24, then 30 s");
    std::printf("%s (%d failed)\n", failures ? "FAILED" : "all ok", failures);
    return failures ? 1 : 0;
}
