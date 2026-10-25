// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
//
// Update 33: SHADOW DISTANCE (see native_shadow_distance.hpp and BUILD-SM2.md section 43).
// Update 34 (section 44): EnvLightingDef objects first; only heap (private read-write) or
// writable module data is ever written (Update 33's pointer-follow and module scan could take
// a code or .rdata constant and VirtualProtect it: run33 wrote 0x2ce2b2c in .text); growth
// only; each trial checked against the box it matched; boxes before/after every trial kept.
//
// Search order (read only; every read goes through sehRead):
//   1. code: data the game's sun-cascade code reads. Around each call site the boxes come
//      from (learned live this session, plus run32's chains for this game build, used only
//      if they fall in executable code) the RIP-relative operands (ModRM mod 00 rm 101) are
//      decoded; each global's +-1 KiB is scanned, and 64 KiB behind it if it holds a pointer.
//   2. module: the game module's non-executable sections.
//   3. heap: committed private read-write memory (no guard, write-combined or uncached
//      pages; regions over 512 MiB skipped; 2 GiB and 90 s at most, a 1 ms rest every 2 MiB).
// A hit is a float or double equal (5e-4) to a live box size, half, double or depth, or
// (code and module only) three ascending values in the boxes' proportions. Hits are read
// again 8 times 0.5 s apart and kept only if unchanged. Results are candidates, labelled so.
//
// A change (Update 34) first writes (compare-exchange, SEH) a trial growth of 1.5, 2.5 or
// 1.3 times and watches the boxes for 0.8-2 s: the box the candidate matched (or, for an
// EnvLightingDef, a third of all boxes) must show up grown, where no such box was before
// (shadow_distance::verifyMatched / verifyAll); then the asked growth is written. Otherwise
// it is put back and the next is tried (all EnvLightingDef, each alone, 24 single values,
// 8 groups of a kind; the next press goes on). Boxes gone put it back with the reason.
#include "spidy/native_shadow_distance.hpp"

#include <windows.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

namespace sd = spidy::shadow_distance;

extern "C" {
// Update 33: tools/shadow_distance.py reads it (seqlock: sequence odd while written).
__declspec(dllexport) sd::Data SpidyShadowDistanceData;
}

