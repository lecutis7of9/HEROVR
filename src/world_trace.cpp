// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
// Update 37: the WORLD LOG's background thread (include/spidy/world_trace.hpp says what and
// why). Read-only: it reads the component registry and RTTI with guarded reads and
// writes only its own record.
#include "spidy/world_trace.hpp"
#include "spidy/cut8.hpp" // HERO VR 0.0.77 CUT8 POI TARGET
#include "spidy/cut10_poi.hpp" // HERO VR 0.0.78 CUT10 GUI2 POI
#include "spidy/cut16.hpp" // HERO VR 0.0.78 cut16 BOSS PICK
#include <atomic>
#include <chrono>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <exception>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
#include <windows.h>
#include "spidy/safe_format.hpp" // 0.0.79.1 t2158: sprintf_s cuts instead of ending the game (last include)
using namespace spidy;
using namespace spidy::world_trace;

extern "C" {
__declspec(dllexport) world_trace::Data SpidyWorldTraceData;
}
namespace {
// CONFIRMED (game_player.cpp, found live): the registry and the component header.
constexpr uintptr_t registryTable = 0xc2d3d20, registryCount = 0xc2d3d40;
// (The header's owner record +0x10 and own handle +0x1c are not read here.)
constexpr unsigned watchedWords = 16, headerBytes = 0x20, sliceEntries = 4096;
constexpr size_t mostActors = 30000, mostWatched = 4096;
constexpr float slowScanMs = 25, failScanMs = 250;

std::atomic<bool> master{true}, kindOn[kinds]{true, true, true}, running{};
// 0.0.76 CRIME GPS FIX: when the GPS last asked for the enemy lists (steady clock ms; 0 never). A call stops
// counting 2 s later, so a frame that no longer asks (out of the car) lets the scan rest by itself.
std::atomic<int64_t> enemiesWantedMs{};
int64_t steadyMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
bool enemiesWanted() {
    const int64_t at = enemiesWantedMs.load(std::memory_order_relaxed);
    return at != 0 && steadyMs() - at < 2000;
}
// HERO VR 0.0.77 PROMPT CARDS: as enemiesWanted, for the prompt objects.
std::atomic<int64_t> promptsWantedMs{};
bool promptsWanted() {
    const int64_t at = promptsWantedMs.load(std::memory_order_relaxed);
    return at != 0 && steadyMs() - at < 2000;
}
std::mutex promptLock;
// cut4 POI DUMP: lines (each under 240 characters) for the console, drained by game_xr.
std::vector<std::string> poiDumpLines;
std::array<story077d::Seen, promptSlots> promptPub{};
std::array<uintptr_t, promptSlots> promptPubRecord{};
uint32_t promptPubCount{};
Vec3 promptHero{};
bool promptHeroKnown{};
uintptr_t promptHeroRecord{};
std::chrono::steady_clock::time_point promptWhen{}, promptMoved{};
PromptStats promptCounts{};
// HERO VR 0.0.77 WAYPOINT (story077g.hpp): the mode asked for, and the mark picked (record re-read at 10 Hz).
std::atomic<uint32_t> waypointMode{story077g::waypointsOff}; // CUT5: off by default
story077g::Mark waypointPub{};
uintptr_t waypointPubRecord{};
std::atomic<float> lookX{}, lookY{}, lookZ{-1};
std::atomic<float> nearestEnemy{1e9f}; // metres at the last scan (BLOOD SPLATTER's plan B)
std::atomic<float> nearestPerson{1e9f}; // Update 37.2: any civilian / pedestrian (NPC NEAR FADE's log)
std::thread worker;
uintptr_t imageBase{};
// Update 38: the enemies near the hero at 10 Hz (the full scan's enemy records, their
// positions re-read every 0.1 s with the same CONFIRMED position read; no new address).
std::mutex enemyLock;
std::array<Vec3, enemySlots> enemyAt{};
std::array<uint64_t, enemySlots> enemyId{}; // the record's address: one enemy's id while he lives
uint32_t enemyCount{};
std::chrono::steady_clock::time_point enemyWhen{};
std::atomic<float> enemyRate{};
Vec3 heroWorldAt{}; // under enemyLock
std::atomic<bool> fastRefresh{true}; // Version 0.0.62 ENEMY TRACKING LIVE
std::atomic<bool> enemyNear{};      // the last refresh found one (LIVE goes fast only then)
std::array<Vec3, enemySlots> enemyWorld{}; // Version 0.0.3: the same enemies' world feet
std::array<Vec3, farSlots> farWorld{}; // 0.0.74 test 2b CRIME GPS: the far ones
uint32_t farCount{};
// Version 0.0.5 DAMAGE PROBE: the same enemies' hit points (enemyLock), read-only.
constexpr uintptr_t botHealthTable = 0x531b0a0, healthHp = 0xd0, healthMost = 0xa0;
std::array<Vital, enemySlots> enemyVital{};
std::array<std::array<char, nameChars>, enemySlots> enemyClassName{}; // 0.0.75 EVERY ENEMY (enemyLock)
Boss bossNow{}; // HERO VR 0.0.77 BOSS DAMAGE: the boss in farReach (cut16: cut16::BossPick; enemyLock; id 0 none)
std::atomic<uint64_t> healthFound{}, heroNowRecord{};
// Version 0.0.3 BOT STATE PROBE (read-only; include/spidy/bot_probe.hpp).
std::atomic<bool> botOn{};
std::atomic<uint64_t> botWant{};
std::mutex botLock;
bot_probe::State botShared;
bool botRan{};

bool read(uintptr_t p, void* out, size_t n) {
    __try {
        if (p < 0x10000)
            return false;
        std::memcpy(out, reinterpret_cast<void*>(p), n);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}
uintptr_t pointer(uintptr_t p) {
    uintptr_t v{};
    read(p, &v, sizeof(v));
    return v;
}
bool typeName(uintptr_t descriptor, char (&out)[nameChars]) {
    for (size_t n = sizeof(out) - 1; n >= 16; n /= 2)
        if (read(descriptor + 0x10, out, n)) {
            out[n] = 0;
            return true;
        }
    return false;
}
struct Class {
    uint32_t bits{};
    char name[nameChars]{};
    int prompt = -1; // HERO VR 0.0.77 PROMPT CARDS: story077d kind of it or a class it derives from
    // HERO VR 0.0.77 CUT8 BOSS HP: 1 it is or derives from .?AVBotHealth@@ (by RTTI, so a class derived from it - a
    // boss's own health - counts too; cut7 matched only BotHealth's exact vtable); healthNamed: some class in its
    // hierarchy has "Health" in its name (the diagnostic logs it when a boss has no BotHealth).
    bool botHealth{}, healthNamed{};
    bool propish{}; // 79.2 PROP PROBE: its own RTTI name has MeleeInteractable, Throwable or Yank in it
};
// A vtable's own class name and the bits of it and every class it derives from (MSVC x64 RTTI).
Class classify(uintptr_t vtable) {
    Class c;
    struct Locator {
        uint32_t signature, offset, constructorOffset, type, hierarchy, self;
    } locator{};
    struct Hierarchy {
        uint32_t signature, attributes, count, bases;
    } hierarchy{};
    const auto at = pointer(vtable - 8);
    if (!read(at, &locator, sizeof(locator)) || locator.signature != 1 || imageBase + locator.self != at ||
        !typeName(imageBase + locator.type, c.name) ||
        !read(imageBase + locator.hierarchy, &hierarchy, sizeof(hierarchy)) || !hierarchy.count ||
        hierarchy.count > 64)
        return c;
    c.propish = std::strstr(c.name, "MeleeInteractable") || std::strstr(c.name, "Throwable") || std::strstr(c.name, "Yank");
    for (uint32_t i = 0; i < hierarchy.count; ++i) {
        uint32_t descriptor{}, type{};
        char name[nameChars]{};
        if (read(imageBase + hierarchy.bases + 4ull * i, &descriptor, 4) && read(imageBase + descriptor, &type, 4) &&
            typeName(imageBase + type, name)) {
            c.bits |= bitsOf(name);
            if (!std::strcmp(name, ".?AVBotHealth@@"))
                c.botHealth = true;
            if (std::strstr(name, "Health"))
                c.healthNamed = true;
            if (c.prompt < 0) // the most derived first
                if ((c.prompt = story077d::kindOf(name)) >= 0)
                    c.bits |= promptBit;
        }
    }
    return c;
}
bool position(uintptr_t record, Vec3& out) {
    float m[16]{};
    if (!read(pointer(record), m, sizeof(m)))
        return false;
    for (float v : m)
        if (!std::isfinite(v) || std::abs(v) > 1e7f)
            return false;
    out = {m[12], m[13], m[14]};
    return true;
}
// The first short text a watched word points at (a candidate line id or name).
void textOf(const uint64_t* words, char (&out)[textChars]) {
    for (unsigned i = 0; i < watchedWords; ++i) {
        char s[textChars]{};
        if (words[i] < 0x10000 || words[i] % 2 || !read(words[i], s, sizeof(s) - 1))
            continue;
        if (const unsigned n = printable(s, textChars - 1)) {
            std::memcpy(out, s, n);
            out[n] = 0;
            return;
        }
    }
}
void copyName(char (&to)[nameChars], const char* from) { strncpy_s(to, from ? from : "", _TRUNCATE); }
// ---------------------------------------------------------------------------------------------------------------
// 79.2 PROP PROBE (t2166 phase 2a): READ-ONLY. Finds Spider-Man 2's throwables (MeleeInteractable, "aka Throwable")
// and the components the game's own web yank / throw use, in the component registry this thread already walks.
// Nothing is written to the game, nothing is hooked, no event is sent. Every read is the SEH-guarded read() above;
// a class is taken only when its vtable is the exact RVA below AND its RTTI name (read live) is the expected one.
// Static evidence (Spider-Man2.exe 2.810.0.0, sha256 c9b912ef..., MSVC RTTI: type descriptor -> complete object
// locator -> vtable; CONFIRMED statically, not yet seen live):
//   MeleeInteractable 0x5357858, MeleeInteractableManager 0x53a7dd0, WebYankToHeroMotion 0x53a88b8,
//   MeleeInteractableShotMotion 0x53a8600, MeleeInteractableShot 0x53a8220, MeleeInteractableYankedInjector 0x53a8838.
// WebYankToHeroMotion::InitFlightParameters (0xb09f00, by its profiler string) reads the owning actor at +0x10 and
// keeps its flight target at +0x310: the snapshot below covers 0x330 bytes of each motion component.
namespace prop79 {
struct Kind {
    uint32_t rva;
    const char* rtti;
    const char* label;
};
constexpr Kind kindsKnown[] = {
    {0x5357858, ".?AVMeleeInteractable@@", "MeleeInteractable"},
    {0x53a7dd0, ".?AVMeleeInteractableManager@@", "MeleeInteractableManager"},
    {0x53a88b8, ".?AVWebYankToHeroMotion@@", "WebYankToHeroMotion"},
    {0x53a8600, ".?AVMeleeInteractableShotMotion@@", "MeleeInteractableShotMotion"},
    {0x53a8220, ".?AVMeleeInteractableShot@@", "MeleeInteractableShot"},
    {0x53a8838, ".?AVMeleeInteractableYankedInjector@@", "MeleeInteractableYankedInjector"},
};
constexpr unsigned kindCount = sizeof(kindsKnown) / sizeof(kindsKnown[0]);
constexpr unsigned interactable = 0, manager = 1, yankMotion = 2, shotMotion = 3, shot = 4, injector = 5;
constexpr unsigned snapWords = 0x330 / 8, mostProps = 512, mostMotions = 64, mostLines = 4000;
struct Item {
    uintptr_t component{}, record{};
    unsigned kind{};
    Vec3 at{};
    bool placed{};
    double movedLogAt = -100;
    std::array<uint64_t, snapWords> words{};
    bool snapped{}, unreadable{};
};
// 79.2b (t2168 lag): off until game_xr turns it on, and game_xr does so only in VR gameplay with prop_probe = on
// (default off). 2429458 had it on from the dll's load, so the registry walk below ran through the boot and splash.
std::atomic<bool> on{false}, bad{};
std::mutex mtx;
std::vector<Item> items;                                    // this scan's prop-side components (under lock)
std::unordered_map<uintptr_t, unsigned> knownBefore;        // component -> kind, the scan before
std::array<bool, kindCount> checked{};                      // RTTI name verified once per kind
std::array<uint32_t, kindCount> counts{};
std::vector<std::string> lines;                             // drained by game_xr
Vec3 aimFrom[2]{}, aimDir[2]{};
bool aimOk[2]{};
uintptr_t aimPick[2]{};
double aimLogAt[2]{-100, -100}, clockNow{}, summaryAt = -100, secondStart{};
uint32_t linesThisSecond{}, linesTotal{}, scans{};
Vec3 hero{};
bool heroOk{};
uint32_t otherNamed{};
std::unordered_map<std::string, uint32_t> otherNames;     // other prop-ish RTTI names this scan (diagnostic)
void line(const char* text) { // under mtx
    if (clockNow - secondStart >= 1) {
        secondStart = clockNow;
        linesThisSecond = 0;
    }
    if (linesTotal >= mostLines || linesThisSecond >= 12)
        return;
    ++linesThisSecond;
    if (++linesTotal == mostLines) {
        lines.emplace_back("79.2 PROP PROBE: line cap reached (4000) - quiet for the rest of the session");
        return;
    }
    lines.emplace_back(text);
}
} // namespace prop79
bool propWanted() { return prop79::on.load(std::memory_order_relaxed) && !prop79::bad.load(std::memory_order_relaxed); }
// One registry entry (the full scan): keep it when its vtable is one of the six, after its RTTI name matched once.
void propObserve(std::vector<prop79::Item>& now, uintptr_t component, uintptr_t record, uintptr_t vtable, const char* name,
                 bool propish) {
    using namespace prop79;
    const uintptr_t rva = vtable - imageBase;
    for (unsigned k = 0; k < kindCount; ++k) {
        if (rva != kindsKnown[k].rva)
            continue;
        if (!checked[k]) {
            if (!name || std::strcmp(name, kindsKnown[k].rtti)) {
                bad = true;
                char t[220];
                sprintf_s(t, "79.2 PROP PROBE: OFF - vtable 0x%llx is '%s', expected %s (not this exe build)",
                          static_cast<unsigned long long>(rva), name ? name : "?", kindsKnown[k].rtti);
                std::lock_guard<std::mutex> g(mtx);
                line(t);
                return;
            }
            checked[k] = true;
        }
        if (now.size() < (k == interactable ? mostProps : mostProps + mostMotions)) {
            Item it;
            it.component = component;
            it.record = record;
            it.kind = k;
            now.push_back(it);
        }
        return;
    }
    if (propish && name && prop79::otherNames.size() < 64)
        ++prop79::otherNames[name];
}
// After the full scan: positions, counts, appeared / gone, a summary line at most every 10 s.
void propFinish(std::vector<prop79::Item>& now, Vec3 heroAt, bool heroKnown, float clockSeconds) {
    using namespace prop79;
    std::array<uint32_t, kindCount> c{};
    std::unordered_map<uintptr_t, unsigned> known;
    for (auto& it : now) {
        ++c[it.kind];
        known[it.component] = it.kind;
        it.placed = position(it.record, it.at) && finite(it.at);
    }
    std::lock_guard<std::mutex> g(mtx);
    clockNow = clockSeconds;
    ++scans;
    hero = heroAt;
    heroOk = heroKnown;
    // Keep the snapshots and move logs of components seen before.
    std::unordered_map<uintptr_t, size_t> old;
    for (size_t i = 0; i < items.size(); ++i)
        old[items[i].component] = i;
    for (auto& it : now)
        if (auto o = old.find(it.component); o != old.end() && items[o->second].record == it.record) {
            it.words = items[o->second].words;
            it.snapped = items[o->second].snapped;
            it.movedLogAt = items[o->second].movedLogAt;
        }
    char t[240];
    if (scans > 1) { // the first scan is the baseline
        for (const auto& it : now)
            if (it.kind != interactable && !knownBefore.count(it.component)) {
                const float m = it.placed && heroKnown ? length(it.at - heroAt) : -1.f;
                sprintf_s(t, "79.2 PROP PROBE: %s APPEARED comp 0x%llx actor 0x%llx at %.1f %.1f %.1f (%.1f m from hero)",
                          kindsKnown[it.kind].label, static_cast<unsigned long long>(it.component),
                          static_cast<unsigned long long>(it.record), it.at.x, it.at.y, it.at.z, m);
                line(t);
            }
        for (const auto& [component, kind] : knownBefore)
            if (kind != interactable && !known.count(component)) {
                sprintf_s(t, "79.2 PROP PROBE: %s GONE comp 0x%llx", kindsKnown[kind].label,
                          static_cast<unsigned long long>(component));
                line(t);
            }
    }
    knownBefore = std::move(known);
    counts = c;
    items = std::move(now);
    if (clockSeconds - summaryAt >= 10) {
        summaryAt = clockSeconds;
        const Item* nearest = nullptr;
        float best = 1e9f;
        uint32_t placed = 0;
        for (const auto& it : items)
            if (it.kind == interactable && it.placed) {
                ++placed;
                if (heroKnown && length(it.at - heroAt) < best) {
                    best = length(it.at - heroAt);
                    nearest = &it;
                }
            }
        sprintf_s(t, "79.2 PROP PROBE scan %u: manager %s (%u), throwables %u (%u placed), yank motions %u, shot motions %u, "
                     "shots %u, yank injectors %u; hero %s %.1f %.1f %.1f",
                  scans, c[manager] ? "FOUND" : "not found", c[manager], c[interactable], placed, c[yankMotion],
                  c[shotMotion], c[shot], c[injector], heroKnown ? "at" : "unknown", heroAt.x, heroAt.y, heroAt.z);
        line(t);
        if (nearest) {
            sprintf_s(t, "79.2 PROP PROBE nearest throwable: actor 0x%llx at %.1f %.1f %.1f, %.1f m from hero",
                      static_cast<unsigned long long>(nearest->record), nearest->at.x, nearest->at.y, nearest->at.z, best);
            line(t);
        }
        if (!otherNames.empty()) {
            std::string s = "79.2 PROP PROBE other prop classes:";
            for (const auto& [n, k] : otherNames) {
                if (s.size() > 200)
                    break;
                s += ' ';
                s += n;
                s += " x" + std::to_string(k);
            }
            line(s.c_str());
        }
    }
    otherNames.clear();
}
// 10 Hz: the hands' nearest throwable, throwables that move (the game's own yank / throw), motion words that change.
void propTick(float clockSeconds) {
    using namespace prop79;
    if (!propWanted())
        return;
    std::lock_guard<std::mutex> g(mtx);
    if (items.empty())
        return;
    const double dt = clockSeconds - clockNow;
    clockNow = clockSeconds;
    char t[240];
    // Positions of the throwables nearest the hero (at most 64 a tick) and of the motion components' actors.
    std::vector<size_t> order;
    for (size_t i = 0; i < items.size(); ++i)
        if (items[i].placed)
            order.push_back(i);
    if (heroOk)
        std::sort(order.begin(), order.end(), [&](size_t a, size_t b) {
            return length(items[a].at - hero) < length(items[b].at - hero);
        });
    if (order.size() > 64)
        order.resize(64);
    for (size_t i : order) {
        auto& it = items[i];
        Vec3 at{};
        if (!position(it.record, at) || !finite(at)) {
            it.placed = false; // 79.2b: never re-read a failed address until the next scan
            continue;
        }
        const float moved = length(at - it.at);
        if (dt > .02 && dt < .5 && moved > .25f && clockSeconds - it.movedLogAt > .5) {
            it.movedLogAt = clockSeconds;
            sprintf_s(t, "79.2 PROP PROBE: %s actor 0x%llx MOVED %.2f m (%.1f m/s) to %.1f %.1f %.1f, %.1f m from hero",
                      kindsKnown[it.kind].label, static_cast<unsigned long long>(it.record), moved,
                      moved / static_cast<float>(dt), at.x, at.y, at.z, heroOk ? length(at - hero) : -1.f);
            line(t);
        }
        it.at = at;
    }
    // Motion / shot / injector components: their first 0x330 bytes, changed words logged (up to 8 a line).
    unsigned watchedNow = 0;
    for (auto& it : items) {
        if (it.kind == interactable || it.kind == manager || it.unreadable || watchedNow >= 16)
            continue;
        if (heroOk && it.placed && length(it.at - hero) > 60)
            continue;
        ++watchedNow;
        std::array<uint64_t, snapWords> w{};
        if (!read(it.component, w.data(), sizeof(w))) {
            it.unreadable = true; // 79.2b: stop snapshotting it until the next scan
            continue;
        }
        if (it.snapped && w != it.words) {
            std::string s;
            char head[120];
            sprintf_s(head, "79.2 PROP PROBE: %s 0x%llx words:", kindsKnown[it.kind].label,
                      static_cast<unsigned long long>(it.component));
            s = head;
            unsigned n = 0;
            for (unsigned k = 1; k < snapWords && n < 8; ++k)
                if (w[k] != it.words[k]) {
                    char piece[64];
                    sprintf_s(piece, " +%x %llx>%llx", k * 8, static_cast<unsigned long long>(it.words[k]),
                              static_cast<unsigned long long>(w[k]));
                    s += piece;
                    ++n;
                }
            if (n)
                line(s.c_str());
        }
        it.words = w;
        it.snapped = true;
    }
    // Each hand's game-world aim ray: the throwable closest to it (within 2.5 m of the ray, 0.5-40 m along).
    for (unsigned h = 0; h < 2; ++h) {
        if (!aimOk[h])
            continue;
        const Item* pick = nullptr;
        float bestOff = 2.5f, along = 0;
        for (size_t i : order) {
            const auto& it = items[i];
            if (it.kind != interactable)
                continue;
            const Vec3 to = it.at - aimFrom[h];
            const float a = dot(to, aimDir[h]);
            if (a < .5f || a > 40)
                continue;
            const float off = length(to - aimDir[h] * a);
            if (off < bestOff) {
                bestOff = off;
                along = a;
                pick = &it;
            }
        }
        const uintptr_t id = pick ? pick->record : 0;
        if (id != aimPick[h] || (id && clockSeconds - aimLogAt[h] > 3)) {
            aimPick[h] = id;
            aimLogAt[h] = clockSeconds;
            if (pick)
                sprintf_s(t, "79.2 PROP PROBE aim %s: throwable actor 0x%llx at %.1f %.1f %.1f, %.1f m along the ray, %.2f m off it",
                          h ? "R" : "L", static_cast<unsigned long long>(id), pick->at.x, pick->at.y, pick->at.z, along, bestOff);
            else
                sprintf_s(t, "79.2 PROP PROBE aim %s: no throwable within 2.5 m of the ray (hand at %.1f %.1f %.1f)",
                          h ? "R" : "L", aimFrom[h].x, aimFrom[h].y, aimFrom[h].z);
            line(t);
        }
    }
}
// ---------------------------------------------------------------------------------------------------------------

struct Watched {
    uintptr_t record{};
    uint32_t cls{};  // the class's bits
    const char* name = ""; // the class's name (the classes map keeps it)
    uint64_t hash{}, words[watchedWords]{};
    bool seen{};
};

void publish(const Data& work) {
    auto& d = SpidyWorldTraceData;
    const uint64_t s = d.sequence;
    reinterpret_cast<std::atomic<uint64_t>&>(d.sequence).store(s + 1, std::memory_order_release);
    std::atomic_thread_fence(std::memory_order_release);
    std::memcpy(static_cast<void*>(&d), &work, offsetof(Data, sequence));
    std::atomic_thread_fence(std::memory_order_release);
    reinterpret_cast<std::atomic<uint64_t>&>(d.sequence).store(s + 2, std::memory_order_release);
}

void run() {
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_LOWEST);
    Data work;
    work.bytes = sizeof(Data);
    std::unordered_map<uintptr_t, Class> classes;
    std::unordered_map<uintptr_t, Watched> watched, nowWatched;     // dialogue and mission components
    std::unordered_map<uintptr_t, uint32_t> actors, nowActors;     // record -> bits (actor-ish)
    std::unordered_map<uintptr_t, const char*> archetype, nowArchetype;
    RateLimit limits[kinds];
    bool baseline = true;
    float interval = 1.f, clock = 0;
    // HERO VR 0.0.77 PROMPT CARDS: the prompt objects (component address -> record, kind) this scan and live.
    struct PromptLive {
        uintptr_t record{};
        uint32_t kind{};
        std::chrono::steady_clock::time_point since{};
        bool before{};
    };
    std::unordered_map<uintptr_t, std::pair<uintptr_t, uint32_t>> nowPrompts;
    std::unordered_map<uintptr_t, PromptLive> promptsLive;
    bool promptFirstScan = true;
    unsigned registryMisses = 0, slowScans = 0;
    // Update 38: the 10 Hz enemy list (see enemyAt).
    std::vector<uintptr_t> enemyRecords;
    uintptr_t heroRecord{};
    std::unordered_map<uintptr_t, uintptr_t> healthOf, nowHealthOf; // Version 0.0.5: record -> BotHealth
    // HERO VR 0.0.77 CUT8 BOSS HP: record -> 1 exact BotHealth / 2 derived; record -> the health class's name (also a
    // "Health"-named class that is not BotHealth: otherHealth, its component, read for the log only).
    std::unordered_map<uintptr_t, uint32_t> healthFrom, nowHealthFrom;
    std::unordered_map<uintptr_t, const char*> healthClass, nowHealthClass;
    std::unordered_map<uintptr_t, uintptr_t> otherHealth, nowOtherHealth;
    std::unordered_map<uintptr_t, std::array<char, nameChars>> enemyNames; // 0.0.75: record -> its enemy class
    auto rateFrom = std::chrono::steady_clock::now();
    uint32_t refreshes{};
    // Version 0.0.3 BOT STATE PROBE: two pointers deep from one enemy's record, read-only.
    auto botHeap = std::make_unique<bot_probe::State>(); // off the stack (the stack-frame gate)
    bot_probe::State& bot = *botHeap;
    std::vector<uintptr_t> botWords(bot_probe::recordBytes / 8), botInner(bot_probe::objectBytes / 8);
    uint32_t botTick{};
    uintptr_t botRecord{}, botLastVt[bot_probe::found]{};
    Vec3 botAt{};
    auto botWhen = std::chrono::steady_clock::now();
    bool botHaveAt{};
    const auto vtableName = [&](uintptr_t object, uintptr_t& vt) -> const char* {
        vt = 0;
        if (object < 0x10000 || object % 8)
            return nullptr;
        vt = pointer(object);
        if (vt < imageBase || vt >= imageBase + 0x10000000ull || vt % 8)
            return nullptr;
        auto c = classes.find(vt);
        if (c == classes.end()) {
            if (classes.size() > 200000)
                return nullptr;
            c = classes.emplace(vt, classify(vt)).first;
        }
        return c->second.name[0] ? c->second.name : nullptr;
    };
    // cut3 (story077g.hpp, CANDIDATE layout): a game POI component's type - the first pointer in its first 64 words
    // to an object whose RTTI name is a POIType*, or one step further through its config / library entry / instance
    // data. nullptr: none found (counted as untyped).
    std::unordered_map<uintptr_t, const char*> poiTypeOf;
    const auto poiTypeName = [&](uintptr_t component) -> const char* {
        const char* holders[4]{};
        uintptr_t holderAt[4]{};
        uint32_t holderCount = 0;
        uint64_t words[64]{};
        if (!read(component, words, sizeof(words)))
            return nullptr;
        for (uint64_t w : words) {
            uintptr_t vt{};
            const char* n = vtableName(static_cast<uintptr_t>(w), vt);
            if (story077g::isPoiType(n))
                return n;
            if (story077g::poiHolder(n) && holderCount < 4) {
                holders[holderCount] = n;
                holderAt[holderCount++] = static_cast<uintptr_t>(w);
            }
        }
        for (uint32_t h = 0; h < holderCount; ++h) {
            uint64_t inner[32]{};
            if (!read(holderAt[h], inner, sizeof(inner)))
                continue;
            for (uint64_t w : inner) {
                uintptr_t vt{};
                const char* n = vtableName(static_cast<uintptr_t>(w), vt);
                if (story077g::isPoiType(n))
                    return n;
            }
        }
        (void)holders;
        return nullptr;
    };
    // cut4 POI DUMP (diagnostic): a POI component's first 64 words - the RTTI class of every object one points to
    // (and, one level in, of the objects that one points to), and small integers - plus where it is and how far.
    // Each component once, at most 40 a session. Lines stay under 240 characters (the console cuts at 256).
    uint32_t poiDumped = 0;
    const auto poiDump = [&](uintptr_t component, const char* kindName, Vec3 at, float metres) {
        if (poiDumped >= 40)
            return;
        ++poiDumped;
        uint64_t words[64]{};
        if (!read(component, words, sizeof(words)))
            return;
        std::vector<std::string> out;
        char head[200];
        sprintf_s(head, "POI DUMP %u %s at %.1f %.1f %.1f %.0f m:", poiDumped, kindName ? kindName : "?", at.x, at.y,
                  at.z, metres);
        std::string line = head;
        const auto add = [&](const char* piece) {
            if (line.size() + std::strlen(piece) > 230) {
                out.push_back(line);
                line = "POI DUMP ";
                line += std::to_string(poiDumped);
                line += " +";
            }
            line += ' ';
            line += piece;
        };
        for (uint32_t i = 1; i < 64; ++i) {
            const uint64_t w = words[i];
            char piece[120];
            uintptr_t vt{};
            const char* n = vtableName(static_cast<uintptr_t>(w), vt);
            if (n) {
                const char* shortName = std::strncmp(n, ".?AV", 4) ? n : n + 4;
                sprintf_s(piece, "%x=%.40s", i * 8, shortName);
                add(piece);
                uint64_t inner[16]{};
                if (read(static_cast<uintptr_t>(w), inner, sizeof(inner)))
                    for (uint32_t j = 1; j < 16; ++j) {
                        uintptr_t vt2{};
                        const char* n2 = vtableName(static_cast<uintptr_t>(inner[j]), vt2);
                        if (!n2)
                            continue;
                        sprintf_s(piece, "%x.%x=%.40s", i * 8, j * 8, std::strncmp(n2, ".?AV", 4) ? n2 : n2 + 4);
                        add(piece);
                    }
            } else if (w && w < 0x10000) {
                sprintf_s(piece, "%x:%llu", i * 8, static_cast<unsigned long long>(w));
                add(piece);
            } else if (w && (w >> 32) == 0) {
                sprintf_s(piece, "%x:%08llx", i * 8, static_cast<unsigned long long>(w));
                add(piece);
            }
        }
        out.push_back(line);
        std::lock_guard<std::mutex> lock(promptLock);
        for (auto& l : out)
            if (poiDumpLines.size() < 400)
                poiDumpLines.push_back(std::move(l));
    };
    // HERO VR 0.0.77 CUT8 POI TARGET (diagnostic, read-only, draws nothing): the GUI2 POI components hold no world
    // place of their own (cut7's POI DUMP: every one "at 0.0 0.0 0.0") but 32-bit words of the registry handle's form
    // (cut8.hpp handleParts: e.g. +98 6b72c101). Each such word that resolves to a live registry entry (generation and
    // the entry's own handle both match) is logged with that entry's class, its record's place and metres from the
    // hero - when it changes, at most 300 lines a session. CANDIDATE source of the game's objective marker only:
    // proven when a line's place / metres match the flat marker (its distance on the HUD) and follow it.
    uintptr_t poiTable{};
    uint32_t poiCount{};
    uint32_t poiTargetLines{};
    std::unordered_map<uintptr_t, uint64_t> poiTargetSig;
    uint32_t anchorLines{};                             // HERO VR 0.0.78 CUT9 ANCHOR
    std::unordered_map<uintptr_t, uint64_t> anchorSig;  // per object: its slot word and offset from the boss
    uint64_t anchorLogMs{};
    uint64_t gui2PoiMs{};                                // HERO VR 0.0.78 CUT10 GUI2 POI
    uint32_t gui2PoiLines{};
    bool gui2PoiChecked{}, gui2PoiOk{};
    std::unordered_map<uint32_t, uint64_t> gui2PoiSig;
    const auto poiTargets = [&](uintptr_t component, const char* kindName, bool heroOk, Vec3 heroWorld) {
        if (!poiTable || !poiCount || poiTargetLines >= 300)
            return;
        uint64_t words[64]{};
        if (!read(component, words, sizeof(words)))
            return;
        std::string line;
        uint64_t sig = 1469598103934665603ull;
        uint32_t hits = 0;
        for (uint32_t i = 1; i < 64 && hits < 6; ++i)
            for (uint32_t half = 0; half < 2 && hits < 6; ++half) {
                const uint32_t h = static_cast<uint32_t>(words[i] >> (32 * half));
                uint32_t index{}, gen{};
                if (!cut8::handleParts(h, poiCount, index, gen))
                    continue;
                struct Entry16 {
                    uintptr_t address;
                    uint32_t generation, pad;
                } e{};
                if (!read(poiTable + index * 16ull, &e, sizeof(e)) || e.generation != gen || e.address < 0x10000 ||
                    e.address % 8)
                    continue;
                struct Head {
                    uintptr_t vtable, pad0, record;
                    uint32_t pad1, handle;
                } hd{};
                if (!read(e.address, &hd, sizeof(hd)) || hd.handle != h)
                    continue;
                ++hits;
                uintptr_t vt{};
                const char* n = vtableName(e.address, vt);
                Vec3 at{};
                const bool placed = hd.record && position(hd.record, at) && finite(at);
                char piece[160];
                if (placed)
                    sprintf_s(piece, " +%x%s %08x=%.36s at %.1f %.1f %.1f %.0f m;", i * 8, half ? "h" : "", h,
                              n ? (std::strncmp(n, ".?AV", 4) ? n : n + 4) : "?", at.x, at.y, at.z,
                              heroOk ? length(at - heroWorld) : -1.f);
                else
                    sprintf_s(piece, " +%x%s %08x=%.36s (no place);", i * 8, half ? "h" : "", h,
                              n ? (std::strncmp(n, ".?AV", 4) ? n : n + 4) : "?");
                line += piece;
                sig = (sig ^ h ^ (static_cast<uint64_t>(i) << 40)) * 1099511628211ull;
                if (placed)
                    sig = (sig ^ static_cast<uint64_t>(static_cast<int64_t>(at.x) * 7 + static_cast<int64_t>(at.z))) *
                          1099511628211ull;
            }
        if (!hits)
            return;
        auto& was = poiTargetSig[component];
        if (was == sig)
            return;
        was = sig;
        ++poiTargetLines;
        char head[160];
        sprintf_s(head, "CUT8 POI TARGET %u %.30s 0x%llx slot %08llx:", poiTargetLines,
                  kindName ? kindName : "?", static_cast<unsigned long long>(component),
                  static_cast<unsigned long long>(words[0x80 / 8] & 0xffffffffull));
        std::string l = head;
        l += line;
        if (l.size() > 236)
            l.resize(236);
        std::lock_guard<std::mutex> lock(promptLock);
        if (poiDumpLines.size() < 400)
            poiDumpLines.push_back(std::move(l));
    };
    const auto botStep = [&](uintptr_t nearest) {
        if (!botOn.load(std::memory_order_relaxed))
            return;
        const uint64_t want = botWant.load(std::memory_order_relaxed);
        const uintptr_t record = want ? static_cast<uintptr_t>(want) : nearest;
        if (!record)
            return;
        if (record != botRecord) { // a new enemy: a new list (the counts go on)
            botRecord = record;
            bot.foundCount = 0;
            bot.snapCount = 0;
            botHaveAt = false;
            std::memset(botLastVt, 0, sizeof(botLastVt));
        }
        bot.watched = record;
        // His measured velocity (world, m/s) between refreshes.
        Vec3 at{}, velocity{};
        const auto now = std::chrono::steady_clock::now();
        if (position(record, at)) {
            const float gap = std::chrono::duration<float>(now - botWhen).count();
            if (botHaveAt && gap > .02f && gap < .6f)
                velocity = (at - botAt) / gap;
            botAt = at;
            botWhen = now;
            botHaveAt = true;
        }
        // A seen Flung state: snapshot it every refresh (10 Hz).
        for (uint32_t i = 0; i < bot.foundCount; ++i) {
            const auto& f = bot.list[i];
            if (!bot_probe::isFlung(f.name))
                continue;
            uintptr_t object = pointer(record + f.first);
            if (f.level == 2)
                object = pointer(object + f.second);
            uintptr_t vt{};
            if (vtableName(object, vt) && vt - imageBase == f.vtableRva) {
                bot_probe::Snap& snap = bot.snap[bot.snapCount % bot_probe::snaps];
                snap = bot_probe::Snap{};
                snap.seconds = clock;
                snap.vx = velocity.x;
                snap.vy = velocity.y;
                snap.vz = velocity.z;
                snap.vtableRva = f.vtableRva;
                if (!read(object, snap.words, sizeof(snap.words)))
                    ++bot.faults;
                ++bot.snapCount;
            }
            break;
        }
        if (++botTick % 5 == 0) { // twice a second: the scan
            ++bot.scans;
            uintptr_t* const words = botWords.data();
            uintptr_t* const inner = botInner.data();
            if (!read(record, words, bot_probe::recordBytes)) {
                ++bot.faults;
            } else {
                uintptr_t nowVt[bot_probe::found]{};
                uint32_t nowCount{}, objects{};
                const auto seen = [&](uint32_t level, uint32_t first, uint32_t second, uintptr_t vt, const char* name) {
                    ++bot.botStates;
                    if (bot_probe::isFlung(name))
                        ++bot.flungSeen;
                    if (bot_probe::isWallStuck(name))
                        ++bot.wallStuckSeen;
                    bot_probe::note(bot, level, first, second, static_cast<uint32_t>(vt - imageBase), name);
                    if (nowCount < bot_probe::found)
                        nowVt[nowCount++] = vt;
                };
                for (uint32_t i = 0; i < bot_probe::recordBytes / 8; ++i) {
                    uintptr_t vt{};
                    const char* name = vtableName(words[i], vt);
                    if (!vt || !name)
                        continue;
                    ++bot.objectsRead;
                    if (bot_probe::isBotState(name))
                        seen(1, i * 8, 0, vt, name);
                    if (objects++ >= bot_probe::firstLevel || !read(words[i], inner, bot_probe::objectBytes))
                        continue;
                    for (uint32_t j = 0; j < bot_probe::objectBytes / 8; ++j) {
                        uintptr_t vt2{};
                        const char* name2 = vtableName(inner[j], vt2);
                        if (name2 && bot_probe::isBotState(name2))
                            seen(2, i * 8, j * 8, vt2, name2);
                    }
                }
                std::sort(nowVt, nowVt + nowCount);
                if (std::memcmp(nowVt, botLastVt, sizeof(nowVt)) != 0) {
                    ++bot.changes;
                    std::memcpy(botLastVt, nowVt, sizeof(nowVt));
                }
            }
        }
        std::lock_guard<std::mutex> lock(botLock);
        botShared = bot;
        botRan = true;
    };
    const auto refreshEnemies = [&] {
        struct Found {
            float first{};
            Vec3 second{};
            uintptr_t record{};
            Vec3 world{};
        };
        std::array<Found, 64> found{};
        uint32_t n{};
        Vec3 heroAt{}, at{};
        // 0.0.74 test 2b CRIME GPS: the far ones too (the same records, farReach)
        struct Far {
            float metres{};
            Vec3 world{};
        };
        std::array<Far, 96> farList{};
        uint32_t farN{};
        Boss boss{};
        // HERO VR 0.0.78 cut16 BOSS PICK: every boss-class record within farReach (up to bossKept), then
        // cut16::BossPick - the one hit last, else the nearest living - not the nearest one alone.
        constexpr uint32_t bossKept = 8;
        std::array<Boss, bossKept> bosses{};
        uint32_t bossN{};
        if (heroRecord && position(heroRecord, heroAt))
            for (const uintptr_t record : enemyRecords) {
                if (n >= found.size() && farN >= farList.size() && bossN >= bossKept)
                    break;
                if (!position(record, at))
                    continue;
                const Vec3 rel = at - heroAt;
                const float metres = length(rel);
                if (!finite(rel))
                    continue;
                // HERO VR 0.0.77 BOSS DAMAGE (CANDIDATE): the boss classes within farReach (cut16: all of them).
                if (metres <= farReach && bossN < bossKept)
                    if (const auto c = enemyNames.find(record); c != enemyNames.end() && isBossClass(c->second.data())) {
                        Boss& boss = bosses[bossN++];
                        boss = {};
                        boss.id = static_cast<uint64_t>(record);
                        boss.world = at;
                        boss.metres = metres;
                        std::memcpy(boss.name, c->second.data(), nameChars);
                        boss.name[nameChars - 1] = 0;
                        Vital v{boss.id};
                        if (const auto h = healthOf.find(record); h != healthOf.end() &&
                            read(h->second + healthHp, &v.hp, 4) && read(h->second + healthMost, &v.most, 4))
                            v.have = std::isfinite(v.hp) && std::isfinite(v.most) && v.most > 0 && v.most < 1e7f;
                        boss.vital = v;
                        // CUT8 BOSS HP diagnostic: where its HP came from (0 none, 1 BotHealth, 2 derived, 3 only a
                        // Health-named class that is not BotHealth - its +d0/+a0 read for the log, never used).
                        if (const auto f = healthFrom.find(record); f != healthFrom.end())
                            boss.healthFrom = f->second;
                        else if (const auto o = otherHealth.find(record); o != otherHealth.end()) {
                            boss.healthFrom = 3;
                            read(o->second + healthHp, &boss.otherHp, 4);
                            read(o->second + healthMost, &boss.otherMost, 4);
                        }
                        if (const auto hc = healthClass.find(record); hc != healthClass.end() && hc->second)
                            strncpy_s(boss.healthClass, hc->second, _TRUNCATE);
                    }
                if (n < found.size() && metres <= enemyReach)
                    found[n++] = {metres, rel, record, at};
                if (farN < farList.size() && metres <= farReach)
                    farList[farN++] = {metres, at};
            }
        { // cut16 BOSS PICK (include/spidy/cut16.hpp)
            static cut16::BossPick bossPick;
            std::array<cut16::BossPick::In, bossKept> in{};
            for (uint32_t i = 0; i < bossN; ++i)
                in[i] = {bosses[i].id, bosses[i].metres, bosses[i].vital.have != 0, bosses[i].vital.hp,
                         bosses[i].vital.most};
            const double bossNowSeconds =
                std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
            if (const int b = bossPick.pick(in.data(), bossN, bossNowSeconds); b >= 0)
                boss = bosses[static_cast<uint32_t>(b)];
        }
        std::sort(found.begin(), found.begin() + n, [](const auto& a, const auto& b) { return a.first < b.first; });
        std::sort(farList.begin(), farList.begin() + farN, [](const auto& a, const auto& b) { return a.metres < b.metres; });
        {
            std::lock_guard<std::mutex> lock(enemyLock);
            enemyCount = std::min<uint32_t>(n, enemySlots);
            for (uint32_t i = 0; i < enemyCount; ++i) {
                enemyAt[i] = found[i].second;
                enemyId[i] = static_cast<uint64_t>(found[i].record);
                enemyWorld[i] = found[i].world;
                // Version 0.0.5 DAMAGE PROBE: its BotHealth's hit points (guarded reads).
                Vital v{static_cast<uint64_t>(found[i].record)};
                if (const auto h = healthOf.find(found[i].record); h != healthOf.end() &&
                    read(h->second + healthHp, &v.hp, 4) && read(h->second + healthMost, &v.most, 4))
                    v.have = std::isfinite(v.hp) && std::isfinite(v.most) && v.most > 0 && v.most < 1e6f;
                enemyVital[i] = v;
                enemyClassName[i] = {};
                if (const auto c = enemyNames.find(found[i].record); c != enemyNames.end())
                    enemyClassName[i] = c->second;
            }
            enemyWhen = std::chrono::steady_clock::now();
            heroWorldAt = heroAt; // Version 0.0.62: where the hero was for this list
            farCount = std::min<uint32_t>(farN, farSlots);
            for (uint32_t i = 0; i < farCount; ++i)
                farWorld[i] = farList[i].world;
            bossNow = boss;
        }
        enemyNear.store(n > 0, std::memory_order_relaxed);
        if (n)
            nearestEnemy.store(found[0].first, std::memory_order_relaxed);
        botStep(n ? found[0].record : 0);
        ++refreshes;
        const auto now = std::chrono::steady_clock::now();
        const float span = std::chrono::duration<float>(now - rateFrom).count();
        if (span >= 2.f) {
            enemyRate.store(static_cast<float>(refreshes) / span, std::memory_order_relaxed);
            refreshes = 0;
            rateFrom = now;
        }
    };
    auto lastTick = std::chrono::steady_clock::now();
    const auto push = [&](uint32_t event, const char* name, const char* text, Vec3 rel, float gaze, uint32_t bits,
                          uint32_t changed, uint32_t count) {
        Entry& e = work.ring[work.next % events];
        e = {};
        e.seconds = clock;
        e.x = rel.x; e.y = rel.y; e.z = rel.z;
        e.distance = finite(rel) ? length(rel) : -1.f;
        e.gaze = gaze;
        e.event = event; e.bits = bits; e.changed = changed; e.count = count;
        copyName(e.name, name);
        strncpy_s(e.text, text ? text : "", _TRUNCATE);
        work.next = (work.next + 1) % events;
        ++work.counts[event < 7 ? event : 0];
    };
    try {
        while (running.load()) {
            const bool any = master.load() && (kindOn[0].load() || kindOn[1].load() || kindOn[2].load());
            for (unsigned k = 0; k < kinds; ++k)
                work.on[k] = master.load() && kindOn[k].load();
            const auto now = std::chrono::steady_clock::now();
            const float dt = std::chrono::duration<float>(now - lastTick).count();
            lastTick = now;
            clock += dt;
            // HERO VR 0.0.77 PROMPT CARDS: no longer asked for: the list goes, the next ask starts afresh.
            const bool promptsOn = promptsWanted();
            if (!promptsOn && promptFirstScan == false) {
                promptsLive.clear();
                poiTypeOf.clear();
                promptFirstScan = true;
                std::lock_guard<std::mutex> lock(promptLock);
                promptPubCount = 0;
                waypointPub = {};
                waypointPubRecord = 0;
            }
            if (!any && !enemiesWanted() && !promptsOn) {
              if (!propWanted()) { // 79.2 PROP PROBE: on, it keeps the scan running
                work.status = 0;
                nearestEnemy.store(1e9f, std::memory_order_relaxed);
                nearestPerson.store(1e9f, std::memory_order_relaxed);
                baseline = true; // turned back on: a fresh baseline, no burst of "spawns"
                publish(work);
                Sleep(250);
                continue;
              }
            }
            const auto start = std::chrono::steady_clock::now();
            uintptr_t table = pointer(imageBase + registryTable);
            int32_t count{};
            if (!table || !read(imageBase + registryCount, &count, 4) || count <= 0 || count > 0x100000) {
                work.status = 3;
                if (++registryMisses > 600) { // ten minutes at 1 s with no registry: say so and stop
                    work.status = 2; work.fault = 2;
                    nearestEnemy.store(1e9f);
                    publish(work);
                    return;
                }
                publish(work);
                Sleep(1000);
                continue;
            }
            registryMisses = 0;
            work.status = 1;
            poiTable = table; // CUT8 POI TARGET
            poiCount = static_cast<uint32_t>(count);
            nowWatched.clear();
            nowPrompts.clear();
            nowHealthOf.clear();
            nowHealthFrom.clear();
            nowHealthClass.clear();
            nowOtherHealth.clear();
            nowActors.clear();
            nowArchetype.clear();
            uintptr_t hero{};
            std::vector<prop79::Item> propNow; // 79.2 PROP PROBE
            const bool propScan = propWanted();
            // 79.2b: a scan only the probe asked for walks gently: a 1 ms sleep every 1024 entries, 2 s apart.
            const bool propOnly = propScan && !any && !enemiesWanted() && !promptsOn;
            std::vector<std::pair<uintptr_t, uintptr_t>> spawnerOf; // record -> spawner vtable
            for (uint32_t i = 0; i < static_cast<uint32_t>(count) && running.load(); ++i) {
                if (i && i % sliceEntries == 0)
                    Sleep(0);
                if (propOnly && i && i % 1024 == 0)
                    Sleep(1);
                struct Entry16 {
                    uintptr_t address;
                    uint32_t generation, pad;
                } entry{};
                if (!read(table + i * 16ull, &entry, sizeof(entry)) || entry.address < 0x10000 || entry.address % 8 ||
                    !entry.generation || entry.generation > 0xfff)
                    continue;
                struct Head {
                    uintptr_t vtable, pad0, record;
                    uint32_t pad1, handle;
                } head{};
                if (!read(entry.address, &head, sizeof(head)) || head.handle != (entry.generation << 20 | i) ||
                    !head.record)
                    continue;
                ++work.entries;
                auto found = classes.find(head.vtable);
                if (found == classes.end()) {
                    found = classes.emplace(head.vtable, classify(head.vtable)).first;
                    work.classes = classes.size();
                }
                // Version 0.0.5 BotHealth by its vtable; HERO VR 0.0.77 CUT8 BOSS HP (CANDIDATE): also any class
                // derived from BotHealth (RTTI). The exact one wins over a derived one on the same record.
                {
                    const bool exact = head.vtable == imageBase + botHealthTable;
                    if ((exact || found->second.botHealth) && nowHealthOf.size() < 4096) {
                        auto& slot = nowHealthFrom[head.record];
                        if (exact || slot != 1) {
                            nowHealthOf[head.record] = entry.address;
                            slot = exact ? 1u : 2u;
                            nowHealthClass[head.record] = found->second.name;
                        }
                    } else if (found->second.healthNamed && nowHealthClass.size() < 8192 &&
                               !nowHealthFrom.count(head.record)) {
                        nowHealthClass[head.record] = found->second.name; // logged only, not read
                        nowOtherHealth[head.record] = entry.address;
                    }
                }
                if (propScan) // 79.2 PROP PROBE: read-only, exact vtable + RTTI name
                    propObserve(propNow, entry.address, head.record, head.vtable,
                                found->second.name[0] ? found->second.name : nullptr, found->second.propish);
                const uint32_t bits = found->second.bits;
                if (!bits)
                    continue;
                if (bits & heroBit)
                    hero = head.record;
                // HERO VR 0.0.77 INTERACT CARDS: the catch-all kinds (any offer, a path waypoint) only up to 2048,
                // so the truck / the pipe / a carry still fit.
                if ((bits & promptBit) && found->second.prompt >= 0 &&
                    nowPrompts.size() < (story077d::catchAllKind(static_cast<uint32_t>(found->second.prompt)) ? 2048u : 6144u))
                    nowPrompts[entry.address] = {head.record, static_cast<uint32_t>(found->second.prompt)};
                if (bits & (civilianBit | enemyBit | allyBit | companionBit | pedestrianBit | botBit) &&
                    nowActors.size() < mostActors) {
                    nowActors[head.record] |= bits;
                    if (bits & (civilianBit | enemyBit | allyBit | companionBit) || !nowArchetype.count(head.record))
                        nowArchetype[head.record] = found->second.name;
                }
                if (bits & spawnerBit)
                    spawnerOf.emplace_back(head.record, head.vtable);
                const bool dialogueOn = work.on[npc] && (bits & dialogueBit);
                const bool missionOn = work.on[mission] && (bits & (missionBit | objectiveBit | activityBit | checkpointBit));
                if ((dialogueOn || missionOn) && nowWatched.size() < mostWatched) {
                    Watched w;
                    w.record = head.record;
                    w.cls = bits;
                    w.name = found->second.name;
                    if (read(entry.address + headerBytes, w.words, sizeof(w.words)))
                        w.hash = hashWords(w.words, watchedWords);
                    nowWatched.emplace(entry.address, w);
                }
            }
            Vec3 heroAt{};
            const bool heroKnown = hero && position(hero, heroAt);
            if (propScan)
                propFinish(propNow, heroAt, heroKnown, clock);
            const Vec3 eye = heroAt + Vec3{0, 1.6f, 0};
            const Vec3 look{lookX.load(), lookY.load(), lookZ.load()};
            const auto relative = [&](uintptr_t record, float& gaze) {
                Vec3 at{};
                gaze = 180.f;
                if (!heroKnown || !position(record, at))
                    return Vec3{std::nanf(""), std::nanf(""), std::nanf("")};
                gaze = gazeDegrees(eye, look, at + Vec3{0, 1.5f, 0});
                return at - heroAt;
            };
            {
                float nearest = 1e9f, person = 1e9f;
                Vec3 at{};
                if (heroKnown)
                    for (const auto& [record, bits] : nowActors) {
                        if (!(bits & (enemyBit | civilianBit | pedestrianBit)) || !position(record, at))
                            continue;
                        const float metres = length(at - heroAt);
                        if (bits & enemyBit)
                            nearest = std::min(nearest, metres);
                        if (bits & (civilianBit | pedestrianBit) && record != hero)
                            person = std::min(person, metres);
                    }
                nearestEnemy.store(nearest, std::memory_order_relaxed);
                nearestPerson.store(person, std::memory_order_relaxed);
                // Update 38: the enemy records the 10 Hz refresh re-reads until the next scan.
                enemyRecords.clear();
                heroRecord = heroKnown ? hero : 0;
                heroNowRecord.store(heroRecord, std::memory_order_relaxed);
                healthOf.swap(nowHealthOf);
                healthFrom.swap(nowHealthFrom);
                healthClass.swap(nowHealthClass);
                otherHealth.swap(nowOtherHealth);
                healthFound.store(healthOf.size(), std::memory_order_relaxed);
                enemyNames.clear();
                // 0.0.76 CRIME GPS FIX: the 64 nearest the hero (guarded position reads; unknown ones last)
                std::vector<std::pair<float, uintptr_t>> byNear;
                for (const auto& [record, bits] : nowActors)
                    if (bits & enemyBit && record != hero) {
                        Vec3 p{};
                        const float d = heroKnown && position(record, p) && finite(p) ? length(p - heroAt) : 1e30f;
                        byNear.emplace_back(std::isfinite(d) ? d : 1e30f, record);
                    }
                const size_t keep = std::min<size_t>(byNear.size(), 64);
                std::partial_sort(byNear.begin(), byNear.begin() + static_cast<std::ptrdiff_t>(keep), byNear.end(),
                                  [](const auto& a, const auto& b) { return a.first < b.first; });
                for (size_t q = 0; q < keep; ++q) {
                    const uintptr_t record = byNear[q].second;
                    {
                        enemyRecords.push_back(record);
                        std::array<char, nameChars> name{};
                        if (const auto a = nowArchetype.find(record); a != nowArchetype.end() && a->second)
                            for (unsigned k = 0; k + 1 < nameChars && a->second[k]; ++k)
                                name[k] = a->second[k];
                        enemyNames.emplace(record, name);
                    }
                }
            }
            refreshEnemies();
            if (!baseline) {
                // SPAWN: new actors (one line each while the rate allows, then one line with the rest).
                if (work.on[spawn]) {
                    uint32_t extra{};
                    for (const auto& [record, bits] : nowActors) {
                        if (actors.count(record))
                            continue;
                        if (!limits[spawn].take(0)) {
                            ++extra;
                            continue;
                        }
                        const char* spawner = "";
                        static char spawnerName[nameChars];
                        spawnerName[0] = 0;
                        for (const auto& [r, vt] : spawnerOf)
                            if (r == record) {
                                copyName(spawnerName, classes[vt].name);
                                spawner = spawnerName;
                            }
                        float gaze = 0;
                        const Vec3 rel = relative(record, gaze);
                        std::string text = std::string(speakerOf(bits)) + (spawner[0] ? std::string(" via ") + spawner : "");
                        push(spawned, nowArchetype[record], text.c_str(), rel, gaze, bits, 0, 1);
                    }
                    uint32_t gone{};
                    for (const auto& [record, bits] : actors)
                        if (!nowActors.count(record))
                            ++gone;
                    if (extra)
                        push(spawned, "(more actors this scan)", "rate-limited", {}, 180.f, 0, 0, extra);
                    if (gone && limits[spawn].take(0))
                        push(despawned, "(actors gone this scan)", "", {}, 180.f, 0, 0, gone);
                }
                // NPC and MISSION: watched components appearing, going and changing.
                for (const auto& [address, w] : nowWatched) {
                    const auto before = watched.find(address);
                    const bool isDialogue = w.cls & dialogueBit;
                    const Kind kind = isDialogue ? npc : mission;
                    const char* name = w.name;
                    char text[textChars]{};
                    if (before == watched.end()) {
                        if (!isDialogue && limits[kind].take(0)) {
                            textOf(w.words, text);
                            float gaze = 0;
                            push(missionStart, name, text, relative(w.record, gaze), gaze, w.cls, 0, 1);
                        }
                        continue;
                    }
                    if (before->second.hash == w.hash || !limits[kind].take(0))
                        continue;
                    uint32_t changed{};
                    for (unsigned i = 0; i < watchedWords; ++i)
                        if (before->second.words[i] != w.words[i])
                            changed |= 1u << i;
                    textOf(w.words, text);
                    float gaze = 0;
                    const Vec3 rel = relative(w.record, gaze);
                    if (isDialogue) {
                        const auto actor = nowActors.find(w.record);
                        const uint32_t actorBits = actor == nowActors.end() ? 0 : actor->second;
                        std::string said = std::string(speakerOf(actorBits)) + (gaze <= gazeCone ? ", looked at" : "") +
                                           (text[0] ? std::string(": ") + text : "");
                        push(dialogue, name, said.c_str(), rel, gaze, actorBits | w.cls, changed, 1);
                    } else {
                        push(missionState, name, text, rel, gaze, w.cls, changed, 1);
                    }
                }
                if (work.on[mission])
                    for (const auto& [address, w] : watched)
                        if (!(w.cls & dialogueBit) && !nowWatched.count(address) && limits[mission].take(0))
                            push(missionStop, w.name, "", {}, 180.f, w.cls, 0, 1);
            }
            baseline = !any; // 0.0.76: a scan for the CRIME GPS alone logs nothing; a log turned on starts fresh
            watched.swap(nowWatched);
            actors.swap(nowActors);
            archetype.swap(nowArchetype);
            work.actors = actors.size();
            for (unsigned k = 0; k < kinds; ++k)
                limits[k].refill(interval), work.dropped[k] = limits[k].dropped;
            // The census of the classes the log watches, with how many are registered.
            std::unordered_map<std::string, std::pair<uint32_t, uint32_t>> seen;
            for (const auto& [address, w] : watched) {
                auto& s = seen[w.name];
                s.first = w.cls; ++s.second;
            }
            work.censusCount = 0;
            for (const auto& [name, s] : seen) {
                if (work.censusCount >= census)
                    break;
                auto& c = work.census_[work.censusCount++];
                copyName(c.name, name.c_str());
                c.bits = s.first; c.live = s.second;
            }
            // HERO VR 0.0.77 PROMPT CARDS (CANDIDATE): which prompt objects came and went, and the list published.
            if (promptsOn) {
                const auto pnow = std::chrono::steady_clock::now();
                PromptStats counts;
                {
                    std::lock_guard<std::mutex> lock(promptLock);
                    counts = promptCounts;
                }
                ++counts.scans;
                for (auto& l : counts.live)
                    l = 0;
                for (const auto& [address, rk] : nowPrompts) {
                    if (rk.second < story077d::kindCount)
                        ++counts.live[rk.second];
                    if (!promptsLive.count(address)) {
                        promptsLive[address] = PromptLive{rk.first, rk.second, pnow, promptFirstScan};
                        if (!promptFirstScan && rk.second < story077d::kindCount)
                            ++counts.appeared[rk.second];
                    }
                }
                for (auto it = promptsLive.begin(); it != promptsLive.end();) {
                    if (!nowPrompts.count(it->first)) {
                        if (it->second.kind < story077d::kindCount)
                            ++counts.gone[it->second.kind];
                        it = promptsLive.erase(it);
                    } else
                        ++it;
                }
                promptFirstScan = false;
                // HERO VR 0.0.77 INTERACT CARDS AND WAYPOINT: every placed object / marker kind's nearest (the
                // log), and the waypoint's candidates (story077g::pick).
                for (uint32_t k = 0; k < story077d::kindCount; ++k) {
                    counts.nearest[k] = 1e9f;
                    counts.nearestAt[k] = {};
                }
                std::vector<story077g::Candidate> marks;
                std::vector<uintptr_t> markRecords;
                uint32_t reads = 0, poiNewReads = 0, poiTargetReads = 0;
                // HERO VR 0.0.78 CUT9 ANCHOR (diagnostic, read-only, draws nothing; the owner on cut8: "it just didnt
                // show the Y on him to zip to him"): which of the game's own placed prompt / POI objects sits on the
                // boss. Every one within 4 m of the boss is logged with its class, its +80 slot word and its offset
                // from him when that changes (at most 240 lines a session), and every 10 s the boss's place with the
                // nearest such object. Proof of the Y's anchor: a line whose object follows him (offset steady while
                // he moves) and whose slot word changes as the Y comes and goes. CANDIDATE until a report shows it.
                Boss anchorBoss{};
                {
                    std::lock_guard<std::mutex> lock(enemyLock);
                    anchorBoss = bossNow;
                }
                const bool anchorOn = anchorBoss.id && finite(anchorBoss.world);
                float anchorNearest = 1e9f;
                const char* anchorNearestCls = nullptr;
                uint32_t anchorWithin = 0;
                counts.poiComponents = counts.poiTyped = counts.poiUntyped = counts.poiTypeCount = 0;
                for (auto& c : counts.objectiveCandidates)
                    c = 0;
                for (const auto& [address, l] : promptsLive) {
                    const story077d::Kind* k = story077d::kind(l.kind);
                    if (!k || k->where == story077d::screen || !l.record || ++reads > 4096)
                        continue;
                    Vec3 at{};
                    if (!position(l.record, at) || !finite(at))
                        continue;
                    if (anchorOn) { // his own record's components too (a POI component on him has his record)
                        const Vec3 off = at - anchorBoss.world;
                        const float d = length(off);
                        if (std::isfinite(d) && d < anchorNearest) {
                            anchorNearest = d;
                            anchorNearestCls = k->cls;
                        }
                        if (std::isfinite(d) && d <= 4.f) {
                            ++anchorWithin;
                            uint32_t slot{};
                            read(address + 0x80, &slot, 4);
                            const uint64_t sig = (static_cast<uint64_t>(slot) << 32) ^
                                                 static_cast<uint64_t>(static_cast<int64_t>(std::lround(off.x)) * 73856093 ^
                                                                       std::lround(off.y) * 19349663 ^ std::lround(off.z) * 83492791) ^
                                                 anchorBoss.id;
                            auto& was = anchorSig[address];
                            if (was != sig && anchorLines < 240) {
                                was = sig;
                                ++anchorLines;
                                char line[236];
                                sprintf_s(line, "CUT9 ANCHOR %u %.32s 0x%llx slot %08x on the boss %.28s%s: offset %.1f %.1f %.1f "
                                                "(%.1f m); boss at %.1f %.1f %.1f %.0f m",
                                          anchorLines, k->cls, static_cast<unsigned long long>(address), slot,
                                          anchorBoss.name[0] ? anchorBoss.name : "?",
                                          l.record == anchorBoss.id ? " (his record)" : "", off.x, off.y, off.z, d,
                                          anchorBoss.world.x, anchorBoss.world.y, anchorBoss.world.z,
                                          heroKnown ? length(anchorBoss.world - heroAt) : -1.f);
                                std::lock_guard<std::mutex> lock(promptLock);
                                if (poiDumpLines.size() < 400)
                                    poiDumpLines.push_back(line);
                            }
                        }
                    }
                    if (heroKnown) {
                        const float m = length(at - heroAt);
                        if (std::isfinite(m) && m < counts.nearest[l.kind]) {
                            counts.nearest[l.kind] = m;
                            counts.nearestAt[l.kind] = at;
                        }
                    }
                    // cut3 (story077g.hpp): a game POI component - its type (by RTTI) says whether it is the
                    // objective. Read once per component (cached), at most 64 new ones a scan.
                    if (story077g::poiComponentKind(l.kind)) {
                        ++counts.poiComponents;
                        auto t = poiTypeOf.find(address);
                        if (t == poiTypeOf.end() && poiNewReads < 64) {
                            ++poiNewReads;
                            t = poiTypeOf.emplace(address, poiTypeName(address)).first;
                            poiDump(address, k->cls, at, heroKnown ? length(at - heroAt) : -1.f);
                        }
                        if (poiTargetReads++ < 48) // CUT8 POI TARGET (diagnostic): its handle words, logged on change
                            poiTargets(address, k->cls, heroKnown, heroAt);
                        if (t == poiTypeOf.end())
                            continue;
                        const char* tn = t->second;
                        if (!tn) {
                            ++counts.poiUntyped;
                            continue;
                        }
                        ++counts.poiTyped;
                        const float m = heroKnown ? length(at - heroAt) : -1.f;
                        uint32_t slot = 0;
                        while (slot < counts.poiTypeCount && std::strcmp(counts.poiTypes[slot].name, tn))
                            ++slot;
                        if (slot == counts.poiTypeCount && slot < PromptStats::poiTypeSlots) {
                            strncpy_s(counts.poiTypes[slot].name, tn, _TRUNCATE);
                            counts.poiTypes[slot].live = 0;
                            counts.poiTypes[slot].nearest = 1e9f;
                            ++counts.poiTypeCount;
                        }
                        if (slot < counts.poiTypeCount) {
                            ++counts.poiTypes[slot].live;
                            if (m >= 0 && m < counts.poiTypes[slot].nearest)
                                counts.poiTypes[slot].nearest = m;
                        }
                        const uint32_t src = story077g::sourceOfType(tn);
                        if (src != story077g::none && marks.size() < 512) {
                            marks.push_back({src, static_cast<uint64_t>(address), at});
                            markRecords.push_back(l.record);
                            ++counts.objectiveCandidates[src];
                        }
                    }
                }
                for (auto it = poiTypeOf.begin(); it != poiTypeOf.end();) // forget the gone ones
                    it = promptsLive.count(it->first) ? std::next(it) : poiTypeOf.erase(it);
                for (auto it = poiTargetSig.begin(); it != poiTargetSig.end();) // CUT8 POI TARGET: the gone ones
                    it = promptsLive.count(it->first) ? std::next(it) : poiTargetSig.erase(it);
                for (auto it = anchorSig.begin(); it != anchorSig.end();) // CUT9 ANCHOR: the gone ones
                    it = promptsLive.count(it->first) ? std::next(it) : anchorSig.erase(it);
                if (anchorOn && anchorLines < 240) { // CUT9 ANCHOR: the boss's place every 10 s
                    const uint64_t ms = GetTickCount64();
                    if (!anchorLogMs || ms - anchorLogMs >= 10000) {
                        anchorLogMs = ms;
                        ++anchorLines;
                        char line[236];
                        sprintf_s(line, "CUT9 ANCHOR BOSS %.28s at %.1f %.1f %.1f %.0f m: %u game prompt / POI objects within 4 m; "
                                        "nearest %.32s %.1f m",
                                  anchorBoss.name[0] ? anchorBoss.name : "?", anchorBoss.world.x, anchorBoss.world.y,
                                  anchorBoss.world.z, heroKnown ? length(anchorBoss.world - heroAt) : -1.f, anchorWithin,
                                  anchorNearestCls ? anchorNearestCls : "none", anchorNearestCls ? anchorNearest : -1.f);
                        std::lock_guard<std::mutex> lock(promptLock);
                        if (poiDumpLines.size() < 400)
                            poiDumpLines.push_back(line);
                    }
                }
                // HERO VR 0.0.78 CUT10 GUI2 POI (diagnostic, read-only, draws nothing): the game's own POI manager
                // (include/spidy/cut10_poi.hpp), every 10 s, only when the POIView and manager tables match exactly.
                // Logs the icon-record count, each live icon record's words (on change), and any world place found in
                // the 0x370 records, so a report shows which record is the Y zip prompt / waypoint / beacon.
                if (gui2PoiLines < cut10::maxLines) {
                    const uint64_t ms = GetTickCount64();
                    if (!gui2PoiMs || ms - gui2PoiMs >= 10000) {
                        gui2PoiMs = ms;
                        const uintptr_t view = imageBase + cut10::poiViewRva, manager = view + cut10::managerOffset;
                        std::vector<std::string> out;
                        if (!gui2PoiChecked) {
                            gui2PoiChecked = true;
                            gui2PoiOk = cut10::layoutMatches(imageBase, pointer(view), pointer(manager));
                            out.push_back(gui2PoiOk ? "CUT10 GUI2 POI the game's POIView and POI manager found (2.810.0.0 layout)"
                                                    : "CUT10 GUI2 POI not this build's layout: nothing read");
                        }
                        if (gui2PoiOk && cut10::layoutMatches(imageBase, pointer(view), pointer(manager))) {
                            int32_t icons = -1, records = -1;
                            read(manager + cut10::iconCount, &icons, 4);
                            read(manager + cut10::recordCount, &records, 4);
                            const uintptr_t iconAt = pointer(manager + cut10::iconArray);
                            const uintptr_t recordAt = pointer(manager + cut10::recordArray);
                            char line[236];
                            sprintf_s(line, "CUT10 GUI2 POI manager 0x%llx: %d icon records at 0x%llx, %d records at 0x%llx; hero %s",
                                      static_cast<unsigned long long>(manager), icons, static_cast<unsigned long long>(iconAt),
                                      records, static_cast<unsigned long long>(recordAt), heroKnown ? "known" : "unknown");
                            out.push_back(line);
                            for (int32_t i = 0; cut10::countSane(icons) && iconAt && i < icons &&
                                                i < static_cast<int32_t>(cut10::maxRecords); ++i) {
                                uint32_t w[12]{};
                                if (!read(iconAt + static_cast<uintptr_t>(i) * cut10::iconStride, w, sizeof w))
                                    break;
                                const uint64_t sig = cut10::signature(w, 12);
                                auto& was = gui2PoiSig[static_cast<uint32_t>(i)];
                                if (was == sig)
                                    continue;
                                was = sig;
                                float f[2]{};
                                std::memcpy(f, &w[8], sizeof f);
                                sprintf_s(line, "CUT10 GUI2 POI icon %d: %08x %08x %08x %08x %08x %08x atlas %08x %08x f %.3f %.3f "
                                                "%08x %08x", i, w[0], w[1], w[2], w[3], w[4], w[5], w[6], w[7], f[0], f[1], w[10], w[11]);
                                out.push_back(line);
                            }
                            for (int32_t i = 0; heroKnown && cut10::countSane(records) && recordAt && i < records &&
                                                i < static_cast<int32_t>(cut10::maxRecords); ++i) {
                                float r[cut10::recordStride / 4]{};
                                if (!read(recordAt + static_cast<uintptr_t>(i) * cut10::recordStride, r, sizeof r))
                                    break;
                                for (uint32_t o = 0; o + 3 <= cut10::recordStride / 4; ++o)
                                    if (cut10::worldTriple(r + o, heroAt.x, heroAt.y, heroAt.z)) {
                                        const uint64_t sig = cut10::signature(reinterpret_cast<const uint32_t*>(r + o), 3) ^ o;
                                        auto& was = gui2PoiSig[0x10000u + static_cast<uint32_t>(i) * 0x100u + o];
                                        if (was != sig) {
                                            was = sig;
                                            sprintf_s(line, "CUT10 GUI2 POI record %d +0x%x world %.1f %.1f %.1f (%.0f m)", i, o * 4,
                                                      r[o], r[o + 1], r[o + 2],
                                                      length(Vec3{r[o], r[o + 1], r[o + 2]} - heroAt));
                                            out.push_back(line);
                                        }
                                        o += 2;
                                    }
                            }
                        }
                        std::lock_guard<std::mutex> lock(promptLock);
                        for (auto& l : out)
                            if (gui2PoiLines < cut10::maxLines && poiDumpLines.size() < 600) {
                                ++gui2PoiLines;
                                poiDumpLines.push_back(std::move(l));
                            }
                    }
                }
                const story077g::Mark mark = story077g::pick(marks.data(), static_cast<uint32_t>(marks.size()), heroAt,
                                                             heroKnown, waypointMode.load(std::memory_order_relaxed));
                uintptr_t markRecord{};
                for (size_t q = 0; q < marks.size() && mark.shown; ++q)
                    if (marks[q].id == mark.id)
                        markRecord = markRecords[q];
                ++counts.waypointPicks[mark.shown ? mark.source % story077g::sources : 0];
                {
                    std::lock_guard<std::mutex> lock(promptLock);
                    waypointPub = mark;
                    waypointPubRecord = markRecord;
                }
                // The newest new screen-kind ones, then the nearest object-kind ones.
                std::vector<std::pair<float, std::pair<story077d::Seen, uintptr_t>>> pick;
                for (const auto& [address, l] : promptsLive) {
                    const story077d::Kind* k = story077d::kind(l.kind);
                    if (!k || k->where == story077d::watchOnly || k->where == story077d::marker)
                        continue;
                    story077d::Seen seenNow;
                    seenNow.id = static_cast<uint64_t>(address);
                    seenNow.kind = l.kind;
                    seenNow.before = l.before;
                    seenNow.age = std::chrono::duration<float>(pnow - l.since).count();
                    Vec3 at{};
                    seenNow.placed = l.record && position(l.record, at) && finite(at);
                    seenNow.world = seenNow.placed ? at : Vec3{};
                    float score;
                    if (k->where == story077d::screen) {
                        if (l.before || seenNow.age > 15.f)
                            continue;
                        score = seenNow.age - 1000.f; // ahead of every object
                    } else if (k->where == story077d::cine) {
                        // CUT5: a scene's Y offer, old or new, within its reach (+2 m), ahead of the objects.
                        if (!seenNow.placed || !heroKnown)
                            continue;
                        score = length(at - heroAt);
                        if (!std::isfinite(score) || score > k->reach + 2.f)
                            continue;
                        score -= 500.f;
                    } else {
                        if (!seenNow.placed || !heroKnown)
                            continue;
                        score = length(at - heroAt);
                        // HERO VR 0.0.78 cut15 BOSS Y CARD (CANDIDATE): a web strike object on the boss (his record,
                        // or within bossAnchorMetres of his place) is kept out to bossCardMetres, ahead of the rest.
                        if (anchorOn && story077d::webStrikeKind(l.kind) &&
                            (l.record == anchorBoss.id ||
                             length(at - anchorBoss.world) <= story077d::bossAnchorMetres)) {
                            if (!std::isfinite(score) || score > story077d::bossCardMetres)
                                continue;
                            seenNow.onBoss = true;
                            pick.push_back({score - 400.f, {seenNow, l.record}});
                            continue;
                        }
                        if (!std::isfinite(score) || score > 60.f)
                            continue;
                        // HERO VR 0.0.77 INTERACT CARDS: an interactable only within its reach (+2 m), the catch-all
                        // offers after the named ones (so the truck / the pipe always get a slot).
                        if (story077d::interactKind(l.kind) && score > k->reach + 2.f)
                            continue;
                        if (story077d::catchAllKind(l.kind))
                            score += 100.f;
                    }
                    pick.push_back({score, {seenNow, l.record}});
                }
                const size_t keep = std::min<size_t>(pick.size(), promptSlots);
                std::partial_sort(pick.begin(), pick.begin() + static_cast<std::ptrdiff_t>(keep), pick.end(),
                                  [](const auto& a, const auto& b) { return a.first < b.first; });
                std::lock_guard<std::mutex> lock(promptLock);
                promptPubCount = static_cast<uint32_t>(keep);
                for (size_t q = 0; q < keep; ++q) {
                    promptPub[q] = pick[q].second.first;
                    promptPubRecord[q] = pick[q].second.second;
                }
                promptHero = heroAt;
                promptHeroKnown = heroKnown;
                promptHeroRecord = heroKnown ? hero : 0;
                promptWhen = promptMoved = pnow;
                promptCounts = counts;
            }
            ++work.scans;
            const float ms = std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - start).count();
            work.lastScanMs = ms;
            work.meanScanMs += (ms - work.meanScanMs) / static_cast<float>(std::min<uint64_t>(work.scans, 64));
            work.peakScanMs = std::max(work.peakScanMs, ms);
            if (ms > slowScanMs)
                interval = std::min(interval * 2, 8.f);
            slowScans = ms > failScanMs ? slowScans + 1 : 0;
            if (slowScans >= 3) {
                work.status = 2; work.fault = 3;
                    nearestEnemy.store(1e9f);
                publish(work);
                return;
            }
            work.intervalS = interval;
            publish(work);
            // Version 0.0.62 ENEMY TRACKING LIVE: 30 a second while an enemy is near (the same
            // guarded reads of the same records, 3x as often, on this thread - not the game's or the
            // frame's); 10 a second with none near and on 0.0.6.
            const float rest = propOnly ? std::max(interval, 2.f) : interval; // 79.2b: probe-only scans 2 s apart
            for (float slept = 0; slept < rest && running.load();) {
                const bool fast = fastRefresh.load(std::memory_order_relaxed) && enemyNear.load(std::memory_order_relaxed);
                Sleep(fast ? 33 : 100);
                slept += fast ? .033f : .1f;
                refreshEnemies(); // Update 38: 10 Hz
                propTick(clock + slept); // 79.2 PROP PROBE: 10 Hz, read-only
                if (promptsOn) { // HERO VR 0.0.77 PROMPT CARDS: the published ones' positions, the hero's
                    std::lock_guard<std::mutex> lock(promptLock);
                    for (uint32_t q = 0; q < promptPubCount; ++q) {
                        Vec3 at{};
                        if (promptPubRecord[q] && position(promptPubRecord[q], at) && finite(at)) {
                            promptPub[q].world = at;
                            promptPub[q].placed = true;
                        }
                    }
                    Vec3 h{};
                    if (promptHeroRecord && position(promptHeroRecord, h) && finite(h))
                        promptHero = h;
                    if (waypointPub.shown && waypointPubRecord) { // HERO VR 0.0.77 WAYPOINT
                        Vec3 w{};
                        if (position(waypointPubRecord, w) && finite(w)) {
                            waypointPub.world = w;
                            if (promptHeroKnown)
                                waypointPub.metres = length(w - promptHero);
                        }
                    }
                    promptMoved = std::chrono::steady_clock::now();
                }
            }
        }
    } catch (...) {
        work.status = 2; work.fault = 1;
                    nearestEnemy.store(1e9f); // never past this thread: the log stops, the game goes on
        publish(work);
    }
}
} // namespace

