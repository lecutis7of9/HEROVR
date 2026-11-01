// HERO VR 0.0.76 SM1: the game's bots for the heroes' powers (sm1_combat.hpp, spidy::sm1::targets). Made by Agent Mulder.
// A port of the enemy part of his game_targets.cpp (SpidyVR 0.7.0 by Ilya Mezerowsky, GPL-3.0): the game's component
// registry, read the same way, each component's class told by its RTTI names. READ ONLY: guarded copies, no writes,
// no game calls. Addresses: CANDIDATE (his sources, Spider-Man.exe 4.630.0.0), checked by start() before use.
#include "spidy/sm1_combat.hpp"
#include "spidy/sm1_addon.hpp"

#include <windows.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>

#if defined(_MSC_VER)
#define SM1_TRY __try
#define SM1_EXCEPT __except (EXCEPTION_EXECUTE_HANDLER)
#else
#define SM1_TRY if (true)
#define SM1_EXCEPT else
#endif

namespace spidy::sm1::targets {
namespace {
using combat::Target;

bool read(uintptr_t p, void* out, size_t n) {
    if (p < 0x10000 || p > 0x7fffffffffffull)
        return false;
    bool ok = true;
    SM1_TRY {
        std::memcpy(out, reinterpret_cast<const void*>(p), n);
    }
    SM1_EXCEPT {
        ok = false;
    }
    return ok;
}
uintptr_t pointer(uintptr_t p) {
    uintptr_t v = 0;
    return read(p, &v, sizeof(v)) ? v : 0;
}

Log logTo = nullptr;
void say(const char* fmt, ...) {
    if (!logTo)
        return;
    char text[512];
    va_list a;
    va_start(a, fmt);
    vsnprintf(text, sizeof(text), fmt, a);
    va_end(a);
    logTo(text);
}

uintptr_t base = 0;
uint64_t imageBytes = 0;

// The classes by vtable, and the type descriptors' own bits by image offset (his classify, the same reads).
std::mutex classesLock;
std::unordered_map<uintptr_t, uint32_t> classes;
std::unordered_map<uint32_t, uint32_t> types;
uint32_t classify(uintptr_t vtable) {
    struct Locator {
        uint32_t signature, offset, constructorOffset, type, hierarchy, self;
    } locator{};
    struct Hierarchy {
        uint32_t signature, attributes, count, bases;
    } hierarchy{};
    const uintptr_t at = pointer(vtable - 8);
    if (!read(at, &locator, sizeof(locator)) || locator.signature != 1 || locator.offset || base + locator.self != at ||
        locator.hierarchy >= imageBytes || !read(base + locator.hierarchy, &hierarchy, sizeof(hierarchy)) ||
        !hierarchy.count || hierarchy.count > 64 || hierarchy.bases >= imageBytes)
        return 0;
    uint32_t bits = 0;
    for (uint32_t i = 0; i < hierarchy.count; ++i) {
        uint32_t descriptor = 0, type = 0;
        if (!read(base + hierarchy.bases + 4ull * i, &descriptor, 4) || descriptor >= imageBytes ||
            !read(base + descriptor, &type, 4) || type >= imageBytes)
            return 0;
        auto known = types.find(type);
        if (known == types.end()) {
            char name[64]{};
            uint32_t own = 0;
            for (size_t n = sizeof(name) - 1; n >= 16; n /= 2)
                if (read(base + type + 0x10, name, n)) {
                    name[n] = 0;
                    own = combat::ruleBits(name);
                    break;
                }
            known = types.emplace(type, own).first;
        }
        bits |= known->second;
    }
    return bits;
}
uint32_t classOf(uintptr_t vtable) {
    if (!vtable || vtable - base >= imageBytes)
        return 0;
    std::lock_guard<std::mutex> lock(classesLock);
    auto known = classes.find(vtable);
    if (known == classes.end()) {
        if (classes.size() > 200000)
            return 0;
        known = classes.emplace(vtable, classify(vtable)).first;
    }
    return known->second;
}

bool sizeOf(uintptr_t moverManager, combat::Size& out) {
    struct Body {
        float low, high, radius;
    } body{};
    float scale = 0;
    if (pointer(moverManager + 0xdb8) != base + moverBodySize || !read(moverManager + 0xdc0, &body, sizeof(body)) ||
        !read(moverManager + 0xdf8, &scale, sizeof(scale)))
        return false;
    const combat::Size s{body.low - body.radius, body.high * scale + body.radius, body.radius};
    if (!std::isfinite(s.low) || !std::isfinite(s.high) || !(body.radius > .02f && body.radius < 10) ||
        !(s.high - s.low > .05f && s.high - s.low < 20) || std::fabs(s.low) > 20)
        return false;
    out = s;
    return true;
}

// One pass over the registry: the enemies (bot, health, not civilian/ally/neutral), his scan's reads.
bool scan(std::vector<Target>& out) {
    out.clear();
    const uintptr_t table = pointer(base + registryTable);
    int32_t count = 0;
    if (!table || !read(base + registryCount, &count, 4) || count <= 0 || count > 0x100000)
        return false;
    struct Entry {
        uintptr_t address;
        uint32_t generation, pad;
    };
    std::unordered_map<uint64_t, uint64_t> machines, healths;
    std::unordered_map<uint64_t, uint32_t> actorBits;
    std::unordered_map<uint64_t, std::pair<uint64_t, uint32_t>> movers;
    constexpr uint32_t chunk = 2048;
    static Entry entries[chunk];
    for (uint32_t first = 0; first < static_cast<uint32_t>(count); first += chunk) {
        const uint32_t n = std::min<uint32_t>(chunk, static_cast<uint32_t>(count) - first);
        const bool whole = read(table + first * 16ull, entries, n * sizeof(Entry));
        for (uint32_t k = 0; k < n; ++k) {
            const uint32_t i = first + k;
            Entry entry{};
            if (whole)
                entry = entries[k];
            else if (!read(table + i * 16ull, &entry, sizeof(entry)))
                continue;
            if (entry.address < 0x10000 || entry.address % 8 || !entry.generation || entry.generation > 0xfff)
                continue;
            struct Head {
                uintptr_t vtable, record;
                uint32_t pad, handle;
            } head{};
            if (!read(entry.address, &head, sizeof(head)) || head.handle != (entry.generation << 20 | i) || !head.record)
                continue;
            if (head.vtable == base + syncStaticStateMachine) {
                machines[head.record] = entry.address;
                continue;
            }
            if (head.vtable == base + botHealthVtable) {
                healths[head.record] = entry.address;
                actorBits[head.record] |= combat::botHealth;
                continue;
            }
            if (head.vtable == base + plainHealthVtable) {
                healths.emplace(head.record, entry.address);
                actorBits[head.record] |= combat::health;
                continue;
            }
            if (head.vtable - base >= imageBytes)
                continue;
            const uint32_t bits = classOf(head.vtable) & ~(combat::down | combat::held);
            if (!bits)
                continue;
            if (bits & combat::botHealth)
                healths[head.record] = entry.address;
            else if (bits & combat::health)
                healths.emplace(head.record, entry.address);
            actorBits[head.record] |= bits;
            if (bits & combat::bot)
                movers[head.record] = {entry.address, head.handle};
        }
        if ((first / chunk) % 16 == 15)
            Sleep(0); // the game's threads first
    }
    for (const auto& [record, mover] : movers) {
        const uint32_t bits = actorBits[record];
        if (!combat::enemy(bits))
            continue;
        Target t;
        t.record = record;
        t.mover = mover.first;
        t.handle = mover.second;
        t.bits = bits;
        if (const auto h = healths.find(record); h != healths.end())
            t.health = h->second;
        if (const auto m = machines.find(record); m != machines.end())
            t.machine = m->second;
        sizeOf(static_cast<uintptr_t>(t.mover), t.size);
        out.push_back(t);
    }
    return true;
}

// The thread.
std::mutex listLock;
std::vector<Target> list;
uint64_t scans = 0;
std::atomic<bool> wanted{false}, started{false};
HANDLE wake = nullptr;
DWORD WINAPI run(void*) {
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_LOWEST);
    std::vector<Target> next;
    size_t lastCount = static_cast<size_t>(-1);
    ULONGLONG lastSay = 0;
    for (;;) {
        if (!wanted.load()) {
            WaitForSingleObject(wake, INFINITE);
            continue;
        }
        const ULONGLONG t0 = GetTickCount64();
        const bool ok = scan(next);
        const ULONGLONG took = GetTickCount64() - t0;
        {
            std::lock_guard<std::mutex> lock(listLock);
            list.swap(next);
            ++scans;
        }
        const size_t n = ok ? list.size() : 0;
        if (n != lastCount && GetTickCount64() - lastSay > 5000) {
            say("targets: %s, %zu enemies (scan %llu ms)", ok ? "registry read" : "registry not readable", n,
                static_cast<unsigned long long>(took));
            lastCount = n;
            lastSay = GetTickCount64();
        }
        WaitForSingleObject(wake, 500);
    }
}
std::string exeVersion() {
    wchar_t path[MAX_PATH] = {};
    if (!GetModuleFileNameW(nullptr, path, MAX_PATH))
        return {};
    DWORD dummy = 0;
    const DWORD n = GetFileVersionInfoSizeW(path, &dummy);
    if (!n)
        return {};
    std::string data(n, '\0');
    VS_FIXEDFILEINFO* f = nullptr;
    UINT len = 0;
    if (!GetFileVersionInfoW(path, 0, n, data.data()) || !VerQueryValueW(data.data(), L"\\", reinterpret_cast<void**>(&f), &len) || !f)
        return {};
    char b[64];
    std::snprintf(b, sizeof(b), "%u.%u.%u.%u", HIWORD(f->dwFileVersionMS), LOWORD(f->dwFileVersionMS),
                  HIWORD(f->dwFileVersionLS), LOWORD(f->dwFileVersionLS));
    return b;
}
} // namespace

