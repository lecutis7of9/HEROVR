// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
// 0.0.76 SUIT PROBE 1 + 2/3 + 4 + 5 + WORN SUIT (include/spidy/suit_probe.hpp). READ-ONLY: every read of the game goes through peek()
// (SEH); nothing here writes game memory, calls a game function, changes page protection or makes a hook.
// The only file read is the user's own sm2scan.json (tools/sm2_suits/sm2scan.py), never written.
#include <windows.h>

#include <algorithm>
#include <atomic>
#include <mutex>
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
// 0.0.76 (the probe hitch): each probe's reads this frame, and the frame's deadline (QPC ticks; 0: none)
LARGE_INTEGER frameDeadline{};
constexpr uint32_t readsPerFrame = 160;   // per probe (PROBE 3's deep pass: deepReadsPerFrame, so a 10,000-object
constexpr uint32_t deepReadsPerFrame = 600; // pass still ends in about half a minute; the clock bounds it anyway)
constexpr uint64_t microsPerFrame = 1200; // all probes together (~10% of an 88 Hz frame)
bool late() {
    if (!frameDeadline.QuadPart)
        return false;
    LARGE_INTEGER t{};
    QueryPerformanceCounter(&t);
    return t.QuadPart >= frameDeadline.QuadPart;
}
Budget budget(uint32_t most = readsPerFrame) {
    Budget b;
    b.most = most;
    b.late = late;
    return b;
}
Scan1 scan1;
ULONGLONG scan1Started{};
Look look;
Watch5 watch5;
LookLimits lookLimits;
uint64_t lookRecord{};
ULONGLONG lookStarted{};
WornSuit worn; // 0.0.76 WORN SUIT (CANDIDATE): sticky across loads; cleared only when the probe is turned off
std::filesystem::file_time_type newerStamp{}; // the HERO VR folder's sm2scan.json last read for a newer scan
// read by other threads: the worn suit's item id (the MP packet) and the suits' item ids' names
std::atomic<uint64_t> wornItem{0};
std::mutex namesLock;
std::vector<std::pair<uint64_t, std::string>> itemNames;
void publishNames(const Table& t) {
    std::vector<std::pair<uint64_t, std::string>> n;
    for (size_t i = 0; i < t.names.size() && i < t.items.size(); ++i)
        if (t.items[i])
            n.emplace_back(t.items[i], t.names[i]);
    std::lock_guard lock(namesLock);
    itemNames = std::move(n);
}
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
        publishNames(scanTable);
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

// 0.0.76 auto-scan: the launcher writes a new sm2scan.json (with the loadout / material ids) into the HERO VR folder
// in the background, maybe after the game started. While the one loaded has no look ids, that file is read again
// when it changes (every 15 s, its time only), and taken when it has them.
void lookForNewer(void (*log)(const char*)) {
    const std::wstring home = spidyHome();
    if (home.empty())
        return;
    const auto place = std::filesystem::path(home) / L"sm2scan.json";
    std::error_code ec;
    const auto stamp = std::filesystem::last_write_time(place, ec);
    if (ec || stamp == newerStamp)
        return;
    const auto size = std::filesystem::file_size(place, ec);
    if (ec || size < 2 || size > (16u << 20))
        return;
    newerStamp = stamp;
    std::ifstream in(place, std::ios::binary);
    const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    Table t = Table::fromScan(text);
    if (t.empty() || t.looks.empty())
        return;
    scanTable = std::move(t);
    tableFrom = narrow(place.wstring());
    publishNames(scanTable);
    // every probe starts again on the new table (WORN SUIT keeps its suit: the ids are the same)
    deep = Deep{}, deepRecord = 0, watch2.reset(), passEnded = 0;
    container = Container{}, containerRecord = 0, watch4.reset(), containerStarted = 0;
    look = Look{}, lookRecord = 0, watch5.reset(), lookStarted = 0;
    const std::string line = "SUIT PROBE 2: the launcher's new sm2scan.json is here: " + std::to_string(scanTable.names.size()) +
                             " suits, " + std::to_string(scanTable.ids.size()) + " ids, " +
                             std::to_string(scanTable.looks.size()) + " look ids from " + tableFrom +
                             " (PROBE 5 reads the looks now)";
    log(line.c_str());
}

// The game's exe (vtables, code): matched but never walked into.
void exeRange() {
    const auto base = reinterpret_cast<uint64_t>(GetModuleHandleW(nullptr));
    IMAGE_DOS_HEADER dos{};
    IMAGE_NT_HEADERS64 nt{};
    if (!base || !peek(base, &dos, sizeof dos) || dos.e_magic != IMAGE_DOS_SIGNATURE ||
        !peek(base + static_cast<uint32_t>(dos.e_lfanew), &nt, sizeof nt) || nt.Signature != IMAGE_NT_SIGNATURE)
        return;
    limits.skipLo = containerLimits.skipLo = lookLimits.skipLo = base;
    limits.skipHi = containerLimits.skipHi = lookLimits.skipHi = base + nt.OptionalHeader.SizeOfImage;
}

