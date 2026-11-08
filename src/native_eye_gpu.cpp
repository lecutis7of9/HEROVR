#include "spidy/native_eye_gpu.hpp"
#include "spidy/native_address_finder.hpp"
#include "spidy/native_hud.hpp"
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
#include "spidy/scene_depth_match.hpp"
#include "spidy/streamer_view.hpp"
#include "spidy/vr_aa.hpp"
#include "spidy/taa_watch.hpp"
#include "spidy/taa_depth.hpp"
#include "spidy/taa_depth_check.hpp"
#include "spidy/taa_depth_trace.hpp"
#include "spidy/near_clip.hpp"
#include "spidy/taa_frame_log.hpp"
#include "spidy/taa_jitter.hpp"
#include "spidy/taa_still.hpp"
#include "spidy/ui_dump.hpp"
#include "spidy/prompt_layer.hpp"
#include "spidy/smaa/area_tex.h"
#include "spidy/smaa/search_tex.h"
#include "spidy/smaa/smaa_shader.hpp"
#include <MinHook.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdio>
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
__declspec(dllexport) vr_aa::Data SpidyVrAaData; // 0.0.76 SPIDY TAA (copyTimingLock)
__declspec(dllexport) taa_watch::Data SpidyTaaWatchData; // 0.0.76 TAA WATCH (watchLock)
__declspec(dllexport) taa_frame_log::Data SpidyTaaFrameLogData; // 0.0.76 TAA FRAME LOG (watchLock)
// 0.0.76 A-depth CHECKED (taa_depth_check.hpp): the depth convention check and the depth path's 10 s log (watchLock).
__declspec(dllexport) taa_depth_check::Data SpidyTaaDepthCheckData;
// 0.0.76 A-depth TRACE (taa_depth_trace.hpp, read-only): per-eye depth, history age, graze bins, hitches (watchLock).
__declspec(dllexport) taa_depth_trace::Data SpidyTaaDepthTraceData;
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
// HERO VR 0.0.77 PROMPT WINDOW: the copies are wanted only for the window over the eyes (VR play): the
// mirror, the monitor's rate hold and the shown eye's copy go on as without them, and the game's own view
// keeps its lens (screenRequested stays false).
std::atomic<bool> screenInsetOnly{};
bool screenWantedFlag() {
    return screenWanted.load() && !screenInsetOnly.load();
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
};
thread_local FoveaTag foveaTag{};
// 0.0.75 SEE-THROUGH: an eye job's list, by the list (0.0.74 kept this per thread, and test 10
// showed the game records the job's passes on another thread than its begin timestamp: no pass
// was ever matched). The job's pair stamp; whether its G-buffer pass was seen (and its depth
// view), whether its depth was dealt with (copied or skipped), and the depth view last cleared
// on it with its value (the convention check). `window`: tagged by the only eye job open when
// its G-buffer pass was recorded, not by a marker on the list.
struct DepthTag {
    unsigned eye{};
    EyePairState::Stamp stamp{};
    bool gbuffer{}, depthDone{}, window{};
    SIZE_T gbufferDsv{}, clearedDsv{};
    float clearedDepth = -1;
};
std::recursive_mutex depthTagLock; // after `recording`, never held while taking it or depthLock
std::unordered_map<ID3D12GraphicsCommandList*, DepthTag> depthTags;
std::atomic<uint32_t> depthTagCount{}, depthOpenMask{};
std::array<EyePairState::Stamp, 2> depthOpenStamp{}; // under depthTagLock
std::atomic<uint64_t> depthListsMarked{}, depthListsWindow{}, depthPassesTagged{}, depthPassesUnclaimed{},
    depthTargetsTagged{}, depthDsvTagged{}, depthExitsTagged{};
