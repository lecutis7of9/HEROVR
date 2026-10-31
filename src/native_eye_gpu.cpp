#include "spidy/native_eye_gpu.hpp"
#include "spidy/native_address_finder.hpp"
#include "spidy/copy_eye_texture.hpp"
#include "spidy/eye_pair_state.hpp"
#include "spidy/eye_sharpen.hpp"
#include "spidy/eye_color.hpp"
#include "spidy/foveation.hpp"
#include "spidy/eye_swap.hpp"
#include "spidy/eye_debug.hpp"
#include "spidy/eye_taa.hpp"
#include "spidy/monitor_rate.hpp"
#include "spidy/presentation_gate.hpp"
#include "spidy/streamer_view.hpp"
#include <MinHook.h>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstring>
#include <d3dcompiler.h>
#include <dxgi1_4.h>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>
#include <windows.h>
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;
using namespace spidy;
extern "C" IMAGE_DOS_HEADER __ImageBase;
extern "C" {
__declspec(dllexport) native_gpu::Data SpidyGpuData;
__declspec(dllexport) native_gpu::ScreenData SpidyScreenData;
__declspec(dllexport) native_gpu::HistoryData SpidyEyeHistoryData;
}
namespace spidy::native_gpu {
namespace {
using Barrier = void(STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*, UINT, const D3D12_RESOURCE_BARRIER*);
using Reset = HRESULT(STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*, ID3D12CommandAllocator*,
                                          ID3D12PipelineState*);
using Execute = void(STDMETHODCALLTYPE*)(ID3D12CommandQueue*, UINT, ID3D12CommandList* const*);
using EndQuery = void(STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*, ID3D12QueryHeap*, D3D12_QUERY_TYPE,
                                          UINT);
using Present = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*, UINT, UINT);
using Present1 = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain1*, UINT, UINT, const DXGI_PRESENT_PARAMETERS*);
Barrier originalBarrier{};
Reset originalReset{};
Execute originalExecute{};
EndQuery originalEndQuery{};
Present originalPresent{};
Present1 originalPresent1{};
std::array<void*, 4> hooks{};
// DXGI Present and Present1; absent when no swapchain could be made to find them.
std::array<void*, 2> presentHooks{};
std::atomic<bool> enabled{};
std::atomic<uint32_t> fault{};
std::mutex lifecycle, submission, telemetry;
// Recursive: under Proton (vkd3d-proton) one function serves every command list, so the hooks below
// also see Spidy's own D3D12 calls and can re-enter on a thread that already holds this lock. A plain
// std::mutex then throws resource_deadlock_would_occur and the game closes.
std::recursive_mutex recording;
std::condition_variable copyReady;
uint64_t deadline{};
bool hooked{};
bool copyToXr{}, captureImages{}, staged{};
uint64_t requestedSerial{}, requestFence{};
std::array<ComPtr<ID3D12Resource>, 2> targets;
struct Marker {
    unsigned eye{};
    EyePairState::Stamp stamp{};
    bool ended{}, valid{};
};
thread_local Marker currentMarker{};
struct Event {
    unsigned eye{};
    bool marker{}, ended{};
    EyePairState::Stamp stamp{};
    D3D12_RESOURCE_TRANSITION_BARRIER transition{};
    D3D12_RESOURCE_BARRIER_FLAGS flags{};
};
std::unordered_map<ID3D12CommandList*, std::vector<Event>> lists;
EyePairState pair;
uint64_t executes{}, matchedMarkers{}, pairs{}, lastGeneration{}, lastSerial{}, fenceValue{};
std::atomic<uint64_t> markers{};
uint64_t begins[2]{}, ends[2]{};
ComPtr<ID3D12Device> device;
ComPtr<ID3D12CommandQueue> queue;
std::array<ComPtr<ID3D12Resource>, 2> resources, readbacks;
std::array<std::atomic<ID3D12Resource*>, 2> watched{};
std::array<D3D12_RESOURCE_STATES, 2> states{};
std::array<bool, 2> known{};
// Update 32.1 (eye_swap.hpp): eye images the game replaced mid-session (a
// graphics setting), kept referenced until the bridge's GPU work submitted
// before the swap has finished; an eye whose AA shader view still names the
// old image (rewritten once that work is done); how many images were taken
// over; and, when a new image could not be taken, the eyes' pause (3503).
struct RetiredEye {
    ComPtr<ID3D12Resource> texture;
    uint64_t fence{};
};
std::vector<RetiredEye> retiredEyes;
// 0.0.71 MIRROR VIEW (private test): the game's third view (stereo_probe.cpp's mirror) as a small
// picture copied into both staged eyes. Its own state is learned from the game's barriers like
// an eye's; anything unexpected sets mvFault and only the picture stops (never the eyes, never
// `fault`, so VR goes on and nothing falls back to flatscreen).
ComPtr<ID3D12Resource> mvResource;
std::atomic<ID3D12Resource*> mvWatched{};
D3D12_RESOURCE_STATES mvState{};
bool mvKnown{};
std::atomic<uint32_t> mvFault{};
std::atomic<uint64_t> mvCopies{};
std::array<bool, 2> eyeViewStale{};
std::atomic<uint32_t> eyeSwapCount{}, eyesLostCode{};
std::array<D3D12_PLACED_SUBRESOURCE_FOOTPRINT, 2> layouts{};
std::array<std::vector<uint8_t>, 2> pixels;
ComPtr<ID3D12CommandAllocator> allocator;
ComPtr<ID3D12GraphicsCommandList> command;
ComPtr<ID3D12Fence> fence;
HANDLE fenceEvent{};
UINT width{}, height{};
// The headset's eye size (setEyeOutput; 0: the eyes' own). Above it the eyes
// are supersampled and the staged images are filtered down to it (drawAa).
std::atomic<uint32_t> wantedOutWidth{}, wantedOutHeight{};
UINT outWidth{}, outHeight{};
bool downsampling() { return outWidth != width || outHeight != height; }
// The size of the images read back (captureImage) and published (Data).
UINT imageWidth() { return staged ? outWidth : width; }
UINT imageHeight() { return staged ? outHeight : height; }
struct CopyCommands {
    ComPtr<ID3D12CommandAllocator> allocator;
    ComPtr<ID3D12GraphicsCommandList> list;
    uint64_t fence{};
};
std::array<CopyCommands, 3> captureCommands, presentCommands, screenCommands, mirrorCommands, shownCommands;
std::array<ComPtr<ID3D12Resource>, 2> stagedEyes;
EyePairState::Stamp capturedStamp;
uint64_t capturedPairs{}, reusedPairs{}, capturedMs{};
// copyTiming (submission lock): headset copies not yet seen finished, with
// when they and their pair were submitted (QueryPerformanceCounter).
int64_t capturedQpc{};
struct PendingCopy {
    uint64_t fence{};
    int64_t submitted{}, paired{};
};
std::array<PendingCopy, 16> pendingCopies;
unsigned pendingCount{};
TimingMetric copyGpu, copyPair;
std::mutex copyTimingLock;
double qpcMs(int64_t from, int64_t to) {
    static const double perMs = [] {
        LARGE_INTEGER f{};
        QueryPerformanceFrequency(&f);
        return static_cast<double>(f.QuadPart) / 1000.0;
    }();
    return static_cast<double>(to - from) / perMs;
}
int64_t qpcNow() {
    LARGE_INTEGER now{};
    QueryPerformanceCounter(&now);
    return now.QuadPart;
}
void noteCopies(uint64_t completed) { // submission lock held by caller
    if (!pendingCount || completed == UINT64_MAX)
        return;
    const auto now = qpcNow();
    unsigned kept = 0;
    std::lock_guard lock(copyTimingLock);
    for (unsigned i = 0; i < pendingCount; ++i) {
        const auto& p = pendingCopies[i];
        if (p.fence <= completed) {
            copyGpu.add(qpcMs(p.submitted, now));
            if (p.paired)
                copyPair.add(qpcMs(p.paired, now));
        } else
            pendingCopies[kept++] = p;
    }
    pendingCount = kept;
}
// The game's presented frame (submission lock). A texture replaced by a new
// size stays alive for a few seconds, for draws still reading it.
std::atomic<bool> screenWanted{};
bool screenWantedFlag() {
    return screenWanted.load();
}
Screen screen;
uint64_t screenMs{};
struct Retired {
    ComPtr<ID3D12Resource> texture;
    uint64_t releaseMs{};
};
std::vector<Retired> retiredScreens;
std::vector<uint8_t> screenPixels; // probe readback of the newest copy
std::atomic<uint64_t> screenPresents{};
// Every Present of any swapchain in the process since the hooks went in.
std::atomic<uint32_t> presentCalls{};
// The left eye on the game's window (mirrorEye): wanted while the game does
// not render its own view, and how many presents got it.
std::atomic<bool> mirrorWanted{};
std::atomic<uint32_t> mirrorCalls{};
// While the window shows the left eye, at most this many of the game's
// presents a second reach DXGI (monitor_rate.hpp; 0: all of them), the time
// the next one is due, and how many were held back.
std::atomic<uint32_t> mirrorRate{}, presentsHeld{};
std::atomic<int64_t> mirrorDue{};
// The left eye as the headset showed it, the overlay's drawings on it
// (keepShownEye; COPY_SOURCE between uses, submission lock), and when it was
// copied. shownWanted: the window has drawn since, so the next one is copied.
ComPtr<ID3D12Resource> shownEye;
uint64_t shownMs{};
bool shownFailed{};
std::atomic<bool> shownWanted{true};
std::atomic<uint32_t> shownMirrors{};
struct MirrorPipeline {
    ComPtr<ID3D12RootSignature> root;
    ComPtr<ID3D12PipelineState> state;
    ComPtr<ID3D12DescriptorHeap> source, target;
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    bool failed{};
} mirrorPipeline;
// 0.0.74 test 10 MONITOR (streamer_view.hpp; 0.0.73's STREAMER MODE, always on, no
// setting): in play (the game thread, each frame), the session's path (streamer_view::Path:
// undecided, headsetEye, or why the window went back to the game's own view for good), and
// the copy's own view (submission lock): the left eye's orientation and lens the copy in
// shownEye was made with, and whether it was taken for the monitor (so the sharpened headset
// image may be on the window). The window's smoothed view (present thread, submission lock)
// and its own pipeline.
std::atomic<bool> streamerWanted{};
std::atomic<uint32_t> streamerDraws{}, streamerPath{};
// 0.0.74 test 10 MONITOR MEMORY: a real device-removed sign was seen (GetDeviceRemovedReason failing,
// or the fence's UINT64_MAX, fault 3301) - what game_xr keeps in monitor-copy.txt.
std::atomic<bool> removedSeen{};
// streamer_view::Latch's rule on the atomic: the first fallback is final for the session.
void monitorFallBack(uint32_t why) {
    uint32_t path = streamerPath.load();
    while (!spidy::streamer_view::gameView(path) && why > spidy::streamer_view::headsetEye)
        if (streamerPath.compare_exchange_weak(path, why))
            return;
}
void monitorShown() {
    uint32_t path = spidy::streamer_view::undecided;
    streamerPath.compare_exchange_strong(path, spidy::streamer_view::headsetEye);
}
bool monitorStreaming() {
    return streamerWanted.load() && !spidy::streamer_view::gameView(streamerPath.load());
}
// Two GPUs (docs/TWO-GPU-074-test3.md): the window's monitor must hang off the game's own GPU,
// or the copy is not drawn there (checked once a session; 0 not yet, 1 same GPU).
uint32_t outputChecks{};
bool outputSameGpu{};
struct ShownView {
    spidy::Quat orientation{};
    spidy::streamer_view::Fov fov{};
    bool valid{}, streamer{};
} shownView;
spidy::streamer_view::Smoother streamerSmoother;
int64_t streamerQpc{};
MirrorPipeline streamerPipeline;
std::atomic<uint32_t> screenSkipped{}, backBufferFormat{};
void publishScreen(uint32_t status, const void* rows, uint32_t rowBytes) {
    std::lock_guard lock(telemetry);
    auto& d = SpidyScreenData;
    InterlockedIncrement64(&d.sequence);
    if (status)
        d.status = status;
    d.frames = screen.frame;
    d.width = screen.width;
    d.height = screen.height;
    d.format = screen.view;
    if (rows) {
        d.pixels = reinterpret_cast<uint64_t>(rows);
        d.rowBytes = rowBytes;
    }
    d.presents = screenPresents;
    d.skip = screenSkipped;
    d.backBufferFormat = backBufferFormat;
    InterlockedIncrement64(&d.sequence);
}
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
void publish() { // submission and recording held by caller
    std::lock_guard lock(telemetry);
    auto& d = SpidyGpuData;
    InterlockedIncrement64(&d.sequence);
    d.error = fault.load();
    d.executes = executes;
    d.markers = markers;
    d.matchedMarkers = matchedMarkers;
    d.pairs = pairs;
    d.serial = lastSerial;
    d.generation = lastGeneration;
    d.fence = fenceValue;
    d.completed = fence ? fence->GetCompletedValue() : 0;
    if (fence)
        noteCopies(d.completed);
    d.width = imageWidth();
    d.height = imageHeight();
    d.captured = capturedPairs;
    d.reused = reusedPairs;
    d.capturedSerial = capturedStamp.serial;
    for (unsigned i = 0; i < 2; ++i) {
        d.begins[i] = begins[i];
        d.ends[i] = ends[i];
        d.resources[i] = reinterpret_cast<uint64_t>(resources[i].Get());
        d.states[i] = states[i];
        d.known[i] = known[i];
        d.pixels[i] = pixels[i].empty() ? 0 : reinterpret_cast<uint64_t>(pixels[i].data());
    }
    InterlockedIncrement64(&d.sequence);
}
void add(ID3D12CommandList* list, const Event& event) {
    auto found = lists.find(list);
    if (found == lists.end()) {
        if (lists.size() >= 4096) {
            fault = 3101;
            return;
        }
        found = lists.emplace(list, std::vector<Event>{}).first;
    }
    if (found->second.size() >= 4096) {
        fault = 3102;
        return;
    }
    found->second.push_back(event);
}
// Update 37.2: FOVEATED (foveation.hpp), D3D12 Variable Rate Shading tier 2 on the eye views'
// G-buffer passes only. The hooks go in the first time it is turned on (never before: OFF costs
// nothing) and stay; foveaOn gates them.
using OmSetTargets = void(STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*, UINT, const D3D12_CPU_DESCRIPTOR_HANDLE*,
                                              BOOL, const D3D12_CPU_DESCRIPTOR_HANDLE*);
using SetViewports = void(STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*, UINT, const D3D12_VIEWPORT*);
OmSetTargets originalOmSetTargets{};
SetViewports originalSetViewports{};
std::array<void*, 2> foveaHooks{};
std::atomic<bool> foveaHooked{}, foveaOn{}, foveaDebug{};
std::atomic<uint32_t> foveaLevel{}, foveaInner{foveation::defaultInner}, foveaFault{};
std::atomic<uint32_t> foveaTier{}, foveaTile{}, foveaHas4x4{};
std::array<std::atomic<float>, 4> foveaCentres{}; // left u, v, right u, v (0: not known yet)
std::atomic<uint64_t> foveaTagged{}, foveaApplied{}, foveaUntagged{}, foveaResets{}, foveaImageCount{},
    foveaMismatches{};
std::atomic<float> foveaShare{1};
std::array<std::atomic<ID3D12Resource*>, 2> foveaImage{};
std::atomic<uint64_t> foveaReady{UINT64_MAX};
std::atomic<UINT> foveaW{}, foveaH{};
constexpr uint64_t foveaMismatchLimit = 30;
// The pass the game records now, when it is inside an eye view's render job (the begin
// timestamp Spidy marks, endQuery): that list and eye, and whether the rate is set on it.
struct FoveaTag {
    ID3D12GraphicsCommandList* list{};
    unsigned eye{};
    bool set{};
    // 0.0.74 test 10 SEE-THROUGH: the job's pair stamp; whether its G-buffer pass was seen (and
    // its depth view), whether its depth was dealt with (copied or skipped), and the depth view
    // last cleared on it with its value (the convention check).
    EyePairState::Stamp stamp{};
    bool gbuffer{}, depthDone{};
    SIZE_T gbufferDsv{}, clearedDsv{};
    float clearedDepth = -1;
};
thread_local FoveaTag foveaTag{};
// 0.0.74 test 10 SEE-THROUGH (Batmobile / van seen through walls): the eye jobs' scene depth for
// Spidy's world-placed models (d3d12_renderer's occluded models). Passive observers: the
// OMSetRenderTargets / RSSetViewports hooks above (installed with the eyes now; foveaOn still
// gates everything they change) and ClearDepthStencilView, which only reads. At the first
// transition out of DEPTH_WRITE of an eye-sized D32 depth after the job's G-buffer pass, the
// depth is copied, on the game's own list, into a ring slot of that eye. Anything unexpected
// (other format or size, another queue, a lost list, an allocation) skips or turns only this
// off: never `fault`, never the eyes.
using ClearDepth = void(STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*, D3D12_CPU_DESCRIPTOR_HANDLE,
                                            D3D12_CLEAR_FLAGS, FLOAT, UINT8, UINT, const D3D12_RECT*);
ClearDepth originalClearDepth{};
void* clearDepthHook{};
std::atomic<bool> clearDepthOn{};
std::recursive_mutex depthLock; // recursive for the same reason as `recording`
constexpr unsigned depthRingSize = 3;
struct DepthSlot {
    ComPtr<ID3D12Resource> texture; // R32_TYPELESS, PIXEL_SHADER_RESOURCE between copies
    EyePairState::Stamp stamp{};
    ID3D12CommandList* list{};       // the game list the copy is recorded on
    bool pending{}, executed{};
};
std::array<std::array<DepthSlot, depthRingSize>, 2> depthRing;
std::array<unsigned, 2> depthNext{};
UINT depthW{}, depthH{};
struct RetiredDepth {
    ComPtr<ID3D12Resource> texture;
    uint64_t ms{};
};
std::vector<RetiredDepth> retiredDepth;
// Why depth is off for the session (0: on): 1 standard-Z clear, 2 other clear value,
// 3 allocation failed, 4 hooks not installed.
std::atomic<uint32_t> depthOff{};
std::atomic<uint64_t> depthCaptured{}, depthSkipSize{}, depthSkipFormat{}, depthSkipNoPass{},
    depthSkipQueue{}, depthSkipLost{}, depthClearReversed{}, depthClearStandard{}, depthClearOther{},
    depthGiven{}, depthMissed{};