// PROBE 4: record+0x080 -> +0x028 read directly, a pass about once a second (perStep entries a frame).
void containerFrame(uint64_t record, bool menu, ULONGLONG now, void (*log)(const char*)) {
    if (scanTable.empty())
        return;
    const uint64_t ms = now >= onSince ? now - onSince : 0;
    worn.menu(menu, ms);
    // WORN SUIT: fed every finished pass (a load keeps the suit: sticky)
    auto feedWorn = [&] {
        std::vector<uint64_t> ids;
        for (const auto& p : container.found())
            ids.push_back(p.id);
        for (const auto& line : worn.pass(scanTable, ids, ms))
            log(line.c_str());
        wornItem.store(worn.item(), std::memory_order_relaxed);
    };
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
            feedWorn();
            return;
        }
    }
    Budget b = budget();
    if (container.step(reader, scanTable, containerLimits, &b)) {
        for (const auto& line : watch4.pass(scanTable, container))
            log(line.c_str());
        feedWorn();
    }
}

// PROBE 5 (the last suit run): the suit / look ids (equipped loadout, vanity items, materials / textures, the
// suits' item, model and mask ids) on the record, its instance and the hero, two steps into what they point at
// (the components' material overrides); the record's own fields compared across each menu; the menu's open and
// exit told. A pass about once a second, a few objects a frame within the frame's budget.
void lookFrame(uint64_t hero, uint64_t record, bool menu, ULONGLONG now, void (*log)(const char*)) {
    if (scanTable.empty())
        return;
    if (!record) {
        lookRecord = 0;
        return;
    }
    const uint64_t ms = now >= onSince ? now - onSince : 0;
    for (const auto& line : watch5.menu(menu, ms))
        log(line.c_str());
    if (record != lookRecord) {
        lookRecord = record;
        look = Look{};
        watch5.newPlayer();
        lookStarted = 0;
    }
    auto reader = [](uint64_t p, void* out, size_t n) { return safeRead(p, out, n); };
    if (look.status() != Look::reading) {
        if (lookStarted && now - lookStarted < 1000)
            return;
        lookStarted = now ? now : 1;
        if (scanTable.looks.empty()) { // an older sm2scan.json: told once, nothing read
            for (const auto& line : watch5.pass(scanTable, look, ms))
                log(line.c_str());
            return;
        }
        const uint64_t roots[3] = {record, peek64(record), hero};
        look.start(reader, roots, scanTable, lookLimits);
        if (look.status() != Look::reading) {
            for (const auto& line : watch5.pass(scanTable, look, ms))
                log(line.c_str());
            return;
        }
    }
    Budget b = budget();
    if (look.step(reader, scanTable, lookLimits, &b))
        for (const auto& line : watch5.pass(scanTable, look, ms))
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
    if (scanTable.looks.empty() && now - lastLook >= 15000) { // an older sm2scan.json: the launcher's new one?
        lastLook = now;
        lookForNewer(log);
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
    Budget b = budget(deepReadsPerFrame);
    if (deep.step(reader, scanTable, limits, &b)) {
        passEnded = now ? now : 1;
        for (const auto& line : watch2.pass(scanTable, deep))
            log(line.c_str());
        for (const auto& line : watch2.watch(scanTable, deep))
            log(line.c_str());
    }
}
} // namespace

uint64_t wornSuitId() noexcept { return wornItem.load(std::memory_order_relaxed); }

std::string suitNameOfItem(uint64_t item) noexcept {
    try {
        std::lock_guard lock(namesLock);
        for (const auto& n : itemNames)
            if (n.first == item)
                return n.second;
    } catch (...) {
    }
    return {};
}

void frame(uint64_t hero, uint64_t record, bool on, bool menu, void (*rawLog)(const char*)) noexcept {
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
            frameDeadline.QuadPart = t0.QuadPart + static_cast<long long>(qpf.QuadPart * microsPerFrame / 1000000ull);
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
            containerFrame(record, menu, now, log);
            lookFrame(hero, record, menu, now, log);
        } else if (wasOn) {
            look = Look{};
            lookRecord = 0;
            watch5.reset();
            scan1 = Scan1{};
            deep = Deep{};
            deepRecord = 0;
            watch2.reset();
            container = Container{};
            containerRecord = 0;
            watch4.reset();
            worn.reset();
            wornItem.store(0, std::memory_order_relaxed);
        }
        // PROBE 1, spread over frames (0.0.76: it read ~4,000 times in one frame a second, the periodic hitch):
        // a pass starts at most once a second, reads a few root words a frame, and is told when it is done.
        bool scanNow = on != wasOn;
        std::vector<Hit> hits;
        if (on && record) {
            auto reader = [](uint64_t p, void* out, size_t n) { return safeRead(p, out, n); };
            if (!scan1.busy() && (!scan1Started || now - scan1Started >= 1000)) {
                scan1Started = now ? now : 1;
                const uint64_t roots[2] = {record, peek64(record)};
                scan1.start(roots);
            }
            Budget b = budget();
            if (scan1.busy() && scan1.step(reader, Limits{}, &b)) {
                hits = scan1.hits();
                scanNow = true;
            } else if (scanNow)
                scanNow = false; // turned on: told when the first pass is done
        } else if (on && now - lastScan >= 1000)
            scanNow = true, scan1 = Scan1{};
        if (scanNow) {
            const bool turnedOff = wasOn && !on;
            wasOn = on;
            lastScan = now;
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
