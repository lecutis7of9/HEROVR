// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
//
// Update 27 / 27.1 / 27.2 / 27.3 / 28: ADDRESS FINDER. Read-only. Candidates only.
// 28: one row per call site (sameSite), room kept for value hits, older cameras
// matched too, more matrix/jitter forms, stable lod confirm, "almost" list.
// On any fault: disable this path, one OutputDebugStringA line, leave VR alone.
// 27.1: every __try/__except lives in a POD-only SEH helper (MSVC C2712 /EHsc).
// 27.2: never stall a frame — budgeted live sampling; heavy scans + finish on a
// background thread; report buffer refreshed periodically and on finish/fail.
// 27.3: raw hits vs confirmed; trivial reject; K-frame confirm with changing
// values; dedupe by call site + cb offset; cap never ends run; frameCount from
// noteView (not per barrier); early finish only when lighting/TAA/LOD are strong.
// 30: BIND TRACE. Camera data found at a GPU address is watched; later root CBV
// binds covering it (and root-constant hits) are stored in their own reserved
// rows with call site, slot, offset, camera and the pass target (OMSetRenderTargets
// / RSSetViewports / last DEPTH_WRITE barrier). Depth-only = likely shadow pass.
// The two pass-target hooks fail alone: passes then read "unknown", nothing else stops.
#include "spidy/native_address_finder.hpp"
#include "spidy/native_render_memory.hpp"
#include "spidy/native_shadow_distance.hpp"
#include "spidy/sl_watch.hpp"
#include <MinHook.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>
#include <windows.h>
#ifdef _MSC_VER
#include <intrin.h>
#define SPIDY_RETURN_ADDRESS() _ReturnAddress()
#else
#define SPIDY_RETURN_ADDRESS() __builtin_return_address(0)
#endif

