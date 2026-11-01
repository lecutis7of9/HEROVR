// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
// 0.0.76 SUIT PROBE 1 + 2/3 + 4 (include/spidy/suit_probe.hpp). READ-ONLY: every read of the game goes through peek()
// (SEH); nothing here writes game memory, calls a game function, changes page protection or makes a hook.
// The only file read is the user's own sm2scan.json (tools/sm2_suits/sm2scan.py), never written.
#include <windows.h>

#include <algorithm>
#include <cstring>
#include <iterator>
#include <exception>
#include <filesystem>
#include <fstream>

#include "spidy/phone_music.hpp"
#include "spidy/suit_probe.hpp"

namespace spidy::suit_probe {
namespace {
bool peek(uint64_t p, void* out, size_t n) {
    __try {
        if (p < lowest || p >= highest || n > highest - p)
            return false;
        std::memcpy(out, reinterpret_cast<const void*>(p), n);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}
// Which memory may be read, from VirtualQuery, kept a pass (a few seconds): a read only of committed, readable
// pages that are not guard pages (touching a thread's stack guard page would take it away), so the SEH in peek
// is the last line, not the usual one, and costs no exception in the common case.
struct Region {
    uint64_t lo, hi;
    bool ok;
};
std::vector<Region> regions; // sorted by lo, never overlapping
const Region* regionOf(uint64_t p) {
    auto it = std::upper_bound(regions.begin(), regions.end(), p, [](uint64_t v, const Region& r) { return v < r.lo; });
    if (it != regions.begin() && p < std::prev(it)->hi)
        return &*std::prev(it);
    MEMORY_BASIC_INFORMATION mbi{};
    if (!VirtualQuery(reinterpret_cast<LPCVOID>(p), &mbi, sizeof mbi))
        return nullptr;
    const DWORD canRead = PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READ |
                           PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
    const auto lo = reinterpret_cast<uint64_t>(mbi.BaseAddress);
    Region r{lo, lo + mbi.RegionSize,
             mbi.State == MEM_COMMIT && (mbi.Protect & canRead) && !(mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS))};
    if (r.hi <= p)
        return nullptr;
    if (regions.size() >= 8192)
        regions.clear();
    it = std::upper_bound(regions.begin(), regions.end(), r.lo, [](uint64_t v, const Region& e) { return v < e.lo; });
    return &*regions.insert(it, r);
}
bool readable(uint64_t p, size_t n) {
    if (p < lowest || p >= highest || n > highest - p)
        return false;
    for (uint64_t at = p; at < p + n;) {
        const Region* r = regionOf(at);
        if (!r || !r->ok)
            return false;
        at = r->hi;
    }
    return true;
}
bool safeRead(uint64_t p, void* out, size_t n) { return readable(p, n) && peek(p, out, n); }
uint64_t peek64(uint64_t p) {
    uint64_t v{};
    return safeRead(p, &v, 8) ? v : 0;
}
ULONGLONG regionsAt{}; // when regions was last emptied (every 3 s: memory changes)
// the VR loop's thread only
Watch watch;
ULONGLONG lastScan{}; // GetTickCount64
bool wasOn{};
Table scanTable;
std::string tableFrom;
ULONGLONG lastLook{}; // when sm2scan.json was last looked for (every 15 s while missing)
bool toldMissing{};
Deep deep;
Watch2 watch2;
DeepLimits limits;
uint64_t deepRecord{};
ULONGLONG passEnded{};
Container container; // PROBE 4
Watch4 watch4;
ContainerLimits containerLimits;
uint64_t containerRecord{};
ULONGLONG containerStarted{};
Cost cost; // what frame() costs the VR thread while on
ULONGLONG onSince{}, costTold{};
LARGE_INTEGER qpf{};
void (*sink)(const char*) = nullptr; // the caller's log, for stampedLog

// Every line with its time: since the probe was turned on, and the local clock.
void stampedLog(const char* line) {
    if (!sink || !line)
        return;
    SYSTEMTIME st{};
    GetLocalTime(&st);
    const ULONGLONG now = GetTickCount64();
    const std::string s = stamped(line, onSince && now >= onSince ? now - onSince : 0, st.wHour, st.wMinute, st.wSecond,
                                  st.wMilliseconds);
    sink(s.c_str());
}

std::string narrow(const std::wstring& w) {
    if (w.empty())
        return {};
    const int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
    std::string s(n > 0 ? n : 0, '\0');
    if (n > 0)
        WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()), s.data(), n, nullptr, nullptr);
    return s;
}

