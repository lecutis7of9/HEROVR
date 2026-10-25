// Update 38.5 90s ANIMATED preview: the port's own eye_color::cartoonPixel (the shader's CPU copy) on a PPM.
#include "spidy/eye_color.hpp"
#include <cstdio>
#include <cstdlib>
#include <vector>
using namespace spidy::eye_color;
int main(int argc, char** argv) {
    if (argc < 7) return 1; // in out strength cartoon outlines bands
    Config c;
    c.on = true;
    c.filter = animated90s;
    c.strength = static_cast<uint32_t>(std::atoi(argv[3]));
    c.cartoon = static_cast<uint32_t>(std::atoi(argv[4]));
    c.outlines = std::atoi(argv[5]) != 0;
    c.bands = static_cast<uint32_t>(std::atoi(argv[6]));
    FILE* in = std::fopen(argv[1], "rb");
    int w, h, mx;
    if (!in || std::fscanf(in, "P6 %d %d %d", &w, &h, &mx) != 3) return 2;
    std::fgetc(in);
    std::vector<unsigned char> px(static_cast<size_t>(w) * h * 3), out(px.size());
    if (std::fread(px.data(), 1, px.size(), in) != px.size()) return 3;
    std::fclose(in);
    const Params p = params(c);
    const Toon t = toon(c);
    auto fetch = [&](int x, int y) {
        x = std::clamp(x, 0, w - 1);
        y = std::clamp(y, 0, h - 1);
        const unsigned char* q = &px[(static_cast<size_t>(y) * w + x) * 3];
        return std::array<float, 3>{q[0] / 255.f, q[1] / 255.f, q[2] / 255.f};
    };
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            auto o = cartoonPixel(p, t, fetch, x, y, (x + .5f) / w, (y + .5f) / h);
            unsigned char* q = &out[(static_cast<size_t>(y) * w + x) * 3];
            for (int i = 0; i < 3; ++i) q[i] = static_cast<unsigned char>(o[static_cast<size_t>(i)] * 255.f + .5f);
        }
    FILE* o = std::fopen(argv[2], "wb");
    std::fprintf(o, "P6\n%d %d\n255\n", w, h);
    std::fwrite(out.data(), 1, out.size(), o);
    std::fclose(o);
}