bool start(Log log) {
    static std::atomic<int> result{-1};
    static std::mutex once;
    std::lock_guard<std::mutex> lock(once);
    if (result >= 0)
        return result == 1;
    logTo = log;
    result = 0;
    const std::string v = exeVersion();
    if (v != hostVersion) {
        say("targets OFF: the addresses are for Spider-Man.exe %s, this is %s", hostVersion, v.empty() ? "unknown" : v.c_str());
        return false;
    }
    base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    int32_t header = 0;
    uint32_t size = 0;
    if (!read(base + 0x3c, &header, 4) || !read(base + header + 0x50, &size, 4) || !size) {
        say("targets OFF: the game image's header could not be read");
        return false;
    }
    imageBytes = size;
    // The CANDIDATE vtables must be the classes they are said to be (RTTI names), or nothing is used.
    const uint32_t bh = classOf(base + botHealthVtable), ph = classOf(base + plainHealthVtable);
    if (!(bh & combat::botHealth) || !(ph & combat::health)) {
        say("targets OFF: the health vtables are not BotHealth/Health here (0x%x, 0x%x)", bh, ph);
        return false;
    }
    const uint32_t ssm = classOf(base + syncStaticStateMachine);
    say("targets: BotHealth and Health vtables match their RTTI names (CANDIDATE RVAs confirmed at run time); state "
        "machine class bits 0x%x", ssm);
    wake = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    HANDLE h = wake ? CreateThread(nullptr, 0, run, nullptr, 0, nullptr) : nullptr;
    if (!h) {
        say("targets OFF: the scan thread did not start");
        return false;
    }
    CloseHandle(h);
    started = true;
    result = 1;
    return true;
}
void want(bool on) {
    if (!started.load())
        return;
    if (wanted.exchange(on) != on && on)
        SetEvent(wake);
}
uint64_t fighting(std::vector<Target>& out) {
    out.clear();
    if (!started.load())
        return 0;
    uint64_t n = 0;
    {
        std::lock_guard<std::mutex> lock(listLock);
        out = list;
        n = scans;
    }
    // Now: still the same actor, in play, up and not held; with his position and hit points (his up/held/position).
    size_t keep = 0;
    for (auto& t : out) {
        uintptr_t head[3] = {};
        if (!read(static_cast<uintptr_t>(t.mover), head, sizeof(head)) || head[1] != t.record ||
            static_cast<uint32_t>(head[2] >> 32) != t.handle)
            continue;
        float m[16]{};
        if (!read(pointer(static_cast<uintptr_t>(t.record)), m, sizeof(m)))
            continue;
        t.feet = {m[12], m[13], m[14]};
        if (!finite(t.feet) || length(t.feet) > 1e6f)
            continue;
        float hp = -1;
        if (t.health && read(static_cast<uintptr_t>(t.health) + healthPointsAt, &hp, 4) && std::isfinite(hp)) {
            if (hp <= 0)
                continue;
            t.hp = hp;
        }
        if (t.machine) {
            uint8_t stated = 0;
            const uintptr_t state = pointer(static_cast<uintptr_t>(t.machine) + 0x70);
            if (!read(static_cast<uintptr_t>(t.machine) + 0x80, &stated, 1) || !stated || !state)
                continue; // not in play
            if (classOf(pointer(state)) & (combat::down | combat::held))
                continue;
        }
        out[keep++] = t;
    }
    out.resize(keep);
    return n;
}
} // namespace spidy::sm1::targets