// sm2scan.json in the HERO VR folder, its reports folder, or on the Desktop (also OneDrive's).
void lookForScan(void (*log)(const char*)) {
    std::vector<std::filesystem::path> places;
    const std::wstring home = spidyHome();
    if (!home.empty()) {
        places.emplace_back(std::filesystem::path(home) / L"sm2scan.json");
        places.emplace_back(std::filesystem::path(home) / L"reports" / L"sm2scan.json");
    }
    wchar_t profile[MAX_PATH]{};
    if (GetEnvironmentVariableW(L"USERPROFILE", profile, MAX_PATH)) {
        places.emplace_back(std::filesystem::path(profile) / L"Desktop" / L"sm2scan.json");
        places.emplace_back(std::filesystem::path(profile) / L"OneDrive" / L"Desktop" / L"sm2scan.json");
        places.emplace_back(std::filesystem::path(profile) / L"Downloads" / L"sm2scan.json");
    }
    for (const auto& place : places) {
        std::error_code ec;
        const auto size = std::filesystem::file_size(place, ec);
        if (ec || size < 2 || size > (16u << 20))
            continue;
        std::ifstream in(place, std::ios::binary);
        const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        Table t = Table::fromScan(text);
        if (t.empty()) {
            const std::string line = "SUIT PROBE 2: " + narrow(place.wstring()) + " is not an sm2scan.json with suits";
            log(line.c_str());
            continue;
        }
        scanTable = std::move(t);
        tableFrom = narrow(place.wstring());
        const std::string line = "SUIT PROBE 2: " + std::to_string(scanTable.names.size()) + " suits, " +
                                 std::to_string(scanTable.ids.size()) + " ids from " + tableFrom;
        log(line.c_str());
        std::string watching = "SUIT PROBE 3: watching every place of " + std::to_string(scanTable.watchIds.size()) +
                               " model/mask ids of";
        for (uint16_t s : scanTable.watchSuits)
            watching += " " + scanTable.names[s];
        if (scanTable.watchSuits.empty())
            watching += " none (none of the watched suits is in this sm2scan.json)";
        log(watching.c_str());
        return;
    }
    if (!toldMissing) {
        toldMissing = true;
        const std::string line = "SUIT PROBE 2: no sm2scan.json yet (put it in " + narrow(home) +
                                 " or on the Desktop); the deep search waits for it";
        log(line.c_str());
    }
}

// The game's exe (vtables, code): matched but never walked into.
void exeRange() {
    const auto base = reinterpret_cast<uint64_t>(GetModuleHandleW(nullptr));
    IMAGE_DOS_HEADER dos{};
    IMAGE_NT_HEADERS64 nt{};
    if (!base || !peek(base, &dos, sizeof dos) || dos.e_magic != IMAGE_DOS_SIGNATURE ||
        !peek(base + static_cast<uint32_t>(dos.e_lfanew), &nt, sizeof nt) || nt.Signature != IMAGE_NT_SIGNATURE)
        return;
    limits.skipLo = containerLimits.skipLo = base;
    limits.skipHi = containerLimits.skipHi = base + nt.OptionalHeader.SizeOfImage;
}