std::atomic<uint32_t> depthSeenFormat{}, depthSeenW{}, depthSeenH{};
bool depthFormatOk(DXGI_FORMAT f) { return f == DXGI_FORMAT_D32_FLOAT || f == DXGI_FORMAT_R32_TYPELESS; }
bool depthFamily(DXGI_FORMAT f) {
    switch (f) {
    case DXGI_FORMAT_D32_FLOAT:
    case DXGI_FORMAT_R32_TYPELESS:
    case DXGI_FORMAT_D24_UNORM_S8_UINT:
    case DXGI_FORMAT_R24G8_TYPELESS:
    case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
    case DXGI_FORMAT_R32G8X24_TYPELESS:
    case DXGI_FORMAT_D16_UNORM:
    case DXGI_FORMAT_R16_TYPELESS:
        return true;
    default:
        return false;
    }
}
void depthStop(uint32_t why) {
    uint32_t none = 0;
    depthOff.compare_exchange_strong(none, why);
}
// The G-buffer pass's depth view on the tagged job: the clear seen on it says which way the
// depth runs (0 far, reversed: as the eye lens is made; 1: standard, depth is not used).
// Judged whichever comes first, the clear or the pass's binding; a later clear of it to the far
// value leaves sky (nothing hides), to anything else turns the depth off.
void depthJudge(FoveaTag& t) {
    if (!t.gbuffer || !t.gbufferDsv || t.clearedDsv != t.gbufferDsv || t.clearedDepth < 0)
        return;
    if (t.clearedDepth == 0.f)
        ++depthClearReversed;
    else if (t.clearedDepth == 1.f) {
        ++depthClearStandard;
        depthStop(1);
    } else {
        ++depthClearOther;
        depthStop(2);
    }
    t.clearedDepth = -1; // counted once
}
void depthNotePass(FoveaTag& t, const D3D12_CPU_DESCRIPTOR_HANDLE* dsv) {
    t.gbuffer = true;
    t.gbufferDsv = dsv ? dsv->ptr : 0;
    depthJudge(t);
}
void STDMETHODCALLTYPE clearDepth(ID3D12GraphicsCommandList* list, D3D12_CPU_DESCRIPTOR_HANDLE dsv,
                                  D3D12_CLEAR_FLAGS flags, FLOAT depth, UINT8 stencil, UINT rects,
                                  const D3D12_RECT* rect) {
    originalClearDepth(list, dsv, flags, depth, stencil, rects, rect);
    auto& t = foveaTag;
    if (t.list != list || !(flags & D3D12_CLEAR_FLAG_DEPTH) || rects)
        return;
    t.clearedDsv = dsv.ptr;
    t.clearedDepth = std::isfinite(depth) ? depth : 2.f;
    depthJudge(t);
}
// Barrier hook, before the game's own barrier: on the tagged job after its G-buffer pass.
void depthBarrier(ID3D12GraphicsCommandList* list, UINT count, const D3D12_RESOURCE_BARRIER* bs) {
    auto& t = foveaTag;
    if (!bs || t.eye > 1 || depthOff.load(std::memory_order_relaxed) || !staged)
        return;
    for (UINT n = 0; n < count; ++n) {
        const auto& b = bs[n];
        if (b.Type != D3D12_RESOURCE_BARRIER_TYPE_TRANSITION || b.Flags != D3D12_RESOURCE_BARRIER_FLAG_NONE ||
            b.Transition.StateBefore != D3D12_RESOURCE_STATE_DEPTH_WRITE || !b.Transition.pResource ||
            (b.Transition.Subresource != 0 && b.Transition.Subresource != D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES))
            continue;
        auto* res = b.Transition.pResource;
        const auto d = res->GetDesc();
        if (d.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D || !depthFamily(d.Format))
            continue;
        t.depthDone = true; // the first depth out of the pass: this one or none
        depthSeenFormat = static_cast<uint32_t>(d.Format);
        depthSeenW = static_cast<uint32_t>(d.Width);
        depthSeenH = d.Height;
        UINT w{}, h{};
        {
            std::lock_guard recordLock(recording);
            w = width;
            h = height;
        }
        if (!w || !h || d.Width != w || d.Height != h) {
            ++depthSkipSize;
            return;
        }
        if (!depthFormatOk(d.Format) || d.MipLevels != 1 || d.DepthOrArraySize != 1 || d.SampleDesc.Count != 1) {
            ++depthSkipFormat;
            return;
        }
        std::lock_guard lock(depthLock);
        if (!device)
            return;
        const auto now = GetTickCount64();
        std::erase_if(retiredDepth, [&](const RetiredDepth& r) { return now - r.ms > 3000; });
        if (depthW != w || depthH != h) { // a new eye size: new slots, the old kept a while
            for (auto& eye : depthRing)
                for (auto& slot : eye) {
                    if (slot.texture)
                        retiredDepth.push_back({std::move(slot.texture), now});
                    slot = {};
                }
            depthW = w;
            depthH = h;
        }
        auto& slot = depthRing[t.eye][depthNext[t.eye]];
        if (!slot.texture) {
            D3D12_HEAP_PROPERTIES heap{};
            heap.Type = D3D12_HEAP_TYPE_DEFAULT;
            D3D12_RESOURCE_DESC desc{};
            desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
            desc.Width = w;
            desc.Height = h;
            desc.DepthOrArraySize = 1;
            desc.MipLevels = 1;
            desc.Format = DXGI_FORMAT_R32_TYPELESS;
            desc.SampleDesc.Count = 1;
            desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
            desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
            if (FAILED(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc,
                                                       D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, nullptr,
                                                       IID_PPV_ARGS(&slot.texture))) ||
                !slot.texture) {
                slot = {};
                depthStop(3);
                return;
            }
        }
        depthNext[t.eye] = (depthNext[t.eye] + 1) % depthRingSize;
        D3D12_RESOURCE_BARRIER to[2]{};
        to[0].Type = to[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        to[0].Transition = {res, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_STATE_DEPTH_WRITE,
                            D3D12_RESOURCE_STATE_COPY_SOURCE};
        to[1].Transition = {slot.texture.Get(), D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST};
        originalBarrier(list, 2, to);
        list->CopyResource(slot.texture.Get(), res);
        for (auto& b2 : to)
            std::swap(b2.Transition.StateBefore, b2.Transition.StateAfter);
        originalBarrier(list, 2, to); // the game's own barrier follows from DEPTH_WRITE, as it expects
        slot.stamp = t.stamp;
        slot.list = list;
        slot.pending = true;
        slot.executed = false;
        ++depthCaptured;
        return;
    }
}
// The game's queue ran these lists (or another queue did: that copy is not trusted), or a
// list was reset before it ran (its copy never happens).
void depthExecuted(bool gameQueue, UINT count, ID3D12CommandList* const* commands) {
    if (!depthCaptured.load(std::memory_order_relaxed) || !commands)
        return;
    std::lock_guard lock(depthLock);
    for (auto& eye : depthRing)
        for (auto& slot : eye)
            if (slot.pending)
                for (UINT n = 0; n < count; ++n)
                    if (commands[n] == slot.list) {
                        slot.pending = false;
                        slot.executed = gameQueue;
                        if (!gameQueue) {
                            slot.stamp = {};
                            ++depthSkipQueue;
                        }
                        break;
                    }
}
void depthReset(ID3D12GraphicsCommandList* list) {
    if (!depthCaptured.load(std::memory_order_relaxed))
        return; // nothing ever recorded: no lock on every list's Reset
    std::lock_guard lock(depthLock);
    for (auto& eye : depthRing)
        for (auto& slot : eye)
            if (slot.pending && slot.list == list) {
                slot.pending = slot.executed = false;
                slot.stamp = {};
                slot.list = nullptr;
                ++depthSkipLost;
            }
}
void stopFovea(uint32_t why) {
    uint32_t none = foveation::fine;
    foveaFault.compare_exchange_strong(none, why);
    foveaOn = false;
}
void clearRate(ID3D12GraphicsCommandList* list) {
    foveaTag.set = false;
    ComPtr<ID3D12GraphicsCommandList5> l5;
    if (SUCCEEDED(list->QueryInterface(IID_PPV_ARGS(&l5)))) {
        l5->RSSetShadingRateImage(nullptr);
        l5->RSSetShadingRate(D3D12_SHADING_RATE_1X1, nullptr);
        ++foveaResets;
    }
}
void STDMETHODCALLTYPE omSetTargets(ID3D12GraphicsCommandList* list, UINT n, const D3D12_CPU_DESCRIPTOR_HANDLE* rtvs,
                                    BOOL single, const D3D12_CPU_DESCRIPTOR_HANDLE* dsv) {
    originalOmSetTargets(list, n, rtvs, single, dsv);
    auto& t = foveaTag;
    const bool gbuffer = n >= 3 && dsv;
    if (t.list != list) {
        if (gbuffer && foveaOn.load(std::memory_order_relaxed))
            foveaUntagged.fetch_add(1, std::memory_order_relaxed);
        return;
    }
    if (t.set)
        clearRate(list); // each pass starts at full rate; only a G-buffer pass gets the image
    if (!gbuffer)
        return;
    foveaTagged.fetch_add(1, std::memory_order_relaxed);
    if (!t.gbuffer)
        depthNotePass(t, dsv); // 0.0.74 test 10 SEE-THROUGH: reads only
    if (!foveaOn.load(std::memory_order_relaxed) || t.eye > 1)
        return;
    auto* image = foveaImage[t.eye].load();
    if (!image || !fence || fence->GetCompletedValue() < foveaReady.load())
        return;
    ComPtr<ID3D12GraphicsCommandList5> l5;
    if (FAILED(list->QueryInterface(IID_PPV_ARGS(&l5)))) {
        stopFovea(foveation::noInterface);
        return;
    }
    const D3D12_SHADING_RATE_COMBINER combiners[2] = {D3D12_SHADING_RATE_COMBINER_PASSTHROUGH,
                                                      D3D12_SHADING_RATE_COMBINER_OVERRIDE};
    l5->RSSetShadingRate(D3D12_SHADING_RATE_1X1, combiners);
    l5->RSSetShadingRateImage(image);
    t.set = true;
    foveaApplied.fetch_add(1, std::memory_order_relaxed);
}
// A pass drawn at another size than the image was made for (an upscaler's lower resolution):
// that pass goes back to full rate; many of them turn FOVEATED off (OTHER SIZE).
void STDMETHODCALLTYPE setViewports(ID3D12GraphicsCommandList* list, UINT n, const D3D12_VIEWPORT* v) {
    originalSetViewports(list, n, v);
    auto& t = foveaTag;
    if (!t.set || t.list != list || !n || !v)
        return;
    const auto w = static_cast<float>(foveaW.load()), h = static_cast<float>(foveaH.load());
    if (std::fabs(v[0].Width - w) > w * .02f + 1 || std::fabs(v[0].Height - h) > h * .02f + 1) {
        clearRate(list);
        if (foveaMismatches.fetch_add(1) + 1 >= foveaMismatchLimit)
            stopFovea(foveation::sizeMismatch);
    }
}
// endQuery: an eye job's begin timestamp tags the list; its end clears it (and the rate).
void foveaMarker(ID3D12GraphicsCommandList* list) {
    if (!foveaHooked.load(std::memory_order_relaxed) || !currentMarker.valid)
        return;
    auto& t = foveaTag;
    if (!currentMarker.ended) {
        if (t.set && t.list && t.list != list)
            t.set = false; // that list's rate was left on: its own job end or Reset clears it
        t = {list, currentMarker.eye, false};
        t.stamp = currentMarker.stamp; // 0.0.74 test 10 SEE-THROUGH
    } else if (t.list == list) {
        if (t.set)
            clearRate(list);
        if (t.gbuffer && !t.depthDone)
            ++depthSkipNoPass; // 0.0.74 test 10 SEE-THROUGH: no depth left the pass in the job
        t = {};
    }
}
void STDMETHODCALLTYPE barrier(ID3D12GraphicsCommandList* list, UINT count,
                               const D3D12_RESOURCE_BARRIER* bs) {
    // Update 27: ADDRESS FINDER callout before the early return (read-only).
    native_address_finder::onBarrier(list, count, bs);
    // 0.0.74 test 10 SEE-THROUGH: the scene depth, copied ahead of the game's own barrier.
    if (enabled && foveaTag.list == list && foveaTag.gbuffer && !foveaTag.depthDone)
        depthBarrier(list, count, bs);
    if (enabled) {
        const auto* left = watched[0].load();
        const auto* right = watched[1].load();
        const auto* mirror = mvWatched.load();
        bool relevant{};
        for (UINT n = 0; n < count; ++n)
            relevant |= bs[n].Type == D3D12_RESOURCE_BARRIER_TYPE_TRANSITION &&
                        (bs[n].Transition.pResource == left || bs[n].Transition.pResource == right ||
                         (mirror && bs[n].Transition.pResource == mirror));
        if (!relevant) {
            originalBarrier(list, count, bs);
            return;
        }
        std::lock_guard lock(recording);
        for (UINT n = 0; n < count; ++n)
            if (bs[n].Type == D3D12_RESOURCE_BARRIER_TYPE_TRANSITION)
                for (unsigned i = 0; i < 2; ++i)
                    if (resources[i] && bs[n].Transition.pResource == resources[i].Get()) {
                        Event e{};
                        e.eye = i;
                        e.transition = bs[n].Transition;
                        e.flags = bs[n].Flags;
                        add(list, e);
                    }
        for (UINT n = 0; n < count; ++n) // 0.0.71 MIRROR VIEW: its image's states, as eye 2
            if (bs[n].Type == D3D12_RESOURCE_BARRIER_TYPE_TRANSITION && mvResource &&
                bs[n].Transition.pResource == mvResource.Get()) {
                Event e{};
                e.eye = 2;
                e.transition = bs[n].Transition;
                e.flags = bs[n].Flags;
                add(list, e);
            }
    }
    originalBarrier(list, count, bs);
}
HRESULT STDMETHODCALLTYPE reset(ID3D12GraphicsCommandList* list, ID3D12CommandAllocator* a,
                                ID3D12PipelineState* p) {
    auto result = originalReset(list, a, p);
    if (foveaTag.list == list)
        foveaTag = {}; // Update 37.2: a reset list has no shading rate
    if (SUCCEEDED(result))
        depthReset(list); // 0.0.74 test 10 SEE-THROUGH
    if (enabled && SUCCEEDED(result)) {
        std::lock_guard lock(recording);
        lists.erase(list);
    }
    return result;
}
void STDMETHODCALLTYPE endQuery(ID3D12GraphicsCommandList* list, ID3D12QueryHeap* heap, D3D12_QUERY_TYPE type,
                                UINT index) {
    originalEndQuery(list, heap, type, index);
    if (enabled) {
        if (type == D3D12_QUERY_TYPE_TIMESTAMP)
            foveaMarker(list); // Update 37.2
        markers.fetch_add(1, std::memory_order_relaxed);
        if (!currentMarker.valid || type != D3D12_QUERY_TYPE_TIMESTAMP)
            return;
        std::lock_guard lock(recording);
        if (currentMarker.valid && type == D3D12_QUERY_TYPE_TIMESTAMP) {
            ++matchedMarkers;
            Event e{};
            e.eye = currentMarker.eye;
            e.marker = true;
            e.ended = currentMarker.ended;
            e.stamp = currentMarker.stamp;
            add(list, e);
        }
    }
}
bool makeReadbacks() {
    if (width && (!captureImages || (readbacks[0] && readbacks[1])))
        return true;
    if (!resources[0] || !resources[1])
        return false;
    for (unsigned i = 0; i < 2; ++i) {
        const auto desc = resources[i]->GetDesc();
        if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D ||
            desc.Format != DXGI_FORMAT_R8G8B8A8_UNORM || desc.MipLevels != 1 || desc.DepthOrArraySize != 1 ||
            desc.SampleDesc.Count != 1 || !validEyeSize(desc.Width) || !validEyeSize(desc.Height) ||
            (desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS)) {
            fault = 3201;
            return false;
        }
        if (i && (width != desc.Width || height != desc.Height)) {
            fault = 3202;
            return false;
        }
        width = static_cast<UINT>(desc.Width);
        height = desc.Height;
        outWidth = wantedOutWidth.load() ? wantedOutWidth.load() : width;
        outHeight = wantedOutHeight.load() ? wantedOutHeight.load() : height;
        if (!captureImages)
            continue;
        // Staged, the images read back are the headset's (copyLatest), at its size.
        auto image = desc;
        image.Width = imageWidth();
        image.Height = imageHeight();
        UINT64 bytes{};
        device->GetCopyableFootprints(&image, 0, 1, 0, &layouts[i], nullptr, nullptr, &bytes);
        D3D12_HEAP_PROPERTIES heap{};
        heap.Type = D3D12_HEAP_TYPE_READBACK;
        D3D12_RESOURCE_DESC buffer{};
        buffer.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        buffer.Width = bytes;
        buffer.Height = 1;
        buffer.DepthOrArraySize = 1;
        buffer.MipLevels = 1;
        buffer.SampleDesc.Count = 1;
        buffer.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        if (FAILED(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &buffer,
                                                   D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
                                                   IID_PPV_ARGS(&readbacks[i])))) {
            fault = 3203;
            return false;
        }
        pixels[i].resize(static_cast<size_t>(imageWidth()) * imageHeight() * 4);
    }
    return true;
}
CopyCommands* readyCommands(std::array<CopyCommands, 3>& pool) {
    const auto completed = fence->GetCompletedValue();
    if (completed == UINT64_MAX) {
        removedSeen = true; // the device was removed (0.0.74 test 10 MONITOR MEMORY)
        fault = 3301;
        return nullptr;
    }
    for (auto& slot : pool) {
        if (slot.fence > completed)
            continue;
        if (!slot.allocator) {
            if (FAILED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                                      IID_PPV_ARGS(&slot.allocator))) ||
                FAILED(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, slot.allocator.Get(),
                                                 nullptr, IID_PPV_ARGS(&slot.list))) ||
                FAILED(slot.list->Close())) {
                fault = 3801;
                return nullptr;
            }
        }
        if (FAILED(slot.allocator->Reset()) ||
            FAILED(originalReset(slot.list.Get(), slot.allocator.Get(), nullptr))) {
            fault = 3802;
            return nullptr;
        }
        return &slot;
    }
    return nullptr;
}
bool submitCommands(CopyCommands& slot) {
    if (FAILED(slot.list->Close())) {
        fault = 3803;
        return false;
    }
    ID3D12CommandList* list = slot.list.Get();
    originalExecute(queue.Get(), 1, &list);
    slot.fence = ++fenceValue;
    if (FAILED(queue->Signal(fence.Get(), slot.fence))) {
        fault = 3804;
        return false;
    }
    return true;
}
// Update 37.2: FOVEATED's shading-rate images, one per eye, made on Spidy's own list ahead of
// the game's submission (execute, under the submission lock) whenever the level, INNER SIZE, the
// lens centres or the eye size change. Earlier sets are kept 3 s (lists recorded with them).
struct FoveaSet {
    std::array<ComPtr<ID3D12Resource>, 2> eye;
    ComPtr<ID3D12Resource> upload;
    uint64_t retiredMs{};
};
std::vector<FoveaSet> foveaSets;
std::array<CopyCommands, 3> foveaCommands;
struct FoveaKey {
    uint32_t level{}, inner{}, w{}, h{};
    int32_t c[4]{};
    bool operator==(const FoveaKey&) const = default;
};
FoveaKey foveaBuilt{};
foveation::Centre foveaCentre(unsigned eye) {
    const float u = foveaCentres[2 * eye].load(), v = foveaCentres[2 * eye + 1].load();
    return u > 0 && v > 0 ? foveation::Centre{u, v} : foveation::Centre{};
}
void buildFovea() {
    if (!foveaOn.load() || !width || !height || !device)
        return;
    FoveaKey k{foveaLevel.load(), foveaInner.load(), width, height, {}};
    for (unsigned i = 0; i < 4; ++i)
        k.c[i] = static_cast<int32_t>(std::lround(foveaCentres[i].load() * 200));
    const auto now = GetTickCount64();
    std::erase_if(foveaSets, [&](const FoveaSet& f) { return f.retiredMs && now - f.retiredMs > 3000; });
    if (k == foveaBuilt)
        return;
    auto* slot = readyCommands(foveaCommands);
    if (!slot)
        return; // the next submission tries again
    FoveaSet set;
    const UINT tile = std::max(foveaTile.load(), 1u);
    uint32_t tilesW{}, tilesH{};
    std::array<std::vector<uint8_t>, 2> rates;
    for (unsigned i = 0; i < 2; ++i)
        rates[i] = foveation::image(k.level, k.inner, foveaCentre(i), width, height, tile, foveaHas4x4.load() != 0,
                                    tilesW, tilesH);
    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width = tilesW;
    desc.Height = tilesH;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_R8_UINT;
    desc.SampleDesc.Count = 1;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT foot{};
    UINT64 bytes{};
    device->GetCopyableFootprints(&desc, 0, 1, 0, &foot, nullptr, nullptr, &bytes);
    const UINT64 stride = (bytes + D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT - 1) &
                          ~static_cast<UINT64>(D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT - 1);
    D3D12_HEAP_PROPERTIES gpuHeap{}, uploadHeap{};
    gpuHeap.Type = D3D12_HEAP_TYPE_DEFAULT;
    uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;
    D3D12_RESOURCE_DESC buffer{};
    buffer.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    buffer.Width = stride * 2;
    buffer.Height = 1;
    buffer.DepthOrArraySize = 1;
    buffer.MipLevels = 1;
    buffer.SampleDesc.Count = 1;
    buffer.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    void* mapped{};
    bool ok = SUCCEEDED(device->CreateCommittedResource(&uploadHeap, D3D12_HEAP_FLAG_NONE, &buffer,
                                                        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                                        IID_PPV_ARGS(&set.upload))) &&
              SUCCEEDED(set.upload->Map(0, nullptr, &mapped));
    for (unsigned i = 0; ok && i < 2; ++i)
        ok = SUCCEEDED(device->CreateCommittedResource(&gpuHeap, D3D12_HEAP_FLAG_NONE, &desc,
                                                       D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
                                                       IID_PPV_ARGS(&set.eye[i])));
    if (!ok) {
        if (mapped)
            set.upload->Unmap(0, nullptr);
        slot->list->Close(); // opened by readyCommands; nothing recorded
        stopFovea(foveation::imageFailed);
        return;
    }
    for (unsigned i = 0; i < 2; ++i)
        for (uint32_t y = 0; y < tilesH; ++y)
            std::memcpy(static_cast<uint8_t*>(mapped) + stride * i + static_cast<size_t>(y) * foot.Footprint.RowPitch,
                        rates[i].data() + static_cast<size_t>(y) * tilesW, tilesW);
    set.upload->Unmap(0, nullptr);
    D3D12_RESOURCE_BARRIER b[2]{};
    for (unsigned i = 0; i < 2; ++i) {
        D3D12_TEXTURE_COPY_LOCATION dst{}, src{};
        dst.pResource = set.eye[i].Get();
        dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        src.pResource = set.upload.Get();
        src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        src.PlacedFootprint = foot;
        src.PlacedFootprint.Offset = stride * i;
        slot->list->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
        b[i].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        b[i].Transition = {set.eye[i].Get(), D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_STATE_COPY_DEST,
                           D3D12_RESOURCE_STATE_SHADING_RATE_SOURCE};
    }
    originalBarrier(slot->list.Get(), 2, b);
    if (!submitCommands(*slot)) {
        stopFovea(foveation::imageFailed);
        return;
    }
    // The fence first: a pass that sees the new image before its upload is done skips it.
    foveaReady = slot->fence;
    foveaW = width;
    foveaH = height;
    for (unsigned i = 0; i < 2; ++i)
        foveaImage[i] = set.eye[i].Get();
    for (auto& f : foveaSets)
        if (!f.retiredMs)
            f.retiredMs = now;
    foveaSets.push_back(std::move(set));
    foveaShare = (foveation::shadedShare(rates[0]) + foveation::shadedShare(rates[1])) / 2;
    foveaBuilt = k;
    ++foveaImageCount;
}
// Update 16: each eye pass's newest GPU time, for the wrist hologram's FPS
// page (eyePassesGpuMs): plain atomics, so reading it takes no lock.
struct PassClock {
    std::atomic<float> ms{};
    std::atomic<uint64_t> tick{}; // GetTickCount64 when noted; 0 never
    void note(double value) {
        ms = static_cast<float>(value);
        tick = GetTickCount64();
    }
};
PassClock aaClock, historyClock, sharpenClock;
// Update 35: the history pass's recent timings, for EYE AA's cost on the wrist.
std::mutex historyMedianLock;
eye_taa::MsMedian historyMedian;
// Eye anti-aliasing (native_eye_gpu.hpp, setEyeAA). The pass is built on the
// first pair that wants it, under the submission and recording locks.
std::atomic<uint32_t> aaMode{}, aaFault{};
// 0.0.74 test 10 SM1STYLE (sm1style.hpp; game_xr sets it every frame while SpidyVR's per-eye TAA runs): no FXAA, no
// EYE AA history and no CAS sharpen on the eyes, whatever the hologram, live.txt or the launcher asked for. The
// asked values are kept (eyeAA / eyeHistory / eyeSharpen, live.txt) and come back when it is released.
std::atomic<bool> sm1Lock{};
struct AaPipeline {
    ComPtr<ID3D12RootSignature> root;
    ComPtr<ID3D12PipelineState> state, down; // FXAA; the downsample filter
    ComPtr<ID3D12DescriptorHeap> views, targets;
    ComPtr<ID3D12QueryHeap> queries;
    ComPtr<ID3D12Resource> stamps; // readback, mapped while the bridge lives
    const volatile uint64_t* stampData{};
    double ticksPerMs{};
    std::array<bool, 3> timed{}; // a captureCommands slot holds a measured pass
    bool ready{};
} aa;
TimingMetric aaGpu; // copyTimingLock
std::atomic<uint64_t> aaPairs{};
// FXAA 3.11 quality (Timothy Lottes, NVIDIA), PC path, preset 29 steps, luma
// computed from RGB. The eyes hold display-encoded values, as FXAA expects.
const char* fxaaShader = R"(
Texture2D source : register(t0);
SamplerState bilinear : register(s0);
cbuffer Frame : register(b0) { float2 rcpFrame; float subpix; float edgeThreshold; };
static const float edgeThresholdMin = 0.0625;
static const float steps[12] = {1.0, 1.5, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 4.0, 8.0};
float luma(float3 c) { return dot(c, float3(0.299, 0.587, 0.114)); }
float lumaAt(float2 p) { return luma(source.SampleLevel(bilinear, p, 0).rgb); }
float lumaOff(float2 p, int2 o) { return luma(source.SampleLevel(bilinear, p, 0, o).rgb); }
float4 vs(uint id : SV_VertexID) : SV_POSITION {
    const float2 corner = float2((id << 1) & 2, id & 2);
    return float4(corner * float2(2, -2) + float2(-1, 1), 0, 1);
}
float4 ps(float4 position : SV_POSITION) : SV_TARGET {
    float2 posM = position.xy * rcpFrame;
    const float4 rgbyM = source.SampleLevel(bilinear, posM, 0);
    const float lumaM = luma(rgbyM.rgb);
    float lumaS = lumaOff(posM, int2(0, 1));
    const float lumaE = lumaOff(posM, int2(1, 0));
    float lumaN = lumaOff(posM, int2(0, -1));
    const float lumaW = lumaOff(posM, int2(-1, 0));
    const float rangeMax = max(max(lumaN, lumaW), max(lumaE, max(lumaS, lumaM)));
    const float rangeMin = min(min(lumaN, lumaW), min(lumaE, min(lumaS, lumaM)));
    const float range = rangeMax - rangeMin;
    if (range < max(edgeThresholdMin, rangeMax * edgeThreshold))
        return rgbyM;
    const float lumaNW = lumaOff(posM, int2(-1, -1));
    const float lumaSE = lumaOff(posM, int2(1, 1));
    const float lumaNE = lumaOff(posM, int2(1, -1));
    const float lumaSW = lumaOff(posM, int2(-1, 1));
    const float lumaNS = lumaN + lumaS;
    const float lumaWE = lumaW + lumaE;
    const float subpixRcpRange = 1.0 / range;
    const float subpixNSWE = lumaNS + lumaWE;
    const float edgeHorz1 = -2.0 * lumaM + lumaNS;
    const float edgeVert1 = -2.0 * lumaM + lumaWE;
    const float lumaNESE = lumaNE + lumaSE;
    const float lumaNWNE = lumaNW + lumaNE;
    const float edgeHorz2 = -2.0 * lumaE + lumaNESE;
    const float edgeVert2 = -2.0 * lumaN + lumaNWNE;
    const float lumaNWSW = lumaNW + lumaSW;
    const float lumaSWSE = lumaSW + lumaSE;
    const float edgeHorz4 = abs(edgeHorz1) * 2.0 + abs(edgeHorz2);
    const float edgeVert4 = abs(edgeVert1) * 2.0 + abs(edgeVert2);
    const float edgeHorz3 = -2.0 * lumaW + lumaNWSW;
    const float edgeVert3 = -2.0 * lumaS + lumaSWSE;
    const float edgeHorz = abs(edgeHorz3) + edgeHorz4;
    const float edgeVert = abs(edgeVert3) + edgeVert4;
    const float subpixNWSWNESE = lumaNWSW + lumaNESE;
    float lengthSign = rcpFrame.x;
    const bool horzSpan = edgeHorz >= edgeVert;
    const float subpixA = subpixNSWE * 2.0 + subpixNWSWNESE;
    if (!horzSpan) {
        lumaN = lumaW;
        lumaS = lumaE;
    } else
        lengthSign = rcpFrame.y;
    const float subpixB = subpixA * (1.0 / 12.0) - lumaM;
    const float gradientN = lumaN - lumaM;
    const float gradientS = lumaS - lumaM;
    float lumaNN = lumaN + lumaM;
    const float lumaSS = lumaS + lumaM;
    const bool pairN = abs(gradientN) >= abs(gradientS);
    const float gradient = max(abs(gradientN), abs(gradientS));
    if (pairN)
        lengthSign = -lengthSign;
    const float subpixC = saturate(abs(subpixB) * subpixRcpRange);
    float2 posB = posM;
    const float2 offNP = horzSpan ? float2(rcpFrame.x, 0.0) : float2(0.0, rcpFrame.y);
    if (!horzSpan)
        posB.x += lengthSign * 0.5;
    else
        posB.y += lengthSign * 0.5;
    float2 posN = posB - offNP * steps[0];
    float2 posP = posB + offNP * steps[0];
    const float subpixD = -2.0 * subpixC + 3.0;
    float lumaEndN = lumaAt(posN);
    const float subpixE = subpixC * subpixC;
    float lumaEndP = lumaAt(posP);
    if (!pairN)
        lumaNN = lumaSS;
    const float gradientScaled = gradient * 0.25;
    const float lumaMM = lumaM - lumaNN * 0.5;
    const float subpixF = subpixD * subpixE;
    const bool lumaMLTZero = lumaMM < 0.0;
    lumaEndN -= lumaNN * 0.5;
    lumaEndP -= lumaNN * 0.5;
    bool doneN = abs(lumaEndN) >= gradientScaled;
    bool doneP = abs(lumaEndP) >= gradientScaled;
    if (!doneN)
        posN -= offNP * steps[1];
    if (!doneP)
        posP += offNP * steps[1];
    for (int i = 2; i < 12; ++i) {
        if (doneN && doneP)
            break;
        if (!doneN)
            lumaEndN = lumaAt(posN) - lumaNN * 0.5;
        if (!doneP)
            lumaEndP = lumaAt(posP) - lumaNN * 0.5;
        doneN = abs(lumaEndN) >= gradientScaled;
        doneP = abs(lumaEndP) >= gradientScaled;
        if (!doneN)
            posN -= offNP * steps[i];
        if (!doneP)
            posP += offNP * steps[i];
    }
    float dstN = posM.x - posN.x;
    float dstP = posP.x - posM.x;
    if (!horzSpan) {
        dstN = posM.y - posN.y;
        dstP = posP.y - posM.y;
    }
    const bool goodSpanN = (lumaEndN < 0.0) != lumaMLTZero;
    const bool goodSpanP = (lumaEndP < 0.0) != lumaMLTZero;
    const float spanLengthRcp = 1.0 / (dstP + dstN);
    const bool directionN = dstN < dstP;
    const float dst = min(dstN, dstP);
    const bool goodSpan = directionN ? goodSpanN : goodSpanP;
    const float subpixG = subpixF * subpixF;
    const float pixelOffset = dst * -spanLengthRcp + 0.5;
    const float subpixH = subpixG * subpix;
    const float pixelOffsetGood = goodSpan ? pixelOffset : 0.0;
    const float pixelOffsetSubpix = max(pixelOffsetGood, subpixH);
    if (!horzSpan)
        posM.x += pixelOffsetSubpix * lengthSign;
    else
        posM.y += pixelOffsetSubpix * lengthSign;
    return float4(source.SampleLevel(bilinear, posM, 0).rgb, rgbyM.a);
}
)";
// The downsample for supersampled eyes (setEyeOutput): each headset pixel is
// the Mitchell-Netravali (B = C = 1/3) filtered average of the eye's pixels
// under it, out to `reach` headset pixels from its centre, in linear light
// (the eyes hold sRGB-encoded values). A plain or bilinear copy at that ratio
// keeps the aliasing of patterns finer than the headset's pixels (the curved
// moire on rows of fins); this filter removes them with little blur.
const char* downsampleShader = R"(
Texture2D source : register(t0);
cbuffer Frame : register(b0) { float2 scale; float reach; float unused; };
float4 vs(uint id : SV_VertexID) : SV_POSITION {
    const float2 corner = float2((id << 1) & 2, id & 2);
    return float4(corner * float2(2, -2) + float2(-1, 1), 0, 1);
}
float mitchell(float x) {
    x = abs(x);
    if (x >= 2.0)
        return 0.0;
    const float x2 = x * x, x3 = x2 * x;
    if (x < 1.0)
        return (7.0 * x3 - 12.0 * x2 + 16.0 / 3.0) / 6.0;
    return (-7.0 / 3.0 * x3 + 12.0 * x2 - 20.0 * x + 32.0 / 3.0) / 6.0;
}
float3 toLinear(float3 c) { return c <= 0.04045 ? c / 12.92 : pow((c + 0.055) / 1.055, 2.4); }
float3 toDisplay(float3 c) { return c <= 0.0031308 ? c * 12.92 : 1.055 * pow(c, 1.0 / 2.4) - 0.055; }
float4 ps(float4 position : SV_POSITION) : SV_TARGET {
    uint w, h;
    source.GetDimensions(w, h);
    const float2 centre = position.xy * scale;     // in the eye's pixels
    const float2 step = max(scale, 1.0);           // eye pixels per filter unit
    const float2 radius = min(reach, 2.0) * step;
    const int2 lo = max(int2(floor(centre - radius)), 0);
    const int2 hi = min(int2(ceil(centre + radius)), int2(w, h) - 1);
    float3 sum = 0;
    float total = 0;
    [loop] for (int y = lo.y; y <= hi.y; ++y) {
        const float wy = mitchell((y + 0.5 - centre.y) / step.y);
        if (wy == 0.0)
            continue;
        [loop] for (int x = lo.x; x <= hi.x; ++x) {
            const float weight = wy * mitchell((x + 0.5 - centre.x) / step.x);
            sum += weight * toLinear(source.Load(int3(x, y, 0)).rgb);
            total += weight;
        }
    }
    const float3 colour = total > 0.0 ? saturate(sum / total) : toLinear(source.Load(int3(min(int2(centre), int2(w, h) - 1), 0)).rgb);
    return float4(toDisplay(colour), source.Load(int3(min(int2(centre), int2(w, h) - 1), 0)).a);
}
)";
// The Mitchell filter's full reach, in headset pixels.
constexpr float downsampleRadius = 2.f;
// FXAA's quality settings: sub-pixel smoothing 0.5 (its default 0.75 softens
// the eyes' fine detail more than a headset wants) and edge threshold 0.125
// (its "high quality" value).
constexpr float aaSubpix = 0.5f, aaEdgeThreshold = 0.125f;
// Update 37: AA QUALITY 1, STRONG (live.txt aa_quality): FXAA 3.11's own strongest
// thresholds (edge 0.063, "overkill", and its default sub-pixel 0.75): fainter edges
// smoothed too, a little softer. Not SMAA (no SMAA pass in this build).
constexpr float aaStrongSubpix = 0.75f, aaStrongEdgeThreshold = 0.063f;
std::atomic<uint32_t> aaQuality{};
bool failAa(uint32_t code) {
    aaFault = code;
    aa.ready = false;
    return false;
}
// The AA pass for the eye images and stagedEyes (both fixed for the bridge's
// life, so the descriptors are written once). Submission and recording held.
bool prepareAa() {
    if (aaFault)
        return false;
    if (aa.ready)
        return true;
    for (unsigned i = 0; i < 2; ++i)
        if (resources[i]->GetDesc().Flags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE)
            return failAa(5);
    D3D12_DESCRIPTOR_RANGE range{D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, 0};
    D3D12_ROOT_PARAMETER parameters[2]{};
    parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    parameters[0].DescriptorTable = {1, &range};
    parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    parameters[1].Constants.Num32BitValues = 4;
    parameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_STATIC_SAMPLER_DESC sampler{};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.MaxLOD = D3D12_FLOAT32_MAX;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_ROOT_SIGNATURE_DESC rs{2, parameters, 1, &sampler, D3D12_ROOT_SIGNATURE_FLAG_NONE};
    ComPtr<ID3DBlob> signature, error;
    if (FAILED(D3D12SerializeRootSignature(&rs, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error)) ||
        FAILED(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(),
                                           IID_PPV_ARGS(&aa.root))))
        return failAa(1);
    // FXAA or, for supersampled eyes, the downsample filter: the same root.
    const bool down = downsampling();
    const char* shader = down ? downsampleShader : fxaaShader;
    const char* name = down ? "spidy_eye_downsample" : "spidy_eye_fxaa";
    ComPtr<ID3DBlob> vs, ps;
    if (FAILED(D3DCompile(shader, std::strlen(shader), name, nullptr, nullptr, "vs", "vs_5_0",
                          D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &vs, &error)) ||
        FAILED(D3DCompile(shader, std::strlen(shader), name, nullptr, nullptr, "ps", "ps_5_0",
                          D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &ps, &error)))
        return failAa(2);
    D3D12_GRAPHICS_PIPELINE_STATE_DESC d{};
    d.pRootSignature = aa.root.Get();
    d.VS = {vs->GetBufferPointer(), vs->GetBufferSize()};
    d.PS = {ps->GetBufferPointer(), ps->GetBufferSize()};
    d.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    d.NumRenderTargets = 1;
    d.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    d.SampleDesc.Count = 1;
    d.SampleMask = UINT_MAX;
    d.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    d.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    d.RasterizerState.DepthClipEnable = TRUE;
    auto& blend = d.BlendState.RenderTarget[0];
    blend.SrcBlend = blend.SrcBlendAlpha = D3D12_BLEND_ONE;
    blend.DestBlend = blend.DestBlendAlpha = D3D12_BLEND_ZERO;
    blend.BlendOp = blend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    blend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    if (FAILED(device->CreateGraphicsPipelineState(&d, IID_PPV_ARGS(down ? &aa.down : &aa.state))))
        return failAa(3);
    D3D12_DESCRIPTOR_HEAP_DESC views{};
    views.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    views.NumDescriptors = 2;
    views.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    D3D12_DESCRIPTOR_HEAP_DESC targetViews{};
    targetViews.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    targetViews.NumDescriptors = 2;
    if (FAILED(device->CreateDescriptorHeap(&views, IID_PPV_ARGS(&aa.views))) ||
        FAILED(device->CreateDescriptorHeap(&targetViews, IID_PPV_ARGS(&aa.targets))))
        return failAa(4);
    const auto viewStep = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    const auto targetStep = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    for (unsigned i = 0; i < 2; ++i) {
        D3D12_SHADER_RESOURCE_VIEW_DESC sv{};
        sv.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        sv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        sv.Texture2D.MipLevels = 1;
        auto view = aa.views->GetCPUDescriptorHandleForHeapStart();
        view.ptr += static_cast<SIZE_T>(viewStep) * i;
        device->CreateShaderResourceView(resources[i].Get(), &sv, view);
        D3D12_RENDER_TARGET_VIEW_DESC rv{};
        rv.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        rv.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        auto target = aa.targets->GetCPUDescriptorHandleForHeapStart();
        target.ptr += static_cast<SIZE_T>(targetStep) * i;
        device->CreateRenderTargetView(stagedEyes[i].Get(), &rv, target);
    }
    // The GPU time is optional: without timestamps the pass still runs.
    UINT64 frequency{};
    D3D12_QUERY_HEAP_DESC qd{};
    qd.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
    qd.Count = 2 * static_cast<UINT>(captureCommands.size());
    D3D12_HEAP_PROPERTIES readback{};
    readback.Type = D3D12_HEAP_TYPE_READBACK;
    D3D12_RESOURCE_DESC buffer{};
    buffer.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    buffer.Width = sizeof(uint64_t) * qd.Count;
    buffer.Height = 1;
    buffer.DepthOrArraySize = 1;
    buffer.MipLevels = 1;
    buffer.SampleDesc.Count = 1;
    buffer.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    void* mapped{};
    if (SUCCEEDED(queue->GetTimestampFrequency(&frequency)) && frequency &&
        SUCCEEDED(device->CreateQueryHeap(&qd, IID_PPV_ARGS(&aa.queries))) &&
        SUCCEEDED(device->CreateCommittedResource(&readback, D3D12_HEAP_FLAG_NONE, &buffer,
                                                  D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
                                                  IID_PPV_ARGS(&aa.stamps))) &&
        SUCCEEDED(aa.stamps->Map(0, nullptr, &mapped))) {
        aa.stampData = static_cast<const volatile uint64_t*>(mapped);
        aa.ticksPerMs = static_cast<double>(frequency) / 1000.0;
    } else {
        aa.queries.Reset();
        aa.stamps.Reset();
    }
    aa.ready = true;
    return true;
}
// The measured pass that last used this slot, now that its fence passed.
void noteAaTime(size_t slot) {
    if (!aa.timed[slot] || !aa.stampData)
        return;
    aa.timed[slot] = false;
    const uint64_t begin = aa.stampData[2 * slot], end = aa.stampData[2 * slot + 1];
    if (end > begin) {
        std::lock_guard lock(copyTimingLock);
        aaGpu.add(static_cast<double>(end - begin) / aa.ticksPerMs);
    }
    if (end > begin)
        aaClock.note(static_cast<double>(end - begin) / aa.ticksPerMs);
}
// Both eyes through FXAA, or the supersampled eyes through the downsample
// filter, into stagedEyes, which enter and leave COPY_SOURCE;
// the eye images return to the states the game left them in.
void drawAa(ID3D12GraphicsCommandList* list, size_t slot) {
    const auto first = static_cast<UINT>(2 * slot);
    if (aa.queries)
        originalEndQuery(list, aa.queries.Get(), D3D12_QUERY_TYPE_TIMESTAMP, first);
    D3D12_RESOURCE_BARRIER b[4]{};
    UINT count = 0;
    for (unsigned i = 0; i < 2; ++i) {
        if (!(states[i] & D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)) {
            b[count].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            b[count++].Transition = {resources[i].Get(), D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, states[i],
                                     D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE};
        }
        b[count].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        b[count++].Transition = {stagedEyes[i].Get(), D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                                 D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET};
    }
    originalBarrier(list, count, b);
    // FXAA: one over the eye's size, and its settings. The downsample: the
    // eye's pixels per headset pixel across and down, and the filter's reach.
    const bool down = downsampling();
    const float frame[4] = {down ? static_cast<float>(width) / static_cast<float>(outWidth)
                                 : 1.f / static_cast<float>(width),
                            down ? static_cast<float>(height) / static_cast<float>(outHeight)
                                 : 1.f / static_cast<float>(height),
                            down ? downsampleRadius : aaQuality.load() ? aaStrongSubpix : aaSubpix,
                            down ? 0.f : aaQuality.load() ? aaStrongEdgeThreshold : aaEdgeThreshold};
    list->SetPipelineState(down ? aa.down.Get() : aa.state.Get());
    list->SetGraphicsRootSignature(aa.root.Get());
    ID3D12DescriptorHeap* heaps[] = {aa.views.Get()};
    list->SetDescriptorHeaps(1, heaps);
    list->SetGraphicsRoot32BitConstants(1, 4, frame, 0);
    const D3D12_VIEWPORT viewport{0, 0, static_cast<float>(outWidth), static_cast<float>(outHeight), 0, 1};
    const D3D12_RECT rect{0, 0, static_cast<LONG>(outWidth), static_cast<LONG>(outHeight)};
    list->RSSetViewports(1, &viewport);
    list->RSSetScissorRects(1, &rect);
    list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    const auto viewStep = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    const auto targetStep = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    for (unsigned i = 0; i < 2; ++i) {
        auto view = aa.views->GetGPUDescriptorHandleForHeapStart();
        view.ptr += static_cast<UINT64>(viewStep) * i;
        auto target = aa.targets->GetCPUDescriptorHandleForHeapStart();
        target.ptr += static_cast<SIZE_T>(targetStep) * i;
        list->SetGraphicsRootDescriptorTable(0, view);
        list->OMSetRenderTargets(1, &target, FALSE, nullptr);
        list->DrawInstanced(3, 1, 0, 0);
    }
    for (UINT n = 0; n < count; ++n)
        std::swap(b[n].Transition.StateBefore, b[n].Transition.StateAfter);
    originalBarrier(list, count, b);
    if (aa.queries) {
        originalEndQuery(list, aa.queries.Get(), D3D12_QUERY_TYPE_TIMESTAMP, first + 1);
        list->ResolveQueryData(aa.queries.Get(), D3D12_QUERY_TYPE_TIMESTAMP, first, 2, aa.stamps.Get(),
                               sizeof(uint64_t) * first);
        aa.timed[slot] = true;
    }
}
// Update 16: eye sharpening (native_eye_gpu.hpp, setEyeSharpen;
// eye_sharpen.hpp). For the headset only: instead of copying stagedEyes into
// the headset's images (copyLatest), the pass draws them there sharpened, so
// it adds no copy of its own, and the web/HUD overlay is drawn over it as
// before. Built on the first headset frame that wants it, under the
// submission and recording locks. If anything is missing (the compile, the
// pipeline, a headset image that is not a render target) it stops for the
// session (sharpenFault) and copyLatest copies as before: the eyes never go
// without a picture.
std::atomic<uint32_t> sharpenPercent{}, sharpenFault{};
// Update 38: the percent asked for, kept: one set before start() (the launcher's start-up
// sharpen 140) was lost (staged still false) and the session ran unsharpened.
std::atomic<uint32_t> sharpenWanted{};
std::atomic<bool> sharpening{}; // the last headset frame was drawn sharpened
// Update 24: live.txt eye_debug (eye_debug.hpp): 1 both headset images from the left eye's picture.
std::atomic<uint32_t> eyeDebugMode{};
// Update 26: the mode the newest pair copied to the headset was made with.
std::atomic<uint32_t> pairDebugMode{};
struct SharpenPipeline {
    ComPtr<ID3D12RootSignature> root;
    ComPtr<ID3D12PipelineState> state, srgb; // to UNORM targets; to UNORM_SRGB ones
    ComPtr<ID3D12PipelineState> toon, toonSrgb; // Update 38.5: 90s ANIMATED (none: toonFault)
    ComPtr<ID3D12DescriptorHeap> views, targets;
    ComPtr<ID3D12QueryHeap> queries;
    ComPtr<ID3D12Resource> stamps;
    const volatile uint64_t* stampData{};
    double ticksPerMs{};
    std::array<bool, 3> timed{}; // a presentCommands slot holds a measured pass
    bool ready{};
} sharp;
TimingMetric sharpenGpu; // copyTimingLock
std::atomic<uint64_t> sharpenPairs{};
// CAS (eye_sharpen.hpp sharpenChannel, the same arithmetic): the eyes hold
// display-encoded values and are sharpened as such, like FXAA. To a typed
// sRGB target (whose writes are encoded by the GPU) the result goes out as
// light (linearOut), so the stored bytes are the same as a UNORM target's.
// No vector ?: (dxc's HLSL 2021 rejects it; checked with dxc on the box).
const char* sharpenShader = R"(
Texture2D source : register(t0);
cbuffer Frame : register(b0) {
    float peak; float linearOut; float lift; float colorOn;
    float gain; float contrast; float crush; float saturation; // Update 37.2: COLOR (eye_color.hpp)
    float vibrance; float mono; float vignette; float unused;
    float4 balance; float4 shadowTone; float4 highTone;
    float4 fovea; float4 foveaView; // Update 37.2: FOVEATED DEBUG (centre u, v, inner, outer; on, aspect)
};
float4 vs(uint id : SV_VertexID) : SV_POSITION {
    const float2 corner = float2((id << 1) & 2, id & 2);
    return float4(corner * float2(2, -2) + float2(-1, 1), 0, 1);
}
float3 toLinear(float3 c) {
    return lerp(pow((c + 0.055) / 1.055, 2.4), c / 12.92, step(c, 0.04045));
}
// Update 37.2: eye_color.hpp apply(), the same steps in the same order, display-encoded.
float3 toDisplay(float3 l) {
    return lerp(1.055 * pow(max(l, 0.0), 1.0 / 2.4) - 0.055, l * 12.92, step(l, 0.0031308));
}
float3 shoulder(float3 x) {
    return lerp(x, 0.8 + 0.2 * (1.0 - exp(-max(x - 0.8, 0.0) / 0.2)), step(0.8, x));
}
float3 grade(float3 c, float2 uv) {
    c = saturate(c);
    if (colorOn < 0.5)
        return c;
    if (gain != 1.0)
        c = shoulder(toDisplay(toLinear(c) * gain));
    c = saturate(((c - 0.5) * contrast + 0.5 - crush) / (1.0 - crush));
    const float3 lw = float3(0.2126, 0.7152, 0.0722);
    float y = dot(c, lw);
    const float mx = max(c.r, max(c.g, c.b)), mn = min(c.r, min(c.g, c.b));
    const float skin = saturate((c.r - c.g) * 4.0) * saturate((c.g - c.b) * 4.0);
    const float boost = saturation * (1.0 + vibrance * (1.0 - (mx - mn)) * (1.0 - 0.7 * skin));
    c = saturate(y + (c - y) * boost);
    c = saturate(c * balance.rgb);
    y = dot(c, lw);
    c = saturate(c + shadowTone.rgb * (1.0 - y) * (1.0 - y) + highTone.rgb * y * y);
    y = dot(c, lw);
    c = c + (y - c) * mono;
    const float2 d = uv - 0.5;
    const float d2 = dot(d, d) * 2.0;
    return saturate(c * saturate(1.0 - vignette * 0.7 * d2));
}
float4 ps(float4 position : SV_POSITION) : SV_TARGET {
    uint w, h;
    source.GetDimensions(w, h);
    const int2 p = int2(position.xy);
    const int2 hi = int2(w, h) - 1;
    const float3 a = source.Load(int3(clamp(p + int2(-1, -1), 0, hi), 0)).rgb;
    const float3 b = source.Load(int3(clamp(p + int2(0, -1), 0, hi), 0)).rgb;
    const float3 c = source.Load(int3(clamp(p + int2(1, -1), 0, hi), 0)).rgb;
    const float3 d = source.Load(int3(clamp(p + int2(-1, 0), 0, hi), 0)).rgb;
    const float4 centre = source.Load(int3(clamp(p, 0, hi), 0));
    const float3 e = centre.rgb;
    const float3 f = source.Load(int3(clamp(p + int2(1, 0), 0, hi), 0)).rgb;
    const float3 g = source.Load(int3(clamp(p + int2(-1, 1), 0, hi), 0)).rgb;
    const float3 hh = source.Load(int3(clamp(p + int2(0, 1), 0, hi), 0)).rgb;
    const float3 i = source.Load(int3(clamp(p + int2(1, 1), 0, hi), 0)).rgb;
    float3 mn = min(min(min(d, e), min(f, b)), hh);
    const float3 mn2 = min(min(mn, a), min(min(c, g), i));
    mn += mn2;
    float3 mx = max(max(max(d, e), max(f, b)), hh);
    const float3 mx2 = max(max(mx, a), max(max(c, g), i));
    mx += mx2;
    const float3 guard = sqrt(saturate(min(mn, 2.0 - mx) / max(mx, 1e-5)));
    const float3 amp = guard + (1.0 - guard) * saturate(lift); // Update 21: 101-200%
    const float3 wt = amp * peak;
    const float3 s = saturate((b * wt + d * wt + f * wt + hh * wt + e) / (1.0 + 4.0 * wt));
    const float2 uv = position.xy / float2(w, h);
    float3 o = grade(s, uv); // Update 37.2: COLOR, both eyes alike
    if (foveaView.x > 0.5) { // FOVEATED DEBUG: green full detail, yellow half, red quarter
        const float2 d = (uv - fovea.xy) * float2(2.0 * foveaView.y, 2.0);
        const float r = length(d);
        const float3 zone = lerp(lerp(float3(1, 0, 0), float3(1, 1, 0), step(r, fovea.w)), float3(0, 1, 0),
                                 step(r, fovea.z));
        o = lerp(o, zone, 0.3);
    }
    return float4(lerp(o, toLinear(o), linearOut), centre.a);
}
)";
// Update 38.5: 90s ANIMATED (eye_color::cartoonPixel, the same steps in the same order). Its own
// entry point, compiled after sharpenShader's text into its own pipeline: if it does not compile
// (or its pipeline cannot be made) only 90s ANIMATED turns off, with a notice (toonFault); the
// sharpening and the other looks carry on. Constants in sharpenShader's free slots: unused =
// paint (CARTOON STRENGTH 0-1), balance.w = bands, shadowTone.w = ink (0: OUTLINES OFF),
// highTone.w = stride (whole pixels: the same in both eyes and every frame). No CAS here (the
// painting replaces it). 25 Loads a pixel.
const char* toonShader = R"(
float sstep(float a, float b, float x) {
    const float t = saturate((x - a) / (b - a));
    return t * t * (3.0 - 2.0 * t);
}
// Version 0.0.6 DAREDEVIL SONAR (eye_color::sonarPixel, the same steps in the same order): toon's
// sonar mode (highTone.w above 3.5), its phase in seconds in foveaView.z.
float sonarRings(float r, float phase) {
    float ring = 0.0;
    [unroll] for (int k = 0; k < 3; ++k) {
        const float t = frac(phase * 0.5 + float(k) / 3.0);
        const float d = (r - t * 0.75) / 0.008;
        ring += (1.0 - t) * exp(-d * d);
    }
    return ring;
}
// Version 0.0.62 BEACONS IN VISION (eye_color::beaconMask / beaconPulse, the same steps).
float beaconMask(float3 c) {
    const float mx = max(c.r, max(c.g, c.b));
    const float mn = min(c.r, min(c.g, c.b));
    return sstep(0.55, 0.85, mx) * sstep(0.45, 0.7, (mx - mn) / max(mx, 0.001));
}
float4 sonar(int2 p, int2 hi, uint w, uint h, bool beacons) {
    const float3 lw = float3(0.299, 0.587, 0.114);
    float l[9];
    float3 tap[9];
    [unroll] for (int j = 0; j < 3; ++j)
        [unroll] for (int i = 0; i < 3; ++i) {
            tap[j * 3 + i] = source.Load(int3(clamp(p + int2(i - 1, j - 1) * 2, int2(0, 0), hi), 0)).rgb;
            l[j * 3 + i] = dot(tap[j * 3 + i], lw);
        }
    const float alpha = source.Load(int3(clamp(p, int2(0, 0), hi), 0)).a;
    const float gx = (l[2] + 2.0 * l[5] + l[8]) - (l[0] + 2.0 * l[3] + l[6]);
    const float gy = (l[6] + 2.0 * l[7] + l[8]) - (l[0] + 2.0 * l[1] + l[2]);
    const float e = pow(saturate(sqrt(gx * gx + gy * gy) * 0.8), 0.8);
    const float2 uv = (float2(p) + 0.5) / float2(max(w, 1u), max(h, 1u));
    const float aspect = float(max(h, 1u)) / float(max(w, 1u));
    const float2 d = float2(uv.x - 0.5, (uv.y - 0.92) * 1.6 * aspect);
    const float r = length(d);
    const float ring = sonarRings(r, foveaView.z);
    const float nearK = exp(-r / 0.55);
    const float edge = e * (0.35 + 0.65 * nearK) + e * ring * 1.5;
    const float base = l[4] * 0.06;
    float3 col = saturate(float3(base + edge + ring * 0.25, base * 0.4 + edge * 0.38 + ring * 0.08,
                                 base * 0.3 + edge * 0.08));
    if (beacons) {
        const float pulse = 0.9 + 0.1 * sin(foveaView.z * 3.14159265);
        const float m0 = beaconMask(tap[4]);
        float halo = 0.0;
        float3 haloCol = tap[4];
        [unroll] for (int k = 0; k < 9; ++k) {
            if (k != 4) {
                const float m = beaconMask(tap[k]);
                if (m > halo) {
                    halo = m;
                    haloCol = tap[k];
                }
            }
        }
        const float glow = halo * (1.0 - m0) * 0.6 * pulse;
        col = lerp(col, saturate(tap[4] * 1.15 * pulse), m0);
        col = max(col, saturate(haloCol * glow));
    }
    return float4(lerp(col, toLinear(col), linearOut), alpha);
}
float4 toon(float4 position : SV_POSITION) : SV_TARGET {
    uint w, h;
    source.GetDimensions(w, h);
    const int2 p = int2(position.xy);
    const int2 hi = int2(w, h) - 1;
    if (highTone.w > 3.5)
        return sonar(p, hi, w, h, highTone.w > 7.5); // 0.0.62 BEACONS IN VISION: +8
    const int stride = clamp(int(highTone.w + 0.5), 1, 2);
    const float paint = saturate(unused);
    const float bands = clamp(balance.w, 3.0, 5.0);
    const float inkK = saturate(shadowTone.w);
    const float3 lw = float3(0.2126, 0.7152, 0.0722);
    float3 g[25];
    [unroll] for (int j = 0; j < 5; ++j)
        [unroll] for (int i = 0; i < 5; ++i)
            g[j * 5 + i] = source.Load(int3(clamp(p + int2(i - 2, j - 2) * stride, int2(0, 0), hi), 0)).rgb;
    const float alpha = source.Load(int3(clamp(p, int2(0, 0), hi), 0)).a;
    float3 m[4];
    float wsum = 0.0;
    float3 painted = float3(0, 0, 0);
    [unroll] for (int q = 0; q < 4; ++q) {
        const int i0 = (q & 1) * 2, j0 = (q >> 1) * 2;
        float3 sum = float3(0, 0, 0);
        float s = 0.0, s2 = 0.0;
        [unroll] for (int jj = 0; jj < 3; ++jj)
            [unroll] for (int ii = 0; ii < 3; ++ii) {
                const float3 k = g[(j0 + jj) * 5 + i0 + ii];
                sum += k;
                const float l = dot(k, lw);
                s += l;
                s2 += l * l;
            }
        m[q] = sum / 9.0;
        const float v = max(s2 / 9.0 - (s / 9.0) * (s / 9.0), 0.0) * 400.0 + 1e-3;
        const float wq = 1.0 / (v * v);
        wsum += wq;
        painted += m[q] * wq;
    }
    float3 col = saturate(g[12] + (painted / wsum - g[12]) * paint);
    const float2 uv = position.xy / float2(w, h);
    col = grade(col, uv);
    float yl = dot(col, lw);
    const float dark = 0.65 * sstep(0.45, 0.08, yl);
    col = saturate(col * (1.0 + (float3(0.55, 0.72, 1.35) - 1.0) * dark));
    yl = dot(col, lw);
    const float teal = saturate((col.b - col.r) * 3.0) * saturate((yl - 0.45) * 3.0);
    col.g = saturate(col.g + 0.10 * teal);
    yl = dot(col, lw);
    const float f = yl * bands;
    const float base = floor(f), fr = f - base;
    const float hard = 0.30 - 0.18 * paint;
    const float banded = (base + sstep(0.5 - hard, 0.5 + hard, fr)) / bands;
    const float target = yl + (min(banded, 1.0) - yl) * (0.55 + 0.4 * paint);
    col = saturate(col * (target / max(yl, 1e-3)));
    if (inkK > 0.0) {
        const float l0 = dot(m[0], lw), l1 = dot(m[1], lw), l2 = dot(m[2], lw), l3 = dot(m[3], lw);
        const float gx = (l1 + l3) - (l0 + l2);
        const float gy = (l2 + l3) - (l0 + l1);
        const float3 cx = abs((m[1] + m[3]) - (m[0] + m[2]));
        const float3 cy = abs((m[2] + m[3]) - (m[0] + m[1]));
        const float chroma = max(max(max(cx.r, cx.g), cx.b), max(max(cy.r, cy.g), cy.b));
        const float e = sqrt(gx * gx + gy * gy) + 0.35 * chroma;
        const float lo = 0.16 - 0.05 * paint, hi2 = lo + 0.10;
        const float ink = sstep(lo, hi2, e) * inkK * (0.75 + 0.25 * sstep(hi2, hi2 + 0.3, e));
        col = col + (float3(0.05, 0.04, 0.10) - col) * ink;
    }
    const float pp = 0.025 * paint;
    col = saturate(pp + col * (1.0 - pp) * (1.0 - 0.06 * paint + 0.06 * paint * float3(1.0, 0.97, 0.90)));
    return float4(lerp(col, toLinear(col), linearOut), alpha);
}
)";
// Update 37.2: COLOR / ATMOSPHERE (eye_color.hpp), drawn by this pass. colorLock guards both.
std::mutex colorLock;
eye_color::Config colorConfig;
eye_color::Params colorParams;
std::atomic<bool> colorOn{};
std::atomic<uint64_t> colorPairs{};
// Update 38.5: 90s ANIMATED. toonFault: 0 fine, 1 its shader did not compile, 2 its pipeline could
// not be made (then only 90s ANIMATED is off; the pass and the other looks carry on).
eye_color::Toon colorToon; // colorLock
std::atomic<bool> toonOn{};
std::atomic<uint32_t> toonFault{};
bool failSharpen(uint32_t code) {
    sharpenFault = code;
    colorOn = false; // Update 37.2: no grade without the pass (the copy as before)
    sharpenPercent = 0; // the copy from now on; the slider shows UNAVAILABLE
    sharp.ready = false;
    sharpening = false;
    return false;
}
// The pass, for stagedEyes into whichever headset images copyLatest is given
// (their target views are written each frame). Submission and recording held.
bool prepareSharpen() {
    if (sharpenFault)
        return false;
    if (sharp.ready)
        return true;
    for (unsigned i = 0; i < 2; ++i)
        if (!stagedEyes[i] || (stagedEyes[i]->GetDesc().Flags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE))
            return failSharpen(5);
    D3D12_DESCRIPTOR_RANGE range{D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, 0};
    D3D12_ROOT_PARAMETER parameters[2]{};
    parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    parameters[0].DescriptorTable = {1, &range};
    parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    parameters[1].Constants.Num32BitValues = 32; // Update 37.2: 4 sharpening + 20 COLOR + 8 FOVEATED DEBUG
    parameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_ROOT_SIGNATURE_DESC rs{2, parameters, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_NONE};
    ComPtr<ID3DBlob> signature, error;
    if (FAILED(D3D12SerializeRootSignature(&rs, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error)) ||
        FAILED(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(),
                                           IID_PPV_ARGS(&sharp.root))))
        return failSharpen(1);
    ComPtr<ID3DBlob> vs, ps;
    const char* shader = sharpenShader;
    if (FAILED(D3DCompile(shader, std::strlen(shader), "spidy_eye_sharpen", nullptr, nullptr, "vs", "vs_5_0",
                          D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &vs, &error)) ||
        FAILED(D3DCompile(shader, std::strlen(shader), "spidy_eye_sharpen", nullptr, nullptr, "ps", "ps_5_0",
                          D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &ps, &error)))
        return failSharpen(2);
    D3D12_GRAPHICS_PIPELINE_STATE_DESC d{};
    d.pRootSignature = sharp.root.Get();
    d.VS = {vs->GetBufferPointer(), vs->GetBufferSize()};
    d.PS = {ps->GetBufferPointer(), ps->GetBufferSize()};
    d.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    d.NumRenderTargets = 1;
    d.SampleDesc.Count = 1;
    d.SampleMask = UINT_MAX;
    d.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    d.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    d.RasterizerState.DepthClipEnable = TRUE;
    auto& blend = d.BlendState.RenderTarget[0];
    blend.SrcBlend = blend.SrcBlendAlpha = D3D12_BLEND_ONE;
    blend.DestBlend = blend.DestBlendAlpha = D3D12_BLEND_ZERO;
    blend.BlendOp = blend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    blend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    d.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    if (FAILED(device->CreateGraphicsPipelineState(&d, IID_PPV_ARGS(&sharp.state))))
        return failSharpen(3);
    d.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    if (FAILED(device->CreateGraphicsPipelineState(&d, IID_PPV_ARGS(&sharp.srgb))))
        return failSharpen(3);
    {   // Update 38.5: 90s ANIMATED, its own entry point and pipeline. A failure here turns only it off.
        const std::string text = std::string(sharpenShader) + toonShader;
        ComPtr<ID3DBlob> tps, terror;
        if (FAILED(D3DCompile(text.data(), text.size(), "spidy_eye_toon", nullptr, nullptr, "toon", "ps_5_0",
                              D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &tps, &terror))) {
            toonFault = 1;
        } else {
            auto t = d;
            t.PS = {tps->GetBufferPointer(), tps->GetBufferSize()};
            t.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
            const bool unorm = SUCCEEDED(device->CreateGraphicsPipelineState(&t, IID_PPV_ARGS(&sharp.toon)));
            t.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
            if (!unorm || FAILED(device->CreateGraphicsPipelineState(&t, IID_PPV_ARGS(&sharp.toonSrgb)))) {
                sharp.toon.Reset();
                sharp.toonSrgb.Reset();
                toonFault = 2;
            }
        }
        if (toonFault.load())
            toonOn = false;
    }
    D3D12_DESCRIPTOR_HEAP_DESC views{};
    views.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    views.NumDescriptors = 2;
    views.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    D3D12_DESCRIPTOR_HEAP_DESC targetViews{};
    targetViews.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    targetViews.NumDescriptors = 2;
    if (FAILED(device->CreateDescriptorHeap(&views, IID_PPV_ARGS(&sharp.views))) ||
        FAILED(device->CreateDescriptorHeap(&targetViews, IID_PPV_ARGS(&sharp.targets))))
        return failSharpen(4);
    const auto viewStep = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    for (unsigned i = 0; i < 2; ++i) {
        D3D12_SHADER_RESOURCE_VIEW_DESC sv{};
        sv.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        sv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        sv.Texture2D.MipLevels = 1;
        auto view = sharp.views->GetCPUDescriptorHandleForHeapStart();
        view.ptr += static_cast<SIZE_T>(viewStep) * i;
        device->CreateShaderResourceView(stagedEyes[i].Get(), &sv, view);
    }
    // The GPU time is optional, as the AA pass's.
    UINT64 frequency{};
    D3D12_QUERY_HEAP_DESC qd{};
    qd.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
    qd.Count = 2 * static_cast<UINT>(presentCommands.size());
    D3D12_HEAP_PROPERTIES readback{};
    readback.Type = D3D12_HEAP_TYPE_READBACK;
    D3D12_RESOURCE_DESC buffer{};
    buffer.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    buffer.Width = sizeof(uint64_t) * qd.Count;
    buffer.Height = 1;
    buffer.DepthOrArraySize = 1;
    buffer.MipLevels = 1;
    buffer.SampleDesc.Count = 1;
    buffer.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    void* mapped{};
    if (SUCCEEDED(queue->GetTimestampFrequency(&frequency)) && frequency &&
        SUCCEEDED(device->CreateQueryHeap(&qd, IID_PPV_ARGS(&sharp.queries))) &&
        SUCCEEDED(device->CreateCommittedResource(&readback, D3D12_HEAP_FLAG_NONE, &buffer,
                                                  D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
                                                  IID_PPV_ARGS(&sharp.stamps))) &&
        SUCCEEDED(sharp.stamps->Map(0, nullptr, &mapped))) {
        sharp.stampData = static_cast<const volatile uint64_t*>(mapped);
        sharp.ticksPerMs = static_cast<double>(frequency) / 1000.0;
    } else {
        sharp.queries.Reset();
        sharp.stamps.Reset();
    }
    sharp.ready = true;
    return true;
}
void noteSharpenTime(size_t slot) {
    if (slot >= sharp.timed.size() || !sharp.timed[slot] || !sharp.stampData)
        return;
    sharp.timed[slot] = false;
    const uint64_t begin = sharp.stampData[2 * slot], end = sharp.stampData[2 * slot + 1];
    if (end > begin) {
        std::lock_guard lock(copyTimingLock);
        sharpenGpu.add(static_cast<double>(end - begin) / sharp.ticksPerMs);
    }
    if (end > begin)
        sharpenClock.note(static_cast<double>(end - begin) / sharp.ticksPerMs);
}
// The target view for a headset image: its own UNORM or UNORM_SRGB, the
// UNORM one for a typeless image; UNKNOWN when it cannot be drawn to.
DXGI_FORMAT sharpenTarget(const D3D12_RESOURCE_DESC& desc) {
    if (!(desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) || desc.MipLevels > 1 ||
        desc.DepthOrArraySize != 1 || desc.SampleDesc.Count != 1)
        return DXGI_FORMAT_UNKNOWN;
    switch (desc.Format) {
    case DXGI_FORMAT_R8G8B8A8_TYPELESS:
    case DXGI_FORMAT_R8G8B8A8_UNORM:
        return DXGI_FORMAT_R8G8B8A8_UNORM;
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
        return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    default:
        return DXGI_FORMAT_UNKNOWN;
    }
}
// Update 37.2: FOVEATED DEBUG's constants for one eye: its zones over the picture (all zero: off).
std::array<float, 8> foveaTint(unsigned eye) {
    std::array<float, 8> t{};
    if (!foveaDebug.load() || foveaLevel.load() == foveation::off || !width || !height)
        return t;
    const auto c = foveaCentre(eye);
    const auto z = foveation::zones(foveaLevel.load(), foveaInner.load());
    t = {c.u, c.v, z.inner, std::isinf(z.outer) ? 1e9f : z.outer, 1, static_cast<float>(width) / static_cast<float>(height),
         0, 0};
    return t;
}
// Both staged eyes, sharpened, into the headset's images (RENDER_TARGET, as
// the copy leaves them). stagedEyes leave and re-enter COPY_SOURCE. False
// (nothing recorded) when an image cannot be drawn to: then the copy.
bool drawSharpen(ID3D12GraphicsCommandList* list, size_t slot, ID3D12Resource* const* incoming, uint32_t percent,
                 uint32_t debug) {
    DXGI_FORMAT formats[2]{};
    for (unsigned i = 0; i < 2; ++i)
        if ((formats[i] = sharpenTarget(incoming[i]->GetDesc())) == DXGI_FORMAT_UNKNOWN)
            return failSharpen(6);
    if (formats[0] != formats[1])
        return failSharpen(6);
    const auto first = static_cast<UINT>(2 * slot);
    if (sharp.queries)
        originalEndQuery(list, sharp.queries.Get(), D3D12_QUERY_TYPE_TIMESTAMP, first);
    D3D12_RESOURCE_BARRIER b[2]{};
    for (unsigned i = 0; i < 2; ++i) {
        b[i].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        b[i].Transition = {stagedEyes[i].Get(), D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                           D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE};
    }
    originalBarrier(list, 2, b);
    const bool srgb = formats[0] == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    // Update 37.2: percent 0 with COLOR on: no sharpening (peak 0 leaves each pixel as it is).
    eye_color::Params cp;
    eye_color::Toon tp;
    if (colorOn.load()) {
        std::lock_guard lock(colorLock);
        cp = colorParams;
        tp = colorToon;
    }
    // Update 38.5: 90s ANIMATED in the free slots (11, 15, 19, 23), its own pipeline when built.
    const bool cel = tp.on > .5f && toonOn.load() && sharp.toon && sharp.toonSrgb;
    const float frame[24] = {percent ? eye_sharpen::peak(eye_sharpen::sharpness(percent)) : 0.f,
                             srgb ? 1.f : 0.f,
                             eye_sharpen::boost(percent),
                             cp.on,
                             cp.gain, cp.contrast, cp.crush, cp.saturation,
                             cp.vibrance, cp.mono, cp.vignette, tp.paint,
                             cp.balance[0], cp.balance[1], cp.balance[2], tp.bands,
                             cp.shadow[0], cp.shadow[1], cp.shadow[2], tp.ink,
                             cp.high[0], cp.high[1], cp.high[2],
                             static_cast<float>(tp.stride) + (tp.sonar > 1.5f ? 8.f : tp.sonar > .5f ? 4.f : 0.f)}; // 0.0.6 SONAR: +4; 0.0.62 BEACONS: +8
    // Version 0.0.6 DAREDEVIL SONAR: the pulse rings' clock (seconds, wrapped) in foveaView.z.
    const float sonarPhase =
        tp.sonar > .5f ? static_cast<float>(std::fmod(
                             std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(), 600.0))
                       : 0.f;
    if (cel)
        list->SetPipelineState(srgb ? sharp.toonSrgb.Get() : sharp.toon.Get());
    else
        list->SetPipelineState(srgb ? sharp.srgb.Get() : sharp.state.Get());
    list->SetGraphicsRootSignature(sharp.root.Get());
    ID3D12DescriptorHeap* heaps[] = {sharp.views.Get()};
    list->SetDescriptorHeaps(1, heaps);
    list->SetGraphicsRoot32BitConstants(1, 24, frame, 0);
    const D3D12_VIEWPORT viewport{0, 0, static_cast<float>(outWidth), static_cast<float>(outHeight), 0, 1};
    const D3D12_RECT rect{0, 0, static_cast<LONG>(outWidth), static_cast<LONG>(outHeight)};
    list->RSSetViewports(1, &viewport);
    list->RSSetScissorRects(1, &rect);
    list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    const auto viewStep = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    const auto targetStep = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    for (unsigned i = 0; i < 2; ++i) {
        // Render target views are read when recorded: rewriting them next
        // frame, for that frame's images, is safe.
        D3D12_RENDER_TARGET_VIEW_DESC rv{};
        rv.Format = formats[i];
        rv.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        auto target = sharp.targets->GetCPUDescriptorHandleForHeapStart();
        target.ptr += static_cast<SIZE_T>(targetStep) * i;
        device->CreateRenderTargetView(incoming[i], &rv, target);
        auto view = sharp.views->GetGPUDescriptorHandleForHeapStart();
        // Update 24: eye_debug same reads the left eye's picture for both.
        view.ptr += static_cast<UINT64>(viewStep) * eye_debug::eyeDebugSource(debug, i);
        list->SetGraphicsRootDescriptorTable(0, view);
        auto tint = foveaTint(i);
        tint[6] = sonarPhase; // Version 0.0.6 DAREDEVIL SONAR
        list->SetGraphicsRoot32BitConstants(1, 8, tint.data(), 24); // Update 37.2
        list->OMSetRenderTargets(1, &target, FALSE, nullptr);
        list->DrawInstanced(3, 1, 0, 0);
    }
    for (auto& barrier : b)
        std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);
    originalBarrier(list, 2, b);
    if (sharp.queries) {
        originalEndQuery(list, sharp.queries.Get(), D3D12_QUERY_TYPE_TIMESTAMP, first + 1);
        list->ResolveQueryData(sharp.queries.Get(), D3D12_QUERY_TYPE_TIMESTAMP, first, 2, sharp.stamps.Get(),
                               sizeof(uint64_t) * first);
        sharp.timed[slot] = true;
    }
    return true;
}
// Eye history (native_eye_gpu.hpp, setEyeHistory; eye_history.hpp): each new
// staged pair blended with the one before it, turned to the new pair's view.
// Built on the first pair that wants it, under the submission and recording
// locks, like the AA pass.
std::atomic<uint32_t> historyPercent{}, historyFault{};
// Update 33: EYE AA (eye_taa.hpp) on: the history runs at least eye_taa::historyPercent,
// the clamp as tight as the slider's own share leaves it.
std::atomic<bool> temporalOn{};
std::atomic<uint32_t> trailPercent{spidy::eye_history::defaultTrail}; // Update 34.2: EYE AA TRAIL
std::atomic<uint32_t> cutPercent{spidy::eye_history::defaultCut};     // Update 37: HISTORY CUT
// 0.0.73 test 3 LIGHT HOLD (eye_history.hpp): the shader's bright hold, ON by default (0 = test 2), and
// the history share the last pass kept (thousandths, eye 0; 0 when it kept none) for JITTER CALM.
std::atomic<uint32_t> lightHoldPercent{spidy::eye_history::lightHoldOn};
std::atomic<uint32_t> keptMilli{};
uint32_t historyNow() {
    if (sm1Lock.load(std::memory_order_relaxed))
        return 0; // 0.0.74 test 10 SM1STYLE
    return eye_taa::historyInForce(historyPercent.load(), temporalOn.load());
}
std::mutex viewLock;
struct NotedViews {
    uint64_t serial{};
    std::array<eye_history::EyeView, 2> eyes{};
};
std::array<NotedViews, 32> notedViews{}; // viewLock
unsigned nextNoted{};                    // viewLock
struct HistoryPipeline {
    ComPtr<ID3D12RootSignature> root;
    ComPtr<ID3D12PipelineState> state;
    ComPtr<ID3D12DescriptorHeap> views, targets;
    ComPtr<ID3D12QueryHeap> queries;
    ComPtr<ID3D12Resource> stamps;
    const volatile uint64_t* stampData{};
    double ticksPerMs{};
    std::array<bool, 3> timed{};
    // Per eye, the picture so far and the one being drawn (parity is read).
    std::array<std::array<ComPtr<ID3D12Resource>, 2>, 2> pictures;
    unsigned parity{};
    bool ready{}, valid{}; // valid: pictures[*][parity] hold `shown`
    std::array<eye_history::EyeView, 2> shown{};
} history;
const char* historyShader = R"(
Texture2D current : register(t0);
Texture2D previous : register(t1);
SamplerState bilinear : register(s0);
cbuffer Frame : register(b0) { float4 row0; float4 row1; float4 row2; float4 extra; };
float4 vs(uint id : SV_VertexID) : SV_POSITION {
    const float2 corner = float2((id << 1) & 2, id & 2);
    return float4(corner * float2(2, -2) + float2(-1, 1), 0, 1);
}
float4 ps(float4 position : SV_POSITION) : SV_TARGET {
    uint w, h;
    current.GetDimensions(w, h);
    const int2 p = int2(position.xy);
    const int2 last = int2(w, h) - 1;
    const float4 here = current.Load(int3(p, 0));
    float3 lo = here.rgb, hi = here.rgb;
    [unroll] for (int y = -1; y <= 1; ++y)
        [unroll] for (int x = -1; x <= 1; ++x) {
            const float3 n = current.Load(int3(clamp(p + int2(x, y), int2(0, 0), last), 0)).rgb;
            lo = min(lo, n);
            hi = max(hi, n);
        }
    const float3 uv = float3(position.xy * extra.yz, 1);
    const float3 q = float3(dot(row0.xyz, uv), dot(row1.xyz, uv), dot(row2.xyz, uv));
    float keep = extra.x;
    float2 old = float2(-1, -1);
    if (q.z > 1e-5)
        old = q.xy / q.z;
    if (keep <= 0 || any(old < 0) || any(old > 1))
        return here;
    // Update 21: extra.w widens the clamp (eye_history.hpp historyLoosen).
    const float3 give = (hi - lo) * saturate(extra.w);
    const float3 raw = previous.SampleLevel(bilinear, old, 0).rgb;
    // 0.0.73 test 3 LIGHT HOLD (eye_history.hpp heldPixel, row0.w the hold, 0 = test 2's sum): a bright
    // old pixel (a street light this frame's sub-pixel step missed) may stay above the 3x3 by part of
    // its excess, its anti-ghost cut eased, and the blend weighted 1 / (1 + luma).
    const float bright = saturate((max(raw.r, max(raw.g, raw.b)) - 0.35) * 2.0) * saturate(row0.w);
    const float3 lift = max(raw - (hi + give), 0) * (bright * 0.8);
    const float3 before = clamp(raw, max(lo - give, 0), hi + give + lift);
    // Version 0.0.62 ANTI-GHOST: the history follows the head only, so a hand (or anything that
    // moved on its own) leaves its old picture behind - the "ghost hand copies". History far outside
    // this pixel's own 3x3 colours is such a thing, not edge shimmer: keep less of it (none at
    // twice the neighbourhood's spread), so the trail is gone in a frame or two.
    const float3 outside = max(max(lo - raw, raw - hi), 0);
    const float spread = max(max(hi.r - lo.r, hi.g - lo.g), hi.b - lo.b) + 0.03;
    const float miss = max(outside.r, max(outside.g, outside.b)) / spread;
    const float cut = saturate(1.5 - miss * 0.75);
    keep *= cut + (1 - cut) * (bright * 0.6);
    const float3 plain = lerp(here.rgb, before, keep);
    if (!(bright > 0))
        return float4(plain, here.a);
    const float3 weights = float3(0.2126, 0.7152, 0.0722);
    const float a = (1 - keep) / (1 + dot(here.rgb, weights)), b = keep / (1 + dot(before, weights));
    const float3 karis = (here.rgb * a + before * b) / max(a + b, 1e-6);
    return float4(lerp(plain, karis, bright), here.a);
}
)";
bool failHistory(uint32_t code) {
    historyFault = code;
    history.ready = false;
    std::lock_guard lock(copyTimingLock);
    SpidyEyeHistoryData.fault = code;
    return false;
}
bool prepareHistory() {
    if (historyFault)
        return false;
    if (history.ready)
        return true;
    D3D12_DESCRIPTOR_RANGE range{D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0, 0, 0};
    D3D12_ROOT_PARAMETER parameters[2]{};
    parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    parameters[0].DescriptorTable = {1, &range};
    parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    parameters[1].Constants.Num32BitValues = 16;
    parameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_STATIC_SAMPLER_DESC sampler{};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.MaxLOD = D3D12_FLOAT32_MAX;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_ROOT_SIGNATURE_DESC rs{2, parameters, 1, &sampler, D3D12_ROOT_SIGNATURE_FLAG_NONE};
    ComPtr<ID3DBlob> signature, error;
    if (FAILED(D3D12SerializeRootSignature(&rs, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error)) ||
        FAILED(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(),
                                           IID_PPV_ARGS(&history.root))))
        return failHistory(1);
    ComPtr<ID3DBlob> vs, ps;
    const auto length = std::strlen(historyShader);
    if (FAILED(D3DCompile(historyShader, length, "spidy_eye_history", nullptr, nullptr, "vs", "vs_5_0",
                          D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &vs, &error)) ||
        FAILED(D3DCompile(historyShader, length, "spidy_eye_history", nullptr, nullptr, "ps", "ps_5_0",
                          D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &ps, &error)))
        return failHistory(2);
    D3D12_GRAPHICS_PIPELINE_STATE_DESC d{};
    d.pRootSignature = history.root.Get();
    d.VS = {vs->GetBufferPointer(), vs->GetBufferSize()};
    d.PS = {ps->GetBufferPointer(), ps->GetBufferSize()};
    d.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    d.NumRenderTargets = 1;
    d.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    d.SampleDesc.Count = 1;
    d.SampleMask = UINT_MAX;
    d.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    d.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    d.RasterizerState.DepthClipEnable = TRUE;
    auto& blend = d.BlendState.RenderTarget[0];
    blend.SrcBlend = blend.SrcBlendAlpha = D3D12_BLEND_ONE;
    blend.DestBlend = blend.DestBlendAlpha = D3D12_BLEND_ZERO;
    blend.BlendOp = blend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    blend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    if (FAILED(device->CreateGraphicsPipelineState(&d, IID_PPV_ARGS(&history.state))))
        return failHistory(3);
    // Two pictures an eye, the headset's size, in the staged images' format.
    for (unsigned i = 0; i < 2; ++i)
        for (unsigned k = 0; k < 2; ++k) {
            auto desc = stagedEyes[i]->GetDesc();
            desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
            D3D12_HEAP_PROPERTIES heap{};
            heap.Type = D3D12_HEAP_TYPE_DEFAULT;
            if (FAILED(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc,
                                                       D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, nullptr,
                                                       IID_PPV_ARGS(&history.pictures[i][k]))))
                return failHistory(5);
        }
    // Views: for eye i read from picture k, the table at (2i + k) * 2 holds
    // the staged image (t0) and that picture (t1). Targets: picture (i, k) at 2i + k.
    D3D12_DESCRIPTOR_HEAP_DESC views{};
    views.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    views.NumDescriptors = 8;
    views.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    D3D12_DESCRIPTOR_HEAP_DESC targetViews{};
    targetViews.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    targetViews.NumDescriptors = 4;
    if (FAILED(device->CreateDescriptorHeap(&views, IID_PPV_ARGS(&history.views))) ||
        FAILED(device->CreateDescriptorHeap(&targetViews, IID_PPV_ARGS(&history.targets))))
        return failHistory(4);
    const auto viewStep = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    const auto targetStep = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    D3D12_SHADER_RESOURCE_VIEW_DESC sv{};
    sv.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    sv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    sv.Texture2D.MipLevels = 1;
    D3D12_RENDER_TARGET_VIEW_DESC rv{};
    rv.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    rv.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    for (unsigned i = 0; i < 2; ++i)
        for (unsigned k = 0; k < 2; ++k) {
            auto view = history.views->GetCPUDescriptorHandleForHeapStart();
            view.ptr += static_cast<SIZE_T>(viewStep) * ((2 * i + k) * 2);
            device->CreateShaderResourceView(stagedEyes[i].Get(), &sv, view);
            view.ptr += viewStep;
            device->CreateShaderResourceView(history.pictures[i][k].Get(), &sv, view);
            auto target = history.targets->GetCPUDescriptorHandleForHeapStart();
            target.ptr += static_cast<SIZE_T>(targetStep) * (2 * i + k);
            device->CreateRenderTargetView(history.pictures[i][k].Get(), &rv, target);
        }
    UINT64 frequency{};
    D3D12_QUERY_HEAP_DESC qd{};
    qd.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
    qd.Count = 2 * static_cast<UINT>(captureCommands.size());
    D3D12_HEAP_PROPERTIES readback{};
    readback.Type = D3D12_HEAP_TYPE_READBACK;
    D3D12_RESOURCE_DESC buffer{};
    buffer.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    buffer.Width = sizeof(uint64_t) * qd.Count;
    buffer.Height = 1;
    buffer.DepthOrArraySize = 1;
    buffer.MipLevels = 1;
    buffer.SampleDesc.Count = 1;
    buffer.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    void* mapped{};
    if (SUCCEEDED(queue->GetTimestampFrequency(&frequency)) && frequency &&
        SUCCEEDED(device->CreateQueryHeap(&qd, IID_PPV_ARGS(&history.queries))) &&
        SUCCEEDED(device->CreateCommittedResource(&readback, D3D12_HEAP_FLAG_NONE, &buffer,
                                                  D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
                                                  IID_PPV_ARGS(&history.stamps))) &&
        SUCCEEDED(history.stamps->Map(0, nullptr, &mapped))) {
        history.stampData = static_cast<const volatile uint64_t*>(mapped);
        history.ticksPerMs = static_cast<double>(frequency) / 1000.0;
    } else {
        history.queries.Reset();
        history.stamps.Reset();
    }
    history.ready = true;
    history.valid = false;
    return true;
}
void noteHistoryTime(size_t slot) {
    if (!history.timed[slot] || !history.stampData)
        return;
    history.timed[slot] = false;
    const uint64_t begin = history.stampData[2 * slot], end = history.stampData[2 * slot + 1];
    if (end > begin) {
        std::lock_guard lock(copyTimingLock);
        SpidyEyeHistoryData.gpuMs += static_cast<double>(end - begin) / history.ticksPerMs;
        ++SpidyEyeHistoryData.timed;
    }
    if (end > begin) {
        historyClock.note(static_cast<double>(end - begin) / history.ticksPerMs);
        std::lock_guard lock(historyMedianLock);
        historyMedian.note(static_cast<float>(static_cast<double>(end - begin) / history.ticksPerMs));
    }
}
bool findViews(uint64_t serial, std::array<eye_history::EyeView, 2>& out) {
    std::lock_guard lock(viewLock);
    for (const auto& noted : notedViews)
        if (noted.serial == serial && serial) {
            out = noted.eyes;
            return true;
        }
    return false;
}
// The staged pair (in COPY_SOURCE) blended with the history, turned to its
// view: drawn into the other picture, which is copied back into the staged
// image and kept for the next pair.
void drawHistory(ID3D12GraphicsCommandList* list, size_t slot, uint64_t serial) {
    std::array<eye_history::EyeView, 2> now{};
    const bool viewed = findViews(serial, now);
    const uint32_t percent = historyNow(), loosenPercent = historyPercent.load(), trail = trailPercent.load();
    float frame[2][16]{};
    bool blended = false, cut = false, moving = false;
    float maxTravel = 0, leastKeep = 1, leastStill = 0;
    for (unsigned i = 0; i < 2; ++i) {
        std::array<float, 9> h{};
        const bool both = viewed && history.valid &&
                          eye_history::historyReprojection(history.shown[i], now[i], h);
        const float turn = both ? eye_history::historyTurn(history.shown[i], now[i]) : 0;
        const float travel = both ? eye_history::historyTravel(history.shown[i], now[i]) : 0;
        // Update 23: less of the old picture, held less loosely, while the head turns.
        // Update 34.2: and far less while the eyes travel (a swing): down to TRAIL at swing
        // speed, none past the cut; the clamp the plain one while moving.
        const float still = eye_history::turnedWeight(eye_history::historyWeight(percent, both, turn), turn);
        // Update 37: HISTORY CUT scales the motion the fade-out sees (not the turned weight).
        const uint32_t cutNow = cutPercent.load(std::memory_order_relaxed);
        const float travelCut = eye_history::cutMotion(travel, cutNow), turnCut = eye_history::cutMotion(turn, cutNow);
        const float keep = eye_history::movedWeight(still, travelCut, turnCut, trail);
        for (int r = 0; r < 3; ++r)
            for (int c = 0; c < 3; ++c)
                frame[i][r * 4 + c] = h[r * 3 + c];
        frame[i][12] = keep;
        frame[i][3] = keep > 0 ? eye_history::lightHoldInForce(lightHoldPercent.load(std::memory_order_relaxed),
                                                               travelCut, turnCut)
                               : 0.f; // 0.0.73 test 3 LIGHT HOLD (row0.w)
        frame[i][13] = 1.f / static_cast<float>(outWidth);
        frame[i][14] = 1.f / static_cast<float>(outHeight);
        frame[i][15] = eye_history::movedLoosen(
            eye_history::turnedLoosen(
                eye_taa::loosenInForce(eye_history::historyLoosen(loosenPercent), temporalOn.load()), turn),
            travelCut, turnCut);
        blended = blended || keep > 0;
        if (i == 0 && both) {
            maxTravel = travel;
            cut = still > 0 && !(keep > 0);
            moving = keep > 0 && eye_history::motionShare(travelCut, turnCut) > .5f;
            leastKeep = keep;
            leastStill = still;
        }
    }
    const unsigned read = history.parity, write = 1 - read;
    const auto first = static_cast<UINT>(2 * slot);
    if (history.queries)
        originalEndQuery(list, history.queries.Get(), D3D12_QUERY_TYPE_TIMESTAMP, first);
    D3D12_RESOURCE_BARRIER b[4]{};
    auto transition = [&](unsigned n, ID3D12Resource* r, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to) {
        b[n].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        b[n].Transition = {r, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, from, to};
    };
    for (unsigned i = 0; i < 2; ++i) {
        transition(2 * i, stagedEyes[i].Get(), D3D12_RESOURCE_STATE_COPY_SOURCE,
                   D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        transition(2 * i + 1, history.pictures[i][write].Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                   D3D12_RESOURCE_STATE_RENDER_TARGET);
    }
    originalBarrier(list, 4, b);
    list->SetPipelineState(history.state.Get());
    list->SetGraphicsRootSignature(history.root.Get());
    ID3D12DescriptorHeap* heaps[] = {history.views.Get()};
    list->SetDescriptorHeaps(1, heaps);
    const D3D12_VIEWPORT viewport{0, 0, static_cast<float>(outWidth), static_cast<float>(outHeight), 0, 1};
    const D3D12_RECT rect{0, 0, static_cast<LONG>(outWidth), static_cast<LONG>(outHeight)};
    list->RSSetViewports(1, &viewport);
    list->RSSetScissorRects(1, &rect);
    list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    const auto viewStep = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    const auto targetStep = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    for (unsigned i = 0; i < 2; ++i) {
        auto view = history.views->GetGPUDescriptorHandleForHeapStart();
        view.ptr += static_cast<UINT64>(viewStep) * ((2 * i + read) * 2);
        auto target = history.targets->GetCPUDescriptorHandleForHeapStart();
        target.ptr += static_cast<SIZE_T>(targetStep) * (2 * i + write);
        list->SetGraphicsRootDescriptorTable(0, view);
        list->SetGraphicsRoot32BitConstants(1, 16, frame[i], 0);
        list->OMSetRenderTargets(1, &target, FALSE, nullptr);
        list->DrawInstanced(3, 1, 0, 0);
    }
    for (unsigned i = 0; i < 2; ++i) {
        transition(2 * i, stagedEyes[i].Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                   D3D12_RESOURCE_STATE_COPY_DEST);
        transition(2 * i + 1, history.pictures[i][write].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET,
                   D3D12_RESOURCE_STATE_COPY_SOURCE);
    }
    originalBarrier(list, 4, b);
    for (unsigned i = 0; i < 2; ++i)
        list->CopyResource(stagedEyes[i].Get(), history.pictures[i][write].Get());
    for (unsigned i = 0; i < 2; ++i) {
        transition(2 * i, stagedEyes[i].Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COPY_SOURCE);
        transition(2 * i + 1, history.pictures[i][write].Get(), D3D12_RESOURCE_STATE_COPY_SOURCE,
                   D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    }
    originalBarrier(list, 4, b);
    if (history.queries) {
        originalEndQuery(list, history.queries.Get(), D3D12_QUERY_TYPE_TIMESTAMP, first + 1);
        list->ResolveQueryData(history.queries.Get(), D3D12_QUERY_TYPE_TIMESTAMP, first, 2, history.stamps.Get(),
                               sizeof(uint64_t) * first);
        history.timed[slot] = true;
    }
    history.parity = write;
    // 0.0.73 test 3 JITTER CALM: the share of its still-head history this pass kept (1 still, 0 when a
    // swing cut it), for the next pair's sub-pixel step.
    keptMilli.store(blended && viewed && std::isfinite(leastKeep) && leastKeep > 0 && leastStill > 0
                        ? static_cast<uint32_t>(std::min(leastKeep / leastStill, 1.f) * 1000.f + .5f)
                        : 0u,
                    std::memory_order_relaxed);
    // Without its view the picture is still kept, but it cannot be turned to
    // the next pair's: that one starts again.
    history.valid = viewed;
    history.shown = now;
    std::lock_guard lock(copyTimingLock);
    if (blended)
        ++SpidyEyeHistoryData.blended;
    else
        ++SpidyEyeHistoryData.restarted;
    SpidyEyeHistoryData.trail = trail;
    if (viewed && std::isfinite(maxTravel) && maxTravel < 1e6f) {
        SpidyEyeHistoryData.travelSum += maxTravel;
        SpidyEyeHistoryData.travelMax = std::max(SpidyEyeHistoryData.travelMax, maxTravel);
    }
    SpidyEyeHistoryData.moving += moving ? 1 : 0;
    SpidyEyeHistoryData.motionCuts += cut ? 1 : 0;
    if (blended && leastKeep > 0)
        SpidyEyeHistoryData.keepMin = std::min(SpidyEyeHistoryData.keepMin, leastKeep);
    if (!viewed)
        ++SpidyEyeHistoryData.unviewed;
}
// Update 32.1: replaced eye images let go once the GPU is past them, and the
// AA pass's views of the eyes rewritten once no submitted pass reads them.
// False: not yet (the pair waits; the headset keeps the last one).
bool settleSwappedEyes() {
    const auto completed = fence ? fence->GetCompletedValue() : 0;
    std::erase_if(retiredEyes, [&](const RetiredEye& r) { return eye_swap::releasable(r.fence, completed); });
    if (!eyeViewStale[0] && !eyeViewStale[1])
        return true;
    if (!eye_swap::releasable(fenceValue, completed))
        return false;
    if (aa.ready && aa.views) {
        const auto step = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        for (unsigned i = 0; i < 2; ++i)
            if (eyeViewStale[i]) {
                D3D12_SHADER_RESOURCE_VIEW_DESC sv{};
                sv.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
                sv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
                sv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
                sv.Texture2D.MipLevels = 1;
                auto view = aa.views->GetCPUDescriptorHandleForHeapStart();
                view.ptr += static_cast<SIZE_T>(step) * i;
                device->CreateShaderResourceView(resources[i].Get(), &sv, view);
            }
    }
    eyeViewStale = {};
    return true;
}
// 0.0.71 MIRROR VIEW: the mirror's picture into the lower middle of both staged eyes (they are in
// COPY_SOURCE between uses). A copy needs the same pixel kind and a fitting size, else the picture
// stops for good (mvFault) and the eyes go on untouched.
bool mirrorCopyable(DXGI_FORMAT a, DXGI_FORMAT b) {
    if (a == b)
        return true;
    auto family = [](DXGI_FORMAT f) -> int {
        switch (f) {
        case DXGI_FORMAT_R8G8B8A8_UNORM:
        case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
        case DXGI_FORMAT_R8G8B8A8_TYPELESS:
            return 1;
        case DXGI_FORMAT_B8G8R8A8_UNORM:
        case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
        case DXGI_FORMAT_B8G8R8A8_TYPELESS:
            return 2;
        case DXGI_FORMAT_R10G10B10A2_UNORM:
        case DXGI_FORMAT_R10G10B10A2_TYPELESS:
            return 3;
        case DXGI_FORMAT_R16G16B16A16_FLOAT:
        case DXGI_FORMAT_R16G16B16A16_TYPELESS:
            return 4;
        default:
            return 0;
        }
    };
    return family(a) && family(a) == family(b);
}
void copyMirrorView(ID3D12GraphicsCommandList* list) {
    if (!mvResource || !mvKnown || mvFault.load() || !stagedEyes[0] || !stagedEyes[1])
        return;
    const auto src = mvResource->GetDesc();
    const auto dst = stagedEyes[0]->GetDesc();
    if (src.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D || src.SampleDesc.Count != 1 ||
        !mirrorCopyable(src.Format, dst.Format)) {
        mvFault = 1;
        return;
    }
    const auto w = static_cast<UINT>(src.Width), h = src.Height;
    const UINT shift = outWidth / 50; // a little nearer than far away: each eye sees it shifted in
    if (w + 2 * shift + 8 > outWidth || h + 8 > outHeight) {
        mvFault = 3;
        return;
    }
    D3D12_RESOURCE_BARRIER b[3]{};
    UINT count = 0;
    const bool move = mvState != D3D12_RESOURCE_STATE_COPY_SOURCE;
    if (move) {
        b[count].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        b[count++].Transition = {mvResource.Get(), D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, mvState,
                                 D3D12_RESOURCE_STATE_COPY_SOURCE};
    }
    for (unsigned i = 0; i < 2; ++i) {
        b[count].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        b[count++].Transition = {stagedEyes[i].Get(), D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                                 D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_COPY_DEST};
    }
    originalBarrier(list, count, b);
    const UINT centre = (outWidth - w) / 2;
    const UINT top = std::min<UINT>(outHeight - h - 4, outHeight / 2 + outHeight / 20);
    for (unsigned i = 0; i < 2; ++i) {
        D3D12_TEXTURE_COPY_LOCATION from{}, to{};
        from.pResource = mvResource.Get();
        from.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        to.pResource = stagedEyes[i].Get();
        to.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        const UINT x = i == 0 ? centre + shift : centre - shift;
        list->CopyTextureRegion(&to, x, top, 0, &from, nullptr);
    }
    for (UINT n = 0; n < count; ++n)
        std::swap(b[n].Transition.StateBefore, b[n].Transition.StateAfter);
    originalBarrier(list, count, b);
    ++mvCopies;
}
void stagePair(EyePairState::Stamp stamp) {
    if (eyesLostCode.load() || !known[0] || !known[1] || !makeReadbacks() || !settleSwappedEyes())
        return;
    for (unsigned i = 0; i < 2; ++i)
        if (!stagedEyes[i]) {
            auto desc = resources[i]->GetDesc();
            // A render target too, for the AA pass (drawAa); the headset's
            // size, which is smaller than the eye's when it is supersampled.
            desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
            desc.Width = outWidth;
            desc.Height = outHeight;
            D3D12_HEAP_PROPERTIES heap{};
            heap.Type = D3D12_HEAP_TYPE_DEFAULT;
            if (FAILED(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc,
                                                       D3D12_RESOURCE_STATE_COPY_SOURCE, nullptr,
                                                       IID_PPV_ARGS(&stagedEyes[i])))) {
                fault = 3805;
                return;
            }
        }
    auto* slot = readyCommands(captureCommands);
    if (!slot)
        return;
    const auto index = static_cast<size_t>(slot - captureCommands.data());
    noteAaTime(index);
    noteHistoryTime(index);
    // Supersampled eyes cannot be copied: they are always filtered down,
    // without FXAA (the extra samples already smooth the edges).
    const bool down = downsampling();
    const bool smooth = (down || (aaMode.load() == 1 && !sm1Lock.load())) && prepareAa(); // SM1STYLE: no FXAA
    if (down && !smooth) {
        fault = 3806; // the headset would get nothing: say why (aaFault)
        return;
    }
    if (smooth)
        drawAa(slot->list.Get(), index);
    else
        for (unsigned i = 0; i < 2; ++i)
            copyEyeTexture(slot->list.Get(), resources[i].Get(), states[i], stagedEyes[i].Get(), originalBarrier,
                           D3D12_RESOURCE_STATE_COPY_SOURCE);
    // The history after AA: it blends what the headset is shown.
    if (historyNow() && prepareHistory()) {
        drawHistory(slot->list.Get(), index, stamp.serial);
    } else {
        history.valid = false;
        keptMilli.store(0, std::memory_order_relaxed); // JITTER CALM: no history, no step to average
    }
    copyMirrorView(slot->list.Get()); // 0.0.71: after the AA and history, so neither smears it
    if (!submitCommands(*slot)) {
        history.valid = false;
        return;
    }
    aaPairs += smooth;
    capturedStamp = stamp;
    capturedMs = GetTickCount64();
    capturedQpc = qpcNow();
    ++capturedPairs;
}
void captureImage(ID3D12GraphicsCommandList* list, ID3D12Resource* image, unsigned eye) {
    D3D12_RESOURCE_BARRIER b{};
    b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b.Transition = {image, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_STATE_RENDER_TARGET,
                    D3D12_RESOURCE_STATE_COPY_SOURCE};
    originalBarrier(list, 1, &b);
    D3D12_TEXTURE_COPY_LOCATION src{}, dst{};
    src.pResource = image;
    src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dst.pResource = readbacks[eye].Get();
    dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    dst.PlacedFootprint = layouts[eye];
    list->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
    std::swap(b.Transition.StateBefore, b.Transition.StateAfter);
    originalBarrier(list, 1, &b);
}
void capturePair(EyePairState::Stamp stamp) {
    if (eyesLostCode.load() || !known[0] || !known[1] || !makeReadbacks() || !settleSwappedEyes())
        return;
    if (copyToXr)
        for (unsigned i = 0; i < 2; ++i) {
            if (!targets[i]) {
                fault = 3305;
                return;
            }
            const auto destination = targets[i]->GetDesc();
            if (destination.Width != width || destination.Height != height) {
                fault = 3306;
                return;
            }
        }
    const auto completed = fence->GetCompletedValue();
    if (completed == UINT64_MAX) {
        removedSeen = true; // the device was removed (0.0.74 test 10 MONITOR MEMORY)
        fault = 3301;
        return;
    }
    if (completed < fenceValue)
        return; // previous allocator and readbacks still in use
    if (FAILED(allocator->Reset()) || FAILED(originalReset(command.Get(), allocator.Get(), nullptr))) {
        fault = 3302;
        return;
    }
    for (unsigned i = 0; i < 2; ++i) {
        if (copyToXr) {
            copyEyeTexture(command.Get(), resources[i].Get(), states[i], targets[i].Get(), originalBarrier);
        }
        if (!captureImages)
            continue;
        // Read the acquired XR image after its copy, so diagnostics show the
        // actual submitted texture rather than another native render generation.
        auto* captured = copyToXr ? targets[i].Get() : resources[i].Get();
        const auto capturedState = copyToXr ? D3D12_RESOURCE_STATE_RENDER_TARGET : states[i];
        D3D12_RESOURCE_BARRIER b{};
        b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        b.Transition = {captured, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, capturedState,
                        D3D12_RESOURCE_STATE_COPY_SOURCE};
        if (capturedState != D3D12_RESOURCE_STATE_COPY_SOURCE)
            originalBarrier(command.Get(), 1, &b);
        {
            D3D12_TEXTURE_COPY_LOCATION src{}, dst{};
            src.pResource = captured;
            src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            dst.pResource = readbacks[i].Get();
            dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
            dst.PlacedFootprint = layouts[i];
            command->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
        }
        std::swap(b.Transition.StateBefore, b.Transition.StateAfter);
        if (capturedState != D3D12_RESOURCE_STATE_COPY_SOURCE)
            originalBarrier(command.Get(), 1, &b);
    }
    if (FAILED(command->Close())) {
        fault = 3303;
        return;
    }
    ID3D12CommandList* list = command.Get();
    originalExecute(queue.Get(), 1, &list);
    if (FAILED(queue->Signal(fence.Get(), ++fenceValue))) {
        fault = 3304;
        return;
    }
    ++pairs;
    lastSerial = stamp.serial;
    lastGeneration = stamp.generation;
    if (copyToXr)
        requestFence = fenceValue;
    copyReady.notify_all();
}
void STDMETHODCALLTYPE execute(ID3D12CommandQueue* q, UINT count, ID3D12CommandList* const* commands) {
    if (!enabled) {
        originalExecute(q, count, commands);
        return;
    }
    // Runtime/overlay queues must not serialize behind the game submission lock.
    if (q != queue.Get()) {
        originalExecute(q, count, commands);
        depthExecuted(false, count, commands); // 0.0.74 test 10 SEE-THROUGH
        std::lock_guard recordLock(recording);
        for (UINT n = 0; n < count; ++n)
            if (auto found = lists.find(commands[n]); found != lists.end() && !found->second.empty()) {
                // 0.0.71: the mirror's image on another queue stops only the mirror picture.
                bool eyeEvent{};
                for (const auto& e : found->second)
                    eyeEvent |= e.eye < 2;
                if (!eyeEvent) {
                    mvFault = 4;
                    mvKnown = false;
                    continue;
                }
                fault = 3401;
                copyReady.notify_all();
            }
        return;
    }
    std::lock_guard submitLock(submission);
    if (pendingCount && fence)
        noteCopies(fence->GetCompletedValue());
    buildFovea(); // Update 37.2: ahead of the game's lists on this queue
    originalExecute(q, count, commands);
    depthExecuted(true, count, commands); // 0.0.74 test 10 SEE-THROUGH
    std::lock_guard recordLock(recording);
    ++executes;
    bool relevant{};
    for (UINT n = 0; n < count; ++n)
        if (auto found = lists.find(commands[n]); found != lists.end())
            for (const auto& e : found->second) {
                if (e.eye == 2) { // 0.0.71 MIRROR VIEW: its own state, its own fault
                    if (e.marker || !mvResource || e.transition.pResource != mvResource.Get())
                        continue;
                    if (e.flags != D3D12_RESOURCE_BARRIER_FLAG_NONE ||
                        (e.transition.Subresource != 0 &&
                         e.transition.Subresource != D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES)) {
                        mvFault = 2;
                        mvKnown = false;
                        continue;
                    }
                    if (mvKnown && mvState != e.transition.StateBefore) {
                        mvKnown = false; // learn it again from the next barrier
                        continue;
                    }
                    mvState = e.transition.StateAfter;
                    mvKnown = true;
                    continue;
                }
                relevant = true;
                if (q != queue.Get()) {
                    fault = 3401;
                    continue;
                }
                if (e.marker) {
                    if (e.ended) {
                        pair.end(e.eye, e.stamp);
                        ++ends[e.eye];
                    } else {
                        pair.begin(e.eye, e.stamp);
                        ++begins[e.eye];
                    }
                } else {
                    // Update 32.1: recorded for an eye image since replaced.
                    if (e.transition.pResource != resources[e.eye].Get())
                        continue;
                    if (e.flags != D3D12_RESOURCE_BARRIER_FLAG_NONE ||
                        (e.transition.Subresource != 0 &&
                         e.transition.Subresource != D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES)) {
                        fault = 3402;
                        known[e.eye] = false;
                        continue;
                    }
                    if (known[e.eye] && states[e.eye] != e.transition.StateBefore) {
                        fault = 3403;
                        known[e.eye] = false;
                        continue;
                    }
                    states[e.eye] = e.transition.StateAfter;
                    known[e.eye] = true;
                }
            }
    if (!relevant)
        return;
    const auto ready = pair.ready();
    if (staged) {
        if (enabled && !fault && GetTickCount64() < deadline && ready.serial &&
            ready.generation > capturedStamp.generation)
            stagePair(ready);
    } else if (enabled && !fault && GetTickCount64() < deadline && ready.serial &&
               ready.generation > lastGeneration &&
               (!copyToXr || (requestedSerial == ready.serial && !requestFence)))
        capturePair(ready);
    publish();
    if (fault)
        copyReady.notify_all();
}
DXGI_FORMAT typelessFamily(DXGI_FORMAT view) {
    switch (view) {
    case DXGI_FORMAT_R8G8B8A8_UNORM:
        return DXGI_FORMAT_R8G8B8A8_TYPELESS;
    case DXGI_FORMAT_B8G8R8A8_UNORM:
        return DXGI_FORMAT_B8G8R8A8_TYPELESS;
    case DXGI_FORMAT_R10G10B10A2_UNORM:
        return DXGI_FORMAT_R10G10B10A2_TYPELESS;
    default:
        return DXGI_FORMAT_R16G16B16A16_TYPELESS;
    }
}
// Present thread: copies the back buffer about to be shown on the game's
// render queue, after the frame it rendered there and before the queue renders
// into that buffer again. The swapchain reports the queue it was created with,
// which with Streamline is a wrapper of that queue, so the game's swapchain is
// recognized by its back buffer's device.
ScreenSkip copyScreen(IDXGISwapChain* swapchain) {
    ComPtr<IDXGISwapChain3> chain;
    ComPtr<ID3D12Resource> buffer;
    if (FAILED(swapchain->QueryInterface(IID_PPV_ARGS(&chain))) ||
        FAILED(chain->GetBuffer(chain->GetCurrentBackBufferIndex(), IID_PPV_ARGS(&buffer))))
        return screenNoBuffer;
    ComPtr<ID3D12Device> owner;
    ComPtr<IUnknown> ownerIdentity, gameIdentity;
    if (FAILED(buffer->GetDevice(IID_PPV_ARGS(&owner))) || FAILED(owner.As(&ownerIdentity)) ||
        FAILED(device.As(&gameIdentity)) || ownerIdentity.Get() != gameIdentity.Get())
        return screenOtherDevice;
    const auto desc = buffer->GetDesc();
    backBufferFormat = desc.Format;
    bool linear{};
    const auto view = screenView(desc.Format, linear);
    if (view == DXGI_FORMAT_UNKNOWN)
        return screenFormat;
    if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D || desc.SampleDesc.Count != 1 ||
        desc.DepthOrArraySize != 1 || desc.MipLevels != 1 || !validEyeSize(desc.Width) || !validEyeSize(desc.Height))
        return screenShape;
    std::lock_guard submitLock(submission);
    std::lock_guard recordLock(recording);
    if (!enabled || fault || GetTickCount64() >= deadline)
        return screenStopped;
    const auto now = GetTickCount64();
    std::erase_if(retiredScreens, [&](const Retired& old) { return now >= old.releaseMs; });
    if (!screen.texture || screen.width != desc.Width || screen.height != desc.Height || screen.view != view) {
        if (screen.texture)
            retiredScreens.push_back({screen.texture, now + 3000});
        screen = {};
        auto copy = desc;
        copy.Format = typelessFamily(view);
        copy.Flags = D3D12_RESOURCE_FLAG_NONE;
        copy.Alignment = 0;
        D3D12_HEAP_PROPERTIES heap{};
        heap.Type = D3D12_HEAP_TYPE_DEFAULT;
        if (FAILED(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &copy,
                                                   D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, nullptr,
                                                   IID_PPV_ARGS(&screen.texture))))
            return screenTexture;
        screen.view = view;
        screen.linear = linear;
        screen.width = static_cast<uint32_t>(desc.Width);
        screen.height = desc.Height;
    }
    auto* slot = readyCommands(screenCommands);
    if (!slot)
        return screenBusy; // earlier copies still in flight: skip this frame
    D3D12_RESOURCE_BARRIER b[2]{};
    b[0].Type = b[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b[0].Transition = {buffer.Get(), D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_STATE_PRESENT,
                       D3D12_RESOURCE_STATE_COPY_SOURCE};
    b[1].Transition = {screen.texture.Get(), D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                       D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST};
    originalBarrier(slot->list.Get(), 2, b);
    slot->list->CopyResource(screen.texture.Get(), buffer.Get());
    for (auto& barrier : b)
        std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);
    originalBarrier(slot->list.Get(), 2, b);
    if (!submitCommands(*slot))
        return screenStopped;
    ++screen.frame;
    screenMs = now;
    return screenCopied;
}
// The pipeline that draws a part of one texture over a whole target of
// `format`: a screen-covering triangle and one linear sample (submission lock).
// 0.0.74 test 10 MONITOR: the streamer's pipeline (streamer true) is the same but for its
// shader: a rotation-only reprojection of the eye to the window's smoothed view, cut to the
// window's shape about the eye's axis (streamer_view.hpp sample()), black off the image.
bool preparePipeline(MirrorPipeline& m, DXGI_FORMAT format, bool streamer) {
    if (m.failed)
        return false;
    if (m.state && m.format == format)
        return true;
    m.failed = true; // cleared at the end: a failure is not tried again every frame
    if (!m.root) {
        D3D12_DESCRIPTOR_RANGE range{D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, 0};
        D3D12_ROOT_PARAMETER parameters[2]{};
        parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        parameters[0].DescriptorTable = {1, &range};
        parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        parameters[1].Constants.Num32BitValues = streamer ? 24 : 8;
        parameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        D3D12_STATIC_SAMPLER_DESC sampler{};
        sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        sampler.MaxLOD = D3D12_FLOAT32_MAX;
        sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        D3D12_ROOT_SIGNATURE_DESC rs{2, parameters, 1, &sampler, D3D12_ROOT_SIGNATURE_FLAG_NONE};
        ComPtr<ID3DBlob> signature, error;
        D3D12_DESCRIPTOR_HEAP_DESC views{};
        views.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        views.NumDescriptors = 1;
        views.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        D3D12_DESCRIPTOR_HEAP_DESC targetsHeap{};
        targetsHeap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        targetsHeap.NumDescriptors = 1;
        if (FAILED(D3D12SerializeRootSignature(&rs, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error)) ||
            FAILED(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(),
                                               IID_PPV_ARGS(&m.root))) ||
            FAILED(device->CreateDescriptorHeap(&views, IID_PPV_ARGS(&m.source))) ||
            FAILED(device->CreateDescriptorHeap(&targetsHeap, IID_PPV_ARGS(&m.target)))) {
            m.root.Reset();
            return false;
        }
    }
    // Both images hold display-encoded values and neither view is sRGB, so
    // what is sampled is what is stored.
    const char* shader = R"(
