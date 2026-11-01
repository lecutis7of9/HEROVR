// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
// 0.0.76 SUIT PROBE's logic (include/spidy/suit_probe.hpp) on a fake address space: the game's asset ids,
// the path rule, the walk (direct and one object deep), a fault anywhere is skipped, and the console lines;
// PROBE 2: the suits of an sm2scan.json, the deep walk (ids, paths, chains, caps, faults) and its lines. Given a
// path (a real sm2scan.json) it also reads that and says how many suits and ids it holds.
#include "spidy/suit_probe.hpp"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <functional>
#include <iterator>
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

int main(int argc, char** argv) {
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

    // ---- PROBE 2 ----
    check(Json::parse("{\"a\": [1, \"x\\\"y\", true, null, {\"b\": -2.5e3}]}").get("a")->items.size() == 5, "json read");
    check(Json::parse("{\"a\": [1, ").type == Json::isNull, "a cut file: nothing, no throw");
    check(Json::parse(std::string(100, '[')).type == Json::isNull, "too deep: nothing");
    check(hexId("BC46538AB3E876D2") == 0xBC46538AB3E876D2ull && hexId("12") == 0 && hexId("BC46538AB3E876DZ") == 0,
          "hex ids");
    const char* scanText = R"({"tool": "sm2scan 1", "suits": [
      {"name": "SUIT_A", "display": "SUIT_A_TITLE", "id": "BC46538AB3E876D2",
       "item": "configs/inventory/inv_reward_loadout_a.config",
       "models": [{"id": "A000000000000001", "path": "characters/hero/hero_a/hero_a.model"},
                  {"id": "A000000000000099", "path": "characters/hero/shared/shared_body.model"}],
       "mask": {"id": "A000000000000002", "path": "characters/prop/mask_a.model"}},
      {"name": "SUIT_B", "id": "BC46538AB3E87600", "item": "configs/inventory/inv_reward_loadout_b.config",
       "models": [{"id": "A000000000000003", "path": "characters/hero/hero_b/hero_b.model"},
                  {"id": "A000000000000099", "path": "characters/hero/shared/shared_body.model"}],
       "mask": null},
      {"name": "SUIT_MOD", "id": "", "item": "configs/inventory/mod.config",
       "models": [{"path": "mods/hero/fortnite daredevil billy clubs.model"}]}]})";
    const Table table = Table::fromScan(scanText);
    check(table.names.size() == 3 && table.ids.size() == 14, "3 suits, 14 ids (each path's own id too, the shared model once)");
    const Target* shared = table.find(0xA000000000000099ull);
    check(shared && shared->suits.size() == 2 && shared->kind == kindModel, "a shared model names both suits");
    check(table.find(0xA000000000000002ull) && table.find(0xA000000000000002ull)->kind == kindMask, "the mask");
    check(table.find(assetId("mods/hero/fortnite daredevil billy clubs.model")) != nullptr,
          "no id: computed from its path (a space kept)");
    check(table.find(assetId("configs/inventory/mod.config")) != nullptr, "the loadout id from its item path");
    check(Table::fromScan("{\"suits\": 3}").empty() && Table::fromScan("not json").empty(), "not a scan: empty");
    check(assetPath("configs/inventory/inv_a.config", 64) == 30, "a .config path");
    check(assetPath("mods/a b.model", 64) == 14, "a space is fine in PROBE 2");
    check(assetPath("mods/a.txt", 64) == 0 && assetPath("a.model", 64) == 0, "not an asset path");

    Memory d;
    const uint64_t rec = 0x11000000, inst = 0x12000000, hero = 0x13000000;
    d.put64(rec, inst);
    d.put64(rec + 0x30, 0x14000000);           // record+0x030 -> a part
    d.put64(0x14000000 + 0x38, 0x15000000);    //   -> +0x038 -> a deeper part
    d.put64(0x15000000 + 0x10, 0x16000000);    //   -> +0x010 -> the suit's part
    d.put64(0x16000000 + 0x08, 0xA000000000000001ull); // holding SUIT_A's model id
    d.put64(hero + 0x100, 0x17000000);         // hero+0x100 -> the loadout config path
    d.putText(0x17000000, "configs/inventory/inv_reward_loadout_a.config");
    d.put64(inst + 0x20, 0x18000000);          // a pointer to a missing page: skipped
    d.put64(inst + 0x28, 0x13000004);          // unaligned: never walked into
    d.put64(inst + 0x30, 0x19000000);          // inside the exe range: matched only
    d.put64(0x19000000, 0xA000000000000003ull);
    d.put64(0x16000000 + 0x40, rec);           // a loop back to the record: visited once
    d.put64(0x16000000 + 0x48, 0xA000000000000099ull); // the shared body
    DeepLimits lim;
    lim.skipLo = 0x19000000;
    lim.skipHi = 0x1a000000;
    Deep deep;
    const uint64_t droots[3] = {rec, inst, hero};
    deep.start(droots);
    int steps = 0;
    while (!deep.step(d, table, lim) && ++steps < 1000) {
    }
    const auto& got = deep.results();
    auto chainOf = [&](uint64_t id) {
        for (const auto& f : got)
            if (f.id == id)
                return f.chain;
        return std::string("-");
    };
    check(got.size() == 3, "three ids found (the exe-range one never walked into)");
    check(chainOf(0xA000000000000001ull) == "record+0x030 -> +0x038 -> +0x010 +0x008", "the deep chain of a model id");
    check(chainOf(assetId("configs/inventory/inv_reward_loadout_a.config")).find("hero +0x100 -> \"configs/") == 0,
          "a loadout path's chain");
    check(chainOf(0xA000000000000003ull) == "-", "the exe range is not walked");
    check(deep.objects() < 20, "each object once");

    Watch2 w2;
    auto l2 = w2.pass(table, got, deep.objects());
    check(l2.size() == 5 && l2[0].find("SUIT PROBE 2: change 1") == 0 && l2[0].find("3 suit id(s)") != std::string::npos,
          "the first pass is logged");
    check(l2.back() == "SUIT PROBE 2: worn suit (CANDIDATE until it follows the swaps): SUIT_A", "the worn suit");
    bool sharedLine = false;
    for (const auto& line : l2)
        sharedLine = sharedLine || line.find("shared by 2 suits model characters/hero/shared/shared_body.model") != std::string::npos;
    check(sharedLine, "a shared model says so");
    check(w2.pass(table, got, deep.objects()).empty(), "the same ids: quiet");
    d.put64(0x16000000 + 0x08, 0xA000000000000003ull); // a swap to SUIT_B's model
    d.put64(hero + 0x100, 0);
    deep.start(droots);
    while (!deep.step(d, table, lim)) {
    }
    l2 = w2.pass(table, deep.results(), deep.objects());
    check(!l2.empty() && l2[0].find("change 2") != std::string::npos && l2.back().find("SUIT_B") != std::string::npos,
          "a swap is logged with the new suit");
    l2 = w2.pass(table, {}, 5);
    check(l2.size() == 1 && l2[0].find("CANDIDATE layout missed") != std::string::npos, "nothing found: said once");

    // Caps: a wide tree stops at `objects`, a step reads at most perStep objects.
    Memory wide;
    for (uint64_t i = 0; i < 0x40; ++i) {
        wide.put64(0x21000000 + i * 8, 0x22000000 + i * 0x1000);
        for (uint64_t j = 0; j < 0x40; ++j)
            wide.put64(0x22000000 + i * 0x1000 + j * 8, 0x30000000 + (i * 0x40 + j) * 0x1000);
    }
    DeepLimits small;
    small.objects = 50;
    small.faultsPerObject = small.faultsPerStep = 1u << 30; // the caps of objects alone first
    Deep capped;
    const uint64_t wroots[3] = {0x21000000, 0, 0};
    capped.start(wroots);
    wide.reads = 0;
    capped.step(wide, table, small);
    check(capped.objects() == small.perStep && wide.reads <= small.perStep * (small.words + 1) * 2 + small.perStep,
          "one step: perStep objects, bounded reads");
    while (!capped.step(wide, table, small)) {
    }
    check(capped.objects() == 50, "a pass stops at its cap");
    // Faults: the grandchildren are unmapped, so each object stops after faultsPerObject, a step after faultsPerStep.
    DeepLimits careful;
    Deep faulty;
    faulty.start(wroots);
    faulty.step(wide, table, careful); // the root: 0x40 good pointers
    wide.reads = 0;
    faulty.step(wide, table, careful);
    check(wide.reads <= careful.faultsPerStep + careful.faultsPerStep / careful.faultsPerObject * 2 + 2,
          "a frame of bad pointers stops early");
    Deep empty;
    const uint64_t nroots[3] = {0, 0x1234, 0xffff800000000000ull};
    empty.start(nroots);
    check(!empty.busy() && !empty.step(wide, table, small), "no roots: nothing read");

    if (argc > 1) { // a real sm2scan.json (never in the repo)
        std::ifstream in(argv[1], std::ios::binary);
        const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        const Table real = Table::fromScan(text);
        std::printf("  %s: %zu suits, %zu ids\n", argv[1], real.names.size(), real.ids.size());
        check(!real.empty(), "the real scan reads");
    }
    std::printf("%s\n", failures ? "FAILED" : "ALL PASSED");
    return failures ? 1 : 0;
}
