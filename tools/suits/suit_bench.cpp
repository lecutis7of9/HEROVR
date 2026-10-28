// Build 9 SUITS: the worn-suit cost and preview. Loads every suit slot with the game's own reader
// (mesh_model::Library::load, prepareTextures) and runs the game's own per-frame wear path (suit_wear::estimate,
// moves, skin, then draw: the SUIT BODY part in first person and the whole suit through a mirror's reflection)
// for a standing player (eye 1.65 m, hands up in front), timing each stage over many frames. Dumps the drawn
// vertices (spidy::Vertex) and a timing JSON; tools/suits/suit_preview.py rasterises them.
//
//   spidy_suit_bench <Models/> <cache/> <out/> [frames] [texture size]
// (the cache: model_dump's, <hash>.rgba per image; a miss is written as <hash>.enc and listed.)
#include "spidy/hero007_visual.hpp"
#include "spidy/mesh_model.hpp"
#include "spidy/suit_wear.hpp"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstdio>
#include <fstream>
#include <set>
#include <string>
#include <vector>

using namespace spidy;
namespace mm = spidy::mesh_model;
namespace sw = spidy::suit_wear;

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
    out.width = wh[0], out.height = wh[1];
    out.rgba.resize(size_t{wh[0]} * wh[1] * 4);
    f.read(reinterpret_cast<char*>(out.rgba.data()), static_cast<std::streamsize>(out.rgba.size()));
    return static_cast<bool>(f);
}
static void save(const std::string& path, const std::vector<Vertex>& v) {
    std::ofstream f(path, std::ios::binary);
    f.write(reinterpret_cast<const char*>(v.data()), static_cast<std::streamsize>(v.size() * sizeof(Vertex)));
}
using Clock = std::chrono::steady_clock;
static double us(Clock::time_point a, Clock::time_point b) { return std::chrono::duration<double, std::micro>(b - a).count(); }