Texture2D source : register(t0);
SamplerState scaled : register(s0);
cbuffer Part : register(b0) { float2 targetPixel; float2 offset; float2 scale; float2 unused; };
float4 vs(uint id : SV_VertexID) : SV_POSITION {
    const float2 corner = float2((id << 1) & 2, id & 2);
    return float4(corner * float2(2, -2) + float2(-1, 1), 0, 1);
}
float4 ps(float4 position : SV_POSITION) : SV_TARGET {
    return float4(source.SampleLevel(scaled, offset + position.xy * targetPixel * scale, 0).rgb, 1);
}
)";
    // streamer_view.hpp constantsOf: the turn's rows, the crop's and the lens's tangents
    // (left, right, down, up), the window's pixel size.
    const char* streamerShader = R"(
Texture2D source : register(t0);
SamplerState scaled : register(s0);
cbuffer Streamer : register(b0) { float4 row0; float4 row1; float4 row2; float4 crop; float4 lens; float4 pixel; };
float4 vs(uint id : SV_VertexID) : SV_POSITION {
    const float2 corner = float2((id << 1) & 2, id & 2);
    return float4(corner * float2(2, -2) + float2(-1, 1), 0, 1);
}
float4 ps(float4 position : SV_POSITION) : SV_TARGET {
    const float2 uv = position.xy * pixel.xy;
    const float3 d = float3(lerp(crop.x, crop.y, uv.x), lerp(crop.w, crop.z, uv.y), -1);
    const float3 e = float3(dot(row0.xyz, d), dot(row1.xyz, d), dot(row2.xyz, d));
    if (e.z > -0.0001)
        return float4(0, 0, 0, 1);
    const float2 t = e.xy / -e.z;
    const float2 st = float2((t.x - lens.x) / (lens.y - lens.x), (lens.w - t.y) / (lens.w - lens.z));
    if (any(st < 0) || any(st > 1))
        return float4(0, 0, 0, 1);
    return float4(source.SampleLevel(scaled, st, 0).rgb, 1);
}
)";
    if (streamer)
        shader = streamerShader;
    ComPtr<ID3DBlob> vs, ps, error;
    if (FAILED(D3DCompile(shader, std::strlen(shader), "spidy_mirror", nullptr, nullptr, "vs", "vs_5_0",
                          D3DCOMPILE_ENABLE_STRICTNESS, 0, &vs, &error)) ||
        FAILED(D3DCompile(shader, std::strlen(shader), "spidy_mirror", nullptr, nullptr, "ps", "ps_5_0",
                          D3DCOMPILE_ENABLE_STRICTNESS, 0, &ps, &error)))
        return false;
    D3D12_GRAPHICS_PIPELINE_STATE_DESC d{};
    d.pRootSignature = m.root.Get();
    d.VS = {vs->GetBufferPointer(), vs->GetBufferSize()};
    d.PS = {ps->GetBufferPointer(), ps->GetBufferSize()};
    d.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    d.NumRenderTargets = 1;
    d.RTVFormats[0] = format;
    d.SampleDesc.Count = 1;
    d.SampleMask = UINT_MAX;
    d.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    d.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    d.RasterizerState.DepthClipEnable = TRUE;
    auto& blend = d.BlendState.RenderTarget[0];
    blend.SrcBlend = blend.SrcBlendAlpha = D3D12_BLEND_ONE;
    blend.DestBlend = blend.DestBlendAlpha = D3D12_BLEND_ZERO;
    blend.BlendOp = blend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    blend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    m.state.Reset();
    if (FAILED(device->CreateGraphicsPipelineState(&d, IID_PPV_ARGS(&m.state))))
        return false;
    m.format = format;
    m.failed = false;
    return true;
}
bool prepareMirror(DXGI_FORMAT format) {
    return preparePipeline(mirrorPipeline, format, false);
}
// Present thread: the newest left eye over the back buffer about to be shown,
// on the game's queue. While the game does not render its own view (the stereo
// module's views option 32), nothing else draws there, and the window would
// show whichever old frames its buffers still hold. The eye fills the window's
// width or height and is cut to its shape about the middle; it is not
// stretched. A float (HDR) back buffer is left alone. The eye is the one the
// headset showed, Spidy's drawings on it, while a copy of that is fresh
// (keepShownEye), and the game's own image of it otherwise.
// 0.0.74 test 10 MONITOR (streamer_view.hpp): in play the copy also goes over the game's own
// view, sharpened or not, unless the session fell back (monitorFallBack): then the window keeps
// the game's own view (0.0.72) and nothing is drawn on it. A present that cannot draw the copy
// (none fresh, no usable lens) leaves the game's own view on the window too.
// Two GPUs: the window's monitor on the game's GPU (once a session); else no copy is drawn.
bool monitorOnGameGpu(IDXGISwapChain3* chain) {
    if (outputSameGpu)
        return true;
    ComPtr<IDXGIOutput> output;
    ComPtr<IDXGIAdapter> adapter;
    DXGI_ADAPTER_DESC a{};
    if (SUCCEEDED(chain->GetContainingOutput(&output)) && SUCCEEDED(output->GetParent(IID_PPV_ARGS(&adapter))) &&
        SUCCEEDED(adapter->GetDesc(&a))) {
        const auto luid = device->GetAdapterLuid();
        if (a.AdapterLuid.LowPart == luid.LowPart && a.AdapterLuid.HighPart == luid.HighPart)
            return outputSameGpu = true;
        monitorFallBack(spidy::streamer_view::otherGpu); // the monitor is on another GPU
        return false;
    }
    if (++outputChecks >= 240) // never told which GPU: taken as another
        monitorFallBack(spidy::streamer_view::otherGpu);
    return false;
}
void mirrorEye(IDXGISwapChain* swapchain) {
    namespace sv0 = spidy::streamer_view;
    const bool wanted = monitorStreaming();
    ComPtr<IDXGISwapChain3> chain;
    ComPtr<ID3D12Resource> buffer;
    if (FAILED(swapchain->QueryInterface(IID_PPV_ARGS(&chain))) ||
        FAILED(chain->GetBuffer(chain->GetCurrentBackBufferIndex(), IID_PPV_ARGS(&buffer))))
        return;
    ComPtr<ID3D12Device> owner;
    ComPtr<IUnknown> ownerIdentity, gameIdentity;
    if (FAILED(buffer->GetDevice(IID_PPV_ARGS(&owner))) || FAILED(owner.As(&ownerIdentity)) ||
        FAILED(device.As(&gameIdentity)) || ownerIdentity.Get() != gameIdentity.Get()) {
        if (wanted)
            monitorFallBack(sv0::otherGpu); // the window's image is not on the game's device
        return;
    }
    const auto desc = buffer->GetDesc();
    bool linear{};
    const auto view = screenView(desc.Format, linear);
    if (view == DXGI_FORMAT_UNKNOWN || linear || desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D ||
        desc.SampleDesc.Count != 1) {
        if (wanted)
            monitorFallBack(sv0::floatBackBuffer);
        return;
    }
    if (!desc.Width || !desc.Height)
        return;
    if (wanted) {
        if (FAILED(device->GetDeviceRemovedReason())) {
            removedSeen = true;
            monitorFallBack(sv0::deviceLost);
            return;
        }
        if (!monitorOnGameGpu(chain.Get()))
            return;
    }
    std::lock_guard submitLock(submission);
    std::lock_guard recordLock(recording);
    const auto now = GetTickCount64();
    if (wanted && (fault || eyesLostCode.load()))
        monitorFallBack(sv0::deviceLost);
    if (!enabled || fault || now >= deadline || !staged || !stagedEyes[0] || !capturedStamp.serial ||
        now - capturedMs > imageHoldMs || !width || !height)
        return;
    // Update 16: while sharpening, the unsharpened stagedEyes[0] (the monitor
    // gets no sharpening and no copy is added for it). 0.0.74 test 10 MONITOR: a
    // copy taken for the monitor is the headset's own (sharpened) image and is shown.
    const bool streaming = monitorStreaming();
    const bool shown = shownEye && shownEyeFresh(shownMs, now, mirrorRate.load()) &&
                       (!sharpening || shownView.streamer);
    // While the game renders its own view, only the monitor's fresh copy goes over it.
    if (!mirrorWanted && !(streaming && shown && shownView.streamer)) {
        if (streaming)
            shownWanted = true; // no fresh copy: the next frame's is taken
        return;
    }
    // 0.0.74 test 10 MONITOR: the copy through the streamer shader, if it has a usable view and
    // the shader builds (else the session falls back); the old mirror only in its own mode.
    sv0::Constants streamerConstants{};
    bool streamed = false;
    if (streaming && shown && shownView.streamer) {
        const auto smooth = sv0::monitorSmooth;
        const auto crop = sv0::fitCrop(shownView.fov, sv0::windowAspect(static_cast<uint32_t>(desc.Width), desc.Height),
                                       sv0::marginOf(smooth));
        if (!shownView.valid || !crop.valid) {
            // no usable lens this picture: the game's own view stays
        } else if (!preparePipeline(streamerPipeline, view, true)) {
            monitorFallBack(sv0::pipelineFailed);
        } else {
            const auto qpc = qpcNow();
            LARGE_INTEGER frequency{};
            QueryPerformanceFrequency(&frequency);
            const float dt = streamerQpc && frequency.QuadPart
                                 ? static_cast<float>(static_cast<double>(qpc - streamerQpc) /
                                                      static_cast<double>(frequency.QuadPart))
                                 : 1.f;
            streamerQpc = qpc;
            const auto smoothed = streamerSmoother.step(shownView.orientation, dt, sv0::smoothSeconds(smooth),
                                                        sv0::slack(shownView.fov, crop));
            streamerConstants = sv0::constantsOf(sv0::turnOf(shownView.orientation, smoothed), crop, shownView.fov,
                                                 static_cast<uint32_t>(desc.Width), desc.Height);
            streamed = true;
        }
    } else if (!streaming) {
        streamerSmoother.reset();
        streamerQpc = 0;
    }
    if (!streamed && !mirrorWanted) {
        shownWanted = true;
        return; // never the old mirror over the game's own view
    }
    if (!streamed && !prepareMirror(view))
        return;
    auto& pipeline = streamed ? streamerPipeline : mirrorPipeline;
    auto* slot = readyCommands(mirrorCommands);
    if (!slot)
        return; // earlier draws still in flight: this present keeps what the buffer holds
    auto* eye = shown ? shownEye.Get() : stagedEyes[0].Get();
    D3D12_SHADER_RESOURCE_VIEW_DESC sv{};
    sv.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    sv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    sv.Texture2D.MipLevels = 1;
    device->CreateShaderResourceView(eye, &sv, pipeline.source->GetCPUDescriptorHandleForHeapStart());
    D3D12_RENDER_TARGET_VIEW_DESC rv{};
    rv.Format = view;
    rv.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    const auto rtv = pipeline.target->GetCPUDescriptorHandleForHeapStart();
    device->CreateRenderTargetView(buffer.Get(), &rv, rtv);
    D3D12_RESOURCE_BARRIER b[2]{};
    b[0].Type = b[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b[0].Transition = {buffer.Get(), D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_STATE_PRESENT,
                       D3D12_RESOURCE_STATE_RENDER_TARGET};
    b[1].Transition = {eye, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_STATE_COPY_SOURCE,
                       D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE};
    auto* list = slot->list.Get();
    originalBarrier(list, 2, b);
    // The part of the eye with the window's shape, about its middle.
    const float window = static_cast<float>(desc.Width) / static_cast<float>(desc.Height);
    const float image = static_cast<float>(outWidth) / static_cast<float>(outHeight);
    const float across = window < image ? window / image : 1, down = window < image ? 1 : image / window;
    const float part[8] = {1.f / static_cast<float>(desc.Width), 1.f / static_cast<float>(desc.Height),
                           (1 - across) / 2, (1 - down) / 2, across, down, 0, 0};
    list->SetPipelineState(pipeline.state.Get());
    list->SetGraphicsRootSignature(pipeline.root.Get());
    ID3D12DescriptorHeap* heaps[] = {pipeline.source.Get()};
    list->SetDescriptorHeaps(1, heaps);
    list->SetGraphicsRootDescriptorTable(0, pipeline.source->GetGPUDescriptorHandleForHeapStart());
    if (streamed)
        list->SetGraphicsRoot32BitConstants(1, 24, streamerConstants.c, 0);
    else
        list->SetGraphicsRoot32BitConstants(1, 8, part, 0);
    list->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
    const D3D12_VIEWPORT viewport{0, 0, static_cast<float>(desc.Width), static_cast<float>(desc.Height), 0, 1};
    const D3D12_RECT rect{0, 0, static_cast<LONG>(desc.Width), static_cast<LONG>(desc.Height)};
    list->RSSetViewports(1, &viewport);
    list->RSSetScissorRects(1, &rect);
    list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    list->DrawInstanced(3, 1, 0, 0);
    for (auto& barrier : b)
        std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);
    originalBarrier(list, 2, b);
    if (submitCommands(*slot)) {
        ++mirrorCalls;
        shownMirrors += shown;
        if (streamed) {
            ++streamerDraws;
            monitorShown();
        }
    }
    shownWanted = true;
}
void captureScreen(IDXGISwapChain* swapchain) {
    ++screenPresents;
    screenSkipped = copyScreen(swapchain);
    publishScreen(0, nullptr, 0);
}
// Present reaches Present1 in some DXGI builds: one of them handles a frame.
thread_local bool presenting{};
void presentFrame(IDXGISwapChain* swapchain, UINT flags) {
    if (!enabled || (flags & DXGI_PRESENT_TEST))
        return;
    if (screenWanted)
        captureScreen(swapchain);
    else if (mirrorWanted || monitorStreaming()) // 0.0.74 test 10 MONITOR: over the game's own view too
        mirrorEye(swapchain);
}
// The game presents once a frame whether or not it rendered a view of its
// own. While it does not, the window only mirrors the left eye, and a present
// costs GPU time the eyes could have: hold back those beyond the monitor's rate.
// The game goes on without them (it paces itself by its own fences), and a
// held present draws no mirror either.
bool holdPresent(UINT flags) {
    const auto rate = mirrorRate.load();
    if (!rate || !enabled || !mirrorWanted || screenWanted || (flags & DXGI_PRESENT_TEST))
        return false;
    LARGE_INTEGER now{}, frequency{};
    QueryPerformanceCounter(&now);
    QueryPerformanceFrequency(&frequency);
    auto due = mirrorDue.load();
    if (spidy::monitorPresentDue(now.QuadPart, due, frequency.QuadPart, rate)) {
        mirrorDue = due;
        return false;
    }
    ++presentsHeld;
    return true;
}
HRESULT STDMETHODCALLTYPE present(IDXGISwapChain* swapchain, UINT interval, UINT flags) {
    ++presentCalls;
    const bool outer = !presenting;
    if (outer && holdPresent(flags))
        return S_OK;
    if (outer) {
        presenting = true;
        presentFrame(swapchain, flags);
    }
    const auto result = originalPresent(swapchain, interval, flags);
    if (outer)
        presenting = false;
    return result;
}
HRESULT STDMETHODCALLTYPE present1(IDXGISwapChain1* swapchain, UINT interval, UINT flags,
                                   const DXGI_PRESENT_PARAMETERS* parameters) {
    ++presentCalls;
    const bool outer = !presenting;
    if (outer && holdPresent(flags))
        return S_OK;
    if (outer) {
        presenting = true;
        presentFrame(swapchain, flags);
    }
    const auto result = originalPresent1(swapchain, interval, flags, parameters);
    if (outer)
        presenting = false;
    return result;
}
// Every swapchain in the process shares DXGI's Present methods. A throwaway
// 64-pixel swapchain on a hidden window, on a queue of its own, gives their
// addresses; the hooks act only on swapchains presenting on the game's queue.
bool presentEntries(void*& presentAddress, void*& present1Address) {
    using CreateFactory = HRESULT(WINAPI*)(UINT, REFIID, void**);
    const auto dxgi = GetModuleHandleW(L"dxgi.dll");
    const auto create = dxgi ? reinterpret_cast<CreateFactory>(GetProcAddress(dxgi, "CreateDXGIFactory2")) : nullptr;
    if (!create)
        return false;
    WNDCLASSEXW type{sizeof(type)};
    type.lpfnWndProc = DefWindowProcW;
    type.hInstance = reinterpret_cast<HINSTANCE>(&__ImageBase);
    type.lpszClassName = L"SpidyPresentEntries";
    RegisterClassExW(&type);
    const auto window = CreateWindowExW(0, type.lpszClassName, L"", WS_OVERLAPPEDWINDOW, 0, 0, 64, 64, nullptr,
                                        nullptr, type.hInstance, nullptr);
    if (!window)
        return false;
    bool found{};
    {
        ComPtr<IDXGIFactory2> factory;
        ComPtr<ID3D12CommandQueue> own;
        ComPtr<IDXGISwapChain1> chain;
        D3D12_COMMAND_QUEUE_DESC q{};
        q.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        DXGI_SWAP_CHAIN_DESC1 d{};
        d.Width = d.Height = 64;
        d.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        d.SampleDesc.Count = 1;
        d.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        d.BufferCount = 2;
        d.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        if (SUCCEEDED(create(0, IID_PPV_ARGS(&factory))) && SUCCEEDED(device->CreateCommandQueue(&q, IID_PPV_ARGS(&own))) &&
            SUCCEEDED(factory->CreateSwapChainForHwnd(own.Get(), window, &d, nullptr, nullptr, &chain))) {
            const auto table = *reinterpret_cast<void***>(chain.Get());
            presentAddress = table[8];   // IDXGISwapChain::Present
            present1Address = table[22]; // IDXGISwapChain1::Present1
            found = true;
        }
    }
    DestroyWindow(window);
    return found;
}
} // namespace
DXGI_FORMAT screenView(DXGI_FORMAT backBuffer, bool& linear) {
    linear = false;
    switch (backBuffer) {
    case DXGI_FORMAT_R8G8B8A8_TYPELESS:
    case DXGI_FORMAT_R8G8B8A8_UNORM:
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
        return DXGI_FORMAT_R8G8B8A8_UNORM;
    case DXGI_FORMAT_B8G8R8A8_TYPELESS:
    case DXGI_FORMAT_B8G8R8A8_UNORM:
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
        return DXGI_FORMAT_B8G8R8A8_UNORM;
    case DXGI_FORMAT_R10G10B10A2_TYPELESS:
    case DXGI_FORMAT_R10G10B10A2_UNORM:
        return DXGI_FORMAT_R10G10B10A2_UNORM;
    case DXGI_FORMAT_R16G16B16A16_TYPELESS:
    case DXGI_FORMAT_R16G16B16A16_FLOAT:
        linear = true;
        return DXGI_FORMAT_R16G16B16A16_FLOAT;
    default:
        return DXGI_FORMAT_UNKNOWN;
    }
}
uint32_t presents() {
    return presentCalls.load();
}
void mirror(bool wanted) {
    mirrorWanted = wanted;
}
void monitorRate(uint32_t rate) {
    mirrorRate = rate;
}
uint32_t heldPresents() {
    return presentsHeld.load();
}
uint32_t mirrored() {
    return mirrorCalls.load();
}
uint32_t mirroredShown() {
    return shownMirrors.load();
}
void streamer(bool inPlay) {
    streamerWanted = inPlay;
}
bool streamerOn() {
    return monitorStreaming();
}
uint32_t streamerPresents() {
    return streamerDraws.load();
}
uint32_t monitorPath() {
    return streamerPath.load();
}
void monitorSkip() {
    monitorFallBack(spidy::streamer_view::skippedAfterLoss);
}
bool deviceRemovedSeen() {
    return removedSeen.load() || fault.load() == 3301;
}
void wantScreen(bool wanted) {
    screenWanted = wanted;
}
bool screenRequested() {
    return screenWantedFlag();
}
bool latestScreen(Screen& out, uint64_t maxAgeMs) {
    std::lock_guard submitLock(submission);
    const auto now = GetTickCount64();
    if (!screen.texture || !screen.frame || now < screenMs || now - screenMs > maxAgeMs)
        return false;
    out = screen;
    return true;
}
// Probes only: reads the newest copy back to memory, waiting for the GPU.
DWORD readScreen() {
    std::lock_guard submitLock(submission);
    std::lock_guard recordLock(recording);
    if (!enabled || fault || !screen.texture || !screen.frame)
        return 3951;
    const auto desc = screen.texture->GetDesc();
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout{};
    UINT64 bytes{};
    device->GetCopyableFootprints(&desc, 0, 1, 0, &layout, nullptr, nullptr, &bytes);
    D3D12_HEAP_PROPERTIES heap{};
    heap.Type = D3D12_HEAP_TYPE_READBACK;
    D3D12_RESOURCE_DESC buffer{};
    buffer.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    buffer.Width = bytes;
    buffer.Height = 1;
    buffer.DepthOrArraySize = 1;
    buffer.MipLevels = 1;
    buffer.SampleDesc.Count = 1;
    buffer.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    ComPtr<ID3D12Resource> readback;
    if (FAILED(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &buffer, D3D12_RESOURCE_STATE_COPY_DEST,
                                               nullptr, IID_PPV_ARGS(&readback))))
        return 3952;
    auto* slot = readyCommands(screenCommands);
    if (!slot)
        return 3953;
    D3D12_RESOURCE_BARRIER b{};
    b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b.Transition = {screen.texture.Get(), D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_SOURCE};
    originalBarrier(slot->list.Get(), 1, &b);
    D3D12_TEXTURE_COPY_LOCATION src{}, dst{};
    src.pResource = screen.texture.Get();
    src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dst.pResource = readback.Get();
    dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    dst.PlacedFootprint = layout;
    slot->list->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
    std::swap(b.Transition.StateBefore, b.Transition.StateAfter);
    originalBarrier(slot->list.Get(), 1, &b);
    if (!submitCommands(*slot))
        return 3954;
    if (fence->GetCompletedValue() < fenceValue &&
        (FAILED(fence->SetEventOnCompletion(fenceValue, fenceEvent)) ||
         WaitForSingleObject(fenceEvent, 2000) != WAIT_OBJECT_0))
        return 3955;
    void* data{};
    if (FAILED(readback->Map(0, nullptr, &data)))
        return 3956;
    const auto rowBytes = static_cast<uint32_t>(desc.Width * (screen.view == DXGI_FORMAT_R16G16B16A16_FLOAT ? 8 : 4));
    screenPixels.resize(static_cast<size_t>(rowBytes) * desc.Height);
    for (UINT y = 0; y < desc.Height; ++y)
        std::memcpy(screenPixels.data() + static_cast<size_t>(y) * rowBytes,
                    static_cast<uint8_t*>(data) + layout.Offset + static_cast<size_t>(y) * layout.Footprint.RowPitch,
                    rowBytes);
    D3D12_RANGE written{0, 0};
    readback->Unmap(0, &written);
    publishScreen(1, screenPixels.data(), rowBytes);
    return 0;
}
void enterMarker(unsigned eye, uint64_t serial, uint64_t generation, bool ended) {
    currentMarker = {eye, {serial, generation}, ended, eye < 2};
}
void leaveMarker() {
    currentMarker = {};
}
void copyTiming(TimingMetric& gpu, TimingMetric& pairOut) {
    std::lock_guard lock(copyTimingLock);
    gpu = copyGpu;
    pairOut = copyPair;
}
uint32_t error() {
    return fault.load();
}
std::atomic<uint32_t> supersamplePercent{100};
bool setSupersample(uint32_t percent) {
    if (percent < minimumSupersample || percent > maximumSupersample || enabled)
        return false;
    supersamplePercent = percent;
    return true;
}
uint32_t supersample() {
    return supersamplePercent.load();
}
void setEyeOutput(uint32_t outputWidth, uint32_t outputHeight) {
    wantedOutWidth = outputWidth;
    wantedOutHeight = outputHeight;
}
bool setEyeAA(uint32_t mode) {
    if (mode > 1)
        return false;
    aaMode = staged ? mode : 0;
    return true;
}
// Update 15: the wrist hologram's GRAPHICS page reads what is in force.
void setSm1StyleLock(bool on) {
    if (sm1Lock.exchange(on) != on)
        OutputDebugStringA(on ? "SM1STYLE: our FXAA, EYE AA history and CAS sharpen locked off\n"
                              : "SM1STYLE: released; our eye passes follow their settings again\n");
}
bool sm1StyleLock() {
    return sm1Lock.load();
}
uint32_t eyeAA() {
    return aaMode.load();
}
uint32_t eyeHistory() {
    return historyPercent.load();
}
bool setEyeSharpen(uint32_t percent) {
    if (percent > eye_sharpen::maximumPercent || (percent && sharpenFault))
        return false;
    sharpenWanted = percent;
    sharpenPercent = staged ? percent : 0;
    if (!sharpenPercent)
        sharpening = false;
    return true;
}
bool setEyeDebug(uint32_t mode) {
    if (!eye_debug::validEyeDebug(mode))
        return false;
    eyeDebugMode = mode;
    return true;
}
uint32_t eyeDebug() {
    return eyeDebugMode.load();
}
uint32_t pairEyeDebug() {
    return pairDebugMode.load();
}
uint32_t eyeSharpen() {
    return sharpenPercent.load();
}
// Update 37.2: COLOR / ATMOSPHERE. Refused (false) once the pass could not be built.
bool setEyeColor(const eye_color::Config& c) {
    if (c.on && sharpenFault)
        return false;
    const auto p = eye_color::params(c);
    auto t = eye_color::toon(c);
    if (t.on > .5f && toonFault.load())
        t = {}; // Update 38.5: its shader failed: the grade alone (game_xr shows the notice)
    {
        std::lock_guard lock(colorLock);
        colorConfig = c;
        colorParams = p;
        colorToon = t;
    }
    toonOn = t.on > .5f;
    colorOn = c.on && (!eye_color::neutral(p) || t.on > .5f);
    return true;
}
uint32_t eyeToonFault() {
    return toonFault.load();
}
eye_color::Config eyeColor() {
    std::lock_guard lock(colorLock);
    return colorConfig;
}
bool eyeColorActive() {
    return colorOn.load();
}
uint64_t eyeColorPairs() {
    return colorPairs.load();
}
// Update 37.2: FOVEATED. False (and its fault set) when this GPU cannot or a hook failed; OFF
// always works. The pass hooks go in the first time a level is set.
// The pass hooks (lifecycle held): FOVEATED's two, and since 0.0.74 test 10 (SEE-THROUGH) put
// in with the eyes, where they only watch until foveaOn; and the depth clear's, read only.
bool passHooks() {
    if (foveaHooked)
        return true;
    if (!command)
        return false;
    auto vt = *reinterpret_cast<void***>(command.Get());
    foveaHooks = {vt[46], vt[21]}; // OMSetRenderTargets, RSSetViewports
    bool ok = true;
    if (!originalOmSetTargets)
        ok = MH_CreateHook(foveaHooks[0], reinterpret_cast<void*>(omSetTargets),
                           reinterpret_cast<void**>(&originalOmSetTargets)) == MH_OK;
    if (ok && !originalSetViewports)
        ok = MH_CreateHook(foveaHooks[1], reinterpret_cast<void*>(setViewports),
                           reinterpret_cast<void**>(&originalSetViewports)) == MH_OK;
    // Both or neither: the viewport check needs the pass hook and the reverse.
    ok = ok && MH_EnableHook(foveaHooks[0]) == MH_OK;
    if (ok && MH_EnableHook(foveaHooks[1]) != MH_OK) {
        MH_DisableHook(foveaHooks[0]);
        ok = false;
    }
    if (!ok)
        return false;
    foveaHooked = true;
    // SEE-THROUGH's convention check: without it no scene depth, everything else as before.
    if (!clearDepthHook) {
        if (MH_CreateHook(vt[47], reinterpret_cast<void*>(clearDepth), reinterpret_cast<void**>(&originalClearDepth)) ==
            MH_OK)
            clearDepthHook = vt[47];
    }
    clearDepthOn = clearDepthHook && originalClearDepth && MH_EnableHook(clearDepthHook) == MH_OK;
    if (!clearDepthOn)
        depthStop(4);
    return true;
}
// 0.0.74 test 10 SEE-THROUGH: the scene depth of exactly this pair, both eyes.
bool sceneDepth(uint64_t serial, uint64_t generation, SceneDepth& out) {
    out = {};
    if (!enabled || !staged || depthOff.load() || !foveaHooked.load() || !clearDepthOn.load() || !serial)
        return false;
    const EyePairState::Stamp want{serial, generation};
    std::lock_guard lock(depthLock);
    for (unsigned eye = 0; eye < 2; ++eye) {
        for (const auto& slot : depthRing[eye])
            if (slot.texture && slot.executed && !slot.pending && slot.stamp == want) {
                out.eyes[eye] = slot.texture;
                break;
            }
        if (!out.eyes[eye]) {
            out = {};
            ++depthMissed;
            return false;
        }
    }
    out.width = depthW;
    out.height = depthH;
    ++depthGiven;
    return true;
}
std::string sceneDepthLine() {
    try {
        const auto n = [](const std::atomic<uint64_t>& v) { return std::to_string(v.load()); };
        std::string eyes;
        {
            std::lock_guard recordLock(recording);
            eyes = std::to_string(width) + "x" + std::to_string(height);
        }
        return "0.0.74 test 10 SEE-THROUGH depth: captured " + n(depthCaptured) + ", skipped " +
               std::to_string(depthSkipSize + depthSkipFormat + depthSkipNoPass + depthSkipQueue + depthSkipLost) +
               " (size " + n(depthSkipSize) + ", format " + n(depthSkipFormat) + ", no depth out of the pass " +
               n(depthSkipNoPass) + ", other queue " + n(depthSkipQueue) + ", list lost " + n(depthSkipLost) +
               "), format " + std::to_string(depthSeenFormat.load()) + " " + std::to_string(depthSeenW.load()) + "x" +
               std::to_string(depthSeenH.load()) + " (eyes " + eyes +
               "), clears reversed " + n(depthClearReversed) + " / standard " + n(depthClearStandard) + " / other " +
               n(depthClearOther) + ", pairs given " + n(depthGiven) + " / missed " + n(depthMissed) + ", hooks " +
               (foveaHooked.load() ? "pass" : "none") + (clearDepthOn.load() ? "+clear" : "") + ", off " +
               std::to_string(depthOff.load());
    } catch (...) {
        return {};
    }
}
bool setFoveation(uint32_t level, uint32_t inner, bool debug) {
    if (level >= foveation::levels)
        return false;
    foveaInner = std::clamp(inner, foveation::innerMin, foveation::innerMax);
    foveaDebug = debug;
    foveaLevel = level;
    if (level == foveation::off) {
        foveaOn = false;
        return true;
    }
    std::lock_guard life(lifecycle);
    if (!enabled || !device || !command || !staged)
        return false; // the headset's eye copy is not running: nothing to foveate
    if (foveaFault)
        return false;
    if (!foveaTier) {
        D3D12_FEATURE_DATA_D3D12_OPTIONS6 o6{};
        if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS6, &o6, sizeof(o6))) ||
            o6.VariableShadingRateTier < D3D12_VARIABLE_SHADING_RATE_TIER_2 || !o6.ShadingRateImageTileSize) {
            foveaTier = 1;
            stopFovea(foveation::noTier2);
            return false;
        }
        foveaTier = 2;
        foveaTile = o6.ShadingRateImageTileSize;
        foveaHas4x4 = o6.AdditionalShadingRatesSupported ? 1u : 0u;
    }
    if (foveaTier != 2)
        return false;
    if (!passHooks()) {
        stopFovea(foveation::hookFailed);
        return false;
    }
    foveaOn = true;
    return true;
}
FoveaStatus foveaStatus() {
    FoveaStatus f;
    f.level = foveaLevel.load();
    f.inner = foveaInner.load();
    f.debug = foveaDebug.load();
    f.on = foveaOn.load();
    f.fault = foveaFault.load();
    f.tier = foveaTier.load();
    f.tile = foveaTile.load();
    f.has4x4 = foveaHas4x4.load() != 0;
    f.shadedShare = foveaShare.load();
    f.taggedPasses = foveaTagged.load();
    f.appliedPasses = foveaApplied.load();
    f.untaggedPasses = foveaUntagged.load();
    f.resets = foveaResets.load();
    f.images = foveaImageCount.load();
    f.mismatches = foveaMismatches.load();
    for (unsigned i = 0; i < 4; ++i)
        f.centres[i] = foveaCentres[i].load();
    return f;
}
uint32_t eyeSharpenFault() {
    return sharpenFault.load();
}
void eyeSharpenTiming(TimingMetric& gpu, uint32_t& percent, uint32_t& code, uint64_t& sharpened) {
    {
        std::lock_guard lock(copyTimingLock);
        gpu = sharpenGpu;
    }
    percent = sm1Lock.load() ? 0u : sharpenPercent.load();
    code = sharpenFault.load();
    sharpened = sharpenPairs.load();
}
float eyePassesGpuMs() {
    const uint64_t now = GetTickCount64();
    float total = 0;
    bool allKnown = true, any = false;
    const auto add = [&](bool on, const PassClock& clock) {
        if (!on)
            return;
        any = true;
        const uint64_t tick = clock.tick.load();
        if (!tick || now - tick > 1000)
            allKnown = false;
        else
            total += clock.ms.load();
    };
    add(staged && !aaFault.load() && (downsampling() || (aaMode.load() == 1 && !sm1Lock.load())), aaClock);
    add(staged && !historyFault.load() && historyNow(), historyClock);
    add(sharpening.load(), sharpenClock);
    return !any ? 0.f : allKnown ? total : std::numeric_limits<float>::quiet_NaN();
}
uint32_t monitorRateNow() {
    return mirrorRate.load();
}
bool setEyeHistory(uint32_t percent) {
    if (percent > eye_history::maximumPercent)
        return false;
    historyPercent = percent;
    std::lock_guard lock(copyTimingLock);
    SpidyEyeHistoryData.percent = percent;
    return true;
}
bool setEyeTrail(uint32_t percent) {
    if (percent > eye_history::maximumTrail)
        return false;
    trailPercent = percent;
    std::lock_guard lock(copyTimingLock);
    SpidyEyeHistoryData.trail = percent;
    return true;
}
// 0.0.73 test 3 LIGHT HOLD: 0 off (test 2), 50 ON, 100 STRONG.
bool setEyeLightHold(uint32_t percent) {
    if (percent > eye_history::lightHoldStrong)
        return false;
    lightHoldPercent = percent;
    return true;
}
bool setEyeHistoryCut(uint32_t percent) {
    if (percent < eye_history::minimumCut || percent > eye_history::maximumCut)
        return false;
    cutPercent = percent;
    return true;
}
uint32_t eyeTrail() {
    return trailPercent.load();
}
bool setEyeTemporal(bool on) {
    if (on && historyFault.load())
        return false;
    temporalOn = on;
    return true;
}
bool eyeTemporal() {
    return temporalOn.load();
}
uint32_t eyeHistoryFault() {
    return historyFault.load();
}
float eyeHistoryMs() { // Update 35: the median of the last 31 passes (was the newest)
    const uint64_t tick = historyClock.tick.load();
    if (!tick || GetTickCount64() - tick > 1000)
        return std::numeric_limits<float>::quiet_NaN();
    std::lock_guard lock(historyMedianLock);
    return historyMedian.median();
}
void noteEyeViews(uint64_t serial, const std::array<eye_history::EyeView, 2>& views) {
    std::lock_guard lock(viewLock);
    notedViews[nextNoted] = {serial, views};
    nextNoted = (nextNoted + 1) % notedViews.size();
    // Update 37.2: each lens's centre for FOVEATED (its straight-ahead point in the eye image).
    for (unsigned i = 0; i < 2; ++i) {
        const auto& t = views[i].tangents; // left, right, down, up
        const auto c = foveation::lensCentre(t[0], t[1], t[3], t[2]);
        foveaCentres[2 * i] = c.u;
        foveaCentres[2 * i + 1] = c.v;
    }
}
void eyeAATiming(TimingMetric& gpu, uint32_t& mode, uint32_t& code, uint64_t& smoothed) {
    {
        std::lock_guard lock(copyTimingLock);
        gpu = aaGpu;
    }
    // 2: the supersampled eyes' downsample filter, whatever FXAA is set to.
    mode = width && downsampling() ? 2 : sm1Lock.load() ? 0u : aaMode.load();
    code = aaFault.load();
    smoothed = aaPairs.load();
}
eye_swap::Desc swapDesc(const D3D12_RESOURCE_DESC& d) {
    return {static_cast<uint32_t>(d.Dimension), d.Width, d.Height, d.DepthOrArraySize, d.MipLevels,
            static_cast<uint32_t>(d.Format), d.SampleDesc.Count, static_cast<uint32_t>(d.Flags)};
}
uint32_t eyeSwaps() {
    return eyeSwapCount.load();
}
uint32_t eyesLost() {
    return eyesLostCode.load();
}
void registerEye(unsigned eye, ID3D12Resource* resource) {
    if (!enabled || eye >= 2 || !resource)
        return;
    std::lock_guard lock(recording);
    if (resources[eye].Get() == resource)
        return;
    if (eyesLostCode.load())
        return;
    ComPtr<ID3D12Device> owner;
    ComPtr<IUnknown> a, b;
    const bool sameDevice = SUCCEEDED(resource->GetDevice(IID_PPV_ARGS(&owner))) && SUCCEEDED(owner.As(&a)) &&
                            SUCCEEDED(device.As(&b)) && a.Get() == b.Get();
    if (resources[eye]) {
        // Update 32.1: the game rebuilt its render targets (a graphics
        // setting changed). Before, 3501 ended VR; now the new image is
        // taken when it is the same kind, else the eyes pause (3503).
        if (!sameDevice || !eye_swap::compatible(swapDesc(resources[eye]->GetDesc()), swapDesc(resource->GetDesc()))) {
            eyesLostCode = eye_swap::lostCode;
            copyReady.notify_all();
            return;
        }
        retiredEyes.push_back({resources[eye], fenceValue});
        resources[eye] = resource;
        watched[eye] = resource;
        known[eye] = false;
        eyeViewStale[eye] = true;
        ++eyeSwapCount;
        return;
    }
    if (!sameDevice) {
        fault = 3502;
        return;
    }
    resources[eye] = resource;
    watched[eye] = resource;
}
// 0.0.71 MIRROR VIEW: take (or, with nullptr, let go of) the mirror view's image. The old one is
// kept referenced until the bridge's GPU work already submitted has finished, like a swapped eye.
void registerMirror(ID3D12Resource* resource) {
    std::lock_guard lock(recording);
    if (mvResource.Get() == resource)
        return;
    if (mvResource)
        retiredEyes.push_back({mvResource, fenceValue});
    mvResource = nullptr;
    mvWatched = nullptr;
    mvKnown = false;
    if (!resource || !enabled || mvFault.load())
        return;
    ComPtr<ID3D12Device> owner;
    ComPtr<IUnknown> a, b;
    if (!device || FAILED(resource->GetDevice(IID_PPV_ARGS(&owner))) || FAILED(owner.As(&a)) ||
        FAILED(device.As(&b)) || a.Get() != b.Get()) {
        mvFault = 5;
        return;
    }
    mvResource = resource;
    mvWatched = resource;
}
MirrorGpu mirrorGpu() {
    return {mvFault.load(), mvResource ? 1u : 0u, mvKnown ? 1u : 0u, mvCopies.load()};
}
bool arm(uint64_t serial, ID3D12Resource* left, ID3D12Resource* right) {
    std::lock_guard submitLock(submission);
    std::lock_guard recordLock(recording);
    if (!enabled || !copyToXr || staged || fault || eyesLostCode.load() || !serial || requestedSerial ||
        GetTickCount64() >= deadline)
        return false;
    ID3D12Resource* incoming[] = {left, right};
    for (unsigned i = 0; i < 2; ++i) {
        if (!incoming[i]) {
            fault = 3701;
            return false;
        }
        ComPtr<ID3D12Device> owner;
        ComPtr<IUnknown> a, b;
        if (FAILED(incoming[i]->GetDevice(IID_PPV_ARGS(&owner))) || FAILED(owner.As(&a)) ||
            FAILED(device.As(&b)) || a.Get() != b.Get()) {
            fault = 3702;
            return false;
        }
        const auto desc = incoming[i]->GetDesc();
        if (!rgba8CopyTarget(desc)) {
            fault = 3703;
            return false;
        }
        if (resources[i]) {
            const auto source = resources[i]->GetDesc();
            if (source.Width != desc.Width || source.Height != desc.Height ||
                resources[i].Get() == incoming[i]) {
                fault = 3704;
                return false;
            }
        }
    }
    targets = {left, right};
    requestedSerial = serial;
    requestFence = 0;
    return true;
}
bool copyLatest(ID3D12Resource* left, ID3D12Resource* right, uint64_t& serial, uint64_t& generation) {
    serial = generation = 0;
    std::lock_guard submitLock(submission);
    std::lock_guard recordLock(recording);
    if (!enabled || !staged || fault || eyesLostCode.load() || !capturedStamp.serial ||
        GetTickCount64() >= deadline || GetTickCount64() - capturedMs > imageHoldMs)
        return false;
    ID3D12Resource* incoming[] = {left, right};
    for (unsigned i = 0; i < 2; ++i) {
        if (!incoming[i]) {
            fault = 3701;
            return false;
        }
        const auto desc = incoming[i]->GetDesc();
        if (!rgba8CopyTarget(desc) || desc.Width != outWidth || desc.Height != outHeight) {
            fault = 3703;
            return false;
        }
        ComPtr<ID3D12Device> owner;
        ComPtr<IUnknown> a, b;
        if (FAILED(incoming[i]->GetDevice(IID_PPV_ARGS(&owner))) || FAILED(owner.As(&a)) ||
            FAILED(device.As(&b)) || a.Get() != b.Get()) {
            fault = 3702;
            return false;
        }
    }
    auto* slot = readyCommands(presentCommands);
    if (!slot)
        return false;
    const auto slotIndex = static_cast<size_t>(slot - presentCommands.data());
    noteSharpenTime(slotIndex);
    // Update 16: the headset's images sharpened (in place of the copy), or
    // copied as before when sharpening is off or cannot run.
    // 0.0.74 test 10 SM1STYLE: no CAS while it runs (the pass still grades COLOR / the fovea test, strength 0).
    const uint32_t percent = sm1Lock.load() ? 0u : sharpenPercent.load();
    // Update 24: read once, so both images of this pair follow the same mode.
    const uint32_t debug = eyeDebugMode.load();
    // Update 37.2: the pass also runs for COLOR alone (sharpening 0).
    const bool graded = staged && (colorOn.load() || // not in the screen-copy mode (as sharpening)
                                   (foveaDebug.load() && foveaLevel.load() != foveation::off));
    const bool sharpened = (percent || graded) && prepareSharpen() &&
                           drawSharpen(slot->list.Get(), slotIndex, incoming, percent, debug);
    for (unsigned i = 0; i < 2; ++i) {
        if (!sharpened)
            copyEyeTexture(slot->list.Get(), stagedEyes[eye_debug::eyeDebugSource(debug, i)].Get(),
                           D3D12_RESOURCE_STATE_COPY_SOURCE, incoming[i], originalBarrier);
        if (captureImages)
            captureImage(slot->list.Get(), incoming[i], i);
    }
    if (!submitCommands(*slot)) {
        if (sharpened)
            sharp.timed[slotIndex] = false;
        return false;
    }
    sharpening = sharpened;
    sharpenPairs += sharpened;
    colorPairs += sharpened && graded;
    if (pendingCount == pendingCopies.size()) { // not seen finishing: drop the oldest
        std::move(pendingCopies.begin() + 1, pendingCopies.end(), pendingCopies.begin());
        --pendingCount;
    }
    pendingCopies[pendingCount++] = {slot->fence, qpcNow(), capturedQpc};
    reusedPairs += capturedStamp.generation == lastGeneration;
    lastSerial = serial = capturedStamp.serial;
    lastGeneration = generation = capturedStamp.generation;
    pairDebugMode = debug;
    ++pairs;
    publish();
    return true;
}
void keepShownEye(ID3D12Resource* left) {
    keepShownEye(left, nullptr, nullptr);
}
void keepShownEye(ID3D12Resource* left, const float orientation[4], const float fov[4]) {
    // Update 16: while the headset's images are sharpened, the window shows
    // stagedEyes[0] (before sharpening, without Spidy's drawings): no copy.
    // 0.0.74 test 10 MONITOR: the copy is made anyway (the headset's own image), and
    // also while the game renders its own view (the copy goes over it).
    const bool streaming = monitorStreaming();
    if (!left || !(mirrorWanted || streaming) || screenWanted || !shownWanted || (sharpening && !streaming))
        return;
    std::lock_guard submitLock(submission);
    std::lock_guard recordLock(recording);
    const auto nowMs = GetTickCount64();
    if (streaming && !mirrorWanted && (fault || eyesLostCode.load()))
        monitorFallBack(spidy::streamer_view::deviceLost); // the window keeps the game's own view
    if (!enabled || !staged || fault || shownFailed || !stagedEyes[0] || nowMs >= deadline)
        return;
    // Over the game's own view every present reaches the window: copies stay at the
    // monitor's rate, not one each frame.
    if (!mirrorWanted) {
        const auto rate = mirrorRate.load();
        if (rate && shownMs && nowMs >= shownMs && nowMs - shownMs < 1000 / rate)
            return;
    }
    const auto desc = left->GetDesc();
    if (!rgba8CopyTarget(desc)) {
        if (streaming && !mirrorWanted)
            monitorFallBack(spidy::streamer_view::noCopy); // the headset's image cannot be copied
        return;
    }
    if (desc.Width != outWidth || desc.Height != outHeight)
        return; // eyes being rebuilt (a graphics setting): the next frame's
    if (!shownEye) {
        const auto own = stagedEyes[0]->GetDesc();
        D3D12_HEAP_PROPERTIES heap{};
        heap.Type = D3D12_HEAP_TYPE_DEFAULT;
        // Without it the window goes on with the game's own image of the eye.
        if (FAILED(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &own,
                                                   D3D12_RESOURCE_STATE_COPY_SOURCE, nullptr,
                                                   IID_PPV_ARGS(&shownEye)))) {
            shownFailed = true; // not tried again every frame
            if (streaming && !mirrorWanted)
                monitorFallBack(spidy::streamer_view::noCopy);
            return;
        }
    }
    auto* slot = readyCommands(shownCommands);
    if (!slot)
        return; // earlier copies still in flight: the next frame's image instead
    copyEyeTexture(slot->list.Get(), left, D3D12_RESOURCE_STATE_RENDER_TARGET, shownEye.Get(),
                   originalBarrier, D3D12_RESOURCE_STATE_COPY_SOURCE);
    if (!submitCommands(*slot))
        return;
    shownMs = GetTickCount64();
    shownWanted = false;
    shownView = {};
    shownView.streamer = streaming;
    if (orientation && fov) {
        shownView.orientation = {orientation[0], orientation[1], orientation[2], orientation[3]};
        shownView.fov = {fov[0], fov[1], fov[2], fov[3]};
        shownView.valid = spidy::streamer_view::usable(shownView.fov) &&
                          std::isfinite(orientation[0] + orientation[1] + orientation[2] + orientation[3]);
    }
}
bool waitCopy(uint64_t serial, uint32_t timeoutMs) {
    std::unique_lock lock(submission);
    copyReady.wait_for(lock, std::chrono::milliseconds(std::min(timeoutMs, 500u)),
                       [&] { return requestedSerial != serial || requestFence || fault || eyesLostCode.load() || !enabled; });
    if (requestedSerial != serial)
        return false;
    const bool submitted = requestFence != 0;
    // Copies and overlay are submitted to the exact direct queue bound to
    // OpenXR. Queue order supplies the dependency at image release. Allocator
    // reuse still checks the GPU fence; the CPU need not drain the game queue.
    requestedSerial = requestFence = 0;
    targets = {};
    return submitted;
}
DWORD start(void* input) {
    std::lock_guard life(lifecycle);
    Config c{};
    if (enabled || hooked)
        return 1000;
    if (!read(reinterpret_cast<uintptr_t>(input), &c, sizeof(c)) || c.magic != 0x53475043 || c.version != 3 ||
        c.bytes != sizeof(c) || c.pid != GetCurrentProcessId() ||
        c.base != reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr)) ||
        !GetModuleHandleW(L"Spider-Man2.exe") || (c.durationMs && c.durationMs < 500) ||
        c.durationMs > 30000 || c.copyToXr > 2 || c.captureImages > 1 || c.eyeAA > 1 ||
        (!c.copyToXr && !c.captureImages) || !c.queue)
        return 1001;
    copyToXr = c.copyToXr != 0;
    staged = c.copyToXr == 2;
    captureImages = c.captureImages != 0;
    // Only the staged images (copyToXr 2) pass through the AA pass.
    aaMode = staged ? c.eyeAA : 0;
    // Update 38: the sharpening asked for before the start, in force now.
    sharpenPercent = staged && !sharpenFault.load() ? sharpenWanted.load() : 0u;
    pendingCount = 0; // a new fence starts at 0
    queue = reinterpret_cast<ID3D12CommandQueue*>(c.queue);
    if (queue->GetDesc().Type != D3D12_COMMAND_LIST_TYPE_DIRECT ||
        FAILED(queue->GetDevice(IID_PPV_ARGS(&device))))
        return 1002;
    if (FAILED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator))) ||
        FAILED(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.Get(), nullptr,
                                         IID_PPV_ARGS(&command))) ||
        FAILED(command->Close()) ||
        FAILED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence))))
        return 1003;
    fenceEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!fenceEvent)
        return 1004;
    auto vt = *reinterpret_cast<void***>(command.Get());
    hooks = {vt[26], vt[10], (*reinterpret_cast<void***>(queue.Get()))[10], vt[53]}; // EndQuery
    auto s = MH_Initialize();
    if (s != MH_OK && s != MH_ERROR_ALREADY_INITIALIZED)
        return 1100 + s;
    s = MH_CreateHook(hooks[0], reinterpret_cast<void*>(barrier), reinterpret_cast<void**>(&originalBarrier));
    if (s == MH_OK)
        s = MH_CreateHook(hooks[1], reinterpret_cast<void*>(reset), reinterpret_cast<void**>(&originalReset));
    if (s == MH_OK)
        s = MH_CreateHook(hooks[2], reinterpret_cast<void*>(execute),
                          reinterpret_cast<void**>(&originalExecute));
    if (s == MH_OK)
        s = MH_CreateHook(hooks[3], reinterpret_cast<void*>(endQuery),
                          reinterpret_cast<void**>(&originalEndQuery));
    if (s != MH_OK) {
        for (auto h : hooks)
            MH_RemoveHook(h);
        return 1200 + s;
    }
    hooked = true;
    deadline = c.durationMs ? GetTickCount64() + c.durationMs : UINT64_MAX;
    enabled = true;
    for (auto h : hooks)
        if ((s = MH_EnableHook(h)) != MH_OK) {
            enabled = false;
            for (auto t : hooks)
                MH_DisableHook(t);
            return 1300 + s;
        }
    // 0.0.74 test 10 SEE-THROUGH: the passive pass hooks, with the headset's eyes only; when they
    // fail, only the scene depth is missing (FOVEATED tries again when turned on).
    {
        std::lock_guard depth(depthLock); // a new start: nothing of the last one's lists
        for (auto& eye : depthRing)
            eye = {};
        depthNext = {};
        depthW = depthH = 0;
        retiredDepth.clear();
        depthOff = 0;
    }
    if (staged && !passHooks())
        depthStop(4);
    // The game's presented frame is optional: without it the headset shows
    // nothing while gameplay is unavailable, as before.
    void* presentAddress{};
    void* present1Address{};
    if (presentEntries(presentAddress, present1Address) &&
        MH_CreateHook(presentAddress, reinterpret_cast<void*>(present), reinterpret_cast<void**>(&originalPresent)) ==
            MH_OK) {
        presentHooks[0] = presentAddress;
        if (MH_CreateHook(present1Address, reinterpret_cast<void*>(present1),
                          reinterpret_cast<void**>(&originalPresent1)) == MH_OK)
            presentHooks[1] = present1Address;
        bool hookedPresent = true;
        for (auto h : presentHooks)
            if (h)
                hookedPresent = MH_EnableHook(h) == MH_OK && hookedPresent;
        publishScreen(hookedPresent ? 1 : 2, nullptr, 0);
    }
    std::lock_guard lock(telemetry);
    InterlockedIncrement64(&SpidyGpuData.sequence);
    SpidyGpuData.status = 1;
    InterlockedIncrement64(&SpidyGpuData.sequence);
    return 0;
}
bool readPixels() { // submission and recording held by caller; GPU work complete
    const UINT imageW = imageWidth(), imageH = imageHeight();
    for (unsigned i = 0; i < 2; ++i) {
        void* data{};
        const auto& layout = layouts[i];
        // The last row has no pitch padding: rows whose bytes are not a
        // multiple of 256 make the buffer shorter than RowPitch * imageH.
        D3D12_RANGE range{0, static_cast<SIZE_T>(layout.Offset + static_cast<UINT64>(layout.Footprint.RowPitch) *
                                                                       (imageH - 1) +
                                                 imageW * 4)};
        if (FAILED(readbacks[i]->Map(0, &range, &data)))
            return false;
        for (unsigned y = 0; y < imageH; ++y)
            std::memcpy(pixels[i].data() + static_cast<size_t>(y) * imageW * 4,
                        static_cast<uint8_t*>(data) + layout.Offset +
                            static_cast<size_t>(y) * layout.Footprint.RowPitch,
                        imageW * 4);
        D3D12_RANGE written{0, 0};
        readbacks[i]->Unmap(0, &written);
    }
    return true;
}
DWORD freeze() {
    std::lock_guard life(lifecycle);
    std::lock_guard submitLock(submission);
    std::lock_guard recordLock(recording);
    if (!enabled || !captureImages || staged || copyToXr)
        return 3901;
    if (!pairs || !readbacks[0] || !readbacks[1])
        return 3902;
    if (fence->GetCompletedValue() < fenceValue &&
        (FAILED(fence->SetEventOnCompletion(fenceValue, fenceEvent)) ||
         WaitForSingleObject(fenceEvent, 1000) != WAIT_OBJECT_0))
        return 3903;
    if (!readPixels())
        return 3904;
    publish();
    return 0;
}
DWORD stop() {
    std::lock_guard life(lifecycle);
    enabled = false;
    screenWanted = false;
    copyReady.notify_all();
    foveaOn = false;
    if (hooked) {
        for (auto h : hooks)
            MH_DisableHook(h);
        for (auto h : presentHooks)
            if (h)
                MH_DisableHook(h);
    }
    if (foveaHooked)
        for (auto h : foveaHooks)
            if (h)
                MH_DisableHook(h);
    foveaHooked = false;
    if (clearDepthHook) // 0.0.74 test 10 SEE-THROUGH
        MH_DisableHook(clearDepthHook);
    clearDepthOn = false;
    std::lock_guard submitLock(submission);
    std::lock_guard recordLock(recording);
    if (fence && fence->GetCompletedValue() < fenceValue) {
        if (FAILED(fence->SetEventOnCompletion(fenceValue, fenceEvent)) ||
            WaitForSingleObject(fenceEvent, 5000) != WAIT_OBJECT_0)
            fault = 3601;
    }
    if (fence && (fence->GetCompletedValue() == UINT64_MAX || fence->GetCompletedValue() < fenceValue))
        fault = 3601;
    if (pairs && captureImages && !fault && !readPixels())
        fault = 3602;
    publish();
    std::lock_guard lock(telemetry);
    InterlockedIncrement64(&SpidyGpuData.sequence);
    SpidyGpuData.status = fault ? 3 : 2;
    InterlockedIncrement64(&SpidyGpuData.sequence);
    return fault.load();
}
} // namespace spidy::native_gpu
extern "C" __declspec(dllexport) DWORD WINAPI SpidyGpuStart(void* input) {
    return native_gpu::start(input);
}
extern "C" __declspec(dllexport) DWORD WINAPI SpidyGpuStop(void*) {
    return native_gpu::stop();
}
// Diagnostic: how many times the game presented since the GPU bridge started
// (Present and Present1 each count); with 1 how many presents got the left
// eye on the game's window, and with 3 how many of them got it as the headset
// showed it; with 2 how many presents were held back from the window.
extern "C" __declspec(dllexport) DWORD WINAPI SpidyPresentCount(void* which) {
    switch (reinterpret_cast<uintptr_t>(which)) {
    case 0:
        return native_gpu::presents();
    case 2:
        return native_gpu::heldPresents();
    case 3:
        return native_gpu::mirroredShown();
    default:
        return native_gpu::mirrored();
    }
}
// While the window shows the left eye, at most this many of the game's
// presents a second reach it (1-240; 0: every one). Takes effect at once.
extern "C" __declspec(dllexport) DWORD WINAPI SpidyMonitorRate(void* rate) {
    const auto value = reinterpret_cast<uintptr_t>(rate);
    if (!spidy::validMonitorRate(static_cast<uint32_t>(std::min<uintptr_t>(value, UINT32_MAX))))
        return 3911;
    native_gpu::monitorRate(static_cast<uint32_t>(value));
    return 0;
}
// Eye anti-aliasing now (live.txt "aa"): 0 off, 1 FXAA. Takes effect with the
// next eye pair; refused (3960) for another value.
extern "C" __declspec(dllexport) DWORD WINAPI SpidyEyeAA(void* mode) {
    return native_gpu::setEyeAA(static_cast<uint32_t>(std::min<uintptr_t>(reinterpret_cast<uintptr_t>(mode), 2)))
               ? 0
               : 3960;
}
// Update 37: AA QUALITY (live.txt aa_quality): 0 FXAA as before, 1 STRONG (FXAA's
// strongest thresholds). Next eye pair; refused (3997) above.
extern "C" __declspec(dllexport) DWORD WINAPI SpidyEyeAAQuality(void* quality) {
    const auto value = reinterpret_cast<uintptr_t>(quality);
    if (value > 1)
        return 3997;
    native_gpu::aaQuality = static_cast<uint32_t>(value);
    return 0;
}
// 0.0.73 test 3 LIGHT HOLD (combat38 light_hold, eye_history.hpp): the history shader's bright hold,
// 0 off, 50 ON (default), 100 STRONG; refused (3996) past 100.
extern "C" __declspec(dllexport) DWORD WINAPI SpidyEyeLightHold(void* percent) {
    return native_gpu::setEyeLightHold(
               static_cast<uint32_t>(std::min<uintptr_t>(reinterpret_cast<uintptr_t>(percent), 1000)))
               ? 0
               : 3996;
}
// JITTER CALM: the share of its still-head history the last pass kept (0..1; 0 while none was kept or none ran).
extern "C" float SpidyEyeHistoryKept() {
    return static_cast<float>(native_gpu::keptMilli.load(std::memory_order_relaxed)) / 1000.f;
}
// Update 37: HISTORY CUT (live.txt history_cut, eye_history::cutMotion), 25-400 percent;
// refused (3998) outside.
extern "C" __declspec(dllexport) DWORD WINAPI SpidyEyeHistoryCut(void* percent) {
    return native_gpu::setEyeHistoryCut(
               static_cast<uint32_t>(std::min<uintptr_t>(reinterpret_cast<uintptr_t>(percent), 1000)))
               ? 0
               : 3998;
}
// Eye history (live.txt history): this percent of each pair is the pair
// before it, turned to the new view (0 off, at most 95 since Update 21, the
// clamp widened from 50 on); refused (3962) above.
extern "C" __declspec(dllexport) DWORD WINAPI SpidyEyeHistory(void* percent) {
    return native_gpu::setEyeHistory(
               static_cast<uint32_t>(std::min<uintptr_t>(reinterpret_cast<uintptr_t>(percent), 1000)))
               ? 0
               : 3962;
}
// Update 34.2: EYE AA TRAIL (live.txt trail, the wrist's EYE AA TRAIL): the percent of
// the history kept at swing speed, 0-100; refused (3994) above.
extern "C" __declspec(dllexport) DWORD WINAPI SpidyEyeTrail(void* percent) {
    return native_gpu::setEyeTrail(
               static_cast<uint32_t>(std::min<uintptr_t>(reinterpret_cast<uintptr_t>(percent), 1000)))
               ? 0
               : 3994;
}
// Update 16: eye sharpening (live.txt sharpen): 0 off (no pass), 1-100 the
// headset's images sharpened (CAS), 101-200 (Update 21) the strong mode past
// CAS's top (eye_sharpen.hpp boost); refused (3963) above 200 or once the pass
// could not be built (its code: SpidyXrTimingData's eyeSharpenFault).
extern "C" __declspec(dllexport) DWORD WINAPI SpidyEyeSharpen(void* percent) {
    return native_gpu::setEyeSharpen(
               static_cast<uint32_t>(std::min<uintptr_t>(reinterpret_cast<uintptr_t>(percent), 1000)))
               ? 0
               : 3963;
}
// Update 24: live.txt eye_debug (eye_debug.hpp): 0 off, 1 both headset images
// from the left eye's picture (a flicker test; the world looks flat). Only the
// copy to the headset changes; refused (3991) for anything else. (Update 26: Update
// 25's 2, HEAD SHADOWS, is refused again: it never took effect in SM2.)
extern "C" __declspec(dllexport) DWORD WINAPI SpidyEyeDebug(void* mode) {
    return native_gpu::setEyeDebug(
               static_cast<uint32_t>(std::min<uintptr_t>(reinterpret_cast<uintptr_t>(mode), 1000)))
               ? 0
               : 3991;
}
// Eye supersampling (launcher -Supersample): the eyes are rendered at this
// percent of the headset's eye size, per side (100-200; 100 none), and filtered
// down to it. Only before SpidyXrStart; refused (3961) otherwise.
extern "C" __declspec(dllexport) DWORD WINAPI SpidyEyeSupersample(void* percent) {
    return native_gpu::setSupersample(
               static_cast<uint32_t>(std::min<uintptr_t>(reinterpret_cast<uintptr_t>(percent), 1000)))
               ? 0
               : 3961;
}
extern "C" __declspec(dllexport) DWORD WINAPI SpidyGpuFreeze(void*) {
    return native_gpu::freeze();
}
// Probes: 0 stops and 1 starts copying the game's presented frame; 2 reads
// the newest copy back into SpidyScreenData.
extern "C" __declspec(dllexport) DWORD WINAPI SpidyGpuScreen(void* mode) {
    const auto value = reinterpret_cast<uintptr_t>(mode);
    if (value > 2)
        return 3950;
    if (value < 2) {
        native_gpu::wantScreen(value == 1);
        return 0;
    }
    return native_gpu::readScreen();
}
