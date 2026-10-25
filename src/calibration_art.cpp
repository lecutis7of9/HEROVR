// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
// Update 36: the calibration art atlas (calibration_art.hpp).
#include "spidy/calibration_art.hpp"
#include <algorithm>
#include <array>
#include <cmath>

namespace spidy::calibration_art {
// calibration_art_data.cpp, generated from assets/calibration_art.qoi.
extern const std::uint8_t atlasFile[];
extern const std::size_t atlasFileSize;

Uv uv(const Rect& r, float s, float t) {
    const float x0 = r.x + .5f, x1 = static_cast<float>(r.x + r.w) - .5f;
    const float y0 = r.y + .5f, y1 = static_cast<float>(r.y + r.h) - .5f;
    s = std::clamp(s, 0.f, 1.f);
    t = std::clamp(t, 0.f, 1.f);
    return {(x0 + (x1 - x0) * s) / width, (y0 + (y1 - y0) * t) / height};
}

bool decodeQoi(std::span<const std::uint8_t> f, Image& out, std::size_t maxPixels) {
    out = {};
    if (f.size() < 14 + 8 || f[0] != 'q' || f[1] != 'o' || f[2] != 'i' || f[3] != 'f')
        return false;
    const auto be = [&](std::size_t i) {
        return std::uint32_t{f[i]} << 24 | std::uint32_t{f[i + 1]} << 16 | std::uint32_t{f[i + 2]} << 8 | f[i + 3];
    };
    const std::uint32_t w = be(4), h = be(8);
    if (!w || !h || w > 16384 || h > 16384 || std::size_t{w} * h > maxPixels || (f[12] != 3 && f[12] != 4))
        return false;
    const std::size_t n = std::size_t{w} * h;
    std::vector<std::uint8_t> px(n * 4);
    std::array<std::array<std::uint8_t, 4>, 64> index{};
    std::array<std::uint8_t, 4> p{0, 0, 0, 255};
    std::size_t at = 14;
    const std::size_t end = f.size() - 8;
    int run = 0;
    for (std::size_t i = 0; i < n; ++i) {
        if (run > 0) {
            --run;
        } else {
            if (at >= end)
                return false;
            const std::uint8_t b = f[at++];
            if (b == 0xfe || b == 0xff) {
                const std::size_t k = b == 0xfe ? 3 : 4;
                if (at + k > end)
                    return false;
                for (std::size_t c = 0; c < k; ++c)
                    p[c] = f[at++];
            } else if ((b & 0xc0) == 0x00) {
                p = index[b];
            } else if ((b & 0xc0) == 0x40) {
                p[0] = static_cast<std::uint8_t>(p[0] + ((b >> 4) & 3) - 2);
                p[1] = static_cast<std::uint8_t>(p[1] + ((b >> 2) & 3) - 2);
                p[2] = static_cast<std::uint8_t>(p[2] + (b & 3) - 2);
            } else if ((b & 0xc0) == 0x80) {
                if (at >= end)
                    return false;
                const std::uint8_t b2 = f[at++];
                const int dg = (b & 0x3f) - 32;
                p[0] = static_cast<std::uint8_t>(p[0] + dg - 8 + ((b2 >> 4) & 0xf));
                p[1] = static_cast<std::uint8_t>(p[1] + dg);
                p[2] = static_cast<std::uint8_t>(p[2] + dg - 8 + (b2 & 0xf));
            } else {
                run = b & 0x3f; // this pixel and `run` more
            }
            index[(p[0] * 3 + p[1] * 5 + p[2] * 7 + p[3] * 11) % 64] = p;
        }
        std::copy(p.begin(), p.end(), px.begin() + static_cast<std::ptrdiff_t>(i * 4));
    }
    out.width = static_cast<int>(w);
    out.height = static_cast<int>(h);
    out.rgba = std::move(px);
    return true;
}

std::span<const std::uint8_t> embedded() {
    return {atlasFile, atlasFileSize};
}

bool load(Image& out) {
    if (!decodeQoi(embedded(), out) || out.width != width || out.height != height) {
        out = {};
        return false;
    }
    return true;
}

namespace {
float decode(std::uint8_t c) {
    const float x = c / 255.f;
    return x <= .04045f ? x / 12.92f : std::pow((x + .055f) / 1.055f, 2.4f);
}
std::uint8_t encode(float x) {
    x = std::clamp(x, 0.f, 1.f);
    const float s = x <= .0031308f ? x * 12.92f : 1.055f * std::pow(x, 1 / 2.4f) - .055f;
    return static_cast<std::uint8_t>(std::lround(s * 255));
}
} // namespace

std::vector<Image> mips(const Image& base, int levels) {
    std::vector<Image> chain;
    if (base.width <= 0 || base.height <= 0 ||
        base.rgba.size() != static_cast<std::size_t>(base.width) * base.height * 4 || levels < 1)
        return chain;
    std::array<float, 256> lut{};
    for (int i = 0; i < 256; ++i)
        lut[i] = decode(static_cast<std::uint8_t>(i));
    chain.push_back(base);
    while (static_cast<int>(chain.size()) < levels && (chain.back().width > 1 || chain.back().height > 1)) {
        const Image& a = chain.back();
        Image b;
        b.width = std::max(a.width / 2, 1);
        b.height = std::max(a.height / 2, 1);
        b.rgba.resize(static_cast<std::size_t>(b.width) * b.height * 4);
        for (int y = 0; y < b.height; ++y)
            for (int x = 0; x < b.width; ++x) {
                float sum[3]{}, alpha = 0, weight = 0;
                for (int dy = 0; dy < 2; ++dy)
                    for (int dx = 0; dx < 2; ++dx) {
                        const int sx = std::min(2 * x + dx, a.width - 1), sy = std::min(2 * y + dy, a.height - 1);
                        const auto* s = &a.rgba[(static_cast<std::size_t>(sy) * a.width + sx) * 4];
                        const float w = s[3] / 255.f;
                        for (int c = 0; c < 3; ++c)
                            sum[c] += lut[s[c]] * (w + 1e-4f);
                        weight += w + 1e-4f;
                        alpha += w;
                    }
                auto* d = &b.rgba[(static_cast<std::size_t>(y) * b.width + x) * 4];
                for (int c = 0; c < 3; ++c)
                    d[c] = encode(sum[c] / weight);
                d[3] = static_cast<std::uint8_t>(std::lround(alpha / 4 * 255));
            }
        chain.push_back(std::move(b));
    }
    return chain;
}
} // namespace spidy::calibration_art