// PROBE 4: record+0x080 -> +0x028 read directly, a pass about once a second (perStep entries a frame).
void containerFrame(uint64_t record, ULONGLONG now, void (*log)(const char*)) {
    if (scanTable.empty())
        return;
    if (!record) {
        containerRecord = 0;
        return;
    }
    if (record != containerRecord) { // a new player (a load): start again
        containerRecord = record;
        container = Container{};
        watch4.reset();
        containerStarted = 0;
    }
    auto reader = [](uint64_t p, void* out, size_t n) { return safeRead(p, out, n); };
    if (container.status() != Container::reading) {
        if (containerStarted && now - containerStarted < 1000)
            return;
        containerStarted = now ? now : 1;
        container.start(reader, record, scanTable, containerLimits);
        if (container.status() != Container::reading) {
            for (const auto& line : watch4.pass(scanTable, container))
                log(line.c_str());
            return;
        }
    }
    if (container.step(reader, scanTable, containerLimits))
        for (const auto& line : watch4.pass(scanTable, container))
            log(line.c_str());
}

void deepFrame(uint64_t hero, uint64_t record, ULONGLONG now, void (*log)(const char*)) {
    if (scanTable.empty()) {
        if (!lastLook || now - lastLook >= 15000) {
            lastLook = now;
            lookForScan(log);
        }
        return;
    }
    if (!record) {
        deepRecord = 0;
        return;
    }
    if (record != deepRecord) { // a new player (a load): start again; changes are still told against the last set
        deepRecord = record;
        deep = Deep{};
        watch2.newPlayer();
        passEnded = 0;
    }
    if (!deep.busy()) {
        if (passEnded && now - passEnded < 2000)
            return;
        const uint64_t roots[3] = {record, peek64(record), hero};
        deep.start(roots);
        if (!deep.busy())
            return;
    }
    auto reader = [](uint64_t p, void* out, size_t n) { return safeRead(p, out, n); };
    if (deep.step(reader, scanTable, limits)) {
        passEnded = now ? now : 1;
        for (const auto& line : watch2.pass(scanTable, deep))
            log(line.c_str());
        for (const auto& line : watch2.watch(scanTable, deep))
            log(line.c_str());
    }
}
} // namespace

void frame(uint64_t hero, uint64_t record, bool on, void (*rawLog)(const char*)) noexcept {
    try {
        if (!rawLog)
            return;
        sink = rawLog;
        void (*log)(const char*) = stampedLog;
        const ULONGLONG now = GetTickCount64();
        LARGE_INTEGER t0{};
        if (on) {
            if (!qpf.QuadPart)
                QueryPerformanceFrequency(&qpf);
            QueryPerformanceCounter(&t0);
            if (!onSince)
                onSince = costTold = now ? now : 1;
        }
        if (now - regionsAt >= 3000) {
            regions.clear();
            regionsAt = now;
        }
        if (on && !limits.skipHi)
            exeRange();
        if (on) {
            deepFrame(hero, record, now, log);
            containerFrame(record, now, log);
        } else if (wasOn) {
            deep = Deep{};
            deepRecord = 0;
            watch2.reset();
            container = Container{};
            containerRecord = 0;
            watch4.reset();
        }
        const bool scanNow = !(on == wasOn && (!on || now - lastScan < 1000));
        if (scanNow) {
            const bool turnedOff = wasOn && !on;
            wasOn = on;
            lastScan = now;
            std::vector<Hit> hits;
            if (on && record) {
                const uint64_t roots[2] = {record, peek64(record)};
                auto reader = [](uint64_t p, void* out, size_t n) { return safeRead(p, out, n); };
                hits = scan(roots, reader);
            }
            for (const auto& line : watch.update(on, record, hits))
                log(line.c_str());
            if (turnedOff) {
                if (cost.frames)
                    log(cost.line(now - onSince).c_str());
                cost.reset();
                onSince = costTold = 0;
            }
        }
        if (on && qpf.QuadPart) {
            LARGE_INTEGER t1{};
            QueryPerformanceCounter(&t1);
            const long long ticks = t1.QuadPart - t0.QuadPart;
            cost.add(ticks > 0 ? static_cast<uint64_t>(ticks) * 1000000ull / static_cast<uint64_t>(qpf.QuadPart) : 0);
            if (now - costTold >= 30000) {
                costTold = now;
                log(cost.line(now - onSince).c_str());
            }
        }
    } catch (...) {
    }
}

} // namespace spidy::suit_probe
