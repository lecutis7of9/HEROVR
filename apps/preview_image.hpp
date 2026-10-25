#pragma once
// The previews' drawing (spidy_mark_preview, spidy_phone_preview): an image in
// linear RGB, as the overlay blends, read from and written to 24-bit BMP
// files, and the overlay's own triangles drawn into it from an eye at the
// origin, a sample a pixel with their blending.
#include "spidy/vertex.hpp"
#include "spidy/web_shading.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace spidy::preview {
inline float decode(float c) {
    return c <= .04045f ? c / 12.92f : std::pow((c + .055f) / 1.055f, 2.4f);
}
inline float encode(float c) {
    c = std::clamp(c, 0.f, 1.f);
    return c <= .0031308f ? c * 12.92f : 1.055f * std::pow(c, 1 / 2.4f) - .055f;
}
struct Image {
    int w{}, h{};
    std::vector<float> rgb; // linear, as the overlay blends
    Image() = default;
    Image(int width, int height) : w(width), h(height), rgb(static_cast<size_t>(width) * height * 3, 0.f) {}
    float* at(int x, int y) {
        return &rgb[(static_cast<size_t>(y) * w + x) * 3];
    }
    const float* at(int x, int y) const {
        return &rgb[(static_cast<size_t>(y) * w + x) * 3];
    }
};
inline bool load(const std::string& path, Image& image) {
    std::ifstream in(path, std::ios::binary);
    const std::vector<unsigned char> d((std::istreambuf_iterator<char>(in)), {});
    const auto word = [&](size_t o) {
        return static_cast<uint32_t>(d[o] | d[o + 1] << 8 | d[o + 2] << 16 | static_cast<uint32_t>(d[o + 3]) << 24);
    };
    if (d.size() < 54 || d[0] != 'B' || d[1] != 'M' || (d[28] != 24 && d[28] != 32) || word(30))
        return false;
    const int w = static_cast<int>(word(18)), h = static_cast<int>(word(22)), bytes = d[28] / 8;
    const size_t offset = word(10), stride = (static_cast<size_t>(w) * bytes + 3) & ~size_t{3};
    if (w <= 0 || !h || offset + stride * static_cast<size_t>(std::abs(h)) > d.size())
        return false;
    image = Image(w, std::abs(h));
    for (int y = 0; y < image.h; ++y) {
        const unsigned char* row = &d[offset + stride * (h > 0 ? image.h - 1 - y : y)];
        for (int x = 0; x < w; ++x) {
            const unsigned char* p = row + x * bytes;
            float* o = image.at(x, y);
            o[0] = decode(p[2] / 255.f);
            o[1] = decode(p[1] / 255.f);
            o[2] = decode(p[0] / 255.f);
        }
    }
    return true;
}
inline bool save(const Image& image, const std::string& path) {
    const size_t stride = (static_cast<size_t>(image.w) * 3 + 3) & ~size_t{3};
    std::vector<unsigned char> d(54 + stride * image.h, 0);
    const auto put = [&](size_t o, uint32_t v) {
        for (int i = 0; i < 4; ++i)
            d[o + i] = static_cast<unsigned char>(v >> (8 * i));
    };
    d[0] = 'B';
    d[1] = 'M';
    put(2, static_cast<uint32_t>(d.size()));
    put(10, 54);
    put(14, 40);
    put(18, static_cast<uint32_t>(image.w));
    put(22, static_cast<uint32_t>(image.h));
    d[26] = 1;
    d[28] = 24;
    put(34, static_cast<uint32_t>(stride * image.h));
    for (int y = 0; y < image.h; ++y) {
        unsigned char* row = &d[54 + stride * (image.h - 1 - y)];
        for (int x = 0; x < image.w; ++x) {
            const float* p = image.at(x, y);
            for (int k = 0; k < 3; ++k)
                row[x * 3 + k] = static_cast<unsigned char>(std::lround(encode(p[2 - k]) * 255));
        }
    }
    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char*>(d.data()), static_cast<std::streamsize>(d.size()));
    std::printf("%s %dx%d\n", path.c_str(), image.w, image.h);
    return file.good();
}
// The eye: at the origin, looking along -z, `f` pixels a unit of tangent
// about (cx, cy).
struct Camera {
    float f, cx, cy;
    Vec3 ray(float u, float v) const {
        return normalized({(u - cx) / f, -(v - cy) / f, -1});
    }
};
// Draws the vertices into the cell (x0, y0, w, h) of `image`, camera pixel
// (u0, v0) at the cell's corner: a sample at each pixel's centre, blended by
// the vertices' opacity as the overlay blends. With `depth` (a float a pixel
// of the image, cleared to infinity) the triangles are depth-tested and
// write their depth, as the overlay's depth-writing pass does; without it
// they are drawn in order, as its translucent pass draws.
inline void draw(Image& image, const std::vector<Vertex>& vertices, const Camera& camera, float u0, float v0,
                 int x0, int y0, int w, int h, std::vector<float>* depth = nullptr) {
    namespace ws = web_shading;
    for (size_t i = 0; i + 2 < vertices.size(); i += 3) {
        float x[3], y[3], z[3];
        bool ahead = true;
        for (int k = 0; k < 3 && ahead; ++k) {
            const Vec3 p = vertices[i + k].position;
            ahead = p.z < -.01f;
            x[k] = camera.cx + camera.f * p.x / -p.z - u0 + static_cast<float>(x0);
            y[k] = camera.cy - camera.f * p.y / -p.z - v0 + static_cast<float>(y0);
            z[k] = -p.z;
        }
        const float area = (x[1] - x[0]) * (y[2] - y[0]) - (x[2] - x[0]) * (y[1] - y[0]);
        if (!ahead || std::abs(area) < 1e-9f)
            continue;
        // The shape's change across a pixel as on the GPU (fwidth): over the
        // triangle its change is even, so its gradient is exact.
        const Shape* s[3] = {&vertices[i].shape, &vertices[i + 1].shape, &vertices[i + 2].shape};
        const bool strand = ws::isStrand(*s[0]), tube = ws::isTube(*s[0]), ink = ws::isInk(*s[0]),
                   gloss = ws::isGloss(*s[0]);
        const auto fwidth = [&](float a0, float a1, float a2) {
            const float dx = ((a1 - a0) * (y[2] - y[0]) - (a2 - a0) * (y[1] - y[0])) / area;
            const float dy = ((a2 - a0) * (x[1] - x[0]) - (a1 - a0) * (x[2] - x[0])) / area;
            return std::abs(dx) + std::abs(dy);
        };
        float perPixel[2]{};
        if (strand || ink) {
            perPixel[0] = fwidth(s[0]->x, s[1]->x, s[2]->x);
            perPixel[1] = fwidth(s[0]->y, s[1]->y, s[2]->y);
        }
        if (gloss) {
            perPixel[0] = fwidth(s[0]->z, s[1]->z, s[2]->z);
            perPixel[1] = fwidth(s[0]->x, s[1]->x, s[2]->x);
        }
        if (tube) {
            perPixel[0] = fwidth(ws::tubeLight(*s[0]), ws::tubeLight(*s[1]), ws::tubeLight(*s[2]));
            perPixel[1] = fwidth(s[0]->w, s[1]->w, s[2]->w);
        }
        const int left = std::max(x0, static_cast<int>(std::floor(std::min({x[0], x[1], x[2]}))));
        const int right = std::min(x0 + w - 1, static_cast<int>(std::ceil(std::max({x[0], x[1], x[2]}))));
        const int top = std::max(y0, static_cast<int>(std::floor(std::min({y[0], y[1], y[2]}))));
        const int bottom = std::min(y0 + h - 1, static_cast<int>(std::ceil(std::max({y[0], y[1], y[2]}))));
        for (int py = top; py <= bottom; ++py)
            for (int px = left; px <= right; ++px) {
                // A centre on an edge two triangles share belongs to one of them, as on the GPU: the sample
                // sits a hair off the centre, so no edge runs through it, and a translucent mark's seams are
                // not blended twice.
                const float sx = static_cast<float>(px) + .5f + 1.7e-4f;
                const float sy = static_cast<float>(py) + .5f + 3.1e-4f;
                const float b0 = ((x[1] - sx) * (y[2] - sy) - (x[2] - sx) * (y[1] - sy)) / area;
                const float b1 = ((x[2] - sx) * (y[0] - sy) - (x[0] - sx) * (y[2] - sy)) / area;
                const float b2 = 1 - b0 - b1;
                if (b0 < 0 || b1 < 0 || b2 < 0)
                    continue;
                float* d = nullptr;
                float distance = 0;
                if (depth) {
                    // Depth from 1/z, which is even across the screen.
                    distance = 1 / (b0 / z[0] + b1 / z[1] + b2 / z[2]);
                    d = &(*depth)[static_cast<size_t>(py) * image.w + px];
                    if (!(distance < *d))
                        continue;
                }
                float a = std::clamp(
                    vertices[i].alpha * b0 + vertices[i + 1].alpha * b1 + vertices[i + 2].alpha * b2, 0.f, 1.f);
                Vec3 c = vertices[i].color * b0 + vertices[i + 1].color * b1 + vertices[i + 2].color * b2;
                const Shape shape{s[0]->x * b0 + s[1]->x * b1 + s[2]->x * b2, s[0]->y * b0 + s[1]->y * b1 + s[2]->y * b2,
                                  s[0]->z * b0 + s[1]->z * b1 + s[2]->z * b2, s[0]->w * b0 + s[1]->w * b1 + s[2]->w * b2};
                if (strand)
                    ws::shadeStrand(c, a, shape, perPixel[0], perPixel[1]);
                else if (tube)
                    ws::shadeTube(c, shape, perPixel[0], perPixel[1]);
                else if (ink)
                    ws::shadeInk(c, a, shape, perPixel[0]);
                else if (gloss) {
                    // Brighter than white saturates, as a UNORM target does before blending.
                    ws::shadeGloss(c, shape, perPixel[0], perPixel[1]);
                    c = {std::min(c.x, 1.f), std::min(c.y, 1.f), std::min(c.z, 1.f)};
                }
                if (d)
                    *d = distance;
                float* o = image.at(px, py);
                o[0] = c.x * a + o[0] * (1 - a);
                o[1] = c.y * a + o[1] * (1 - a);
                o[2] = c.z * a + o[2] * (1 - a);
            }
    }
}
// Update 36: the overlay's art pass (D3D12Renderer::renderViews' `art`):
// each vertex's colour and opacity times the atlas's texel at its shape.xy,
// sampled as the GPU does (sRGB decoded, bilinear, the mip a pixel's step
// across the atlas picks), blended in order without depth.
struct Texture {
    struct Level {
        int w{}, h{};
        std::vector<float> rgba; // linear colour, straight alpha
    };
    std::vector<Level> levels;
    template <class Mip> static Texture from(const std::vector<Mip>& mips) {
        Texture t;
        for (const auto& m : mips) {
            Level l{m.width, m.height, std::vector<float>(m.rgba.size())};
            for (size_t i = 0; i < m.rgba.size(); ++i)
                l.rgba[i] = i % 4 == 3 ? m.rgba[i] / 255.f : decode(m.rgba[i] / 255.f);
            t.levels.push_back(std::move(l));
        }
        return t;
    }
    void sample(int level, float u, float v, float out[4]) const {
        const auto& l = levels[static_cast<size_t>(std::clamp(level, 0, static_cast<int>(levels.size()) - 1))];
        const float x = std::clamp(u, 0.f, 1.f) * l.w - .5f, y = std::clamp(v, 0.f, 1.f) * l.h - .5f;
        const int x0 = static_cast<int>(std::floor(x)), y0 = static_cast<int>(std::floor(y));
        const float fx = x - x0, fy = y - y0;
        for (int c = 0; c < 4; ++c)
            out[c] = 0;
        for (int dy = 0; dy < 2; ++dy)
            for (int dx = 0; dx < 2; ++dx) {
                const int xx = std::clamp(x0 + dx, 0, l.w - 1), yy = std::clamp(y0 + dy, 0, l.h - 1);
                const float wgt = (dx ? fx : 1 - fx) * (dy ? fy : 1 - fy);
                const float* p = &l.rgba[(static_cast<size_t>(yy) * l.w + xx) * 4];
                for (int c = 0; c < 4; ++c)
                    out[c] += p[c] * wgt;
            }
    }
};
inline void drawArt(Image& image, const std::vector<Vertex>& vertices, const Texture& texture, const Camera& camera,
                    float u0, float v0, int x0, int y0, int w, int h, int maxLevel = 4) {
    if (texture.levels.empty())
        return;
    const float tw = static_cast<float>(texture.levels[0].w), th = static_cast<float>(texture.levels[0].h);
    for (size_t i = 0; i + 2 < vertices.size(); i += 3) {
        float x[3], y[3], iz[3];
        bool ahead = true;
        for (int k = 0; k < 3 && ahead; ++k) {
            const Vec3 p = vertices[i + k].position;
            ahead = p.z < -.01f;
            x[k] = camera.cx + camera.f * p.x / -p.z - u0 + static_cast<float>(x0);
            y[k] = camera.cy - camera.f * p.y / -p.z - v0 + static_cast<float>(y0);
            iz[k] = 1 / -p.z;
        }
        const float area = (x[1] - x[0]) * (y[2] - y[0]) - (x[2] - x[0]) * (y[1] - y[0]);
        if (!ahead || std::abs(area) < 1e-9f)
            continue;
        // The mip from the texels a pixel steps over (the panel faces the eye,
        // so this is near enough even over a triangle).
        const auto grad = [&](float a0, float a1, float a2) {
            const float dx = ((a1 - a0) * (y[2] - y[0]) - (a2 - a0) * (y[1] - y[0])) / area;
            const float dy = ((a2 - a0) * (x[1] - x[0]) - (a1 - a0) * (x[2] - x[0])) / area;
            return std::max(std::abs(dx), std::abs(dy));
        };
        const Shape* s[3] = {&vertices[i].shape, &vertices[i + 1].shape, &vertices[i + 2].shape};
        const float step = std::max(grad(s[0]->x, s[1]->x, s[2]->x) * tw, grad(s[0]->y, s[1]->y, s[2]->y) * th);
        const int level = std::clamp(static_cast<int>(std::floor(std::log2(std::max(step, 1.f)) + .5f)), 0, maxLevel);
        const int left = std::max(x0, static_cast<int>(std::floor(std::min({x[0], x[1], x[2]}))));
        const int right = std::min(x0 + w - 1, static_cast<int>(std::ceil(std::max({x[0], x[1], x[2]}))));
        const int top = std::max(y0, static_cast<int>(std::floor(std::min({y[0], y[1], y[2]}))));
        const int bottom = std::min(y0 + h - 1, static_cast<int>(std::ceil(std::max({y[0], y[1], y[2]}))));
        for (int py = top; py <= bottom; ++py)
            for (int px = left; px <= right; ++px) {
                const float sx = static_cast<float>(px) + .5f + 1.7e-4f;
                const float sy = static_cast<float>(py) + .5f + 3.1e-4f;
                float b0 = ((x[1] - sx) * (y[2] - sy) - (x[2] - sx) * (y[1] - sy)) / area;
                float b1 = ((x[2] - sx) * (y[0] - sy) - (x[0] - sx) * (y[2] - sy)) / area;
                float b2 = 1 - b0 - b1;
                if (b0 < 0 || b1 < 0 || b2 < 0)
                    continue;
                // Perspective-correct, as the GPU interpolates.
                const float q = b0 * iz[0] + b1 * iz[1] + b2 * iz[2];
                b0 *= iz[0] / q;
                b1 *= iz[1] / q;
                b2 = 1 - b0 - b1;
                const auto& a = vertices[i];
                const auto& b = vertices[i + 1];
                const auto& c = vertices[i + 2];
                float t[4];
                texture.sample(level, a.shape.x * b0 + b.shape.x * b1 + c.shape.x * b2,
                               a.shape.y * b0 + b.shape.y * b1 + c.shape.y * b2, t);
                const Vec3 color = (a.color * b0 + b.color * b1 + c.color * b2);
                const float alpha = std::clamp((a.alpha * b0 + b.alpha * b1 + c.alpha * b2) * t[3], 0.f, 1.f);
                float* o = image.at(px, py);
                o[0] = std::min(color.x * t[0], 1.f) * alpha + o[0] * (1 - alpha);
                o[1] = std::min(color.y * t[1], 1.f) * alpha + o[1] * (1 - alpha);
                o[2] = std::min(color.z * t[2], 1.f) * alpha + o[2] * (1 - alpha);
            }
    }
}
} // namespace spidy::preview