std::atomic<uint32_t> depthMostTargets{};
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
// HERO VR 0.0.77 CUT7 FPS: the eyes' depth is watched for (pass tags, barrier copies) only while the see-through
// models are drawn (game_xr: live.txt see_through_depth auto) or always; cut6's log: captured 0, skipped thousands.
std::atomic<bool> depthInUse{};
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
// 0.0.76 TAA WATCH (taa_watch.hpp, read-only): SpidyTaaWatchData's lock, taken last (nothing is taken under it).
std::mutex watchLock;
std::atomic<bool> taaDepthConfirmed{}; // 0.0.76 A-depth's auto: the depth log's verdict is CONFIRMED
// The eye depth log: the eye-sized fmt 19 / 20 depth leaving each eye job's G-buffer pass, for the first
// taa_watch::depthFrames pairs (watchLock). The resources seen, for the flat view's check in the barrier hook.
struct DepthWatch {
    uint64_t serial{};
    std::array<ID3D12Resource*, 2> seen{};
    std::array<std::array<ID3D12Resource*, 4>, 2> kinds{};
    std::array<std::array<uint32_t, 4>, 2> counts{};
} depthWatch;
std::array<std::atomic<ID3D12Resource*>, 8> depthWatched{};
std::atomic<bool> depthWatchOpen{};
bool planarDepth(DXGI_FORMAT f) {
    return f == DXGI_FORMAT_R32G8X24_TYPELESS || f == DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
}
void depthWatchPairDone() { // watchLock held
    auto& log = SpidyTaaWatchData.depth;
    if (!depthWatch.serial)
        return;
    ++log.frames;
    log.bothEyes += depthWatch.seen[0] && depthWatch.seen[1];
    log.shared += depthWatch.seen[0] && depthWatch.seen[0] == depthWatch.seen[1];
    log.verdict = taa_watch::depthVerdict(log);
    taaDepthConfirmed = log.verdict == taa_watch::confirmed;
    depthWatch.serial = 0;
    depthWatch.seen = {};
    if (log.frames >= taa_watch::depthFrames)
        depthWatchOpen = false;
}
void watchDepth(unsigned eye, uint64_t serial, ID3D12Resource* res, const D3D12_RESOURCE_DESC& d) {
    if (eye > 1 || !serial)
        return;
    std::lock_guard lock(watchLock);
    auto& log = SpidyTaaWatchData.depth;
    ++log.exits;
    if (log.frames >= taa_watch::depthFrames)
        return;
    if (serial != depthWatch.serial) {
        depthWatchPairDone();
        if (log.frames >= taa_watch::depthFrames)
            return;
        depthWatch.serial = serial;
    }
    log.format = static_cast<uint32_t>(d.Format);
    log.width = static_cast<uint32_t>(d.Width);
    log.height = d.Height;
    depthWatch.seen[eye] = res;
    auto& kinds = depthWatch.kinds[eye];
    auto& counts = depthWatch.counts[eye];
    unsigned best = 0;
    for (unsigned k = 0; k < kinds.size(); ++k) {
        if (kinds[k] == res || !kinds[k]) {
            if (!kinds[k]) {
                kinds[k] = res;
                ++log.distinct[eye];
            }
            ++counts[k];
            break;
        }
    }
    for (unsigned k = 1; k < kinds.size(); ++k)
        if (counts[k] > counts[best])
            best = k;
    log.resource[eye] = reinterpret_cast<uint64_t>(kinds[best]);
    for (auto& w : depthWatched) {
        auto* had = w.load();
        if (had == res)
            break;
        if (!had) {
            w = res;
            break;
        }
    }
    depthWatchOpen = log.frames < taa_watch::depthFrames;
}
// Any list: one of the logged depths leaving DEPTH_WRITE outside the eye jobs' own lists.
void watchFlat(ID3D12GraphicsCommandList* list, UINT count, const D3D12_RESOURCE_BARRIER* bs) {
    if (!bs)
        return;
    for (UINT n = 0; n < count; ++n) {
        const auto& b = bs[n];
        if (b.Type != D3D12_RESOURCE_BARRIER_TYPE_TRANSITION ||
            !(b.Transition.StateBefore & D3D12_RESOURCE_STATE_DEPTH_WRITE) || !b.Transition.pResource)
            continue;
        bool ours = false;
        for (auto& w : depthWatched)
            ours |= w.load(std::memory_order_relaxed) == b.Transition.pResource;
        if (!ours)
            continue;
        bool tagged = false;
        {
            std::lock_guard tagLock(depthTagLock);
            tagged = depthTags.find(list) != depthTags.end();
        }
        if (tagged)
            continue;
        const bool eyeOpen = depthOpenMask.load(std::memory_order_relaxed) != 0;
        std::lock_guard lock(watchLock);
        if (SpidyTaaWatchData.depth.frames >= taa_watch::depthFrames)
            return;
        if (eyeOpen)
            ++SpidyTaaWatchData.depth.otherList;
        else
            ++SpidyTaaWatchData.depth.flatSeen;
    }
}
// 0.0.76 A-depth (taa_depth.hpp, live.txt vr_taa_depth, off by default): with the switch on, the same eye-sized
// depth leaving each eye job's G-buffer pass (fmt 19 / 20 or a plain D32) has its depth plane copied, on the
// game's own list, into a buffer of that eye (a ring of 3): one plane of 32-bit floats, rows on 256-byte steps.
// The buffers start in COMMON and are only promoted (the copy, the history's read), so no state is tracked.
// Anything amiss (a footprint not the eye's, an allocation) turns only this off for the session: the history
// is then turned as before (taaDepthFault, counted as depth missed), never `fault`, never the eyes.
// SpidyVrTaaDepth: 0 off (the default, while the eye depth is CANDIDATE: test 07c1675's depth log saw both eyes'
// depth in 82 of its 200 pairs), 1 on, 2 auto: off until the depth log above has CONFIRMED the eye depth
// (taa_depth::active), then on by itself.
std::atomic<uint32_t> taaDepthOn{taa_depth::off}; // off by default: the eye depth is still CANDIDATE
bool taaDepthActive() {
    return taa_depth::active(taaDepthOn.load(std::memory_order_relaxed), taaDepthConfirmed.load(std::memory_order_relaxed));
}
std::atomic<uint32_t> taaDepthFault{}; // 1 footprint, 2 allocation, 3 the pass could not be built
// 0.0.76 SEAM TRACE (live.txt vr_taa_seam, read-only; taa_depth_trace.hpp seam cells, hot edges, samples): 0 off (the
// default), 1 on - with vr_taa_depth off the eyes' depth is copied and read for the trace's bins alone (the history
// still read where the head motion puts it, the same picture as without it); with vr_taa_depth on it changes nothing
// (that path is traced already).
std::atomic<uint32_t> taaSeamOn{};
struct TaaDepthSlot {
    ComPtr<ID3D12Resource> buffer;
    EyePairState::Stamp stamp{};
    ID3D12CommandList* list{};
    float nearM{};
    float farM{}; // 0.0.76 A-depth CHECKED: the far the eye lens was last given (near_clip::eyeLens), 0 if none yet
    bool pending{}, executed{};
};
std::array<std::array<TaaDepthSlot, depthRingSize>, 2> taaDepthRing; // depthLock
// 0.0.76 A-depth CHECKED (read-only): the GPU time of the two depth copies (a timestamp of ours each side of each
// copy, on the game's list; 16 pairs of 4), resolved on the history's list with the pair that used them and read
// when that capture slot is done.
struct DepthCopyTimes {
    ComPtr<ID3D12QueryHeap> queries;
    ComPtr<ID3D12Resource> stamps;
    const volatile uint64_t* data{};
    double ticksPerMs{};
    bool failed{};
    std::array<uint64_t, 16> serial{};
    std::array<uint8_t, 16> marks{}; // bit 2 * eye + after
    std::array<bool, 3> timed{};     // per capture slot
} depthCopyTimes; // depthLock
bool makeDepthCopyTimes() { // depthLock held
    auto& t = depthCopyTimes;
    if (t.queries || t.failed)
        return t.queries != nullptr;
    t.failed = true;
    if (!device || !queue)
        return false;
    UINT64 frequency{};
    D3D12_QUERY_HEAP_DESC qd{};
    qd.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
    qd.Count = 64;
    D3D12_HEAP_PROPERTIES readback{};
    readback.Type = D3D12_HEAP_TYPE_READBACK;
    D3D12_RESOURCE_DESC buffer{};
    buffer.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    buffer.Width = sizeof(uint64_t) * 4 * 3; // four a capture slot
    buffer.Height = 1;
    buffer.DepthOrArraySize = 1;
    buffer.MipLevels = 1;
    buffer.SampleDesc.Count = 1;
    buffer.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    void* mapped{};
    if (FAILED(queue->GetTimestampFrequency(&frequency)) || !frequency ||
        FAILED(device->CreateQueryHeap(&qd, IID_PPV_ARGS(&t.queries))) ||
        FAILED(device->CreateCommittedResource(&readback, D3D12_HEAP_FLAG_NONE, &buffer,
                                               D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&t.stamps))) ||
        FAILED(t.stamps->Map(0, nullptr, &mapped))) {
        t.queries.Reset();
        t.stamps.Reset();
        return false;
    }
    t.data = static_cast<const volatile uint64_t*>(mapped);
    t.ticksPerMs = static_cast<double>(frequency) / 1000.0;
    t.failed = false;
    return true;
}
void markDepthCopy(ID3D12GraphicsCommandList* list, uint64_t serial, unsigned eye, bool after) { // depthLock held
    if (!serial || eye > 1 || !originalEndQuery || !makeDepthCopyTimes())
        return;
    auto& t = depthCopyTimes;
    const unsigned ring = static_cast<unsigned>(serial % t.serial.size());
    if (t.serial[ring] != serial) {
        t.serial[ring] = serial;
        t.marks[ring] = 0;
    }
    const unsigned bit = 2 * eye + (after ? 1 : 0);
    originalEndQuery(list, t.queries.Get(), D3D12_QUERY_TYPE_TIMESTAMP, ring * 4 + bit);
    t.marks[ring] = static_cast<uint8_t>(t.marks[ring] | (1u << bit));
}
std::array<unsigned, 2> taaDepthNext{};
UINT64 taaDepthW{};
UINT taaDepthH{}, taaDepthPitch{}; // pitch in floats
std::atomic<uint64_t> taaDepthCaptured{};
void taaDepthStop(uint32_t why) {
    uint32_t none = 0;
    if (taaDepthFault.compare_exchange_strong(none, why)) {
        char line[96];
        std::snprintf(line, sizeof line, "SpidyVR: 0.0.76 A-depth off for the session (%u), history as before\n", why);
        OutputDebugStringA(line);
    }
}
void taaDepthCopy(ID3D12GraphicsCommandList* list, const DepthTag& t, ID3D12Resource* res,
                  const D3D12_RESOURCE_DESC& d) {
    if (!(taaDepthActive() || taaSeamOn.load(std::memory_order_relaxed)) ||
        taaDepthFault.load(std::memory_order_relaxed) || t.eye > 1 ||
        !t.stamp.serial)
        return;
    std::lock_guard lock(depthLock);
    if (!device)
        return;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT fp{};
    UINT rows{};
    UINT64 rowBytes{}, bytes{};
    device->GetCopyableFootprints(&d, 0, 1, 0, &fp, &rows, &rowBytes, &bytes);
    const uint32_t pitch = taa_depth::pitchFloats(fp.Footprint.RowPitch, fp.Footprint.Width, fp.Footprint.Height,
                                                  d.Width, d.Height, bytes);
    if (!pitch || fp.Offset != 0 || fp.Footprint.Depth != 1) {
        taaDepthStop(1);
        return;
    }
    const auto now = GetTickCount64();
    std::erase_if(retiredDepth, [&](const RetiredDepth& r) { return now - r.ms > 3000; });
    if (taaDepthW != d.Width || taaDepthH != d.Height || taaDepthPitch != pitch) { // a new eye size: new buffers
        for (auto& eye : taaDepthRing)
            for (auto& slot : eye) {
                if (slot.buffer)
                    retiredDepth.push_back({std::move(slot.buffer), now});
                slot = {};
            }
        taaDepthW = d.Width;
        taaDepthH = d.Height;
        taaDepthPitch = pitch;
    }
    auto& slot = taaDepthRing[t.eye][taaDepthNext[t.eye]];
    if (!slot.buffer) {
        D3D12_HEAP_PROPERTIES heap{};
        heap.Type = D3D12_HEAP_TYPE_DEFAULT;
        D3D12_RESOURCE_DESC desc{};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        desc.Width = bytes;
        desc.Height = 1;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.Format = DXGI_FORMAT_UNKNOWN;
        desc.SampleDesc.Count = 1;
        desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        if (FAILED(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COMMON,
                                                   nullptr, IID_PPV_ARGS(&slot.buffer))) ||
            !slot.buffer) {
            slot = {};
            taaDepthStop(2);
            return;
        }
    }
    taaDepthNext[t.eye] = (taaDepthNext[t.eye] + 1) % depthRingSize;
    markDepthCopy(list, t.stamp.serial, t.eye, false); // 0.0.76 A-depth CHECKED (read-only)
    D3D12_RESOURCE_BARRIER to{};
    to.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    to.Transition = {res, 0, D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_COPY_SOURCE}; // the depth plane
    originalBarrier(list, 1, &to);
    D3D12_TEXTURE_COPY_LOCATION into{};
    into.pResource = slot.buffer.Get();
    into.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    into.PlacedFootprint = fp;
    D3D12_TEXTURE_COPY_LOCATION from{};
    from.pResource = res;
    from.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    from.SubresourceIndex = 0;
    list->CopyTextureRegion(&into, 0, 0, 0, &from, nullptr);
    std::swap(to.Transition.StateBefore, to.Transition.StateAfter);
    originalBarrier(list, 1, &to); // back to DEPTH_WRITE: the game's own barrier follows, as it expects
    markDepthCopy(list, t.stamp.serial, t.eye, true); // 0.0.76 A-depth CHECKED (read-only)
    slot.stamp = t.stamp;
    slot.list = list;
    slot.nearM = near_clip::metres();
    {
        float lensNear{}, lensFar{};
        near_clip::eyeLens(lensNear, lensFar);
        slot.farM = std::isfinite(lensFar) && lensFar > slot.nearM ? lensFar : 0.f;
    }
    slot.pending = true;
    slot.executed = false;
    ++taaDepthCaptured;
}
// The depth of this pair's eye, executed on the game's queue (depthLock taken; `recording` may be held).
struct TaaDepthView {
    D3D12_GPU_VIRTUAL_ADDRESS at{};
    float nearM{};
    float farM{};
    ID3D12Resource* buffer{};
};
bool taaDepthFor(uint64_t serial, unsigned eye, TaaDepthView& out) {
    if (!serial || eye > 1)
        return false;
    for (const auto& slot : taaDepthRing[eye])
        if (slot.buffer && slot.executed && !slot.pending && slot.stamp.serial == serial) {
            out = {slot.buffer->GetGPUVirtualAddress(), slot.nearM, slot.farM, slot.buffer.Get()};
            return out.at != 0 && std::isfinite(out.nearM) && out.nearM > 0;
        }
    return false;
}
// The G-buffer pass's depth view on the tagged job: the clear seen on it says which way the
// depth runs (0 far, reversed: as the eye lens is made; 1: standard, depth is not used).
// Judged whichever comes first, the clear or the pass's binding; a later clear of it to the far
// value leaves sky (nothing hides), to anything else turns the depth off.
void depthJudge(DepthTag& t) {
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
void depthNotePass(DepthTag& t, const D3D12_CPU_DESCRIPTOR_HANDLE* dsv) {
    t.gbuffer = true;
    t.gbufferDsv = dsv ? dsv->ptr : 0;
    depthJudge(t);
}
void STDMETHODCALLTYPE clearDepth(ID3D12GraphicsCommandList* list, D3D12_CPU_DESCRIPTOR_HANDLE dsv,
                                  D3D12_CLEAR_FLAGS flags, FLOAT depth, UINT8 stencil, UINT rects,
                                  const D3D12_RECT* rect) {
    originalClearDepth(list, dsv, flags, depth, stencil, rects, rect);
    if (!depthTagCount.load(std::memory_order_relaxed) || !(flags & D3D12_CLEAR_FLAG_DEPTH) || rects)
        return;
    std::lock_guard tagLock(depthTagLock);
    const auto found = depthTags.find(list);
    if (found == depthTags.end())
        return;
    auto& t = found->second;
    t.clearedDsv = dsv.ptr;
    t.clearedDepth = std::isfinite(depth) ? depth : 2.f;
    depthJudge(t);
}
// Barrier hook, before the game's own barrier: on the tagged job after its G-buffer pass.
void depthBarrier(ID3D12GraphicsCommandList* list, DepthTag& t, UINT count, const D3D12_RESOURCE_BARRIER* bs) {
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
        ++depthExitsTagged;
        depthSeenFormat = static_cast<uint32_t>(d.Format);
        depthSeenW = static_cast<uint32_t>(d.Width);
        depthSeenH = d.Height;
        UINT w{}, h{};
        {
            std::lock_guard recordLock(recording);
            w = width;
            h = height;
        }
        if (!scene_depth::sizeFits(d.Width, d.Height, w, h)) {
            ++depthSkipSize;
            return;
        }
        w = static_cast<UINT>(d.Width); // 0.0.75: the eye's render size (an upscaler's lower one too)
        h = d.Height;
        if (!depthFormatOk(d.Format) || d.MipLevels != 1 || d.DepthOrArraySize != 1 || d.SampleDesc.Count != 1) {
            ++depthSkipFormat;
            if (planarDepth(d.Format) && d.MipLevels == 1 && d.DepthOrArraySize == 1 && d.SampleDesc.Count == 1) {
                watchDepth(t.eye, t.stamp.serial, res, d); // 0.0.76 TAA WATCH: logged only
                taaDepthCopy(list, t, res, d);             // 0.0.76 A-depth: only with vr_taa_depth on
            }
            return;
        }
        taaDepthCopy(list, t, res, d); // 0.0.76 A-depth: only with vr_taa_depth on
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
    if ((!depthCaptured.load(std::memory_order_relaxed) && !taaDepthCaptured.load(std::memory_order_relaxed)) ||
        !commands)
        return;
    std::lock_guard lock(depthLock);
    for (auto& eye : taaDepthRing) // 0.0.76 A-depth
        for (auto& slot : eye)
            if (slot.pending)
                for (UINT n = 0; n < count; ++n)
                    if (commands[n] == slot.list) {
                        slot.pending = false;
                        slot.executed = gameQueue;
                        if (!gameQueue)
                            slot.stamp = {};
                        break;
                    }
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
    if (!depthCaptured.load(std::memory_order_relaxed) && !taaDepthCaptured.load(std::memory_order_relaxed))
        return; // nothing ever recorded: no lock on every list's Reset
    std::lock_guard lock(depthLock);
    for (auto& eye : taaDepthRing) // 0.0.76 A-depth
        for (auto& slot : eye)
            if (slot.pending && slot.list == list) {
                slot.pending = slot.executed = false;
                slot.stamp = {};
                slot.list = nullptr;
            }
    for (auto& eye : depthRing)
        for (auto& slot : eye)
            if (slot.pending && slot.list == list) {
                slot.pending = slot.executed = false;
                slot.stamp = {};
                slot.list = nullptr;
                ++depthSkipLost;
            }
}
// 0.0.75 SEE-THROUGH: a pass's targets on any thread. On a marked list: counted, and its first
// G-buffer pass (3+ colour targets and a depth) noted. On an unmarked list while exactly one eye
// job is open: that list is taken as the job's (window tag); otherwise counted as unclaimed.
void depthTargets(ID3D12GraphicsCommandList* list, UINT n, const D3D12_CPU_DESCRIPTOR_HANDLE* dsv) {
    if (!depthTagCount.load(std::memory_order_relaxed) && !depthOpenMask.load(std::memory_order_relaxed))
        return;
    if (depthOff.load(std::memory_order_relaxed) || !depthInUse.load(std::memory_order_relaxed))
        return;
    const bool gbuffer = n >= 3 && dsv;
    std::lock_guard tagLock(depthTagLock);
    auto found = depthTags.find(list);
    if (found == depthTags.end()) {
        if (!gbuffer)
            return;
        const auto eye = scene_depth::soleOpenEye(depthOpenMask.load());
        if (eye > 1 || !depthOpenStamp[eye].serial) {
            if (depthOpenMask.load())
                ++depthPassesUnclaimed;
            return;
        }
        DepthTag tag{};
        tag.eye = eye;
        tag.stamp = depthOpenStamp[eye];
        tag.window = true;
        found = depthTags.emplace(list, tag).first;
        depthTagCount = static_cast<uint32_t>(depthTags.size());
        ++depthListsWindow;
    }
    auto& t = found->second;
    ++depthTargetsTagged;
    if (dsv)
        ++depthDsvTagged;
    auto most = depthMostTargets.load();
    while (n > most && !depthMostTargets.compare_exchange_weak(most, n)) {
    }
    if (gbuffer && !t.gbuffer) {
        ++depthPassesTagged;
        depthNotePass(t, dsv);
    }
}
void depthUntag(ID3D12GraphicsCommandList* list) {
    if (!depthTagCount.load(std::memory_order_relaxed))
        return;
    std::lock_guard tagLock(depthTagLock);
    const auto found = depthTags.find(list);
    if (found == depthTags.end())
        return;
    if (found->second.gbuffer && !found->second.depthDone)
        ++depthSkipNoPass; // no depth left the pass in the job
    depthTags.erase(found);
    depthTagCount = static_cast<uint32_t>(depthTags.size());
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
    depthTargets(list, n, dsv); // 0.0.75 SEE-THROUGH: reads only
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
    if (!currentMarker.ended) { // 0.0.75 SEE-THROUGH: the list, whatever thread records on it
        if (!depthOff.load(std::memory_order_relaxed) && depthInUse.load(std::memory_order_relaxed) &&
            currentMarker.eye < 2) {
            std::lock_guard tagLock(depthTagLock);
            DepthTag tag{};
            tag.eye = currentMarker.eye;
            tag.stamp = currentMarker.stamp;
            depthTags[list] = tag;
            depthTagCount = static_cast<uint32_t>(depthTags.size());
            ++depthListsMarked;
        }
    } else
        depthUntag(list);
    auto& t = foveaTag;
    if (!currentMarker.ended) {
        if (t.set && t.list && t.list != list)
            t.set = false; // that list's rate was left on: its own job end or Reset clears it
        t = {list, currentMarker.eye, false};
    } else if (t.list == list) {
        if (t.set)
            clearRate(list);
        t = {};
    }
}
// HERO VR 0.0.77 CUT7 WRIST MAP SOURCE (CANDIDATE): the game's minimap is drawn into the HUD panel's texture in
// VR play (native_hud moves the second movie off the game's view), so the presented frame the cut6 wrist map
// cropped showed the third-person view (the owner's video: brick). While hunting, each render-target transition's
// resource is looked at once (GetDesc) and kept when it has the panel's made size; the present thread then finds
// which of them the panel's engine objects point at (hudScan) and copies that one. Read-only on the game's side.
std::atomic<bool> hudWanted{}, hudHunting{};
std::atomic<ID3D12Resource*> hudTracked{};
std::atomic<uint32_t> hudState{}, hudHuntW{}, hudHuntH{};
std::atomic<uint64_t> hudLooked{}, hudSized{}, hudCopies{}, hudSkipState{}, hudSkipBusy{}, hudFinds{}, hudLost{};
std::mutex hudLock; // the candidates only (never taken with another lock held but submission/recording)
std::unordered_map<ID3D12Resource*, uint32_t> hudCandidates; // made-size render targets, their last state
std::unordered_map<ID3D12Resource*, bool> hudLookedAt;
void watchHud(UINT count, const D3D12_RESOURCE_BARRIER* bs) {
    if (!bs)
        return;
    auto* tracked = hudTracked.load(std::memory_order_relaxed);
    const bool hunting = hudHunting.load(std::memory_order_relaxed);
    for (UINT i = 0; i < count; ++i) {
        if (bs[i].Type != D3D12_RESOURCE_BARRIER_TYPE_TRANSITION || !bs[i].Transition.pResource)
            continue;
        auto* r = bs[i].Transition.pResource;
        const auto after = static_cast<uint32_t>(bs[i].Transition.StateAfter);
        if (r == tracked) {
            hudState.store(after, std::memory_order_relaxed);
            continue;
        }
        if (!hunting || !((bs[i].Transition.StateBefore | bs[i].Transition.StateAfter) &
                          D3D12_RESOURCE_STATE_RENDER_TARGET))
            continue;
        std::unique_lock lock(hudLock, std::try_to_lock);
        if (!lock.owns_lock())
            continue; // the present thread is scanning: the next frame's barriers
        if (const auto c = hudCandidates.find(r); c != hudCandidates.end()) {
            c->second = after;
            continue;
        }
        if (hudLookedAt.size() >= 20000 || !hudLookedAt.emplace(r, true).second)
            continue;
        ++hudLooked;
        const auto d = r->GetDesc();
        if (d.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D && d.Width == hudHuntW.load() &&
            d.Height == hudHuntH.load() && d.MipLevels == 1 && d.SampleDesc.Count == 1 && d.DepthOrArraySize == 1) {
            hudCandidates.emplace(r, after);
            ++hudSized;
        }
    }
}
// HERO VR 0.0.78 cut18 UI DUMP (diagnostic, include/spidy/ui_dump.hpp): where the game draws its button prompts in VR
// play. While a dump is asked for (about 45 presents), each render-target transition's resource is looked at once
// (GetDesc) and kept, with a reference, when it is a colour target of the frame's shape; its last recorded state and
// how often it went from RENDER_TARGET to readable are counted. Read-only on the game's side.
std::atomic<bool> uiHunting{};
std::atomic<uint32_t> uiAsked{}, uiBackW{}, uiBackH{};
std::atomic<uint64_t> uiLooked{};
struct UiTarget {
    ComPtr<ID3D12Resource> resource;
    uint32_t state{}, format{}, width{}, height{};
    uint64_t toRead{}, transitions{};
};
std::mutex uiLock; // the targets only (never taken with another lock held)
std::unordered_map<ID3D12Resource*, UiTarget> uiTargets;
std::unordered_map<ID3D12Resource*, bool> uiLookedAt;
void watchUi(UINT count, const D3D12_RESOURCE_BARRIER* bs) {
    if (!bs)
        return;
    const uint32_t backW = uiBackW.load(std::memory_order_relaxed), backH = uiBackH.load(std::memory_order_relaxed);
    for (UINT i = 0; i < count; ++i) {
        if (bs[i].Type != D3D12_RESOURCE_BARRIER_TYPE_TRANSITION || !bs[i].Transition.pResource)
            continue;
        const auto before = static_cast<uint32_t>(bs[i].Transition.StateBefore);
        const auto after = static_cast<uint32_t>(bs[i].Transition.StateAfter);
        if (!((before | after) & D3D12_RESOURCE_STATE_RENDER_TARGET))
            continue;
        auto* r = bs[i].Transition.pResource;
        std::unique_lock lock(uiLock, std::try_to_lock);
        if (!lock.owns_lock())
            continue;
        if (const auto t = uiTargets.find(r); t != uiTargets.end()) {
            t->second.state = after;
            ++t->second.transitions;
            if ((before & D3D12_RESOURCE_STATE_RENDER_TARGET) && ui_dump::readableState(after))
                ++t->second.toRead;
            continue;
        }
        if (uiTargets.size() >= ui_dump::maxCandidates || uiLookedAt.size() >= 20000 ||
            !uiLookedAt.emplace(r, true).second)
            continue;
        ++uiLooked;
        const auto d = r->GetDesc();
        if (d.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D || d.MipLevels != 1 || d.SampleDesc.Count != 1 ||
            d.DepthOrArraySize != 1 || d.Width > 16384 || !ui_dump::bytesPerPixel(d.Format) ||
            !ui_dump::sized(static_cast<uint32_t>(d.Width), d.Height, backW, backH))
            continue;
        UiTarget t;
        t.resource = r; // a reference: kept alive until the dump
        t.state = after;
        t.format = d.Format;
        t.width = static_cast<uint32_t>(d.Width);
        t.height = d.Height;
        t.transitions = 1;
        t.toRead = (before & D3D12_RESOURCE_STATE_RENDER_TARGET) && ui_dump::readableState(after) ? 1 : 0;
        uiTargets.emplace(r, std::move(t));
    }
}
// HERO VR 0.0.78 cut21 PROMPT LAYER (CANDIDATE, include/spidy/prompt_layer.hpp): the game's UI layer once found
// (the cut18 UI DUMP's TARGET #1 - RGBA8 at the back buffer's size, mostly clear, its prompts, waypoint and markers
// drawn with alpha): its last recorded state and when it last changed state. Read-only on the game's side.
std::atomic<ID3D12Resource*> layerTracked{};
std::atomic<uint32_t> layerState{};
std::atomic<uint64_t> layerSeenMs{}, layerTransitions{};
void watchLayer(ID3D12Resource* tracked, UINT count, const D3D12_RESOURCE_BARRIER* bs) {
    if (!bs)
        return;
    for (UINT i = 0; i < count; ++i)
        if (bs[i].Type == D3D12_RESOURCE_BARRIER_TYPE_TRANSITION && bs[i].Transition.pResource == tracked) {
            layerState.store(static_cast<uint32_t>(bs[i].Transition.StateAfter), std::memory_order_relaxed);
            layerSeenMs.store(GetTickCount64(), std::memory_order_relaxed);
            layerTransitions.fetch_add(1, std::memory_order_relaxed);
        }
}
void STDMETHODCALLTYPE barrier(ID3D12GraphicsCommandList* list, UINT count,
                               const D3D12_RESOURCE_BARRIER* bs) {
    // Update 27: ADDRESS FINDER callout before the early return (read-only).
    native_address_finder::onBarrier(list, count, bs);
    if (hudHunting.load(std::memory_order_relaxed) || hudTracked.load(std::memory_order_relaxed))
        watchHud(count, bs); // CUT7 WRIST MAP SOURCE: read-only
    if (uiHunting.load(std::memory_order_relaxed))
        watchUi(count, bs); // cut18 UI DUMP: read-only (also the cut21 PROMPT LAYER's find)
    if (auto* layer = layerTracked.load(std::memory_order_relaxed))
        watchLayer(layer, count, bs); // cut21 PROMPT LAYER: read-only
    if (depthWatchOpen.load(std::memory_order_relaxed))
        watchFlat(list, count, bs); // 0.0.76 TAA WATCH: read-only
    // 0.0.74 test 10 SEE-THROUGH: the scene depth, copied ahead of the game's own barrier.
    // 0.0.76 FREEZE FIX: the tag is copied out and depthTagLock let go before depthBarrier, which
    // takes `recording` and depthLock. Holding depthTagLock across it was the reverse of execute()
    // and the monitor (`recording`, then our own OMSetRenderTargets into depthTargets'
    // depthTagLock): two threads could wait on each other for ever (0.0.75's freeze on loading).
    if (enabled && depthTagCount.load(std::memory_order_relaxed)) {
        DepthTag tag{};
        bool wanted = false;
        {
            std::lock_guard tagLock(depthTagLock);
            const auto found = depthTags.find(list);
            if (found != depthTags.end() && found->second.gbuffer && !found->second.depthDone) {
                tag = found->second;
                wanted = true;
            }
        }
        if (wanted) {
            depthBarrier(list, tag, count, bs); // no depthTagLock held here
            if (tag.depthDone) {
                std::lock_guard tagLock(depthTagLock);
                const auto found = depthTags.find(list);
                if (found != depthTags.end() && found->second.stamp == tag.stamp)
                    found->second.depthDone = true;
            }
        }
    }
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
    if (SUCCEEDED(result)) {
        depthUntag(list); // 0.0.75: a reset list is no eye job's any more
        depthReset(list); // 0.0.74 test 10 SEE-THROUGH
    }
    if (enabled && SUCCEEDED(result)) {
        std::lock_guard lock(recording);
        lists.erase(list);
    }
    return result;
}
// 0.0.76 TAA WATCH (read-only): the game's own eye jobs' GPU time. A timestamp of ours beside the game's own at each
// eye job's begin and end marker (16 pairs of slots, `recording`), read with the pair it timed (stagePair).
struct EyeTimes {
    ComPtr<ID3D12QueryHeap> queries;
    ComPtr<ID3D12Resource> stamps;
    const volatile uint64_t* data{};
    double ticksPerMs{};
    bool failed{};
    std::array<uint64_t, 16> serial{};
    std::array<uint8_t, 16> marks{}; // bit 2 * eye + ended
    std::array<bool, 3> timed{};     // per capture slot
} eyeTimes;
bool makeEyeTimes() { // `recording` held
    if (eyeTimes.queries || eyeTimes.failed)
        return eyeTimes.queries != nullptr;
    eyeTimes.failed = true;
    if (!device || !queue)
        return false;
    UINT64 frequency{};
    D3D12_QUERY_HEAP_DESC qd{};
    qd.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
    qd.Count = 64;
    D3D12_HEAP_PROPERTIES readback{};
    readback.Type = D3D12_HEAP_TYPE_READBACK;
    D3D12_RESOURCE_DESC buffer{};
    buffer.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    buffer.Width = sizeof(uint64_t) * 4 * 3; // four a capture slot
    buffer.Height = 1;
    buffer.DepthOrArraySize = 1;
    buffer.MipLevels = 1;
    buffer.SampleDesc.Count = 1;
    buffer.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    void* mapped{};
    if (FAILED(queue->GetTimestampFrequency(&frequency)) || !frequency ||
        FAILED(device->CreateQueryHeap(&qd, IID_PPV_ARGS(&eyeTimes.queries))) ||
        FAILED(device->CreateCommittedResource(&readback, D3D12_HEAP_FLAG_NONE, &buffer,
                                               D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
                                               IID_PPV_ARGS(&eyeTimes.stamps))) ||
        FAILED(eyeTimes.stamps->Map(0, nullptr, &mapped))) {
        eyeTimes.queries.Reset();
        eyeTimes.stamps.Reset();
        return false;
    }
    eyeTimes.data = static_cast<const volatile uint64_t*>(mapped);
    eyeTimes.ticksPerMs = static_cast<double>(frequency) / 1000.0;
    eyeTimes.failed = false;
    return true;
}
void markEyeTime(ID3D12GraphicsCommandList* list) { // `recording` held, at an eye job's marker
    if (!currentMarker.valid || currentMarker.eye > 1 || !currentMarker.stamp.serial || !makeEyeTimes())
        return;
    const uint64_t serial = currentMarker.stamp.serial;
    const unsigned ring = static_cast<unsigned>(serial % eyeTimes.serial.size());
    if (eyeTimes.serial[ring] != serial) {
        eyeTimes.serial[ring] = serial;
        eyeTimes.marks[ring] = 0;
    }
    const unsigned bit = 2 * currentMarker.eye + (currentMarker.ended ? 1 : 0);
    if (!currentMarker.ended && (eyeTimes.marks[ring] & (1u << bit)))
        return; // the job's first begin
    originalEndQuery(list, eyeTimes.queries.Get(), D3D12_QUERY_TYPE_TIMESTAMP, ring * 4 + bit);
    eyeTimes.marks[ring] = static_cast<uint8_t>(eyeTimes.marks[ring] | (1u << bit));
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
            markEyeTime(list); // 0.0.76 TAA WATCH: read-only timing
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
// 0.0.76 SPIDY TAA (vr_aa.hpp; set at the start from SpidyXrStart's options, setVrAa): ours, smaa or legacy. Ours and
// smaa lock our FXAA, the HISTORY slider and CAS sharpen off, as SM1STYLE does; legacy leaves them to their settings.
std::atomic<uint32_t> vrAaMode{vr_aa::ours};
bool vrAaLocked() {
    return vrAaMode.load(std::memory_order_relaxed) != vr_aa::legacy;
}
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
    // HERO VR 0.0.78 PUNISHER ARSENAL optics (punisher078::opticConstants, this eye): the lens's disk (centre u, v;
    // radius u, v), the source (scope: the axis's far point u, v, zoom; red dot: the dot u, v), mode (0 none,
    // 1 scope, 2 red dot), then the eye box shade and the dot's radius (u, v).
    float4 scopeLens; float4 scopeSrc; float4 scopeMisc;
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
// HERO VR 0.0.78: the source at uv, bilinear (the root has no sampler: four Loads).
float3 fetch(float2 uv, uint w, uint h) {
    const float2 t = clamp(uv, 0.0, 1.0) * float2(w, h) - 0.5;
    const int2 b = int2(floor(t));
    const float2 f = t - floor(t);
    const int2 hi = int2(w, h) - 1;
    const float3 c00 = source.Load(int3(clamp(b, int2(0, 0), hi), 0)).rgb;
    const float3 c10 = source.Load(int3(clamp(b + int2(1, 0), int2(0, 0), hi), 0)).rgb;
    const float3 c01 = source.Load(int3(clamp(b + int2(0, 1), int2(0, 0), hi), 0)).rgb;
    const float3 c11 = source.Load(int3(clamp(b + int2(1, 1), int2(0, 0), hi), 0)).rgb;
    return lerp(lerp(c00, c10, f.x), lerp(c01, c11, f.x), f.y);
}
// HERO VR 0.0.78 PUNISHER ARSENAL: the sniper scope's picture (the patch along its axis, magnified, behind a
// duplex reticle; black outside the exit pupil, so an eye off the axis sees the scope's shadow close in) or the
// rifle's red dot (collimated: where the eye's ray parallel to the sight's axis meets the window), on the lens.
// The gun model (no lens glass) is drawn over this pass by the overlay.
float3 optic(float3 o, float2 uv, uint w, uint h) {
    if (scopeSrc.w < 0.5)
        return o;
    const float2 n = (uv - scopeLens.xy) / max(scopeLens.zw, 1e-6);
    const float r = length(n);
    if (r > 1.0)
        return o;
    if (scopeSrc.w < 1.5) {
        // The picture centred on the axis (far away), magnified into the lens's disk; the reticle at the disk's centre
        // (the eye off the axis sees the same picture, the exit pupil's shadow closing in - as a real scope).
        const float2 m = n; // from the lens centre, in lens radii
        float3 c = grade(fetch(scopeSrc.xy + (uv - scopeLens.xy) / max(scopeSrc.z, 1.0), w, h), uv);
        const float ax = abs(m.x), ay = abs(m.y);
        const float thinT = 0.010, thickT = 0.032;
        const float hair = max(step(ay, lerp(thinT, thickT, step(0.45, ax))), step(ax, lerp(thinT, thickT, step(0.45, ay))));
        c = lerp(c, float3(0.015, 0.015, 0.015), hair);
        c = lerp(c, float3(0.95, 0.06, 0.03), step(length(m), 0.018)); // the lit centre
        const float pupil = 1.0 - 0.8 * saturate(scopeMisc.x);
        c *= saturate((pupil - length(m)) / 0.08) * (1.0 - 0.5 * saturate(scopeMisc.x));
        c *= 1.0 - 0.55 * saturate((r - 0.82) / 0.18); // the tube's edge
        return c;
    }
    const float2 dd = (uv - scopeSrc.xy) / max(scopeMisc.yz, 1e-6);
    const float inside = 1.0 - step(1.0, length((scopeSrc.xy - scopeLens.xy) / max(scopeLens.zw, 1e-6)));
    const float dotK = saturate(1.6 - length(dd)) * inside;
    const float3 tinted = o * float3(0.97, 1.0, 0.99); // the window's faint coating
    return lerp(tinted, float3(1.0, 0.10, 0.06), dotK);
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
    o = optic(o, uv, w, h);  // HERO VR 0.0.78 PUNISHER ARSENAL
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
    col = optic(col, uv, w, h); // HERO VR 0.0.78 PUNISHER ARSENAL
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
    parameters[1].Constants.Num32BitValues = 44; // Update 37.2: 4 sharpening + 20 COLOR + 8 FOVEATED DEBUG; 0.0.78: + 12 OPTICS
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
// HERO VR 0.0.78 PUNISHER ARSENAL: each eye's optics (punisher078::opticConstants; setEyeOptics), fresh for
// opticsFreshMs after they were set (then none: a stale scope is never drawn where the gun no longer is).
std::mutex opticsLock;
std::array<float, 12> opticsNow[2]{}; // opticsLock
uint64_t opticsAtMs{};                // opticsLock
constexpr uint64_t opticsFreshMs = 100;
bool opticsLive() {
    std::lock_guard lock(opticsLock);
    return opticsAtMs && GetTickCount64() - opticsAtMs <= opticsFreshMs && (opticsNow[0][7] > .5f || opticsNow[1][7] > .5f);
}
std::array<float, 12> eyeOptics(unsigned eye) {
    std::lock_guard lock(opticsLock);
    if (!opticsAtMs || GetTickCount64() - opticsAtMs > opticsFreshMs || eye > 1)
        return {};
    return opticsNow[eye];
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
        const auto optics = eyeOptics(i); // HERO VR 0.0.78 PUNISHER ARSENAL (zero: none)
        list->SetGraphicsRoot32BitConstants(1, 12, optics.data(), 32);
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
    // 0.0.76 SPIDY TAA: its own pass while EYE AA (its lens step) is on; never under SMAA.
    if (const uint32_t mode = vrAaMode.load(std::memory_order_relaxed); mode != vr_aa::legacy)
        return mode == vr_aa::ours && temporalOn.load() && !historyFault.load() ? eye_taa::historyPercent : 0u;
    return eye_taa::historyInForce(historyPercent.load(), temporalOn.load());
}
// 0.0.76 SPIDY TAA: our own pass (taaShader) in place of the plain history blend.
bool spidyTaaNow() {
    return vrAaMode.load(std::memory_order_relaxed) == vr_aa::ours && !sm1Lock.load(std::memory_order_relaxed);
}
// The reduced-resolution OWN-AA path reconstructs into full-size eye history.
bool temporalUpscale() {
    return width && height && outWidth > width && outHeight > height && spidyTaaNow() &&
           temporalOn.load(std::memory_order_relaxed) && !taaDepthActive();
}
std::mutex viewLock;
struct NotedViews {
    uint64_t serial{};
    std::array<eye_history::EyeView, 2> eyes{};
    std::array<float, 2> jitter{};  // 0.0.76 JITTER CHECK: eye 0's own step (eye pixels, x right, y up)
    std::array<uint32_t, 2> size{}; // the eye's size those pixels are of
};
std::array<NotedViews, 32> notedViews{}; // viewLock
unsigned nextNoted{};                    // viewLock
struct HistoryPipeline {
    ComPtr<ID3D12RootSignature> root;
    ComPtr<ID3D12PipelineState> state, taa; // taa: 0.0.76 SPIDY TAA (taaShader), built in its mode only
    ComPtr<ID3D12RootSignature> depthRoot;   // 0.0.76 A-depth: taaShader's TAA_DEPTH variant, built when first on
    ComPtr<ID3D12PipelineState> taaDepth;
    ComPtr<ID3D12RootSignature> depthCountRoot; // 0.0.76 TAA JITTER: the TAA_DEPTH + TAA_COUNT variant (or none)
    ComPtr<ID3D12PipelineState> taaDepthCount;
    uint64_t shownSerial{};                  // the pair `shown` is (A-depth: its depth)
    // 0.0.76 TAA JITTER: taaShader with TAA_COUNT (the rejection counts), its root signature (the history's, then a
    // root UAV), the counts (64 bytes a capture slot: eye 0 then eye 1, 8 counters each, taa_depth_check::Counter)
    // and their readback.
    ComPtr<ID3D12RootSignature> countRoot;
    ComPtr<ID3D12PipelineState> taaCount;
    ComPtr<ID3D12Resource> counts, countRead;
    const volatile uint32_t* countData{};
    std::array<std::array<uint32_t, 16>, 3> countLast{}; // the totals last read per slot (the counts only grow)
    std::array<bool, 3> counted{}, countStill{};
    // 0.0.76 A-depth CHECKED: per capture slot, the path (taa_depth_check::Path) and movement (Category) of the pair
    // counted there, and whether the pair timed there was SPIDY TAA's, turned with the depth.
    std::array<uint32_t, 3> countPath{}, countCategory{};
    // 0.0.76 A-depth TRACE (taa_depth_trace.hpp, read-only): its variant, per eye the history ages (this pair's and
    // the last one's, by the pictures' parity), whether an eye's ages of the last pair are there, the graze bins
    // and their zeros, and the readback (per capture slot: its graze bins, then its 2 eyes' probe words) with
    // which eyes of a slot were traced and that slot's movement category.
    ComPtr<ID3D12RootSignature> depthAgeRoot;
    ComPtr<ID3D12PipelineState> taaDepthAge;
    // 0.0.76 SEAM TRACE: TAA_COUNT + TAA_AGE without TAA_DEPTH on depthAgeRoot - SPIDY TAA's counted picture (the
    // history read where the head motion puts it), the eye's depth read for the bins only.
    ComPtr<ID3D12PipelineState> taaSeam;
    std::array<std::array<ComPtr<ID3D12Resource>, 2>, 2> ages;
    uint32_t agesW{}, agesH{};
    std::array<bool, 2> aged{};
    ComPtr<ID3D12Resource> graze, zeros, traceRead;
    const volatile uint32_t* traceData{};
    std::array<bool, 3> traced{};
    std::array<bool, 3> timedOurs{}, timedDepth{};
    ComPtr<ID3D12DescriptorHeap> views, targets;
    ComPtr<ID3D12QueryHeap> queries;
    ComPtr<ID3D12Resource> stamps;
    const volatile uint64_t* stampData{};
    double ticksPerMs{};
    std::array<bool, 3> timed{};
    // Per eye, the picture so far and the one being drawn (parity is read).
    std::array<std::array<ComPtr<ID3D12Resource>, 2>, 2> pictures;
    unsigned parity{};
    bool ready{}, valid{}, upscale{}; // upscale: history consumes native 70% eyes; valid: pictures[*][parity] hold `shown`
    std::array<eye_history::EyeView, 2> shown{};
    std::array<float, 2> shownJitter{}; // 0.0.76 JITTER CHECK: eye 0's step in `shown`
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

// 0.0.76 SPIDY TAA (vr_aa.hpp): our own per-eye temporal AA, the history pass's root signature and pictures. Each
// eye's last picture (t1) turned to this one (the homography rows, from that eye's own previous and current view and
// lens), read with a 5-tap Catmull-Rom (no bilinear blur build-up), clipped to this pixel's 3 x 3 neighbourhood in
// YCoCg - the variance box (mean +/- gamma * sigma) inside the min / max - and blended with luma-weighted (1 / (1 +
// luma)) shares so bright sub-pixel steps do not flicker. extra: x the share kept, yz 1 / size, w gamma.
const char* taaShader = R"(
Texture2D current : register(t0);
Texture2D previous : register(t1);
SamplerState bilinear : register(s0);
cbuffer Frame : register(b0) { float4 row0; float4 row1; float4 row2; float4 extra; };
#if defined(TAA_DEPTH) || defined(TAA_AGE)
// 0.0.76 A-depth (taa_depth.hpp): each eye's depth, this pair's and the last one's (one plane of floats, reversed-Z).
// shift.xyz the previous eye's shift for the travel over the near plane, shift.w the last near over this one;
// sizes: the depth's width, height and row pitch (floats); lin (taa_depth_check.hpp linearOf, from the checked
// convention): xy turn this pair's depth into near / distance, zw the last pair's.
StructuredBuffer<float> depthNow : register(t2);
StructuredBuffer<float> depthBefore : register(t3);
cbuffer Depth : register(b1) {
    float4 shift;
    float4 sizes;
    float4 lin;
#ifdef TAA_AGE
    // 0.0.76 A-depth TRACE (taa_depth_trace.hpp, read-only): x 1 when this eye's ages of the last pair are there (it
    // was turned with the depth then), y this pair's near (metres), zw the static-surface distances for the probes;
    // lens: this eye's tangents (left, right, down, up) for the normals of the graze bins.
    float4 probe;
    float4 lens;
#endif
};
#endif
#ifdef TAA_COUNT
// 0.0.76 TAA JITTER (read-only): this eye's counts (one pixel in 16 x 16; taa_depth_check.hpp Counter), bytes 0 the
// pixels with a history to blend, 4 those rejected (clip moved it or depth refused it), 8 the clip alone, 12 the
// depth test alone, 16 both, 20 depth-path pixels on head motion instead (no usable depth), 24 those the depth test
// ran on, 28 all pixels. The picture is the same with or without it.
RWByteAddressBuffer counts : register(u1); // u0 shares its slot with SV_Target0 in ps_5_0
#endif
#ifdef TAA_AGE
// 0.0.76 A-depth TRACE (read-only; the picture is the same with or without it): each pixel's history age (match << 16
// | kept, taa_depth_trace.hpp) written to ageOut, the last pair's read from ageIn at the place the history came from;
// after the eye's w * h ages, its 8 probe pixels' words (age, flags, residual, distance).
RWStructuredBuffer<uint> ageOut : register(u2);
RWStructuredBuffer<uint> ageIn : register(u3);
// The graze bins (taa_depth_trace.hpp Bin / GrazeWord): this eye's 6 x 10 words, zeroed before the pass; counted
// pixels only.
RWByteAddressBuffer graze : register(u4);
float viewZ(float dd) { return probe.y / max(dd * lin.x + lin.y, 1e-12); }
float3 viewAt(float2 u, float z) { return float3(lerp(lens.x, lens.y, u.x), lerp(lens.w, lens.z, u.y), 1) * z; }
// The surface's bin at depth pixel dp: its normal from the nearer neighbour on each axis (a jump over 10 % on both
// sides of an axis: a depth edge), |n.v| against the view ray. edge: any of the 4 neighbours more than 10 % away or
// with no depth (a silhouette), or a crease on either axis (a recessed window's corner).
uint grazeBin(int2 dp, int2 depthLast, uint pitch, out bool edge) {
    edge = false;
    const float d = depthNow[dp.y * pitch + dp.x];
    if (!(d > 0 && d < 1))
        return 5;
    const float z = viewZ(d);
    int2 pick[2];
    float pz[2];
    [unroll] for (int axis = 0; axis < 2; ++axis) {
        const int2 step = axis == 0 ? int2(1, 0) : int2(0, 1);
        float best = 1e30, bz = 0;
        int2 bp = dp;
        // A crease: the depth's slope (linear across a plane in screen space) changes by more than a quarter between
        // the two sides of this pixel (a corner: the wall turning into the window's reveal, even with no 10 % step).
        const int2 c0 = clamp(dp - step, int2(0, 0), depthLast), c1 = clamp(dp + step, int2(0, 0), depthLast);
        const float d0 = depthNow[c0.y * pitch + c0.x], d1 = depthNow[c1.y * pitch + c1.x];
        if (any(c0 != dp) && any(c1 != dp) && d0 > 0 && d0 < 1 && d1 > 0 && d1 < 1 &&
            abs((d1 - d) - (d - d0)) > 0.25 * max(abs(d1 - d), abs(d - d0)) + 1e-5 * d)
            edge = true;
        [unroll] for (int side = -1; side <= 1; side += 2) {
            const int2 c = clamp(dp + step * side, int2(0, 0), depthLast);
            const float nd = depthNow[c.y * pitch + c.x];
            if (any(c != dp) && !(nd > 0 && nd < 1))
                edge = true;
            if (any(c != dp) && nd > 0 && nd < 1) {
                const float nz = viewZ(nd);
                if (abs(nz - z) > 0.1 * z)
                    edge = true;
                if (abs(nz - z) < best) {
                    best = abs(nz - z);
                    bz = nz;
                    bp = c;
                }
            }
        }
        if (!(best <= 0.1 * z)) {
            edge = true;
            return 4;
        }
        pick[axis] = bp;
        pz[axis] = bz;
    }
    const float3 here = viewAt((float2(dp) + 0.5) / sizes.xy, z);
    const float3 n = cross(viewAt((float2(pick[0]) + 0.5) / sizes.xy, pz[0]) - here,
                           viewAt((float2(pick[1]) + 0.5) / sizes.xy, pz[1]) - here);
    const float len = length(n);
    if (!(len > 1e-20)) {
        edge = true;
        return 4;
    }
    const float a = abs(dot(n / len, normalize(here)));
    return a >= 0.5 ? 0 : a >= 0.25 ? 1 : a >= 0.0625 ? 2 : 3;
}
uint agePrevious(float2 old, uint w, uint h) {
    if (!(probe.x > 0) || any(old < 0) || any(old > 1))
        return 0;
    const uint2 o = min(uint2(old * float2(w, h)), uint2(w - 1, h - 1));
    return ageIn[o.y * w + o.x];
}
void noteAge(int2 p, uint w, uint h, uint match, uint kept, uint flags, float residual, float distance) {
    ageOut[p.y * w + p.x] = (min(match, 65535u) << 16) | min(kept, 65535u);
    const uint row0 = (uint)(h * 0.42), row1 = (uint)(h * 0.58);
    if ((uint)p.y != row0 && (uint)p.y != row1)
        return;
    const float columns[4] = {0.35, 0.45, 0.55, 0.65};
    [unroll] for (uint k = 0; k < 4; ++k)
        if ((uint)p.x == (uint)(w * columns[k])) {
            const uint at = w * h + 4 * (k + ((uint)p.y == row1 ? 4 : 0));
            ageOut[at] = (min(match, 65535u) << 16) | min(kept, 65535u);
            ageOut[at + 1] = flags | 1 | (probe.x > 0 ? 128 : 0);
            ageOut[at + 2] = asuint(residual);
            ageOut[at + 3] = asuint(distance);
        }
}
#endif
float4 vs(uint id : SV_VertexID) : SV_POSITION {
    const float2 corner = float2((id << 1) & 2, id & 2);
    return float4(corner * float2(2, -2) + float2(-1, 1), 0, 1);
}
float3 toYCoCg(float3 c) {
    return float3(dot(c, float3(0.25, 0.5, 0.25)), dot(c, float3(0.5, 0, -0.5)), dot(c, float3(-0.25, 0.5, -0.25)));
}
float3 fromYCoCg(float3 c) {
    return float3(c.x + c.y - c.z, c.x + c.z, c.x - c.y - c.z);
}
#ifdef TAA_UPSCALE
// Reconstruct the lower-resolution current eye directly at the full output
// size with a five-tap Catmull-Rom filter. Previous history is full-size.
float4 sampleCurrent(float2 uv) {
    uint sw, sh;
    current.GetDimensions(sw, sh);
    const float2 size = float2(sw, sh);
    const float2 at = uv * size;
    const float2 c1 = floor(at - 0.5) + 0.5;
    const float2 f = at - c1;
    const float2 w0 = f * (-0.5 + f * (1.0 - 0.5 * f));
    const float2 w1 = 1.0 + f * f * (-2.5 + 1.5 * f);
    const float2 w2 = f * (0.5 + f * (2.0 - 1.5 * f));
    const float2 w3 = f * f * (-0.5 + 0.5 * f);
    const float2 w12 = w1 + w2;
    const float2 t12 = (c1 + w2 / w12) / size;
    const float2 t0 = (c1 - 1.0) / size;
    const float2 t3 = (c1 + 2.0) / size;
    float3 sum = current.SampleLevel(bilinear, float2(t12.x, t0.y), 0).rgb * (w12.x * w0.y);
    sum += current.SampleLevel(bilinear, float2(t0.x, t12.y), 0).rgb * (w0.x * w12.y);
    sum += current.SampleLevel(bilinear, t12, 0).rgb * (w12.x * w12.y);
    sum += current.SampleLevel(bilinear, float2(t3.x, t12.y), 0).rgb * (w3.x * w12.y);
    sum += current.SampleLevel(bilinear, float2(t12.x, t3.y), 0).rgb * (w12.x * w3.y);
    const float weight = w12.x * w0.y + w0.x * w12.y + w12.x * w12.y + w3.x * w12.y + w12.x * w3.y;
    const float3 reconstructed = saturate(sum / max(weight, 1e-4));
    // Temporal-detail experiment: thin bright windows can ring as the negative
    // Catmull-Rom lobes cross their edges. Fall partway back to bilinear ONLY
    // where the two reconstructions strongly disagree. Preserve fine detail
    // elsewhere and leave the existing full-resolution AA path untouched.
    const float4 base = current.SampleLevel(bilinear, uv, 0);
    const float3 difference = abs(reconstructed - base.rgb);
    const float disagreement = max(difference.r, max(difference.g, difference.b));
    const float antiRing = 0.45 * saturate((disagreement - 0.025) * 6.0);
    return float4(lerp(reconstructed, base.rgb, antiRing), base.a);
}
#endif
)"
R"(float3 catmullRom(float2 uv, float2 size) {
    const float2 at = uv * size;
    const float2 c1 = floor(at - 0.5) + 0.5;
    const float2 f = at - c1;
    const float2 w0 = f * (-0.5 + f * (1.0 - 0.5 * f));
    const float2 w1 = 1.0 + f * f * (-2.5 + 1.5 * f);
    const float2 w2 = f * (0.5 + f * (2.0 - 1.5 * f));
    const float2 w3 = f * f * (-0.5 + 0.5 * f);
    const float2 w12 = w1 + w2;
    const float2 t12 = (c1 + w2 / w12) / size;
    const float2 t0 = (c1 - 1.0) / size;
    const float2 t3 = (c1 + 2.0) / size;
    float3 sum = previous.SampleLevel(bilinear, float2(t12.x, t0.y), 0).rgb * (w12.x * w0.y);
    sum += previous.SampleLevel(bilinear, float2(t0.x, t12.y), 0).rgb * (w0.x * w12.y);
    sum += previous.SampleLevel(bilinear, t12, 0).rgb * (w12.x * w12.y);
    sum += previous.SampleLevel(bilinear, float2(t3.x, t12.y), 0).rgb * (w3.x * w12.y);
    sum += previous.SampleLevel(bilinear, float2(t12.x, t3.y), 0).rgb * (w12.x * w3.y);
    const float weight = w12.x * w0.y + w0.x * w12.y + w12.x * w12.y + w3.x * w12.y + w12.x * w3.y;
    return saturate(sum / max(weight, 1e-4));
}
// The history at `old` read with Catmull-Rom and clipped to this pixel's 3 x 3 variance box in YCoCg; most > 1: the
// clip moved it.
float3 clipHistory(int2 p, int2 last, float2 old, float2 size, float3 middle, out float most) {
    float3 lo = middle, hi = middle, m1 = 0, m2 = 0;
    [unroll] for (int y = -1; y <= 1; ++y)
        [unroll] for (int x = -1; x <= 1; ++x) {
            const float3 n = toYCoCg(current.Load(int3(clamp(p + int2(x, y), int2(0, 0), last), 0)).rgb);
            lo = min(lo, n);
            hi = max(hi, n);
            m1 += n;
            m2 += n * n;
        }
    const float3 mean = m1 / 9.0;
    const float3 sigma = sqrt(max(m2 / 9.0 - mean * mean, 0));
#ifdef TAA_UPSCALE
    // The 70% image contains fewer samples than the full-size accumulated
    // history. During nearly still views, ease the variance clipping toward
    // the native 3x3 min/max (never beyond it) so thin windows can settle.
    // Under motion extra.x approaches 0.6 and the original tight clipping
    // returns, limiting trails on nearby moving objects.
    const float stillness = saturate((extra.x - 0.60) / 0.33);
    const float relax = 0.40 * stillness;
    const float3 boxLo = lerp(max(lo, mean - extra.w * sigma), lo, relax);
    const float3 boxHi = lerp(min(hi, mean + extra.w * sigma), hi, relax);
#else
    const float3 boxLo = max(lo, mean - extra.w * sigma);
    const float3 boxHi = min(hi, mean + extra.w * sigma);
#endif
    float3 before = toYCoCg(catmullRom(old, size));
    // Clip toward the box's centre (not a per-channel clamp): keeps the history's hue, no colour shift.
    const float3 centre = 0.5 * (boxHi + boxLo);
    const float3 extent = 0.5 * (boxHi - boxLo) + 1e-4;
    const float3 offset = before - centre;
    const float3 units = abs(offset / extent);
    most = max(units.x, max(units.y, units.z));
    if (most > 1)
        before = centre + offset / most;
    return before;
}
#ifdef TAA_AGE
// A SPIKE: this pixel's luma above its own 3 x 3 box (mean + gamma sigma, clipHistory's upper bound; luma only).
bool lumaSpike(int2 p, int2 last, float y) {
    float m1 = 0, m2 = 0;
    [unroll] for (int j = -1; j <= 1; ++j)
        [unroll] for (int i = -1; i <= 1; ++i) {
            const float n = toYCoCg(current.Load(int3(clamp(p + int2(i, j), int2(0, 0), last), 0)).rgb).x;
            m1 += n;
            m2 += n * n;
        }
    const float mean = m1 / 9.0;
    return y > mean + extra.w * sqrt(max(m2 / 9.0 - mean * mean, 0)) + 1e-4;
}
// A counted pixel into its graze bin: kind 0 no history (outside the image), 1 refused by the depth, 2 kept (most:
// the clip's); the luma change is this pixel against the history read there, before any clip; edge: grazeBin's;
// spike: lumaSpike's. Words: taa_depth_trace.hpp GrazeWord (23 a bin). A refused history shows this pixel as it
// is, so its output change is the input's (word 20 gets the same change); a kept one adds noteShown's.
void noteGraze(uint bin, uint kind, float most, bool fallback, uint age, float residual, float2 old, float2 size,
               float luma, bool edge, bool spike) {
    const uint at = bin * 92;
    graze.InterlockedAdd(at, 1);
    if (edge)
        graze.InterlockedAdd(at + 40, 1);
    if (fallback)
        graze.InterlockedAdd(at + 20, 1);
    if (residual >= 0) {
        const uint micro = (uint)min(residual * 1e6, 4e9);
        graze.InterlockedAdd(at + 32, micro);
        graze.InterlockedMax(at + 36, micro);
    }
    if (kind == 0)
        return;
    graze.InterlockedAdd(at + 4, 1);
    graze.InterlockedAdd(at + (kind == 1 ? (most > 1 ? 16 : 12) : 8), kind == 1 || most > 1 ? 1 : 0);
    graze.InterlockedAdd(at + 24, age);
    const float change = abs(luma - toYCoCg(catmullRom(old, size)).x);
    graze.InterlockedAdd(at + 28, (uint)(change * 1000 + 0.5));
    const bool moved = kind == 1 || most > 1;
    if (edge) {
        graze.InterlockedAdd(at + 44, 1);
        if (moved)
            graze.InterlockedAdd(at + (kind == 1 ? (most > 1 ? 56 : 52) : 48), 1);
        graze.InterlockedAdd(at + 60, (uint)(change * 1000 + 0.5));
        graze.InterlockedAdd(at + 84, age);
        graze.InterlockedMax(at + 88, age);
    }
    if (spike) {
        graze.InterlockedAdd(at + 64, 1);
        if (most > 1)
            graze.InterlockedAdd(at + 68, 1);
    }
    if (change > 0.25) {
        graze.InterlockedAdd(at + 72, 1);
        if (edge)
            graze.InterlockedAdd(at + 76, 1);
    }
    if (edge && kind == 1)
        graze.InterlockedAdd(at + 80, (uint)(change * 1000 + 0.5));
}
// The edge pixel's TAA output against the same history read: before (noteGraze's change, the input) vs after.
void noteShown(uint bin, bool edge, float shown, float2 old, float2 size) {
    if (edge)
        graze.InterlockedAdd(bin * 92 + 80, (uint)(abs(shown - toYCoCg(catmullRom(old, size)).x) * 1000 + 0.5));
}
)"
R"(// 0.0.76 SEAM / EDGE TRACE (taa_depth_trace.hpp SeamWord, HotWord, SampleWord; read-only): a counted pixel with a
// history and a depth into its seam cell (angle x distance x flat / seam) - the pre-TAA change (this pair's eye image
// against the history read) and the shown change (the TAA's output against the same read); a seam pixel whose 3 x 3
// luma spans over hotSpread (a lit frame face against the dark recess) also into its HOT EDGE cell (age, weight,
// rejection, spreads, bright / dark) and, while there is room, the pass's samples. kind 1: refused by the depth.
void noteSeam(int2 p, int2 last, int2 dp, int2 depthLast, uint pitch, uint bin, float z, bool edge, uint kind,
              float most, uint age, float weight, float luma, float shown, float2 old, float2 size) {
    if (bin >= 5 || !(z > 0))
        return;
    const float hist = toYCoCg(catmullRom(old, size)).x;
    const float inC = abs(luma - hist), outC = abs(shown - hist);
    const uint angle = bin == 0 ? 0 : bin == 1 ? 1 : bin == 4 ? 3 : 2;
    const uint at = 552 + ((angle * 3 + (z < 8 ? 0 : z < 30 ? 1 : 2)) * 2 + (edge ? 1 : 0)) * 20;
    graze.InterlockedAdd(at, 1);
    graze.InterlockedAdd(at + 4, (uint)(inC * 1000 + 0.5));
    graze.InterlockedAdd(at + 8, (uint)(outC * 1000 + 0.5));
    if (inC > 0.0625)
        graze.InterlockedAdd(at + 12, 1);
    if (outC > 0.0625)
        graze.InterlockedAdd(at + 16, 1);
    if (!edge)
        return;
    float lo = 1e30, hi = -1e30, m1 = 0, m2 = 0, dlo = 1e30, dhi = 0;
    [unroll] for (int j = -1; j <= 1; ++j)
        [unroll] for (int i = -1; i <= 1; ++i) {
            const float n = toYCoCg(current.Load(int3(clamp(p + int2(i, j), int2(0, 0), last), 0)).rgb).x;
            lo = min(lo, n);
            hi = max(hi, n);
            m1 += n;
            m2 += n * n;
            const int2 c = clamp(dp + int2(i, j), int2(0, 0), depthLast);
            const float nd = depthNow[c.y * pitch + c.x];
            if (nd > 0 && nd < 1) {
                const float nz = viewZ(nd);
                dlo = min(dlo, nz);
                dhi = max(dhi, nz);
            }
        }
    if (!(hi - lo > 0.25))
        return;
    const float mean = m1 / 9.0;
    const float sigma = sqrt(max(m2 / 9.0 - mean * mean, 0));
    const float spreadZ = dhi > dlo ? min((dhi - dlo) / z, 1000.0) : 0;
    const bool clipped = most > 1, refused = kind == 1, bright = luma > mean;
    const uint h = 1032 + angle * 60;
    graze.InterlockedAdd(h, 1);
    graze.InterlockedAdd(h + 4, age);
    graze.InterlockedMax(h + 8, age);
    graze.InterlockedAdd(h + 12, (uint)(weight * 1000 + 0.5));
    graze.InterlockedAdd(h + (refused ? (clipped ? 24 : 20) : (clipped ? 16 : 28)), 1);
    graze.InterlockedAdd(h + 32, (uint)((hi - lo) * 1000 + 0.5));
    graze.InterlockedAdd(h + 36, (uint)(sigma * 1000 + 0.5));
    graze.InterlockedAdd(h + 40, (uint)(spreadZ * 1000 + 0.5));
    if (bright)
        graze.InterlockedAdd(h + 44, 1);
    graze.InterlockedAdd(h + 48, (uint)(inC * 1000 + 0.5));
    graze.InterlockedAdd(h + 52, (uint)(outC * 1000 + 0.5));
    if (outC > 0.0625)
        graze.InterlockedAdd(h + 56, 1);
    uint slot;
    graze.InterlockedAdd(1272, 1, slot);
    if (slot < 8) {
        const uint s = 1276 + slot * 32;
        graze.Store(s, (uint)p.x | ((uint)p.y << 16));
        graze.Store(s + 4, age);
        graze.Store(s + 8, (clipped ? 1u : 0u) | (refused ? 2u : 0u) | (bright ? 4u : 0u) | 128u | (bin << 8));
        graze.Store(s + 12, (uint)(weight * 1000 + 0.5));
        graze.Store(s + 16, min((uint)(luma * 1000 + 0.5), 65535u) | (min((uint)((hi - lo) * 1000 + 0.5), 65535u) << 16));
        graze.Store(s + 20, asuint(depthNow[dp.y * pitch + dp.x]));
        graze.Store(s + 24, (uint)(spreadZ * 1000 + 0.5));
        graze.Store(s + 28, min((uint)(inC * 1000 + 0.5), 65535u) | (min((uint)(outC * 1000 + 0.5), 65535u) << 16));
    }
}
#endif
float4 ps(float4 position : SV_POSITION) : SV_TARGET {
    uint w, h;
#ifdef TAA_UPSCALE
    previous.GetDimensions(w, h);
#else
    current.GetDimensions(w, h);
#endif
    const int2 p = int2(position.xy);
    const int2 last = int2(w, h) - 1;
#ifdef TAA_UPSCALE
    uint sw, sh;
    current.GetDimensions(sw, sh);
    const float2 currentUv = position.xy / float2(w, h);
    const int2 sourceLast = int2(sw, sh) - 1;
    const int2 sourcePixel = clamp(int2(currentUv * float2(sw, sh)), int2(0, 0), sourceLast);
    const float4 here = sampleCurrent(currentUv);
#else
    const float4 here = current.Load(int3(p, 0));
#endif
    const float3 uv = float3(position.xy * extra.yz, 1);
    float3 q = float3(dot(row0.xyz, uv), dot(row1.xyz, uv), dot(row2.xyz, uv));
#ifdef TAA_COUNT
    const bool counted = ((p.x | p.y) & 15) == 0;
    if (counted)
        counts.InterlockedAdd(28, 1);
#endif
#ifdef TAA_DEPTH
    // The pixel put back in the world with its depth and moved into the previous eye. Static geometry only: the
    // depth only places the pixel, no motion vectors, nothing moves on its own. No usable depth (0, 1, NaN, or the
    // place outside the previous eye): this pixel keeps the head-motion place (`turned`) - fallback, not rejection.
    const float3 turned = q;
    const int2 depthLast = int2(sizes.xy) - 1;
    const uint pitch = (uint)sizes.z;
    const int2 dp = clamp(int2(uv.xy * sizes.xy), int2(0, 0), depthLast);
    const float d = depthNow[dp.y * pitch + dp.x];
    bool deep = false;
    float want = 0;
    if (d > 0 && d < 1) { // false for NaN
        const float3 moved = q / (d * lin.x + lin.y) + shift.xyz;
        if (moved.z > 1e-5) {
            const float2 at = moved.xy / moved.z;
            if (all(at >= 0) && all(at <= 1)) {
                q = moved;
                deep = true;
                want = shift.w / moved.z;
            }
        }
    }
#endif
#ifdef TAA_AGE
#ifndef TAA_DEPTH
    // 0.0.76 SEAM TRACE (vr_taa_depth off, vr_taa_seam on): the eye's depth read for the trace only; the history is
    // read where the head motion puts it, exactly as without the trace (q is not touched).
    const int2 depthLast = int2(sizes.xy) - 1;
    const uint pitch = (uint)sizes.z;
    const int2 dp = clamp(int2(uv.xy * sizes.xy), int2(0, 0), depthLast);
    const float d = depthNow[dp.y * pitch + dp.x];
    const bool deep = false;
    const bool sited = d > 0 && d < 1;
#else
    const bool sited = deep;
#endif
    const float distance = sited ? probe.y / max(d * lin.x + lin.y, 1e-12) : 0;
    const uint surface = sited && distance >= probe.z && distance <= probe.w ? 4 : 0;
    float residual = -1;
    bool edge = false;
    const uint bin = counted ? grazeBin(dp, depthLast, pitch, edge) : 5;
    const float seamZ = d > 0 && d < 1 ? viewZ(d) : 0;
    uint keptAge = 0;
#endif
    float2 old = float2(-1, -1);
    if (q.z > 1e-5)
        old = q.xy / q.z;
    if (!(extra.x > 0) || any(old < 0) || any(old > 1)) {
#ifdef TAA_AGE
        noteAge(p, w, h, 0, 0, 32 | (deep ? 2 : 0) | surface, residual, distance);
        if (counted)
            noteGraze(bin, 0, 0, !deep, 0, residual, old, float2(w, h), 0, edge, false);
#endif
        return here;
    }
#ifdef TAA_DEPTH
    bool refused = false;
    if (deep) {
        // Disocclusion: the previous eye's depth there must be the one this pixel should have, at one of the
        // 2 x 2 around the place, or this pixel was hidden last pair and starts again. None of the 2 x 2 usable:
        // the head-motion place instead (fallback).
        const int2 base = int2(floor(old * sizes.xy - 0.5));
        bool agrees = false, seen = false;
        [unroll] for (int k = 0; k < 4; ++k) {
            const int2 c = clamp(base + int2(k & 1, k >> 1), int2(0, 0), depthLast);
            const float had = depthBefore[c.y * pitch + c.x];
            if (had > 0 && had < 1) {
                seen = true;
                const float was = had * lin.z + lin.w;
                agrees = agrees || abs(was - want) <= 0.1 * max(was, want) + 1e-7;
#ifdef TAA_AGE
                const float off = abs(was - want) / max(max(was, want), 1e-12);
                residual = residual < 0 ? off : min(residual, off);
#endif
            }
        }
        if (seen) {
            refused = !agrees;
        } else {
            deep = false;
            q = turned;
            old = float2(-1, -1);
            if (q.z > 1e-5)
                old = q.xy / q.z;
            if (any(old < 0) || any(old > 1)) {
#ifdef TAA_AGE
                noteAge(p, w, h, 0, 0, 32 | 64 | surface, residual, distance);
                if (counted)
                    noteGraze(bin, 0, 0, true, 0, residual, old, float2(w, h), 0, edge, false);
#endif
                return here;
            }
        }
    }
#ifdef TAA_COUNT
    if (counted && !deep)
        counts.InterlockedAdd(20, 1);
    if (counted && deep)
        counts.InterlockedAdd(24, 1);
#endif
#endif
    const float3 middle = toYCoCg(here.rgb);
#ifdef TAA_DEPTH
    if (refused) {
#ifdef TAA_COUNT
        if (counted) { // a history refused by the depth: rejected, and was the clip going to move it too?
            float most;
            clipHistory(p, last, old, float2(w, h), middle, most);
            counts.InterlockedAdd(0, 1);
            counts.InterlockedAdd(4, 1);
            counts.InterlockedAdd(most > 1 ? 16 : 12, 1);
#ifdef TAA_AGE
            noteGraze(bin, 1, most, false, 0, residual, old, float2(w, h), middle.x, edge,
                      lumaSpike(p, last, middle.x));
            noteSeam(p, last, dp, depthLast, pitch, bin, seamZ, edge, 1, most, 0, 0, middle.x, middle.x, old,
                     float2(w, h));
#endif
        }
#endif
#ifdef TAA_AGE
        noteAge(p, w, h, 0, 0, 8 | 2 | surface, residual, distance);
#endif
        return here;
    }
#endif
    float most;
#ifdef TAA_UPSCALE
    const float3 before = clipHistory(sourcePixel, sourceLast, old, float2(w, h), middle, most);
#else
    const float3 before = clipHistory(p, last, old, float2(w, h), middle, most);
#endif
#ifdef TAA_AGE
    {
        const uint had = agePrevious(old, w, h);
        const uint match = deep ? (had >> 16) + 1 : 0;
        keptAge = most > 1 ? 0 : (had & 65535u) + 1;
        noteAge(p, w, h, match, keptAge, (deep ? 2 : 64) | surface | (most > 1 ? 16 : 0), residual, distance);
        if (counted)
            noteGraze(bin, 2, most, !deep, match, residual, old, float2(w, h), middle.x, edge,
                      lumaSpike(p, last, middle.x));
    }
#endif
#ifdef TAA_COUNT
    if (counted) {
        counts.InterlockedAdd(0, 1);
        if (most > 1) {
            counts.InterlockedAdd(4, 1);
            counts.InterlockedAdd(8, 1);
        }
    }
#endif
    const float keep = saturate(extra.x);
    const float a = (1 - keep) / (1 + middle.x), b = keep / (1 + before.x);
    const float3 mixed = (middle * a + before * b) / max(a + b, 1e-6);
#ifdef TAA_AGE
    if (counted) {
        const float shown = toYCoCg(saturate(fromYCoCg(mixed))).x;
        noteShown(bin, edge, shown, old, float2(w, h));
        noteSeam(p, last, dp, depthLast, pitch, bin, seamZ, edge, 2, most, keptAge, b / max(a + b, 1e-6), middle.x,
                 shown, old, float2(w, h));
    }
#endif
    return float4(saturate(fromYCoCg(mixed)), here.a);
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
    // 0.0.76 SPIDY TAA: its own pixel shader, the same root signature and target. Only in its mode, so another mode
    // never fails for it.
    if (spidyTaaNow()) {
        ComPtr<ID3DBlob> taaPs;
        const D3D_SHADER_MACRO upscaleDefines[] = {{"TAA_UPSCALE", "1"}, {nullptr, nullptr}};
        if (FAILED(D3DCompile(taaShader, std::strlen(taaShader), "spidy_taa",
                              temporalUpscale() ? upscaleDefines : nullptr, nullptr, "ps", "ps_5_0",
                              D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &taaPs, &error)))
            return failHistory(2);
        d.PS = {taaPs->GetBufferPointer(), taaPs->GetBufferSize()};
        if (FAILED(device->CreateGraphicsPipelineState(&d, IID_PPV_ARGS(&history.taa))))
            return failHistory(3);
    }
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
            device->CreateShaderResourceView((temporalUpscale() ? resources[i] : stagedEyes[i]).Get(), &sv, view);
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
    history.upscale = temporalUpscale();
    history.ready = true;
    history.valid = false;
    return true;
}
// 0.0.76 A-depth: taaShader's TAA_DEPTH variant, its own root signature (the history's two pictures and frame, then
// the two depths as root views and their constants). Built the first time the switch is on; a failure turns only
// A-depth off (taaDepthStop 3), the history goes on as before.
// 0.0.76 A-depth TRACE: its variant or buffers could not be made (1 root / shader, 2 pipeline, 3 eye size, 4
// buffers): the depth path goes on counted as before, untraced, for the session.
void traceFault(uint32_t code) {
    {
        std::lock_guard lock(watchLock);
        SpidyTaaDepthTraceData.ageFault = code;
    }
    char line[112];
    std::snprintf(line, sizeof line, "SpidyVR: 0.0.76 A-depth TRACE off (%u); the depth path counted as before\n", code);
    OutputDebugStringA(line);
}
bool prepareTaaDepth() {
    if (taaDepthFault.load())
        return false;
    if (history.taaDepth)
        return true;
    D3D12_DESCRIPTOR_RANGE range{D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0, 0, 0};
    D3D12_ROOT_PARAMETER parameters[5]{};
    parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    parameters[0].DescriptorTable = {1, &range};
    parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    parameters[1].Constants = {0, 0, 16};
    parameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
    parameters[2].Descriptor = {2, 0};
    parameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
    parameters[3].Descriptor = {3, 0};
    parameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    parameters[4].Constants = {1, 0, 12}; // shift, sizes, lin (A-depth CHECKED)
    for (auto& parameter : parameters)
        parameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_STATIC_SAMPLER_DESC sampler{};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.MaxLOD = D3D12_FLOAT32_MAX;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_ROOT_SIGNATURE_DESC rs{5, parameters, 1, &sampler, D3D12_ROOT_SIGNATURE_FLAG_NONE};
    ComPtr<ID3DBlob> signature, error, vs, ps;
    const D3D_SHADER_MACRO defines[] = {{"TAA_DEPTH", "1"}, {nullptr, nullptr}};
    const auto length = std::strlen(taaShader);
    if (FAILED(D3D12SerializeRootSignature(&rs, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error)) ||
        FAILED(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(),
                                           IID_PPV_ARGS(&history.depthRoot))) ||
        FAILED(D3DCompile(taaShader, length, "spidy_taa_depth", defines, nullptr, "vs", "vs_5_0",
                          D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &vs, &error)) ||
        FAILED(D3DCompile(taaShader, length, "spidy_taa_depth", defines, nullptr, "ps", "ps_5_0",
                          D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &ps, &error))) {
        history.depthRoot.Reset();
        taaDepthStop(3);
        return false;
    }
    D3D12_GRAPHICS_PIPELINE_STATE_DESC d{};
    d.pRootSignature = history.depthRoot.Get();
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
    if (FAILED(device->CreateGraphicsPipelineState(&d, IID_PPV_ARGS(&history.taaDepth)))) {
        history.depthRoot.Reset();
        history.taaDepth.Reset();
        taaDepthStop(3);
        return false;
    }
    // 0.0.76 TAA JITTER: the same with the rejection counts (a root UAV, u1, after the five). Read-only; if it cannot
    // be built A-depth runs as above and those pairs go uncounted.
    D3D12_ROOT_PARAMETER counted[6]{};
    for (unsigned k = 0; k < 5; ++k)
        counted[k] = parameters[k];
    counted[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
    counted[5].Descriptor = {1, 0};
    counted[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    const D3D12_ROOT_SIGNATURE_DESC countRs{6, counted, 1, &sampler, D3D12_ROOT_SIGNATURE_FLAG_NONE};
    const D3D_SHADER_MACRO countDefines[] = {{"TAA_DEPTH", "1"}, {"TAA_COUNT", "1"}, {nullptr, nullptr}};
    ComPtr<ID3DBlob> countSignature, countPs;
    if (FAILED(D3D12SerializeRootSignature(&countRs, D3D_ROOT_SIGNATURE_VERSION_1, &countSignature, &error)) ||
        FAILED(device->CreateRootSignature(0, countSignature->GetBufferPointer(), countSignature->GetBufferSize(),
                                           IID_PPV_ARGS(&history.depthCountRoot))) ||
        FAILED(D3DCompile(taaShader, length, "spidy_taa_depth_count", countDefines, nullptr, "ps", "ps_5_0",
                          D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &countPs, &error))) {
        history.depthCountRoot.Reset();
        return true;
    }
    d.pRootSignature = history.depthCountRoot.Get();
    d.PS = {countPs->GetBufferPointer(), countPs->GetBufferSize()};
    if (FAILED(device->CreateGraphicsPipelineState(&d, IID_PPV_ARGS(&history.taaDepthCount)))) {
        history.depthCountRoot.Reset();
        history.taaDepthCount.Reset();
        return true;
    }
    // 0.0.76 A-depth TRACE (taa_depth_trace.hpp, read-only): the counted variant once more with the history ages and
    // the graze bins (TAA_AGE: u2 this pair's ages, u3 the last pair's, u4 the bins; b1 20 constants, its probe and
    // lens after the 12). The same picture as taaDepthCount (only its UAVs differ); if it cannot be built the depth
    // path is counted as before, untraced (SpidyTaaDepthTraceData.ageFault).
    D3D12_ROOT_PARAMETER aged[9]{};
    for (unsigned k = 0; k < 6; ++k)
        aged[k] = counted[k];
    aged[4].Constants = {1, 0, 20};
    for (unsigned k = 6; k < 9; ++k) {
        aged[k].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
        aged[k].Descriptor = {k - 4, 0}; // u2 ageOut, u3 ageIn, u4 graze
        aged[k].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    }
    const D3D12_ROOT_SIGNATURE_DESC ageRs{9, aged, 1, &sampler, D3D12_ROOT_SIGNATURE_FLAG_NONE};
    const D3D_SHADER_MACRO ageDefines[] = {
        {"TAA_DEPTH", "1"}, {"TAA_COUNT", "1"}, {"TAA_AGE", "1"}, {nullptr, nullptr}};
    ComPtr<ID3DBlob> ageSignature, agePs;
    d.pRootSignature = nullptr;
    if (FAILED(D3D12SerializeRootSignature(&ageRs, D3D_ROOT_SIGNATURE_VERSION_1, &ageSignature, &error)) ||
        FAILED(device->CreateRootSignature(0, ageSignature->GetBufferPointer(), ageSignature->GetBufferSize(),
                                           IID_PPV_ARGS(&history.depthAgeRoot))) ||
        FAILED(D3DCompile(taaShader, length, "spidy_taa_depth_trace", ageDefines, nullptr, "ps", "ps_5_0",
                          D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &agePs, &error))) {
        history.depthAgeRoot.Reset();
        traceFault(1);
        return true;
    }
    d.pRootSignature = history.depthAgeRoot.Get();
    d.PS = {agePs->GetBufferPointer(), agePs->GetBufferSize()};
    if (FAILED(device->CreateGraphicsPipelineState(&d, IID_PPV_ARGS(&history.taaDepthAge)))) {
        history.depthAgeRoot.Reset();
        history.taaDepthAge.Reset();
        traceFault(2);
        return true;
    }
    // 0.0.76 SEAM TRACE: the same root, no TAA_DEPTH (t3 bound but unread). If it cannot be built vr_taa_seam only
    // leaves the head-motion path untraced.
    const D3D_SHADER_MACRO seamDefines[] = {{"TAA_COUNT", "1"}, {"TAA_AGE", "1"}, {nullptr, nullptr}};
    ComPtr<ID3DBlob> seamPs;
    if (SUCCEEDED(D3DCompile(taaShader, length, "spidy_taa_seam_trace", seamDefines, nullptr, "ps", "ps_5_0",
                             D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &seamPs, &error))) {
        d.PS = {seamPs->GetBufferPointer(), seamPs->GetBufferSize()};
        if (FAILED(device->CreateGraphicsPipelineState(&d, IID_PPV_ARGS(&history.taaSeam))))
            history.taaSeam.Reset();
    }
    if (!history.taaSeam)
        OutputDebugStringA("SpidyVR: 0.0.76 SEAM TRACE variant could not be built (vr_taa_seam traces nothing)\n");
    return true;
}
// 0.0.76 A-depth TRACE: the ages (per eye, this pair's and the last one's: w * h words and the probes' after them),
// the graze bins (a capture slot's 2 eyes, zeroed from `zeros` before each pass) and both readbacks. Made once for
// the eye size; any failure leaves the trace off for the session (the depth path counted as before).
bool prepareTaaAge(uint32_t w, uint32_t h) {
    if (!history.taaDepthAge || SpidyTaaDepthTraceData.ageFault)
        return false;
    if (history.ages[0][0] && history.agesW == w && history.agesH == h)
        return true;
    if (history.ages[0][0])
        return false; // the eyes changed size mid-session: untraced rather than reallocated under the GPU
    if (!w || !h || static_cast<uint64_t>(w) * h > 64ull * 1024 * 1024) {
        traceFault(3);
        return false;
    }
    D3D12_RESOURCE_DESC buffer{};
    buffer.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    buffer.Height = 1;
    buffer.DepthOrArraySize = 1;
    buffer.MipLevels = 1;
    buffer.SampleDesc.Count = 1;
    buffer.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    D3D12_HEAP_PROPERTIES heap{};
    heap.Type = D3D12_HEAP_TYPE_DEFAULT;
    D3D12_HEAP_PROPERTIES readback{};
    readback.Type = D3D12_HEAP_TYPE_READBACK;
    auto uav = buffer;
    uav.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    uav.Width = (static_cast<uint64_t>(w) * h + taa_depth_trace::probes * taa_depth_trace::probeWords) * 4;
    bool ok = true;
    for (auto& eye : history.ages)
        for (auto& r : eye)
            ok = ok && SUCCEEDED(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &uav,
                                                                 D3D12_RESOURCE_STATE_COMMON, nullptr,
                                                                 IID_PPV_ARGS(&r)));
    const uint64_t grazeSlot = 2ull * taa_depth_trace::grazeBytesPerEye;
    uav.Width = grazeSlot * history.countLast.size();
    ok = ok && SUCCEEDED(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &uav, D3D12_RESOURCE_STATE_COMMON,
                                                         nullptr, IID_PPV_ARGS(&history.graze)));
    buffer.Width = grazeSlot; // committed DEFAULT: zeroed, never written
    ok = ok && SUCCEEDED(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &buffer,
                                                         D3D12_RESOURCE_STATE_COMMON, nullptr,
                                                         IID_PPV_ARGS(&history.zeros)));
    buffer.Width = (grazeSlot + 2ull * taa_depth_trace::probes * taa_depth_trace::probeWords * 4) *
                   history.countLast.size();
    void* mapped{};
    ok = ok && SUCCEEDED(device->CreateCommittedResource(&readback, D3D12_HEAP_FLAG_NONE, &buffer,
                                                         D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
                                                         IID_PPV_ARGS(&history.traceRead))) &&
         SUCCEEDED(history.traceRead->Map(0, nullptr, &mapped));
    if (!ok) {
        for (auto& eye : history.ages)
            for (auto& r : eye)
                r.Reset();
        history.graze.Reset();
        history.zeros.Reset();
        history.traceRead.Reset();
        traceFault(4);
        return false;
    }
    history.traceData = static_cast<const volatile uint32_t*>(mapped);
    history.agesW = w;
    history.agesH = h;
    history.aged = {};
    return true;
}
// 0.0.76 TAA JITTER: SPIDY TAA's TAA_COUNT variant (the same picture, plus the rejection counts). Built once in its
// mode; a failure only leaves the counts out (taaCountFault), the TAA goes on through history.taa.
std::atomic<uint32_t> taaCountFault{};
std::atomic<uint32_t> taaJitterModeNow{}; // SpidyVrTaaJitter: taa_jitter::Mode, on (the default)
std::atomic<uint32_t> taaStillMilli{};    // SpidyVrTaaStill: 0 the build's still share, else 600-990 thousandths
bool prepareTaaCount() {
    if (taaCountFault.load())
        return false;
    if (history.taaCount)
        return true;
    const auto fail = [](uint32_t code) {
        history.countRoot.Reset();
        history.taaCount.Reset();
        history.counts.Reset();
        history.countRead.Reset();
        history.countData = nullptr;
        taaCountFault = code;
        char line[96];
        std::snprintf(line, sizeof line, "SpidyVR: 0.0.76 TAA JITTER counts off (%u), SPIDY TAA as before\n", code);
        OutputDebugStringA(line);
        return false;
    };
    D3D12_DESCRIPTOR_RANGE range{D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0, 0, 0};
    D3D12_ROOT_PARAMETER parameters[3]{};
    parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    parameters[0].DescriptorTable = {1, &range};
    parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    parameters[1].Constants.Num32BitValues = 16;
    parameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    parameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
    parameters[2].Descriptor = {1, 0}; // u1: the counts
    parameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_STATIC_SAMPLER_DESC sampler{};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.MaxLOD = D3D12_FLOAT32_MAX;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_ROOT_SIGNATURE_DESC rs{3, parameters, 1, &sampler, D3D12_ROOT_SIGNATURE_FLAG_NONE};
    ComPtr<ID3DBlob> signature, error, vs, ps;
    if (FAILED(D3D12SerializeRootSignature(&rs, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error)) ||
        FAILED(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(),
                                           IID_PPV_ARGS(&history.countRoot))))
        return fail(1);
    const D3D_SHADER_MACRO defines[] = {{"TAA_COUNT", "1"}, {nullptr, nullptr}};
    const auto length = std::strlen(taaShader);
    if (FAILED(D3DCompile(taaShader, length, "spidy_taa_count", defines, nullptr, "vs", "vs_5_0",
                          D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &vs, &error)) ||
        FAILED(D3DCompile(taaShader, length, "spidy_taa_count", defines, nullptr, "ps", "ps_5_0",
                          D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &ps, &error)))
        return fail(2);
    D3D12_GRAPHICS_PIPELINE_STATE_DESC d{};
    d.pRootSignature = history.countRoot.Get();
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
    if (FAILED(device->CreateGraphicsPipelineState(&d, IID_PPV_ARGS(&history.taaCount))))
        return fail(3);
    D3D12_RESOURCE_DESC buffer{};
    buffer.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    buffer.Width = 64 * history.countLast.size();
    buffer.Height = 1;
    buffer.DepthOrArraySize = 1;
    buffer.MipLevels = 1;
    buffer.SampleDesc.Count = 1;
    buffer.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    D3D12_HEAP_PROPERTIES heap{};
    heap.Type = D3D12_HEAP_TYPE_DEFAULT;
    auto uav = buffer;
    uav.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    D3D12_HEAP_PROPERTIES readback{};
    readback.Type = D3D12_HEAP_TYPE_READBACK;
    void* mapped{};
    // A committed DEFAULT buffer starts zeroed: the counts are running totals, read as differences.
    if (FAILED(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &uav, D3D12_RESOURCE_STATE_COMMON, nullptr,
                                               IID_PPV_ARGS(&history.counts))) ||
        FAILED(device->CreateCommittedResource(&readback, D3D12_HEAP_FLAG_NONE, &buffer,
                                               D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
                                               IID_PPV_ARGS(&history.countRead))) ||
        FAILED(history.countRead->Map(0, nullptr, &mapped)))
        return fail(4);
    history.countData = static_cast<const volatile uint32_t*>(mapped);
    history.countLast = {};
    history.counted = {};
    return true;
}
// 0.0.76 A-depth TRACE (read-only): what drawHistory saw of the depth this pair (each eye's on its own, and the
// path it took), noted with the pair's time and mode once the pair is staged (notePairTrace).
struct PairTrace {
    bool drawn{}, wanted{}, used{}, seam{};
    std::array<bool, 2> nowValid{}, beforeValid{};
} pairTrace;
// 0.0.76 TAA JITTER: the slot's counts (its list is done) into the running TAA WATCH window.
void noteHistoryCount(size_t slot) {
    if (slot >= history.counted.size() || !history.counted[slot] || !history.countData)
        return;
    history.counted[slot] = false;
    uint32_t now[16]{};
    for (unsigned k = 0; k < 16; ++k)
        now[k] = history.countData[16 * slot + k];
    auto& last = history.countLast[slot];
    uint32_t eyes[2][taa_depth_check::counters]{};
    for (unsigned i = 0; i < 2; ++i)
        for (unsigned k = 0; k < taa_depth_check::counters; ++k)
            eyes[i][k] = now[taa_depth_check::counters * i + k] - last[taa_depth_check::counters * i + k];
    using taa_depth_check::rejectedAt, taa_depth_check::sampledAt;
    const uint32_t sampled = eyes[0][sampledAt] + eyes[1][sampledAt], clipped = eyes[0][rejectedAt] + eyes[1][rejectedAt];
    for (unsigned k = 0; k < 16; ++k)
        last[k] = now[k];
    std::lock_guard lock(watchLock);
    taa_watch::noteRejection(SpidyTaaWatchData.current, history.countStill[slot], sampled, clipped);
    // 0.0.76 A-depth CHECKED: clip / depth / both apart, per path and movement
    taa_depth_check::noteCounts(SpidyTaaDepthCheckData.current, history.countPath[slot], history.countCategory[slot],
                                eyes);
    // 0.0.76 A-depth TRACE: the slot's graze bins (this pass's alone) and probe words, per eye
    if (slot < history.traced.size() && history.traced[slot] && history.traceData) {
        history.traced[slot] = false;
        constexpr uint32_t grazeWordsPerEye = taa_depth_trace::grazeBytesPerEye / 4;
        constexpr uint32_t probeWordsPerEye = taa_depth_trace::probes * taa_depth_trace::probeWords;
        const volatile uint32_t* base = history.traceData + (2 * grazeWordsPerEye + 2 * probeWordsPerEye) * slot;
        for (unsigned i = 0; i < 2; ++i) {
            uint32_t graze[grazeWordsPerEye]{}, probe[probeWordsPerEye]{};
            for (uint32_t k = 0; k < grazeWordsPerEye; ++k)
                graze[k] = base[grazeWordsPerEye * i + k];
            for (uint32_t k = 0; k < probeWordsPerEye; ++k)
                probe[k] = base[2 * grazeWordsPerEye + probeWordsPerEye * i + k];
            taa_depth_trace::noteGraze(SpidyTaaDepthTraceData.current, history.countCategory[slot], graze);
            taa_depth_trace::noteProbes(SpidyTaaDepthTraceData.current, history.countCategory[slot], probe);
            // 0.0.76 SEAM / EDGE TRACE: the seam cells and hot edges (both eyes), eye 0's samples
            taa_depth_trace::noteSeam(SpidyTaaDepthTraceData.current, history.countCategory[slot], graze);
            if (i == 0)
                taa_depth_trace::noteSamples(SpidyTaaDepthTraceData.current, history.countCategory[slot], graze);
        }
    }
}
// 0.0.76 A-depth TRACE (read-only): one staged pair into the trace's 10 s window: its time since the last (a hitch
// line past hitchMs), the mode it ran in and, when drawHistory ran, each eye's depth.
void notePairTrace(uint64_t serial) {
    static int64_t lastQpc{};
    static uint64_t startMs{};
    const int64_t nowQpc = qpcNow();
    const float ms = lastQpc ? static_cast<float>(qpcMs(lastQpc, nowQpc)) : 0.f;
    lastQpc = nowQpc;
    const PairTrace seen = pairTrace;
    pairTrace = {};
    std::lock_guard lock(watchLock);
    auto& d = SpidyTaaDepthTraceData;
    taa_depth_trace::roll(d, startMs, GetTickCount64());
    auto& w = d.current;
    ++w.pairs;
    w.aaMode = vrAaMode.load(std::memory_order_relaxed);
    w.depthSetting = taaDepthOn.load(std::memory_order_relaxed);
    w.seamSetting = taaSeamOn.load(std::memory_order_relaxed);
    w.eyeW = width;
    w.eyeH = height;
    w.outW = outWidth;
    w.outH = outHeight;
    w.ssPercent = supersample();
    taa_depth_trace::noteFrame(d, static_cast<uint64_t>(qpcMs(0, nowQpc) * 1000.0), ms,
                               static_cast<uint32_t>(serial));
    if (!seen.drawn)
        return;
    w.wanted += seen.wanted;
    w.used += seen.used;
    w.seamPairs += seen.seam;
    if (!seen.wanted)
        return;
    bool eye[2]{};
    for (unsigned i = 0; i < 2; ++i) {
        w.nowValid[i] += seen.nowValid[i];
        w.beforeValid[i] += seen.beforeValid[i];
        eye[i] = seen.nowValid[i] && seen.beforeValid[i];
    }
    ++w.masks[taa_depth_trace::eyeMask(eye[0], eye[1])];
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
    if (end > begin) { // 0.0.76 TAA WATCH
        std::lock_guard lock(watchLock);
        const auto ms = static_cast<float>(static_cast<double>(end - begin) / history.ticksPerMs);
        SpidyTaaWatchData.current.taaGpuMs += ms;
        ++SpidyTaaWatchData.current.taaTimed;
        if (history.timedOurs[slot] && ms < 250) { // 0.0.76 A-depth CHECKED: the pass with and without the depth
            auto& w = SpidyTaaDepthCheckData.current;
            if (history.timedDepth[slot]) {
                w.depthDrawMs += ms;
                ++w.depthDrawTimed;
            } else {
                w.plainDrawMs += ms;
                ++w.plainDrawTimed;
            }
        }
    }
    if (end > begin) {
        historyClock.note(static_cast<double>(end - begin) / history.ticksPerMs);
        std::lock_guard lock(historyMedianLock);
        historyMedian.note(static_cast<float>(static_cast<double>(end - begin) / history.ticksPerMs));
    }
}
// 0.0.76 TAA WATCH: the game's eye jobs' GPU span for the pair this capture slot last carried (its list is done).
void noteEyeTime(size_t slot) {
    if (slot >= eyeTimes.timed.size() || !eyeTimes.timed[slot] || !eyeTimes.data)
        return;
    eyeTimes.timed[slot] = false;
    uint64_t t[4]{};
    for (unsigned k = 0; k < 4; ++k)
        t[k] = eyeTimes.data[4 * slot + k];
    if (!(t[1] > t[0]) || !(t[3] > t[2]))
        return;
    const uint64_t first = std::min(t[0], t[2]), last = std::max(t[1], t[3]);
    const double ms = static_cast<double>(last - first) / eyeTimes.ticksPerMs;
    if (!(ms > 0) || ms > 250)
        return; // not one pair's work (a stale slot)
    std::lock_guard lock(watchLock);
    SpidyTaaWatchData.current.eyeGpuMs += static_cast<float>(ms);
    ++SpidyTaaWatchData.current.eyeTimed;
}
void resolveEyeTime(ID3D12GraphicsCommandList* list, size_t slot, uint64_t serial) { // `recording` held
    if (!eyeTimes.queries || slot >= eyeTimes.timed.size() || !serial)
        return;
    const unsigned ring = static_cast<unsigned>(serial % eyeTimes.serial.size());
    if (eyeTimes.serial[ring] != serial || eyeTimes.marks[ring] != 0xf)
        return;
    list->ResolveQueryData(eyeTimes.queries.Get(), D3D12_QUERY_TYPE_TIMESTAMP, ring * 4, 4, eyeTimes.stamps.Get(),
                           sizeof(uint64_t) * 4 * slot);
    eyeTimes.marks[ring] = 0; // read once
    eyeTimes.timed[slot] = true;
}
// 0.0.76 A-depth CHECKED (taa_depth_check.hpp): the depth copies' GPU time for the pair this slot carried.
void resolveDepthCopyTime(ID3D12GraphicsCommandList* list, size_t slot, uint64_t serial) { // depthLock held
    auto& t = depthCopyTimes;
    if (!t.queries || slot >= t.timed.size() || !serial)
        return;
    const unsigned ring = static_cast<unsigned>(serial % t.serial.size());
    if (t.serial[ring] != serial || t.marks[ring] != 0xf)
        return;
    list->ResolveQueryData(t.queries.Get(), D3D12_QUERY_TYPE_TIMESTAMP, ring * 4, 4, t.stamps.Get(),
                           sizeof(uint64_t) * 4 * slot);
    t.marks[ring] = 0; // read once
    t.timed[slot] = true;
}
void noteDepthCopyTime(size_t slot) {
    double ms = 0;
    {
        std::lock_guard lock(depthLock);
        auto& t = depthCopyTimes;
        if (slot >= t.timed.size() || !t.timed[slot] || !t.data)
            return;
        t.timed[slot] = false;
        uint64_t v[4]{};
        for (unsigned k = 0; k < 4; ++k)
            v[k] = t.data[4 * slot + k];
        if (!(v[1] > v[0]) || !(v[3] > v[2]))
            return;
        ms = static_cast<double>((v[1] - v[0]) + (v[3] - v[2])) / t.ticksPerMs;
    }
    if (!(ms > 0) || ms > 50)
        return; // not one pair's copies (a stale slot)
    std::lock_guard lock(watchLock);
    SpidyTaaDepthCheckData.current.copyMs += static_cast<float>(ms);
    ++SpidyTaaDepthCheckData.current.copyTimed;
}
// The convention check's state for the session (taa_depth_check::State; failed stays failed) and the convention the
// shader is given once it passed.
std::atomic<uint32_t> depthCheckState{taa_depth_check::waiting};
std::atomic<uint32_t> depthCheckInForce{taa_depth_check::conventions};
// The check's probe: a band of rows about each eye's centre row of one pair's depth (both eyes), copied on the
// history's list into a readback buffer and checked when that capture slot is done. One at a time (depthLock).
struct DepthProbe {
    ComPtr<ID3D12Resource> read;
    const volatile float* data{};
    UINT64 bytes{};
    bool pending{}, failed{};
    size_t slot{};
    uint64_t serial{};
    uint32_t pitch{}, firstRow{};
    std::array<taa_depth_check::Lens, 2> lens{};
    uint32_t pairsSince{};
} depthProbe;
inline constexpr uint32_t probeWaiting = 30, probePassed = 45; // pairs between probes: until it passed, after
bool recordDepthProbe(ID3D12GraphicsCommandList* list, size_t slot, uint64_t serial,
                      const std::array<TaaDepthView, 2>& at, const std::array<taa_depth_check::Lens, 2>& lens) {
    auto& p = depthProbe; // depthLock held
    using taa_depth_check::bandRows;
    if (p.failed || p.pending || !device || taaDepthH < bandRows || !taaDepthPitch || !at[0].buffer || !at[1].buffer)
        return false;
    const UINT64 rowBytes = static_cast<UINT64>(taaDepthPitch) * sizeof(float), eyeBytes = rowBytes * bandRows;
    if (!p.read || p.bytes < 2 * eyeBytes) {
        if (p.read)
            retiredDepth.push_back({std::move(p.read), GetTickCount64()});
        p.data = nullptr;
        p.bytes = 0;
        D3D12_HEAP_PROPERTIES readback{};
        readback.Type = D3D12_HEAP_TYPE_READBACK;
        D3D12_RESOURCE_DESC buffer{};
        buffer.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        buffer.Width = 2 * eyeBytes;
        buffer.Height = 1;
        buffer.DepthOrArraySize = 1;
        buffer.MipLevels = 1;
        buffer.SampleDesc.Count = 1;
        buffer.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        void* mapped{};
        if (FAILED(device->CreateCommittedResource(&readback, D3D12_HEAP_FLAG_NONE, &buffer,
                                                   D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&p.read))) ||
            FAILED(p.read->Map(0, nullptr, &mapped)) || !mapped) {
            p.read.Reset();
            p.failed = true; // no check, so the depth path stays off (never on unchecked)
            OutputDebugStringA("SpidyVR: 0.0.76 A-depth CHECK could not make its readback: depth path stays off\n");
            return false;
        }
        p.data = static_cast<const volatile float*>(mapped);
        p.bytes = 2 * eyeBytes;
    }
    const uint32_t firstRow = taaDepthH / 2 - bandRows / 2;
    // The depth buffers stay in COMMON: a buffer is promoted to COPY_SOURCE (and the history's read) by itself.
    for (unsigned i = 0; i < 2; ++i)
        list->CopyBufferRegion(p.read.Get(), eyeBytes * i, at[i].buffer, rowBytes * firstRow, eyeBytes);
    p.pending = true;
    p.slot = slot;
    p.serial = serial;
    p.pitch = taaDepthPitch;
    p.firstRow = firstRow;
    p.lens = lens;
    return true;
}
void cancelDepthProbe(size_t slot) { // the slot's list was not submitted
    std::lock_guard lock(depthLock);
    if (depthProbe.pending && depthProbe.slot == slot)
        depthProbe.pending = false;
}
// The probe's check (its slot is done): the round trip and the stereo test, the verdict, the log line.
void noteDepthCheck(size_t slot) {
    namespace tdc = taa_depth_check;
    std::array<tdc::Lens, 2> lens{};
    std::vector<float> band;
    uint32_t pitch{}, firstRow{};
    uint64_t serial{};
    {
        std::lock_guard lock(depthLock);
        auto& p = depthProbe;
        if (!p.pending || p.slot != slot || !p.data)
            return;
        p.pending = false;
        pitch = p.pitch;
        firstRow = p.firstRow;
        serial = p.serial;
        lens = p.lens;
        band.resize(static_cast<size_t>(2) * tdc::bandRows * pitch);
        for (size_t k = 0; k < band.size(); ++k)
            band[k] = p.data[k];
    }
    const size_t eyeFloats = static_cast<size_t>(tdc::bandRows) * pitch;
    const tdc::Band bands[2] = {{band.data(), firstRow, tdc::bandRows, pitch},
                                {band.data() + eyeFloats, firstRow, tdc::bandRows, pitch}};
    double trip[2] = {NAN, NAN};
    bool inFrustum[2]{};
    float centreDepth[2] = {NAN, NAN}, centreDistance[2] = {NAN, NAN};
    int32_t centreOffset[2]{};
    const bool lensOk = lens[0].nearM > 0 && lens[0].farM > lens[0].nearM && lens[1].nearM > 0 &&
                        lens[1].farM > lens[1].nearM && lens[0].width && lens[0].height;
    const uint32_t row = firstRow + tdc::bandRows / 2;
    for (unsigned i = 0; i < 2 && lensOk; ++i) {
        // The centre pixel, or the nearest pixel on the centre row with a usable depth.
        const int64_t cx = lens[i].width / 2;
        for (int64_t off = 0; off < static_cast<int64_t>(lens[i].width / 2); off = off > 0 ? -off : 1 - off) {
            const int64_t x = cx + off;
            if (x < 0 || x >= static_cast<int64_t>(lens[i].width))
                continue;
            const float d = bands[i].at(static_cast<uint32_t>(x), row);
            if (!tdc::usable(d))
                continue;
            const auto t = tdc::roundTrip(lens[i], false, static_cast<double>(x) + .5, static_cast<double>(row) + .5, d);
            trip[i] = t.errorPx;
            inFrustum[i] = t.inFrustum;
            centreDepth[i] = d;
            centreDistance[i] = static_cast<float>(t.distance);
            centreOffset[i] = static_cast<int32_t>(off);
            break;
        }
    }
    const tdc::Stereo st = lensOk ? tdc::stereo(lens[0], lens[1], bands[0], bands[1]) : tdc::Stereo{};
    tdc::Verdict v = lensOk ? tdc::decide(trip, inFrustum, st) : tdc::Verdict{tdc::waiting, tdc::noLens, tdc::conventions};
    const uint32_t before = depthCheckState.load();
    const uint32_t after = tdc::nextState(before, v);
    depthCheckState = after;
    if (after == tdc::passed && v.outcome == tdc::passed)
        depthCheckInForce = v.convention;
    const uint32_t inForce = depthCheckInForce.load();
    const char* stateName = after == tdc::passed ? "PASSED" : after == tdc::failed ? "FAILED" : "WAITING";
    char line[640];
    std::snprintf(line, sizeof line,
                  "SpidyVR: 0.0.76 A-depth CHECK pair %llu: lens convention D3D 0..1, reversed-Z, reciprocal, finite far "
                  "(near %.4g m, far %.6g m, jittered %.3g,%.3g px); round trip L %.4g px R %.4g px (limit 0.05; "
                  "centre depth L %.6g R %.6g, offset L %d R %d); stereo %u edge points: rev-finite %.0f%% "
                  "rev-infinite %.0f%% standard %.0f%% gl %.0f%% linear %.0f%%; best %s; %s (%s)%s%s\n",
                  static_cast<unsigned long long>(serial), lens[0].nearM, lens[0].farM, lens[0].jitter[0],
                  lens[0].jitter[1], trip[0], trip[1], centreDepth[0], centreDepth[1], centreOffset[0], centreOffset[1],
                  st.tested, 100 * st.share(0), 100 * st.share(1), 100 * st.share(2), 100 * st.share(3),
                  100 * st.share(4), tdc::conventionName(v.convention), stateName, tdc::reasonText(v.reason),
                  v.outcome == tdc::failed && v.reason == tdc::otherConvention ? "; likely convention error: the depth is "
                                                                                 : "",
                  v.outcome == tdc::failed && v.reason == tdc::otherConvention ? tdc::conventionName(v.convention) : "");
    OutputDebugStringA(line);
    std::lock_guard lock(watchLock);
    auto& c = SpidyTaaDepthCheckData.check;
    ++c.probes;
    if (v.outcome == tdc::passed)
        ++c.passed;
    else if (v.outcome == tdc::failed)
        ++c.failed;
    else
        ++c.inconclusive;
    c.state = after;
    c.convention = v.convention;
    c.reason = v.reason;
    c.tested = st.tested;
    for (uint32_t k = 0; k < tdc::conventions; ++k)
        c.agree[k] = static_cast<float>(st.share(k));
    for (unsigned i = 0; i < 2; ++i) {
        c.roundTrip[i] = static_cast<float>(trip[i]);
        c.centreDepth[i] = centreDepth[i];
        c.centreDistance[i] = centreDistance[i];
        c.centreOffset[i] = centreOffset[i];
    }
    c.nearM = static_cast<float>(lens[0].nearM);
    c.farM = static_cast<float>(lens[0].farM);
    c.clearReversed = static_cast<uint32_t>(std::min<uint64_t>(depthClearReversed.load(), 0xffffffffu));
    c.clearStandard = static_cast<uint32_t>(std::min<uint64_t>(depthClearStandard.load(), 0xffffffffu));
    c.inForce = inForce;
    c.serial = static_cast<uint32_t>(serial);
    auto& w = SpidyTaaDepthCheckData.current;
    ++w.checks;
    if (v.outcome == tdc::passed)
        ++w.checksPassed;
    const float worst = static_cast<float>(std::fmax(std::isfinite(trip[0]) ? trip[0] : 0, std::isfinite(trip[1]) ? trip[1] : 0));
    if (std::isfinite(trip[0]) || std::isfinite(trip[1])) {
        w.roundTripMax = std::max(w.roundTripMax, worst);
        w.roundTripLast = worst;
    }
    for (unsigned i = 0; i < 2; ++i) // 0.0.76 A-depth TRACE: each eye's round trip, mean and max per 10 s
        taa_depth_trace::noteTrip(SpidyTaaDepthTraceData.current, trip[i]);
}
bool findViews(uint64_t serial, std::array<eye_history::EyeView, 2>& out, NotedViews* noted = nullptr) {
    std::lock_guard lock(viewLock);
    for (const auto& n : notedViews)
        if (n.serial == serial && serial) {
            out = n.eyes;
            if (noted)
                *noted = n;
            return true;
        }
    return false;
}
// The staged pair (in COPY_SOURCE) blended with the history, turned to its
// view: drawn into the other picture, which is copied back into the staged
// image and kept for the next pair.
void drawHistory(ID3D12GraphicsCommandList* list, size_t slot, uint64_t serial) {
    pairTrace = {};
    pairTrace.drawn = true;
    std::array<eye_history::EyeView, 2> now{};
    NotedViews noted{};
    const bool viewed = findViews(serial, now, &noted);
    // 0.0.76 JITTER CHECK (read-only): with a still head, eye 0's step now and last pair and the turn's shift of
    // the eye's centre, both in eye pixels (taa_watch::noteJitterCheck).
    const std::array<float, 2> jitterBefore = history.shownJitter;
    bool jitterChecked = false;
    float jitterShift[2]{};
    taa_frame_log::Entry frameEntry{}; // 0.0.76 TAA FRAME LOG: eye 0's pair (logged once the pass is recorded)
    const uint32_t percent = historyNow(), loosenPercent = historyPercent.load(), trail = trailPercent.load();
    float frame[2][16]{};
    const bool ours = spidyTaaNow() && history.taa;
    bool blended = false, cut = false, moving = false;
    float maxTravel = 0, leastKeep = 1, leastStill = 0;
    bool watchStill = false, watchEased = false; // 0.0.76 TAA WATCH (eye 0, as `moving`)
    float watchTurn = 0, watchTravel = 0;        // 0.0.76 A-depth CHECKED: eye 0's, for the movement category
    // 0.0.76 A-depth (live.txt vr_taa_depth): both eyes' depth of this pair and of the one shown, or the history
    // is turned as before (counted as depth missed). The blend shares and the clip are the same either way.
    // A-depth CHECKED (taa_depth_check.hpp): and only once the depth's convention check has passed. Per-eye gating:
    // one eye's depth (this pair's or the shown one's) missing and BOTH eyes are turned by head motion this pair, so
    // no eye ever uses the depth while the other does not; the pictures kept are the same either way (the depth only
    // moves where the history is read), so neither path's history is spoilt for the other.
    const bool depthWanted = ours && taaDepthActive();
    // 0.0.76 SEAM TRACE: with vr_taa_depth off, vr_taa_seam reads the same depth (the same gates and check) for the
    // trace alone; seamUsed never moves the history.
    const bool seamWanted = ours && !history.upscale && !depthWanted && taaSeamOn.load(std::memory_order_relaxed);
    bool depthUsed = false;
    bool depthFound[2]{};
    uint32_t depthGate = 0; // 0 used, 1 an eye's depth missing, 2 check not passed yet, 3 check failed
    std::array<TaaDepthView, 2> depthNowView{}, depthBeforeView{};
    float depthFrame[2][12]{};
    if ((depthWanted || seamWanted) && viewed && prepareTaaDepth()) {
        std::lock_guard lock(depthLock);
        const bool sized = taaDepthW && taaDepthH && taaDepthPitch;
        for (unsigned i = 0; i < 2; ++i)
            depthFound[i] = sized && taaDepthFor(serial, i, depthNowView[i]);
        bool ready = history.valid && history.shownSerial && depthFound[0] && depthFound[1];
        for (unsigned i = 0; i < 2 && ready; ++i)
            ready = taaDepthFor(history.shownSerial, i, depthBeforeView[i]);
        const uint32_t checkState = depthCheckState.load();
        const uint32_t inForce = depthCheckInForce.load();
        if (!ready)
            depthGate = 1;
        else if (checkState == taa_depth_check::failed)
            depthGate = 3;
        else if (checkState != taa_depth_check::passed || inForce >= taa_depth_check::conventions)
            depthGate = 2;
        depthUsed = depthGate == 0;
        for (unsigned i = 0; i < 2 && depthUsed; ++i) {
            std::array<float, 3> shift{};
            depthUsed = taa_depth::historyShift(history.shown[i], now[i], shift);
            const float nearNow = depthNowView[i].nearM;
            for (unsigned k = 0; k < 3; ++k)
                depthFrame[i][k] = shift[k] / nearNow;
            depthFrame[i][3] = depthBeforeView[i].nearM / nearNow;
            depthFrame[i][4] = static_cast<float>(taaDepthW);
            depthFrame[i][5] = static_cast<float>(taaDepthH);
            depthFrame[i][6] = static_cast<float>(taaDepthPitch);
            // lin: each pair's stored depth to near / distance in the checked convention (its own near and far)
            taa_depth_check::linearOf(inForce, depthNowView[i].nearM, depthNowView[i].farM, depthFrame[i][8],
                                      depthFrame[i][9]);
            taa_depth_check::linearOf(inForce, depthBeforeView[i].nearM, depthBeforeView[i].farM, depthFrame[i][10],
                                      depthFrame[i][11]);
            if (inForce == taa_depth_check::reversedFinite &&
                !(depthNowView[i].farM > depthNowView[i].nearM && depthBeforeView[i].farM > depthBeforeView[i].nearM))
                depthUsed = false; // no far to read the finite convention with
            for (float v : depthFrame[i])
                depthUsed = depthUsed && std::isfinite(v);
        }
        if (depthGate == 0 && !depthUsed)
            depthGate = 1;
        // 0.0.76 A-depth TRACE (read-only): each eye's depth on its own (this pair's, the shown one's), what a
        // per-eye choice would see; the path above stays both eyes or neither.
        for (unsigned i = 0; i < 2; ++i) {
            TaaDepthView before{};
            pairTrace.nowValid[i] = depthFound[i];
            pairTrace.beforeValid[i] = sized && history.valid && history.shownSerial &&
                                       taaDepthFor(history.shownSerial, i, before);
        }
        // The convention check's probe of this pair's depth (both eyes there), until it passed one pair in 30, then
        // about one a second; none once it failed.
        if (sized && depthFound[0] && depthFound[1] && checkState != taa_depth_check::failed) {
            ++depthProbe.pairsSince;
            if (depthProbe.pairsSince >= (checkState == taa_depth_check::passed ? probePassed : probeWaiting)) {
                std::array<taa_depth_check::Lens, 2> lens{};
                for (unsigned i = 0; i < 2; ++i) {
                    lens[i].view = now[i];
                    lens[i].nearM = depthNowView[i].nearM;
                    lens[i].farM = depthNowView[i].farM;
                    lens[i].width = static_cast<uint32_t>(taaDepthW);
                    lens[i].height = taaDepthH;
                    for (unsigned k = 0; k < 2; ++k) // this pair's step, in the depth's pixels
                        lens[i].jitter[k] = noted.size[k] ? static_cast<double>(noted.jitter[k]) *
                                                                (k ? static_cast<double>(taaDepthH) : static_cast<double>(taaDepthW)) /
                                                                noted.size[k]
                                                          : 0.0;
                }
                if (recordDepthProbe(list, slot, serial, depthNowView, lens))
                    depthProbe.pairsSince = 0;
            }
        }
        if (depthFound[0] && depthFound[1])
            resolveDepthCopyTime(list, slot, serial);
    }
    const uint32_t stillMilli = taaStillMilli.load(std::memory_order_relaxed); // 0.0.76 TAA STILL (0: the build's)
    static int64_t watchQpc{};
    const int64_t watchNow = qpcNow();
    const float watchSeconds = watchQpc ? static_cast<float>(qpcMs(watchQpc, watchNow) / 1000.0) : 0.f;
    watchQpc = watchNow;
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
        float keep = eye_history::movedWeight(still, travelCut, turnCut, trail);
        if (ours) // 0.0.76 SPIDY TAA (vr_aa.hpp): eased, not cut, while moving; the clip keeps it from ghosting
            keep = vr_aa::keep(both, turn, travel);
        if (ours) // 0.0.76 TAA STILL (live.txt vr_taa_still): the still share only, unset the build's own
            keep = taa_still::withStill(keep, stillMilli);
        // In the native 70% temporal-upscale experiment, preserve a little
        // more full-size per-eye history only when the view is nearly still.
        // The extra history vanishes continuously as head motion/travel rises;
        // camera cuts and missing reprojection still force keep = 0.
        if (ours && history.upscale && both && keep > 0.f)
            keep = std::min(0.99f, keep + 0.035f * (1.f - vr_aa::motion(turn, travel)));
        for (int r = 0; r < 3; ++r>
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
        if (ours) {
            frame[i][3] = 0;                                 // row0.w: no LIGHT HOLD (the luma weights do it)
            frame[i][15] = vr_aa::clipGamma(turn, travel);   // extra.w: the variance clip's width
        }
        blended = blended || keep > 0;
        if (i == 0 && both) {
            maxTravel = travel;
            cut = still > 0 && !(keep > 0);
            moving = keep > 0 && (ours ? vr_aa::motion(turn, travel)
                                       : eye_history::motionShare(travelCut, turnCut)) > .5f;
            leastKeep = keep;
            leastStill = ours ? keep : still; // SPIDY TAA: JITTER CALM keeps the full step while anything is kept
            watchStill = taa_watch::stillHead(turn, travel, watchSeconds);
            watchTurn = turn;
            watchTravel = travel;
            const float qz = h[6] * .5f + h[7] * .5f + h[8];
            if (watchStill && noted.size[0] && noted.size[1] && qz > 1e-5f) {
                jitterShift[0] = ((h[0] * .5f + h[1] * .5f + h[2]) / qz - .5f) * static_cast<float>(noted.size[0]);
                jitterShift[1] = ((h[3] * .5f + h[4] * .5f + h[5]) / qz - .5f) * static_cast<float>(noted.size[1]);
                jitterChecked = true;
            }
            watchEased = keep > 0 && keep < (ours ? taa_still::share(stillMilli) : still) - 1e-4f;
        }
        if (i == 0 && viewed) { // 0.0.76 TAA FRAME LOG (read-only)
            frameEntry.serial = static_cast<uint32_t>(serial);
            frameEntry.flags = (ours ? taa_frame_log::ours : 0u) |
                               (taaJitterModeNow.load(std::memory_order_relaxed) & 0xffu) << 8;
            frameEntry.turn = turn;
            frameEntry.travel = travel;
            for (int k = 0; k < 2; ++k) {
                frameEntry.jitterNow[k] = noted.jitter[k];
                frameEntry.jitterBefore[k] = jitterBefore[k];
            }
            for (int k = 0; k < 16; ++k) {
                frameEntry.world[k] = now[0].world[k];
                frameEntry.worldBefore[k] = history.shown[0].world[k];
            }
            for (int k = 0; k < 4; ++k) {
                frameEntry.tangents[k] = now[0].tangents[k];
                frameEntry.tangentsBefore[k] = history.shown[0].tangents[k];
            }
            const float qz = h[6] * .5f + h[7] * .5f + h[8];
            if (both && noted.size[0] && noted.size[1] && qz > 1e-5f) {
                frameEntry.flags |= taa_frame_log::history | (watchStill ? taa_frame_log::still : 0u);
                frameEntry.uv[0] = (h[0] * .5f + h[1] * .5f + h[2]) / qz;
                frameEntry.uv[1] = (h[3] * .5f + h[4] * .5f + h[5]) / qz;
                frameEntry.shift[0] = (frameEntry.uv[0] - .5f) * static_cast<float>(noted.size[0]);
                frameEntry.shift[1] = (frameEntry.uv[1] - .5f) * static_cast<float>(noted.size[1]);
                taa_watch::jitterShift(frameEntry.jitterNow, frameEntry.jitterBefore, 1.f, frameEntry.expected);
            }
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
    // Direct temporal upscale reads the game's original smaller eye texture.
    // Restore the state that the game supplied once the GPU history draw ends.
    D3D12_RESOURCE_BARRIER nativeBarriers[2]{};
    UINT nativeBarrierCount = 0;
    if (history.upscale) {
        for (unsigned i = 0; i < 2; ++i)
            if (!(states[i] & D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)) {
                auto& transition = nativeBarriers[nativeBarrierCount++];
                transition.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                transition.Transition = {resources[i].Get(), D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, states[i],
                                         D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE};
            }
        if (nativeBarrierCount)
            originalBarrier(list, nativeBarrierCount, nativeBarriers);
    }
    // 0.0.76 TAA JITTER: the same TAA with the rejection counts when its variant is built (else as before).
    // A-depth: its own counted variant (TAA_DEPTH + TAA_COUNT), or no counts that pair.
    // 0.0.76 SEAM TRACE: the depth read above was for the trace only (vr_taa_depth off): the history is turned by
    // head motion as without it.
    const bool seamReady = seamWanted && depthUsed && history.taaSeam;
    if (!depthWanted)
        depthUsed = false;
    const bool counting = ours && !history.upscale && slot < history.counted.size() && prepareTaaCount() &&
                          (!depthUsed || history.taaDepthCount);
    // 0.0.76 A-depth TRACE (read-only): the counted depth pass with the ages and graze bins when they are there.
    // SEAM TRACE: or the counted head-motion pass, its depth read for the bins alone (seamTraced).
    const bool seamTraced = counting && seamReady && prepareTaaAge(outWidth, outHeight);
    const bool tracing = seamTraced || (counting && depthUsed && prepareTaaAge(outWidth, outHeight));
    pairTrace.seam = seamTraced;
    pairTrace.wanted = depthWanted;
    pairTrace.used = depthUsed;
    const uint64_t grazeSlot = 2ull * taa_depth_trace::grazeBytesPerEye;
    const uint64_t probeBytes = taa_depth_trace::probes * taa_depth_trace::probeWords * 4;
    const uint64_t traceSlot = grazeSlot + 2 * probeBytes;
    const uint64_t agesBytes = static_cast<uint64_t>(history.agesW) * history.agesH * 4;
    if (tracing) {
        D3D12_RESOURCE_BARRIER t[5]{};
        for (unsigned n = 0; n < 5; ++n)
            t[n].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        for (unsigned i = 0; i < 2; ++i)
            for (unsigned k = 0; k < 2; ++k)
                t[2 * i + k].Transition = {history.ages[i][k].Get(), D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                                           D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS};
        t[4].Transition = {history.graze.Get(), D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_STATE_COMMON,
                           D3D12_RESOURCE_STATE_COPY_DEST};
        originalBarrier(list, 5, t);
        list->CopyBufferRegion(history.graze.Get(), grazeSlot * slot, history.zeros.Get(), 0, grazeSlot);
        t[0].Transition = {history.graze.Get(), D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                           D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS};
        originalBarrier(list, 1, t);
    }
    D3D12_RESOURCE_BARRIER countBarrier{};
    countBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    if (counting) {
        countBarrier.Transition = {history.counts.Get(), D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                                   D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS};
        originalBarrier(list, 1, &countBarrier);
    }
    if (seamTraced) {
        list->SetPipelineState(history.taaSeam.Get());
        list->SetGraphicsRootSignature(history.depthAgeRoot.Get());
    } else if (depthUsed) {
        list->SetPipelineState(tracing    ? history.taaDepthAge.Get()
                               : counting ? history.taaDepthCount.Get()
                                          : history.taaDepth.Get());
        list->SetGraphicsRootSignature(tracing    ? history.depthAgeRoot.Get()
                                       : counting ? history.depthCountRoot.Get()
                                                  : history.depthRoot.Get());
    } else {
        list->SetPipelineState(counting ? history.taaCount.Get() : ours ? history.taa.Get() : history.state.Get());
        list->SetGraphicsRootSignature(counting ? history.countRoot.Get() : history.root.Get());
    }
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
        if (depthUsed || seamTraced) { // 0.0.76 A-depth (SEAM TRACE: the same root and constants, the depth unmoving)
            list->SetGraphicsRootShaderResourceView(2, depthNowView[i].at);
            list->SetGraphicsRootShaderResourceView(3, depthBeforeView[i].at);
            if (tracing) { // 0.0.76 A-depth TRACE: the same 12, then probe and lens
                float traced[20]{};
                for (unsigned k = 0; k < 12; ++k)
                    traced[k] = depthFrame[i][k];
                traced[12] = history.aged[i] ? 1.f : 0.f;
                traced[13] = depthNowView[i].nearM;
                traced[14] = taa_depth_trace::probeNearM;
                traced[15] = taa_depth_trace::probeFarM;
                for (unsigned k = 0; k < 4; ++k)
                    traced[16 + k] = now[i].tangents[k];
                list->SetGraphicsRoot32BitConstants(4, 20, traced, 0);
            } else
                list->SetGraphicsRoot32BitConstants(4, 12, depthFrame[i], 0);
        }
        if (counting) // 0.0.76 TAA JITTER: the counts (u1), after the root's own parameters
            list->SetGraphicsRootUnorderedAccessView(depthUsed || seamTraced ? 5 : 2,
                                                     history.counts->GetGPUVirtualAddress() + 64 * slot + 32 * i);
        if (tracing) {
            list->SetGraphicsRootUnorderedAccessView(6, history.ages[i][write]->GetGPUVirtualAddress());
            list->SetGraphicsRootUnorderedAccessView(7, history.ages[i][read]->GetGPUVirtualAddress());
            list->SetGraphicsRootUnorderedAccessView(8, history.graze->GetGPUVirtualAddress() + grazeSlot * slot +
                                                            taa_depth_trace::grazeBytesPerEye * i);
        }
        list->OMSetRenderTargets(1, &target, FALSE, nullptr);
        list->DrawInstanced(3, 1, 0, 0);
    }
    for (UINT i = 0; i < nativeBarrierCount; ++i)
        std::swap(nativeBarriers[i].Transition.StateBefore, nativeBarriers[i].Transition.StateAfter);
    if (nativeBarrierCount)
        originalBarrier(list, nativeBarrierCount, nativeBarriers);
    for (unsigned i = 0; i < 2; ++i) {
        transition(2 * i, stagedEyes[i].Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                   D3D12_RESOURCE_STATE_COPY_DEST);
        transition(2 * i + 1, history.pictures[i][write].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET,
                   D3D12_RESOURCE_STATE_COPY_SOURCE);
    }
    originalBarrier(list, 4, b);
    if (counting) { // 0.0.76 TAA JITTER: this slot's counts to its readback, back to COMMON
        countBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        countBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
        originalBarrier(list, 1, &countBarrier);
        list->CopyBufferRegion(history.countRead.Get(), 64 * slot, history.counts.Get(), 64 * slot, 64);
        countBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
        countBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COMMON;
        originalBarrier(list, 1, &countBarrier);
        history.counted[slot] = true;
        history.countStill[slot] = watchStill;
        history.countPath[slot] = depthUsed ? taa_depth_check::depthPath : taa_depth_check::headPath;
        history.countCategory[slot] = taa_depth_check::category(watchStill, watchTurn, watchTravel, watchSeconds);
    }
    if (tracing) { // 0.0.76 A-depth TRACE: this slot's graze bins and probe words to the readback, all back to COMMON
        D3D12_RESOURCE_BARRIER t[5]{};
        for (unsigned n = 0; n < 5; ++n)
            t[n].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        for (unsigned i = 0; i < 2; ++i) {
            t[2 * i].Transition = {history.ages[i][write].Get(), D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                                   D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE};
            t[2 * i + 1].Transition = {history.ages[i][read].Get(), D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                                       D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON};
        }
        t[4].Transition = {history.graze.Get(), D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                           D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE};
        originalBarrier(list, 5, t);
        list->CopyBufferRegion(history.traceRead.Get(), traceSlot * slot, history.graze.Get(), grazeSlot * slot,
                               grazeSlot);
        for (unsigned i = 0; i < 2; ++i)
            list->CopyBufferRegion(history.traceRead.Get(), traceSlot * slot + grazeSlot + probeBytes * i,
                                   history.ages[i][write].Get(), agesBytes, probeBytes);
        for (unsigned i = 0; i < 2; ++i)
            t[i].Transition = {history.ages[i][write].Get(), D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                               D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_COMMON};
        t[2].Transition = {history.graze.Get(), D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                           D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_COMMON};
        originalBarrier(list, 3, t);
        history.traced[slot] = true;
    }
    history.aged = {tracing, tracing}; // the next pair's ageIn: these, or none
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
        history.timedOurs[slot] = ours;
        history.timedDepth[slot] = depthUsed;
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
    history.shownSerial = serial;
    history.shownJitter = noted.jitter;
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
    if (ours)
        ++SpidyVrAaData.taaPairs;
    {
        std::lock_guard watch(watchLock); // 0.0.76 TAA WATCH: the 10 s windows
        static uint64_t windowStart{};
        taa_watch::roll(SpidyTaaWatchData, windowStart, GetTickCount64());
        taa_watch::notePair(SpidyTaaWatchData.current, !blended, moving, watchEased, watchStill);
        if (depthWanted) { // 0.0.76 A-depth: turned with the eyes' depth, or as before
            if (depthUsed)
                ++SpidyTaaWatchData.current.depthPairs;
            else
                ++SpidyTaaWatchData.current.depthMissed;
        }
        static uint64_t depthWindowStart{}; // 0.0.76 A-depth CHECKED: its own 10 s windows
        taa_depth_check::roll(SpidyTaaDepthCheckData, depthWindowStart, GetTickCount64());
        if (ours) {
            auto& w = SpidyTaaDepthCheckData.current;
            ++w.pairs;
            if (depthWanted) {
                ++w.wanted;
                w.found[0] += depthFound[0] ? 1 : 0;
                w.found[1] += depthFound[1] ? 1 : 0;
                if (depthUsed)
                    ++w.used;
                else if (depthGate == 3)
                    ++w.refused;
                else if (depthGate == 2)
                    ++w.unchecked;
                else
                    ++w.gated;
            }
        }
        SpidyTaaWatchData.current.setup =
            taa_watch::setupWith(SpidyTaaWatchData.current.setup, taaJitterModeNow.load(std::memory_order_relaxed),
                                 ours ? static_cast<uint32_t>(taa_still::share(stillMilli) * 1000.f + .5f) : 0u);
        if (jitterChecked) // 0.0.76 JITTER CHECK
            taa_watch::noteJitterCheck(SpidyTaaWatchData.current, noted.jitter.data(), jitterBefore.data(),
                                       jitterShift);
        if (viewed) { // 0.0.76 TAA FRAME LOG
            SpidyTaaFrameLogData.width = noted.size[0];
            SpidyTaaFrameLogData.height = noted.size[1];
            taa_frame_log::note(SpidyTaaFrameLogData, frameEntry);
        }
    }
}
// 0.0.76 SPIDY TAA (vr_aa.hpp): SMAA 1x per eye (spidy/smaa: Jimenez et al.'s SMAA.hlsl, preset HIGH, luma edges),
// live.txt vr_aa = smaa, and SPIDY TAA's own fallback when its pass cannot be built. No history: nothing temporal
// to shake or ghost. Reads stagedEyes (after the plain copy) and writes it back; one edges / weights / output set,
// used by each eye in turn. Built on the first pair that wants it, under the submission and recording locks.
struct SmaaPipeline {
    ComPtr<ID3D12RootSignature> root;
    ComPtr<ID3D12PipelineState> edges, weights, blend;
    ComPtr<ID3D12Resource> edgeTex, weightTex, output, area, search, upload;
    ComPtr<ID3D12DescriptorHeap> views, targets;
    ComPtr<ID3D12QueryHeap> queries;
    ComPtr<ID3D12Resource> stamps;
    const volatile uint64_t* stampData{};
    double ticksPerMs{};
    std::array<bool, 3> timed{};
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT areaAt{}, searchAt{};
    bool uploaded{}, ready{};
} smaaPass;
std::atomic<uint32_t> smaaFault{};
std::atomic<bool> smaaLast{}; // the last pair went through SMAA
PassClock smaaClock;
bool failSmaa(uint32_t code) {
    smaaFault = code;
    smaaPass.ready = false;
    std::lock_guard lock(copyTimingLock);
    SpidyVrAaData.smaaFault = code;
    return false;
}
bool prepareSmaa() {
    if (smaaFault)
        return false;
    if (smaaPass.ready)
        return true;
    D3D12_DESCRIPTOR_RANGE range{D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 3, 0, 0, 0};
    D3D12_ROOT_PARAMETER parameters[2]{};
    parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    parameters[0].DescriptorTable = {1, &range};
    parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    parameters[1].Constants.Num32BitValues = 4;
    parameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL; // SMAA_RT_METRICS, used by both stages
    D3D12_STATIC_SAMPLER_DESC samplers[2]{};
    for (unsigned k = 0; k < 2; ++k) {
        samplers[k].Filter = k ? D3D12_FILTER_MIN_MAG_MIP_POINT : D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        samplers[k].AddressU = samplers[k].AddressV = samplers[k].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        samplers[k].MaxLOD = D3D12_FLOAT32_MAX;
        samplers[k].ShaderRegister = k;
        samplers[k].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    }
    D3D12_ROOT_SIGNATURE_DESC rs{2, parameters, 2, samplers, D3D12_ROOT_SIGNATURE_FLAG_NONE};
    ComPtr<ID3DBlob> signature, error;
    if (FAILED(D3D12SerializeRootSignature(&rs, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error)) ||
        FAILED(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(),
                                           IID_PPV_ARGS(&smaaPass.root))))
        return failSmaa(1);
    struct Stage {
        const char *vs, *ps;
        DXGI_FORMAT format;
        ComPtr<ID3D12PipelineState>* state;
    } stages[3] = {{"edgeVs", "edgePs", DXGI_FORMAT_R8G8_UNORM, &smaaPass.edges},
                   {"weightVs", "weightPs", DXGI_FORMAT_R8G8B8A8_UNORM, &smaaPass.weights},
                   {"blendVs", "blendPs", DXGI_FORMAT_R8G8B8A8_UNORM, &smaaPass.blend}};
    for (const auto& stage : stages) {
        ComPtr<ID3DBlob> vs, ps;
        if (FAILED(D3DCompile(smaa::shader, sizeof(smaa::shader) - 1, "spidy_smaa", nullptr, nullptr, stage.vs,
                              "vs_5_0", D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &vs, &error)) ||
            FAILED(D3DCompile(smaa::shader, sizeof(smaa::shader) - 1, "spidy_smaa", nullptr, nullptr, stage.ps,
                              "ps_5_0", D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &ps, &error)))
            return failSmaa(2);
        D3D12_GRAPHICS_PIPELINE_STATE_DESC d{};
        d.pRootSignature = smaaPass.root.Get();
        d.VS = {vs->GetBufferPointer(), vs->GetBufferSize()};
        d.PS = {ps->GetBufferPointer(), ps->GetBufferSize()};
        d.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        d.NumRenderTargets = 1;
        d.RTVFormats[0] = stage.format;
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
        if (FAILED(device->CreateGraphicsPipelineState(&d, IID_PPV_ARGS(stage.state->ReleaseAndGetAddressOf()))))
            return failSmaa(3);
    }
    // Targets at the headset's size: edges (RG8), weights (RGBA8), the output (as stagedEyes, copied back).
    D3D12_HEAP_PROPERTIES heap{};
    heap.Type = D3D12_HEAP_TYPE_DEFAULT;
    auto texture = [&](DXGI_FORMAT format, UINT64 w, UINT h, D3D12_RESOURCE_FLAGS flags, D3D12_RESOURCE_STATES state,
                       ComPtr<ID3D12Resource>& out) {
        D3D12_RESOURCE_DESC desc{};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        desc.Width = w;
        desc.Height = h;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.Format = format;
        desc.SampleDesc.Count = 1;
        desc.Flags = flags;
        return SUCCEEDED(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, state, nullptr,
                                                         IID_PPV_ARGS(&out)));
    };
    auto outDesc = stagedEyes[0]->GetDesc();
    outDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    if (!texture(DXGI_FORMAT_R8G8_UNORM, outWidth, outHeight, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
                 D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, smaaPass.edgeTex) ||
        !texture(DXGI_FORMAT_R8G8B8A8_UNORM, outWidth, outHeight, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
                 D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, smaaPass.weightTex) ||
        FAILED(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &outDesc,
                                               D3D12_RESOURCE_STATE_COPY_SOURCE, nullptr,
                                               IID_PPV_ARGS(&smaaPass.output))) ||
        !texture(DXGI_FORMAT_R8G8_UNORM, AREATEX_WIDTH, AREATEX_HEIGHT, D3D12_RESOURCE_FLAG_NONE,
                 D3D12_RESOURCE_STATE_COPY_DEST, smaaPass.area) ||
        !texture(DXGI_FORMAT_R8_UNORM, SEARCHTEX_WIDTH, SEARCHTEX_HEIGHT, D3D12_RESOURCE_FLAG_NONE,
                 D3D12_RESOURCE_STATE_COPY_DEST, smaaPass.search))
        return failSmaa(5);
    // The two look-up textures, staged in an upload buffer (copied on the first pass's list).
    UINT64 areaBytes{}, searchBytes{};
    const auto areaDesc = smaaPass.area->GetDesc(), searchDesc = smaaPass.search->GetDesc();
    device->GetCopyableFootprints(&areaDesc, 0, 1, 0, &smaaPass.areaAt, nullptr, nullptr, &areaBytes);
    device->GetCopyableFootprints(&searchDesc, 0, 1, 0, &smaaPass.searchAt, nullptr, nullptr, &searchBytes);
    smaaPass.searchAt.Offset = (areaBytes + D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT - 1) &
                               ~static_cast<UINT64>(D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT - 1);
    D3D12_HEAP_PROPERTIES uploadHeap{};
    uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;
    D3D12_RESOURCE_DESC buffer{};
    buffer.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    buffer.Width = smaaPass.searchAt.Offset + searchBytes;
    buffer.Height = 1;
    buffer.DepthOrArraySize = 1;
    buffer.MipLevels = 1;
    buffer.SampleDesc.Count = 1;
    buffer.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    void* bytes{};
    if (FAILED(device->CreateCommittedResource(&uploadHeap, D3D12_HEAP_FLAG_NONE, &buffer,
                                               D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                               IID_PPV_ARGS(&smaaPass.upload))) ||
        FAILED(smaaPass.upload->Map(0, nullptr, &bytes)))
        return failSmaa(5);
    for (UINT y = 0; y < AREATEX_HEIGHT; ++y)
        std::memcpy(static_cast<uint8_t*>(bytes) + smaaPass.areaAt.Offset + y * smaaPass.areaAt.Footprint.RowPitch,
                    areaTexBytes + y * AREATEX_PITCH, AREATEX_PITCH);
    for (UINT y = 0; y < SEARCHTEX_HEIGHT; ++y)
        std::memcpy(static_cast<uint8_t*>(bytes) + smaaPass.searchAt.Offset +
                        y * smaaPass.searchAt.Footprint.RowPitch,
                    searchTexBytes + y * SEARCHTEX_PITCH, SEARCHTEX_PITCH);
    smaaPass.upload->Unmap(0, nullptr);
    // Views (every table full, 3 each): eye i's edges [staged i, area, search] at 3i; weights [edges, area, search]
    // at 6; eye i's blend [staged i, weights, search] at 9 + 3i. Targets: edges 0, weights 1, output 2.
    D3D12_DESCRIPTOR_HEAP_DESC views{};
    views.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    views.NumDescriptors = 15;
    views.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    D3D12_DESCRIPTOR_HEAP_DESC targetViews{};
    targetViews.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    targetViews.NumDescriptors = 3;
    if (FAILED(device->CreateDescriptorHeap(&views, IID_PPV_ARGS(&smaaPass.views))) ||
        FAILED(device->CreateDescriptorHeap(&targetViews, IID_PPV_ARGS(&smaaPass.targets))))
        return failSmaa(4);
    const auto viewStep = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    const auto targetStep = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    auto srv = [&](ID3D12Resource* r, DXGI_FORMAT format, unsigned at) {
        D3D12_SHADER_RESOURCE_VIEW_DESC sv{};
        sv.Format = format;
        sv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        sv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        sv.Texture2D.MipLevels = 1;
        auto view = smaaPass.views->GetCPUDescriptorHandleForHeapStart();
        view.ptr += static_cast<SIZE_T>(viewStep) * at;
        device->CreateShaderResourceView(r, &sv, view);
    };
    auto rtv = [&](ID3D12Resource* r, DXGI_FORMAT format, unsigned at) {
        D3D12_RENDER_TARGET_VIEW_DESC rv{};
        rv.Format = format;
        rv.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        auto target = smaaPass.targets->GetCPUDescriptorHandleForHeapStart();
        target.ptr += static_cast<SIZE_T>(targetStep) * at;
        device->CreateRenderTargetView(r, &rv, target);
    };
    for (unsigned i = 0; i < 2; ++i) {
        srv(stagedEyes[i].Get(), DXGI_FORMAT_R8G8B8A8_UNORM, 3 * i);
        srv(smaaPass.area.Get(), DXGI_FORMAT_R8G8_UNORM, 3 * i + 1);
        srv(smaaPass.search.Get(), DXGI_FORMAT_R8_UNORM, 3 * i + 2);
        srv(stagedEyes[i].Get(), DXGI_FORMAT_R8G8B8A8_UNORM, 9 + 3 * i);
        srv(smaaPass.weightTex.Get(), DXGI_FORMAT_R8G8B8A8_UNORM, 10 + 3 * i);
        srv(smaaPass.search.Get(), DXGI_FORMAT_R8_UNORM, 11 + 3 * i);
    }
    srv(smaaPass.edgeTex.Get(), DXGI_FORMAT_R8G8_UNORM, 6);
    srv(smaaPass.area.Get(), DXGI_FORMAT_R8G8_UNORM, 7);
    srv(smaaPass.search.Get(), DXGI_FORMAT_R8_UNORM, 8);
    rtv(smaaPass.edgeTex.Get(), DXGI_FORMAT_R8G8_UNORM, 0);
    rtv(smaaPass.weightTex.Get(), DXGI_FORMAT_R8G8B8A8_UNORM, 1);
    rtv(smaaPass.output.Get(), DXGI_FORMAT_R8G8B8A8_UNORM, 2);
    // The GPU time is optional: without timestamps the pass still runs.
    UINT64 frequency{};
    D3D12_QUERY_HEAP_DESC qd{};
    qd.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
    qd.Count = 2 * static_cast<UINT>(captureCommands.size());
    D3D12_HEAP_PROPERTIES readback{};
    readback.Type = D3D12_HEAP_TYPE_READBACK;
    D3D12_RESOURCE_DESC stampBuffer = buffer;
    stampBuffer.Width = sizeof(uint64_t) * qd.Count;
    void* mapped{};
    if (SUCCEEDED(queue->GetTimestampFrequency(&frequency)) && frequency &&
        SUCCEEDED(device->CreateQueryHeap(&qd, IID_PPV_ARGS(&smaaPass.queries))) &&
        SUCCEEDED(device->CreateCommittedResource(&readback, D3D12_HEAP_FLAG_NONE, &stampBuffer,
                                                  D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
                                                  IID_PPV_ARGS(&smaaPass.stamps))) &&
        SUCCEEDED(smaaPass.stamps->Map(0, nullptr, &mapped))) {
        smaaPass.stampData = static_cast<const volatile uint64_t*>(mapped);
        smaaPass.ticksPerMs = static_cast<double>(frequency) / 1000.0;
    } else {
        smaaPass.queries.Reset();
        smaaPass.stamps.Reset();
    }
    smaaPass.ready = true;
    return true;
}
void noteSmaaTime(size_t slot) {
    if (!smaaPass.timed[slot] || !smaaPass.stampData)
        return;
    smaaPass.timed[slot] = false;
    const uint64_t begin = smaaPass.stampData[2 * slot], end = smaaPass.stampData[2 * slot + 1];
    if (end > begin) {
        const double ms = static_cast<double>(end - begin) / smaaPass.ticksPerMs;
        smaaClock.note(ms);
        std::lock_guard lock(copyTimingLock);
        SpidyVrAaData.smaaGpuMs += ms;
        ++SpidyVrAaData.smaaTimed;
    }
}
// Both stagedEyes (COPY_SOURCE in and out) through SMAA's three passes. True: the look-up textures' upload is on
// this list (undone by smaaListLost if the list is not submitted).
bool drawSmaa(ID3D12GraphicsCommandList* list, size_t slot) {
    const auto first = static_cast<UINT>(2 * slot);
    if (smaaPass.queries)
        originalEndQuery(list, smaaPass.queries.Get(), D3D12_QUERY_TYPE_TIMESTAMP, first);
    D3D12_RESOURCE_BARRIER b[3]{};
    auto transition = [&](unsigned n, ID3D12Resource* r, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to) {
        b[n].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        b[n].Transition = {r, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, from, to};
    };
    const bool uploading = !smaaPass.uploaded;
    if (uploading) {
        D3D12_TEXTURE_COPY_LOCATION src{}, dst{};
        src.pResource = smaaPass.upload.Get();
        src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        src.PlacedFootprint = smaaPass.areaAt;
        dst.pResource = smaaPass.area.Get();
        list->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
        src.PlacedFootprint = smaaPass.searchAt;
        dst.pResource = smaaPass.search.Get();
        list->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
        transition(0, smaaPass.area.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        transition(1, smaaPass.search.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
                   D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        originalBarrier(list, 2, b);
        smaaPass.uploaded = true;
    }
    for (unsigned i = 0; i < 2; ++i) {
        transition(0, stagedEyes[i].Get(), D3D12_RESOURCE_STATE_COPY_SOURCE,
                   D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        originalBarrier(list, 1, b);
    }
    list->SetGraphicsRootSignature(smaaPass.root.Get());
    ID3D12DescriptorHeap* heaps[] = {smaaPass.views.Get()};
    list->SetDescriptorHeaps(1, heaps);
    const D3D12_VIEWPORT viewport{0, 0, static_cast<float>(outWidth), static_cast<float>(outHeight), 0, 1};
    const D3D12_RECT rect{0, 0, static_cast<LONG>(outWidth), static_cast<LONG>(outHeight)};
    list->RSSetViewports(1, &viewport);
    list->RSSetScissorRects(1, &rect);
    list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    const float metrics[4] = {1.f / static_cast<float>(outWidth), 1.f / static_cast<float>(outHeight),
                              static_cast<float>(outWidth), static_cast<float>(outHeight)};
    const auto viewStep = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    const auto targetStep = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    auto table = [&](unsigned at) {
        auto view = smaaPass.views->GetGPUDescriptorHandleForHeapStart();
        view.ptr += static_cast<UINT64>(viewStep) * at;
        return view;
    };
    auto target = [&](unsigned at) {
        auto t = smaaPass.targets->GetCPUDescriptorHandleForHeapStart();
        t.ptr += static_cast<SIZE_T>(targetStep) * at;
        return t;
    };
    const float clear[4]{};
    for (unsigned i = 0; i < 2; ++i) {
        // Edges (cleared: the pass discards where there is none).
        transition(0, smaaPass.edgeTex.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                   D3D12_RESOURCE_STATE_RENDER_TARGET);
        originalBarrier(list, 1, b);
        auto t = target(0);
        list->ClearRenderTargetView(t, clear, 0, nullptr);
        list->OMSetRenderTargets(1, &t, FALSE, nullptr);
        list->SetPipelineState(smaaPass.edges.Get());
        list->SetGraphicsRootDescriptorTable(0, table(3 * i));
        list->SetGraphicsRoot32BitConstants(1, 4, metrics, 0);
        list->DrawInstanced(3, 1, 0, 0);
        // Blending weights.
        transition(0, smaaPass.edgeTex.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET,
                   D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        transition(1, smaaPass.weightTex.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                   D3D12_RESOURCE_STATE_RENDER_TARGET);
        originalBarrier(list, 2, b);
        t = target(1);
        list->OMSetRenderTargets(1, &t, FALSE, nullptr);
        list->SetPipelineState(smaaPass.weights.Get());
        list->SetGraphicsRootDescriptorTable(0, table(6));
        list->DrawInstanced(3, 1, 0, 0);
        // Neighbourhood blending into the output, copied back to the eye.
        transition(0, smaaPass.weightTex.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET,
                   D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        transition(1, smaaPass.output.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
        originalBarrier(list, 2, b);
        t = target(2);
        list->OMSetRenderTargets(1, &t, FALSE, nullptr);
        list->SetPipelineState(smaaPass.blend.Get());
        list->SetGraphicsRootDescriptorTable(0, table(9 + 3 * i));
        list->DrawInstanced(3, 1, 0, 0);
        transition(0, smaaPass.output.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE);
        transition(1, stagedEyes[i].Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                   D3D12_RESOURCE_STATE_COPY_DEST);
        originalBarrier(list, 2, b);
        list->CopyResource(stagedEyes[i].Get(), smaaPass.output.Get());
        transition(0, stagedEyes[i].Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COPY_SOURCE);
        originalBarrier(list, 1, b);
    }
    if (smaaPass.queries) {
        originalEndQuery(list, smaaPass.queries.Get(), D3D12_QUERY_TYPE_TIMESTAMP, first + 1);
        list->ResolveQueryData(smaaPass.queries.Get(), D3D12_QUERY_TYPE_TIMESTAMP, first, 2, smaaPass.stamps.Get(),
                               sizeof(uint64_t) * first);
        smaaPass.timed[slot] = true;
    }
    return uploading;
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
    noteHistoryCount(index); // 0.0.76 TAA JITTER
    noteSmaaTime(index);
    noteEyeTime(index); // 0.0.76 TAA WATCH
    noteDepthCopyTime(index); // 0.0.76 A-depth CHECKED
    noteDepthCheck(index);
    // Supersampled eyes cannot be copied: they are always filtered down,
    // without FXAA (the extra samples already smooth the edges).
    const bool down = downsampling();
    // 0.0.76 SPIDY TAA (vr_aa.hpp): SMAA when asked, or when SPIDY TAA's own pass cannot run (it failed to build,
    // or EYE AA's lens step is off); FXAA if SMAA cannot be built either; a plain copy past that. Never nothing.
    const uint32_t vrMode = vrAaMode.load(std::memory_order_relaxed);
    const bool sm1 = sm1Lock.load();
    const bool taaFailed = vrMode == vr_aa::ours && historyFault.load() != 0;
    const bool wantSmaa = !down && !sm1 &&
                          (vrMode == vr_aa::smaa || (vrMode == vr_aa::ours && (taaFailed || !temporalOn.load())));
    const bool useSmaa = wantSmaa && prepareSmaa();
    // Native 70% -> full-size headset history in one pass. Keep the old
    // spatial resize as a fail-safe if the new reconstruction cannot start.
    const bool directTemporal = temporalUpscale() && prepareHistory() && history.upscale && history.taa;
    const bool smooth = !directTemporal &&
                        (down || (aaMode.load() == 1 && !sm1 && !vrAaLocked()) || (wantSmaa && !useSmaa)) &&
                        prepareAa(); // SM1STYLE, SPIDY TAA, SMAA: no FXAA (but SMAA's own fallback)
    if (down && !smooth && !directTemporal) {
        fault = 3806; // the headset would get nothing: say why (aaFault)
        return;
    }
    if (smooth)
        drawAa(slot->list.Get(), index);
    else if (!directTemporal)
        for (unsigned i = 0; i < 2; ++i)
            copyEyeTexture(slot->list.Get(), resources[i].Get(), states[i], stagedEyes[i].Get(), originalBarrier,
                           D3D12_RESOURCE_STATE_COPY_SOURCE);
    const bool smaaUploading = useSmaa && drawSmaa(slot->list.Get(), index);
    // Under directTemporal, this pass IS the full-resolution upscaler.
    if (directTemporal || (historyNow() && prepareHistory())) {
        drawHistory(slot->list.Get(), index, stamp.serial);
    } else {
        history.valid = false;
        history.aged = {}; // 0.0.76 A-depth TRACE: no ages for the next pass to read
        keptMilli.store(0, std::memory_order_relaxed); // JITTER CALM: no history, no step to average
    }
    copyMirrorView(slot->list.Get()); // 0.0.71: after the AA and history, so neither smears it
    resolveEyeTime(slot->list.Get(), index, stamp.serial); // 0.0.76 TAA WATCH: read-only
    if (!submitCommands(*slot)) {
        history.valid = false;
        cancelDepthProbe(index); // 0.0.76 A-depth CHECKED: that probe's copy never ran
        if (smaaUploading)
            smaaPass.uploaded = false; // the look-up textures are still to copy
        return;
    }
    aaPairs += smooth;
    smaaLast.store(useSmaa, std::memory_order_relaxed);
    notePairTrace(stamp.serial); // 0.0.76 A-depth TRACE: read-only
    {
        std::lock_guard lock(copyTimingLock);
        const bool taaRan = spidyTaaNow() && history.taa && historyNow() && !historyFault.load();
        SpidyVrAaData.running = taaRan ? vr_aa::taa : useSmaa ? vr_aa::smaaPass : smooth && !down && wantSmaa
                                                                                       ? vr_aa::fxaaPass
                                                                                       : vr_aa::none;
        SpidyVrAaData.historyFault = historyFault.load();
        SpidyVrAaData.fellBack = SpidyVrAaData.fellBack || taaFailed;
        SpidyVrAaData.smaaPairs += useSmaa;
        SpidyVrAaData.fxaaPairs += smooth && !down && wantSmaa;
    }
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
// HERO VR 0.0.77 CUT7 WRIST MAP SOURCE: the HUD texture found, the engine object it was found under, its copy.
ComPtr<ID3D12Resource> hudResource;
uintptr_t hudRoot{};
Screen hudShot;
uint64_t hudMs{};
std::array<CopyCommands, 3> hudCommands;
std::vector<Retired> retiredHud;
std::atomic<uint32_t> hudWhy{}; // hudSource reasons
enum HudWhy : uint32_t { hudIdle, hudNoPanel, hudHunt, hudFound, hudCopied, hudNotReadable, hudBusy, hudFormat };
const char* hudWhyName(uint32_t w) {
    static const char* names[] = {"idle (not wanted)", "no HUD panel (the HUD is not on the panel now)",
                                  "hunting the panel's texture", "found", "copied", "not in a readable state",
                                  "copies in flight", "format not readable"};
    return w < 8 ? names[w] : "?";
}
// Present thread: which candidate the panel's engine objects (their first 0x200 bytes, and 0x80 bytes behind each
// pointer there) hold. Read-only, SEH-guarded reads of the game's memory.
ID3D12Resource* hudScan(uintptr_t root, uintptr_t colour) {
    std::lock_guard lock(hudLock);
    if (hudCandidates.empty())
        return nullptr;
    for (const auto object : {root, colour}) {
        uint64_t q[64]{};
        if (!object || !read(object, q, sizeof(q)))
            continue;
        for (const auto v : q)
            if (hudCandidates.count(reinterpret_cast<ID3D12Resource*>(v)))
                return reinterpret_cast<ID3D12Resource*>(v);
        for (const auto v : q) {
            if (v < 0x10000 || (v & 7) || v > 0x7fffffffffffull)
                continue;
            uint64_t inner[16]{};
            if (!read(static_cast<uintptr_t>(v), inner, sizeof(inner)))
                continue;
            for (const auto w : inner)
                if (hudCandidates.count(reinterpret_cast<ID3D12Resource*>(w)))
                    return reinterpret_cast<ID3D12Resource*>(w);
        }
    }
    return nullptr;
}
void captureHud() {
    uintptr_t colour{};
    uint32_t w{}, h{};
    const auto root = native_hud::panelObjects(colour, w, h);
    if (!root || !w || !h) {
        hudHunting = false;
        hudWhy = hudNoPanel;
        return;
    }
    if (root != hudRoot || !hudResource) {
        if (hudResource) { // a load made a new panel: the old texture is let go after its copies
            std::lock_guard submitLock(submission);
            retiredHud.push_back({hudResource, GetTickCount64() + 3000});
            hudResource.Reset();
            hudTracked = nullptr;
            ++hudLost;
        }
        hudRoot = root;
        if (!hudHunting.load() || hudHuntW.load() != w || hudHuntH.load() != h) {
            std::lock_guard lock(hudLock);
            hudCandidates.clear();
            hudLookedAt.clear();
            hudHuntW = w;
            hudHuntH = h;
            hudHunting = true;
        }
        hudWhy = hudHunt;
        auto* found = hudScan(root, colour);
        if (!found)
            return;
        uint32_t state{};
        {
            std::lock_guard lock(hudLock);
            state = hudCandidates[found];
            hudCandidates.clear();
            hudLookedAt.clear();
        }
        hudResource = found; // AddRef: the panel holds it now, so it is alive
        hudState = state;
        hudTracked = found;
        hudHunting = false;
        ++hudFinds;
        hudWhy = hudFound;
    }
    const auto desc = hudResource->GetDesc();
    bool linear{};
    const auto view = screenView(desc.Format, linear);
    if (view == DXGI_FORMAT_UNKNOWN) {
        hudWhy = hudFormat;
        return;
    }
    const auto state = static_cast<D3D12_RESOURCE_STATES>(hudState.load());
    // Only from a state the game leaves it in to be read (its own barrier back after the HUD's draw).
    if (!(state & (D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)) ||
        (state & (D3D12_RESOURCE_STATE_RENDER_TARGET | D3D12_RESOURCE_STATE_UNORDERED_ACCESS))) {
        ++hudSkipState;
        hudWhy = hudNotReadable;
        return;
    }
    std::lock_guard submitLock(submission);
    std::lock_guard recordLock(recording);
    if (!enabled || fault || GetTickCount64() >= deadline)
        return;
    const auto now = GetTickCount64();
    std::erase_if(retiredHud, [&](const Retired& old) { return now >= old.releaseMs; });
    if (!hudShot.texture || hudShot.width != desc.Width || hudShot.height != desc.Height || hudShot.view != view) {
        if (hudShot.texture)
            retiredHud.push_back({hudShot.texture, now + 3000});
        hudShot = {};
        auto copy = desc;
        copy.Format = typelessFamily(view);
        copy.Flags = D3D12_RESOURCE_FLAG_NONE;
        copy.Alignment = 0;
        D3D12_HEAP_PROPERTIES heap{};
        heap.Type = D3D12_HEAP_TYPE_DEFAULT;
        if (FAILED(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &copy,
                                                   D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, nullptr,
                                                   IID_PPV_ARGS(&hudShot.texture))))
            return;
        hudShot.view = view;
        hudShot.linear = linear;
        hudShot.width = static_cast<uint32_t>(desc.Width);
        hudShot.height = desc.Height;
    }
    auto* slot = readyCommands(hudCommands);
    if (!slot) {
        ++hudSkipBusy;
        hudWhy = hudBusy;
        return;
    }
    D3D12_RESOURCE_BARRIER b[2]{};
    b[0].Type = b[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b[0].Transition = {hudResource.Get(), D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, state,
                       D3D12_RESOURCE_STATE_COPY_SOURCE};
    b[1].Transition = {hudShot.texture.Get(), D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                       D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST};
    originalBarrier(slot->list.Get(), 2, b);
    slot->list->CopyResource(hudShot.texture.Get(), hudResource.Get());
    for (auto& barrier : b)
        std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);
    originalBarrier(slot->list.Get(), 2, b);
    if (!submitCommands(*slot))
        return;
    ++hudShot.frame;
    ++hudCopies;
    hudMs = now;
    hudWhy = hudCopied;
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
// HERO VR 0.0.77 CUT7 BOOT SCREEN (diagnostic): the game's presents and the screen copies made of them, for the
// boot intros' picture against their sound (the owner: the picture about a minute behind on the VR screen).
std::atomic<uint64_t> bootPresents{}, bootCopies{}, bootSkips{};
void captureScreen(IDXGISwapChain* swapchain) {
    ++screenPresents;
    screenSkipped = copyScreen(swapchain);
    ++(screenSkipped == screenCopied ? bootCopies : bootSkips);
    publishScreen(0, nullptr, 0);
}
void bootPresent() {
    ++bootPresents;
}
// Present reaches Present1 in some DXGI builds: one of them handles a frame.
thread_local bool presenting{};
// HERO VR 0.0.78 cut18 UI DUMP (diagnostic): after the hunt, one list on the game's queue copies the back buffer
// (PRESENT) and the best-scored targets in a readable state (their last recorded state, as captureHud) into
// readback buffers; the present thread waits for it once (at most 2 s, outside the locks), turns each into RGBA8 and
// hands them to game_xr (takeUiDump), which writes the PNGs off this thread.
std::array<CopyCommands, 3> uiCommands;
uint32_t uiReason{}, uiPresents{};
std::mutex uiShotLock;
std::vector<UiShot> uiShots;
uint32_t uiShotReason{};
std::string uiShotLine;
std::atomic<bool> uiShotReady{};
const char* uiFormatName(uint32_t f) {
    switch (ui_dump::concrete(f)) {
    case ui_dump::fR16G16B16A16Float:
        return "RGBA16F";
    case ui_dump::fR10G10B10A2Unorm:
        return "RGB10A2";
    case ui_dump::fR11G11B10Float:
        return "R11G11B10F";
    case ui_dump::fR8G8B8A8Unorm:
    case ui_dump::fR8G8B8A8Srgb:
        return "RGBA8";
    case ui_dump::fB8G8R8A8Unorm:
    case ui_dump::fB8G8R8A8Srgb:
        return "BGRA8";
    default:
        return "?";
    }
}
void uiDump(IDXGISwapChain* swapchain, std::vector<UiTarget>& all, std::vector<UiShot>& shots, std::string& line) {
    struct Job {
        ID3D12Resource* resource{};
        uint32_t state{}, format{}, width{}, height{}, rank{};
        uint64_t toRead{};
        bool back{};
        ComPtr<ID3D12Resource> readback;
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout{};
    };
    std::vector<Job> jobs;
    ComPtr<IDXGISwapChain3> chain;
    ComPtr<ID3D12Resource> buffer;
    if (SUCCEEDED(swapchain->QueryInterface(IID_PPV_ARGS(&chain))) &&
        SUCCEEDED(chain->GetBuffer(chain->GetCurrentBackBufferIndex(), IID_PPV_ARGS(&buffer)))) {
        const auto d = buffer->GetDesc();
        ComPtr<ID3D12Device> owner;
        ComPtr<IUnknown> ownerIdentity, gameIdentity;
        const bool ours = SUCCEEDED(buffer->GetDevice(IID_PPV_ARGS(&owner))) &&
                          SUCCEEDED(owner.As(&ownerIdentity)) && SUCCEEDED(device.As(&gameIdentity)) &&
                          ownerIdentity.Get() == gameIdentity.Get();
        if (ours && ui_dump::bytesPerPixel(d.Format) && d.SampleDesc.Count == 1 && d.Width <= 16384)
            jobs.push_back({buffer.Get(), D3D12_RESOURCE_STATE_PRESENT, static_cast<uint32_t>(d.Format),
                            static_cast<uint32_t>(d.Width), d.Height, 0, 0, true});
        else
            line += "; back buffer not read (format " + std::to_string(static_cast<uint32_t>(d.Format)) + ")";
    }
    uint32_t rank = 0;
    for (const auto& t : all) {
        ++rank;
        if (rank <= 12) {
            char c[160];
            snprintf(c, sizeof(c), "; #%u %ux%u %s state 0x%x, %llu transitions, %llu RT->read", rank, t.width,
                     t.height, uiFormatName(t.format), t.state, static_cast<unsigned long long>(t.transitions),
                     static_cast<unsigned long long>(t.toRead));
            line += c;
        }
        if (jobs.size() < ui_dump::maxTargets + 1 && ui_dump::readableState(t.state))
            jobs.push_back({t.resource.Get(), t.state, t.format, t.width, t.height, rank, t.toRead, false});
    }
    if (jobs.empty()) {
        line += "; nothing to read";
        return;
    }
    uint64_t mine{};
    {
        std::lock_guard submitLock(submission);
        std::lock_guard recordLock(recording);
        if (!enabled || fault) {
            line += "; stopped (Spidy's GPU path off)";
            return;
        }
        for (auto& j : jobs) {
            auto desc = j.resource->GetDesc();
            UINT64 bytes{};
            device->GetCopyableFootprints(&desc, 0, 1, 0, &j.layout, nullptr, nullptr, &bytes);
            D3D12_HEAP_PROPERTIES heap{};
            heap.Type = D3D12_HEAP_TYPE_READBACK;
            D3D12_RESOURCE_DESC rb{};
            rb.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            rb.Width = bytes;
            rb.Height = 1;
            rb.DepthOrArraySize = 1;
            rb.MipLevels = 1;
            rb.SampleDesc.Count = 1;
            rb.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            if (FAILED(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &rb,
                                                       D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
                                                       IID_PPV_ARGS(&j.readback))))
                j.readback.Reset();
        }
        std::erase_if(jobs, [](const Job& j) { return !j.readback; });
        auto* slot = jobs.empty() ? nullptr : readyCommands(uiCommands);
        if (!slot) {
            line += "; no readback or command list";
            return;
        }
        std::vector<D3D12_RESOURCE_BARRIER> b(jobs.size());
        for (size_t i = 0; i < jobs.size(); ++i) {
            b[i].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            b[i].Transition = {jobs[i].resource, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                               static_cast<D3D12_RESOURCE_STATES>(jobs[i].state), D3D12_RESOURCE_STATE_COPY_SOURCE};
        }
        originalBarrier(slot->list.Get(), static_cast<UINT>(b.size()), b.data());
        for (auto& j : jobs) {
            D3D12_TEXTURE_COPY_LOCATION src{}, dst{};
            src.pResource = j.resource;
            src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            dst.pResource = j.readback.Get();
            dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
            dst.PlacedFootprint = j.layout;
            slot->list->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
        }
        for (auto& x : b)
            std::swap(x.Transition.StateBefore, x.Transition.StateAfter);
        originalBarrier(slot->list.Get(), static_cast<UINT>(b.size()), b.data());
        if (!submitCommands(*slot)) {
            line += "; submit failed";
            return;
        }
        mine = slot->fence;
    }
    const auto waitFrom = GetTickCount64();
    if (fence->GetCompletedValue() < mine) {
        HANDLE done = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        const bool ok = done && SUCCEEDED(fence->SetEventOnCompletion(mine, done)) &&
                        WaitForSingleObject(done, 2000) == WAIT_OBJECT_0;
        if (done)
            CloseHandle(done);
        if (!ok) {
            line += "; the copy did not finish in 2 s";
            return;
        }
    }
    char waited[64];
    snprintf(waited, sizeof(waited), "; GPU wait %llu ms",
             static_cast<unsigned long long>(GetTickCount64() - waitFrom));
    line += waited;
    for (auto& j : jobs) {
        void* data{};
        if (FAILED(j.readback->Map(0, nullptr, &data)))
            continue;
        UiShot shot;
        shot.width = j.width;
        shot.height = j.height;
        shot.format = j.format;
        shot.state = j.state;
        shot.rank = j.rank;
        shot.toRead = j.toRead;
        shot.backBuffer = j.back;
        shot.rgba.resize(static_cast<size_t>(j.width) * j.height * 4);
        bool read = true;
        for (uint32_t y = 0; y < j.height && read; ++y)
            read = ui_dump::rowToRgba8(j.format,
                                       static_cast<const uint8_t*>(data) + j.layout.Offset +
                                           static_cast<size_t>(y) * j.layout.Footprint.RowPitch,
                                       j.width, shot.rgba.data() + static_cast<size_t>(y) * j.width * 4, j.back);
        D3D12_RANGE written{0, 0};
        j.readback->Unmap(0, &written);
        if (!read)
            continue;
        const auto c = ui_dump::cover(shot.rgba.data(), j.width, j.height);
        char d[220];
        if (j.back)
            snprintf(d, sizeof(d), "; FRAME %ux%u %s: white %.2f%%", j.width, j.height, uiFormatName(j.format),
                     100.0 * static_cast<double>(c.white) / static_cast<double>(c.pixels ? c.pixels : 1));
        else
            snprintf(d, sizeof(d), "; TARGET #%u %ux%u %s: alpha %.2f%% in (%u,%u)-(%u,%u), white %.2f%%", j.rank,
                     j.width, j.height, uiFormatName(j.format),
                     100.0 * static_cast<double>(c.alpha) / static_cast<double>(c.pixels ? c.pixels : 1), c.x0, c.y0,
                     c.x1, c.y1, 100.0 * static_cast<double>(c.white) / static_cast<double>(c.pixels ? c.pixels : 1));
        line += d;
        shots.push_back(std::move(shot));
    }
}
void uiDumpStep(IDXGISwapChain* swapchain) {
    if (!uiReason) {
        const uint32_t asked = uiAsked.exchange(0);
        if (!asked)
            return;
        ComPtr<IDXGISwapChain3> chain;
        ComPtr<ID3D12Resource> buffer;
        if (FAILED(swapchain->QueryInterface(IID_PPV_ARGS(&chain))) ||
            FAILED(chain->GetBuffer(chain->GetCurrentBackBufferIndex(), IID_PPV_ARGS(&buffer))))
            return;
        const auto d = buffer->GetDesc();
        {
            std::lock_guard lock(uiLock);
            uiTargets.clear();
            uiLookedAt.clear();
        }
        uiBackW = static_cast<uint32_t>(d.Width);
        uiBackH = d.Height;
        uiLooked = 0;
        uiReason = asked;
        uiPresents = 0;
        uiHunting = true;
        return;
    }
    if (++uiPresents < ui_dump::huntPresents)
        return;
    uiHunting = false;
    const uint32_t reason = uiReason;
    uiReason = 0;
    std::vector<UiTarget> all;
    {
        std::lock_guard lock(uiLock);
        all.reserve(uiTargets.size());
        for (auto& kv : uiTargets)
            all.push_back(std::move(kv.second));
        uiTargets.clear();
        uiLookedAt.clear();
    }
    const uint32_t backW = uiBackW.load(), backH = uiBackH.load();
    std::sort(all.begin(), all.end(), [&](const UiTarget& a, const UiTarget& b) {
        return ui_dump::score(a.format, a.width, a.height, a.toRead, backW, backH) >
               ui_dump::score(b.format, b.width, b.height, b.toRead, backW, backH);
    });
    char head[320];
    snprintf(head, sizeof(head),
             "HERO VR 0.0.78 cut18 UI DUMP (%s): %u presents watched, %llu render targets looked at, %zu "
             "colour targets of the frame's shape %ux%u",
             ui_dump::reasonName(reason), ui_dump::huntPresents, static_cast<unsigned long long>(uiLooked.load()),
             all.size(), backW, backH);
    std::string line = head;
    std::vector<UiShot> shots;
    uiDump(swapchain, all, shots, line);
    all.clear(); // the references go
    std::lock_guard lock(uiShotLock);
    uiShots = std::move(shots);
    uiShotReason = reason;
    uiShotLine = std::move(line);
    uiShotReady = true;
}
// HERO VR 0.0.78 cut21 PROMPT LAYER (CANDIDATE): while game_xr wants it (VR play), the game's UI layer is found with
// the UI DUMP's hunt (45 presents, watching barriers only), then its best candidates (prompt_layer::candidate, at most
// maxProbes) checked one at a time by the same no-wait alpha read-back as the cards' - the first that is
// prompt_layer::isLayer kept. Never the UI DUMP's read-back (it waits up to 2 s), never a wait on the GPU. Then
// copied each present from its readable state (as the HUD panel), and every
// maskEveryMs read back (no wait: mapped on a later present) into a cell grid of where its alpha is. game_xr cuts
// that grid's small pieces (prompts, the waypoint, markers) into small cards; never the whole layer.
enum LayerWhy : uint32_t {
    layerIdle,
    layerWaitDump,
    layerHunting,
    layerBackingOff,
    layerGaveUp,
    layerChecking,
    layerCopied,
    layerNotReadable,
    layerBusy,
    layerFormat,
    layerStopped
};
const char* layerWhyName(uint32_t w) {
    static const char* names[] = {"idle (not wanted)", "waiting for a UI DUMP to finish", "finding the layer",
                                  "not found yet (trying again)", "not found (gave up this session)",
                                  "checking a candidate's alpha (no wait)", "copied",
                                  "not in a readable state", "copies in flight", "format not readable",
                                  "stopped (Spidy's GPU path off)"};
    return w < 11 ? names[w] : "?";
}
struct LayerReadback {
    ComPtr<ID3D12Resource> buffer;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout{};
    uint64_t fence{};
    uint32_t width{}, height{};
    bool pending{};
};
bool layerLocating{};
std::atomic<bool> layerWantedFlag{};
ComPtr<ID3D12Resource> layerResource;
uint32_t layerBackW{}, layerBackH{}, layerFailed{}, layerTries{};
uint64_t layerNextTry{}, layerMs{}, layerMaskAskedMs{};
Screen layerShot;
std::array<CopyCommands, 3> layerCommands;
std::array<LayerReadback, 2> layerReadbacks;
std::vector<Retired> retiredLayer;
std::mutex layerGridLock; // the grid only
prompt_layer::Grid layerGrid;
uint64_t layerGridMs{};
std::atomic<uint32_t> layerWhy{};
std::atomic<bool> layerVerified{};  // layerResource is the layer (its alpha read back and isLayer), not a candidate
std::vector<UiTarget> layerCandidates; // a find's, best first; the present thread's
size_t layerCandidate{};
uint64_t layerProbeMs{}, layerProbes{};
std::atomic<uint64_t> layerFinds{}, layerLost{}, layerCopies{}, layerMasks{}, layerSkipState{}, layerSkipBusy{};
std::mutex layerLineLock;
std::string layerFoundLine = "not found yet";
void dropLayer(uint64_t now) {
    if (!layerResource && layerShot.texture == nullptr)
        return;
    layerTracked = nullptr;
    std::lock_guard submitLock(submission);
    if (layerResource)
        retiredLayer.push_back({layerResource, now + 3000}); // after the copies in flight
    if (layerShot.texture)
        retiredLayer.push_back({layerShot.texture, now + 3000});
    for (auto& r : layerReadbacks) {
        if (r.buffer)
            retiredLayer.push_back({r.buffer, now + 3000});
        r = {};
    }
    layerResource.Reset();
    layerShot = {};
    layerVerified = false;
    std::lock_guard gridLock(layerGridLock);
    layerGrid = {};
    layerGridMs = 0;
}
void stopLocating() {
    layerCandidates.clear(); // the references go
    layerCandidate = 0;
    if (!layerLocating)
        return;
    layerLocating = false;
    uiHunting = false;
    std::lock_guard lock(uiLock);
    uiTargets.clear(); // the references go
    uiLookedAt.clear();
}
// The find: the UI DUMP's hunt (barriers watched, nothing read), then its candidates checked by checkCandidate.
void findFailed(uint64_t now, const char* why) {
    layerCandidates.clear();
    layerCandidate = 0;
    layerNextTry = now + prompt_layer::locateDelay(layerFailed++);
    layerWhy = layerTries >= prompt_layer::maxLocates ? layerGaveUp : layerBackingOff;
    char head[200];
    snprintf(head, sizeof(head), "find %u at %ux%u: %s", layerTries, layerBackW, layerBackH, why);
    std::lock_guard lock(layerLineLock);
    layerFoundLine = head;
}
// The next candidate held (not yet the layer): its alpha read back, no wait, by captureLayer.
void adoptCandidate(uint64_t now) {
    if (layerCandidate >= layerCandidates.size()) {
        findFailed(now, "none of the candidates is the layer");
        return;
    }
    const auto& c = layerCandidates[layerCandidate];
    layerResource = c.resource;
    layerState = c.state;
    layerSeenMs = now;
    layerTracked = c.resource.Get();
    layerVerified = false;
    layerMaskAskedMs = 0; // read at once
    layerProbeMs = now;
    ++layerProbes;
    layerWhy = layerChecking;
}
void locateLayer(uint64_t now) {
    if (!layerLocating) {
        if (uiReason || uiAsked.load()) {
            layerWhy = layerWaitDump;
            return;
        }
        if (layerTries >= prompt_layer::maxLocates) {
            layerWhy = layerGaveUp;
            return;
        }
        if (now < layerNextTry) {
            layerWhy = layerBackingOff;
            return;
        }
        {
            std::lock_guard lock(uiLock);
            uiTargets.clear();
            uiLookedAt.clear();
        }
        uiBackW = layerBackW;
        uiBackH = layerBackH;
        uiLooked = 0;
        uiPresents = 0;
        uiHunting = true;
        layerLocating = true;
        ++layerTries;
        layerWhy = layerHunting;
        return;
    }
    if (++uiPresents < ui_dump::huntPresents)
        return;
    uiHunting = false;
    layerLocating = false;
    std::vector<UiTarget> all;
    {
        std::lock_guard lock(uiLock);
        all.reserve(uiTargets.size());
        for (auto& kv : uiTargets)
            all.push_back(std::move(kv.second));
        uiTargets.clear();
        uiLookedAt.clear();
    }
    const uint32_t backW = layerBackW, backH = layerBackH;
    std::sort(all.begin(), all.end(), [&](const UiTarget& a, const UiTarget& b) {
        return ui_dump::score(a.format, a.width, a.height, a.toRead, backW, backH) >
               ui_dump::score(b.format, b.width, b.height, b.toRead, backW, backH);
    });
    layerCandidates.clear();
    layerCandidate = 0;
    for (auto& t : all)
        if (layerCandidates.size() < prompt_layer::maxProbes &&
            prompt_layer::candidate(t.format, t.width, t.height, backW, backH, t.toRead))
            layerCandidates.push_back(std::move(t));
    all.clear(); // the other references go
    if (layerCandidates.empty()) {
        findFailed(now, "no target of the frame's size with alpha was drawn and read");
        return;
    }
    adoptCandidate(now);
}
// A candidate's first alpha read-back in: the layer (kept, copied from now on) or the next candidate. No wait: called
// only when a read-back the GPU already finished was mapped, or on its time running out.
void checkCandidate(uint64_t now, const prompt_layer::Grid* grid) {
    if (layerVerified || layerCandidate >= layerCandidates.size())
        return;
    const auto& c = layerCandidates[layerCandidate];
    if (grid && prompt_layer::isLayer(c.format, c.width, c.height, layerBackW, layerBackH, grid->alphaSamples,
                                      grid->samples)) {
        layerVerified = true;
        layerFailed = 0;
        ++layerFinds;
        char head[220];
        snprintf(head, sizeof(head),
                 "find %u: %ux%u %s (format %u), %.2f%% alpha, %llu RT->read in %u presents, candidate %zu of %zu",
                 layerTries, c.width, c.height, uiFormatName(c.format), c.format,
                 100.0 * static_cast<double>(grid->alphaSamples) / static_cast<double>(grid->samples ? grid->samples : 1),
                 static_cast<unsigned long long>(c.toRead), ui_dump::huntPresents, layerCandidate + 1,
                 layerCandidates.size());
        layerCandidates.clear(); // the others' references go
        layerCandidate = 0;
        std::lock_guard lock(layerLineLock);
        layerFoundLine = head;
        return;
    }
    if (!grid && now < layerProbeMs + prompt_layer::probeMs)
        return;
    dropLayer(now); // not it (or never readable): the next
    ++layerCandidate;
    adoptCandidate(now);
}
// Maps the read-backs the GPU has finished (never waits: only those its fence has passed): the newest one's alpha
// into the grid, and into `got` (true when one came in).
prompt_layer::Grid layerReadGrid; // the present thread's, off the stack
bool readLayerMasks(uint64_t now) {
    bool got = false;
    const auto completed = fence->GetCompletedValue();
    if (completed == UINT64_MAX)
        return false;
    for (auto& r : layerReadbacks) {
        if (!r.pending || r.fence > completed)
            continue;
        r.pending = false;
        void* data{};
        if (FAILED(r.buffer->Map(0, nullptr, &data)))
            continue;
        auto& grid = layerReadGrid;
        if (grid.begin(r.width, r.height))
            for (uint32_t y = 0; y < r.height; y += 2)
                grid.row(static_cast<const uint8_t*>(data) + r.layout.Offset +
                             static_cast<size_t>(y) * r.layout.Footprint.RowPitch,
                         y);
        D3D12_RANGE written{0, 0};
        r.buffer->Unmap(0, &written);
        got = true;
        ++layerMasks;
        if (!layerVerified)
            continue; // a candidate's: checked by captureLayer, not drawn
        std::lock_guard gridLock(layerGridLock);
        layerGrid = grid;
        layerGridMs = now;
    }
    return got;
}
// Each present: the layer copied (for the cards) and every maskEveryMs its alpha read back, both on the game's queue
// with no wait for the GPU. A candidate (not yet checked) gets the alpha read-back only, until checkCandidate decides.
void captureLayer(uint64_t now) {
    const bool got = readLayerMasks(now);
    if (!layerVerified) {
        checkCandidate(now, got ? &layerReadGrid : nullptr);
        if (!layerResource || layerVerified)
            return; // the next candidate (or the layer) from the next present
    }
    const bool verified = layerVerified;
    const auto desc = layerResource->GetDesc();
    bool linear{};
    const auto view = screenView(desc.Format, linear);
    if (view == DXGI_FORMAT_UNKNOWN || !prompt_layer::eightBitAlpha(static_cast<uint32_t>(desc.Format))) {
        layerWhy = layerFormat;
        return;
    }
    const auto state = static_cast<D3D12_RESOURCE_STATES>(layerState.load());
    if (!(state & (D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)) ||
        (state & (D3D12_RESOURCE_STATE_RENDER_TARGET | D3D12_RESOURCE_STATE_UNORDERED_ACCESS))) {
        ++layerSkipState;
        layerWhy = layerNotReadable;
        return;
    }
    std::lock_guard submitLock(submission);
    std::lock_guard recordLock(recording);
    if (!enabled || fault || now >= deadline) {
        layerWhy = layerStopped;
        return;
    }
    std::erase_if(retiredLayer, [&](const Retired& old) { return now >= old.releaseMs; });
    if (verified && (!layerShot.texture || layerShot.width != desc.Width || layerShot.height != desc.Height ||
        layerShot.view != view)) {
        if (layerShot.texture)
            retiredLayer.push_back({layerShot.texture, now + 3000});
        layerShot = {};
        auto copy = desc;
        copy.Format = typelessFamily(view);
        copy.Flags = D3D12_RESOURCE_FLAG_NONE;
        copy.Alignment = 0;
        D3D12_HEAP_PROPERTIES heap{};
        heap.Type = D3D12_HEAP_TYPE_DEFAULT;
        if (FAILED(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &copy,
                                                   D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, nullptr,
                                                   IID_PPV_ARGS(&layerShot.texture))))
            return;
        layerShot.view = view;
        layerShot.linear = linear;
        layerShot.width = static_cast<uint32_t>(desc.Width);
        layerShot.height = desc.Height;
    }
    // A mask read-back every maskEveryMs, into a free buffer (none waited for).
    LayerReadback* mask{};
    if (now - layerMaskAskedMs >= prompt_layer::maskEveryMs)
        for (auto& r : layerReadbacks)
            if (!r.pending) {
                mask = &r;
                break;
            }
    if (mask && (!mask->buffer || mask->width != desc.Width || mask->height != desc.Height)) {
        if (mask->buffer)
            retiredLayer.push_back({mask->buffer, now + 3000});
        *mask = {};
        UINT64 bytes{};
        device->GetCopyableFootprints(&desc, 0, 1, 0, &mask->layout, nullptr, nullptr, &bytes);
        D3D12_HEAP_PROPERTIES heap{};
        heap.Type = D3D12_HEAP_TYPE_READBACK;
        D3D12_RESOURCE_DESC rb{};
        rb.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        rb.Width = bytes;
        rb.Height = rb.DepthOrArraySize = rb.MipLevels = 1;
        rb.SampleDesc.Count = 1;
        rb.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        if (!bytes || FAILED(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &rb,
                                                             D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
                                                             IID_PPV_ARGS(&mask->buffer)))) {
            *mask = {};
            mask = nullptr;
        } else {
            mask->width = static_cast<uint32_t>(desc.Width);
            mask->height = desc.Height;
        }
    }
    if (!verified && !mask)
        return; // a candidate's read-back in flight: nothing to copy
    auto* slot = readyCommands(layerCommands); // never waits: a free slot or none
    if (!slot) {
        ++layerSkipBusy;
        layerWhy = layerBusy;
        return;
    }
    const UINT barriers = verified ? 2 : 1;
    D3D12_RESOURCE_BARRIER b[2]{};
    b[0].Type = b[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b[0].Transition = {layerResource.Get(), D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, state,
                       D3D12_RESOURCE_STATE_COPY_SOURCE};
    if (verified)
        b[1].Transition = {layerShot.texture.Get(), D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                           D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST};
    originalBarrier(slot->list.Get(), barriers, b);
    if (verified)
        slot->list->CopyResource(layerShot.texture.Get(), layerResource.Get());
    if (mask) {
        D3D12_TEXTURE_COPY_LOCATION src{}, dst{};
        src.pResource = layerResource.Get();
        src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dst.pResource = mask->buffer.Get();
        dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        dst.PlacedFootprint = mask->layout;
        slot->list->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
    }
    for (UINT i = 0; i < barriers; ++i)
        std::swap(b[i].Transition.StateBefore, b[i].Transition.StateAfter);
    originalBarrier(slot->list.Get(), barriers, b);
    if (!submitCommands(*slot))
        return;
    if (mask) {
        mask->fence = slot->fence;
        mask->pending = true;
        layerMaskAskedMs = now;
    }
    if (!verified)
        return;
    ++layerShot.frame;
    ++layerCopies;
    layerMs = now;
    layerWhy = layerCopied;
}
void promptLayerStep(IDXGISwapChain* swapchain) {
    const auto now = GetTickCount64();
    if (!layerWantedFlag.load()) {
        stopLocating();
        if (layerResource)
            dropLayer(now);
        layerWhy = layerIdle;
        return;
    }
    DXGI_SWAP_CHAIN_DESC sd{};
    if (FAILED(swapchain->GetDesc(&sd)) || !sd.BufferDesc.Width || !sd.BufferDesc.Height)
        return;
    if (sd.BufferDesc.Width != layerBackW || sd.BufferDesc.Height != layerBackH) {
        stopLocating();
        if (layerResource) {
            dropLayer(now);
            ++layerLost;
        }
        layerBackW = sd.BufferDesc.Width;
        layerBackH = sd.BufferDesc.Height;
        layerTries = layerFailed = 0; // a new size: found again
        layerNextTry = 0;
    }
    if (layerResource && layerVerified && now > layerSeenMs.load() + prompt_layer::quietLostMs) {
        dropLayer(now); // the game let it go or draws elsewhere now: found again
        ++layerLost;
    }
    if (!layerResource) {
        locateLayer(now);
        return;
    }
    captureLayer(now);
}
void presentFrame(IDXGISwapChain* swapchain, UINT flags) {
    if (!enabled || (flags & DXGI_PRESENT_TEST))
        return;
    const bool insetOnly = screenInsetOnly.load();
    bootPresent(); // CUT7 BOOT SCREEN diagnostic: counts only
    if (!layerLocating && (uiReason || uiAsked.load(std::memory_order_relaxed)))
        uiDumpStep(swapchain); // cut18 UI DUMP diagnostic: a few times a session
    if (layerWantedFlag.load(std::memory_order_relaxed) || layerResource || layerLocating)
        promptLayerStep(swapchain); // cut21 PROMPT LAYER: the game's UI layer, its copy and its alpha's grid
    if (hudWanted.load())
        captureHud(); // CUT7 WRIST MAP SOURCE: the HUD panel's texture, the minimap's real source
    else
        hudHunting = false;
    if (screenWanted)
        captureScreen(swapchain); // before the mirror: the game's own frame, its prompts (0.0.77)
    if ((!screenWanted || insetOnly) && (mirrorWanted || monitorStreaming())) // 0.0.74 test 10 MONITOR
        mirrorEye(swapchain);
}
// The game presents once a frame whether or not it rendered a view of its
// own. While it does not, the window only mirrors the left eye, and a present
// costs GPU time the eyes could have: hold back those beyond the monitor's rate.
// The game goes on without them (it paces itself by its own fences), and a
// held present draws no mirror either.
bool holdPresent(UINT flags) {
    const auto rate = mirrorRate.load();
    if (!rate || !enabled || !mirrorWanted || screenWantedFlag() || (flags & DXGI_PRESENT_TEST))
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
void screenForInset(bool only) {
    screenInsetOnly = only;
}
bool screenRequested() {
    return screenWantedFlag();
}
void sceneDepthInUse(bool used) {
    depthInUse = used;
}
void wantHud(bool wanted) {
    hudWanted = wanted;
}
void requestUiDump(uint32_t reason) {
    uint32_t none = 0;
    uiAsked.compare_exchange_strong(none, reason);
}
bool takeUiDump(uint32_t& reason, std::vector<UiShot>& shots, std::string& line) {
    if (!uiShotReady.exchange(false))
        return false;
    std::lock_guard lock(uiShotLock);
    reason = uiShotReason;
    shots = std::move(uiShots);
    uiShots.clear();
    line = std::move(uiShotLine);
    return true;
}
void wantPromptLayer(bool wanted) {
    layerWantedFlag = wanted;
}
bool latestPromptLayer(Screen& out, prompt_layer::Grid& grid, uint64_t maxAgeMs) {
    const auto now = GetTickCount64();
    {
        std::lock_guard submitLock(submission);
        if (!layerVerified || !layerShot.texture || !layerShot.frame || now < layerMs || now - layerMs > maxAgeMs)
            return false;
        out = layerShot;
    }
    std::lock_guard gridLock(layerGridLock);
    if (!layerGridMs || now < layerGridMs || now - layerGridMs > prompt_layer::maskMaxAgeMs ||
        layerGrid.width != out.width || layerGrid.height != out.height)
        return false;
    grid = layerGrid;
    return true;
}
std::string promptLayerLine() {
    const auto n = [](uint64_t v) { return std::to_string(v); };
    std::string found;
    {
        std::lock_guard lock(layerLineLock);
        found = layerFoundLine;
    }
    char state[16];
    sprintf_s(state, "%x", layerState.load());
    return std::string("HERO VR 0.0.78 cut21 PROMPT LAYER (CANDIDATE): the game's UI layer ") +
           layerWhyName(layerWhy.load()) + "; " + found + "; found " + n(layerFinds) + ", lost " + n(layerLost) +
           ", candidates checked " + n(layerProbes) + ", copies " + n(layerCopies) + ", masks " + n(layerMasks) +
           ", skipped (state 0x" + state + ") " +
           n(layerSkipState) + ", busy " + n(layerSkipBusy) + ", transitions " + n(layerTransitions);
}
bool latestHud(Screen& out, uint64_t maxAgeMs) {
    std::lock_guard submitLock(submission);
    const auto now = GetTickCount64();
    if (!hudShot.texture || !hudShot.frame || now < hudMs || now - hudMs > maxAgeMs)
        return false;
    out = hudShot;
    return true;
}
std::string hudSourceLine() {
    const auto n = [](uint64_t v) { return std::to_string(v); };
    return std::string("HERO VR 0.0.77 CUT7 WRIST MAP SOURCE (CANDIDATE): the HUD panel's texture ") +
           hudWhyName(hudWhy.load()) + "; looked at " + n(hudLooked) + " render targets, " + n(hudSized) +
           " of the panel's size " + n(hudHuntW) + "x" + n(hudHuntH) + ", found " + n(hudFinds) + ", lost " +
           n(hudLost) + ", copies " + n(hudCopies) + ", skipped (state 0x" + [] {
               char b[16];
               sprintf_s(b, "%x", hudState.load());
               return std::string(b);
           }() + ") " + n(hudSkipState) + ", busy " + n(hudSkipBusy);
}
void bootCounts(uint64_t& presentsOut, uint64_t& copies, uint64_t& skips, uint32_t& skipWhy, uint64_t& copyAgeMs) {
    presentsOut = bootPresents.load();
    copies = bootCopies.load();
    skips = bootSkips.load();
    skipWhy = static_cast<uint32_t>(screenSkipped);
    std::lock_guard submitLock(submission);
    const auto now = GetTickCount64();
    copyAgeMs = screen.frame && now >= screenMs ? now - screenMs : UINT64_MAX;
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
void openJob(unsigned eye, uint64_t serial, uint64_t generation) {
    if (eye > 1)
        return;
    std::lock_guard tagLock(depthTagLock);
    depthOpenStamp[eye] = {serial, generation};
    depthOpenMask.fetch_or(1u << eye);
}
void closeJob(unsigned eye) {
    if (eye > 1)
        return;
    std::lock_guard tagLock(depthTagLock);
    depthOpenMask.fetch_and(~(1u << eye));
    const auto stamp = depthOpenStamp[eye];
    depthOpenStamp[eye] = {};
    // the lists this job's window tagged end with it (a marked list ends at its own end marker)
    std::erase_if(depthTags, [&](const auto& kv) {
        const auto& t = kv.second;
        if (!t.window || t.eye != eye || !(t.stamp == stamp))
            return false;
        if (t.gbuffer && !t.depthDone)
            ++depthSkipNoPass;
        return true;
    });
    depthTagCount = static_cast<uint32_t>(depthTags.size());
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
void setVrAa(uint32_t mode) {
    if (mode > vr_aa::legacy)
        mode = vr_aa::ours;
    vrAaMode = mode;
    std::lock_guard lock(copyTimingLock);
    SpidyVrAaData.asked = mode;
}
uint32_t vrAa() {
    return vrAaMode.load();
}
uint32_t taaJitterMode() {
    return taaJitterModeNow.load(std::memory_order_relaxed);
}
bool vrAaLocks() {
    return vrAaLocked();
}
uint32_t vrAaSmaaFault() {
    return smaaFault.load();
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
bool setEyeOptics(const float left[12], const float right[12]) {
    if (sharpenFault)
        return false;
    std::lock_guard lock(opticsLock);
    for (unsigned k = 0; k < 12; ++k) {
        opticsNow[0][k] = left && std::isfinite(left[k]) ? left[k] : 0.f;
        opticsNow[1][k] = right && std::isfinite(right[k]) ? right[k] : 0.f;
    }
    opticsAtMs = GetTickCount64();
    return true;
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
               std::to_string(depthOff.load()) + "; 0.0.75 lists marked " + n(depthListsMarked) + " / by open job " +
               n(depthListsWindow) + ", G-buffer passes on them " + n(depthPassesTagged) + " / unclaimed " +
               n(depthPassesUnclaimed) + ", targets set on them " + n(depthTargetsTagged) + " (with depth " +
               n(depthDsvTagged) + ", most colour targets " + std::to_string(depthMostTargets.load()) +
               "), depths out " + n(depthExitsTagged);
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
    percent = (sm1Lock.load() || vrAaLocked()) ? 0u : sharpenPercent.load();
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
    add(staged && !aaFault.load() && (downsampling() || (aaMode.load() == 1 && !sm1Lock.load() && !vrAaLocked())),
        aaClock);
    add(staged && !historyFault.load() && historyNow(), historyClock);
    add(staged && smaaLast.load(std::memory_order_relaxed), smaaClock); // 0.0.76 SPIDY TAA
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
void noteEyeViews(uint64_t serial, const std::array<eye_history::EyeView, 2>& views, float jitterX, float jitterY,
                  uint32_t width, uint32_t height) {
    std::lock_guard lock(viewLock);
    notedViews[nextNoted] = {serial, views, {jitterX, jitterY}, {width, height}};
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
    mode = width && downsampling() ? 2 : (sm1Lock.load() || vrAaLocked()) ? 0u : aaMode.load();
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
    // 0.0.76 SPIDY TAA / SMAA: no CAS either (vr_aa.hpp).
    const uint32_t percent = (sm1Lock.load() || vrAaLocked()) ? 0u : sharpenPercent.load();
    // Update 24: read once, so both images of this pair follow the same mode.
    const uint32_t debug = eyeDebugMode.load();
    // Update 37.2: the pass also runs for COLOR alone (sharpening 0).
    // HERO VR 0.0.78 PUNISHER ARSENAL: the pass also runs for a scope or red dot in view.
    const bool graded = staged && (colorOn.load() || // not in the screen-copy mode (as sharpening)
                                   (foveaDebug.load() && foveaLevel.load() != foveation::off) || opticsLive());
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
    colorPairs += sharpened && graded && colorOn.load(); // 0.0.78: not the optics alone
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
    if (!left || !(mirrorWanted || streaming) || screenWantedFlag() || !shownWanted || (sharpening && !streaming))
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
    screenInsetOnly = false;
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
// 0.0.76 A-depth (live.txt vr_taa_depth): 0 off (the default), 1 on, 2 auto (on once the eye depth is CONFIRMED),
// SPIDY TAA's history turned with the eyes' depth. Live; refused (4607) above 2.
extern "C" __declspec(dllexport) DWORD WINAPI SpidyVrTaaDepth(void* value) {
    const auto v = reinterpret_cast<uintptr_t>(value);
    if (v > spidy::taa_depth::automatic)
        return 4607;
    native_gpu::taaDepthOn = static_cast<uint32_t>(v);
    OutputDebugStringA(v == 1   ? "SpidyVR: 0.0.76 A-depth on (vr_taa_depth 1)\n"
                       : v == 2 ? "SpidyVR: 0.0.76 A-depth auto (vr_taa_depth 2: on once the eye depth is CONFIRMED)\n"
                                : "SpidyVR: 0.0.76 A-depth off (vr_taa_depth 0)\n");
    return 0;
}
// 0.0.76 SEAM TRACE (live.txt vr_taa_seam, read-only): 0 off (the default), 1 on - SPIDY TAA's seam / edge trace on
// the head-motion path too (the eyes' depth read for its bins only). Live; refused (4615) above 1.
extern "C" __declspec(dllexport) DWORD WINAPI SpidyVrTaaSeam(void* value) {
    const auto v = reinterpret_cast<uintptr_t>(value);
    if (v > 1)
        return 4615;
    native_gpu::taaSeamOn = static_cast<uint32_t>(v);
    OutputDebugStringA(v ? "SpidyVR: 0.0.76 SEAM TRACE on (vr_taa_seam 1)\n" : "SpidyVR: 0.0.76 SEAM TRACE off (vr_taa_seam 0)\n");
    return 0;
}
// Eye anti-aliasing now (live.txt "aa"): 0 off, 1 FXAA. Takes effect with the
// next eye pair; refused (3960) for another value.
// 0.0.76 TAA JITTER (live.txt vr_taa_jitter, a diagnostic; taa_jitter.hpp): 0 on (the default), 1 off, 2 fixed,
// 3 half - our own Halton lens step (game_xr's eye_taa::step) only; SPIDY TAA and each eye's history go on with
// whatever lens results. Live; refused (4611) above 3. Never touches the game's jitter (vr_jitter / vr_game_jitter).
extern "C" __declspec(dllexport) DWORD WINAPI SpidyVrTaaJitter(void* value) {
    const auto v = reinterpret_cast<uintptr_t>(value);
    if (v >= taa_jitter::modes)
        return 4611;
    native_gpu::taaJitterModeNow = static_cast<uint32_t>(v);
    char line[96];
    std::snprintf(line, sizeof line, "SpidyVR: 0.0.76 TAA JITTER %s (vr_taa_jitter)\n",
                  taa_jitter::name(static_cast<uint32_t>(v)));
    OutputDebugStringA(line);
    return 0;
}
// 0.0.76 TAA STILL (live.txt vr_taa_still, taa_still.hpp): SPIDY TAA's still share in thousandths, 600-990 (0.60-0.99),
// or 0 for this build's own (0.9; C-still094 0.94). Live; refused (4612) otherwise. The moving share is unchanged.
extern "C" __declspec(dllexport) DWORD WINAPI SpidyVrTaaStill(void* value) {
    const auto v = reinterpret_cast<uintptr_t>(value);
    if (v > taa_still::highest || !taa_still::valid(static_cast<uint32_t>(v)))
        return 4612;
    native_gpu::taaStillMilli = static_cast<uint32_t>(v);
    char line[96];
    std::snprintf(line, sizeof line, "SpidyVR: 0.0.76 TAA STILL %.2f (vr_taa_still%s)\n",
                  taa_still::share(static_cast<uint32_t>(v)), v ? "" : " unset: the build's own");
    OutputDebugStringA(line);
    return 0;
}
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
