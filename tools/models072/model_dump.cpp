// HERO VR 0.0.72 MODEL CHECK: loads every shipped model slot with the game's own reader
// (mesh_model::Library::load, prepareTextures) and draws it with the game's own drawers
// (hero007::appendModel / appendBatarangModel into the textured model sink), then dumps what the
// overlay would get: the sink's vertices (spidy::Vertex, 44 bytes; shape = u, v, layer, 1), the
// texture array's level 0 per layer (RGBA8 sRGB) and a summary.json. tests/hero072_models_tests.py
// rasterises the dump next to the source model (the GPU itself is not run).
//
// PNG / JPEG are decoded by the game with WIC (Windows only); here a cache stands in: each image's
// bytes are hashed (FNV-1a 64) and read from <cache>/<hash>.rgba (uint32 width, height, then RGBA8).
// A miss writes <cache>/<hash>.enc and is listed in summary.json "misses" so the caller can decode it
// (PIL) and run again.
//
//   spidy_model_dump <Models folder/> <cache folder/> <out folder/> [texture size]
#include "spidy/hero007_visual.hpp"
#include "spidy/mesh_model.hpp"

#include <cstdio>
#include <fstream>
#include <set>
#include <string>
#include <vector>

using namespace spidy;
namespace mm = spidy::mesh_model;

static std::string cacheDir;
static std::set<std::string> misses;

static std::string hashOf(const uint8_t* d, size_t n) {
    uint64_t h = 1469598103934665603ull;
    for (size_t i = 0; i < n; ++i)
        h = (h ^ d[i]) * 1099511628211ull;
    char s[17];
    std::snprintf(s, sizeof s, "%016llx", static_cast<unsigned long long>(h));
    return s;
}
static bool cacheDecoder(const uint8_t* data, size_t size, mm::Picture& out) {
    const std::string h = hashOf(data, size);
    std::ifstream f(cacheDir + h + ".rgba", std::ios::binary);
    if (!f) {
        std::ofstream e(cacheDir + h + ".enc", std::ios::binary);
        e.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size));
        misses.insert(h);
        return false;
    }
    uint32_t wh[2]{};
    f.read(reinterpret_cast<char*>(wh), 8);
    if (!f || !wh[0] || !wh[1] || wh[0] > 16384 || wh[1] > 16384)
        return false;
    out.width = wh[0];
    out.height = wh[1];
    out.rgba.resize(size_t{wh[0]} * wh[1] * 4);
    f.read(reinterpret_cast<char*>(out.rgba.data()), static_cast<std::streamsize>(out.rgba.size()));
    return static_cast<bool>(f);
}
static std::string esc(const std::string& s) {
    std::string o;
    for (char c : s)
        o += c == '"' || c == '\\' ? std::string("\\") + c : std::string(1, c);
    return o;
}

int main(int argc, char** argv) {
    if (argc < 4) {
        std::fprintf(stderr, "usage: spidy_model_dump <Models/> <cache/> <out/> [size]\n");
        return 2;
    }
    const std::string root = argv[1], out = argv[3];
    cacheDir = argv[2];
    const uint32_t size = argc > 4 ? static_cast<uint32_t>(std::stoul(argv[4])) : 512;
    uint32_t levels = 1;
    while ((size >> levels) >= 1 && levels < 16)
        ++levels;
    mm::Library lib;
    for (uint32_t s = 1; s < mm::slots; ++s)
        lib.load(s, root);
    const auto array = mm::prepareTextures(lib, cacheDecoder, size, levels);
    for (size_t l = 0; l < array.layers.size(); ++l) {
        std::ofstream f(out + "layer_" + std::to_string(l) + ".rgba", std::ios::binary);
        const uint32_t wh[2]{size, size};
        f.write(reinterpret_cast<const char*>(wh), 8);
        f.write(reinterpret_cast<const char*>(array.layers[l].data()), std::streamsize{size} * size * 4);
    }
    std::string js = "{\n \"size\": " + std::to_string(size) + ",\n \"layers\": " + std::to_string(array.layers.size()) +
                     ",\n \"misses\": [";
    bool first = true;
    for (const auto& m : misses)
        js += (first ? "\"" : ", \"") + m + "\"", first = false;
    js += "],\n \"notes\": [";
    first = true;
    for (const auto& n : array.notes)
        js += (first ? "\"" : ", \"") + esc(n) + "\"", first = false;
    js += "],\n \"slots\": {";
    std::vector<Vertex> sink;
    hero007::setModelSink(&sink);
    for (uint32_t s = 1; s < mm::slots; ++s) {
        const mm::Mesh& m = lib.meshes[s];
        sink.clear();
        std::vector<Vertex> plain;
        bool drawn = false;
        if (const mm::Mesh* model = lib.model(s)) {
            hero007::Item it;
            it.at = {0, 0, 0};
            const Vec3 viewer{0, 0, 2};
            drawn = s < mm::styles ? hero007::appendBatarangModel(plain, it, *model, viewer)
                                   : hero007::appendModel(plain, it, *model, viewer);
        }
        const auto& v = sink.empty() ? plain : sink;
        std::ofstream f(out + "slot_" + std::to_string(s) + ".bin", std::ios::binary);
        f.write(reinterpret_cast<const char*>(v.data()), static_cast<std::streamsize>(v.size() * sizeof(Vertex)));
        size_t texturedVerts = 0;
        for (const auto& x : v)
            texturedVerts += x.shape.z >= 0 && x.shape.w == 1;
        std::set<int> layers;
        for (const auto& im : m.images)
            if (im.layer >= 0)
                layers.insert(im.layer);
        js += std::string(s == 1 ? "\n" : ",\n") + "  \"" + mm::slotFolders[s] + "\": {\"status\": \"" + mm::statusNames[lib.status[s]] +
              "\", \"error\": \"" + esc(lib.error[s]) + "\", \"triangles\": " + std::to_string(m.triangles()) +
              ", \"sourceTriangles\": " + std::to_string(m.sourceTriangles) + ", \"decimated\": " + (m.decimated ? "true" : "false") +
              ", \"hasUvs\": " + (m.hasUvs() ? "true" : "false") + ", \"hasNormals\": " + (m.hasNormals() ? "true" : "false") +
              ", \"textured\": " + (m.textured() ? "true" : "false") + ", \"images\": " + std::to_string(m.images.size()) +
              ", \"layers\": " + std::to_string(layers.size()) + ", \"drawn\": " + (drawn ? "true" : "false") +
              ", \"sink\": " + (sink.empty() ? "false" : "true") + ", \"vertices\": " + std::to_string(v.size()) +
              ", \"texturedVertices\": " + std::to_string(texturedVerts) + "}";
    }
    hero007::setModelSink(nullptr);
    js += "\n }\n}\n";
    std::ofstream(out + "summary.json") << js;
    std::printf("dumped %u slots, %zu layers, %zu misses\n", mm::slots - 1, array.layers.size(), misses.size());
    return 0;
}
