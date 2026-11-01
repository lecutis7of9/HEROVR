// HERO VR 0.0.76 SM1: the research scan (live.txt sm1scan). Made by Agent Mulder.
// Mulder's Heroes add-on for iilyn's Spider-Man VR mod (SpidyVR by Ilya Mezerowsky, GPL-3.0,
// https://github.com/IlyaMez/SpidyVR). The RTTI walk follows the MSVC x64 layout as documented publicly and as
// Tkachov's Overstrike/ALERT research tools (GPL-3) read Insomniac's data; nothing of theirs is copied.
//
// READ-ONLY: this file never writes game memory and never calls a game function or one of his functions. Each
// read checks VirtualQuery (committed, readable, not a guard page) and copies under SEH; a fault ends the scan only.
// One lowest-priority thread, small chunks with sleeps between them, a time cap and a size cap.
// Output: %APPDATA%\HERO VR SM1\scan\sm1scan-<time>.jsonl (+ .bin for raw dumps, + .txt summary). Every value is a
// CANDIDATE for later work; nothing here is used by any feature.
#include "spidy/sm1_scan.hpp"

#include "spidy/sm1_addon.hpp"

#include <windows.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#if defined(_MSC_VER)
#define SCAN_TRY __try
#define SCAN_EXCEPT __except (EXCEPTION_EXECUTE_HANDLER)
#else
// The Linux cross check (zig/mingw) has no __try; the MSVC build is the one that ships.
#define SCAN_TRY if (true)
#define SCAN_EXCEPT else
#endif

