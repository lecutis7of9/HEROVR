// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
// Update 37: the WORLD LOG's background thread (include/spidy/world_trace.hpp says what and
// why). Read-only: it reads the component registry and RTTI with guarded reads and
// writes only its own record.
#include "spidy/world_trace.hpp"
#include <atomic>
#include <chrono>
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
    for (uint32_t i = 0; i < hierarchy.count; ++i) {
        uint32_t descriptor{}, type{};
        char name[nameChars]{};
        if (read(imageBase + hierarchy.bases + 4ull * i, &descriptor, 4) && read(imageBase + descriptor, &type, 4) &&
            typeName(imageBase + type, name))
            c.bits |= bitsOf(name);
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
    unsigned registryMisses = 0, slowScans = 0;
    // Update 38: the 10 Hz enemy list (see enemyAt).
    std::vector<uintptr_t> enemyRecords;
    uintptr_t heroRecord{};
    std::unordered_map<uintptr_t, uintptr_t> healthOf, nowHealthOf; // Version 0.0.5: record -> BotHealth
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
        if (heroRecord && position(heroRecord, heroAt))
            for (const uintptr_t record : enemyRecords) {
                if (n >= found.size() && farN >= farList.size())
                    break;
                if (!position(record, at))
                    continue;
                const Vec3 rel = at - heroAt;
                const float metres = length(rel);
                if (!finite(rel))
                    continue;
                if (n < found.size() && metres <= enemyReach)
                    found[n++] = {metres, rel, record, at};
                if (farN < farList.size() && metres <= farReach)
                    farList[farN++] = {metres, at};
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
            }
            enemyWhen = std::chrono::steady_clock::now();
            heroWorldAt = heroAt; // Version 0.0.62: where the hero was for this list
            farCount = std::min<uint32_t>(farN, farSlots);
            for (uint32_t i = 0; i < farCount; ++i)
                farWorld[i] = farList[i].world;
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
            if (!any) {
                work.status = 0;
                nearestEnemy.store(1e9f, std::memory_order_relaxed);
                nearestPerson.store(1e9f, std::memory_order_relaxed);
                baseline = true; // turned back on: a fresh baseline, no burst of "spawns"
                publish(work);
                Sleep(250);
                continue;
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
            nowWatched.clear();
            nowHealthOf.clear();
            nowActors.clear();
            nowArchetype.clear();
            uintptr_t hero{};
            std::vector<std::pair<uintptr_t, uintptr_t>> spawnerOf; // record -> spawner vtable
            for (uint32_t i = 0; i < static_cast<uint32_t>(count) && running.load(); ++i) {
                if (i && i % sliceEntries == 0)
                    Sleep(0);
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
                if (head.vtable == imageBase + botHealthTable && nowHealthOf.size() < 4096) // Version 0.0.5
                    nowHealthOf[head.record] = entry.address;
                auto found = classes.find(head.vtable);
                if (found == classes.end()) {
                    found = classes.emplace(head.vtable, classify(head.vtable)).first;
                    work.classes = classes.size();
                }
                const uint32_t bits = found->second.bits;
                if (!bits)
                    continue;
                if (bits & heroBit)
                    hero = head.record;
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
                healthFound.store(healthOf.size(), std::memory_order_relaxed);
                for (const auto& [record, bits] : nowActors)
                    if (bits & enemyBit && record != hero && enemyRecords.size() < 64)
                        enemyRecords.push_back(record);
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
            baseline = false;
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
            for (float slept = 0; slept < interval && running.load();) {
                const bool fast = fastRefresh.load(std::memory_order_relaxed) && enemyNear.load(std::memory_order_relaxed);
                Sleep(fast ? 33 : 100);
                slept += fast ? .033f : .1f;
                refreshEnemies(); // Update 38: 10 Hz
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