namespace spidy::native_shadow_distance {
namespace {

std::atomic<bool> running{false}, stopping{false}, restart{false}, watchOn{false};
std::atomic<uint32_t> packedNow{sd::pack(sd::idle, sd::none)};
std::atomic<int> want{0};
std::atomic<float> gameReachA{0.f}, liveReachA{0.f};
HANDLE worker{};
SRWLOCK watchLock = SRWLOCK_INIT;
sd::Watch watch;                    // under watchLock
std::atomic<uint32_t> learned[4]{}; // call sites sun boxes came from
std::atomic<uint64_t> budgetWindow{0};
std::atomic<uint32_t> budgetUsed{0};
// Return sites around the sun cascade uploads seen in run32's ORTHO CAPTURE chains
// (this game build). Only a starting point: used when they are in executable code.
constexpr uint32_t run32Sites[] = {0x2ccc294, 0x2abe8dd, 0x2c904aa, 0x2a8c76e, 0x2e01533,
                                   0x2e01a9c, 0x2c960c9, 0x2b0912e, 0x2c92d72};

void say(const char* line) {
    char buf[300];
    std::snprintf(buf, sizeof buf, "SHADOW DISTANCE: %s\n", line);
    OutputDebugStringA(buf);
}

// ---- SEH helpers: POD only (MSVC C2712) ----
bool sehRead(const void* p, void* out, size_t n) {
    __try {
        if (!p || reinterpret_cast<uintptr_t>(p) < 0x10000)
            return false;
        std::memcpy(out, p, n);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}
// 1 swapped, 0 something else was there (seen), -1 faulted.
int sehSwap32(void* p, uint32_t expect, uint32_t value, uint32_t* seen) {
    __try {
        const LONG old = InterlockedCompareExchange(static_cast<volatile LONG*>(p), static_cast<LONG>(value),
                                                    static_cast<LONG>(expect));
        *seen = static_cast<uint32_t>(old);
        return static_cast<uint32_t>(old) == expect ? 1 : 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return -1;
    }
}
int sehSwap64(void* p, uint64_t expect, uint64_t value, uint64_t* seen) {
    __try {
        const LONG64 old = InterlockedCompareExchange64(static_cast<volatile LONG64*>(p),
                                                        static_cast<LONG64>(value), static_cast<LONG64>(expect));
        *seen = static_cast<uint64_t>(old);
        return static_cast<uint64_t>(old) == expect ? 1 : 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return -1;
    }
}

uintptr_t moduleBase() { return reinterpret_cast<uintptr_t>(GetModuleHandleW(L"Spider-Man2.exe")); }

// ---- the exported record (seqlock: odd while written) ----
void publish(const std::vector<struct Hit>* hits, int active);
void event(uint32_t kind, uint32_t code, float a, float b);

uint64_t nowMs() { return GetTickCount64(); }

void setState(uint32_t state, uint32_t reason) {
    const uint32_t p = sd::pack(state, reason);
    if (packedNow.exchange(p) != p) {
        char line[160];
        std::snprintf(line, sizeof line, "state %u reason %u (%s)", state, reason,
                      reason ? sd::reasonText(reason) : "-");
        say(line);
        event(1, p, 0, 0);
    }
    if (!sd::sliderMoves(p)) // Update 34.2: a value set while searching is kept
        want = 0;
}

// The watch, copied under the shared lock (the render thread only try-locks).
sd::Watch snapshot() {
    AcquireSRWLockShared(&watchLock);
    sd::Watch w = watch;
    ReleaseSRWLockShared(&watchLock);
    return w;
}

struct Hit {
    uintptr_t addr{};
    uint32_t rva{UINT32_MAX};
    uint8_t origin{}, form{}, target{}, outcome{};
    uint16_t stride{}, confirms{};
    bool followed{};
    double orig[3]{};
    uint64_t bits[3]{};    // the game's bits
    uint64_t written[3]{}; // what is in memory now from us (== bits when not written)
    double err{};
    unsigned count() const { return form == sd::envPair ? 2u : form >= sd::triple32 ? 3u : 1u; }
    bool wide() const { return form == sd::f64 || form == sd::triple64; }
    uintptr_t at(unsigned k) const { return addr + static_cast<uintptr_t>(k) * stride; }
};

std::vector<Hit> hits;  // the worker's own
int activeHit = -1;     // index of the hit in use, or -1
std::vector<int> activeGroup; // the hits written together (a group trial)
float H0 = 0;           // the game's reach (widest sun box half) when the change began
float appliedScale = 1;
uint64_t scanned = 0, regions = 0, raw = 0;
uint32_t trials = 0;
uintptr_t scanFloor = 0; // size matches below it were found in the last (overlapping) piece
// Update 34: EnvLightingDef objects found in the heap (each an envPair hit), kept apart from
// the size matches so the 12288-hit cap never drops them; their game values for the record.
constexpr size_t maxEnvHits = 64;
std::vector<Hit> envHits;
sd::Env envRecord[sd::maxEnv]{};
uint32_t envFound = 0;
sd::Trial trialRing[sd::maxTrials]{};
uint32_t trialN = 0;

sd::Event ring[sd::maxEvents];
uint32_t ringN = 0;
void event(uint32_t kind, uint32_t code, float a, float b) {
    ring[ringN % sd::maxEvents] = sd::Event{nowMs(), kind, code, a, b};
    ++ringN;
}

void publish(const std::vector<Hit>* list, int active) {
    sd::Data& d = SpidyShadowDistanceData;
    InterlockedIncrement64(&d.sequence);
    d.magic = sd::dataMagic;
    d.version = sd::dataVersion;
    d.bytes = sizeof(sd::Data);
    d.packed = packedNow.load();
    d.gameReach = gameReachA.load();
    d.liveReach = liveReachA.load();
    d.wantCars = static_cast<float>(want.load());
    d.scale = appliedScale;
    const sd::Watch w = snapshot();
    d.sizes = w.n;
    d.boxes = w.boxes;
    for (unsigned i = 0; i < sd::maxSeen; ++i) {
        d.sizeHalf[i] = i < w.n ? w.kinds[i].half : 0.f;
        d.sizeDepth[i] = i < w.n ? w.kinds[i].depth : 0.f;
    }
    for (unsigned i = 0; i < 4; ++i)
        d.sites[i] = learned[i].load();
    d.scannedBytes = scanned;
    d.regions = regions;
    d.rawHits = raw;
    d.trials = trials;
    d.active = active >= 0 ? static_cast<uint32_t>(active) : UINT32_MAX;
    if (list) {
        d.stable = static_cast<uint32_t>(list->size());
        const size_t n = std::min<size_t>(list->size(), sd::maxCandidates);
        d.candidates = static_cast<uint32_t>(n);
        for (size_t i = 0; i < n; ++i) {
            const Hit& h = (*list)[i];
            sd::Candidate& c = d.candidate[i];
            c = {};
            c.address = h.addr;
            c.rva = h.rva;
            c.origin = h.origin;
            c.form = h.form;
            c.target = h.target;
            c.outcome = h.outcome;
            c.stride = h.stride;
            c.confirms = h.confirms;
            for (unsigned k = 0; k < 3; ++k)
                c.value[k] = static_cast<float>(h.orig[k]);
        }
    }
    d.events = ringN;
    std::memcpy(d.event, ring, sizeof ring);
    d.trialsLogged = trialN;
    std::memcpy(d.trial, trialRing, sizeof trialRing);
    d.envFound = envFound;
    std::memcpy(d.env, envRecord, sizeof envRecord);
    InterlockedIncrement64(&d.sequence);
}

// ---- the search ----
struct Sorted {
    std::vector<std::pair<double, int>> v; // target value, target index
    double lo{}, hi{};
};
Sorted sortedOf(const sd::Targets& t) {
    Sorted s;
    for (unsigned i = 0; i < t.n; ++i)
        s.v.push_back({t.value[i], static_cast<int>(i)});
    std::sort(s.v.begin(), s.v.end());
    s.lo = s.v.empty() ? 1 : s.v.front().first * (1 - sd::matchTolerance);
    s.hi = s.v.empty() ? 0 : s.v.back().first * (1 + sd::matchTolerance);
    return s;
}
int find(const Sorted& s, double x, double& err) {
    if (!(x >= s.lo && x <= s.hi))
        return -1;
    auto it = std::lower_bound(s.v.begin(), s.v.end(), std::make_pair(x / (1 + sd::matchTolerance), -1));
    for (; it != s.v.end() && it->first <= x / (1 - sd::matchTolerance); ++it)
        if (sd::within(x, it->first)) {
            err = std::fabs(x - it->first) / it->first;
            return it->second;
        }
    return -1;
}

constexpr size_t maxHits = 12288;

// Scans a copy of [at, at + n) (at 8-aligned).
// An EnvLightingDef at copy + i (i 8-aligned, the whole object in the copy).
void envAt(const uint8_t* copy, size_t i, uintptr_t at) {
    sd::EnvValues v;
    if (!sd::envLooks(copy + i, &v))
        return;
    for (const Hit& e : envHits)
        if (e.addr == at + i + sd::envRegLodRange)
            return; // seen in the overlap of the last piece
    if (envFound < sd::maxEnv)
        envRecord[envFound] = sd::Env{at + i, v.regLodRange, v.totalShadowRange, v.casterLower, v.casterUpper,
                                      v.regLodCount, v.farLodCount, {}};
    ++envFound;
    if (envHits.size() >= maxEnvHits)
        return;
    Hit h;
    h.addr = at + i + sd::envRegLodRange;
    h.origin = sd::fromEnv;
    h.form = sd::envPair;
    h.target = sd::targetAll;
    h.stride = static_cast<uint16_t>(sd::envTotalShadowRange - sd::envRegLodRange);
    const float f[2]{v.regLodRange, v.totalShadowRange};
    for (unsigned q = 0; q < 2; ++q) {
        h.orig[q] = f[q];
        uint32_t b;
        std::memcpy(&b, f + q, 4);
        h.bits[q] = h.written[q] = b;
    }
    envHits.push_back(h);
}

void scanCopy(const uint8_t* copy, size_t n, uintptr_t at, uint8_t origin, const sd::Targets& t,
              const Sorted& s, bool triples, bool env) {
    if (env)
        for (size_t i = 0; i + sd::envSize <= n; i += 8) {
            const uint8_t reg = copy[i + sd::envRegLodCount];
            if (reg < 1 || reg > 16 || copy[i + sd::envPerfModeOpts] > 1 || copy[i + sd::envEnabled] > 1)
                continue;
            envAt(copy, i, at);
        }
    const uintptr_t base = moduleBase();
    auto push = [&](Hit h) {
        if (hits.size() >= maxHits || h.addr < scanFloor)
            return;
        ++raw;
        h.origin = origin;
        h.rva = base && h.addr >= base && h.addr < base + (1ull << 31) ? static_cast<uint32_t>(h.addr - base)
                                                                     : UINT32_MAX;
        for (unsigned k = 0; k < 3; ++k)
            h.written[k] = h.bits[k];
        hits.push_back(h);
    };
    const size_t nf = n / 4;
    const auto* f = reinterpret_cast<const float*>(copy);
    for (size_t i = 0; i < nf; ++i) {
        double err = 0;
        const int m = find(s, f[i], err);
        if (m >= 0) {
            Hit h;
            h.addr = at + i * 4;
            h.form = sd::f32;
            h.target = t.kind[m];
            h.orig[0] = f[i];
            uint32_t b;
            std::memcpy(&b, f + i, 4);
            h.bits[0] = b;
            h.err = err;
            push(h);
        }
        if (triples)
            for (const unsigned stride : {4u, 8u, 16u}) {
                const size_t j = i + stride / 4, k = i + 2 * stride / 4;
                if (k >= nf || !(f[j] > f[i] * 1.01f) || !(f[k] > f[j] * 1.01f))
                    continue;
                if (!sd::tripleMatch(f[i], f[j], f[k], t))
                    continue;
                Hit h;
                h.addr = at + i * 4;
                h.form = sd::triple32;
                h.target = 0xff;
                h.stride = static_cast<uint16_t>(stride);
                const float v[3]{f[i], f[j], f[k]};
                for (unsigned q = 0; q < 3; ++q) {
                    h.orig[q] = v[q];
                    uint32_t b;
                    std::memcpy(&b, v + q, 4);
                    h.bits[q] = b;
                }
                h.err = 0;
                push(h);
            }
    }
    const size_t nd = n / 8;
    const auto* d = reinterpret_cast<const double*>(copy);
    for (size_t i = 0; i < nd; ++i) {
        double err = 0;
        const int m = find(s, d[i], err);
        if (m >= 0) {
            Hit h;
            h.addr = at + i * 8;
            h.form = sd::f64;
            h.target = t.kind[m];
            h.orig[0] = d[i];
            std::memcpy(&h.bits[0], d + i, 8);
            h.err = err;
            push(h);
        }
        if (triples)
            for (const unsigned stride : {8u, 16u}) {
                const size_t j = i + stride / 8, k = i + 2 * stride / 8;
                if (k >= nd || !(d[j] > d[i] * 1.01) || !(d[k] > d[j] * 1.01))
                    continue;
                if (!sd::tripleMatch(d[i], d[j], d[k], t))
                    continue;
                Hit h;
                h.addr = at + i * 8;
                h.form = sd::triple64;
                h.target = 0xff;
                h.stride = static_cast<uint16_t>(stride);
                for (unsigned q = 0; q < 3; ++q) {
                    h.orig[q] = d[i + q * stride / 8];
                    std::memcpy(&h.bits[q], d + i + q * stride / 8, 8);
                }
                push(h);
            }
    }
}

// Reads [a, a + n) in 1 MiB pieces and scans them; false when stopping.
bool scanRange(uintptr_t a, size_t n, uint8_t origin, const sd::Targets& t, const Sorted& s, bool triples,
               std::vector<uint8_t>& buf, uint64_t deadline, bool env = false) {
    a &= ~uintptr_t{7};
    size_t sinceRest = 0;
    uintptr_t seenTo = 0;
    while (n >= 16) {
        if (stopping.load() || nowMs() > deadline)
            return false;
        const size_t piece = std::min<size_t>(n & ~size_t{7}, 1u << 20);
        if (piece < 16)
            break;
        buf.resize(piece);
        if (sehRead(reinterpret_cast<const void*>(a), buf.data(), piece)) {
            scanFloor = seenTo;
            scanCopy(buf.data(), piece, a, origin, t, s, triples, env);
            scanned += piece;
        }
        seenTo = a + piece;
        // An EnvLightingDef across two pieces is seen in the next (they overlap by its size).
        const size_t step = env && piece == (1u << 20) && n > piece ? piece - sd::envSize : piece;
        a += step;
        n -= step;
        sinceRest += step;
        if (sinceRest >= (2u << 20)) {
            Sleep(1);
            sinceRest = 0;
        }
    }
    return true;
}

struct Section {
    uintptr_t start{};
    size_t size{};
    bool exec{}, write{};
};
std::vector<Section> sections() {
    std::vector<Section> out;
    const uintptr_t base = moduleBase();
    IMAGE_DOS_HEADER dh{};
    if (!base || !sehRead(reinterpret_cast<const void*>(base), &dh, sizeof dh) || dh.e_magic != IMAGE_DOS_SIGNATURE)
        return out;
    IMAGE_NT_HEADERS64 nt{};
    if (!sehRead(reinterpret_cast<const void*>(base + dh.e_lfanew), &nt, sizeof nt) ||
        nt.Signature != IMAGE_NT_SIGNATURE)
        return out;
    const uintptr_t first = base + dh.e_lfanew + offsetof(IMAGE_NT_HEADERS64, OptionalHeader) +
                            nt.FileHeader.SizeOfOptionalHeader;
    for (unsigned i = 0; i < nt.FileHeader.NumberOfSections && i < 96; ++i) {
        IMAGE_SECTION_HEADER sh{};
        if (!sehRead(reinterpret_cast<const void*>(first + i * sizeof sh), &sh, sizeof sh))
            break;
        out.push_back(Section{base + sh.VirtualAddress, sh.Misc.VirtualSize,
                              (sh.Characteristics & IMAGE_SCN_MEM_EXECUTE) != 0,
                              (sh.Characteristics & IMAGE_SCN_MEM_WRITE) != 0});
    }
    return out;
}
bool inExec(const std::vector<Section>& secs, uintptr_t a) {
    for (const auto& s : secs)
        if (s.exec && a >= s.start && a < s.start + s.size)
            return true;
    return false;
}
bool inData(const std::vector<Section>& secs, uintptr_t a) {
    for (const auto& s : secs)
        if (!s.exec && a >= s.start && a < s.start + s.size)
            return true;
    return false;
}
// The writable, non-executable module section holding a, or nullptr (code and .rdata are never searched).
const Section* writableSection(const std::vector<Section>& secs, uintptr_t a) {
    for (const auto& s : secs)
        if (!s.exec && s.write && a >= s.start && a < s.start + s.size)
            return &s;
    return nullptr;
}
std::vector<Section> moduleSections; // filled by scanAll; used again before every write
// Update 34: a value may be written only if it is in committed read-write memory that is not
// a guard, uncached or write-combined page, and either private memory outside the game module
// or in one of the module's writable data sections. Checked at search and before every write.
bool writableTarget(uintptr_t a, size_t n) {
    MEMORY_BASIC_INFORMATION mbi{};
    if (!VirtualQuery(reinterpret_cast<void*>(a), &mbi, sizeof mbi) || mbi.State != MEM_COMMIT)
        return false;
    const DWORD prot = mbi.Protect & 0xff;
    if (mbi.Protect & (PAGE_GUARD | PAGE_NOCACHE | PAGE_WRITECOMBINE))
        return false;
    if (a + n > reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize)
        return false;
    const uintptr_t base = moduleBase();
    if (base && a >= base && a < base + (1ull << 31)) {
        const Section* sec = writableSection(moduleSections, a);
        return sec && a + n <= sec->start + sec->size && (prot == PAGE_READWRITE || prot == PAGE_WRITECOPY);
    }
    return mbi.Type == MEM_PRIVATE && prot == PAGE_READWRITE;
}

// RIP-relative data operands in [from, to): SSE loads and arithmetic, mov and lea.
std::vector<uintptr_t> codeGlobals(const std::vector<Section>& secs, const std::vector<uint32_t>& sites) {
    std::vector<uintptr_t> out;
    const uintptr_t base = moduleBase();
    std::vector<uint8_t> code(0x1400);
    for (const uint32_t site : sites) {
        const uintptr_t from = base + site - 0x1000;
        if (!inExec(secs, base + site) || !inExec(secs, from) ||
            !sehRead(reinterpret_cast<const void*>(from), code.data(), code.size()))
            continue;
        for (size_t i = 0; i + 8 < code.size(); ++i) {
            size_t p = i;
            if (code[p] == 0x66 || code[p] == 0xF2 || code[p] == 0xF3)
                ++p;
            if ((code[p] & 0xF0) == 0x40)
                ++p;
            size_t op = p;
            bool ok = false;
            if (code[op] == 0x0F) {
                const uint8_t o = code[op + 1];
                ok = o == 0x10 || o == 0x28 || o == 0x2E || o == 0x2F || o == 0x51 || o == 0x58 || o == 0x59 ||
                     o == 0x5C || o == 0x5D || o == 0x5E || o == 0x5F || o == 0x5A || o == 0x6E || o == 0x7E;
                op += 2;
            } else if (code[op] == 0x8B || code[op] == 0x8D || code[op] == 0x3B) {
                ok = true;
                op += 1;
            }
            if (!ok || op + 5 > code.size() || (code[op] & 0xC7) != 0x05)
                continue;
            int32_t disp;
            std::memcpy(&disp, &code[op + 1], 4);
            const uintptr_t next = from + op + 5;
            const uintptr_t target = next + static_cast<intptr_t>(disp);
            if (inData(secs, target) && std::find(out.begin(), out.end(), target) == out.end() && out.size() < 512)
                out.push_back(target);
        }
    }
    return out;
}

bool userPointer(uint64_t p) { return p > 0x10000 && p < 0x00007FFFFFFF0000ull && (p & 7) == 0; }

bool scanAll(const sd::Targets& t) {
    hits.clear();
    envHits.clear();
    envFound = 0;
    for (auto& e : envRecord)
        e = sd::Env{};
    scanned = regions = raw = 0;
    const Sorted s = sortedOf(t);
    std::vector<uint8_t> buf;
    moduleSections = sections();
    const auto& secs = moduleSections;
    if (secs.empty()) {
        say("game module headers not readable; module and code search skipped");
    }
    const uintptr_t base = moduleBase();
    // 1. code
    std::vector<uint32_t> sites(std::begin(run32Sites), std::end(run32Sites));
    for (auto& l : learned)
        if (const uint32_t v = l.load(); v && std::find(sites.begin(), sites.end(), v) == sites.end())
            sites.push_back(v);
    const auto globals = secs.empty() ? std::vector<uintptr_t>{} : codeGlobals(secs, sites);
    {
        char line[96];
        std::snprintf(line, sizeof line, "%zu code sites, %zu data operands near them", sites.size(), globals.size());
        say(line);
    }
    const uint64_t codeDeadline = nowMs() + 15000;
    for (const uintptr_t g : globals) {
        // Update 34: only the writable data section the global is in (never code or .rdata).
        const Section* sec = writableSection(secs, g);
        if (!sec)
            continue;
        const uintptr_t lo = std::max<uintptr_t>(g - 1024, sec->start), hi = std::min<uintptr_t>(g + 1024, sec->start + sec->size);
        if (hi > lo && !scanRange(lo, hi - lo, sd::fromCode, t, s, true, buf, codeDeadline))
            break;
        uint64_t p = 0;
        if (sehRead(reinterpret_cast<const void*>(g), &p, 8) && userPointer(p) &&
            !(base && p >= base && p < base + (1ull << 31))) {
            // Update 34: follow only into private read-write memory outside the module.
            MEMORY_BASIC_INFORMATION mbi{};
            if (VirtualQuery(reinterpret_cast<void*>(p), &mbi, sizeof mbi) && mbi.State == MEM_COMMIT &&
                mbi.Type == MEM_PRIVATE && (mbi.Protect & 0xff) == PAGE_READWRITE &&
                !(mbi.Protect & (PAGE_GUARD | PAGE_WRITECOMBINE | PAGE_NOCACHE))) {
                const uintptr_t end = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
                const size_t n = static_cast<size_t>(std::min<uint64_t>(end - p, 64u << 10));
                if (!scanRange(static_cast<uintptr_t>(p), n, sd::fromCode, t, s, true, buf, codeDeadline))
                    break;
            }
        }
    }
    event(5, 1, static_cast<float>(hits.size()), static_cast<float>(globals.size()));
    // 2. module data
    const uint64_t moduleDeadline = nowMs() + 30000;
    size_t moduleBytes = 0;
    for (const auto& sec : secs) {
        if (sec.exec || !sec.write || stopping.load()) // Update 34: writable data only
            continue;
        const size_t n = std::min<size_t>(sec.size, (256u << 20) - std::min<size_t>(moduleBytes, 256u << 20));
        moduleBytes += n;
        ++regions;
        if (!scanRange(sec.start, n, sd::fromModule, t, s, true, buf, moduleDeadline))
            break;
    }
    event(5, 2, static_cast<float>(hits.size()), static_cast<float>(moduleBytes >> 20));
    // 3. heap
    const uint64_t heapDeadline = nowMs() + 90000;
    uint64_t heapBytes = 0;
    uintptr_t a = 0x10000;
    MEMORY_BASIC_INFORMATION mbi{};
    while (!stopping.load() && nowMs() < heapDeadline && heapBytes < (2ull << 30) &&
           VirtualQuery(reinterpret_cast<void*>(a), &mbi, sizeof mbi)) {
        const uintptr_t start = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
        const size_t size = mbi.RegionSize;
        a = start + size;
        if (!size || a <= start)
            break;
        if (mbi.State != MEM_COMMIT || mbi.Type != MEM_PRIVATE || (mbi.Protect & 0xff) != PAGE_READWRITE ||
            (mbi.Protect & (PAGE_GUARD | PAGE_WRITECOMBINE | PAGE_NOCACHE)) || size > (512u << 20))
            continue;
        if (base && start >= base && start < base + (1ull << 31))
            continue;
        ++regions;
        heapBytes += size;
        if (!scanRange(start, size, sd::fromHeap, t, s, false, buf, heapDeadline, true))
            break;
    }
    event(5, 3, static_cast<float>(hits.size()), static_cast<float>(heapBytes >> 20));
    char line[160];
    std::snprintf(line, sizeof line, "searched %llu MiB in %llu regions: %llu raw hits, %u EnvLightingDef",
                  static_cast<unsigned long long>(scanned >> 20), static_cast<unsigned long long>(regions),
                  static_cast<unsigned long long>(raw), envFound);
    say(line);
    for (uint32_t i = 0; i < std::min(envFound, sd::maxEnv); ++i) {
        const sd::Env& e = envRecord[i];
        std::snprintf(line, sizeof line,
                      "EnvLightingDef candidate at 0x%llx: RegLodRange %.2f x%u, FarLodCount %u, TotalShadowRange %.2f",
                      static_cast<unsigned long long>(e.address), e.regLodRange, e.regLodCount, e.farLodCount,
                      e.totalShadowRange);
        say(line);
    }
    return !stopping.load();
}

bool readNow(const Hit& h, uint64_t* out) {
    for (unsigned k = 0; k < h.count(); ++k) {
        out[k] = 0;
        if (!sehRead(reinterpret_cast<const void*>(h.at(k)), &out[k], h.wide() ? 8 : 4))
            return false;
    }
    return true;
}
double valueOf(const Hit& h, uint64_t bits) {
    if (h.wide()) {
        double d;
        std::memcpy(&d, &bits, 8);
        return d;
    }
    float f;
    const uint32_t b = static_cast<uint32_t>(bits);
    std::memcpy(&f, &b, 4);
    return f;
}
uint64_t bitsOf(const Hit& h, double v) {
    if (h.wide()) {
        uint64_t b;
        std::memcpy(&b, &v, 8);
        return b;
    }
    const float f = static_cast<float>(v);
    uint32_t b;
    std::memcpy(&b, &f, 4);
    return b;
}

// Write `want` bits over `expect` bits in each element; any mismatch undoes the rest.
// 1 done, 0 the memory held something else, -1 faulted or not writable memory.
// Update 34: no VirtualProtect, ever: memory that is not plainly writable data is refused.
int swapOne(uintptr_t a, bool wide, uint64_t expect, uint64_t value) {
    void* p = reinterpret_cast<void*>(a);
    if (wide) {
        uint64_t seen = 0;
        return sehSwap64(p, expect, value, &seen);
    }
    uint32_t seen = 0;
    return sehSwap32(p, static_cast<uint32_t>(expect), static_cast<uint32_t>(value), &seen);
}
int swapAll(Hit& h, const uint64_t* expect, const uint64_t* value) {
    for (unsigned k = 0; k < h.count(); ++k)
        if (!writableTarget(h.at(k), h.wide() ? 8 : 4))
            return -1;
    unsigned done = 0;
    int result = 1;
    for (unsigned k = 0; k < h.count(); ++k) {
        const int r = swapOne(h.at(k), h.wide(), expect[k], value[k]);
        if (r != 1) {
            result = r;
            break;
        }
        ++done;
    }
    if (result != 1)
        for (unsigned k = 0; k < done; ++k) // undo what went in
            swapOne(h.at(k), h.wide(), value[k], expect[k]);
    return result;
}
// Scale h to `scale` of the game's value (from what we wrote last).
int writeScale(Hit& h, double scale) {
    uint64_t next[3]{};
    for (unsigned k = 0; k < h.count(); ++k)
        next[k] = scale == 1.0 ? h.bits[k] : bitsOf(h, h.orig[k] * scale);
    const int r = swapAll(h, h.written, next);
    if (r == 1)
        for (unsigned k = 0; k < h.count(); ++k)
            h.written[k] = next[k];
    return r;
}
bool isWritten(const Hit& h) {
    for (unsigned k = 0; k < h.count(); ++k)
        if (h.written[k] != h.bits[k])
            return true;
    return false;
}
// Put the game's value back where we still own it (the memory holds what we wrote).
void putBack(Hit& h) {
    if (!isWritten(h))
        return;
    if (writeScale(h, 1.0) != 1)
        for (unsigned k = 0; k < h.count(); ++k)
            h.written[k] = h.bits[k]; // the game owns it now; leave it alone
}

// Confirm: 8 reads 0.5 s apart; keep what held still. Ranks the rest.
void confirm() {
    for (int round = 0; round < 8 && !stopping.load(); ++round) {
        Sleep(500);
        size_t keep = 0;
        for (size_t i = 0; i < hits.size(); ++i) {
            uint64_t now[3]{};
            if (!readNow(hits[i], now))
                continue;
            bool same = true;
            for (unsigned k = 0; k < hits[i].count(); ++k)
                same = same && now[k] == hits[i].bits[k];
            if (!same)
                continue;
            ++hits[i].confirms;
            hits[keep++] = hits[i];
        }
        hits.resize(keep);
        publish(&hits, -1);
    }
    // Update 34: anything not plainly writable data is dropped (never written, never protected).
    size_t keep = 0;
    for (size_t i = 0; i < hits.size(); ++i) {
        bool ok = true;
        for (unsigned k = 0; k < hits[i].count(); ++k)
            ok = ok && writableTarget(hits[i].at(k), hits[i].wide() ? 8 : 4);
        if (ok)
            hits[keep++] = hits[i];
    }
    if (keep != hits.size()) {
        char line[96];
        std::snprintf(line, sizeof line, "%zu candidate(s) in code or read-only memory dropped", hits.size() - keep);
        say(line);
    }
    hits.resize(keep);
    // EnvLightingDef first, then code-adjacent data, module, heap; triples, then by kind, then closest.
    auto rank = [](uint8_t origin) { return origin == sd::fromEnv ? -1 : static_cast<int>(origin); };
    std::stable_sort(hits.begin(), hits.end(), [&](const Hit& a, const Hit& b) {
        if (a.origin != b.origin)
            return rank(a.origin) < rank(b.origin);
        const bool ta = a.form >= sd::triple32, tb = b.form >= sd::triple32;
        if (ta != tb)
            return ta;
        if (a.target != b.target)
            return a.target < b.target;
        return a.err < b.err;
    });
}

struct Look {
    float reach{};
    uint64_t boxes{};
    unsigned kinds{};
};
Look look(uint64_t since) {
    const sd::Watch w = snapshot();
    Look l;
    l.reach = w.reach(since, &l.kinds);
    l.boxes = w.lastMs >= since ? w.boxes : 0;
    return l;
}
// The game's reach now (no change in), checking candidates still hold the values they had.
// Shadow Quality may have changed: values that moved in step with the boxes stay (followed).
bool refreshBaseline() {
    const float r = look(nowMs() - 800).reach;
    if (!(r > 0))
        return false;
    if (H0 > 0 && std::fabs(r / H0 - 1) > .03f) {
        size_t keep = 0;
        for (size_t i = 0; i < hits.size(); ++i) {
            Hit h = hits[i];
            uint64_t now[3]{};
            if (!readNow(h, now))
                continue;
            bool same = true, follows = true;
            for (unsigned k = 0; k < h.count(); ++k) {
                same = same && now[k] == h.bits[k];
                follows = follows && sd::within(valueOf(h, now[k]) / h.orig[k], r / H0, .01);
            }
            if (!same && !follows)
                continue;
            if (!same) {
                for (unsigned k = 0; k < h.count(); ++k) {
                    h.bits[k] = h.written[k] = now[k];
                    h.orig[k] = valueOf(h, now[k]);
                }
                h.followed = true;
            }
            hits[keep++] = h;
        }
        hits.resize(keep);
        event(5, 4, H0, r);
    }
    H0 = r;
    gameReachA = r;
    return true;
}

void putBackActive() {
    for (const int i : activeGroup)
        putBack(hits[static_cast<size_t>(i)]);
    activeGroup.clear();
    activeHit = -1;
    appliedScale = 1;
}

// Update 34: every trial's boxes before and after, for the record and the console.
void logTrial(uint32_t cand, float scale, float matched, bool depth, const sd::Sizes& b, const sd::Sizes& a,
              uint8_t verdict) {
    sd::Trial& t = trialRing[trialN % sd::maxTrials];
    t = {};
    t.candidate = cand;
    t.scale = scale;
    t.matched = matched;
    t.verdict = verdict;
    t.depth = depth ? 1 : 0;
    t.nBefore = static_cast<uint8_t>(b.n);
    t.nAfter = static_cast<uint8_t>(a.n);
    for (unsigned i = 0; i < sd::maxSeen; ++i) {
        t.before[i] = i < b.n ? (depth ? b.depth[i] : b.half[i]) : 0.f;
        t.after[i] = i < a.n ? (depth ? a.depth[i] : a.half[i]) : 0.f;
    }
    ++trialN;
    char line[300];
    int at = std::snprintf(line, sizeof line, "trial %u: candidate %d x%.2f against %s %.2f m; before", trialN,
                           cand == UINT32_MAX ? -1 : static_cast<int>(cand), scale, matched > 0 ? "box" : "all boxes",
                           matched);
    for (unsigned i = 0; i < b.n && at > 0 && at < 250; ++i)
        at += std::snprintf(line + at, sizeof line - static_cast<size_t>(at), " %.1f", t.before[i]);
    if (at > 0 && at < 260)
        at += std::snprintf(line + at, sizeof line - static_cast<size_t>(at), "; after");
    for (unsigned i = 0; i < a.n && at > 0 && at < 280; ++i)
        at += std::snprintf(line + at, sizeof line - static_cast<size_t>(at), " %.1f", t.after[i]);
    if (at > 0 && at < 290)
        std::snprintf(line + at, sizeof line - static_cast<size_t>(at), " -> %s",
                      verdict == sd::scaled ? "FOLLOWED" : verdict == sd::outOfRange ? "wild" : "no change");
    say(line);
}

// What a group's trial is checked against: the one box a lone size match matched, or all boxes.
struct Check {
    bool all{true}, depth{};
    float matched{};
};
Check checkOf(const std::vector<int>& g) {
    if (g.size() == 1) {
        const Hit& h = hits[static_cast<size_t>(g.front())];
        if (h.target < sd::targetKinds && h.form <= sd::f64)
            return Check{false, sd::targetIsDepth(h.target), sd::targetBoxSize(h.target, static_cast<float>(h.orig[0]))};
    }
    return Check{};
}
// The game's own reach for a group, in metres: an EnvLightingDef's TotalShadowRange, a lone
// match's box, else the smallest box in the group (the slider grows that).
float refOf(const std::vector<int>& g) {
    const Hit& f = hits[static_cast<size_t>(g.front())];
    if (f.origin == sd::fromEnv)
        return static_cast<float>(f.orig[1]);
    float r = 0;
    for (const int i : g) {
        const Hit& h = hits[static_cast<size_t>(i)];
        const float b = h.target < sd::targetKinds && h.form <= sd::f64
                            ? sd::targetBoxSize(h.target, static_cast<float>(h.orig[0]))
                            : H0;
        r = r > 0 ? std::min(r, b) : b;
    }
    return r > 0 ? r : H0;
}
constexpr float maxScale = 10.f; // never more than ten times the game's own size
float askedFor(int cars, float ref) { return std::min(maxScale, sd::growthFor(cars, ref)); }

// The kinds of sun box seen since `since`, after at least 3 new boxes in 0.8 to 2 s.
sd::Sizes boxesAfter(uint64_t since, uint32_t maxMs) {
    const uint64_t start = nowMs();
    const uint64_t boxes0 = snapshot().boxes;
    while (!stopping.load() && nowMs() < start + maxMs) {
        Sleep(30);
        if (snapshot().boxes >= boxes0 + 3 && nowMs() >= start + 800)
            break;
    }
    return sd::sizesSince(snapshot(), since);
}

// Writes the group at a trial scale (growth), checks the boxes, then sets the asked scale.
// Returns 0 followed (left at `asked`), 1 no change or not writable (put back), 2 anomaly
// (put back; reason set), 4 cannot test now (no boxes; reason set).
int tryGroup(const std::vector<int>& group, float asked, uint32_t& reason) {
    const sd::Sizes before = sd::sizesSince(snapshot(), nowMs() - 1500);
    if (!before.n) {
        reason = sd::noSunNow;
        return 4;
    }
    const Check c = checkOf(group);
    float scale = 0;
    if (c.all) {
        for (const float s : {1.5f, 2.5f, 1.3f}) {
            bool clear = false;
            for (unsigned i = 0; i < before.n; ++i)
                clear = clear || !sd::sizeNear(before, false, before.half[i] * s, sd::trialTolerance(s));
            if (clear) {
                scale = s;
                break;
            }
        }
    } else {
        scale = sd::pickTrialScale(before, c.depth, c.matched);
    }
    const uint32_t first = static_cast<uint32_t>(group.front());
    if (!(scale > 1)) { // every grown size is already another box: cannot tell
        logTrial(group.size() == 1 ? first : UINT32_MAX, 0, c.matched, c.depth, before, sd::Sizes{}, sd::noChange);
        for (const int j : group)
            hits[static_cast<size_t>(j)].outcome = sd::noMove;
        return 1;
    }
    ++trials;
    const uint64_t t0 = nowMs();
    std::vector<int> done;
    for (const int i : group) {
        Hit& h = hits[static_cast<size_t>(i)];
        const int r = writeScale(h, scale);
        if (r == 1)
            done.push_back(i);
        else
            h.outcome = r < 0 ? sd::faulted : sd::moved; // refused, faulted or not the game's value now
    }
    event(2, first, scale, static_cast<float>(done.size()));
    if (done.empty())
        return 1;
    const sd::Sizes after = boxesAfter(t0 + 150, 2000);
    if (!after.n) {
        for (const int j : done)
            putBack(hits[static_cast<size_t>(j)]);
        logTrial(group.size() == 1 ? first : UINT32_MAX, scale, c.matched, c.depth, before, after, sd::noChange);
        const sd::Sizes back = boxesAfter(nowMs(), 1500);
        reason = back.n ? sd::boxesStopped : sd::noSunNow;
        return back.n ? 2 : 4;
    }
    const sd::Verdict v = c.all ? sd::verifyAll(before, after, scale)
                                : sd::verifyMatched(before, after, c.depth, c.matched, scale);
    logTrial(group.size() == 1 ? first : UINT32_MAX, scale, c.matched, c.depth, before, after, v);
    if (v == sd::scaled) {
        bool ok = true;
        for (const int j : done)
            ok = ok && writeScale(hits[static_cast<size_t>(j)], asked) == 1;
        if (ok) {
            for (const int j : done)
                hits[static_cast<size_t>(j)].outcome = sd::worked;
            activeGroup = done;
            return 0;
        }
    }
    for (const int j : done) {
        putBack(hits[static_cast<size_t>(j)]);
        if (hits[static_cast<size_t>(j)].outcome == sd::untried)
            hits[static_cast<size_t>(j)].outcome = sd::noMove;
    }
    if (v == sd::outOfRange) {
        reason = sd::boxesWild;
        return 2;
    }
    return 1;
}

float activeRef = 0; // the game's reach of the group in use (metres)

// A change to `cars`; the state after it is set here.
void change(int cars) {
    if (cars == 0) {
        for (const int i : activeGroup)
            putBack(hits[static_cast<size_t>(i)]);
        appliedScale = 1;
        setState(sd::ready, sd::none);
        liveReachA = gameReachA.load();
        event(4, 0, 0, 0);
        return;
    }
    if (!activeGroup.empty()) { // a value already found: grow it (or give the game's back)
        const float asked = askedFor(cars, activeRef);
        bool ok = true;
        for (const int i : activeGroup)
            ok = ok && writeScale(hits[static_cast<size_t>(i)], asked > 1.0001f ? asked : 1.0) == 1;
        if (!ok) {
            putBackActive();
            setState(sd::reverted, sd::valueMoved);
            return;
        }
        appliedScale = asked;
        liveReachA = activeRef * asked;
        if (asked > 1.0001f)
            setState(sd::applied, sd::none);
        else
            setState(sd::ready, sd::alreadyFarther);
        event(3, static_cast<uint32_t>(cars), asked, liveReachA.load());
        return;
    }
    if (!refreshBaseline()) {
        setState(sd::ready, sd::noSunNow);
        return;
    }
    setState(sd::testing, sd::none);
    publish(&hits, activeHit);
    // Trials: every EnvLightingDef together, then each alone (8), then 24 single size matches,
    // then up to 8 groups of the same kind (16 each). Each press goes on where the last stopped.
    std::vector<std::vector<int>> plan;
    std::vector<int> env;
    for (size_t i = 0; i < hits.size(); ++i)
        if (hits[i].origin == sd::fromEnv && hits[i].outcome == sd::untried)
            env.push_back(static_cast<int>(i));
    if (env.size() >= 2)
        plan.push_back(env);
    for (size_t k = 0; k < env.size() && k < 8; ++k)
        plan.push_back({env[k]});
    size_t singles = 0;
    for (size_t i = 0; i < hits.size() && singles < 24; ++i)
        if (hits[i].origin != sd::fromEnv && hits[i].outcome == sd::untried) {
            plan.push_back({static_cast<int>(i)});
            ++singles;
        }
    for (uint8_t kind = 0; kind < sd::targetKinds; ++kind) {
        std::vector<int> g;
        for (size_t i = 0; i < hits.size() && g.size() < 16; ++i)
            if (hits[i].target == kind && hits[i].outcome == sd::noMove)
                g.push_back(static_cast<int>(i));
        if (g.size() >= 2)
            plan.push_back(g);
    }
    bool grew = false;
    for (const auto& g : plan) {
        if (stopping.load())
            break;
        // A whole-group trial puts its members back to noMove; skip members already tried alone.
        bool fresh = g.size() > 1;
        for (const int i : g)
            fresh = fresh || hits[static_cast<size_t>(i)].outcome == sd::untried;
        if (!fresh)
            continue;
        const float ref = refOf(g);
        const float asked = askedFor(cars, ref);
        if (!(asked > 1.0001f))
            continue; // the game already reaches farther than asked: nothing to grow
        grew = true;
        uint32_t reason = sd::none;
        const int r = tryGroup(g, asked, reason);
        if (r == 0) {
            activeHit = activeGroup.front();
            activeRef = ref;
            gameReachA = ref;
            appliedScale = asked;
            liveReachA = ref * asked;
            char line[200];
            const Hit& h = hits[static_cast<size_t>(activeHit)];
            std::snprintf(line, sizeof line,
                          "candidate %d (%zu written, origin %u form %u at %s+0x%llx) moved the sun boxes; game %.1f m, now %.1f m (x%.2f)",
                          activeHit, activeGroup.size(), h.origin, h.form, h.rva != UINT32_MAX ? "module" : "heap",
                          static_cast<unsigned long long>(h.rva != UINT32_MAX ? h.rva : h.addr), ref,
                          liveReachA.load(), asked);
            say(line);
            setState(sd::applied, sd::none);
            event(3, static_cast<uint32_t>(cars), asked, liveReachA.load());
            return;
        }
        if (r == 2) {
            setState(sd::reverted, reason);
            event(4, reason, 0, 0);
            return;
        }
        if (r == 4) {
            setState(sd::ready, reason);
            return;
        }
        publish(&hits, -1);
        Sleep(150); // the game's value settles back before the next
    }
    if (stopping.load())
        return;
    if (!grew) {
        setState(sd::ready, sd::alreadyFarther);
        return;
    }
    size_t left = 0;
    for (const auto& h : hits)
        left += h.outcome == sd::untried ? 1 : 0;
    if (left) {
        char line[120];
        std::snprintf(line, sizeof line, "no candidate this round moved the sun boxes; %zu untried, move the slider again", left);
        say(line);
        setState(sd::ready, sd::none);
        return;
    }
    setState(sd::noEffect, sd::none);
    say("no candidate moved the sun boxes; all put back");
}

// While a change is in: the memory still ours. Update 34: the boxes are not judged here any
// more (run33's widest-box mix put a working change back after 28 s).
void monitor() {
    if (activeGroup.empty())
        return;
    for (const int i : activeGroup) {
        Hit& h = hits[static_cast<size_t>(i)];
        uint64_t now[3]{};
        if (!readNow(h, now)) {
            for (unsigned k = 0; k < h.count(); ++k)
                h.written[k] = h.bits[k];
            putBackActive();
            setState(sd::reverted, sd::writeFault);
            return;
        }
        bool ours = true, game = true;
        for (unsigned k = 0; k < h.count(); ++k) {
            ours = ours && now[k] == h.written[k];
            game = game && now[k] == h.bits[k];
        }
        if (ours)
            continue;
        for (unsigned k = 0; k < h.count(); ++k)
            h.written[k] = h.bits[k]; // not ours any more: never written again
        putBackActive();
        setState(game ? sd::ready : sd::reverted, game ? sd::gameReset : sd::valueMoved);
        return;
    }
}

void searchOnce() {
    hits.clear();
    activeGroup.clear();
    activeHit = -1;
    activeRef = 0;
    H0 = 0;
    setState(sd::searching, sd::none);
    publish(nullptr, -1);
    // Wait for the sun boxes: a few sightings, ideally three sizes.
    const uint64_t begin = nowMs();
    for (;;) {
        if (stopping.load())
            return;
        Sleep(250);
        const uint64_t now = nowMs();
        const sd::Watch w = snapshot();
        unsigned kinds = 0, steady = 0;
        w.reach(now - 2000, &kinds);
        for (unsigned i = 0; i < w.n; ++i)
            steady += w.kinds[i].lastMs + 2000 >= now && w.kinds[i].hits >= 3 ? 1u : 0u;
        if (steady >= 3 || (steady >= 1 && now > begin + 20000))
            break;
        if (now > begin + 15000 && !kinds)
            setState(sd::searching, sd::noSun);
        if (now > begin + 600000) {
            setState(sd::notFound, sd::noSun);
            return;
        }
        if ((now - begin) % 2000 < 250)
            publish(nullptr, -1);
    }
    setState(sd::searching, sd::none);
    const sd::Watch w = snapshot();
    const sd::Targets t = sd::targetsFrom(w, nowMs() - 3000);
    H0 = w.reach(nowMs() - 2000);
    gameReachA = H0;
    {
        char line[200];
        int at = std::snprintf(line, sizeof line, "%u sun box sizes (half m):", t.sizeN);
        for (unsigned i = 0; i < t.sizeN && at > 0 && at < 180; ++i)
            at += std::snprintf(line + at, sizeof line - static_cast<size_t>(at), " %.3f", t.sizes[i]);
        say(line);
    }
    if (!scanAll(t))
        return;
    hits.insert(hits.begin(), envHits.begin(), envHits.end());
    const uint64_t rawFound = raw + envHits.size();
    publish(&hits, -1);
    confirm();
    if (stopping.load())
        return;
    char line[160];
    std::snprintf(line, sizeof line, "%zu candidates held still for 4 s (of %llu raw); first are the slider's",
                  hits.size(), static_cast<unsigned long long>(rawFound));
    say(line);
    if (hits.empty()) {
        setState(sd::notFound, rawFound ? sd::unsteady : sd::noMatch);
        return;
    }
    setState(sd::ready, sd::none);
}

DWORD WINAPI work(void*) {
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
    searchOnce();
    int appliedCars = 0, lastAsked = 0;
    uint64_t lastPublish = 0, lastMonitor = 0;
    while (!stopping.load()) {
        Sleep(30);
        const uint32_t st = sd::stateOf(packedNow.load());
        if (restart.exchange(false) && (st == sd::notFound || st == sd::noEffect)) {
            searchOnce();
            appliedCars = 0;
            continue;
        }
        const uint64_t now = nowMs();
        // Update 34.2: each value the slider asked, recorded on this thread (the ring's).
        if (const int asked = want.load(); asked != lastAsked) {
            lastAsked = asked;
            event(6, static_cast<uint32_t>(asked), asked * sd::carMetres, gameReachA.load());
        }
        if (sd::sliderLive(packedNow.load())) {
            const int w = want.load();
            if (w != appliedCars) {
                change(w);
                appliedCars = w; // the same value is not tried again until the slider moves
                publish(&hits, activeHit);
            } else if (now >= lastMonitor + 100) {
                lastMonitor = now;
                const uint32_t before = packedNow.load();
                monitor();
                if (packedNow.load() != before)
                    appliedCars = 0; // the game took it back: the slider's value is tried again
                if (activeGroup.empty())
                    liveReachA = look(now - 800).reach;
            }
        } else {
            appliedCars = 0;
        }
        if (now >= lastPublish + 500) {
            lastPublish = now;
            publish(&hits, activeHit);
        }
    }
    putBackActive();
    if (sd::stateOf(packedNow.load()) == sd::applied)
        setState(sd::ready, sd::ended);
    publish(&hits, -1);
    return 0;
}

} // namespace

void start() {
    if (stopping.load())
        return;
    watchOn = true;
    if (running.exchange(true)) {
        restart = true;
        return;
    }
    worker = CreateThread(nullptr, 0, work, nullptr, 0, nullptr);
    if (!worker) {
        running = false;
        watchOn = false;
        say("disabled: CreateThread failed");
        setState(sd::notFound, sd::searchFault);
        publish(nullptr, -1);
    }
}

bool watching() { return watchOn.load(std::memory_order_relaxed); }

bool takeCopy(uint32_t site) {
    bool known = false;
    for (auto& l : learned)
        known = known || (site && l.load(std::memory_order_relaxed) == site);
    const uint64_t window = GetTickCount64() / 10;
    if (budgetWindow.load(std::memory_order_relaxed) != window) {
        budgetWindow = window;
        budgetUsed = 0;
    }
    return ++budgetUsed <= (known ? 48u : 6u);
}

namespace {
struct Found {
    float half[8], depth[8];
    unsigned n;
};
// Copies the upload (at most 64 KiB) under SEH, then finds sun boxes: POD only.
bool sehCopyUpload(const void* mapped, size_t bytes, float* out) {
    return sehRead(mapped, out, bytes);
}
} // namespace

void noteUpload(const void* mapped, size_t bytes, uint32_t site) {
    static thread_local float copy[16384];
    if (!mapped || bytes < 64)
        return;
    const size_t n = std::min<size_t>(bytes, sizeof copy) & ~size_t{63};
    if (!sehCopyUpload(mapped, n, copy))
        return;
    Found f{};
    for (size_t i = 0; i + 16 <= n / 4 && f.n < 8; i += 4) {
        if (copy[i + 15] != 1.f)
            continue;
        sd::Box b;
        if (sd::sunBox(copy + i, b)) {
            f.half[f.n] = b.half[0];
            f.depth[f.n] = b.half[2];
            ++f.n;
        }
    }
    if (!f.n)
        return;
    if (site) {
        bool known = false;
        for (auto& l : learned)
            known = known || l.load() == site;
        for (auto& l : learned) {
            if (known)
                break;
            uint32_t zero = 0;
            if (l.compare_exchange_strong(zero, site))
                break;
        }
    }
    if (!TryAcquireSRWLockExclusive(&watchLock))
        return;
    const uint64_t ms = GetTickCount64();
    for (unsigned i = 0; i < f.n; ++i)
        watch.note(f.half[i], f.depth[i], ms);
    ReleaseSRWLockExclusive(&watchLock);
}

void set(int cars) {
    // Update 34.2: refused only when locked; said and recorded (event 6) so the report can
    // tell a slider never moved from a trial that did nothing (run341 could not).
    if (!sd::sliderMoves(packedNow.load()))
        return;
    const int c = std::clamp(cars, 0, sd::maxCars);
    if (want.exchange(c) != c) {
        char line[96];
        std::snprintf(line, sizeof line, "asked %d cars (%d m)", c, static_cast<int>(std::lround(c * sd::carMetres)));
        say(line);
    }
}
uint32_t now() { return packedNow.load(); }
float wantCars() { return static_cast<float>(want.load()); }
float gameReach() { return gameReachA.load(); }
float liveReach() { return liveReachA.load(); }

void stop(uint32_t ms) {
    stopping = true;
    watchOn = false;
    if (worker) {
        WaitForSingleObject(worker, ms);
        CloseHandle(worker);
        worker = nullptr;
    }
}

} // namespace spidy::native_shadow_distance
