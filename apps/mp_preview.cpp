// 0.0.73 MP SOLO TEST: desktop previews of the remote player, from the game's own code. The player's pose goes
// through the real packet code (mp::fromLocal -> encode -> decode), then mp::partner / mp::toTracked and the
// game's own suit path (suit_wear estimate / moves / skin / draw with mesh_model's mesh) or mp::appendFigure,
// plus mp::appendNameplate - exactly what game_xr draws. Dumps spidy::Vertex arrays; tools/mp/mp_preview.py
// rasterises them from the player's eye.
//
//   spidy_mp_preview <Models/> <cache/> <out/>
// Writes out/layer_N.rgba (the texture array), out/model_K.bin (suit triangles), out/soft_K.bin (figure +
// nameplate, translucent) and out/scenes.json.
#include "spidy/hero007_visual.hpp"
#include "spidy/mesh_model.hpp"
#include "spidy/mp_ghost.hpp"
#include "spidy/mp_net.hpp"
#include "spidy/suit_wear.hpp"

#include <cstdio>
#include <cstring>
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
static int suitIndex(const char* folder) {
    for (uint32_t k = 0; k < mm::suitSlots; ++k)
        if (!std::strcmp(mm::suitFolders[k], folder))
            return static_cast<int>(k);
    return -1;
}

int main(int argc, char** argv) {
    if (argc < 4) {
        std::fprintf(stderr, "usage: spidy_mp_preview <Models/> <cache/> <out/>\n");
        return 2;
    }
    const std::string root = argv[1], out = argv[3];
    cacheDir = argv[2];
    const uint32_t size = 512;
    uint32_t levels = 1;
    while ((size >> levels) >= 1 && levels < 16)
        ++levels;
    mm::Library lib;
    for (uint32_t s = mm::slotSuitFirst; s < mm::slotSuitEnd; ++s)
        lib.load(s, root);
    const auto array = mm::prepareTextures(lib, cacheDecoder, size, levels);
    for (size_t l = 0; l < array.layers.size(); ++l) {
        std::ofstream f(out + "layer_" + std::to_string(l) + ".rgba", std::ios::binary);
        const uint32_t wh[2]{size, size};
        f.write(reinterpret_cast<const char*>(wh), 8);
        f.write(reinterpret_cast<const char*>(array.layers[l].data()), std::streamsize{size} * size * 4);
    }
    struct Scene {
        const char* title;
        uint8_t hero;
        const char* suit; // nullptr: the hero's own look
        bool buddy;
        const char* name;
        int ping;
    };
    const Scene scenes[] = {
        {"LOOPBACK GHOST - you in THOR's suit, mirrored, your raised RIGHT hand is its LEFT", 9, "Suit_thor_1", false, "", 3},
        {"LOOPBACK GHOST - no suit model (IRON MAN's own look): the soft figure in his colour", 1, nullptr, false, "", 4},
        {"TEST BUDDY connected - its puppet (BATMAN suit) waving, walking its circle", 4, "Suit_batman_1", true, "TEST BUDDY", 6},
    };
    std::string js = "{\n \"layers\": " + std::to_string(array.layers.size()) + ",\n \"misses\": " + std::to_string(misses.size()) + ",\n \"scenes\": [";
    std::vector<Vertex> model, soft;
    model.reserve(hero007::modelSinkBudget);
    for (size_t k = 0; k < sizeof scenes / sizeof scenes[0]; ++k) {
        const Scene& sc = scenes[k];
        // The player (or the buddy's puppet): standing, right hand raised (a wave), left hand low in front.
        mp::LocalPose l;
        l.eye = {{0, 1.65f, 0}, Quat::yaw(sc.buddy ? .3f : 0.f)};
        l.hand[0] = l.hand[1] = true;
        l.grip[0] = {{-.25f, 1.05f, -.25f}, {}};
        l.grip[1] = {{.38f, 1.78f, -.18f}, {}};
        l.curl[0] = .2f, l.curl[1] = 0;
        l.standingEye = 1.65f;
        l.worldFeet = sc.buddy ? Vec3{.6f, 0, .3f} : Vec3{};
        const int si = sc.suit ? suitIndex(sc.suit) : -1;
        const uint8_t suit = si >= 0 ? static_cast<uint8_t>(si) : mp::noSuit;
        mp::Packet p;
        p.type = mp::pose;
        p.role = sc.buddy ? mp::roleBuddy : mp::roleEcho;
        p.seq = 100 + static_cast<uint32_t>(k);
        p.state = mp::fromLocal(l, sc.hero, suit, mp::suitHashOf(suit), mp::swinging);
        const std::vector<uint8_t> bytes = mp::encode(p);
        mp::Packet back;
        const mp::Error e = mp::decode(bytes.data(), bytes.size(), back);
        if (e != mp::Error::none) {
            std::fprintf(stderr, "decode failed: %s\n", mp::errorName(e));
            return 1;
        }
        mp::AnchorFollow follow;
        follow.update(0, .016f);
        const mp::Ghost g = mp::partner(back.state, follow.spot({}), follow.ghostYaw(), !sc.buddy, sc.buddy);
        const sw::Tracked t = mp::toTracked(g, {0, 0, 0}, Quat{});
        const Vec3 viewer{0, 1.65f, 0};
        model.clear();
        soft.clear();
        bool drawnSuit = false;
        const uint32_t slot = mp::slotOfState(back.state);
        if (slot) {
            const mm::Mesh* m = lib.model(slot);
            sw::Rig rig;
            sw::Skeleton live;
            sw::BoneMove mv[sw::bones];
            sw::Skinned sk;
            if (m && sw::makeRig(*m, rig, mm::suitFolders[slot - mm::slotSuitFirst]) && sw::estimate(rig, t, live) && sw::moves(rig, live, mv) &&
                sw::skin(rig, mv, sk)) {
                if (rig.hasCape)
                    sw::cape(rig, mv, live, 0.f, sk);
                drawnSuit = sw::draw(model, hero007::modelSinkBudget, *m, rig, sk, sw::allBones);
            }
        }
        if (!drawnSuit)
            mp::appendFigure(soft, t, viewer, mp::heroColor(back.state.hero), 1.f);
        const std::string plate = mp::nameplate(sc.buddy, sc.name, sc.ping);
        mp::appendNameplate(soft, plate, t.feet + t.up * (1.65f + .35f), t.up, viewer, 1.f);
        save(out + "model_" + std::to_string(k) + ".bin", model);
        save(out + "soft_" + std::to_string(k) + ".bin", soft);
        char line[768];
        std::snprintf(line, sizeof line,
                      "%s\n  {\"title\": \"%s\", \"plate\": \"%s\", \"packetBytes\": %zu, \"suit\": \"%s\", \"suitDrawn\": %s, \"modelVertices\": %zu, "
                      "\"softVertices\": %zu, \"feet\": [%.3f, %.3f, %.3f]}",
                      k ? "," : "", sc.title, plate.c_str(), bytes.size(), sc.suit ? sc.suit : "", drawnSuit ? "true" : "false", model.size(),
                      soft.size(), t.feet.x, t.feet.y, t.feet.z);
        js += line;
    }
    js += "\n ]\n}\n";
    std::ofstream(out + "scenes.json") << js;
    std::printf("%s", js.c_str());
    return misses.empty() ? 0 : 3;
}