int main(int argc, char** argv) {
    if (argc < 4) {
        std::fprintf(stderr, "usage: spidy_suit_bench <Models/> <cache/> <out/> [frames] [size]\n");
        return 2;
    }
    const std::string root = argv[1], out = argv[3];
    cacheDir = argv[2];
    const int frames = argc > 4 ? std::max(1, std::stoi(argv[4])) : 300;
    const uint32_t size = argc > 5 ? static_cast<uint32_t>(std::stoul(argv[5])) : 512;
    uint32_t levels = 1;
    while ((size >> levels) >= 1 && levels < 16)
        ++levels;
    mm::Library lib;
    for (uint32_t s = mm::slotSuitFirst; s < mm::slots; ++s)
        lib.load(s, root);
    const auto array = mm::prepareTextures(lib, cacheDecoder, size, levels);
    for (size_t l = 0; l < array.layers.size(); ++l) {
        std::ofstream f(out + "layer_" + std::to_string(l) + ".rgba", std::ios::binary);
        const uint32_t wh[2]{size, size};
        f.write(reinterpret_cast<const char*>(wh), 8);
        f.write(reinterpret_cast<const char*>(array.layers[l].data()), std::streamsize{size} * size * 4);
    }
    // The player: feet at the origin, eyes 1.65 m up looking along -z (a little down), both hands up in front.
    sw::Tracked t;
    t.feet = {0, 0, 0};
    t.eye = {0, 1.65f, 0};
    t.eyeForward = normalized(Vec3{0, -.35f, -1});
    t.eyeUp = normalized(cross(cross(t.eyeForward, Vec3{0, 1, 0}), t.eyeForward));
    for (int i = 0; i < 2; ++i) {
        t.hand[i] = true;
        t.grip[i] = {i == 0 ? -.22f : .22f, 1.22f, -.38f};
        // Test build 12 HANDS: a real OpenXR grip (Touch) for a hand held out level ahead, its palm in: the grip's
        // -Z up the fist's tube (60 degrees up), +Y = Z x X, +X right on both hands
        t.gripForward[i] = Vec3{0, .8660254f, -.5f};
        t.gripUp[i] = Vec3{0, .5f, .8660254f};
        t.gripX[i] = {1, 0, 0};
        t.handleTiltDeg[i] = 30.f;
        if (const char* c = std::getenv("SUIT_BENCH_CURL"))
            t.curl[i] = static_cast<float>(std::atof(c));
    }
    t.standingEye = 1.65f;
    // A mirror 1.4 m ahead facing the player (its normal +z), life size.
    const Vec3 n{0, 0, 1}, c{0, 1.f, -1.4f};
    float place[12];
    {
        const float nn[3] = {n.x, n.y, n.z}, cc[3] = {c.x, c.y, c.z};
        for (int r = 0; r < 3; ++r) {
            float tt = cc[r];
            for (int q = 0; q < 3; ++q) {
                const float m = (r == q ? 1.f : 0.f) - 2.f * nn[r] * nn[q];
                place[r * 4 + q] = m;
                tt -= m * cc[q];
            }
            place[r * 4 + 3] = tt;
        }
    }
    std::string js = "{\n \"frames\": " + std::to_string(frames) + ",\n \"layers\": " + std::to_string(array.layers.size()) +
                     ",\n \"misses\": " + std::to_string(misses.size()) + ",\n \"suits\": {";
    std::vector<Vertex> sink;
    sink.reserve(hero007::modelSinkBudget);
    for (uint32_t k = 0; k < mm::suitSlots; ++k) {
        const uint32_t s = mm::slotSuitFirst + k;
        const mm::Mesh* m = lib.model(s);
        sw::Rig rig;
        const auto r0 = Clock::now();
        const bool rigged = m && sw::makeRig(*m, rig, mm::suitFolders[k]);
        const double rigUs = us(r0, Clock::now());
        double tEst = 0, tMove = 0, tSkin = 0, tArms = 0, tBody = 0, tMirror = 0, worst = 0;
        size_t vArms = 0, vBody = 0, vMirror = 0;
        bool ok = rigged;
        sw::Skinned sk;
        for (int f = 0; ok && f < frames; ++f) {
            sw::Tracked tf = t; // a little sway, as a live player
            const float a = .02f * std::sin(f * .1f);
            tf.grip[0].x += a, tf.grip[1].y += a, tf.eye.x += a * .5f;
            sw::Skeleton live;
            sw::BoneMove mv[sw::bones];
            const auto a0 = Clock::now();
            ok = sw::estimate(rig, tf, live);
            const auto a1 = Clock::now();
            ok = ok && sw::moves(rig, live, mv);
            const auto a2 = Clock::now();
            ok = ok && sw::skin(rig, mv, sk);
            const auto a3 = Clock::now();
            sink.clear();
            ok = ok && sw::draw(sink, hero007::modelSinkBudget, *m, rig, sk, sw::armBones);
            const auto a4 = Clock::now();
            if (f == frames - 1)
                save(out + "arms_" + std::to_string(k) + ".bin", sink), vArms = sink.size();
            sink.clear();
            ok = ok && sw::draw(sink, hero007::modelSinkBudget, *m, rig, sk, sw::bodyBones);
            const auto a5 = Clock::now();
            if (f == frames - 1)
                save(out + "body_" + std::to_string(k) + ".bin", sink), vBody = sink.size();
            sink.clear();
            ok = ok && sw::draw(sink, hero007::modelSinkBudget, *m, rig, sk, sw::allBones, place);
            const auto a6 = Clock::now();
            if (f == frames - 1)
                save(out + "mirror_" + std::to_string(k) + ".bin", sink), vMirror = sink.size();
            tEst += us(a0, a1), tMove += us(a1, a2), tSkin += us(a2, a3), tArms += us(a3, a4), tBody += us(a4, a5), tMirror += us(a5, a6);
            worst = std::max(worst, us(a0, a4) + us(a5, a6));
        }
        const double F = frames;
        char line[1024];
        std::snprintf(line, sizeof line,
                      "%s\n  \"%s\": {\"ok\": %s, \"triangles\": %u, \"vertices\": %zu, \"rigUs\": %.1f, \"estimateUs\": %.2f, \"movesUs\": %.2f, "
                      "\"skinUs\": %.1f, \"drawArmsUs\": %.1f, \"drawBodyUs\": %.1f, \"drawMirrorUs\": %.1f, \"frameArmsMirrorUs\": %.1f, "
                      "\"worstFrameUs\": %.1f, \"armsVertices\": %zu, \"bodyVertices\": %zu, \"mirrorVertices\": %zu}",
                      k ? "," : "", mm::suitFolders[k], ok ? "true" : "false", m ? m->triangles() : 0u, rig.rest.size(), rigUs, tEst / F,
                      tMove / F, tSkin / F, tArms / F, tBody / F, tMirror / F, (tEst + tMove + tSkin + tArms + tMirror) / F, worst, vArms,
                      vBody, vMirror);
        js += line;
    }
    js += "\n }\n}\n";
    std::ofstream(out + "bench.json") << js;
    std::printf("%s", js.c_str());
    return misses.empty() ? 0 : 3;
}