namespace spidy::world_trace {
void start(uintptr_t base) {
    if (running.exchange(true))
        return;
    imageBase = base;
    try {
        worker = std::thread(run);
    } catch (...) {
        running = false;
    }
}
void stop() {
    running = false;
    if (worker.joinable())
        worker.join();
}
void look(Vec3 forward) {
    if (!finite(forward))
        return;
    lookX.store(forward.x, std::memory_order_relaxed);
    lookY.store(forward.y, std::memory_order_relaxed);
    lookZ.store(forward.z, std::memory_order_relaxed);
}
void setOn(bool on) { master = on; }
void setPromptsWanted(bool on) { promptsWantedMs.store(on ? steadyMs() : 0, std::memory_order_relaxed); }
uint32_t prompts(std::array<story077d::Seen, promptSlots>& out, Vec3& heroWorld, bool& heroKnownOut, float& ageSeconds) {
    ageSeconds = 1e9f;
    heroKnownOut = false;
    if (!running.load())
        return 0;
    std::lock_guard<std::mutex> lock(promptLock);
    ageSeconds = std::chrono::duration<float>(std::chrono::steady_clock::now() - promptMoved).count();
    if (ageSeconds > 1.5f || std::chrono::steady_clock::now() - promptWhen > std::chrono::seconds(4))
        return 0;
    heroWorld = promptHero;
    heroKnownOut = promptHeroKnown;
    const float sinceScan = std::chrono::duration<float>(std::chrono::steady_clock::now() - promptWhen).count();
    for (uint32_t i = 0; i < promptPubCount; ++i) {
        out[i] = promptPub[i];
        out[i].age += sinceScan;
    }
    return promptPubCount;
}
void setWaypointMode(uint32_t mode) { waypointMode.store(mode, std::memory_order_relaxed); }
bool waypoint(story077g::Mark& out) {
    out = {};
    if (!running.load())
        return false;
    std::lock_guard<std::mutex> lock(promptLock);
    const auto now = std::chrono::steady_clock::now();
    if (now - promptMoved > std::chrono::milliseconds(1500) || now - promptWhen > std::chrono::seconds(4) ||
        !waypointPub.shown)
        return false;
    out = waypointPub;
    return true;
}
uint32_t drainPoiDump(std::vector<std::string>& out) {
    std::lock_guard<std::mutex> lock(promptLock);
    const uint32_t n = static_cast<uint32_t>(poiDumpLines.size());
    for (auto& l : poiDumpLines)
        out.push_back(std::move(l));
    poiDumpLines.clear();
    return n;
}
PromptStats promptStats() {
    std::lock_guard<std::mutex> lock(promptLock);
    return promptCounts;
}
// 79.2 PROP PROBE (read-only): on/off (live.txt prop_probe), the hands' game-world aim rays, the lines to log.
void setPropProbe(bool on) { prop79::on.store(on, std::memory_order_relaxed); }
bool propProbeOn() { return propWanted(); }
void propAims(const Vec3 from[2], const Vec3 dir[2], const bool ok[2]) {
    std::unique_lock<std::mutex> g(prop79::mtx, std::try_to_lock); // never waits on the probe's 10 Hz tick
    if (!g.owns_lock())
        return;
    for (unsigned h = 0; h < 2; ++h) {
        const Vec3 d = finite(dir[h]) ? normalized(dir[h]) : Vec3{};
        prop79::aimOk[h] = ok[h] && finite(from[h]) && length(d) > .5f;
        prop79::aimFrom[h] = from[h];
        prop79::aimDir[h] = d;
    }
}
uint32_t drainPropProbe(std::vector<std::string>& out) {
    std::unique_lock<std::mutex> g(prop79::mtx, std::try_to_lock); // never waits on the probe's 10 Hz tick
    if (!g.owns_lock())
        return {};
    const auto n = static_cast<uint32_t>(prop79::lines.size());
    for (auto& l : prop79::lines)
        out.push_back(std::move(l));
    prop79::lines.clear();
    return n;
}
void setEnemiesWanted(bool on) { enemiesWantedMs.store(on ? steadyMs() : 0, std::memory_order_relaxed); }
float nearestEnemyMetres() { return running.load() ? nearestEnemy.load(std::memory_order_relaxed) : 1e9f; }
bool isOn() { return master.load(); }
float nearestPersonMetres() { return running.load() ? nearestPerson.load(std::memory_order_relaxed) : 1e9f; }
uint32_t enemies(std::array<Vec3, enemySlots>& out, std::array<uint64_t, enemySlots>* ids) {
    if (!running.load())
        return 0;
    std::lock_guard<std::mutex> lock(enemyLock);
    if (std::chrono::steady_clock::now() - enemyWhen > std::chrono::milliseconds(500))
        return 0; // stale: none (the log off, a slow scan, a fault)
    for (uint32_t i = 0; i < enemyCount; ++i) {
        out[i] = enemyAt[i];
        if (ids)
            (*ids)[i] = enemyId[i];
    }
    return enemyCount;
}
bool heroAtRefresh(Vec3& out) {
    if (!running.load())
        return false;
    std::lock_guard<std::mutex> lock(enemyLock);
    out = heroWorldAt;
    return enemyCount > 0 && finite(heroWorldAt);
}
void setFastRefresh(bool on) { fastRefresh.store(on, std::memory_order_relaxed); }
float enemyScanHz() { return running.load() ? enemyRate.load(std::memory_order_relaxed) : 0.f; }
uint32_t enemiesWorld(std::array<Vec3, enemySlots>& out, std::array<uint64_t, enemySlots>& ids, float& ageSeconds) {
    ageSeconds = 1e9f;
    if (!running.load())
        return 0;
    std::lock_guard<std::mutex> lock(enemyLock);
    ageSeconds = std::chrono::duration<float>(std::chrono::steady_clock::now() - enemyWhen).count();
    if (ageSeconds > .5f)
        return 0;
    for (uint32_t i = 0; i < enemyCount; ++i) {
        out[i] = enemyWorld[i];
        ids[i] = enemyId[i];
    }
    return enemyCount;
}
uint32_t enemiesFar(std::array<Vec3, farSlots>& out, float& ageSeconds) {
    ageSeconds = 1e9f;
    if (!running.load())
        return 0;
    std::lock_guard<std::mutex> lock(enemyLock);
    ageSeconds = std::chrono::duration<float>(std::chrono::steady_clock::now() - enemyWhen).count();
    if (ageSeconds > 1.f)
        return 0;
    for (uint32_t i = 0; i < farCount; ++i)
        out[i] = farWorld[i];
    return farCount;
}
uint32_t enemyVitals(std::array<Vital, enemySlots>& out) {
    if (!running.load())
        return 0;
    std::lock_guard<std::mutex> lock(enemyLock);
    if (std::chrono::steady_clock::now() - enemyWhen > std::chrono::milliseconds(500))
        return 0;
    for (uint32_t i = 0; i < enemyCount; ++i)
        out[i] = enemyVital[i];
    return enemyCount;
}
bool enemyClass(uint64_t id, char (&out)[nameChars]) {
    out[0] = 0;
    if (!running.load() || !id)
        return false;
    std::lock_guard<std::mutex> lock(enemyLock);
    for (uint32_t i = 0; i < enemyCount; ++i)
        if (enemyId[i] == id) {
            std::memcpy(out, enemyClassName[i].data(), nameChars);
            out[nameChars - 1] = 0;
            return true;
        }
    return false;
}
bool nearestBoss(Boss& out) {
    out = {};
    if (!running.load())
        return false;
    std::lock_guard<std::mutex> lock(enemyLock);
    if (std::chrono::steady_clock::now() - enemyWhen > std::chrono::seconds(1) || !bossNow.id)
        return false;
    out = bossNow;
    return true;
}
uint64_t healthComponents() { return healthFound.load(std::memory_order_relaxed); }
uint64_t heroRecordNow() { return heroNowRecord.load(std::memory_order_relaxed); }
void watchBot(bool on, uint64_t id) {
    botOn.store(on, std::memory_order_relaxed);
    botWant.store(id, std::memory_order_relaxed);
}
bool botProbe(bot_probe::State& out) {
    std::lock_guard<std::mutex> lock(botLock);
    out = botShared;
    return botRan;
}
} // namespace spidy::world_trace

// Live settings: world_log (all three), npc_log, spawn_log, mission_log.
extern "C" __declspec(dllexport) DWORD WINAPI SpidyWorldLog(void* on) {
    const auto v = reinterpret_cast<uintptr_t>(on);
    if (v > 1)
        return 5170;
    master = v != 0;
    return 0;
}
extern "C" __declspec(dllexport) DWORD WINAPI SpidyWorldLogKind(void* packed) {
    // kind << 16 | on
    const auto v = reinterpret_cast<uintptr_t>(packed);
    const auto kind = v >> 16, on = v & 0xffff;
    if (kind >= kinds || on > 1)
        return 5171;
    kindOn[kind] = on != 0;
    return 0;
}
