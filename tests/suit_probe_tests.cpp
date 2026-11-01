// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
// 0.0.76 SUIT PROBE's logic (include/spidy/suit_probe.hpp) on a fake address space: the game's asset ids,
// the path rule, the walk (direct and one object deep), a fault anywhere is skipped, and the console lines.
#include "spidy/suit_probe.hpp"

#include <cstdio>
#include <cstring>
#include <functional>
#include <map>

using namespace spidy::suit_probe;
namespace {
int failures{};
void check(bool ok, const char* what) {
    std::printf("%s %s\n", ok ? "  PASS" : "  FAIL", what);
    failures += ok ? 0 : 1;
}
// Pages of 0x1000 bytes; a read touching a missing page fails (as a fault would).
struct Memory {
    std::map<uint64_t, std::vector<uint8_t>> pages;
    uint32_t reads{};
    uint8_t* at(uint64_t p) {
        auto& page = pages[p & ~0xfffull];
        page.resize(0x1000);
        return page.data() + (p & 0xfff);
    }
    void put64(uint64_t p, uint64_t v) { std::memcpy(at(p), &v, 8); }
    void putText(uint64_t p, const char* s) { std::memcpy(at(p), s, std::strlen(s) + 1); }
    bool operator()(uint64_t p, void* out, size_t n) {
        ++reads;
        for (uint64_t q = p & ~0xfffull; q < p + n; q += 0x1000)
            if (!pages.count(q))
                return false;
        for (size_t i = 0; i < n; ++i)
            static_cast<uint8_t*>(out)[i] = pages[(p + i) & ~0xfffull][(p + i) & 0xfff];
        return true;
    }
};
} // namespace

int main() {
    check(assetId("configs/system/system_progression.config") == 0x9C9C72A303FCFA30ull, "the suit list's asset id");
    check(assetId("CONFIGS\\System//system_progression.config") == 0x9C9C72A303FCFA30ull, "normalized first");
    check(assetId("characters/hero/hero_spiderman_advanced/hero_spiderman_advanced_removesymbiote.model") ==
              0x9ED8BE4D2D309AD6ull,
          "a suit model's asset id");

    const char good[] = "characters/hero/hero_spiderman/hero_spiderman.model";
    check(modelPath(good, sizeof good) == std::strlen(good), "a model path");
    check(modelPath("Characters\\Hero\\X.MODEL", 64) == 23, "any case, either slash");
    check(modelPath("hero_spiderman.model", 64) == 0, "no folder: not a path");
    check(modelPath("characters/hero/a.config", 64) == 0, "not a model");
    check(modelPath("characters/hero/a b.model", 64) == 0, "a space: not a path");
    check(modelPath(good, 10) == 0, "no NUL within the read: not a path");

    Memory m;
    const uint64_t record = 0x10000000, instance = 0x20000000, asset = 0x30000000;
    m.put64(record, instance);
    m.put64(record + 0x58, 0x40000010); // a char* straight to a path
    m.putText(0x40000010, "characters/hero/hero_spiderman/hero_spiderman_suit_a.model");
    m.put64(record + 0x60, 0x50000000);     // a pointer to a missing page: skipped
    m.put64(record + 0x68, 0xffff800000000000ull); // a kernel address: never read
    m.put64(instance + 0x120, asset);       // an asset whose +0x10 holds its path
    m.put64(asset + 0x10, 0x40000800);
    m.putText(0x40000800, "Characters\\Hero\\Hero_Spiderman\\Hero_Spiderman_Mask_A.model");
    m.put64(instance + 0x128, 0x40000010);  // the same path again: once
    m.putText(0x40000ff0, "characters/x"); // runs off the page end: no path, no fault
    m.put64(instance + 0x130, 0x40000ff0);
    const uint64_t roots[2] = {record, instance};
    auto hits = scan(roots, std::ref(m));
    check(hits.size() == 2, "two paths, each once");
    check(hits.size() == 2 && hits[0].root == 0 && hits[0].how == direct && hits[0].off1 == 0x58 &&
              hits[0].path == "characters/hero/hero_spiderman/hero_spiderman_suit_a.model",
          "a char* of the record");
    check(hits.size() == 2 && hits[1].root == 1 && hits[1].how == viaObject && hits[1].off1 == 0x120 &&
              hits[1].off2 == 0x10 && hits[1].path == "characters/hero/hero_spiderman/hero_spiderman_mask_a.model" &&
              hits[1].id == assetId(hits[1].path),
          "an asset of the instance, its path normalized and its id");
    check(m.reads < 2000, "a cheap walk");
    const uint64_t none[2] = {0, 0x1234};
    check(scan(none, std::ref(m)).empty(), "no roots, nothing read");

    Watch w;
    auto lines = w.update(false, record, hits);
    check(lines.empty(), "off: no line");
    lines = w.update(true, 0, {});
    check(lines.size() == 2 && lines[1] == "SUIT PROBE: no player yet", "on, no player");
    lines = w.update(true, record, hits);
    check(lines.size() == 3 && lines[0].find("change 1, 2 model path(s)") != std::string::npos &&
              lines[1].find("id 0x") != std::string::npos && lines[1].find("record+0x058") != std::string::npos &&
              lines[2].find("instance+0x120 -> +0x010") != std::string::npos,
          "the set, with ids and places");
    check(w.update(true, record, hits).empty(), "the same set: quiet");
    std::vector<Hit> other{hits[1]};
    lines = w.update(true, record, other);
    check(lines.size() == 2 && lines[0].find("change 2, 1 model") != std::string::npos, "a suit change: logged");
    lines = w.update(true, record, {});
    check(lines.size() == 1 && lines[0].find("CANDIDATE layout missed") != std::string::npos, "nothing found: said");
    lines = w.update(false, record, {});
    check(lines.size() == 1 && lines[0] == "SUIT PROBE: off", "off again");
    std::printf("%s\n", failures ? "FAILED" : "ALL PASSED");
    return failures ? 1 : 0;
}
