#pragma once
#include "spidy/eye_color.hpp"
#include <array>
#include <cstdint>
#include <d3d12.h>
#include <wrl/client.h>
#include "xr_timing.hpp"
#include "eye_history.hpp"

namespace spidy::native_gpu {
struct Config {
    uint32_t magic = 0x53475043, version = 3, bytes = sizeof(Config), pid{};
    uint64_t base{}, queue{};
    uint32_t durationMs = 10000, copyToXr{}; // 0 capture, 1 direct, 2 staged
    // eyeAA (was reserved, 0): 1 smooths each staged eye pair with FXAA 3.11
    // quality on its way to the headset (eyeAntiAliasing); 0 copies it as is.
    uint32_t captureImages = 1, eyeAA{};
};
struct Data {
    uint32_t magic = 0x53475044, version = 2, bytes = sizeof(Data), status{};
    int64_t sequence{};
    uint32_t error{}, heapRebuilds{}; // 0.0.74 test 3 HEAP GUARD: was reserved (0)
    uint64_t executes{}, markers{}, matchedMarkers{}, pairs{}, serial{}, generation{}, fence{}, completed{};
    uint64_t begins[2]{}, ends[2]{}, resources[2]{}, pixels[2]{};
    uint32_t states[2]{}, known[2]{}, width{}, height{};
    uint64_t captured{}, reused{}, capturedSerial{};
};
static_assert(sizeof(Config) == 48 && sizeof(Data) == 208);
// Called around the engine's timestamp operation. The D3D12 EndQuery detour
// attaches the stamp to its actual command list, without adding game commands.
void enterMarker(unsigned eye, uint64_t serial, uint64_t generation, bool ended);
void leaveMarker();
void registerEye(unsigned eye, ID3D12Resource* resource);
// 0.0.71 MIRROR VIEW (private test): the game's mirror view image (nullptr lets it go), copied as a
// small picture into both headset eyes. fault: 1 pixel kind, 2 barrier kind, 3 size, 4 other queue,
// 5 another device; any fault stops only the picture, for the session.
struct MirrorGpu {
    uint32_t fault{}, image{}, stateKnown{};
    uint64_t copies{};
};
void registerMirror(ID3D12Resource* resource);
MirrorGpu mirrorGpu();
// Update 32.1 (eye_swap.hpp): eye images the game replaced mid-session and
// the bridge took over (a graphics setting rebuilt them), and 3503 once a
// replacement could not be taken: the eyes stay paused, VR goes on.
uint32_t eyeSwaps();
uint32_t eyesLost();
// Targets have been acquired and waited by OpenXR. They enter and leave in
// RENDER_TARGET state. Arm before publishing the corresponding eye command.
bool arm(uint64_t serial, ID3D12Resource* left, ID3D12Resource* right);
// Wait for copy submission on the OpenXR-bound queue. GPU dependencies remain
// ordered on that queue; allocator reuse and final readback check completion.
bool waitCopy(uint64_t serial, uint32_t timeoutMs);
// Nonblocking copy of the newest complete native pair. Output identifies the
// exact poses required for projection-layer submission, including on reuse.
bool copyLatest(ID3D12Resource* left, ID3D12Resource* right, uint64_t& serial, uint64_t& generation);
uint32_t error();
// Eye anti-aliasing (staged mode only): no game addresses, only the eye images
// the bridge already copies. Each new eye pair is drawn through an FXAA 3.11
// quality pass (luma from RGB, display-encoded values) into the staged images
// the headset copies come from, so its cost is paid once a pair, not once a
// headset frame, and the overlay drawn later on the swapchain images stays
// sharp. A pass that cannot be built (no d3dcompiler_47, an eye image that is
// not readable by shaders) leaves the plain copy and records why (aaFault).
// setEyeAA: 0 off, 1 FXAA; false for another value.
bool setEyeAA(uint32_t mode);
// Eye history (eye_history.hpp): each new pair blended with the pair before
// it, turned to the new pair's view, this percent the old one (0 off, at most
// eye_history::maximumPercent; false above it). After AA, at the headset's size.
bool setEyeHistory(uint32_t percent);
// Update 33: EYE AA (eye_taa.hpp): the history pass at least eye_taa::historyPercent
// while on (game_xr moves each eye command's lens by a sub-pixel step). False to turn
// it on once the history pass has failed (eyeHistoryFault, 1-5). eyeHistoryMs: the
// pass's newest GPU time for both eyes, NaN when not measured in the last second.
bool setEyeTemporal(bool on);
// 0.0.73 test 4 IMAGE STYLE SPIDYVR (image_style.hpp): true while the game's own TAA resolves each eye:
// FXAA, the history blend and CAS stand aside (their settings kept). Live; game_xr sets it.
void setCleanImage(bool clean);
bool cleanImageNow();
// 0.0.74 test 3 SPIDYVR STOCK: COLOR grade and foveation stand aside (their settings kept). Live; game_xr sets it.
void setStockImage(bool stock);
// 0.0.74 test 3 fix: HEAP GUARD starved the eyes and fell back to test 2's path (for the session).
bool heapGuardFellBack();
// Update 34.2: EYE AA TRAIL (eye_history::movedWeight): the percent of the history kept
// at swing speed (0-100; default eye_history::defaultTrail). False above 100. Live.
bool setEyeTrail(uint32_t percent);
uint32_t eyeTrail();
bool eyeTemporal();
uint32_t eyeHistoryFault();
float eyeHistoryMs();
// Update 15: the eye AA mode, history percent and monitor rate in force.
uint32_t eyeAA();
uint32_t eyeHistory();
uint32_t monitorRateNow();
// The views the eyes of the pair with this serial were rendered from (the
// eyes command's serial), for the history pass to turn the old pair to the new.
void noteEyeViews(uint64_t serial, const std::array<eye_history::EyeView, 2>& views);
// SpidyEyeHistoryData, for the launcher's report.
struct HistoryData {
    uint32_t magic = 0x53454844; // SEHD
    uint32_t version = 2;        // 2: Update 34.2 (trail and motion)
    uint32_t bytes = 96;
    uint32_t percent{};
    uint32_t fault{};            // the pass could not be built: 1 root, 2 shader, 3 state, 4 views, 5 pictures
    uint32_t trail{};            // Update 34.2: EYE AA TRAIL, percent of the history kept at swing speed
    uint64_t blended{};          // pairs blended with the one before
    uint64_t restarted{};        // pairs shown as they came (a cut, a fast turn, the first)
    uint64_t unviewed{};         // pairs without a noted view
    double gpuMs{};              // GPU time of the timed passes, summed
    uint64_t timed{};
    // Update 34.2: the motion falloff's work.
    uint64_t moving{};           // pairs blended with less history for motion (share over 0.5)
    uint64_t motionCuts{};       // pairs started again for travel past the cut
    double travelSum{};          // metres the eyes moved between pairs, summed (blended or cut)
    float travelMax{};           // the most between two pairs
    float keepMin{1};            // the least history kept by a blended pair (0-1)
};
static_assert(sizeof(HistoryData) == 96);
// Eye supersampling: the eyes are rendered at `percent` of the headset's eye
// size per side (minimumSupersample-maximumSupersample; 100 none) and, staged,
// each pair is filtered down to the headset's size (a Mitchell-Netravali
// filter in linear light) instead of FXAA. setSupersample only before start;
// false for another value. setEyeOutput gives the headset's eye size (its
// swapchain images); 0 x 0, or the eyes' own size, copies them as before.
inline constexpr uint32_t minimumSupersample = 100, maximumSupersample = 200;
bool setSupersample(uint32_t percent);
uint32_t supersample();
void setEyeOutput(uint32_t width, uint32_t height);
// The AA pass's GPU time for both eyes (timestamps on the game's queue), the
// mode now (0 off, 1 FXAA, 2 the supersampled eyes' downsample), why it stopped (0 none; 1 root signature, 2 shader compile,
// 3 pipeline, 4 descriptor heaps, 5 eye image not readable by shaders,
// 6 resources), and how many pairs it smoothed.
void eyeAATiming(TimingMetric& gpu, uint32_t& mode, uint32_t& aaFault, uint64_t& smoothed);
// Update 16: sharpening of the headset's images (CAS, eye_sharpen.hpp), in
// place of copyLatest's copy. setEyeSharpen: 0 off (no pass), 1-200 its
// strength (101-200 Update 21's strong mode); false above 200, or for 1-200
// once the pass has failed. Not in
// the screen-copy mode (it stays 0). The window is then shown from the
// unsharpened image, without Spidy's drawings (no copy is added for it).
bool setEyeSharpen(uint32_t percent);
uint32_t eyeSharpen();
// Update 24: live.txt eye_debug (eye_debug.hpp): 0 off, 1 both headset images
// made from the left eye's picture. False for any other value.
bool setEyeDebug(uint32_t mode);
uint32_t eyeDebug();
// Update 26: the mode the newest pair copyLatest filled was made with, so the
// headset layer can give the right image the view it was really made from.
uint32_t pairEyeDebug();
// 0, or why the pass stopped for the session: 1 root signature, 2 shader
// compile, 3 pipeline, 4 descriptor heaps, 5 staged eyes not readable,
// 6 headset images not drawable. The copy is used instead.
uint32_t eyeSharpenFault();
// Update 37.2: COLOR / ATMOSPHERE (eye_color.hpp), drawn in the sharpening pass for both eyes
// alike (the pass runs for it alone at sharpening 0). False when turning it on after the pass
// failed (then the eyes are copied ungraded, as for sharpening). Live, from any thread.
bool setEyeColor(const eye_color::Config& c);
eye_color::Config eyeColor();
bool eyeColorActive();     // on and not neutral (the pass grades)
uint64_t eyeColorPairs();  // pairs drawn graded
// Update 38.5: 0, or why 90s ANIMATED's shader is off for the session (1 compile, 2 pipeline);
// the pass and the other looks carry on.
uint32_t eyeToonFault();
// Update 37.2: FOVEATED (foveation.hpp), D3D12 VRS tier 2 on the eye views' G-buffer passes.
struct FoveaStatus {
    uint32_t level{}, inner{}, fault{}, tier{}, tile{};
    bool debug{}, on{}, has4x4{};
    float shadedShare = 1;
    uint64_t taggedPasses{}, appliedPasses{}, untaggedPasses{}, resets{}, images{}, mismatches{};
    std::array<float, 4> centres{};
};
bool setFoveation(uint32_t level, uint32_t inner, bool debug);
FoveaStatus foveaStatus();
void eyeSharpenTiming(TimingMetric& gpu, uint32_t& percent, uint32_t& fault, uint64_t& sharpened);
// Update 16: the newest GPU time, in ms, of the eye passes now on (AA or the
// supersample filter, history, sharpening) summed; 0 when none is on; NaN
// when one is on but not measured in the last second. Lock-free.
float eyePassesGpuMs();
// When the headset's copies (copyLatest) finished on the GPU, measured on the
// CPU the first time the fence is seen passed (every execute the hooks see,
// and every copy): `gpu` from the copy's submission, `pair` from the game's
// submission of the eye pair it copied (stagePair). Both on the game's queue,
// which the OpenXR session is bound to, so a copy finishes only after the game
// work queued before it. Diagnostic, for the session report.
void copyTiming(TimingMetric& gpu, TimingMetric& pair);
// Capture mode only: copies the newest captured pair into the pixel buffers
// that Data publishes, without stopping. Returns 0, or why nothing was copied.
DWORD freeze();
// The game's own presented frame, as its window shows it. Pause menus, hint
// cards, subtitles and world markers are drawn only into this image, never
// into the eye views. While wanted, each Present of a swapchain on the game's
// device first copies its back buffer into `texture`, on the game's queue.
struct Screen {
    Microsoft::WRL::ComPtr<ID3D12Resource> texture; // PIXEL_SHADER_RESOURCE between copies
    DXGI_FORMAT view{};                           // read it as this
    bool linear{};                                // light values (float), not display-encoded
    uint32_t width{}, height{};
    uint64_t frame{};
};
void wantScreen(bool wanted);
// Whether the screen's copies are wanted now (wantScreen).
bool screenRequested();
// Presents of any swapchain in the process since start() (diagnostic).
uint32_t presents();
// While wanted and no screen copy is, each Present of the game's swapchain
// first draws the newest left eye over its back buffer, cut to the window's
// shape: the game's window in immersive VR while the game renders no view of
// its own. mirrored() counts those presents (diagnostic).
void mirror(bool wanted);
// At most this many presents a second reach the window while it mirrors the
// left eye (monitor_rate.hpp), and how many were held back.
void monitorRate(uint32_t rate);
uint32_t heldPresents();
uint32_t mirrored();
// The left image the headset is about to show: the one copyLatest filled this
// frame, the overlay's drawings on it, in RENDER_TARGET state. The window's
// mirror shows a copy of it while one is fresh (monitor_rate.hpp), so Spidy's
// own marks are on the monitor too. A copy is made only once the window has
// shown the last one: one eye-sized copy on the game's queue for each picture
// the window gets, none while it mirrors nothing. mirroredShown() counts the
// presents that drew such a copy (diagnostic).
void keepShownEye(ID3D12Resource* left);
uint32_t mirroredShown();
// 0.0.73 STREAMER MODE (streamer_view.hpp): while on, the window shows the copy of the
// headset's image whether or not the game renders its own view (in immersive play, not on a
// screen copy), sharpened or colour-graded as the headset shows it, through the streamer
// shader (16:9 crop about the eye's axis, head shake steadied by `smooth`, a
// streamer_view::Smooth). keepShownEye's overload takes the copied eye's view (orientation
// x, y, z, w in the XR space; fov left, right, down, up in radians); without a usable view,
// or when the shader would not build, the window gets the old mirror of that copy. The
// game thread sets it each frame; off restores everything as it was before this mode.
void streamer(bool on, uint32_t smooth);
bool streamerOn();
void keepShownEye(ID3D12Resource* eye, const float orientation[4], const float fov[4]);
// Presents that drew the copy through the streamer shader, and why it last could not
// (0 none, 1 pipeline, 2 no usable view) (diagnostic).
uint32_t streamerPresents();
uint32_t streamerFault();
// The newest copied frame, if one was copied within maxAgeMs.
bool latestScreen(Screen& out, uint64_t maxAgeMs);
// How the copy is read: the encoded format of the back buffer's family, or
// UNKNOWN for a format the screen does not show.
DXGI_FORMAT screenView(DXGI_FORMAT backBuffer, bool& linear);
// SpidyScreenData, for probes: status 1 once Present is hooked; the copies
// made, and the rows of the newest one SpidyGpuScreen(2) read back. Also the
// presents seen while wanted, why the last one was not copied (ScreenSkip),
// and the format of the last back buffer seen.
enum ScreenSkip : uint32_t {
    screenCopied = 0,
    screenOtherDevice = 1,
    screenNoBuffer = 2,
    screenFormat = 3,
    screenShape = 4,
    screenStopped = 5,
    screenTexture = 6,
    screenBusy = 7,
};
struct ScreenData {
    uint32_t magic = 0x53475353, version = 2, bytes = sizeof(ScreenData), status{};
    int64_t sequence{};
    uint64_t frames{}, pixels{};
    uint32_t width{}, height{}, format{}, rowBytes{};
    uint64_t presents{};
    uint32_t skip{}, backBufferFormat{};
};
static_assert(sizeof(ScreenData) == 72);
DWORD start(void* input);
DWORD stop();
} // namespace spidy::native_gpu