namespace spidy::native_address_finder {
namespace {

constexpr uintptr_t allocatorRva = 0xc15d780;
constexpr uintptr_t activeViewPtrRva = 0xc2ca220;
constexpr size_t maxCandidates = address_finder::maxRawHits; // raw-hit storage only
constexpr size_t maxReport = reportBytes - 1;
constexpr size_t maxScanBytes = 4u << 20;       // background / render-alloc ceiling
constexpr size_t maxLiveScanBytes = 256u << 10; // render-thread copy/const sample
constexpr size_t maxGpuHitsPerFrame = 8;        // per live sample, not per frame flood
constexpr uint32_t maxCopyScansPerFrame = 1;
constexpr uint32_t maxBarrierSamplesPerFrame = 4;
constexpr uint32_t maxConstScansPerFrame = 2;
constexpr uint64_t reportRefreshMs = 10000;
constexpr uint32_t maxBindLookupsPerFrame = 512; // full watch-table searches (fast page filter first)
constexpr uint32_t maxBindTracesPerFrame = 48;   // stored binds (stack walk) per frame
constexpr size_t watchPageSlots = 1024;
constexpr unsigned watchPageProbe = 8;
// Update 30: GPU pages where Update 29 saw camera data (FLAT/HEAD/EYE). GPU
// addresses can move between runs, so these only label binds "seed29"; they are
// never counted as shadow candidates and never used as live addresses.
constexpr uint64_t seed29Pages[] = {0x11df4c0000ull, 0x11df550000ull, 0x11df5e0000ull, 0x11dff90000ull,
                                    0x11dffa0000ull, 0x11dfff0000ull, 0x11e0000000ull, 0x11e0190000ull};

struct Candidate {
    char kind[32]{};
    char detail[160]{};
    uint32_t moduleRva{};
    uint32_t functionRva{};
    char signature[128]{};
    uint64_t gpuVa{};
    uint32_t cbOffset{};
    uint32_t rootSlot{};
    uint32_t hits{}; // raw hit count
    uint32_t distinctFrames{};
    float score{};
    float lastError{};
    float sample[4]{};
    address_finder::ConfirmState confirm{};
    address_finder::HitCategory category{address_finder::HitCategory::other};
    bool confirmed{};
    bool valueBearing{}; // participates in K-frame confirmation
    bool nearMiss{};     // Update 28: within nearMissFactor x tolerance ("almost" only)
};

Data live{};
char reportBuf[reportBytes]{};
std::mutex lock;
extern "C" spidy::native_address_finder::Data SpidyAddressFinderData;
extern "C" char SpidyAddressFinderReport[reportBytes];
extern "C" spidy::sl_watch::Data SpidySlWatchData; // 0.0.73 SL WATCH
void syncExports() {
    SpidyAddressFinderData = live;
    std::memcpy(SpidyAddressFinderReport, reportBuf, reportBytes);
}
std::atomic<bool> armed{};
std::atomic<bool> hooksOn{};
std::atomic<bool> failed{};
std::atomic<uint32_t> faultCode{};
std::atomic<uint64_t> deadlineMs{};
std::atomic<uint64_t> frameCount{};
std::atomic<uint32_t> generation{};
std::vector<Candidate> candidates;
address_finder::Camera stockCam{}, headCam{};
address_finder::Camera eyeCam[2]{}; // Update 29: the eyes' own cameras (noteView 2, 3)
address_finder::Camera stockHistory[address_finder::cameraLags]{}; // Update 28: older stock cameras
address_finder::JitterTrack jitterTrack{};
address_finder::StableState lodStable{}; // Update 28: lod_scale confirm by stable offset
std::atomic<uint32_t> lodOffset{UINT32_MAX};
HANDLE scanThread{};
std::atomic<bool> scanStop{};
std::atomic<bool> finishRequested{};
std::atomic<uint64_t> lastReportMs{};
std::atomic<uint32_t> skipFrames{};
std::atomic<uint32_t> frameOps{};
std::atomic<uint32_t> copyScansThisFrame{};
std::atomic<uint32_t> constScansThisFrame{};
std::atomic<uint32_t> barrierSamplesThisFrame{};
std::atomic<uint64_t> frameStartQpc{};
std::atomic<uint64_t> lastFrameElapsedNs{};
std::atomic<void*> pendingCommandList{}; // for deferred GPU hook install
std::atomic<uint32_t> confirmedCount{};
std::atomic<uint32_t> almostTotal{}; // Update 28: "almost" rows in the last report (HUD DONE n +m ALMOST)
std::atomic<bool> strongLighting{};
std::atomic<bool> strongTaa{};
std::atomic<bool> strongLod{};

// Update 30: bind trace state. watchLock is only ever try-locked on the render
// thread; lock order is lock -> watchLock (never the reverse).
std::mutex watchLock;
address_finder::Watched watched[address_finder::maxWatched]{};
size_t watchedN{};
address_finder::BindRow bindRows[address_finder::bindRowsCap]{};
size_t bindN{};
std::atomic<uint64_t> watchPages[watchPageSlots]{}; // page+1 (0 = empty); never cleared mid-run
std::atomic<uint32_t> bindLookupsThisFrame{};
std::atomic<uint32_t> bindTracesThisFrame{};
std::atomic<uint32_t> bindDropped{};
std::atomic<uint32_t> shadowTotal{};
std::atomic<bool> passHooksOn{};
std::atomic<bool> passHooksFailed{};
std::atomic<bool> bindTraceFailed{};
thread_local address_finder::PassTarget tlsPass{};
thread_local const void* tlsConstsList{};
thread_local uint8_t tlsConstsApi{};

// Update 31: descriptor tables, per-pass bind counts, depth targets, ortho boxes.
// descLock is only ever try-locked on render threads (never with lock or watchLock held).
std::mutex descLock;
address_finder::DescEntry descMap[address_finder::descSlots]{};
size_t descN{};
std::atomic<size_t> descCount{};
std::atomic<uint64_t> heapCpu{}, heapGpu{};
std::atomic<uint32_t> heapCount{}, descInc{};
std::atomic<void*> heapSeen{};
std::atomic<uint32_t> descOpsThisFrame{};
constexpr uint32_t maxDescOpsPerFrame = 8192;
std::atomic<uint64_t> descCreated{}, descCopied{}, tableHits{};
std::atomic<uint64_t> heapStartBinds{}, tableBindsTotal{};
std::atomic<uint32_t> passBinds[6][4]{}; // bindApiName x PassKind
std::atomic<bool> tableHooksOn{};
std::atomic<bool> tableHooksFailed{};
address_finder::DepthTarget depthTargets[address_finder::maxDepthTargets]{}; // under watchLock
size_t depthTargetN{};
thread_local long tlsDepthTarget = -1;
address_finder::OrthoRow orthoRows[address_finder::maxOrthoRows]{}; // under watchLock
size_t orthoN{};
// Update 32: ORTHO CAPTURE (under watchLock).
address_finder::OrthoCapture orthoCaptures[address_finder::maxOrthoCaptures]{};
size_t orthoCaptureN{};
address_finder::OrthoGroupState orthoGroup{};
std::atomic<uint64_t> runStartMs{};

using DrawIndexedFn = void(STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*, UINT, UINT, INT, UINT);
using DispatchFn = void(STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*, UINT, UINT, UINT);
using CopyBufferFn = void(STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*, ID3D12Resource*, UINT64,
                                              ID3D12Resource*, UINT64, UINT64);
using SetPsoFn = void(STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*, ID3D12PipelineState*);
using SetRootCbvFn = void(STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*, UINT, D3D12_GPU_VIRTUAL_ADDRESS);
using SetRootConstsFn = void(STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*, UINT, UINT, const void*);
using ExportFn = uint64_t (*)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
using OmSetRtFn = void(STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*, UINT, const D3D12_CPU_DESCRIPTOR_HANDLE*, BOOL,
                                           const D3D12_CPU_DESCRIPTOR_HANDLE*);
using RsSetViewportsFn = void(STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*, UINT, const D3D12_VIEWPORT*);

DrawIndexedFn originalDrawIndexed{};
DispatchFn originalDispatch{};
CopyBufferFn originalCopyBuffer{};
SetPsoFn originalSetPso{};
SetRootCbvFn originalSetGraphicsCbv{}, originalSetComputeCbv{};
SetRootConstsFn originalSetGraphicsConsts{}, originalSetComputeConsts{};
void* gpuHooks[8]{};
OmSetRtFn originalOmSetRt{};
RsSetViewportsFn originalRsSetViewports{};
void* passHooks[2]{};
using SetHeapsFn = void(STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*, UINT, ID3D12DescriptorHeap* const*);
using SetTableFn = void(STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*, UINT, D3D12_GPU_DESCRIPTOR_HANDLE);
using CreateCbvFn = void(STDMETHODCALLTYPE*)(ID3D12Device*, const D3D12_CONSTANT_BUFFER_VIEW_DESC*,
                                             D3D12_CPU_DESCRIPTOR_HANDLE);
using CreateSrvFn = void(STDMETHODCALLTYPE*)(ID3D12Device*, ID3D12Resource*, const D3D12_SHADER_RESOURCE_VIEW_DESC*,
                                             D3D12_CPU_DESCRIPTOR_HANDLE);
using CopySimpleFn = void(STDMETHODCALLTYPE*)(ID3D12Device*, UINT, D3D12_CPU_DESCRIPTOR_HANDLE,
                                              D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_DESCRIPTOR_HEAP_TYPE);
SetHeapsFn originalSetHeaps{};
SetTableFn originalSetComputeTable{}, originalSetGraphicsTable{};
CreateCbvFn originalCreateCbv{};
CreateSrvFn originalCreateSrv{};
CopySimpleFn originalCopySimple{};
void* tableHooks[6]{};
thread_local ID3D12PipelineState* tlsPso{};

ExportFn originalSlSetConstants{}, originalSlEvaluate{}, originalSlSetTag{}, originalSlSetTagForFrame{};
ExportFn originalNgxEval{}, originalNgxEvalC{}, originalXessExec{}, originalFsrDispatch{};

void bump() { InterlockedIncrement64(&live.sequence); }

void recountConfirmedLocked() {
    uint32_t n = 0;
    bool lit = false, taa = false, lod = false;
    for (const auto& c : candidates) {
        if (!c.confirmed)
            continue;
        ++n;
        if (c.score >= address_finder::strongScore) {
            if (c.category == address_finder::HitCategory::lighting)
                lit = true;
            else if (c.category == address_finder::HitCategory::taa)
                taa = true;
            else if (c.category == address_finder::HitCategory::lod)
                lod = true;
        }
    }
    confirmedCount = n;
    strongLighting = lit;
    strongTaa = taa;
    strongLod = lod;
}

void publishInner() {
    bump();
    live.magic = dataMagic;
    live.version = dataVersion;
    live.bytes = sizeof(Data);
    // HUD DONE n = confirmed; rawHits is the capped store size.
    live.candidates = confirmedCount.load();
    live.rawHits = static_cast<uint32_t>(candidates.size());
    live.generation = generation.load();
    live.fault = faultCode.load();
    live.frames = frameCount.load();
    live.lodOffset = lodOffset.load();
    if (armed.load()) {
        const uint64_t now = GetTickCount64();
        const uint64_t dl = deadlineMs.load();
        live.secondsLeft = now < dl ? static_cast<uint32_t>((dl - now + 999) / 1000) : 0;
        live.status = 1;
    } else if (failed.load()) {
        live.status = 3;
        live.secondsLeft = 0;
    } else if (live.status == 1 || confirmedCount.load() || live.rawHits || live.status == 2) {
        live.status = (confirmedCount.load() == 0 && live.rawHits == 0 && live.fault) ? 3 : 2;
        live.secondsLeft = 0;
    } else {
        live.status = 0;
        live.secondsLeft = 0;
    }
    bump();
    syncExports();
}

void finaliseReport(); // below

void fail(uint32_t code, const char* line) {
    if (failed.exchange(true))
        return;
    faultCode = code;
    armed = false;
    finishRequested = true;
    char buf[320];
    std::snprintf(buf, sizeof buf, "ADDRESS FINDER: disabled (%u): %s\n", code, line ? line : "");
    OutputDebugStringA(buf);
    finaliseReport();
    live.status = 3;
    publishInner();
}

// SEH helpers (Update 27.1): MSVC C2712 forbids __try in any function that also
// needs C++ object unwinding. Keep only POD / raw pointers / sizes here.
bool safeRead(const void* p, void* out, size_t n) {
    __try {
        if (!p || reinterpret_cast<uintptr_t>(p) < 0x10000)
            return false;
        std::memcpy(out, p, n);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

void** sehVtable(void* obj) {
    __try {
        if (!obj)
            return nullptr;
        return *reinterpret_cast<void***>(obj);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
}

HRESULT sehMap(ID3D12Resource* res, UINT sub, const D3D12_RANGE* read, void** mapped) {
    __try {
        if (!res || !mapped)
            return E_FAIL;
        return res->Map(sub, read, mapped);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return E_FAIL;
    }
}

void sehUnmap(ID3D12Resource* res, UINT sub, const D3D12_RANGE* written) {
    __try {
        if (res)
            res->Unmap(sub, written);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}

bool sehGpuVa(ID3D12Resource* res, D3D12_GPU_VIRTUAL_ADDRESS* out) {
    __try {
        if (!res || !out)
            return false;
        *out = res->GetGPUVirtualAddress();
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool sehGetResourceDesc(ID3D12Resource* res, D3D12_RESOURCE_DESC* out) {
    __try {
        if (!res || !out)
            return false;
        *out = res->GetDesc();
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

// Update 31: POD SEH helpers for the descriptor-table trace.
bool sehHeapInfo(ID3D12DescriptorHeap* heap, D3D12_DESCRIPTOR_HEAP_DESC* desc, uint64_t* cpu, uint64_t* gpu) {
    __try {
        if (!heap || !desc || !cpu || !gpu)
            return false;
        *desc = heap->GetDesc();
        *cpu = heap->GetCPUDescriptorHandleForHeapStart().ptr;
        *gpu = heap->GetGPUDescriptorHandleForHeapStart().ptr;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

ID3D12Device* sehDeviceOf(ID3D12GraphicsCommandList* list, UINT* inc) {
    __try {
        if (!list || !inc)
            return nullptr;
        void* dev{};
        if (FAILED(list->GetDevice(__uuidof(ID3D12Device), &dev)) || !dev)
            return nullptr;
        auto* d = static_cast<ID3D12Device*>(dev);
        *inc = d->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        return d;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
}

void sehRelease(IUnknown* p) {
    __try {
        if (p)
            p->Release();
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}

uint64_t qpcNow() {
    LARGE_INTEGER i{};
    QueryPerformanceCounter(&i);
    return static_cast<uint64_t>(i.QuadPart);
}

uint64_t qpcFreq() {
    static const uint64_t f = [] {
        LARGE_INTEGER i{};
        QueryPerformanceFrequency(&i);
        return static_cast<uint64_t>(i.QuadPart);
    }();
    return f;
}

uint64_t qpcToNs(uint64_t ticks) {
    const uint64_t f = qpcFreq();
    return f ? ticks * 1000000000ull / f : 0;
}

void beginFrameBudget() {
    frameStartQpc = qpcNow();
    frameOps = 0;
    copyScansThisFrame = 0;
    constScansThisFrame = 0;
    barrierSamplesThisFrame = 0;
}

uint64_t frameElapsedNs() {
    const uint64_t start = frameStartQpc.load();
    if (!start)
        return 0;
    return qpcToNs(qpcNow() - start);
}

bool liveBudgetAllows() {
    if (skipFrames.load())
        return false;
    return address_finder::frameBudgetAllows(frameOps.load(), address_finder::frameBudgetMaxOps,
                                             frameElapsedNs(), address_finder::frameBudgetNanoseconds);
}

void noteLiveOp() { ++frameOps; }

void endFrameBudget() {
    const uint64_t elapsed = frameElapsedNs();
    lastFrameElapsedNs = elapsed;
    const bool over = elapsed > address_finder::frameBudgetNanoseconds ||
                      frameOps.load() > address_finder::frameBudgetMaxOps;
    skipFrames = address_finder::frameBudgetNextSkip(skipFrames.load(), over);
    frameStartQpc = 0;
}

uintptr_t moduleBase() {
    return reinterpret_cast<uintptr_t>(GetModuleHandleW(L"Spider-Man2.exe"));
}

// Update 28: same call site whether or not the function RVA is known yet (sameSite).
Candidate* findRawLocked(const char* kind, uint32_t functionRva, uint32_t moduleRva, uint32_t cbOffset,
                         uint32_t rootSlot) {
    for (auto& c : candidates) {
        if (std::strcmp(c.kind, kind) == 0 && c.cbOffset == cbOffset && c.rootSlot == rootSlot &&
            address_finder::sameSite(c.functionRva, c.moduleRva, functionRva, moduleRva))
            return &c;
    }
    return nullptr;
}

// Update 28: room for value-bearing hits (rawHitAdmit / evictIndex).
bool makeRoomLocked(bool valueBearing) {
    if (address_finder::rawHitAdmit(candidates.size(), valueBearing, maxCandidates))
        return true;
    if (!valueBearing)
        return false;
    std::vector<address_finder::RawHitSummary> rows(candidates.size());
    for (size_t i = 0; i < candidates.size(); ++i)
        rows[i] = {candidates[i].confirmed, candidates[i].valueBearing && !candidates[i].nearMiss,
                   candidates[i].hits, candidates[i].distinctFrames};
    const long at = address_finder::evictIndex(rows.data(), rows.size());
    if (at < 0)
        return false;
    candidates.erase(candidates.begin() + at);
    return true;
}

void addCandidateLocked(const char* kind, const char* detail, uint32_t moduleRva, uint32_t functionRva,
                        const char* signature, uint64_t gpuVa, uint32_t cbOffset, uint32_t rootSlot,
                        bool valueBearing = false, const float* sample = nullptr, float error = 0.f,
                        bool nearMiss = false) {
    if (Candidate* existing = findRawLocked(kind ? kind : "", functionRva, moduleRva, cbOffset, rootSlot)) {
        ++existing->hits;
        if (gpuVa)
            existing->gpuVa = gpuVa;
        if (functionRva && !existing->functionRva)
            existing->functionRva = functionRva;
        if (signature && signature[0] && !existing->signature[0])
            std::snprintf(existing->signature, sizeof existing->signature, "%s", signature);
        if (detail && detail[0])
            std::snprintf(existing->detail, sizeof existing->detail, "%s", detail);
        if (valueBearing && sample) {
            existing->valueBearing = true;
            existing->lastError = error;
            existing->category = address_finder::categoryForKindName(existing->kind);
            if (std::strstr(existing->detail, "jitter"))
                existing->category = address_finder::HitCategory::taa;
            else if (std::strstr(existing->detail, "pose") || std::strstr(existing->detail, "view") ||
                     std::strstr(existing->detail, "rotation") || std::strstr(existing->detail, "position") ||
                     std::strstr(existing->detail, "proj"))
                existing->category = address_finder::HitCategory::lighting;
            const uint64_t frame = frameCount.load();
            if (address_finder::noteConfirmSample(existing->confirm, frame, sample)) {
                existing->confirmed = true;
                existing->distinctFrames = existing->confirm.distinctFrames;
                existing->score = address_finder::scoreConfirmed(existing->distinctFrames, existing->hits,
                                                                existing->lastError, existing->category);
            } else {
                existing->distinctFrames = existing->confirm.distinctFrames;
            }
            for (int i = 0; i < 4; ++i)
                existing->sample[i] = sample[i];
        }
        recountConfirmedLocked();
        return;
    }
    if (!makeRoomLocked(valueBearing && !nearMiss))
        return; // cap: stop storing new raw hits; never ends the run
    Candidate c{};
    c.nearMiss = nearMiss;
    std::snprintf(c.kind, sizeof c.kind, "%s", kind ? kind : "");
    std::snprintf(c.detail, sizeof c.detail, "%s", detail ? detail : "");
    c.moduleRva = moduleRva;
    c.functionRva = functionRva;
    std::snprintf(c.signature, sizeof c.signature, "%s", signature ? signature : "");
    c.gpuVa = gpuVa;
    c.cbOffset = cbOffset;
    c.rootSlot = rootSlot;
    c.hits = 1;
    c.category = address_finder::categoryForKindName(c.kind);
    c.valueBearing = valueBearing;
    c.lastError = error;
    if (valueBearing && sample) {
        for (int i = 0; i < 4; ++i)
            c.sample[i] = sample[i];
        const uint64_t frame = frameCount.load();
        if (address_finder::noteConfirmSample(c.confirm, frame, sample)) {
            c.confirmed = true;
            c.distinctFrames = c.confirm.distinctFrames;
            c.score = address_finder::scoreConfirmed(c.distinctFrames, c.hits, c.lastError, c.category);
        } else {
            c.distinctFrames = c.confirm.distinctFrames;
        }
        if (std::strstr(c.detail, "jitter"))
            c.category = address_finder::HitCategory::taa;
        else if (std::strstr(c.detail, "pose") || std::strstr(c.detail, "view") ||
                 std::strstr(c.detail, "rotation") || std::strstr(c.detail, "position") ||
                 std::strstr(c.detail, "proj"))
            c.category = address_finder::HitCategory::lighting;
    }
    candidates.push_back(c);
    recountConfirmedLocked();
}

// Background / control path: may wait briefly for the lock.
void addCandidate(const char* kind, const char* detail, uint32_t moduleRva, uint32_t functionRva,
                  const char* signature, uint64_t gpuVa = 0, uint32_t cbOffset = 0, uint32_t rootSlot = 0) {
    std::lock_guard g(lock);
    addCandidateLocked(kind, detail, moduleRva, functionRva, signature, gpuVa, cbOffset, rootSlot);
}

// Render thread: never block on the scan lock — drop the sample if busy.
bool addCandidateLive(const char* kind, const char* detail, uint32_t moduleRva, uint64_t gpuVa = 0,
                      uint32_t cbOffset = 0, uint32_t rootSlot = 0) {
    std::unique_lock g(lock, std::try_to_lock);
    if (!g.owns_lock())
        return false;
    // Signatures / function RVA filled later on the background thread (enrichCandidates).
    addCandidateLocked(kind, detail, moduleRva, 0, "", gpuVa, cbOffset, rootSlot);
    return true;
}

uint32_t returnRva() {
    const uintptr_t base = moduleBase();
    if (!base)
        return 0;
    void* frames[8]{};
    const USHORT n = RtlCaptureStackBackTrace(2, 8, frames, nullptr);
    for (USHORT i = 0; i < n; ++i) {
        const auto a = reinterpret_cast<uintptr_t>(frames[i]);
        if (a >= base && a < base + (512u << 20))
            return static_cast<uint32_t>(a - base);
    }
    return 0;
}

uint32_t functionRvaFor(uint32_t rva) {
    const uintptr_t base = moduleBase();
    if (!base || !rva)
        return 0;
    IMAGE_DOS_HEADER dh{};
    if (!safeRead(reinterpret_cast<const void*>(base), &dh, sizeof dh) || dh.e_magic != IMAGE_DOS_SIGNATURE)
        return 0;
    IMAGE_NT_HEADERS64 nt{};
    if (!safeRead(reinterpret_cast<const void*>(base + dh.e_lfanew), &nt, sizeof nt) ||
        nt.Signature != IMAGE_NT_SIGNATURE)
        return 0;
    const auto& dir = nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION];
    if (!dir.VirtualAddress || !dir.Size)
        return 0;
    const size_t count = dir.Size / sizeof(address_finder::RuntimeFunction);
    std::vector<address_finder::RuntimeFunction> pdata(count);
    if (!safeRead(reinterpret_cast<const void*>(base + dir.VirtualAddress), pdata.data(),
                  count * sizeof(address_finder::RuntimeFunction)))
        return 0;
    return address_finder::functionBegin(pdata.data(), count, rva);
}

std::string sigAtRva(uint32_t rva) {
    const uintptr_t base = moduleBase();
    if (!base || !rva)
        return {};
    uint8_t bytes[address_finder::signatureBytes]{};
    if (!safeRead(reinterpret_cast<const void*>(base + rva), bytes, sizeof bytes))
        return {};
    return address_finder::signatureText(address_finder::signatureAt(bytes, sizeof bytes));
}

// Update 30: fast page filter for the hot CBV detours (relaxed atomics, no lock).
size_t pageSlot(uint64_t page) { return static_cast<size_t>((page * 0x9E3779B97F4A7C15ull) >> 54) % watchPageSlots; }

void addWatchPage(uint64_t page) {
    const uint64_t key = page + 1;
    for (unsigned i = 0; i < watchPageProbe; ++i) {
        auto& slot = watchPages[(pageSlot(page) + i) % watchPageSlots];
        uint64_t cur = slot.load(std::memory_order_relaxed);
        if (cur == key)
            return;
        if (cur == 0 && slot.compare_exchange_strong(cur, key, std::memory_order_relaxed))
            return;
        if (cur == key)
            return;
    }
}

bool pageWatched(uint64_t page) {
    const uint64_t key = page + 1;
    for (unsigned i = 0; i < watchPageProbe; ++i) {
        const uint64_t cur = watchPages[(pageSlot(page) + i) % watchPageSlots].load(std::memory_order_relaxed);
        if (cur == key)
            return true;
        if (cur == 0)
            return false;
    }
    return false;
}

void clearBindTraceLocked() {
    // Update 31: depth targets and ortho boxes live under watchLock too.
    depthTargetN = 0;
    orthoN = 0;
    for (auto& d : depthTargets)
        d = {};
    for (auto& o : orthoRows)
        o = {};
    orthoCaptureN = 0; // Update 32 (the array is rewritten before it is read again)
    orthoGroup = {};
    for (auto& row : passBinds)
        for (auto& c : row)
            c.store(0, std::memory_order_relaxed);
    heapStartBinds = 0;
    tableBindsTotal = 0;
    tableHits = 0;
    descCreated = 0;
    descCopied = 0;
    {
        std::lock_guard d(descLock); // lock -> watchLock -> descLock (control path only)
        for (auto& e : descMap)
            e = {};
        descN = 0;
        descCount = 0;
    }
    watchedN = 0;
    bindN = 0;
    for (auto& w : watched)
        w = {};
    for (auto& r : bindRows)
        r = {};
    for (auto& p : watchPages)
        p.store(0, std::memory_order_relaxed);
    for (uint64_t page : seed29Pages) {
        if (watchedN >= address_finder::maxWatched)
            break;
        auto& w = watched[watchedN++];
        w = {};
        w.va = page;
        w.seed = true;
        addWatchPage(page >> 16);
    }
    bindDropped = 0;
    shadowTotal = 0;
}

// Camera data seen at a GPU address: watch it for later binds.
void watchValue(uint64_t va, uint32_t camMask, const char* tag, address_finder::MatchKind kind, bool livePath) {
    if (!va || bindTraceFailed.load())
        return;
    std::unique_lock g(watchLock, std::defer_lock);
    if (livePath) {
        if (!g.try_lock())
            return;
    } else {
        g.lock();
    }
    if (address_finder::noteWatched(watched, watchedN, address_finder::maxWatched, va, camMask, tag, kind,
                                    frameCount.load()) < address_finder::maxWatched)
        addWatchPage(va >> 16);
}

// Store one bind row (render thread: try-lock only, capped per frame).
void storeBind(const address_finder::Watched* w, uint32_t offset, uint32_t slot, uint8_t api, const void* list,
               uint32_t camMask, const char* tag, address_finder::MatchKind kind, uint32_t table = 0) {
    if (bindTracesThisFrame.load() >= maxBindTracesPerFrame)
        return;
    ++bindTracesThisFrame;
    address_finder::BindRow r{};
    r.rva = returnRva();
    r.slot = slot;
    r.offset = offset;
    r.api = api;
    const address_finder::PassTarget t = tlsPass;
    r.pass = address_finder::passKindFor(t, api == 1 || api == 3 || api == 5, list);
    r.table = table;
    r.rtCount = t.list == list ? t.rtCount : 0;
    r.hasDsv = t.list == list && t.hasDsv;
    r.vpW = t.vpW;
    r.vpH = t.vpH;
    r.depthFormat = t.depthFormat;
    r.depthW = t.depthW;
    r.depthH = t.depthH;
    r.seed = w ? w->seed : false;
    r.camMask = w ? w->camMask : camMask;
    r.tag = w ? w->tag : (tag ? tag : "");
    r.kind = w ? w->kind : kind;
    std::unique_lock g(watchLock, std::try_to_lock);
    if (!g.owns_lock())
        return;
    if (!address_finder::noteBind(bindRows, bindN, address_finder::bindRowsCap, r, frameCount.load()))
        ++bindDropped;
    // Update 31: depth-only passes per depth target (tlsDepthTarget set in onBarrier).
    if (r.pass == address_finder::PassKind::depthOnly && tlsDepthTarget >= 0 &&
        static_cast<size_t>(tlsDepthTarget) < depthTargetN)
        ++depthTargets[tlsDepthTarget].depthOnlyBinds;
}

// Update 31: every bind counted by api and pass kind (cheap; no lookup).
void countBind(const void* list, uint8_t api) {
    if (api >= 6)
        return;
    const auto kind = address_finder::passKindFor(tlsPass, api == 1 || api == 3 || api == 5, list);
    passBinds[api][static_cast<uint8_t>(kind) & 3].fetch_add(1, std::memory_order_relaxed);
}

// Root CBV bind: does its 64 KiB window cover watched camera data?
void traceCbvBind(const void* list, uint8_t api, UINT slot, D3D12_GPU_VIRTUAL_ADDRESS va) {
    if (!va || bindTraceFailed.load())
        return;
    const uint64_t page = va >> 16;
    if (!pageWatched(page) && !pageWatched(page + 1))
        return;
    if (bindLookupsThisFrame.load() >= maxBindLookupsPerFrame)
        return;
    ++bindLookupsThisFrame;
    address_finder::Watched hit{};
    uint64_t offset = 0;
    {
        std::unique_lock g(watchLock, std::try_to_lock);
        if (!g.owns_lock())
            return;
        const long i = address_finder::findWatched(watched, watchedN, va, frameCount.load());
        if (i < 0)
            return;
        hit = watched[i];
        offset = hit.seed ? va - hit.va : hit.va - va; // seed: bind's offset into the page
    }
    storeBind(&hit, static_cast<uint32_t>(offset), slot, api, list, 0, "", hit.kind);
}

void recordMatch(const char* source, uint64_t gpuVa, uint32_t rootSlot, const address_finder::Match& m,
                 const address_finder::Camera& cam, bool livePath, uint32_t camMask = 1u, float flatHeadGapM = 0.f) {
    const uint32_t rva = returnRva();
    char detail[160];
    char form[24] = "";
    if (m.kind == address_finder::MatchKind::jitterScaled)
        std::snprintf(form, sizeof form, " %s", address_finder::jitterFormName(m.form));
    else if (m.kind == address_finder::MatchKind::viewProj || m.kind == address_finder::MatchKind::invViewProj)
        std::snprintf(form, sizeof form, " %s", m.form ? "columns" : "rows");
    // Update 29: every camera the hit matched (cams=) and FLAT-ONLY / HEAD-ONLY / EYE-ONLY.
    const std::string cams = address_finder::cameraMaskText(camMask);
    const char* tag = address_finder::cameraTag(camMask, flatHeadGapM);
    std::snprintf(detail, sizeof detail, "%s %s%s%s cams=%s%s%s lag=%u err=%.5g camJ=%.5g,%.5g", source,
                  address_finder::matchKindName(m.kind), form, m.nearMiss ? " NEAR" : "", cams.c_str(),
                  tag[0] ? " " : "", tag, m.lag, static_cast<double>(m.error), static_cast<double>(cam.jitterX),
                  static_cast<double>(cam.jitterY));
    const char* kind = m.nearMiss ? "cb_near" : "cb_match";
    // Update 30: exact camera data at a GPU address is watched for binds; a hit in
    // root constants is itself the bind (the pass target is this thread's).
    if (!m.nearMiss && address_finder::watchKind(m.kind)) {
        if (gpuVa)
            watchValue(gpuVa + m.offset, camMask, tag, m.kind, livePath);
        else if (livePath && tlsConstsList && !bindTraceFailed.load())
            storeBind(nullptr, m.offset, rootSlot, tlsConstsApi, tlsConstsList, camMask, tag, m.kind);
    }
    const bool isJitter =
        m.kind == address_finder::MatchKind::jitter || m.kind == address_finder::MatchKind::jitterScaled;
    float sample[4] = {cam.pos[0], cam.pos[1], cam.pos[2], cam.jitterX + cam.jitterY * 1.618f};
    if (isJitter) {
        sample[0] = cam.jitterX;
        sample[1] = cam.jitterY;
        sample[2] = 0;
        sample[3] = 0;
        if (!m.nearMiss)
            address_finder::noteJitterHit(jitterTrack, m.offset, cam.jitterX, cam.jitterY);
    } else if (m.kind == address_finder::MatchKind::position) {
        sample[0] = cam.pos[0];
        sample[1] = cam.pos[1];
        sample[2] = cam.pos[2];
        sample[3] = 0;
    } else {
        sample[0] = cam.pose.m[12];
        sample[1] = cam.pose.m[13];
        sample[2] = cam.pose.m[14];
        sample[3] = cam.pose.m[0] + cam.pose.m[5] + cam.pose.m[10];
    }
    if (livePath) {
        std::unique_lock g(lock, std::try_to_lock);
        if (!g.owns_lock())
            return;
        addCandidateLocked(kind, detail, rva, 0, "", gpuVa, m.offset, rootSlot, true, sample, m.error,
                           m.nearMiss);
    } else {
        const uint32_t fn = functionRvaFor(rva);
        const std::string sig = sigAtRva(fn ? fn : rva);
        std::lock_guard g(lock);
        addCandidateLocked(kind, detail, rva, fn, sig.c_str(), gpuVa, m.offset, rootSlot, true, sample,
                           m.error, m.nearMiss);
    }
}

void scanFloats(const char* source, const float* data, size_t floats, uint64_t gpuVa, uint32_t rootSlot,
                bool livePath) {
    if (!armed.load() || failed.load() || !data || floats < 4)
        return;
    // Update 28: the newest camera plus the last cameraLags stock cameras: a
    // constant buffer is often built from the previous frame's camera, and
    // while moving that misses every 2e-3 tolerance (27.4: no matrix at all).
    // Update 29: and the head view and both eyes, so a hit says which it was.
    address_finder::Camera cams[address_finder::cameraSlots]{};
    {
        const auto take = [&] {
            cams[0] = stockCam.valid ? stockCam : headCam;
            for (unsigned k = 0; k < address_finder::cameraLags; ++k)
                cams[1 + k] = stockHistory[k];
            cams[address_finder::headSlot] = headCam;
            cams[address_finder::eyeSlot0] = eyeCam[0];
            cams[address_finder::eyeSlot0 + 1] = eyeCam[1];
        };
        if (livePath) {
            std::unique_lock g(lock, std::try_to_lock);
            if (!g.owns_lock())
                return;
            take();
        } else {
            std::lock_guard g(lock);
            take();
        }
    }
    if (!cams[0].valid)
        return;
    float gap = 0.f;
    if (cams[0].valid && cams[address_finder::headSlot].valid) {
        const float* a = cams[0].pos;
        const float* b = cams[address_finder::headSlot].pos;
        gap = std::sqrt((a[0] - b[0]) * (a[0] - b[0]) + (a[1] - b[1]) * (a[1] - b[1]) + (a[2] - b[2]) * (a[2] - b[2]));
    }
    for (const auto& h : address_finder::matchCameras(data, floats, cams, maxGpuHitsPerFrame, true))
        recordMatch(source, gpuVa, rootSlot, h.match, cams[h.best], livePath, h.mask, gap);
    ++live.scans;
}

// Update 31: orthographic boxes (shadow cascades) in uploaded constants, 16-byte aligned.
// Their half sizes say how far a cascade reaches: the hard shadow line's suspects.
// Update 32: up to orthoChain game-module return addresses (RVAs), innermost first,
// from the caller of the copy hook outwards: the upload flush and what called it.
unsigned returnChain(uint32_t* out, unsigned cap) {
    const uintptr_t base = moduleBase();
    if (!base)
        return 0;
    void* frames[24]{};
    const USHORT n = RtlCaptureStackBackTrace(2, 24, frames, nullptr);
    unsigned k = 0;
    for (USHORT i = 0; i < n && k < cap; ++i) {
        const auto a = reinterpret_cast<uintptr_t>(frames[i]);
        if (a >= base && a < base + (512u << 20))
            out[k++] = static_cast<uint32_t>(a - base);
    }
    return k;
}
void compactPose(const address_finder::Camera& c, float* out, uint32_t& valid) {
    valid = c.valid ? 1 : 0;
    for (int i = 0; i < 3; ++i) {
        out[i] = c.valid ? c.pose.m[12 + i] : 0.f;
        out[3 + i] = c.valid ? c.pose.m[8 + i] : 0.f;
    }
}
void scanOrtho(const float* f, size_t floats, uint64_t gpuVa) {
    if (!armed.load())
        return;
    const uint32_t rva = returnRva();
    struct Box {
        uint32_t at; // bytes into the copy
        float half[3];
    } boxes[8];
    unsigned nb = 0;
    for (size_t i = 0; i + 16 <= floats && nb < 8; i += 4) {
        if (f[i + 15] != 1.f)
            continue;
        float half[3]{};
        if (address_finder::orthoBox(f + i, half))
            boxes[nb++] = Box{static_cast<uint32_t>(i * 4), {half[0], half[1], half[2]}};
    }
    if (!nb)
        return;
    // Update 32: the cameras now (lock is the camera lock; never wait on the render thread).
    address_finder::Camera head{}, flat{};
    {
        std::unique_lock c(lock, std::try_to_lock);
        if (c.owns_lock()) {
            head = headCam;
            flat = stockCam;
        }
    }
    uint32_t chain[address_finder::orthoChain]{};
    bool chained = false;
    const uint64_t now = GetTickCount64();
    std::unique_lock g(watchLock, std::try_to_lock);
    if (!g.owns_lock())
        return;
    for (unsigned k = 0; k < nb; ++k) {
        address_finder::noteOrtho(orthoRows, orthoN, rva, boxes[k].at & 255u, boxes[k].half);
        if (orthoCaptureN >= address_finder::maxOrthoCaptures || !address_finder::orthoWorthCapture(boxes[k].half) ||
            !address_finder::orthoTake(orthoGroup, now))
            continue;
        if (!chained) {
            returnChain(chain, address_finder::orthoChain);
            chained = true;
        }
        auto& c = orthoCaptures[orthoCaptureN++];
        c = {};
        c.ms = now - runStartMs.load();
        c.frame = frameCount.load();
        c.gpuVa = gpuVa;
        c.group = orthoGroup.groups;
        c.copyBytes = static_cast<uint32_t>(floats * 4);
        c.matrixOffset = boxes[k].at;
        address_finder::orthoWindow(c.matrixOffset, c.copyBytes, c.windowOffset, c.windowFloats);
        const float* m = f + boxes[k].at / 4;
        c.rowForm = m[3] == 0.f && m[7] == 0.f && m[11] == 0.f ? 1 : 0;
        std::memcpy(c.chain, chain, sizeof chain);
        std::memcpy(c.half, boxes[k].half, sizeof c.half);
        std::memcpy(c.matrix, m, sizeof c.matrix);
        std::memcpy(c.window, f + c.windowOffset / 4, c.windowFloats * 4);
        compactPose(head, c.head, c.headValid);
        compactPose(flat, c.flat, c.flatValid);
    }
}

void scanBytes(const char* source, const void* p, size_t bytes, uint64_t gpuVa = 0, uint32_t rootSlot = 0,
               bool livePath = false) {
    if (!p || bytes < 16)
        return;
    const size_t cap = livePath ? maxLiveScanBytes : maxScanBytes;
    const size_t n = std::min(bytes, cap);
    std::vector<uint8_t> copy(n);
    if (!safeRead(p, copy.data(), copy.size()))
        return;
    scanFloats(source, reinterpret_cast<const float*>(copy.data()), copy.size() / 4, gpuVa, rootSlot, livePath);
    if (livePath && gpuVa && !bindTraceFailed.load())
        scanOrtho(reinterpret_cast<const float*>(copy.data()), copy.size() / 4, gpuVa);
}

// Detours (defined before maybeInstallGpuHooks).
void STDMETHODCALLTYPE detourSetPso(ID3D12GraphicsCommandList* list, ID3D12PipelineState* pso);
void STDMETHODCALLTYPE detourSetGraphicsCbv(ID3D12GraphicsCommandList* list, UINT slot,
                                            D3D12_GPU_VIRTUAL_ADDRESS va);
void STDMETHODCALLTYPE detourSetComputeCbv(ID3D12GraphicsCommandList* list, UINT slot,
                                           D3D12_GPU_VIRTUAL_ADDRESS va);
void STDMETHODCALLTYPE detourSetGraphicsConsts(ID3D12GraphicsCommandList* list, UINT slot, UINT num,
                                               const void* data);
void STDMETHODCALLTYPE detourSetComputeConsts(ID3D12GraphicsCommandList* list, UINT slot, UINT num,
                                              const void* data);
void STDMETHODCALLTYPE detourCopyBuffer(ID3D12GraphicsCommandList* list, ID3D12Resource* dst, UINT64 dstOff,
                                        ID3D12Resource* src, UINT64 srcOff, UINT64 bytes);
void STDMETHODCALLTYPE detourDispatch(ID3D12GraphicsCommandList* list, UINT x, UINT y, UINT z);
void STDMETHODCALLTYPE detourDrawIndexed(ID3D12GraphicsCommandList* list, UINT count, UINT start, INT base,
                                         UINT inst);

void maybeInstallPassHooks(ID3D12GraphicsCommandList* list); // Update 30, below
void maybeInstallTableHooks(ID3D12GraphicsCommandList* list); // Update 31, below

void maybeInstallGpuHooks(ID3D12GraphicsCommandList* list) {
    if (hooksOn.load() || !list || failed.load())
        return;
    void** vt = sehVtable(list);
    if (!vt)
        return;
    void* targets[] = {vt[13], vt[14], vt[15], vt[25], vt[35], vt[36], vt[37], vt[38]};
    auto s = MH_Initialize();
    if (s != MH_OK && s != MH_ERROR_ALREADY_INITIALIZED) {
        fail(2701, "MH_Initialize for GPU hooks");
        return;
    }
    void* dets[] = {reinterpret_cast<void*>(detourDrawIndexed), reinterpret_cast<void*>(detourDispatch),
                    reinterpret_cast<void*>(detourCopyBuffer), reinterpret_cast<void*>(detourSetPso),
                    reinterpret_cast<void*>(detourSetComputeConsts),
                    reinterpret_cast<void*>(detourSetGraphicsConsts),
                    reinterpret_cast<void*>(detourSetComputeCbv),
                    reinterpret_cast<void*>(detourSetGraphicsCbv)};
    void** originals[] = {reinterpret_cast<void**>(&originalDrawIndexed),
                          reinterpret_cast<void**>(&originalDispatch),
                          reinterpret_cast<void**>(&originalCopyBuffer),
                          reinterpret_cast<void**>(&originalSetPso),
                          reinterpret_cast<void**>(&originalSetComputeConsts),
                          reinterpret_cast<void**>(&originalSetGraphicsConsts),
                          reinterpret_cast<void**>(&originalSetComputeCbv),
                          reinterpret_cast<void**>(&originalSetGraphicsCbv)};
    for (int i = 0; i < 8; ++i) {
        s = MH_CreateHook(targets[i], dets[i], originals[i]);
        if (s != MH_OK && s != MH_ERROR_ALREADY_CREATED) {
            fail(2702 + static_cast<uint32_t>(i), "MH_CreateHook GPU");
            return;
        }
        gpuHooks[i] = targets[i];
        if (MH_EnableHook(targets[i]) != MH_OK) {
            fail(2710 + static_cast<uint32_t>(i), "MH_EnableHook GPU");
            return;
        }
    }
    hooksOn = true;
    maybeInstallPassHooks(list); // Update 30
    maybeInstallTableHooks(list); // Update 31
}

// Update 30: pass targets. Only the count, DSV presence and viewport size are
// kept (no descriptor dereference); SEH copy of the viewport.
void STDMETHODCALLTYPE detourOmSetRt(ID3D12GraphicsCommandList* list, UINT num,
                                     const D3D12_CPU_DESCRIPTOR_HANDLE* rtvs, BOOL single,
                                     const D3D12_CPU_DESCRIPTOR_HANDLE* dsv) {
    if (armed.load()) {
        tlsPass.list = list;
        tlsPass.rtCount = num;
        tlsPass.hasDsv = dsv != nullptr;
        tlsPass.known = true;
    }
    originalOmSetRt(list, num, rtvs, single, dsv);
}
void STDMETHODCALLTYPE detourRsSetViewports(ID3D12GraphicsCommandList* list, UINT num, const D3D12_VIEWPORT* vps) {
    if (armed.load() && num && vps) {
        D3D12_VIEWPORT v{};
        if (safeRead(vps, &v, sizeof v) && v.Width > 0 && v.Height > 0 && v.Width < 65536 && v.Height < 65536) {
            tlsPass.vpW = static_cast<uint32_t>(v.Width);
            tlsPass.vpH = static_cast<uint32_t>(v.Height);
        }
    }
    originalRsSetViewports(list, num, vps);
}

// Separate from the main GPU hooks: a failure here only leaves passes "unknown".
void maybeInstallPassHooks(ID3D12GraphicsCommandList* list) {
    if (passHooksOn.load() || passHooksFailed.load() || !list || failed.load())
        return;
    void** vt = sehVtable(list);
    if (!vt)
        return;
    void* targets[] = {vt[46], vt[21]}; // OMSetRenderTargets, RSSetViewports
    void* dets[] = {reinterpret_cast<void*>(detourOmSetRt), reinterpret_cast<void*>(detourRsSetViewports)};
    void** originals[] = {reinterpret_cast<void**>(&originalOmSetRt),
                          reinterpret_cast<void**>(&originalRsSetViewports)};
    for (int i = 0; i < 2; ++i) {
        void* t{};
        if (!safeRead(&targets[i], &t, sizeof t) || !t) {
            passHooksFailed = true;
            break;
        }
        auto s = MH_CreateHook(t, dets[i], originals[i]);
        if ((s != MH_OK && s != MH_ERROR_ALREADY_CREATED) || !*originals[i] || MH_EnableHook(t) != MH_OK) {
            passHooksFailed = true;
            break;
        }
        passHooks[i] = t;
    }
    if (passHooksFailed.load()) {
        for (void*& h : passHooks)
            if (h) {
                MH_DisableHook(h);
                h = nullptr;
            }
        OutputDebugStringA("ADDRESS FINDER: pass-target hooks off (2730); binds report pass=unknown\n");
        return;
    }
    passHooksOn = true;
}

// Update 31: DESCRIPTOR TABLES (address_finder.hpp). Its own hook group: a failure
// here disables only the table trace (one log line, code 2740) and the report says so.
bool rangeWatched(uint64_t va, uint64_t bytes) {
    if (!va || !bytes)
        return false;
    const uint64_t last = (va + bytes - 1) >> 16;
    for (uint64_t page = va >> 16, k = 0; page <= last && k < 64; ++page, ++k)
        if (pageWatched(page))
            return true;
    return false;
}

bool descBudget(uint32_t ops) {
    if (descOpsThisFrame.load(std::memory_order_relaxed) + ops > maxDescOpsPerFrame)
        return false;
    descOpsThisFrame.fetch_add(ops, std::memory_order_relaxed);
    return true;
}

void noteDescriptor(uint64_t cpu, uint64_t va, uint64_t bytes) {
    if (!armed.load() || failed.load() || tableHooksFailed.load() || !cpu)
        return;
    const bool watch = rangeWatched(va, bytes);
    if (!watch && !descCount.load(std::memory_order_relaxed))
        return; // nothing mapped: no lookup at all
    if (!descBudget(1))
        return;
    std::unique_lock g(descLock, std::try_to_lock);
    if (!g.owns_lock())
        return;
    if (watch) {
        if (address_finder::noteDesc(descMap, descN, cpu, va, static_cast<uint32_t>(std::min<uint64_t>(bytes, ~0u)),
                                     frameCount.load()))
            ++descCreated;
    } else {
        address_finder::forgetDesc(descMap, descN, cpu);
    }
    descCount = descN;
}

void STDMETHODCALLTYPE detourCreateCbv(ID3D12Device* dev, const D3D12_CONSTANT_BUFFER_VIEW_DESC* desc,
                                       D3D12_CPU_DESCRIPTOR_HANDLE cpu) {
    originalCreateCbv(dev, desc, cpu);
    if (!armed.load() || !desc)
        return;
    D3D12_CONSTANT_BUFFER_VIEW_DESC d{};
    if (safeRead(desc, &d, sizeof d))
        noteDescriptor(cpu.ptr, d.BufferLocation, d.SizeInBytes);
}

void STDMETHODCALLTYPE detourCreateSrv(ID3D12Device* dev, ID3D12Resource* res,
                                       const D3D12_SHADER_RESOURCE_VIEW_DESC* desc, D3D12_CPU_DESCRIPTOR_HANDLE cpu) {
    originalCreateSrv(dev, res, desc, cpu);
    if (!armed.load() || !desc || !res)
        return;
    D3D12_SHADER_RESOURCE_VIEW_DESC d{};
    if (!safeRead(desc, &d, sizeof d) || d.ViewDimension != D3D12_SRV_DIMENSION_BUFFER)
        return; // buffers only
    D3D12_GPU_VIRTUAL_ADDRESS base = 0;
    if (!sehGpuVa(res, &base) || !base)
        return;
    // Structured: the stride; raw: 4 bytes; typed: assume 16 (a float4), the widest common case.
    const uint64_t stride = d.Buffer.StructureByteStride ? d.Buffer.StructureByteStride
                            : (d.Buffer.Flags & D3D12_BUFFER_SRV_FLAG_RAW) ? 4 : 16;
    const uint64_t bytes = std::min<uint64_t>(static_cast<uint64_t>(d.Buffer.NumElements) * stride, 64ull << 20);
    noteDescriptor(cpu.ptr, base + d.Buffer.FirstElement * stride, bytes);
}

void STDMETHODCALLTYPE detourCopySimple(ID3D12Device* dev, UINT num, D3D12_CPU_DESCRIPTOR_HANDLE dst,
                                        D3D12_CPU_DESCRIPTOR_HANDLE src, D3D12_DESCRIPTOR_HEAP_TYPE type) {
    originalCopySimple(dev, num, dst, src, type);
    if (!armed.load() || failed.load() || tableHooksFailed.load() || type != D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV ||
        !descCount.load(std::memory_order_relaxed) || !num)
        return;
    const uint32_t inc = descInc.load();
    const UINT n = num > 256 ? 256 : num;
    if (!inc || !descBudget(n))
        return;
    std::unique_lock g(descLock, std::try_to_lock);
    if (!g.owns_lock())
        return;
    for (UINT i = 0; i < n; ++i) {
        const uint64_t from = src.ptr + static_cast<uint64_t>(i) * inc, to = dst.ptr + static_cast<uint64_t>(i) * inc;
        const long at = address_finder::findDesc(descMap, from);
        if (at >= 0) {
            const auto e = descMap[at];
            if (address_finder::noteDesc(descMap, descN, to, e.va, e.bytes, frameCount.load()))
                ++descCopied;
        } else {
            address_finder::forgetDesc(descMap, descN, to);
        }
    }
    descCount = descN;
}

void STDMETHODCALLTYPE detourSetHeaps(ID3D12GraphicsCommandList* list, UINT num, ID3D12DescriptorHeap* const* heaps) {
    if (armed.load() && !tableHooksFailed.load() && heaps && num) {
        for (UINT i = 0; i < num && i < 4; ++i) {
            ID3D12DescriptorHeap* h{};
            if (!safeRead(&heaps[i], &h, sizeof h) || !h || h == heapSeen.load())
                continue;
            D3D12_DESCRIPTOR_HEAP_DESC d{};
            uint64_t cpu{}, gpu{};
            if (!sehHeapInfo(h, &d, &cpu, &gpu) || d.Type != D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV ||
                !(d.Flags & D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE))
                continue;
            heapCpu = cpu;
            heapGpu = gpu;
            heapCount = d.NumDescriptors;
            heapSeen = h;
        }
    }
    originalSetHeaps(list, num, heaps);
}

void traceTableBind(ID3D12GraphicsCommandList* list, uint8_t api, UINT slot, D3D12_GPU_DESCRIPTOR_HANDLE gpu) {
    countBind(list, api);
    ++tableBindsTotal;
    const address_finder::HeapInfo heap{heapCpu.load(), heapGpu.load(), heapCount.load(), descInc.load()};
    const long index = address_finder::tableIndex(heap, gpu.ptr);
    if (index == 0)
        ++heapStartBinds;
    if (index < 0 || !descCount.load(std::memory_order_relaxed) || bindTraceFailed.load())
        return;
    if (bindLookupsThisFrame.load() >= maxBindLookupsPerFrame || !descBudget(address_finder::tableWindow))
        return;
    ++bindLookupsThisFrame;
    address_finder::DescEntry found[4]{};
    uint32_t foundAt[4]{};
    unsigned nf = 0;
    {
        std::unique_lock g(descLock, std::try_to_lock);
        if (!g.owns_lock())
            return;
        for (uint32_t k = 0; k < address_finder::tableWindow && nf < 4; ++k) {
            if (static_cast<uint64_t>(index) + k >= heap.count)
                break;
            const long at = address_finder::findDesc(descMap, address_finder::cpuAt(heap, index + k));
            if (at >= 0) {
                found[nf] = descMap[at];
                foundAt[nf++] = k;
            }
        }
    }
    for (unsigned j = 0; j < nf; ++j) {
        address_finder::Watched hit{};
        {
            std::unique_lock g(watchLock, std::try_to_lock);
            if (!g.owns_lock())
                return;
            const long i = address_finder::findWatchedIn(watched, watchedN, found[j].va, found[j].bytes,
                                                         frameCount.load());
            if (i < 0)
                continue;
            hit = watched[i];
        }
        ++tableHits;
        const uint64_t offset = hit.seed ? 0 : hit.va - found[j].va;
        storeBind(&hit, static_cast<uint32_t>(offset), slot, api, list, 0, "", hit.kind, foundAt[j] + 1);
    }
}

void STDMETHODCALLTYPE detourSetComputeTable(ID3D12GraphicsCommandList* list, UINT slot,
                                             D3D12_GPU_DESCRIPTOR_HANDLE gpu) {
    if (armed.load() && !failed.load() && !tableHooksFailed.load())
        traceTableBind(list, 5, slot, gpu);
    originalSetComputeTable(list, slot, gpu);
}

void STDMETHODCALLTYPE detourSetGraphicsTable(ID3D12GraphicsCommandList* list, UINT slot,
                                              D3D12_GPU_DESCRIPTOR_HANDLE gpu) {
    if (armed.load() && !failed.load() && !tableHooksFailed.load())
        traceTableBind(list, 4, slot, gpu);
    originalSetGraphicsTable(list, slot, gpu);
}

void maybeInstallTableHooks(ID3D12GraphicsCommandList* list) {
    if (tableHooksOn.load() || tableHooksFailed.load() || !list || failed.load())
        return;
    void** vt = sehVtable(list);
    UINT inc = 0;
    ID3D12Device* dev = vt ? sehDeviceOf(list, &inc) : nullptr;
    void** dvt = dev ? sehVtable(dev) : nullptr;
    void* targets[6]{};
    // d3d12.h: list 28 SetDescriptorHeaps, 31 SetComputeRootDescriptorTable,
    // 32 SetGraphicsRootDescriptorTable; device 17 CreateConstantBufferView,
    // 18 CreateShaderResourceView, 24 CopyDescriptorsSimple.
    bool ok = vt && dvt && inc && safeRead(&vt[28], &targets[0], sizeof(void*)) &&
              safeRead(&vt[31], &targets[1], sizeof(void*)) && safeRead(&vt[32], &targets[2], sizeof(void*)) &&
              safeRead(&dvt[17], &targets[3], sizeof(void*)) && safeRead(&dvt[18], &targets[4], sizeof(void*)) &&
              safeRead(&dvt[24], &targets[5], sizeof(void*));
    sehRelease(dev); // vtable entries are code in d3d12core: they outlive the reference
    if (ok) {
        descInc = inc;
        void* dets[] = {reinterpret_cast<void*>(detourSetHeaps),     reinterpret_cast<void*>(detourSetComputeTable),
                        reinterpret_cast<void*>(detourSetGraphicsTable), reinterpret_cast<void*>(detourCreateCbv),
                        reinterpret_cast<void*>(detourCreateSrv),    reinterpret_cast<void*>(detourCopySimple)};
        void** originals[] = {reinterpret_cast<void**>(&originalSetHeaps),
                              reinterpret_cast<void**>(&originalSetComputeTable),
                              reinterpret_cast<void**>(&originalSetGraphicsTable),
                              reinterpret_cast<void**>(&originalCreateCbv),
                              reinterpret_cast<void**>(&originalCreateSrv),
                              reinterpret_cast<void**>(&originalCopySimple)};
        // Originals first (all six), then enable: a detour never runs with a null original.
        for (int i = 0; i < 6 && ok; ++i) {
            if (!targets[i]) {
                ok = false;
                break;
            }
            const auto st = MH_CreateHook(targets[i], dets[i], originals[i]);
            ok = (st == MH_OK || st == MH_ERROR_ALREADY_CREATED) && *originals[i];
            if (ok)
                tableHooks[i] = targets[i];
        }
        for (int i = 0; i < 6 && ok; ++i)
            ok = MH_EnableHook(targets[i]) == MH_OK;
    }
    if (!ok) {
        tableHooksFailed = true;
        for (void*& h : tableHooks)
            if (h) {
                MH_DisableHook(h);
                h = nullptr;
            }
        OutputDebugStringA("ADDRESS FINDER: descriptor-table hooks off (2740); table binds not traced\n");
        return;
    }
    tableHooksOn = true;
}

void STDMETHODCALLTYPE detourSetPso(ID3D12GraphicsCommandList* list, ID3D12PipelineState* pso) {
    tlsPso = pso;
    originalSetPso(list, pso);
}
void STDMETHODCALLTYPE detourSetGraphicsCbv(ID3D12GraphicsCommandList* list, UINT slot,
                                            D3D12_GPU_VIRTUAL_ADDRESS va) {
    if (armed.load() && !failed.load())
    {
        countBind(list, 0); // Update 31
        traceCbvBind(list, 0, slot, va); // Update 30
    }
    if (armed.load() && !failed.load() && liveBudgetAllows()) {
        noteLiveOp();
        const uint32_t rva = returnRva();
        char detail[96];
        std::snprintf(detail, sizeof detail, "SetGraphicsRootCBV slot=%u", slot);
        addCandidateLive("root_cbv", detail, rva, va, 0, slot);
    }
    originalSetGraphicsCbv(list, slot, va);
}
void STDMETHODCALLTYPE detourSetComputeCbv(ID3D12GraphicsCommandList* list, UINT slot,
                                           D3D12_GPU_VIRTUAL_ADDRESS va) {
    if (armed.load() && !failed.load())
    {
        countBind(list, 1); // Update 31
        traceCbvBind(list, 1, slot, va); // Update 30
    }
    if (armed.load() && !failed.load() && liveBudgetAllows()) {
        noteLiveOp();
        const uint32_t rva = returnRva();
        char detail[96];
        std::snprintf(detail, sizeof detail, "SetComputeRootCBV slot=%u", slot);
        addCandidateLive("root_cbv", detail, rva, va, 0, slot);
    }
    originalSetComputeCbv(list, slot, va);
}
void STDMETHODCALLTYPE detourSetGraphicsConsts(ID3D12GraphicsCommandList* list, UINT slot, UINT num,
                                               const void* data) {
    if (armed.load() && !failed.load())
        countBind(list, 2); // Update 31
    if (armed.load() && !failed.load() && data && num && liveBudgetAllows() &&
        constScansThisFrame.load() < maxConstScansPerFrame) {
        ++constScansThisFrame;
        noteLiveOp();
        const UINT n = num > 64 ? 64 : num; // hard cap: 256 bytes of root consts
        tlsConstsList = list;
        tlsConstsApi = 2;
        scanFloats("graphics_consts", static_cast<const float*>(data), n, 0, slot, true);
        tlsConstsList = nullptr;
    }
    originalSetGraphicsConsts(list, slot, num, data);
}
void STDMETHODCALLTYPE detourSetComputeConsts(ID3D12GraphicsCommandList* list, UINT slot, UINT num,
                                              const void* data) {
    if (armed.load() && !failed.load())
        countBind(list, 3); // Update 31
    if (armed.load() && !failed.load() && data && num && liveBudgetAllows() &&
        constScansThisFrame.load() < maxConstScansPerFrame) {
        ++constScansThisFrame;
        noteLiveOp();
        const UINT n = num > 64 ? 64 : num;
        tlsConstsList = list;
        tlsConstsApi = 3;
        scanFloats("compute_consts", static_cast<const float*>(data), n, 0, slot, true);
        tlsConstsList = nullptr;
    }
    originalSetComputeConsts(list, slot, num, data);
}
void STDMETHODCALLTYPE detourCopyBuffer(ID3D12GraphicsCommandList* list, ID3D12Resource* dst, UINT64 dstOff,
                                        ID3D12Resource* src, UINT64 srcOff, UINT64 bytes) {
    // Update 33: SHADOW DISTANCE's light watch (the finder armed or not): small uploads only,
    // a few per 10 ms (more from sites sun boxes came from), read under SEH, boxes to its watch.
    if (native_shadow_distance::watching() && src && bytes >= 1024 && bytes <= (64u << 10)) {
        const uintptr_t base = moduleBase();
        const auto ret = reinterpret_cast<uintptr_t>(SPIDY_RETURN_ADDRESS());
        const uint32_t site = base && ret > base && ret < base + (512u << 20) ? static_cast<uint32_t>(ret - base) : 0;
        if (native_shadow_distance::takeCopy(site)) {
            void* mapped{};
            const D3D12_RANGE read{static_cast<SIZE_T>(srcOff), static_cast<SIZE_T>(srcOff + bytes)};
            D3D12_RANGE empty{0, 0};
            if (SUCCEEDED(sehMap(src, 0, &read, &mapped)) && mapped) {
                native_shadow_distance::noteUpload(static_cast<const uint8_t*>(mapped) + static_cast<size_t>(srcOff),
                                                   static_cast<size_t>(bytes), site);
                sehUnmap(src, 0, &empty);
            }
        }
    }
    if (armed.load() && !failed.load() && src && bytes >= 64 && bytes <= (1u << 20) && liveBudgetAllows() &&
        copyScansThisFrame.load() < maxCopyScansPerFrame) {
        ++copyScansThisFrame;
        noteLiveOp();
        void* mapped{};
        const UINT64 sample = bytes > maxLiveScanBytes ? maxLiveScanBytes : bytes;
        const D3D12_RANGE read{static_cast<SIZE_T>(srcOff), static_cast<SIZE_T>(srcOff + sample)};
        D3D12_RANGE empty{0, 0};
        const HRESULT hr = sehMap(src, 0, &read, &mapped);
        if (SUCCEEDED(hr) && mapped) {
            const auto* p = static_cast<const uint8_t*>(mapped) + static_cast<size_t>(srcOff);
            D3D12_GPU_VIRTUAL_ADDRESS va = 0;
            D3D12_GPU_VIRTUAL_ADDRESS base = 0;
            if (dst && sehGpuVa(dst, &base))
                va = base + dstOff;
            scanBytes("copy_buffer", p, static_cast<size_t>(sample), va, 0, true);
            sehUnmap(src, 0, &empty);
        }
    }
    originalCopyBuffer(list, dst, dstOff, src, srcOff, bytes);
}
void STDMETHODCALLTYPE detourDispatch(ID3D12GraphicsCommandList* list, UINT x, UINT y, UINT z) {
    if (armed.load() && !failed.load() && liveBudgetAllows() && (frameCount.load() & 31u) == 0) {
        noteLiveOp();
        const uint32_t rva = returnRva();
        char detail[96];
        std::snprintf(detail, sizeof detail, "Dispatch %ux%ux%u", x, y, z);
        addCandidateLive("dispatch", detail, rva);
    }
    originalDispatch(list, x, y, z);
}
void STDMETHODCALLTYPE detourDrawIndexed(ID3D12GraphicsCommandList* list, UINT count, UINT start, INT base,
                                         UINT inst) {
    if (armed.load() && !failed.load() && count > 0 && (frameCount.load() & 63u) == 0 && liveBudgetAllows()) {
        noteLiveOp();
        const uint32_t rva = returnRva();
        char detail[96];
        std::snprintf(detail, sizeof detail, "DrawIndexed %u", count);
        addCandidateLive("draw_indexed", detail, rva);
    }
    originalDrawIndexed(list, count, start, base, inst);
}

// ---- 0.0.73 SL WATCH (sl_watch.hpp): what the game hands Streamline, in a normal session ----------------------
// The detours below always pass the call on unchanged; the watch only reads. Off: one relaxed load per call.
std::atomic<uint32_t> slWatchOn{1};
SRWLOCK slWatchLock = SRWLOCK_INIT;
std::atomic<uintptr_t> slLo{}, slHi{}; // sl.interposer.dll's image, for the frame token's virtual

uint32_t sehFrameIndex(uintptr_t fn, const void* token) {
    __try {
        return reinterpret_cast<uint32_t (*)(const void*)>(fn)(token);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return spidy::sl_watch::noFrame;
    }
}
// The FrameToken's index (its virtual operator uint32_t, called only when the token's GUID matches and the
// function lies in sl.interposer.dll); raw gets the dword at +40 (CANDIDATE: the interposer's own counter).
uint32_t slFrameIndex(uint64_t token, uint32_t& raw) {
    namespace sw = spidy::sl_watch;
    raw = sw::noFrame;
    uint8_t head[sw::tokenRawAt + 4]{};
    if (!token || !safeRead(reinterpret_cast<const void*>(token), head, sizeof head))
        return sw::noFrame;
    raw = sw::u32(head + sw::tokenRawAt);
    if (sw::u32(head + sw::tokenGuidAt) != sw::frameTokenGuid)
        return sw::noFrame;
    uintptr_t vtbl{}, fn{};
    std::memcpy(&vtbl, head, sizeof vtbl);
    if (!vtbl || !safeRead(reinterpret_cast<const void*>(vtbl), &fn, sizeof fn))
        return sw::noFrame;
    const uintptr_t lo = slLo.load(std::memory_order_relaxed), hi = slHi.load(std::memory_order_relaxed);
    if (!lo || fn < lo || fn >= hi)
        return sw::noFrame;
    return sehFrameIndex(fn, reinterpret_cast<const void*>(token));
}
// A ViewportHandle's value (+32), checked by its GUID; noFrame when it is not one.
uint32_t slViewportId(uint64_t handle) {
    namespace sw = spidy::sl_watch;
    uint8_t h[sw::viewportValueAt + 4]{};
    if (!handle || !safeRead(reinterpret_cast<const void*>(handle), h, sizeof h) ||
        sw::u32(h + sw::guidAt) != sw::viewportGuid)
        return sw::noFrame;
    return sw::u32(h + sw::viewportValueAt);
}
void slWatchConstants(uint64_t a0, uint64_t a1, uint64_t a2) {
    namespace sw = spidy::sl_watch;
    auto& d = SpidySlWatchData;
    const uint64_t call = static_cast<uint64_t>(
        InterlockedIncrement64(reinterpret_cast<volatile LONG64*>(&d.constantsCalls)));
    uint32_t raw{};
    const uint32_t frame = slFrameIndex(a1, raw);
    const uint32_t vp = slViewportId(a2);
    uint8_t resetFlag{};
    const bool isConstants = a0 && safeRead(reinterpret_cast<const void*>(a0 + sw::flagsAt + sw::reset), &resetFlag, 1);
    const bool sample = sw::wantSample(frame, call, d.every);
    sw::Sample s{};
    bool decoded = false;
    if (sample && isConstants) {
        uint8_t c[sw::constantsBytes];
        if (safeRead(reinterpret_cast<const void*>(a0), c, sizeof c) && sw::decode(c, s)) {
            decoded = true;
            s.call = static_cast<uint32_t>(call);
            s.frame = frame;
            s.frameRaw = raw;
            s.viewport = vp;
            s.callerRva = returnRva();
        }
    }
    if (!TryAcquireSRWLockExclusive(&slWatchLock))
        return; // never wait on the render thread
    InterlockedIncrement64(reinterpret_cast<volatile LONG64*>(&d.sequence));
    if (const int slot = sw::slotFor(d.views, vp); slot >= 0) {
        auto& v = d.views[slot];
        v.id = vp;
        ++v.constants;
        if (frame != v.lastFrame && frame != sw::noFrame && v.lastFrame != sw::noFrame && slot == 0)
            ++d.distinctFrames;
        v.lastFrame = frame;
        if (resetFlag == 1)
            ++v.resets;
    } else {
        ++d.overflow;
    }
    if (decoded) {
        d.ring[d.next % sw::ringSize] = s;
        d.next = (d.next + 1) % sw::ringSize;
        ++d.samples;
    } else if (sample) {
        ++d.foreign;
    }
    InterlockedIncrement64(reinterpret_cast<volatile LONG64*>(&d.sequence));
    ReleaseSRWLockExclusive(&slWatchLock);
}
// slSetTag(viewport, tags, n) and slSetTagForFrame(token, viewport, tags, n): which buffers each viewport tags.
void slWatchTags(uint64_t viewport, uint64_t tags, uint64_t count) {
    namespace sw = spidy::sl_watch;
    auto& d = SpidySlWatchData;
    InterlockedIncrement64(reinterpret_cast<volatile LONG64*>(&d.tagCalls));
    const uint32_t vp = slViewportId(viewport);
    uint32_t mask = 0;
    const uint64_t n = count < sw::maxTags ? count : sw::maxTags;
    for (uint64_t i = 0; tags && i < n; ++i) {
        uint32_t type{};
        if (safeRead(reinterpret_cast<const void*>(tags + i * sw::tagBytes + sw::tagTypeAt), &type, sizeof type))
            mask |= sw::tagBit(type);
    }
    if (!TryAcquireSRWLockExclusive(&slWatchLock))
        return;
    InterlockedIncrement64(reinterpret_cast<volatile LONG64*>(&d.sequence));
    if (const int slot = sw::slotFor(d.views, vp); slot >= 0) {
        d.views[slot].id = vp;
        ++d.views[slot].tagCalls;
        d.views[slot].tagMask |= mask;
    } else {
        ++d.overflow;
    }
    InterlockedIncrement64(reinterpret_cast<volatile LONG64*>(&d.sequence));
    ReleaseSRWLockExclusive(&slWatchLock);
}
// slEvaluateFeature(feature, token, inputs, n, cmd): the viewport among its inputs.
void slWatchEvaluate(uint64_t inputs, uint64_t count) {
    namespace sw = spidy::sl_watch;
    auto& d = SpidySlWatchData;
    InterlockedIncrement64(reinterpret_cast<volatile LONG64*>(&d.evaluateCalls));
    uint32_t vp = sw::noFrame;
    const uint64_t n = count < sw::maxInputs ? count : sw::maxInputs;
    for (uint64_t i = 0; inputs && i < n && vp == sw::noFrame; ++i) {
        uint64_t p{};
        if (safeRead(reinterpret_cast<const void*>(inputs + i * 8), &p, sizeof p))
            vp = slViewportId(p);
    }
    if (vp == sw::noFrame || !TryAcquireSRWLockExclusive(&slWatchLock))
        return;
    InterlockedIncrement64(reinterpret_cast<volatile LONG64*>(&d.sequence));
    if (const int slot = sw::slotFor(d.views, vp); slot >= 0) {
        d.views[slot].id = vp;
        ++d.views[slot].evaluates;
    } else {
        ++d.overflow;
    }
    InterlockedIncrement64(reinterpret_cast<volatile LONG64*>(&d.sequence));
    ReleaseSRWLockExclusive(&slWatchLock);
}
bool slWatching() { return slWatchOn.load(std::memory_order_relaxed) != 0; }

uint64_t hookExport(ExportFn* original, uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4,
                    uint64_t a5, const char* tag, bool decodeSlConstants) {
    if (armed.load() && !failed.load()) {
        const uint32_t rva = returnRva();
        char detail[160];
        std::snprintf(detail, sizeof detail, "%s", tag);
        addCandidateLive("export", detail, rva);
        ++live.exportHits;
        if (decodeSlConstants && a0) {
            uint8_t head[64]{};
            if (safeRead(reinterpret_cast<const void*>(a0), head, sizeof head)) {
                const uint32_t d1 = *reinterpret_cast<uint32_t*>(head + 8);
                const uint16_t d2 = *reinterpret_cast<uint16_t*>(head + 12);
                const uint16_t d3 = *reinterpret_cast<uint16_t*>(head + 14);
                if (d1 == 0xdcd35ad7 && d2 == 0x4e4a && d3 == 0x4bad) {
                    float body[120]{};
                    if (safeRead(reinterpret_cast<const void*>(a0 + 32), body, sizeof body)) {
                        // jitterOffset at Constants +352 → body index (352-32)/4 = 80
                        const float jx = body[80], jy = body[81];
                        char jd[96];
                        std::snprintf(jd, sizeof jd, "slSetConstants jitter=%.6g,%.6g",
                                      static_cast<double>(jx), static_cast<double>(jy));
                        if (!address_finder::isTrivialScalar(jx) || !address_finder::isTrivialScalar(jy)) {
                            float sample[4] = {jx, jy, 0, 0};
                            std::unique_lock g(lock, std::try_to_lock);
                            if (g.owns_lock())
                                addCandidateLocked("sl_constants", jd, rva, 0, "", 0, 0, 0, true, sample, 0.f);
                        }
                        scanFloats("sl_matrices", body, 80, 0, 0, true);
                    }
                }
            }
        }
        if (std::strcmp(tag, "xessD3D12Execute") == 0 && a2) {
            float jitter[2]{};
            uint32_t sizes[2]{};
            if (safeRead(reinterpret_cast<const void*>(a2 + 48), jitter, 8) &&
                safeRead(reinterpret_cast<const void*>(a2 + 64), sizes, 8)) {
                char jd[96];
                std::snprintf(jd, sizeof jd, "xess jitter=%.6g,%.6g size=%ux%u", static_cast<double>(jitter[0]),
                              static_cast<double>(jitter[1]), sizes[0], sizes[1]);
                if (!address_finder::isTrivialScalar(jitter[0]) || !address_finder::isTrivialScalar(jitter[1])) {
                    float sample[4] = {jitter[0], jitter[1], static_cast<float>(sizes[0]),
                                       static_cast<float>(sizes[1])};
                    std::unique_lock g(lock, std::try_to_lock);
                    if (g.owns_lock())
                        addCandidateLocked("xess_exec", jd, rva, 0, "", 0, 0, 0, true, sample, 0.f);
                }
            }
        }
        (void)a1;
        (void)a3;
        (void)a4;
        (void)a5;
    }
    return (*original)(a0, a1, a2, a3, a4, a5);
}

uint64_t detourSlSetConstants(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5) {
    if (slWatching())
        slWatchConstants(a0, a1, a2);
    return hookExport(&originalSlSetConstants, a0, a1, a2, a3, a4, a5, "slSetConstants", true);
}
uint64_t detourSlEvaluate(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5) {
    if (slWatching())
        slWatchEvaluate(a2, a3 & 0xffffffffu);
    return hookExport(&originalSlEvaluate, a0, a1, a2, a3, a4, a5, "slEvaluateFeature", false);
}
uint64_t detourSlSetTag(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5) {
    if (slWatching())
        slWatchTags(a0, a1, a2 & 0xffffffffu);
    return hookExport(&originalSlSetTag, a0, a1, a2, a3, a4, a5, "slSetTag", false);
}
uint64_t detourSlSetTagForFrame(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5) {
    if (slWatching())
        slWatchTags(a1, a2, a3 & 0xffffffffu);
    return hookExport(&originalSlSetTagForFrame, a0, a1, a2, a3, a4, a5, "slSetTagForFrame", false);
}
uint64_t detourNgxEval(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5) {
    return hookExport(&originalNgxEval, a0, a1, a2, a3, a4, a5, "NGX_EvaluateFeature", false);
}
uint64_t detourNgxEvalC(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5) {
    return hookExport(&originalNgxEvalC, a0, a1, a2, a3, a4, a5, "NGX_EvaluateFeature_C", false);
}
uint64_t detourXessExec(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5) {
    return hookExport(&originalXessExec, a0, a1, a2, a3, a4, a5, "xessD3D12Execute", false);
}
uint64_t detourFsrDispatch(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5) {
    return hookExport(&originalFsrDispatch, a0, a1, a2, a3, a4, a5, "ffxDispatch", false);
}

void tryHookExport(const wchar_t* mod, const char* name, ExportFn detour, ExportFn* original, const char* tag) {
    const HMODULE h = GetModuleHandleW(mod);
    if (!h)
        return;
    void* p = reinterpret_cast<void*>(GetProcAddress(h, name));
    if (!p)
        return;
    auto s = MH_Initialize();
    if (s != MH_OK && s != MH_ERROR_ALREADY_INITIALIZED)
        return;
    s = MH_CreateHook(p, reinterpret_cast<void*>(detour), reinterpret_cast<void**>(original));
    if (s != MH_OK && s != MH_ERROR_ALREADY_CREATED)
        return;
    if (MH_EnableHook(p) == MH_OK) {
        char detail[96];
        std::snprintf(detail, sizeof detail, "hooked %s", tag);
        addCandidate("export_hook", detail, 0, 0, "");
    }
}

void installExportHooks() {
    tryHookExport(L"sl.interposer.dll", "slSetConstants", detourSlSetConstants, &originalSlSetConstants,
                  "slSetConstants");
    tryHookExport(L"sl.interposer.dll", "slEvaluateFeature", detourSlEvaluate, &originalSlEvaluate,
                  "slEvaluateFeature");
    tryHookExport(L"sl.interposer.dll", "slSetTag", detourSlSetTag, &originalSlSetTag, "slSetTag");
    tryHookExport(L"sl.interposer.dll", "slSetTagForFrame", detourSlSetTagForFrame, &originalSlSetTagForFrame,
                  "slSetTagForFrame");
    tryHookExport(L"_nvngx.dll", "NVSDK_NGX_D3D12_EvaluateFeature", detourNgxEval, &originalNgxEval,
                  "NGX_EvaluateFeature");
    tryHookExport(L"nvngx.dll", "NVSDK_NGX_D3D12_EvaluateFeature", detourNgxEval, &originalNgxEval,
                  "NGX_EvaluateFeature");
    tryHookExport(L"_nvngx.dll", "NVSDK_NGX_D3D12_EvaluateFeature_C", detourNgxEvalC, &originalNgxEvalC,
                  "NGX_EvaluateFeature_C");
    tryHookExport(L"libxess.dll", "xessD3D12Execute", detourXessExec, &originalXessExec, "xessD3D12Execute");
    tryHookExport(L"ffx_fsr2_x64.dll", "ffxFsr2ContextDispatch", detourFsrDispatch, &originalFsrDispatch,
                  "ffxFsr2ContextDispatch");
}

// 0.0.73 SL WATCH: the four Streamline hooks, each made once (MinHook freezes the threads while it patches, so
// one already made is never asked again). Returns sl_watch's hooked bits; 0 while sl.interposer.dll is absent.
uint32_t slWatchInstall() {
    namespace sw = spidy::sl_watch;
    const HMODULE h = GetModuleHandleW(L"sl.interposer.dll");
    if (!h)
        return 0;
    if (!slLo.load()) {
        const auto base = reinterpret_cast<uintptr_t>(h);
        IMAGE_DOS_HEADER dos{};
        IMAGE_NT_HEADERS64 nt{};
        if (safeRead(h, &dos, sizeof dos) && dos.e_magic == IMAGE_DOS_SIGNATURE &&
            safeRead(reinterpret_cast<const void*>(base + static_cast<uintptr_t>(dos.e_lfanew)), &nt, sizeof nt) &&
            nt.Signature == IMAGE_NT_SIGNATURE) {
            slHi.store(base + nt.OptionalHeader.SizeOfImage);
            slLo.store(base);
        }
    }
    struct One {
        const char* name;
        ExportFn detour;
        ExportFn* original;
        uint32_t bit;
    };
    const One all[] = {{"slSetConstants", detourSlSetConstants, &originalSlSetConstants, sw::hookConstants},
                       {"slEvaluateFeature", detourSlEvaluate, &originalSlEvaluate, sw::hookEvaluate},
                       {"slSetTag", detourSlSetTag, &originalSlSetTag, sw::hookTag},
                       {"slSetTagForFrame", detourSlSetTagForFrame, &originalSlSetTagForFrame, sw::hookTagForFrame}};
    uint32_t hooked = 0;
    for (const auto& o : all) {
        if (!*o.original)
            tryHookExport(L"sl.interposer.dll", o.name, o.detour, o.original, o.name);
        if (*o.original)
            hooked |= o.bit;
    }
    return hooked;
}

void finaliseReport();
void scanRenderAlloc();
void scanLod();
void installExportHooks();
void maybeInstallGpuHooks(ID3D12GraphicsCommandList* list);

void enrichCandidates(const std::vector<address_finder::RuntimeFunction>& pdata) {
    // Fill function RVA + signature for live-path candidates that skipped the heavy work.
    std::vector<std::pair<size_t, uint32_t>> todo;
    {
        std::lock_guard g(lock);
        for (size_t i = 0; i < candidates.size(); ++i) {
            auto& c = candidates[i];
            if (c.moduleRva && (!c.functionRva || !c.signature[0]))
                todo.push_back({i, c.moduleRva});
        }
    }
    for (const auto& [idx, rva] : todo) {
        if (scanStop.load() || finishRequested.load())
            break;
        const uint32_t fn =
            pdata.empty() ? functionRvaFor(rva) : address_finder::functionBegin(pdata.data(), pdata.size(), rva);
        const std::string sig = sigAtRva(fn ? fn : rva);
        std::lock_guard g(lock);
        if (idx >= candidates.size())
            break;
        auto& c = candidates[idx];
        if (!c.functionRva)
            c.functionRva = fn;
        if (!c.signature[0] && !sig.empty())
            std::snprintf(c.signature, sizeof c.signature, "%s", sig.c_str());
    }
}

// Update 30: function RVAs for bind rows (worker thread only).
void enrichBindRows(const std::vector<address_finder::RuntimeFunction>& pdata) {
    uint32_t rvas[address_finder::bindRowsCap]{};
    size_t n = 0;
    {
        std::lock_guard g(watchLock);
        n = bindN;
        for (size_t i = 0; i < n; ++i)
            rvas[i] = bindRows[i].functionRva ? 0 : bindRows[i].rva;
    }
    uint32_t fns[address_finder::bindRowsCap]{};
    for (size_t i = 0; i < n; ++i)
        if (rvas[i])
            fns[i] = pdata.empty() ? functionRvaFor(rvas[i])
                                   : address_finder::functionBegin(pdata.data(), pdata.size(), rvas[i]);
    std::lock_guard g(watchLock);
    for (size_t i = 0; i < n && i < bindN; ++i)
        if (fns[i] && bindRows[i].rva == rvas[i] && !bindRows[i].functionRva)
            bindRows[i].functionRva = fns[i];
}

void refreshReportIfDue(bool force) {
    const uint64_t now = GetTickCount64();
    if (!force && now - lastReportMs.load() < reportRefreshMs)
        return;
    finaliseReport();
    publishInner();
    lastReportMs = now;
}

void completeFinishBody() {
    if (jitterTrack.hits >= address_finder::confirmFramesK) {
        // Update 28: confirmed only if the jitter it followed changed; else "almost".
        // Stored through makeRoomLocked (27.4 lost this row to the full store).
        const bool ok = address_finder::jitterTrackConfirmed(jitterTrack);
        char detail[96];
        std::snprintf(detail, sizeof detail, "jitter at cb+%u (%u hits, %u changes)", jitterTrack.offset,
                      jitterTrack.hits, jitterTrack.changes);
        float sample[4] = {jitterTrack.lastX, jitterTrack.lastY, static_cast<float>(jitterTrack.hits), 0};
        std::lock_guard g(lock);
        addCandidateLocked("taa_jitter", detail, 0, 0, "", 0, jitterTrack.offset, 0, true, sample, 0.f);
        if (Candidate* c = findRawLocked("taa_jitter", 0, 0, jitterTrack.offset, 0)) {
            c->category = address_finder::HitCategory::taa;
            c->distinctFrames = std::max(c->distinctFrames, ok ? jitterTrack.hits : 2u);
            c->confirmed = ok;
            if (ok)
                c->score = address_finder::scoreConfirmed(c->distinctFrames, c->hits, 0.f, c->category);
            recountConfirmedLocked();
        }
    }
    finaliseReport();
    if (!failed.load())
        live.status = 2;
    else
        live.status = 3;
    publishInner();
    lastReportMs = GetTickCount64();
}

void requestFinish() {
    armed = false;
    finishRequested = true;
    scanStop = true; // wake the worker out of long scans
}

DWORD WINAPI scanProc(void*) {
    // THREAD_PRIORITY_BELOW_NORMAL: never fight the game / OpenXR for the core.
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
    const uintptr_t base = moduleBase();
    if (!base) {
        fail(2720, "no Spider-Man2.exe module");
        return 0;
    }
    IMAGE_DOS_HEADER dh{};
    if (!safeRead(reinterpret_cast<const void*>(base), &dh, sizeof dh) || dh.e_magic != IMAGE_DOS_SIGNATURE) {
        fail(2721, "DOS header");
        return 0;
    }
    IMAGE_NT_HEADERS64 nt{};
    if (!safeRead(reinterpret_cast<const void*>(base + dh.e_lfanew), &nt, sizeof nt)) {
        fail(2722, "NT headers");
        return 0;
    }
    const size_t imageSize = nt.OptionalHeader.SizeOfImage;
    if (imageSize < 0x1000 || imageSize > (2u << 30)) {
        fail(2723, "SizeOfImage");
        return 0;
    }
    std::vector<address_finder::RuntimeFunction> pdata;
    const auto& edir = nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION];
    if (edir.VirtualAddress && edir.Size) {
        const size_t count = edir.Size / sizeof(address_finder::RuntimeFunction);
        pdata.resize(count);
        if (!safeRead(reinterpret_cast<const void*>(base + edir.VirtualAddress), pdata.data(),
                      count * sizeof(address_finder::RuntimeFunction)))
            pdata.clear();
    }
    static constexpr const char* needles[] = {
        "GlobalWorldCBuffer", "g_World", "m_ActiveViewCtxPos", "GlobalViewportCBuffer", "g_VP",
        "TemporalAA",        "Jitter",  "MotionVector",       "MotionVectors",         "History",
        "DLSS",              "DLAA",    "IGTI",               "TemporalUpscale",       "TAAU"};
    constexpr size_t chunk = 1u << 20;
    constexpr size_t xrefWindow = 2u << 20; // ±2 MiB around the string, not the whole image
    lastReportMs = GetTickCount64();
    size_t needleIndex = 0;
    size_t scanOff = 0;
    uint32_t bgTicks = 0;
    while (armed.load() || finishRequested.load()) {
        if (failed.load())
            break;
        // Deferred GPU hook install (list pointer captured on the render thread).
        if (void* raw = pendingCommandList.exchange(nullptr))
            maybeInstallGpuHooks(static_cast<ID3D12GraphicsCommandList*>(raw));

        if ((bgTicks & 7u) == 0u)
            installExportHooks();
        if ((bgTicks & 15u) == 1u)
            scanRenderAlloc();
        if ((bgTicks & 31u) == 2u)
            scanLod();
        if ((bgTicks & 3u) == 3u) {
            enrichCandidates(pdata);
            enrichBindRows(pdata); // Update 30
        }

        // One chunk of string scan per loop; Sleep so we never monopolise a core.
        if (needleIndex < sizeof needles / sizeof needles[0] && armed.load() && !finishRequested.load()) {
            const char* needle = needles[needleIndex];
            if (scanOff >= imageSize) {
                scanOff = 0;
                ++needleIndex;
            } else {
                const size_t n = std::min(chunk, imageSize - scanOff);
                std::vector<uint8_t> buf(n);
                if (safeRead(reinterpret_cast<const void*>(base + scanOff), buf.data(), n)) {
                    const auto hits =
                        address_finder::findAscii(buf.data(), n, needle, static_cast<uint32_t>(scanOff));
                    for (uint32_t strRva : hits) {
                        if (finishRequested.load() || scanStop.load())
                            break;
                        ++live.stringHits;
                        char detail[160];
                        std::snprintf(detail, sizeof detail, "string \"%s\"", needle);
                        addCandidate("string", detail, strRva, 0, "");
                        // Local xref window only (whole-image LEA scans stalled the machine).
                        const size_t center = strRva;
                        const size_t x0 = center > xrefWindow ? center - xrefWindow : 0;
                        const size_t x1 = std::min(imageSize, center + xrefWindow);
                        for (size_t so = x0; so < x1 && !scanStop.load() && !finishRequested.load();
                             so += chunk) {
                            const size_t sn = std::min(chunk, x1 - so);
                            std::vector<uint8_t> code(sn);
                            if (!safeRead(reinterpret_cast<const void*>(base + so), code.data(), sn))
                                continue;
                            const auto xrefs = address_finder::findLeaXrefs(
                                code.data(), sn, strRva, static_cast<uint32_t>(so), 8);
                            for (const auto& x : xrefs) {
                                const uint32_t fn =
                                    pdata.empty()
                                        ? 0
                                        : address_finder::functionBegin(pdata.data(), pdata.size(), x.at);
                                addCandidate("string_xref", detail, x.at, fn,
                                             sigAtRva(fn ? fn : x.at).c_str());
                            }
                            if (!xrefs.empty())
                                break;
                            Sleep(0);
                        }
                    }
                }
                scanOff += chunk;
            }
        }

        refreshReportIfDue(false);

        // Full ~60 s window unless lighting + TAA + LOD each have a strong confirmed hit.
        // The raw-hit cap never ends the run. Update 30.1: while the bind trace is on, the
        // early stop is off too (only the window or the frame cap ends the run).
        if (armed.load()) {
            const bool timedOut =
                frameCount.load() > maxFrames || GetTickCount64() >= deadlineMs.load();
            const bool categoriesDone =
                strongLighting.load() && strongTaa.load() && strongLod.load();
            if (address_finder::finishNow(timedOut, categoriesDone, !bindTraceFailed.load()))
                requestFinish();
        }

        ++bgTicks;
        Sleep(1);
        if (finishRequested.load())
            break;
        // If string scan done and still armed, keep doing periodic GPU/alloc work until deadline.
        if (needleIndex >= sizeof needles / sizeof needles[0] && armed.load() && !finishRequested.load())
            Sleep(5);
    }
    completeFinishBody();
    return 0;
}

void scanRenderAlloc() {
    const uintptr_t base = moduleBase();
    if (!base)
        return;
    native_render_memory::Fields f{};
    if (!safeRead(reinterpret_cast<const void*>(base + allocatorRva), &f, sizeof f) ||
        !native_render_memory::usable(f) || !f.ring || !f.committed)
        return;
    // Background only: still capped so one pass cannot run for seconds.
    scanBytes("render_alloc", reinterpret_cast<const void*>(static_cast<uintptr_t>(f.ring)),
              std::min<size_t>(f.committed, maxScanBytes), 0, 0, false);
}

void scanLod() {
    const uintptr_t base = moduleBase();
    if (!base)
        return;
    uintptr_t view = 0;
    if (!safeRead(reinterpret_cast<const void*>(base + activeViewPtrRva), &view, 8) || view < 0x10000)
        return;
    native_view::Descriptor d{};
    if (!safeRead(reinterpret_cast<const void*>(view), &d, sizeof d) || !native_view::valid(d))
        return;
    const float want = native_view::distanceScale(d);
    if (!(want > 0) || !std::isfinite(want))
        return;
    constexpr size_t maxOff = 0x4000;
    std::vector<float> floats(maxOff / 4);
    if (!safeRead(reinterpret_cast<const void*>(view), floats.data(), maxOff))
        return;
    uint32_t best = UINT32_MAX;
    float bestErr = 1e9f;
    for (size_t i = 0; i < floats.size(); ++i) {
        const float e = std::abs(floats[i] - want);
        if (e < 1e-4f && e < bestErr) {
            bestErr = e;
            best = static_cast<uint32_t>(i * 4);
        }
    }
    if (best != UINT32_MAX) {
        lodOffset = best;
        char detail[96];
        std::snprintf(detail, sizeof detail, "distanceScale=%.6g at view+%x", static_cast<double>(want), best);
        float sample[4] = {want, static_cast<float>(best), 0, 0};
        std::lock_guard g(lock);
        addCandidateLocked("lod_scale", detail, 0, 0, "", 0, best, 0, true, sample, bestErr);
        // Update 28: the scale is static, so it confirms by being found at the same
        // offset on K separate scans (frames), not by changing.
        const bool stable = address_finder::noteStableSample(lodStable, frameCount.load(), best);
        if (Candidate* c = findRawLocked("lod_scale", 0, 0, best, 0)) {
            c->distinctFrames = lodStable.offset == best ? lodStable.scans : 1;
            c->category = address_finder::HitCategory::lod;
            if (stable && !c->confirmed) {
                c->confirmed = true;
                c->score = address_finder::scoreConfirmed(c->distinctFrames, c->hits, bestErr, c->category);
            }
            recountConfirmedLocked();
        }
    }
}

void finaliseReport() {
    std::lock_guard g(lock);
    recountConfirmedLocked();
    std::vector<size_t> order;
    order.reserve(candidates.size());
    const auto almost = [](const Candidate& c) {
        return address_finder::isAlmost(c.confirmed, c.valueBearing, c.distinctFrames, c.nearMiss);
    };
    for (size_t i = 0; i < candidates.size(); ++i)
        if (candidates[i].confirmed)
            order.push_back(i);
    std::sort(order.begin(), order.end(), [](size_t a, size_t b) {
        const auto& ca = candidates[a];
        const auto& cb = candidates[b];
        if (ca.score != cb.score)
            return ca.score > cb.score;
        if (ca.distinctFrames != cb.distinctFrames)
            return ca.distinctFrames > cb.distinctFrames;
        return ca.hits > cb.hits;
    });
    size_t at = 0;
    auto append = [&](const char* line) {
        const size_t n = std::strlen(line);
        if (at + n + 2 >= maxReport)
            return;
        std::memcpy(reportBuf + at, line, n);
        at += n;
        reportBuf[at++] = '\n';
        reportBuf[at] = 0;
    };
    std::memset(reportBuf, 0, sizeof reportBuf);
    append("{\"type\":\"header\",\"label\":\"candidates_only\",\"update\":33,\"window_s\":60}");
    // Update 30: bind rows first (their own reserved store; never crowded out).
    {
        std::lock_guard w(watchLock);
        size_t shadows = 0;
        for (size_t i = 0; i < bindN; ++i) {
            const auto& r = bindRows[i];
            const bool shadow = address_finder::shadowPassCandidate(r);
            shadows += shadow ? 1 : 0;
            char line[640];
            std::snprintf(
                line, sizeof line,
                "{\"type\":\"bind\",\"api\":\"%s\",\"rva\":%u,\"function_rva\":%u,\"root_slot\":%u,"
                "\"cb_offset\":%u,\"table\":%u,\"cams\":\"%s\",\"tag\":\"%s\",\"match\":\"%s\",\"seed29\":%s,"
                "\"pass\":\"%s\",\"rt_count\":%u,\"dsv\":%s,\"viewport\":[%u,%u],"
                "\"depth\":{\"format\":%u,\"width\":%u,\"height\":%u},\"hits\":%u,\"frames\":%u,"
                "\"shadow_candidate\":%s}",
                address_finder::bindApiName(r.api), r.rva, r.functionRva, r.slot, r.offset, r.table,
                address_finder::cameraMaskText(r.camMask).c_str(), r.tag ? r.tag : "",
                address_finder::matchKindName(r.kind), r.seed ? "true" : "false",
                address_finder::passKindName(r.pass), r.rtCount, r.hasDsv ? "true" : "false", r.vpW, r.vpH,
                r.depthFormat, r.depthW, r.depthH, r.hits, r.frames, shadow ? "true" : "false");
            append(line);
        }
        char line[2048]; // Update 31: table_summary can pass 1 KiB
        const char* tables = tableHooksOn.load() ? "on" : (tableHooksFailed.load() ? "failed" : "not_installed");
        std::snprintf(line, sizeof line,
                      "{\"type\":\"bind_summary\",\"rows\":%zu,\"cap\":%zu,\"dropped\":%u,\"watched\":%zu,"
                      "\"watched_cap\":%zu,\"shadow_candidates\":%zu,\"pass_hooks\":\"%s\",\"descriptor_tables\":\"%s\"}",
                      bindN, address_finder::bindRowsCap, bindDropped.load(), watchedN, address_finder::maxWatched,
                      shadows, passHooksOn.load() ? "on" : (passHooksFailed.load() ? "failed" : "not_installed"),
                      tables);
        append(line);
        // Update 31: binds per api and pass kind, the shader-visible heap, bindless verdict.
        std::string counts = "{";
        for (uint8_t api = 0; api < 6; ++api) {
            char part[160];
            std::snprintf(part, sizeof part, "%s\"%s\":{\"unknown\":%u,\"depth_only\":%u,\"color\":%u,\"compute\":%u}",
                          api ? "," : "", address_finder::bindApiName(api), passBinds[api][0].load(),
                          passBinds[api][1].load(), passBinds[api][2].load(), passBinds[api][3].load());
            counts += part;
        }
        counts += "}";
        std::snprintf(line, sizeof line,
                      "{\"type\":\"table_summary\",\"hooks\":\"%s\",\"binds_by_pass\":%s,"
                      "\"heap\":{\"descriptors\":%u,\"increment\":%u,\"known\":%s},\"table_binds\":%llu,"
                      "\"heap_start_binds\":%llu,\"bindless\":\"%s\",\"descriptors_mapped\":%zu,"
                      "\"descriptors_created\":%llu,\"descriptors_copied\":%llu,\"tables_covering_camera\":%llu}",
                      tables, counts.c_str(), heapCount.load(), descInc.load(), heapSeen.load() ? "true" : "false",
                      static_cast<unsigned long long>(tableBindsTotal.load()),
                      static_cast<unsigned long long>(heapStartBinds.load()),
                      address_finder::bindlessVerdict(heapStartBinds.load(), tableBindsTotal.load()),
                      descCount.load(), static_cast<unsigned long long>(descCreated.load()),
                      static_cast<unsigned long long>(descCopied.load()),
                      static_cast<unsigned long long>(tableHits.load()));
        append(line);
        for (size_t i = 0; i < depthTargetN; ++i) {
            const auto& d = depthTargets[i];
            std::snprintf(line, sizeof line,
                          "{\"type\":\"depth_target\",\"format\":%u,\"width\":%u,\"height\":%u,"
                          "\"transitions\":%u,\"depth_only_binds\":%u}",
                          d.format, d.width, d.height, d.transitions, d.depthOnlyBinds);
            append(line);
        }
        for (size_t i = 0; i < orthoN; ++i) {
            const auto& o = orthoRows[i];
            std::snprintf(line, sizeof line,
                          "{\"type\":\"ortho_box\",\"rva\":%u,\"record_offset\":%u,"
                          "\"half_m\":[%.4g,%.4g,%.4g],\"hits\":%u}",
                          o.rva, o.recordOffset, static_cast<double>(o.half[0]), static_cast<double>(o.half[1]),
                          static_cast<double>(o.half[2]), o.hits);
            append(line);
        }
        // Update 32: ORTHO CAPTURE rows (tools/ortho_dump.py makes the readable dump).
        for (size_t i = 0; i < orthoCaptureN; ++i) {
            const auto& c = orthoCaptures[i];
            std::string row;
            char part[256];
            std::snprintf(part, sizeof part,
                          "{\"type\":\"ortho_capture\",\"group\":%u,\"ms\":%llu,\"frame\":%llu,\"gpu_va\":%llu,"
                          "\"copy_bytes\":%u,\"matrix_offset\":%u,\"window_offset\":%u,\"row_form\":%s,\"chain\":[",
                          c.group, static_cast<unsigned long long>(c.ms), static_cast<unsigned long long>(c.frame),
                          static_cast<unsigned long long>(c.gpuVa), c.copyBytes, c.matrixOffset, c.windowOffset,
                          c.rowForm ? "true" : "false");
            row += part;
            for (unsigned k = 0; k < address_finder::orthoChain && c.chain[k]; ++k) {
                std::snprintf(part, sizeof part, "%s%u", k ? "," : "", c.chain[k]);
                row += part;
            }
            const auto floatsOut = [&](const char* name, const float* v, unsigned n, bool last = false) {
                row += "\"";
                row += name;
                row += "\":[";
                for (unsigned k = 0; k < n; ++k) {
                    std::snprintf(part, sizeof part, "%s%.7g", k ? "," : "",
                                  std::isfinite(v[k]) ? static_cast<double>(v[k]) : 0.0);
                    row += part;
                }
                row += last ? "]" : "],";
            };
            row += "],";
            floatsOut("half_m", c.half, 3);
            floatsOut("matrix", c.matrix, 16);
            floatsOut("window", c.window, c.windowFloats);
            if (c.headValid)
                floatsOut("head", c.head, 6);
            if (c.flatValid)
                floatsOut("flat", c.flat, 6);
            if (row.back() == ',')
                row.pop_back();
            row += "}";
            append(row.c_str());
        }
        {
            char text[192];
            std::snprintf(text, sizeof text,
                          "{\"type\":\"ortho_capture_summary\",\"captures\":%zu,\"cap\":%u,\"groups\":%u,"
                          "\"group_cap\":%u,\"every_ms\":%llu}",
                          orthoCaptureN, address_finder::maxOrthoCaptures, orthoGroup.groups, address_finder::orthoGroups,
                          static_cast<unsigned long long>(address_finder::orthoGroupEveryMs));
            append(text);
        }
        shadowTotal = static_cast<uint32_t>(shadows);
    }
    if (jitterTrack.hits >= address_finder::confirmFramesK) {
        char line[192];
        std::snprintf(line, sizeof line,
                      "{\"type\":\"jitter_track\",\"offset\":%u,\"hits\":%u,\"last\":[%.8g,%.8g]}",
                      jitterTrack.offset, jitterTrack.hits, static_cast<double>(jitterTrack.lastX),
                      static_cast<double>(jitterTrack.lastY));
        append(line);
    }
    if (lodOffset.load() != UINT32_MAX) {
        char line[96];
        std::snprintf(line, sizeof line, "{\"type\":\"lod\",\"view_offset\":%u}", lodOffset.load());
        append(line);
    }
    // Confirmed first (ranked), then "almost" (Update 28), then remaining raw hits.
    auto emit = [&](const Candidate& c, bool confirmedOnly) {
        if (confirmedOnly != c.confirmed)
            return;
        char line[700];
        std::snprintf(
            line, sizeof line,
            "{\"type\":\"candidate\",\"kind\":\"%s\",\"detail\":\"%s\",\"rva\":%u,\"function_rva\":%u,"
            "\"signature\":\"%s\",\"gpu_va\":%llu,\"cb_offset\":%u,\"root_slot\":%u,\"hits\":%u,"
            "\"confirmed\":%s,\"almost\":%s,\"near_miss\":%s,\"distinct_frames\":%u,\"score\":%.4g}",
            address_finder::jsonEscape(c.kind).c_str(), address_finder::jsonEscape(c.detail).c_str(),
            c.moduleRva, c.functionRva, address_finder::jsonEscape(c.signature).c_str(),
            static_cast<unsigned long long>(c.gpuVa), c.cbOffset, c.rootSlot, c.hits,
            c.confirmed ? "true" : "false", almost(c) ? "true" : "false", c.nearMiss ? "true" : "false",
            c.distinctFrames, static_cast<double>(c.score));
        append(line);
    };
    for (size_t i : order)
        emit(candidates[i], true);
    size_t almostCount = 0;
    for (const auto& c : candidates)
        if (almost(c)) {
            emit(c, false);
            ++almostCount;
        }
    for (const auto& c : candidates)
        if (!c.confirmed && !almost(c))
            emit(c, false);
    char footer[160];
    std::snprintf(footer, sizeof footer,
                  "{\"type\":\"footer\",\"confirmed\":%u,\"almost\":%zu,\"raw_hits\":%zu,\"frames\":%llu}",
                  confirmedCount.load(), almostCount, candidates.size(),
                  static_cast<unsigned long long>(frameCount.load()));
    append(footer);
    almostTotal = static_cast<uint32_t>(almostCount);
}

void stopScanThread() {
    scanStop = true;
    finishRequested = true;
    if (scanThread) {
        // Control / restart path only — never call from the render thread.
        WaitForSingleObject(scanThread, 8000);
        CloseHandle(scanThread);
        scanThread = nullptr;
    }
}

void finish(bool /*ok*/) {
    // May run on the hologram / control path. Never on the D3D12 barrier thread.
    requestFinish();
    if (scanThread && GetCurrentThreadId() != GetThreadId(scanThread)) {
        WaitForSingleObject(scanThread, 8000);
        CloseHandle(scanThread);
        scanThread = nullptr;
        // Worker already called completeFinishBody; if it never started, do it here.
        if (live.status == 1)
            completeFinishBody();
    } else if (!scanThread) {
        completeFinishBody();
    }
}

void tickFrame() {
    // Once per stock view update (~eye frame), never per barrier.
    if (!armed.load() || failed.load())
        return;
    const uint64_t n = ++frameCount;
    bindLookupsThisFrame = 0; // Update 30
    bindTracesThisFrame = 0;
    descOpsThisFrame = 0; // Update 31
    // Timeout only arms the finish flag; categoriesDone is decided on the worker.
    if (n > maxFrames || GetTickCount64() >= deadlineMs.load())
        requestFinish();
    if ((n & 15u) == 0u)
        publishInner();
}


} // namespace

uint32_t set(uint32_t on) {
    if (on > 1)
        return 2791;
    if (on == 0) {
        if (armed.load() || live.status == 1)
            finish(true);
        return 0;
    }
    // Fresh start always: never restore a previous DONE count on the HUD.
    stopScanThread();
    failed = false;
    faultCode = 0;
    frameCount = 0;
    lodOffset = UINT32_MAX;
    jitterTrack = {};
    lodStable = {};
    finishRequested = false;
    scanStop = false;
    skipFrames = 0;
    lastReportMs = 0;
    pendingCommandList = nullptr;
    confirmedCount = 0;
    almostTotal = 0;
    strongLighting = false;
    strongTaa = false;
    strongLod = false;
    bindLookupsThisFrame = 0;
    bindTracesThisFrame = 0;
    bindTraceFailed = false;
    {
        std::lock_guard g(lock);
        {
            std::lock_guard w(watchLock); // Update 30: fresh watch table + 8 seed pages
            clearBindTraceLocked();
        }
        candidates.clear();
        stockCam = {};
        headCam = {};
        eyeCam[0] = eyeCam[1] = {};
        for (auto& h : stockHistory)
            h = {};
        std::memset(reportBuf, 0, sizeof reportBuf);
        live = {};
        live.magic = dataMagic;
        live.version = dataVersion;
        live.bytes = sizeof(Data);
        live.status = 1;
        live.candidates = 0;
        live.rawHits = 0;
        live.frames = 0;
        live.scans = 0;
        live.exportHits = 0;
        live.stringHits = 0;
        live.lodOffset = UINT32_MAX;
    }
    ++generation;
    runStartMs = GetTickCount64();
    deadlineMs = GetTickCount64() + static_cast<uint64_t>(windowSeconds) * 1000ull;
    live.status = 1;
    armed = true;
    scanThread = CreateThread(nullptr, 0, scanProc, nullptr, 0, nullptr);
    if (!scanThread)
        fail(2724, "CreateThread static scan");
    native_shadow_distance::start(); // Update 33: the SHADOW DISTANCE search, read only
    publishInner();
    return 0;
}

uint32_t status() { return live.status; }

uint32_t almost() { return almostTotal.load(); }

uint32_t shadowPasses() { return shadowTotal.load(); }

void viewFields(uint32_t& statusOut, uint32_t& secondsOrCandidates) {
    // Read-only for the hologram: never touch candidates / report under the scan lock.
    const bool run = armed.load();
    const bool bad = failed.load();
    uint32_t st = live.status;
    if (run)
        st = 1;
    else if (bad)
        st = 3;
    else if (st == 1)
        st = 2; // finished since last publish
    statusOut = st;
    if (st == 1) {
        const uint64_t now = GetTickCount64();
        const uint64_t dl = deadlineMs.load();
        secondsOrCandidates = now < dl ? static_cast<uint32_t>((dl - now + 999) / 1000) : 0;
    } else if (st == 2)
        secondsOrCandidates = live.candidates;
    else
        secondsOrCandidates = 0;
}

void onBarrier(ID3D12GraphicsCommandList* list, UINT count, const D3D12_RESOURCE_BARRIER* bs) {
    if (!armed.load() || failed.load())
        return;
    // Stash the list for the worker to install GPU hooks; never MH_* on this thread.
    if (list && !hooksOn.load())
        pendingCommandList = list;
    beginFrameBudget();
    // frameCount ticks from noteView(0), not here (barriers fire many times per eye).
    if (skipFrames.load()) {
        // Auto-backoff after an overrun: count the frame, sample nothing heavy.
        endFrameBudget();
        return;
    }
    if (!bs || !count) {
        endFrameBudget();
        return;
    }
    // Update 30: the last texture moved to DEPTH_WRITE on this thread is the depth
    // target a following depth-only pass most likely draws into (format / size).
    for (UINT i = 0; i < count && i < 16 && !bindTraceFailed.load(); ++i) {
        if (bs[i].Type != D3D12_RESOURCE_BARRIER_TYPE_TRANSITION || !bs[i].Transition.pResource ||
            !(bs[i].Transition.StateAfter & D3D12_RESOURCE_STATE_DEPTH_WRITE))
            continue;
        D3D12_RESOURCE_DESC desc{};
        if (!sehGetResourceDesc(bs[i].Transition.pResource, &desc) ||
            desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D)
            continue;
        tlsPass.depthFormat = static_cast<uint32_t>(desc.Format);
        tlsPass.depthW = static_cast<uint32_t>(desc.Width);
        tlsPass.depthH = desc.Height;
        // Update 31: every depth target this session (shadow maps: sizes and how many).
        std::unique_lock g(watchLock, std::try_to_lock);
        if (g.owns_lock())
            tlsDepthTarget = address_finder::noteDepthTarget(depthTargets, depthTargetN, tlsPass.depthFormat,
                                                             tlsPass.depthW, tlsPass.depthH);
    }
    for (UINT i = 0; i < count && liveBudgetAllows(); ++i) {
        if (barrierSamplesThisFrame.load() >= maxBarrierSamplesPerFrame)
            break;
        if (bs[i].Type != D3D12_RESOURCE_BARRIER_TYPE_TRANSITION || !bs[i].Transition.pResource)
            continue;
        D3D12_RESOURCE_DESC desc{};
        if (!sehGetResourceDesc(bs[i].Transition.pResource, &desc))
            continue;
        if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D)
            continue;
        const UINT w = static_cast<UINT>(desc.Width), h = desc.Height;
        if (w < 64 || h < 64)
            continue;
        if (desc.Format == DXGI_FORMAT_R16G16_FLOAT || desc.Format == DXGI_FORMAT_R16G16B16A16_FLOAT) {
            ++barrierSamplesThisFrame;
            noteLiveOp();
            const uint32_t rva = returnRva();
            char detail[128];
            std::snprintf(detail, sizeof detail, "tex %ux%u fmt=%u", w, h, static_cast<unsigned>(desc.Format));
            addCandidateLive(desc.Format == DXGI_FORMAT_R16G16_FLOAT ? "mvec_target" : "history_color", detail,
                             rva);
        }
    }
    endFrameBudget();
}

void noteView(unsigned kind, const native_view::Descriptor& d) {
    if (!armed.load() || failed.load())
        return;
    const auto cam = address_finder::cameraFromDescriptor(d);
    if (!cam.valid)
        return;
    {
        std::lock_guard g(lock);
        if (kind == 0) {
            // Update 28: keep the last cameraLags stock cameras (newest first).
            for (unsigned k = address_finder::cameraLags; k-- > 1;)
                stockHistory[k] = stockHistory[k - 1];
            stockHistory[0] = stockCam;
            stockCam = cam;
        } else if (kind == 1)
            headCam = cam;
        else if (kind == 2 || kind == 3)
            eyeCam[kind - 2] = cam;
    }
    // One frame tick per stock view (not head, not barriers).
    if (kind == 0)
        tickFrame();
}

} // namespace spidy::native_address_finder

extern "C" {
__declspec(dllexport) spidy::native_address_finder::Data SpidyAddressFinderData{};
__declspec(dllexport) char SpidyAddressFinderReport[spidy::native_address_finder::reportBytes]{};
// 0.0.73 SL WATCH (sl_watch.hpp): the record (SLWT) and its switch, 0 off / 1 on; on also makes the hooks once.
__declspec(dllexport) spidy::sl_watch::Data SpidySlWatchData{};

__declspec(dllexport) DWORD WINAPI SpidySlWatchOn(void* on) {
    using namespace spidy::native_address_finder;
    const auto v = reinterpret_cast<uintptr_t>(on);
    if (v > 1)
        return 4473;
    slWatchOn.store(static_cast<uint32_t>(v));
    SpidySlWatchData.on = static_cast<uint32_t>(v);
    if (v)
        SpidySlWatchData.hooked = slWatchInstall();
    return 0;
}

__declspec(dllexport) DWORD WINAPI SpidyAddressFinder(void* mode) {
    using namespace spidy::native_address_finder;
    const auto v = reinterpret_cast<uintptr_t>(mode);
    if (v > 1)
        return 2791;
    return set(static_cast<uint32_t>(v));
}
}