namespace spidy::sm1::scan {
namespace {
Feed feed;
std::atomic<bool> started{false};
const Limits limits{};

void say(const char* fmt, ...) {
    if (!feed.log)
        return;
    char b[512];
    va_list a;
    va_start(a, fmt);
    std::vsnprintf(b, sizeof(b), fmt, a);
    va_end(a);
    feed.log(b);
}
bool on() { return !feed.enabled || feed.enabled(); }
uint64_t nowMs() { return GetTickCount64(); }

// ---- reads (the only way this file touches game memory) ----
// No C++ object with a destructor may live in a __try function, so the guarded copy is on its own.
bool copyGuarded(void* dst, const void* src, size_t n) {
    bool ok = true;
    SCAN_TRY { std::memcpy(dst, src, n); }
    SCAN_EXCEPT { ok = false; }
    return ok;
}
struct Region {
    uintptr_t lo = 0, hi = 0;
    bool ok = false;
};
// 0.0.76: a few regions remembered (the registry's objects are spread over many heaps; one cached
// region meant a VirtualQuery for nearly every read, the census's cost).
constexpr size_t regionSlots = 16;
Region regions[regionSlots];
size_t regionNext = 0;
void forgetRegion() {
    for (auto& r : regions)
        r = Region{};
    regionNext = 0;
}
const Region* regionOf(uintptr_t p) {
    for (const auto& r : regions)
        if (r.hi && p >= r.lo && p < r.hi)
            return &r;
    MEMORY_BASIC_INFORMATION m{};
    if (!VirtualQuery(reinterpret_cast<const void*>(p), &m, sizeof(m)))
        return nullptr;
    Region& r = regions[regionNext++ % regionSlots];
    r.lo = reinterpret_cast<uintptr_t>(m.BaseAddress);
    r.hi = r.lo + m.RegionSize;
    constexpr DWORD canRead = PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READ |
                              PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
    r.ok = m.State == MEM_COMMIT && (m.Protect & (PAGE_GUARD | PAGE_NOACCESS)) == 0 && (m.Protect & canRead) != 0;
    return r.hi > p ? &r : nullptr;
}
bool readable(uintptr_t p, size_t n) {
    const uintptr_t end = p + n;
    if (end < p)
        return false;
    while (p < end) {
        const Region* r = regionOf(p);
        if (!r || !r->ok)
            return false;
        p = r->hi;
    }
    return true;
}
bool rd(uintptr_t p, void* out, size_t n) {
    if (p < 0x10000 || p > 0x7fffffffffffull || !n)
        return false;
    return readable(p, n) && copyGuarded(out, reinterpret_cast<const void*>(p), n);
}
template <class T> bool get(uintptr_t p, T& v) { return rd(p, &v, sizeof(T)); }
uintptr_t ptr(uintptr_t p) {
    uintptr_t v = 0;
    return get(p, v) ? v : 0;
}
std::string cstr(uintptr_t p, size_t most = 255) {
    std::string s;
    char b[64];
    while (s.size() < most) {
        const size_t n = std::min<size_t>(sizeof(b), most - s.size());
        if (!rd(p + s.size(), b, n)) {
            // the end of a region: one byte at a time
            char c = 0;
            if (!rd(p + s.size(), &c, 1) || !c)
                break;
            s += c;
            continue;
        }
        const void* z = std::memchr(b, 0, n);
        s.append(b, z ? static_cast<size_t>(static_cast<const char*>(z) - b) : n);
        if (z)
            break;
    }
    return s;
}

// ---- the game image ----
struct Section {
    std::string name;
    uint32_t rva = 0, size = 0, raw = 0, flags = 0;
    bool code() const { return (flags & IMAGE_SCN_MEM_EXECUTE) != 0; }
    bool writable() const { return (flags & IMAGE_SCN_MEM_WRITE) != 0; }
};
struct Image {
    uintptr_t base = 0;
    uint32_t size = 0, stamp = 0, checksum = 0;
    std::vector<Section> sections;
    bool in(uintptr_t p) const { return base && p >= base && p < base + size; }
    uint32_t rva(uintptr_t p) const { return in(p) ? static_cast<uint32_t>(p - base) : 0; }
    const Section* at(uint32_t r) const {
        for (const auto& s : sections)
            if (r >= s.rva && r < s.rva + std::max(s.size, s.raw))
                return &s;
        return nullptr;
    }
} img;

bool readImage() {
    img.base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    IMAGE_DOS_HEADER dos{};
    if (!get(img.base, dos) || dos.e_magic != IMAGE_DOS_SIGNATURE)
        return false;
    IMAGE_NT_HEADERS64 nt{};
    if (!get(img.base + static_cast<uint32_t>(dos.e_lfanew), nt) || nt.Signature != IMAGE_NT_SIGNATURE)
        return false;
    img.size = nt.OptionalHeader.SizeOfImage;
    img.stamp = nt.FileHeader.TimeDateStamp;
    img.checksum = nt.OptionalHeader.CheckSum;
    const uintptr_t first = img.base + static_cast<uint32_t>(dos.e_lfanew) + offsetof(IMAGE_NT_HEADERS64, OptionalHeader) +
                            nt.FileHeader.SizeOfOptionalHeader;
    for (unsigned i = 0; i < nt.FileHeader.NumberOfSections && i < 96; ++i) {
        IMAGE_SECTION_HEADER h{};
        if (!get(first + i * sizeof(h), h))
            break;
        Section s;
        s.name.assign(reinterpret_cast<const char*>(h.Name), strnlen(reinterpret_cast<const char*>(h.Name), 8));
        s.rva = h.VirtualAddress;
        s.size = h.Misc.VirtualSize;
        s.raw = h.SizeOfRawData;
        s.flags = h.Characteristics;
        img.sections.push_back(s);
    }
    return true;
}

std::string fileVersion() {
    wchar_t path[MAX_PATH] = {};
    if (!GetModuleFileNameW(nullptr, path, MAX_PATH))
        return {};
    DWORD dummy = 0;
    const DWORD n = GetFileVersionInfoSizeW(path, &dummy);
    if (!n)
        return {};
    std::vector<uint8_t> data(n);
    VS_FIXEDFILEINFO* f = nullptr;
    UINT len = 0;
    if (!GetFileVersionInfoW(path, 0, n, data.data()) || !VerQueryValueW(data.data(), L"\\", reinterpret_cast<void**>(&f), &len) || !f)
        return {};
    char b[64];
    std::snprintf(b, sizeof(b), "%u.%u.%u.%u", HIWORD(f->dwFileVersionMS), LOWORD(f->dwFileVersionMS),
                  HIWORD(f->dwFileVersionLS), LOWORD(f->dwFileVersionLS));
    return b;
}

// ---- output ----
std::string addr(uintptr_t v) {
    char b[24];
    std::snprintf(b, sizeof(b), "\"0x%llx\"", static_cast<unsigned long long>(v));
    return b;
}
std::string rvaText(uintptr_t p) { return img.in(p) ? addr(p - img.base) : "null"; }
std::string floats(const float* f, int n) {
    std::string s = "[";
    for (int i = 0; i < n; ++i)
        s += (i ? "," : "") + number(f[i]);
    return s + "]";
}

struct Out {
    HANDLE json = INVALID_HANDLE_VALUE, bin = INVALID_HANDLE_VALUE;
    uint64_t bytes = 0, binAt = 0, lines = 0;
    bool full = false;
    std::wstring stem;
    static bool put(HANDLE h, const void* p, size_t n) {
        DWORD w = 0;
        return h != INVALID_HANDLE_VALUE && WriteFile(h, p, static_cast<DWORD>(n), &w, nullptr) && w == n;
    }
    bool room(size_t n) {
        if (full || bytes + n > limits.maxFileBytes) {
            if (!full)
                say("sm1scan: the %llu MB cap is reached; the scan stops writing", static_cast<unsigned long long>(limits.maxFileBytes >> 20));
            full = true;
            return false;
        }
        bytes += n;
        return true;
    }
    void line(const std::string& s) {
        if (!room(s.size() + 1))
            return;
        put(json, s.data(), s.size());
        put(json, "\n", 1);
        ++lines;
    }
    // Raw bytes into the .bin; the line gets [offset, length].
    std::string blob(const void* p, size_t n) {
        if (!n || !room(n) || !put(bin, p, n))
            return "null";
        const uint64_t at = binAt;
        binAt += n;
        return "[" + std::to_string(at) + "," + std::to_string(n) + "]";
    }
    // Reads `n` bytes at `p` into the .bin ("null" when unreadable).
    std::string dump(uintptr_t p, size_t n) {
        std::vector<uint8_t> b(n);
        return rd(p, b.data(), n) ? blob(b.data(), n) : "null";
    }
    void close() {
        if (json != INVALID_HANDLE_VALUE)
            CloseHandle(json);
        if (bin != INVALID_HANDLE_VALUE)
            CloseHandle(bin);
        json = bin = INVALID_HANDLE_VALUE;
    }
} out;

struct J {
    std::string s;
    explicit J(const char* kind) { s = "{\"ms\":" + std::to_string(nowMs()) + ",\"kind\":" + quoted(kind); }
    J& raw(const char* k, const std::string& v) {
        s += ",\"";
        s += k;
        s += "\":";
        s += v;
        return *this;
    }
    J& str(const char* k, std::string_view v) { return raw(k, quoted(v)); }
    J& num(const char* k, double v) { return raw(k, number(v)); }
    J& u(const char* k, uint64_t v) { return raw(k, std::to_string(v)); }
    J& a(const char* k, uintptr_t v) { return raw(k, addr(v)); }
    void write() { out.line(s + ",\"status\":\"CANDIDATE\"}"); }
};

std::wstring scanDir() {
    wchar_t buf[MAX_PATH] = {};
    const DWORD n = GetEnvironmentVariableW(L"APPDATA", buf, MAX_PATH);
    if (!n || n >= MAX_PATH)
        return {};
    std::wstring d = std::wstring(buf) + L"\\HERO VR SM1";
    CreateDirectoryW(d.c_str(), nullptr);
    d += L"\\scan";
    CreateDirectoryW(d.c_str(), nullptr);
    return d;
}
std::wstring wide(const std::string& s) { return std::wstring(s.begin(), s.end()); }
std::string narrow(const std::wstring& s) {
    std::string o;
    for (const wchar_t c : s)
        o += c < 0x80 ? static_cast<char>(c) : '?';
    return o;
}
void pruneOld(const std::wstring& dir) {
    std::vector<std::string> files;
    WIN32_FIND_DATAW f{};
    HANDLE h = FindFirstFileW((dir + L"\\sm1scan-*").c_str(), &f);
    if (h == INVALID_HANDLE_VALUE)
        return;
    do
        files.push_back(narrow(f.cFileName));
    while (FindNextFileW(h, &f));
    FindClose(h);
    // Keeps room for this session: the newest keepSessions-1 old ones stay.
    for (const auto& name : oldSessions(files, limits.keepSessions - 1))
        DeleteFileW((dir + L"\\" + wide(name)).c_str());
}
bool openOut(std::wstring& dir) {
    dir = scanDir();
    if (dir.empty())
        return false;
    pruneOld(dir);
    SYSTEMTIME t{};
    GetLocalTime(&t);
    out.stem = dir + L"\\" + wide(sessionName(t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond));
    out.json = CreateFileW((out.stem + L".jsonl").c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    out.bin = CreateFileW((out.stem + L".bin").c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS,
                          FILE_ATTRIBUTE_NORMAL, nullptr);
    return out.json != INVALID_HANDLE_VALUE && out.bin != INVALID_HANDLE_VALUE;
}

// ---- pacing ----
uint64_t deadline = 0;
bool stopping() { return out.full || (deadline && nowMs() > deadline); }
// Between chunks: a short sleep, and a wait while live.txt has sm1scan=0 (the deadline does not move).
bool breathe() {
    Sleep(limits.breatheMs);
    while (!on()) {
        Sleep(1000);
        forgetRegion();
        if (deadline && nowMs() > deadline)
            return false;
    }
    return !stopping();
}

// ---- RTTI (MSVC x64: vtable[-1] -> CompleteObjectLocator) ----
struct Rtti {
    std::string name;
    std::vector<std::string> bases;
};
std::unordered_map<uintptr_t, Rtti> rttiCache;
std::string typeName(uint32_t tdRva) { return className(cstr(img.base + tdRva + 16)); }
const Rtti& rttiOf(uintptr_t vtable) {
    static const Rtti none;
    if (const auto it = rttiCache.find(vtable); it != rttiCache.end())
        return it->second;
    if (rttiCache.size() > 40000)
        return none;
    Rtti r;
    uint32_t col[6] = {};
    const uintptr_t at = img.in(vtable) ? ptr(vtable - 8) : 0;
    if (img.in(at) && rd(at, col, sizeof(col)) && col[0] == 1 && col[5] == img.rva(at) && col[3] < img.size) {
        r.name = typeName(col[3]);
        uint32_t chd[4] = {};
        if (col[4] < img.size && rd(img.base + col[4], chd, sizeof(chd)) && chd[2] <= 64 && chd[3] < img.size)
            for (uint32_t i = 1; i < chd[2]; ++i) { // [0] is the class itself
                uint32_t bcd = 0, td = 0;
                if (get(img.base + chd[3] + i * 4, bcd) && bcd < img.size && get(img.base + bcd, td) && td < img.size)
                    r.bases.push_back(typeName(td));
            }
    }
    return rttiCache.emplace(vtable, std::move(r)).first->second;
}
std::string names(const Rtti& r) {
    std::string s = "[" + quoted(r.name);
    for (const auto& b : r.bases)
        s += "," + quoted(b);
    return s + "]";
}
bool any(const Rtti& r, bool (*test)(std::string_view)) {
    if (test(r.name))
        return true;
    for (const auto& b : r.bases)
        if (test(b))
            return true;
    return false;
}
bool healthName(std::string_view n) { return n.find("Health") != std::string_view::npos; }
bool moverName(std::string_view n) { return n.find("MoverManager") != std::string_view::npos; }
bool spawnName(std::string_view n) {
    Area a{};
    return interesting(n, a) && a == Area::spawning;
}
bool attachName(std::string_view n) {
    Area a{};
    return interesting(n, a) && a == Area::hands;
}

// ---- his exported data (a plain copy; never a call) ----
struct Export {
    const char* name;
    uintptr_t at = 0;
    std::string module;
    std::vector<uint8_t> last;
    uint32_t dumps = 0;
};
std::vector<Export> exportsSeen;
const char* const dataExports[] = {"SpidyXrData",    "SpidyBodyData",   "SpidyBodyPoses",  "SpidyRayData",
                                   "SpidySwingData", "SpidyAimData",    "SpidyRopeData",   "SpidyClimbData",
                                   "SpidyGrabData",  "SpidyStrikeData", "SpidyPunchData",  "SpidySenseData",
                                   "SpidyThrowData", "SpidyFocusData",  "SpidyDodgeData",  "SpidyMotionData",
                                   "SpidyMotionDriven"};
Export* exported(const char* name) {
    for (auto& e : exportsSeen)
        if (!std::strcmp(e.name, name))
            return e.at ? &e : nullptr;
    return nullptr;
}
void findExports() {
    exportsSeen.clear();
    std::vector<const char*> mods(std::begin(spidyModules), std::end(spidyModules));
    mods.push_back(spidyVrModule);
    for (const char* name : dataExports) {
        Export e;
        e.name = name;
        for (const char* m : mods)
            if (HMODULE h = GetModuleHandleA(m))
                if (FARPROC p = GetProcAddress(h, name)) {
                    e.at = reinterpret_cast<uintptr_t>(p);
                    e.module = m;
                    break;
                }
        exportsSeen.push_back(e);
    }
}
// The bytes his header says (magic, version, bytes), at most `most`.
std::vector<uint8_t> exportBytes(const Export& e, size_t most = 16384) {
    uint32_t h[3] = {};
    if (!e.at || !rd(e.at, h, sizeof(h)))
        return {};
    // His exports start {magic, version, bytes}; anything else (a flag, a plain value) gets 64 bytes.
    const size_t n = h[2] >= 12 && h[2] <= 65536 ? std::min<size_t>(h[2], most) : 64;
    std::vector<uint8_t> b(n);
    if (!rd(e.at, b.data(), n))
        return {};
    return b;
}

// ---- phase A: the image, his modules, the anchors, the signatures, the RTTI catalog ----
void writeImage() {
    J j("image");
    SYSTEMTIME t{};
    GetLocalTime(&t);
    char when[40];
    std::snprintf(when, sizeof(when), "%04u-%02u-%02u %02u:%02u:%02u", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);
    j.str("localTime", when).u("tickMs", nowMs());
    j.a("base", img.base).a("sizeOfImage", img.size).a("timeDateStamp", img.stamp).a("checksum", img.checksum);
    j.str("fileVersion", fileVersion()).str("expectedVersion", hostVersion);
    std::string secs = "[";
    for (size_t i = 0; i < img.sections.size(); ++i) {
        const auto& s = img.sections[i];
        secs += (i ? "," : "") + std::string("{\"name\":") + quoted(s.name) + ",\"rva\":" + addr(s.rva) +
                ",\"size\":" + std::to_string(s.size) + ",\"raw\":" + std::to_string(s.raw) +
                ",\"flags\":" + addr(s.flags) + "}";
    }
    j.raw("sections", secs + "]");
    std::string mods = "[";
    bool first = true;
    auto mod = [&](const char* m) {
        HMODULE h = GetModuleHandleA(m);
        mods += (first ? "" : ",") + std::string("{\"name\":") + quoted(m) + ",\"base\":" +
                (h ? addr(reinterpret_cast<uintptr_t>(h)) : "null") + "}";
        first = false;
    };
    for (const char* m : spidyModules)
        mod(m);
    mod(spidyVrModule);
    j.raw("hisModules", mods + "]");
    std::string ex = "[";
    first = true;
    for (const auto& e : exportsSeen) {
        uint32_t h[3] = {};
        const bool ok = e.at && rd(e.at, h, sizeof(h));
        ex += (first ? "" : ",") + std::string("{\"name\":") + quoted(e.name) + ",\"module\":" + quoted(e.module) +
              ",\"at\":" + (e.at ? addr(e.at) : "null") + ",\"magic\":" + (ok ? addr(h[0]) : "null") +
              ",\"version\":" + (ok ? std::to_string(h[1]) : "null") + ",\"bytes\":" + (ok ? std::to_string(h[2]) : "null") + "}";
        first = false;
    }
    j.raw("hisDataExports", ex + "]");
    j.write();
}

void writeAnchors() {
    for (const auto& an : anchors()) {
        J j("anchor");
        j.str("name", an.name).a("rva", an.rva).str("type", kindName(an.kind)).str("area", areaName(an.area));
        j.str("source", an.source);
        const uintptr_t p = img.base + an.rva;
        const Section* s = img.at(an.rva);
        j.raw("section", s ? quoted(s->name) : "null");
        uint8_t b[64] = {};
        const bool ok = rd(p, b, sizeof(b));
        j.raw("bytes", ok ? quoted(hex(b, sizeof(b))) : "null");
        if (an.expect)
            j.raw("matchesHisBytes", ok && !std::memcmp(b, an.expect, an.expectBytes) ? "true" : "false");
        if (ok && an.kind == Kind::vtable) {
            const Rtti& r = rttiOf(p);
            j.raw("rtti", names(r));
            std::string v = "[";
            for (int i = 0; i < 16; ++i) {
                uint64_t f = 0;
                std::memcpy(&f, b + (i % 8) * 8, 8);
                if (i >= 8 && !get(p + i * 8, f))
                    break;
                v += (i ? "," : "") + rvaText(static_cast<uintptr_t>(f));
            }
            j.raw("vfuncRvas", v + "]");
        }
        if (ok && an.kind == Kind::data) {
            uint64_t v = 0;
            std::memcpy(&v, b, 8);
            j.a("value", static_cast<uintptr_t>(v));
            uint8_t t[256];
            if (rd(static_cast<uintptr_t>(v), t, sizeof(t))) {
                j.raw("pointsAt", quoted(hex(t, 64))).raw("pointsAtDump", out.blob(t, sizeof(t)));
                uint64_t vt = 0;
                std::memcpy(&vt, t, 8);
                if (img.in(static_cast<uintptr_t>(vt))) {
                    j.a("pointsAtVtableRva", static_cast<uintptr_t>(vt) - img.base);
                    j.raw("pointsAtRtti", names(rttiOf(static_cast<uintptr_t>(vt))));
                }
            }
        }
        j.write();
    }
}

// One pass over a section's bytes in chunks; `each(chunk, n, rvaOfChunk)` with `overlap` bytes carried over.
template <class F> bool eachChunk(const Section& s, size_t overlap, F&& each) {
    // initialized bytes only: never the zero-fill tail, so no page is committed by reading it
    const uint32_t total = s.raw ? std::min(s.size ? s.size : s.raw, s.raw) : 0;
    std::vector<uint8_t> b(limits.chunkBytes + overlap);
    for (uint32_t at = 0; at < total;) {
        const uint32_t n = std::min<uint32_t>(limits.chunkBytes + static_cast<uint32_t>(overlap), total - at);
        forgetRegion();
        if (rd(img.base + s.rva + at, b.data(), n))
            each(b.data(), static_cast<size_t>(n), s.rva + at);
        if (n <= overlap || !breathe())
            return !stopping();
        at += n - static_cast<uint32_t>(overlap);
        if (n < limits.chunkBytes)
            break;
    }
    return true;
}

void signatureScan() {
    // His entry-byte checks as signatures: where else they occur in the code (0 hits = moved or a different build).
    std::vector<std::vector<uint64_t>> hits(anchors().size());
    for (const auto& s : img.sections)
        if (s.code())
            eachChunk(s, 32, [&](const uint8_t* b, size_t n, uint32_t rva) {
                for (size_t i = 0; i < anchors().size(); ++i)
                    if (anchors()[i].expect && hits[i].size() < 8) {
                        std::vector<uint64_t> h;
                        findAll(b, n, rva, anchors()[i].expect, anchors()[i].expectBytes, h, 8);
                        for (const uint64_t x : h)
                            if (std::find(hits[i].begin(), hits[i].end(), x) == hits[i].end() && hits[i].size() < 8)
                                hits[i].push_back(x);
                    }
            });
    for (size_t i = 0; i < anchors().size(); ++i) {
        const auto& an = anchors()[i];
        if (!an.expect)
            continue;
        std::string list = "[";
        for (size_t k = 0; k < hits[i].size(); ++k)
            list += (k ? "," : "") + addr(static_cast<uintptr_t>(hits[i][k]));
        J j("signature");
        j.str("name", an.name).a("expectedRva", an.rva).raw("pattern", quoted(hex(an.expect, an.expectBytes)));
        j.raw("hitRvas", list + "]").u("hits", hits[i].size());
        j.raw("atExpected", std::find(hits[i].begin(), hits[i].end(), an.rva) != hits[i].end() ? "true" : "false");
        j.write();
    }
}

struct Catalog {
    std::string name;
    Area area{};
    std::vector<uint32_t> cols, vtables;
};
std::map<uint32_t, Catalog> catalog; // by TypeDescriptor RVA

void rttiCatalog() {
    // 1) TypeDescriptor names (".?AV" / ".?AU") in the data sections that name something the scan looks for.
    for (const auto& s : img.sections) {
        if (s.code() || catalog.size() >= 8000)
            continue;
        eachChunk(s, 256, [&](const uint8_t* b, size_t n, uint32_t rva) {
            for (size_t i = 16; i + 6 < n && catalog.size() < 8000; ++i) {
                if (b[i] != '.' || b[i + 1] != '?' || b[i + 2] != 'A' || (b[i + 3] != 'V' && b[i + 3] != 'U'))
                    continue;
                if ((rva + i - 16) % 8)
                    continue; // a TypeDescriptor is 8-aligned and its name is at +16
                size_t e = i;
                while (e < n && b[e] && e - i < 250)
                    ++e;
                if (e >= n || b[e])
                    continue;
                const std::string name = className(std::string_view(reinterpret_cast<const char*>(b + i), e - i));
                Area a{};
                if (!name.empty() && interesting(name, a)) {
                    Catalog& c = catalog[static_cast<uint32_t>(rva + i - 16)];
                    c.name = name;
                    c.area = a;
                }
            }
        });
    }
    if (catalog.empty() || stopping())
        return;
    // 2) CompleteObjectLocators (signature 1, a catalogued TypeDescriptor, their own RVA) in read-only data.
    std::unordered_map<uint32_t, uint32_t> colToTd;
    for (const auto& s : img.sections) {
        if (s.code() || s.writable())
            continue;
        eachChunk(s, 24, [&](const uint8_t* b, size_t n, uint32_t rva) {
            for (size_t i = 0; i + 24 <= n; i += 4) {
                uint32_t c[6];
                std::memcpy(c, b + i, sizeof(c));
                if (c[0] != 1 || c[5] != rva + i)
                    continue;
                if (const auto it = catalog.find(c[3]); it != catalog.end() && it->second.cols.size() < 16) {
                    if (std::find(it->second.cols.begin(), it->second.cols.end(), c[5]) == it->second.cols.end()) {
                        it->second.cols.push_back(c[5]);
                        colToTd[c[5]] = c[3];
                    }
                }
            }
        });
    }
    // 3) vtables: a pointer to one of those locators, the vtable starts right after it.
    if (!colToTd.empty())
        for (const auto& s : img.sections) {
            if (s.code() || s.writable())
                continue;
            eachChunk(s, 8, [&](const uint8_t* b, size_t n, uint32_t rva) {
                for (size_t i = 0; i + 8 <= n; i += 8) {
                    uint64_t v = 0;
                    std::memcpy(&v, b + i, 8);
                    if (!img.in(static_cast<uintptr_t>(v)))
                        continue;
                    if (const auto it = colToTd.find(static_cast<uint32_t>(v - img.base)); it != colToTd.end()) {
                        auto& vts = catalog[it->second].vtables;
                        const uint32_t vt = static_cast<uint32_t>(rva + i + 8);
                        if (vts.size() < 16 && std::find(vts.begin(), vts.end(), vt) == vts.end())
                            vts.push_back(vt);
                    }
                }
            });
        }
    for (const auto& [td, c] : catalog) {
        J j("rttiClass");
        j.str("name", c.name).str("area", areaName(c.area)).a("typeDescriptorRva", td);
        std::string cols = "[", vts = "[";
        for (size_t i = 0; i < c.cols.size(); ++i)
            cols += (i ? "," : "") + addr(c.cols[i]);
        for (size_t i = 0; i < c.vtables.size(); ++i)
            vts += (i ? "," : "") + addr(c.vtables[i]);
        j.raw("locatorRvas", cols + "]").raw("vtableRvas", vts + "]");
        if (!c.vtables.empty()) {
            std::string v = "[";
            for (int i = 0; i < 12; ++i)
                v += (i ? "," : "") + rvaText(static_cast<uintptr_t>(ptr(img.base + c.vtables[0] + i * 8)));
            j.raw("firstVfuncRvas", v + "]");
            j.raw("bases", names(rttiOf(img.base + c.vtables[0])));
        }
        j.write();
    }
}

// ---- phase C: the component registry (game_player.cpp's walk, read only) ----
struct Comp {
    uintptr_t obj = 0, vtable = 0, record = 0;
    uint32_t index = 0;
};
std::vector<Comp> comps;
uint32_t registryCount = 0;

bool census() {
    comps.clear();
    const uintptr_t table = ptr(img.base + 0x7a44320);
    int32_t count = 0;
    if (!table || !get(img.base + 0x7a44340, count) || count <= 0 || count > 0x100000)
        return false;
    registryCount = static_cast<uint32_t>(count);
    std::vector<uint8_t> b(static_cast<size_t>(limits.chunkEntries) * 16);
    for (uint32_t i = 0; i < registryCount;) {
        const uint32_t n = std::min(limits.chunkEntries, registryCount - i);
        forgetRegion();
        if (rd(table + static_cast<uintptr_t>(i) * 16, b.data(), static_cast<size_t>(n) * 16))
            for (uint32_t k = 0; k < n; ++k) {
                uint64_t obj = 0;
                uint32_t gen = 0;
                std::memcpy(&obj, b.data() + k * 16, 8);
                std::memcpy(&gen, b.data() + k * 16 + 8, 4);
                if (obj < 0x10000 || (obj & 7) || !gen || gen > 0xfff)
                    continue;
                uint32_t handle = 0;
                Comp c;
                c.obj = static_cast<uintptr_t>(obj);
                c.index = i + k;
                if (!get(c.obj + 0x14, handle) || handle != handleOf(gen, i + k))
                    continue;
                c.vtable = ptr(c.obj);
                c.record = ptr(c.obj + 8);
                if (img.in(c.vtable))
                    comps.push_back(c);
            }
        i += n;
        if (!breathe())
            return !comps.empty();
    }
    return true;
}

// ---- the session state ----
uintptr_t playerRecord = 0;
std::unordered_set<uintptr_t> dumpedObjects;
std::unordered_map<uintptr_t, std::vector<size_t>> byRecord; // only for records the scan follows
std::vector<uintptr_t> botRecords, spawnRecords, vehicleRecords;
struct SuitWatch {
    uintptr_t obj = 0, vtable = 0, record = 0;
    std::vector<uint8_t> last;
    uint32_t dumps = 0;
};
std::vector<SuitWatch> suitWatch;
uint32_t suitDumps = 0, vehicleDumps = 0;
std::unordered_map<uintptr_t, uint32_t> vehicleDumped; // record -> dumps (3 at most: each close approach)
std::unordered_set<uintptr_t> vehicleNear;
uintptr_t ropeComp = 0;
uint32_t ropeDumps = 0, censusCount = 0, hitEvents = 0, rigDumps = 0;
uint64_t lastLanded = 0, lastKnockouts = 0;
bool strikeSeen = false; // the first read only sets the baseline
bool havePlayerPos = false;
float playerPos[3] = {};
std::string summaryNotes;

bool transformAt(uintptr_t record, float m[16]) {
    const uintptr_t inst = ptr(record);
    return inst && rd(inst, m, 64) && actorTransform(m);
}
float dist2(const float a[3], const float b[3]) {
    const float x = a[0] - b[0], y = a[1] - b[1], z = a[2] - b[2];
    return x * x + y * y + z * z;
}

void classify() {
    std::unordered_map<uintptr_t, uint32_t> hist;
    for (const auto& c : comps)
        ++hist[c.vtable];
    ++censusCount;
    const bool fullList = censusCount == 1 || censusCount % 10 == 0;
    {
        J j("census");
        j.u("registryCount", registryCount).u("components", comps.size()).u("classes", hist.size()).u("n", censusCount);
        if (fullList) {
            std::vector<std::pair<uintptr_t, uint32_t>> v(hist.begin(), hist.end());
            std::sort(v.begin(), v.end(), [](auto& a, auto& b) { return a.second > b.second; });
            std::string list = "[";
            for (size_t i = 0; i < v.size() && i < 1500; ++i) {
                const Rtti& r = rttiOf(v[i].first);
                list += (i ? "," : "") + std::string("{\"vtableRva\":") + rvaText(v[i].first) +
                        ",\"count\":" + std::to_string(v[i].second) + ",\"rtti\":" + names(r) + "}";
                if (i % 64 == 63 && !breathe())
                    break;
            }
            j.raw("histogram", list + "]");
        }
        j.write();
    }
    // Which records the scan follows: the player, bots (the enemies), and spawn/vehicle/seat classes.
    byRecord.clear();
    botRecords.clear();
    spawnRecords.clear();
    std::unordered_set<uintptr_t> bots, spawns;
    for (const auto& c : comps) {
        const Rtti& r = rttiOf(c.vtable);
        if (any(r, botClass))
            bots.insert(c.record);
        else if (any(r, spawnName))
            spawns.insert(c.record);
        if (!playerRecord && c.vtable == img.base + 0x38a93c8)
            playerRecord = c.record; // heroLocal's record when his XrData has none yet
    }
    for (size_t i = 0; i < comps.size(); ++i) {
        const uintptr_t rec = comps[i].record;
        if (rec == playerRecord || bots.count(rec) || spawns.count(rec))
            byRecord[rec].push_back(i);
    }
    botRecords.assign(bots.begin(), bots.end());
    spawnRecords.assign(spawns.begin(), spawns.end());
    // Dump mode (0.0.76): the suit / loadout components (any record) and the vehicles' records.
    {
        std::unordered_set<uintptr_t> seenSuit, cars;
        for (const auto& c : comps) {
            const Rtti& r = rttiOf(c.vtable);
            if (any(r, suitClass) && !seenSuit.count(c.obj) && seenSuit.size() < 24) {
                seenSuit.insert(c.obj);
                bool known = false;
                for (const auto& w : suitWatch)
                    known = known || w.obj == c.obj;
                if (!known)
                    suitWatch.push_back({c.obj, c.vtable, c.record, {}, 0});
            }
            if (any(r, vehicleClass))
                cars.insert(c.record);
        }
        // drop watched objects the census no longer has
        suitWatch.erase(std::remove_if(suitWatch.begin(), suitWatch.end(),
                                       [&](const SuitWatch& w) { return !seenSuit.count(w.obj); }),
                        suitWatch.end());
        vehicleRecords.assign(cars.begin(), cars.end());
    }
    // Nearest first, so the caps keep the ones that matter.
    auto nearest = [](std::vector<uintptr_t>& v) {
        if (!havePlayerPos)
            return;
        std::vector<std::pair<float, uintptr_t>> d;
        for (const uintptr_t r : v) {
            float m[16];
            d.push_back({transformAt(r, m) ? dist2(m + 12, playerPos) : 1e30f, r});
        }
        std::sort(d.begin(), d.end());
        v.clear();
        for (const auto& x : d)
            v.push_back(x.second);
    };
    nearest(botRecords);
    nearest(spawnRecords);
    if (botRecords.size() > limits.maxBots)
        botRecords.resize(limits.maxBots);
    if (spawnRecords.size() > 64)
        spawnRecords.resize(64);
    // Components of those records: names once, each object's first bytes once (offline offsets).
    static std::unordered_set<uintptr_t> described; // once per record, again on the full censuses
    auto describe = [&](const char* kind, uintptr_t rec, size_t dumpBytes) {
        const auto it = byRecord.find(rec);
        if (it == byRecord.end() || (described.count(rec) && !fullList) || described.size() > 4000)
            return;
        described.insert(rec);
        J j(kind);
        j.a("record", rec);
        float m[16];
        if (transformAt(rec, m))
            j.raw("position", floats(m + 12, 3));
        uint8_t recBytes[0xc0];
        if (!dumpedObjects.count(rec) && rd(rec, recBytes, sizeof(recBytes))) {
            j.raw("recordDump", out.blob(recBytes, sizeof(recBytes)));
            dumpedObjects.insert(rec);
        }
        std::string list = "[";
        bool first = true;
        for (const size_t i : it->second) {
            const Comp& c = comps[i];
            list += (first ? "" : ",") + std::string("{\"obj\":") + addr(c.obj) + ",\"index\":" + std::to_string(c.index) +
                    ",\"vtableRva\":" + rvaText(c.vtable) + ",\"rtti\":" + names(rttiOf(c.vtable));
            if (!dumpedObjects.count(c.obj) && dumpedObjects.size() < 6000) {
                list += ",\"dump\":" + out.dump(c.obj, dumpBytes);
                dumpedObjects.insert(c.obj);
            }
            list += "}";
            first = false;
        }
        j.raw("components", list + "]").write();
    };
    if (playerRecord)
        describe("playerComponents", playerRecord, 0x200);
    for (const uintptr_t r : botRecords)
        describe("botComponents", r, 0x200);
    for (const uintptr_t r : spawnRecords)
        describe("spawnComponents", r, 0x100);
    // The player's rope manager (native_webs.hpp: HeroRopeManager, 16 slots of 0x7c8).
    ropeComp = 0;
    if (const auto it = byRecord.find(playerRecord); it != byRecord.end())
        for (const size_t i : it->second)
            if (comps[i].vtable == img.base + 0x38b3df8)
                ropeComp = comps[i].obj;
}

// ---- phase D: snapshots ----
uintptr_t healthOf(uintptr_t rec) {
    const auto it = byRecord.find(rec);
    if (it != byRecord.end())
        for (const size_t i : it->second)
            if (any(rttiOf(comps[i].vtable), healthName))
                return comps[i].obj;
    return 0;
}

struct Pending {
    uint64_t due;
    uintptr_t target;
    uint32_t event, step;
};
std::vector<Pending> pending;

void dumpTarget(uintptr_t target, uint32_t event, uint32_t step) {
    J j("hitTarget");
    j.u("event", event).u("afterMs", step == 0 ? 0 : step == 1 ? 600 : 1500).a("record", target);
    float m[16];
    if (transformAt(target, m))
        j.raw("position", floats(m + 12, 3));
    j.raw("recordDump", out.dump(target, 0xc0));
    // Its health / mover components: from the census when it is a known bot, else a registry search by record.
    std::string list = "[";
    bool first = true;
    for (const auto& c : comps)
        if (c.record == target) {
            const Rtti& r = rttiOf(c.vtable);
            if (!any(r, healthName) && !any(r, moverName) && !any(r, botClass) && !any(r, attachName))
                continue;
            float hp = 0;
            list += (first ? "" : ",") + std::string("{\"obj\":") + addr(c.obj) + ",\"rtti\":" + names(r) +
                    ",\"f88\":" + (get(c.obj + 0x88, hp) ? number(hp) : "null") + ",\"dump\":" + out.dump(c.obj, 0x200) + "}";
            first = false;
        }
    j.raw("components", list + "]").write();
}

void pollStrike() {
    Export* e = exported("SpidyStrikeData");
    uint8_t s[strikeBytes];
    if (!e || !rd(e->at, s, sizeof(s)))
        return;
    uint64_t landed = 0, knockouts = 0, target = 0, cueTarget = 0;
    std::memcpy(&landed, s + strikeLandedAt, 8);
    std::memcpy(&knockouts, s + strikeKnockoutsAt, 8);
    std::memcpy(&target, s + strikeTargetAt, 8);
    std::memcpy(&cueTarget, s + strikeCueTargetAt, 8);
    const bool hit = strikeSeen && landed != lastLanded;
    const bool ko = strikeSeen && knockouts != lastKnockouts;
    strikeSeen = true;
    if ((hit || ko) && hitEvents < 200) {
        ++hitEvents;
        float point[3], dmg = 0;
        std::memcpy(point, s + strikeLastPointAt, 12);
        std::memcpy(&dmg, s + strikeLastDamageAt, 4);
        J j("hit");
        j.u("event", hitEvents).u("landed", landed).u("knockouts", knockouts).a("target", static_cast<uintptr_t>(target));
        j.a("cueTarget", static_cast<uintptr_t>(cueTarget)).raw("point", floats(point, 3)).num("hisDamage", dmg);
        j.raw("knockout", ko ? "true" : "false").raw("strikeDump", out.blob(s, sizeof(s))).write();
        if (target) {
            dumpTarget(static_cast<uintptr_t>(target), hitEvents, 0);
            pending.push_back({nowMs() + 600, static_cast<uintptr_t>(target), hitEvents, 1});
            pending.push_back({nowMs() + 1500, static_cast<uintptr_t>(target), hitEvents, 2});
        }
    }
    lastLanded = landed;
    lastKnockouts = knockouts;
    for (size_t i = 0; i < pending.size();)
        if (nowMs() >= pending[i].due) {
            const Pending p = pending[i];
            pending.erase(pending.begin() + static_cast<std::ptrdiff_t>(i));
            dumpTarget(p.target, p.event, p.step);
        } else {
            ++i;
        }
}

// Raw copies of his data exports when they change (Swing/Rope/Grab/Throw/... hold the anchors and targets).
bool exportsChanged(bool record) {
    bool webs = false;
    for (auto& e : exportsSeen) {
        if (!e.at || !std::strcmp(e.name, "SpidyBodyPoses") || !std::strcmp(e.name, "SpidyXrData"))
            continue;
        auto b = exportBytes(e);
        if (b.empty())
            continue;
        // the first 24 bytes are the header and a sequence; a change past them is a change
        const bool changed = b.size() != e.last.size() ||
                             (b.size() > 24 && std::memcmp(b.data() + 24, e.last.data() + 24, b.size() - 24));
        if (changed && (!std::strcmp(e.name, "SpidyRopeData") || !std::strcmp(e.name, "SpidySwingData") ||
                        !std::strcmp(e.name, "SpidyGrabData")))
            webs = true;
        if (changed && record && e.dumps < 600) {
            ++e.dumps;
            J j("hisData");
            j.str("name", e.name).raw("dump", out.blob(b.data(), b.size())).write();
        }
        e.last = std::move(b);
    }
    return webs;
}

std::vector<int> jointIndex; // jointNames() -> rig index (-1 = not found)
uintptr_t lastRig = 0;
void readRig(uintptr_t rig) {
    if (!rig || rig == lastRig || rigDumps >= 8)
        return;
    lastRig = rig;
    ++rigDumps;
    uint16_t count = 0;
    const uintptr_t joints = ptr(rig + 8);
    if (!get(rig + 2, count) || !count || count > 1024 || !joints)
        return;
    std::vector<uint8_t> t(static_cast<size_t>(count) * 16);
    if (!rd(joints, t.data(), t.size()))
        return;
    jointIndex.assign(jointNames().size(), -1);
    std::string hashes = "[", parents = "[";
    for (uint16_t i = 0; i < count; ++i) {
        int16_t parent = 0;
        uint32_t hash = 0;
        std::memcpy(&parent, t.data() + i * 16, 2);
        std::memcpy(&hash, t.data() + i * 16 + 8, 4);
        hashes += (i ? "," : "") + addr(hash);
        parents += (i ? "," : "") + std::to_string(parent);
        for (size_t k = 0; k < jointNames().size(); ++k)
            if (jointHash(jointNames()[k]) == hash)
                jointIndex[k] = i;
    }
    std::string named = "{";
    for (size_t k = 0; k < jointNames().size(); ++k)
        named += (k ? "," : "") + quoted(jointNames()[k]) + ":" + std::to_string(jointIndex[k]);
    J j("rig");
    j.a("rig", rig).u("joints", count).raw("hashes", hashes + "]").raw("parents", parents + "]").raw("named", named + "}");
    j.raw("restDump", out.dump(ptr(rig + 0x18), std::min<size_t>(count, 256) * 64)).write();
}

void snapshot(bool websChanged) {
    J j("snapshot");
    // His VR data: head, hands and the player record (a copy of his export; his seqlock is not taken).
    if (Export* x = exported("SpidyXrData")) {
        uint8_t b[xrDataBytes];
        if (rd(x->at, b, sizeof(b))) {
            float head[16], hands[32];
            uint64_t rec = 0;
            std::memcpy(head, b + xrHeadAt, sizeof(head));
            std::memcpy(hands, b + xrHandsAt, sizeof(hands));
            std::memcpy(&rec, b + xrPlayerRecordAt, 8);
            j.raw("hisHead", floats(head, 16)).raw("hisHands", floats(hands, 32)).a("hisPlayerRecord", static_cast<uintptr_t>(rec));
            if (rec && rec != playerRecord) {
                playerRecord = static_cast<uintptr_t>(rec);
                summaryNotes += "player record from his SpidyXrData\n";
            }
        }
    }
    if (Export* m = exported("SpidyMotionData")) {
        uint8_t b[motionDataBytes];
        if (rd(m->at, b, sizeof(b))) {
            float pos[3];
            std::memcpy(pos, b + motionPositionAt, 12);
            j.raw("hisMotionPosition", floats(pos, 3)).u("hisGrounded", b[motionGroundedAt]);
        }
    }
    float m[16];
    if (playerRecord && transformAt(playerRecord, m)) {
        j.raw("playerTransform", floats(m, 16));
        std::memcpy(playerPos, m + 12, 12);
        havePlayerPos = true;
    }
    // HeroHealth (game_player.cpp heroHealthTable, HP float at +0x88).
    if (const auto it = byRecord.find(playerRecord); it != byRecord.end())
        for (const size_t i : it->second)
            if (comps[i].vtable == img.base + 0x38a6df0) {
                float hp = 0;
                if (get(comps[i].obj + 0x88, hp))
                    j.num("playerHp88", hp);
            }
    // Bots: position, a health candidate, alive guesses come from comparing these over time offline.
    std::string bots = "[";
    bool first = true;
    for (const uintptr_t r : botRecords) {
        float bm[16];
        const bool at = transformAt(r, bm);
        const uintptr_t h = healthOf(r);
        float hp = 0;
        const bool hasHp = h && get(h + 0x88, hp);
        bots += (first ? "" : ",") + std::string("{\"record\":") + addr(r) + ",\"position\":" + (at ? floats(bm + 12, 3) : "null") +
                ",\"distance\":" + (at && havePlayerPos ? number(std::sqrt(dist2(bm + 12, playerPos))) : "null") +
                ",\"health\":" + (h ? addr(h) : "null") + ",\"hp88\":" + (hasHp ? number(hp) : "null") + "}";
        first = false;
    }
    j.raw("bots", bots + "]");
    std::string spawns = "[";
    first = true;
    for (size_t i = 0; i < spawnRecords.size() && i < 16; ++i) {
        float sm[16];
        if (!transformAt(spawnRecords[i], sm))
            continue;
        spawns += (first ? "" : ",") + std::string("{\"record\":") + addr(spawnRecords[i]) + ",\"position\":" + floats(sm + 12, 3) + "}";
        first = false;
    }
    j.raw("spawnClasses", spawns + "]");
    // The rope manager: the whole component when his web data changed (the 16 slots' targets are in there).
    if (ropeComp && websChanged && ropeDumps < limits.maxRopeDumps) {
        ++ropeDumps;
        j.a("ropeManager", ropeComp).raw("ropeManagerDump", out.dump(ropeComp, 16 * 0x7c8 + 0x400));
    }
    // The hands: the joints as the game posed them (his SpidyBodyPoses copy) and the game's own buffer.
    if (Export* p = exported("SpidyBodyPoses")) {
        uint8_t h[posesGameAt];
        if (rd(p->at, h, sizeof(h))) {
            uint32_t joints = 0;
            uint64_t rig = 0, inst = 0, captured = 0;
            std::memcpy(&joints, h + posesJointsAt, 4);
            std::memcpy(&rig, h + posesRigAt, 8);
            std::memcpy(&inst, h + posesInstanceAt, 8);
            std::memcpy(&captured, h + posesCapturedAt, 8);
            readRig(static_cast<uintptr_t>(rig));
            j.u("posesJoints", joints).a("posesRig", static_cast<uintptr_t>(rig)).a("posesInstance", static_cast<uintptr_t>(inst)).u("posesCaptured", captured);
            const uintptr_t live = inst ? ptr(static_cast<uintptr_t>(inst) + 0xd8) : 0;
            std::string hand = "{";
            bool f = true;
            for (size_t k = 0; k < jointIndex.size(); ++k) {
                const int ix = jointIndex[k];
                if (ix < 0 || static_cast<uint32_t>(ix) >= std::min<uint32_t>(joints, posesMaxJoints))
                    continue;
                if (jointNames()[k].find("_") == std::string::npos)
                    continue; // the hands and arms only, per snapshot
                float a[16], g[16];
                const bool ha = rd(p->at + posesGameAt + static_cast<size_t>(ix) * 64, a, 64);
                const bool hg = live && rd(live + static_cast<size_t>(ix) * 64, g, 64);
                hand += (f ? "" : ",") + quoted(jointNames()[k]) + ":{\"his\":" + (ha ? floats(a, 16) : "null") +
                        ",\"game\":" + (hg ? floats(g, 16) : "null") + "}";
                f = false;
            }
            j.a("poseBuffer", live).raw("handJoints", hand + "}");
        }
    } else if (playerRecord && rigDumps < 1) {
        // No SpidyBodyPoses: the player's instance and its pose buffer, once, raw.
        ++rigDumps;
        const uintptr_t inst = ptr(playerRecord);
        j.a("instance", inst).raw("instanceDump", out.dump(inst, 0x200));
        j.raw("poseBufferDump", out.dump(ptr(inst + 0xd8), 128 * 64));
    }
    j.write();
}

// ---- dump mode (0.0.76) ----
// Suits: each watched component's bytes per snapshot; on a change, the bytes before and after
// (change a suit in the game's Suits menu: the "suitChange" lines are the before/after pairs).
void watchSuits() {
    for (auto& w : suitWatch) {
        std::vector<uint8_t> b(limits.suitWatchBytes);
        if (!rd(w.obj, b.data(), b.size())) {
            // a shorter object near the end of its region: the first 0x100 bytes
            b.resize(0x100);
            if (!rd(w.obj, b.data(), b.size()))
                continue;
        }
        if (w.last.empty()) {
            if (suitDumps < limits.maxSuitDumps) {
                ++suitDumps;
                ++w.dumps;
                J j("suitBaseline");
                j.a("obj", w.obj).a("record", w.record).raw("vtableRva", rvaText(w.vtable)).raw("rtti", names(rttiOf(w.vtable)));
                j.raw("dump", out.blob(b.data(), b.size())).write();
            }
        } else if (b.size() == w.last.size() && std::memcmp(b.data(), w.last.data(), b.size())) {
            uint32_t changed = 0, firstAt = 0xffffffff;
            for (size_t i = 0; i < b.size(); ++i)
                if (b[i] != w.last[i]) {
                    ++changed;
                    firstAt = std::min<uint32_t>(firstAt, static_cast<uint32_t>(i));
                }
            if (suitDumps < limits.maxSuitDumps && w.dumps < limits.maxSuitDumpsEach) {
                ++suitDumps;
                ++w.dumps;
                J j("suitChange");
                j.a("obj", w.obj).a("record", w.record).raw("vtableRva", rvaText(w.vtable)).raw("rtti", names(rttiOf(w.vtable)));
                j.u("changedBytes", changed).u("firstChangedAt", firstAt);
                j.raw("before", out.blob(w.last.data(), w.last.size())).raw("after", out.blob(b.data(), b.size())).write();
            }
        }
        w.last = std::move(b);
    }
}
// Vehicles: when the player comes within vehicleNearMeters of a vehicle record, that record's components
// (names and their first 0x300 bytes); again on a later approach, at most 3 times per record.
void watchVehicles() {
    if (!havePlayerPos || vehicleDumps >= limits.maxVehicleDumps)
        return;
    const float near2 = limits.vehicleNearMeters * limits.vehicleNearMeters;
    for (const uintptr_t rec : vehicleRecords) {
        float m[16];
        const bool close = transformAt(rec, m) && dist2(m + 12, playerPos) < near2;
        if (!close) {
            vehicleNear.erase(rec);
            continue;
        }
        if (vehicleNear.count(rec) || vehicleDumped[rec] >= 3 || vehicleDumps >= limits.maxVehicleDumps)
            continue;
        vehicleNear.insert(rec);
        ++vehicleDumped[rec];
        ++vehicleDumps;
        J j("vehicleComponents");
        j.a("record", rec).raw("position", floats(m + 12, 3)).num("distance", std::sqrt(dist2(m + 12, playerPos)));
        j.raw("recordDump", out.dump(rec, 0xc0));
        std::string list = "[";
        bool first = true;
        for (const auto& c : comps)
            if (c.record == rec) {
                list += (first ? "" : ",") + std::string("{\"obj\":") + addr(c.obj) + ",\"vtableRva\":" + rvaText(c.vtable) +
                        ",\"rtti\":" + names(rttiOf(c.vtable)) + ",\"dump\":" + out.dump(c.obj, 0x300) + "}";
                first = false;
            }
        j.raw("components", list + "]").write();
    }
}

void summary(const char* why, uint64_t began) {
    std::string s;
    char b[256];
    std::snprintf(b, sizeof(b), "HERO VR SM1 research scan (sm1scan). Made by Agent Mulder.\r\nEnded: %s after %llu s\r\n",
                  why, static_cast<unsigned long long>((nowMs() - began) / 1000));
    s += b;
    s += "Every value in the .jsonl is a CANDIDATE: read-only copies of game memory and of SpidyVR's exported data.\r\n";
    std::snprintf(b, sizeof(b), "exe base 0x%llx, image 0x%x bytes, stamp 0x%x, version %s\r\n",
                  static_cast<unsigned long long>(img.base), img.size, img.stamp, fileVersion().c_str());
    s += b;
    uint32_t match = 0, expect = 0;
    for (const auto& an : anchors())
        if (an.expect) {
            ++expect;
            uint8_t x[64];
            if (rd(img.base + an.rva, x, an.expectBytes) && !std::memcmp(x, an.expect, an.expectBytes))
                ++match;
        }
    std::snprintf(b, sizeof(b), "his entry bytes matched: %u of %u; RTTI classes catalogued: %zu\r\n", match, expect, catalog.size());
    s += b;
    std::snprintf(b, sizeof(b), "registry %u entries, %zu components, %u censuses; bots followed %zu; spawn-class records %zu\r\n",
                  registryCount, comps.size(), censusCount, botRecords.size(), spawnRecords.size());
    s += b;
    std::snprintf(b, sizeof(b), "dump mode: suit/loadout components watched %zu, suit dumps %u; vehicle records %zu, vehicle dumps %u\r\n",
                  suitWatch.size(), suitDumps, vehicleRecords.size(), vehicleDumps);
    s += b;
    std::snprintf(b, sizeof(b), "player record 0x%llx; hits recorded %u; rope dumps %u; lines %llu; bytes %llu\r\n",
                  static_cast<unsigned long long>(playerRecord), hitEvents, ropeDumps,
                  static_cast<unsigned long long>(out.lines), static_cast<unsigned long long>(out.bytes));
    s += b;
    s += summaryNotes;
    HANDLE h = CreateFileW((out.stem + L".txt").c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h != INVALID_HANDLE_VALUE) {
        Out::put(h, s.data(), s.size());
        CloseHandle(h);
    }
}

void run() {
    Sleep(limits.startDelayMs);
    while (!on())
        Sleep(2000);
    std::wstring dir;
    if (!readImage() || !openOut(dir)) {
        say("sm1scan: could not read the game image or open the scan folder; the scan is off");
        return;
    }
    const uint64_t began = nowMs();
    say("sm1scan: started, read-only, writing %s.jsonl", narrow(out.stem).c_str());
    findExports();
    writeImage();
    writeAnchors();
    // The static passes have their own cap (the run cap starts when the player is found).
    deadline = nowMs() + 10 * 60 * 1000;
    signatureScan();
    rttiCatalog();
    J("staticDone").write();
    say("sm1scan: static pass done (%zu RTTI classes)", catalog.size());
    // Phase B: wait for the player (his XrData record, or heroLocal in the registry), at most 20 minutes.
    deadline = nowMs() + 20 * 60 * 1000;
    uint64_t nextWaitCensus = 0;
    while (!stopping() && !playerRecord) {
        if (Export* x = exported("SpidyXrData")) {
            uint64_t rec = 0;
            if (get(x->at + xrPlayerRecordAt, rec) && rec)
                playerRecord = static_cast<uintptr_t>(rec);
        }
        // 0.0.76: the registry at most every 30 s while waiting (his XrData usually names the player first)
        if (!playerRecord && nowMs() >= nextWaitCensus) {
            nextWaitCensus = nowMs() + 30000;
            if (census())
                classify();
        }
        if (!playerRecord) {
            for (int i = 0; i < 50 && on(); ++i)
                Sleep(100);
            breathe();
        }
    }
    if (!playerRecord) {
        summary("the player was never found", began);
        say("sm1scan: the player was never found; the scan ended");
        return;
    }
    J("playerFound").a("record", playerRecord).write();
    say("sm1scan: player found; snapshots for %u minutes", limits.runMs / 60000);
    deadline = nowMs() + limits.runMs;
    uint64_t nextCensus = 0, nextSnap = 0;
    while (!stopping()) {
        forgetRegion();
        const uint64_t t = nowMs();
        if (t >= nextCensus) {
            if (census())
                classify();
            nextCensus = nowMs() + limits.censusMs;
        }
        pollStrike();
        const bool webs = exportsChanged(t >= nextSnap);
        if (t >= nextSnap || (webs && ropeDumps < limits.maxRopeDumps)) {
            snapshot(webs);
            watchSuits();
            watchVehicles();
            nextSnap = t + limits.snapshotMs;
        }
        Sleep(limits.eventPollMs);
        if (!breathe())
            break;
    }
    summary(out.full ? "the size cap" : "the time cap", began);
    say("sm1scan: finished (%llu lines, %llu bytes)", static_cast<unsigned long long>(out.lines), static_cast<unsigned long long>(out.bytes));
}

// The outer guard: nothing in here may need unwinding (MSVC), so the C++ part is its own function.
void runGuarded(bool& faulted) {
    try {
        run();
    } catch (...) {
        say("sm1scan: an exception ended the scan");
    }
    faulted = false;
}
DWORD WINAPI scanThread(void*) {
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_LOWEST);
    bool faulted = true;
    SCAN_TRY { runGuarded(faulted); }
    SCAN_EXCEPT { faulted = true; }
    if (faulted)
        say("sm1scan: a fault ended the scan; nothing else is affected");
    out.close();
    return 0;
}
} // namespace

void start(const Feed& f) {
    if (started.exchange(true))
        return;
    feed = f;
    HANDLE h = CreateThread(nullptr, 0, scanThread, nullptr, 0, nullptr);
    if (h)
        CloseHandle(h);
    else
        say("sm1scan: the scan thread did not start; the scan is off");
}
} // namespace spidy::sm1::scan
