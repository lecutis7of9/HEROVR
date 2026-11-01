// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
// 0.0.76 SUIT PROBE's logic (include/spidy/suit_probe.hpp) on a fake address space: the game's asset ids,
// the path rule, the walk (direct and one object deep), a fault anywhere is skipped, and the console lines;
// PROBE 2/3: the suits of an sm2scan.json, the deep walk (ids, paths, chains, lists, loaded models, caps, the
// complete depths, faults), two passes agreeing, the changes only (by place), the watched suits' every place;
// PROBE 4: the container record+0x080 -> +0x028 read directly, the stamps, the cost. Given a
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
    std::setvbuf(stdout, nullptr, _IONBF, 0); // every line out, even if a check aborts
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
       "models": [{"path": "mods/hero/fortnite daredevil billy clubs.model"}]},
      {"name": "SUIT_CLASSIC", "id": "BC46538AB3E8C1A5", "item": "configs/inventory/inv_c.config",
       "models": [{"id": "A000000000000005", "path": "characters/hero/hero_c/hero_c.model"}],
       "mask": {"id": "A000000000000006", "path": "characters/prop/mask_c.model"}}]})";
    const Table table = Table::fromScan(scanText);
    check(table.names.size() == 4 && table.ids.size() == 20, "4 suits, 20 ids (each path's own id too, the shared model once)");
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
    // the unlocked suits: a list of loadout ids (hidden), and what hangs under it is not walked
    d.put64(hero + 0x108, 0x1a100000);
    d.put64(0x1a100000 + 0x00, 0xBC46538AB3E876D2ull);
    d.put64(0x1a100000 + 0x08, 0xBC46538AB3E87600ull);
    d.put64(0x1a100000 + 0x10, 0xBC46538AB3E8C1A5ull);
    d.put64(0x1a100000 + 0x18, assetId("configs/inventory/mod.config"));
    d.put64(0x1a100000 + 0x20, 0x1a200000);    // under the list
    d.put64(0x1a200000 + 0x08, 0xA000000000000006ull); // SUIT_CLASSIC's mask: never reached
    DeepLimits lim;
    lim.skipLo = 0x19000000;
    lim.skipHi = 0x1a000000;
    Deep deep;
    const uint64_t droots[3] = {rec, inst, hero};
    auto run = [&](Deep& dp, Memory& mem, const uint64_t* r, const DeepLimits& l) {
        dp.start(r);
        int guard = 0;
        while (!dp.step(mem, table, l) && ++guard < 100000) {
        }
    };
    run(deep, d, droots, lim);
    auto got = deep.settled();
    auto findOf = [](const std::vector<Deep::Found>& v, uint64_t id) -> const Deep::Found* {
        for (const auto& f : v)
            if (f.id == id)
                return &f;
        return nullptr;
    };
    auto chainOf = [&](uint64_t id) {
        const Deep::Found* f = findOf(got, id);
        return f ? f->chain : std::string("-");
    };
    check(got.size() == 7, "seven ids found (the exe-range one never walked into, nothing under the list)");
    check(chainOf(0xA000000000000001ull) == "record+0x030 -> +0x038 -> +0x010 +0x008", "the deep chain of a model id");
    check(chainOf(assetId("configs/inventory/inv_reward_loadout_a.config")).find("hero +0x100 -> \"configs/") == 0,
          "a loadout path's chain");
    check(chainOf(0xA000000000000003ull) == "-", "the exe range is not walked");
    check(findOf(got, 0xBC46538AB3E876D2ull) && findOf(got, 0xBC46538AB3E876D2ull)->inList &&
              findOf(got, 0xBC46538AB3E87600ull)->inList,
          "loadout ids in a list are marked as the list");
    check(chainOf(0xA000000000000006ull) == "-", "a list is not walked into");
    check(!findOf(got, assetId("configs/inventory/inv_reward_loadout_a.config"))->inList, "a lone loadout path is not a list");
    check(deep.objects() < 20 && deep.complete() == Deep::whole, "each object once, all of it read");
    bool sorted = true;
    for (size_t i = 1; i < got.size(); ++i)
        sorted = sorted && got[i - 1].id < got[i].id;
    check(sorted, "the found set in a fixed order (by id)");

    // A loaded model (an object with a .model path): read wider and walked modelExtra steps past depth.
    Memory lm;
    uint64_t at = 0x41000000;
    const uint64_t lroots[3] = {at, 0, 0};
    for (int i = 0; i < 6; ++i, at += 0x100000) // depth 0..6: a plain chain at +0x018
        lm.put64(at + 0x18, at + 0x100000);
    const uint64_t loaded = at;               // depth 6 (the last plain step): a loaded model
    lm.put64(loaded + 0x00, 0x4f000000);
    lm.putText(0x4f000000, "characters/hero/hero_x/hero_x.model");
    lm.put64(loaded + 0x300, 0xA000000000000005ull);     // SUIT_CLASSIC's model, past the first 0x40 words
    lm.put64(loaded + 0x08, 0x48000000);                  // depth 7
    lm.put64(0x48000000 + 0x10, 0x49000000);              // depth 8
    lm.put64(0x49000000 + 0x2f0, 0xA000000000000006ull); // its mask, deep and wide
    lm.put64(0x48000000 + 0x20, 0x4b000000);              // depth 8, plain (no id of its own)
    lm.put64(0x4b000000 + 0x18, 0x4a000000);              // depth 9: past modelExtra
    lm.put64(0x4a000000, 0xA000000000000001ull);
    Deep ld;
    run(ld, lm, lroots, lim);
    const auto lgot = ld.settled();
    check(findOf(lgot, 0xA000000000000005ull) &&
              findOf(lgot, 0xA000000000000005ull)->chain.find("+0x018 -> +0x018 -> +0x018 -> +0x018 -> +0x018 -> +0x018 +0x300") != std::string::npos,
          "a loaded model is read wider (a model id at +0x300)");
    check(findOf(lgot, 0xA000000000000006ull) != nullptr, "and walked past depth (two steps below it, read wide)");
    check(!findOf(lgot, 0xA000000000000001ull), "but no further than modelExtra");
    Memory plain = lm;
    plain.putText(0x4f000000, "not a path at all");
    plain.put64(loaded + 0x300, 0);
    Deep pd;
    run(pd, plain, lroots, lim);
    check(!findOf(pd.settled(), 0xA000000000000006ull), "a plain object at depth 6 is not walked into");

    // The watched suits: every place their model and mask ids are seen (not just the nearest).
    check(table.watchSuits.size() == 1 && table.names[table.watchSuits[0]] == "SUIT_CLASSIC" && table.watchIds.size() == 4,
          "the watched suit of the scan, its model and mask ids (and their paths' own)");
    check(!table.watched(0xA000000000000001ull) && table.watched(0xA000000000000005ull) &&
              !table.watched(0xBC46538AB3E8C1A5ull),
          "only the watched suits' models and masks (not their loadout)");
    Memory wm;
    const uint64_t wroot = 0x51000000;
    wm.put64(wroot + 0x08, 0x52000000);
    wm.put64(wroot + 0x10, 0x53000000);
    wm.put64(0x52000000 + 0x20, 0xA000000000000005ull);
    wm.put64(0x53000000 + 0x28, 0xA000000000000005ull);
    wm.put64(0x53000000 + 0x30, 0xA000000000000006ull);
    const uint64_t wr3[3] = {wroot, 0, 0};
    Deep wd;
    run(wd, wm, wr3, lim);
    const auto wplaces = wd.watchedPlaces();
    check(wplaces.size() == 3 && wplaces[0].chain == "record+0x008 +0x020" && wplaces[1].chain == "record+0x010 +0x028",
          "every place of a watched id, in read order");
    Watch2 ww;
    check(ww.watch(table, wd).empty(), "one pass: no WATCH line yet");
    auto wl = ww.watch(table, wd);
    check(wl.size() == 4 && wl[0].find("SUIT PROBE 3: WATCH 1 (passes") == 0 &&
              wl[1].find("SUIT_CLASSIC model characters/hero/hero_c/hero_c.model id 0xa000000000000005 at record+0x008 +0x020") !=
                  std::string::npos &&
              wl[3].find("mask") != std::string::npos,
          "two passes agree: every place told");
    check(ww.watch(table, wd).empty(), "the same places: quiet");
    wm.put64(0x52000000 + 0x20, 0);
    wm.put64(0x53000000 + 0x28, 0);
    wm.put64(0x53000000 + 0x30, 0);
    run(wd, wm, wr3, lim);
    check(ww.watch(table, wd).empty(), "a change: quiet until the next pass agrees");
    wl = ww.watch(table, wd);
    check(wl.size() == 2 && wl[1] == "SUIT PROBE 3: WATCH SUIT_CLASSIC: none of its model or mask ids in the memory read",
          "gone: said");
    DeepLimits few = lim;
    few.placesPerId = 1;
    wm.put64(0x52000000 + 0x20, 0xA000000000000005ull);
    wm.put64(0x53000000 + 0x28, 0xA000000000000005ull);
    run(wd, wm, wr3, few);
    check(wd.watchedPlaces().size() == 1, "places a watched id are capped");

    // The lines: two passes agree first, then only the changes; the list's ids counted, never shown.
    Watch2 w2;
    run(deep, d, droots, lim);
    check(w2.pass(table, deep).empty(), "one pass: nothing told yet");
    auto l2 = w2.pass(table, deep);
    check(!l2.empty() && l2[0].find("SUIT PROBE 3: baseline (passes 1 and 2 agree,") == 0 &&
              l2[0].find("3 suit id(s) at 3 place(s); 4 loadout id(s) in lists") != std::string::npos,
          "two passes agree: the baseline");
    check(l2.size() == 5, "the baseline shows the ids outside lists only");
    for (const auto& line : l2)
        if (line.find("inv_reward_loadout_b") != std::string::npos || line.find("0xbc46538ab3e87600") != std::string::npos)
            check(false, "a list id is never shown");
    check(l2.back() == "SUIT PROBE 3: worn suit (CANDIDATE until it follows the swaps): SUIT_A", "the worn suit");
    bool sharedLine = false;
    for (const auto& line : l2)
        sharedLine = sharedLine || line.find("shared by 2 suits model characters/hero/shared/shared_body.model") != std::string::npos;
    check(sharedLine, "a shared model says so");
    check(w2.pass(table, deep).empty(), "the same ids: quiet");
    d.put64(0x16000000 + 0x08, 0xA000000000000005ull); // a swap to SUIT_CLASSIC's model
    d.put64(hero + 0x100, 0);
    d.put64(0x1a100000 + 0x10, 0);                      // and the list loses one (not shown)
    run(deep, d, droots, lim);
    check(w2.pass(table, deep).empty(), "a change: quiet until the next pass agrees");
    run(deep, d, droots, lim);
    l2 = w2.pass(table, deep);
    check(l2.size() == 5 && l2[0].find("SUIT PROBE 3: change 1 (passes 4 and 5 agree,") == 0 &&
              l2[0].find("+1 -2 (lists: +0 -1, not shown)") != std::string::npos,
          "the change: only what was added or removed");
    check(l2[1].find("SUIT PROBE 3: + SUIT_CLASSIC model") == 0 && l2[2].find("SUIT PROBE 3: - SUIT_A") == 0 &&
              l2[2].find(" was at ") != std::string::npos,
          "added and removed lines");
    check(l2[4] == "SUIT PROBE 3: swap (CANDIDATE until it follows the swaps): SUIT_A -> SUIT_CLASSIC", "the swap");
    w2.newPlayer(); // a load: two new passes, still told against the last set
    run(deep, d, droots, lim);
    check(w2.pass(table, deep).empty() && w2.pass(table, deep).empty(), "the same set after a load: quiet");
    // A flicker (one pass different) is never told.
    d.put64(0x16000000 + 0x08, 0xA000000000000001ull);
    run(deep, d, droots, lim);
    check(w2.pass(table, deep).empty(), "one pass different");
    d.put64(0x16000000 + 0x08, 0xA000000000000005ull);
    run(deep, d, droots, lim);
    check(w2.pass(table, deep).empty(), "back again: nothing told");
    // Run 5: the same id at a new place (an entry that moved) is told, by place; no swap (the same suit).
    d.put64(0x16000000 + 0x08, 0);
    d.put64(0x16000000 + 0x18, 0xA000000000000005ull);
    run(deep, d, droots, lim);
    check(w2.pass(table, deep).empty(), "a move: quiet until the next pass agrees");
    run(deep, d, droots, lim);
    l2 = w2.pass(table, deep);
    check(l2.size() == 3 && l2[0].find("places +1 -1 (lists: +0 -0") != std::string::npos &&
              l2[1].find("SUIT PROBE 3: + SUIT_CLASSIC model") == 0 && l2[2].find("SUIT PROBE 3: - SUIT_CLASSIC model") == 0,
          "a move: told by place, no swap line");
    Watch2 w3;
    Deep none3;
    w3.pass(table, none3);
    l2 = w3.pass(table, none3);
    check(l2.size() == 1 && l2[0].find("CANDIDATE layout missed") != std::string::npos, "nothing found: said once");
    check(w3.pass(table, none3).empty(), "and once only");

    // PROBE 4: record+0x080 -> +0x028 read directly; ids in B, in its entries, and one step into an entry.
    {
        Memory c;
        const uint64_t crec = 0x60000000, A = 0x61000000, B = 0x62000000, E1 = 0x63000000, E2 = 0x64000000, P = 0x65000000;
        c.put64(crec + 0x80, A);
        c.put64(A + 0x28, B);
        c.put64(B + 0x008, E1);
        c.put64(B + 0x010, 0xA000000000000002ull); // SUIT_A's mask right in B
        c.put64(B + 0x188, E2);
        c.put64(B + 0x190, 0x66000000);            // an entry on a missing page: skipped
        c.put64(E1 + 0x0e8, P);
        c.put64(P + 0x028, 0xA000000000000005ull); // SUIT_CLASSIC's model one step in (run 5)
        c.put64(E2 + 0x0f0, 0xA000000000000006ull); // its mask in the entry
        c.put64(E2 + 0x128, 0x67000000);            // a pointer to a missing page: skipped
        ContainerLimits cl;
        Container box;
        Watch4 w4;
        auto pass4 = [&]() {
            box.start(c, crec, table, cl);
            while (!box.step(c, table, cl)) {
            }
            return w4.pass(table, box);
        };
        auto p1 = pass4();
        check(p1.size() == 1 && p1[0] == "SUIT PROBE 4: reading record+0x080 -> +0x028 = 0x0000000062000000 about once a second (every place CANDIDATE)",
              "PROBE 4: the container is told once, nothing else after one pass");
        auto p2 = pass4();
        const auto f = box.found();
        check(f.size() == 3 && f[0].chain == "+0x010" && f[1].chain == "+0x008 -> +0x0e8 +0x028" &&
                  f[2].chain == "+0x188 -> +0x0f0",
              "PROBE 4: the three places (in B, one step in, in an entry)");
        check(box.entriesRead() == 2, "PROBE 4: the missing entry is skipped");
        check(p2.size() == 5 && p2[0].find("SUIT PROBE 4: baseline (passes 1 and 2 agree,") == 0 &&
                  p2[0].find("2 entries read): 3 place(s), every place CANDIDATE:") != std::string::npos &&
                  p2[2].find("SUIT PROBE 4: = SUIT_CLASSIC model characters/hero/hero_c/hero_c.model id 0xa000000000000005 at record+0x080 -> +0x028 -> +0x008 -> +0x0e8 +0x028") == 0 &&
                  p2[4] == "SUIT PROBE 4: suits now: SUIT_A (mask), SUIT_CLASSIC (model, mask)",
              "PROBE 4: the baseline, by place, then the suits");
        check(pass4().empty(), "PROBE 4: the same places: quiet");
        c.put64(P + 0x028, 0xA000000000000001ull); // a swap to SUIT_A's model
        check(pass4().empty(), "PROBE 4: a change: quiet until the next pass agrees");
        auto p3 = pass4();
        check(p3.size() == 4 && p3[0].find("SUIT PROBE 4: change 1 (passes 4 and 5 agree,") == 0 &&
                  p3[0].find("): +1 -1, every place CANDIDATE:") != std::string::npos &&
                  p3[1].find("SUIT PROBE 4: + SUIT_A model") == 0 && p3[2].find("SUIT PROBE 4: - SUIT_CLASSIC model") == 0 &&
                  p3[2].find(" was at record+0x080") != std::string::npos &&
                  p3[3] == "SUIT PROBE 4: suits now: SUIT_A (model, mask), SUIT_CLASSIC (mask)",
              "PROBE 4: the change, then the suits");
        c.reads = 0;
        box.start(c, crec, table, cl);
        const uint32_t startReads = c.reads;
        c.reads = 0;
        box.step(c, table, cl);
        check(startReads <= 3 && c.reads <= cl.perStep * (cl.entryWords + 2), "PROBE 4: bounded reads a frame");
        ContainerLimits skip = cl; // the exe is never walked into
        skip.skipLo = E1, skip.skipHi = E1 + 0x1000;
        box.start(c, crec, table, skip);
        while (!box.step(c, table, skip)) {
        }
        check(box.found().size() == 2 && box.entriesRead() == 1, "PROBE 4: never into the exe");
        Memory blank;
        blank.put64(crec, 1);
        Watch4 w5;
        box.start(blank, crec, table, cl);
        auto n1 = w5.pass(table, box);
        box.start(blank, crec, table, cl);
        check(n1.size() == 1 && n1[0].find("SUIT PROBE 4: record+0x080 is not a readable pointer") == 0 &&
                  w5.pass(table, box).empty(),
              "PROBE 4: an unreadable container: said once");
        blank.put64(crec + 0x80, A);
        box.start(blank, crec, table, cl);
        auto n2 = w5.pass(table, box);
        check(n2.size() == 1 && n2[0].find("SUIT PROBE 4: record+0x080 -> +0x028 is not readable") == 0,
              "PROBE 4: a missing list: said");
        box.start(c, 0, table, cl);
        check(box.status() == Container::noHead, "PROBE 4: no record: nothing read");
    }
    check(stamped("SUIT PROBE 4: x", 12345, 16, 57, 16, 123) == "SUIT PROBE 4 [t+12.3 s 16:57:16.123]: x",
          "every probe line stamped");
    check(stamped("hello", 0, 0, 0, 0, 0) == "[t+0.0 s 00:00:00.000] hello", "another line: the stamp in front");
    {
        Cost cost;
        cost.add(10);
        cost.add(30);
        const std::string a = cost.line(1000);
        check(a.find("SUIT PROBE cost: 2 frames, mean 20 us, peak 30 us this stretch; since on 2 frames") == 0 &&
                  a.find("(0.04 ms a second)") != std::string::npos,
              "the cost line");
        cost.add(5);
        const std::string b = cost.line(2000);
        check(b.find("1 frames, mean 5 us, peak 5 us this stretch; since on 3 frames, mean 15 us, peak 30 us") !=
                  std::string::npos,
              "the cost: a stretch, then since on");
    }

    // Caps: a wide tree stops at `objects`, a step reads at most perStep objects; the depth the cap cut is not
    // told, so every pass tells the same memory.
    Memory wide;
    for (uint64_t i = 0; i < 0x40; ++i) {
        wide.put64(0x21000000 + i * 8, 0x22000000 + i * 0x1000);
        for (uint64_t j = 0; j < 0x40; ++j)
            wide.put64(0x22000000 + i * 0x1000 + j * 8, 0x30000000 + (i * 0x40 + j) * 0x1000);
    }
    wide.put64(0x22000000 + 0x3f * 8, 0xBC46538AB3E876D2ull);   // an id at depth 1: told
    wide.put64(0x30000000, 0xA000000000000003ull);               // an id at depth 2 under the cut: not told
    Memory full = wide; // the grandchildren mapped (readable, so walked) for the caps
    for (uint64_t g = 0; g < 0x40 * 0x40; ++g)
        full.put64(0x30000000 + g * 0x1000 + 0x100, 1);
    DeepLimits small;
    small.objects = 100;
    small.faultsPerObject = small.faultsPerStep = 1u << 30; // the caps of objects alone first
    Deep capped;
    const uint64_t wroots[3] = {0x21000000, 0, 0};
    capped.start(wroots);
    full.reads = 0;
    capped.step(full, table, small);
    check(capped.objects() == small.perStep &&
              full.reads <= small.perStep * (small.modelWords * 2 + 2) + small.perStep,
          "one step: perStep objects, bounded reads");
    while (!capped.step(full, table, small)) {
    }
    check(capped.objects() == 100, "a pass stops at its cap");
    check(capped.complete() == 2, "the cap cut depth 2: depths 0 and 1 complete");
    const auto c1 = capped.settled();
    check(findOf(c1, 0xBC46538AB3E876D2ull) && !findOf(c1, 0xA000000000000003ull), "only the complete depths are told");
    Deep again;
    run(again, full, wroots, small);
    const auto c2 = again.settled();
    bool same = c1.size() == c2.size();
    for (size_t i = 0; same && i < c1.size(); ++i)
        same = c1[i].id == c2[i].id && c1[i].chain == c2[i].chain;
    check(same && again.objects() == capped.objects(), "the same memory, the same order, every pass");
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
        std::printf("  watched:");
        for (uint16_t s : real.watchSuits)
            std::printf(" %s", real.names[s].c_str());
        std::printf(" (%zu model/mask ids)\n", real.watchIds.size());
        check(real.watchSuits.size() == 3 && real.watchIds.size() >= 6, "the three watched suits are in it");
    }
    std::printf("%s\n", failures ? "FAILED" : "ALL PASSED");
    return failures ? 1 : 0;
}
