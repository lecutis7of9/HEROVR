// Tests the engine's own secondary scene views on its view-maintenance thread.
// The original scene pass remains owned by the game. No desktop capture is used.
#include "spidy/eye_job_table.hpp"
#include "spidy/eye_resolution.hpp"
#include "spidy/engine_taa.hpp"
#include "spidy/taa_diag074.hpp"
#include "spidy/taa_hold074.hpp"
#include "spidy/image_style.hpp"
#include "spidy/eye_bloom.hpp"
#include "spidy/eye_queries.hpp"
#include "spidy/far_shadows.hpp"
#include "spidy/mirror_probe.hpp"
#include "spidy/mirror_view.hpp"
#include "spidy/native_appearance.hpp"
#include "spidy/native_body.hpp"
#include "spidy/native_eye_frame.hpp"
#include "spidy/native_eye_gpu.hpp"
#include "spidy/native_hud.hpp"
#include "spidy/native_view.hpp"
#include "spidy/native_address_finder.hpp"
#include "spidy/native_shadow_distance.hpp"
#include "spidy/native_webs.hpp"
#include "spidy/near_clip.hpp"
#include "spidy/shadow_camera.hpp"
#include "spidy/shared_lod.hpp"
#include "spidy/view_gates.hpp"
#include "spidy/window_pop.hpp"
#include "spidy/recording_fov.hpp"
#include <MinHook.h>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <initializer_list>
#include <intrin.h>
#include <windows.h>

// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2, Spider-Man2.exe 2.810.0.0).
// Every image offset (RVA) and structure offset in this file is Spider-Man 2's,
// cross-checked against the compiled SM2VR 0.0.27 stereo DLL; none is guessed.
// Features whose Spider-Man 2 values are not known yet are switched off here.
// Each switch is 0 by default; turning one on without its SM2TODO values filled
// in would call into the wrong code, so leave them at 0.
#ifndef SM2_STEREO_RESTED_MARKS
// SM2TODO: the eyes marking the resting game view's drawn list (job +0x2b0,
// view +0x1b80/+0x1b88 upstream). SM2VR removed this; offsets unknown for SM2.
#define SM2_STEREO_RESTED_MARKS 0
#endif
// Update 34: the target-kind widening and the job's temporal AA (upstream 3d11b18,
// 1875270, +0x40c, 1842e10) are found for Spider-Man 2 (engine_taa.hpp: mask table
// 0x5d65408, target maker 0x2c6eec0, TemporalAa 0x2c2dc60, the job's AA +0x570) and
// run as ENGINE TAA, opt-in at launch (-EngineTaa) and live; no compile switch.
#ifndef SM2_STEREO_HUD
// SM2TODO: the HUD panel placement hook (upstream 73ab80, follower vtable
// 38929c8) and native_hud.cpp. SM2VR does not hook them; values unknown.
#define SM2_STEREO_HUD 0
#endif
#if SM2_STEREO_RESTED_MARKS || SM2_STEREO_HUD
#error "SM2TODO: these features still use Spider-Man Remastered values; find the Spider-Man 2 ones first."
#endif
using namespace spidy;
namespace shadowCamera = spidy::shadow_camera;
using native_view::Descriptor;
// NEAR CLIP's near plane for the head's and the eyes' lenses in immersive VR
// (near_clip.hpp), metres: the XR worker sets it, each frame's lenses read it.
std::atomic<float> nearClip{.02f};
void spidy::near_clip::set(int millimetres) {
    nearClip.store(static_cast<float>(std::clamp(millimetres, 10, 100)) / 1000, std::memory_order_relaxed);
}
float spidy::near_clip::metres() {
    return nearClip.load(std::memory_order_relaxed);
}
// RECORDING FOV's angle (recording_fov.hpp), degrees, 0 for off: the XR
// worker sets it, each frame's rest and the game's view's lens read it.
std::atomic<int> recordingFov{};
void spidy::recording_fov::set(int degrees) {
    recordingFov.store(recording_fov::sanitized(degrees), std::memory_order_relaxed);
}
struct Config {
    uint32_t magic, version, bytes, pid;
    uint64_t base;
    uint32_t durationMs, createViews, width, height;
};
struct EyeSample {
    uint64_t view{}, texture{}, textureObject{}, updates{};
    // occlusion: the eye has its own occlusion object (view +1ba0).
    uint32_t flags{}, width{}, height{}, occlusion{};
    float pose[16]{};
    uint64_t readback{};
    uint32_t readbackBytes{}, readbackCompleted{};
    uint32_t renderWidth{}, renderHeight{}, viewportWidth{}, viewportHeight{};
};
struct Data {
    uint32_t magic = 0x53535042, version = 4, bytes = sizeof(Data), state{};
    volatile LONG64 sequence{};
    uint64_t frames{}, created{}, retired{}, primary{};
    uint32_t thread{}, error{};
    EyeSample eyes[2];
};
static_assert(sizeof(Config) == 40 && sizeof(EyeSample) == 144 && sizeof(Data) == 352);
// Diagnostic (SpidyCommandTrace): render commands queued, by command and by
// the view whose list took them (0 none of these, 1 the game's view, 2 and 3
// the eyes).
struct CommandData {
    uint32_t magic = 0x53434d44, version = 1, bytes = sizeof(CommandData), tracing{};
    uint64_t frames{};
    uint64_t counts[4][256]{};
};
// The eyes' shadow sets (SpidyEyeShadows): what each eye's scene submit made or
// took, how far the eyes' own key-light cascades were apart when last
// compared (the largest difference of a transform's and a half size's number),
// and how many cascade sets were fitted to the lens covering both eyes. An
// eye's `slot` is its nearest cascade's shadow map size.
struct ShadowEye {
    uint64_t submits{}, taken{};
    uint32_t flags{}, thread{};
    uint8_t cascades[2]{}, mayTake{}, order{};
    uint16_t lights{}, slot{};
    float extent[4][3]{};
    float pose[4][16]{};
};
// Update 25 (version 2, appended). headFits, headMissed, headFault and
// headSubmits were Update 25's HEAD SHADOWS; Update 26 removed it (SM2 never
// fits an eye's cascades inside its submit), so they stay 0, kept only so the
// layout tools read is unchanged. gameCompared / gameDelta: how far
// (metres) each eye's nearest key-light cascade sits from the game view's own
// one this frame (the one the frame's lighting constants come from).
struct ShadowData {
    uint32_t magic = 0x53534844, version = 2, bytes = sizeof(ShadowData), mode{};
    volatile LONG64 sequence{};
    uint64_t passes{}, compared{}, covered{};
    float poseDelta{}, extentDelta{}, poseDeltaMost{}, extentDeltaMost{};
    ShadowEye eyes[2];
    uint64_t headFits{}, headMissed{};
    uint32_t headFault{}, headSubmits{};
    uint64_t gameCompared{};
    float gameDelta[2]{}, gameDeltaMost[2]{};
};
static_assert(sizeof(ShadowEye) == 336 && sizeof(ShadowData) == 784);
extern "C" {
__declspec(dllexport) CommandData SpidyCommandData;
__declspec(dllexport) ShadowData SpidyShadowData;
__declspec(dllexport) Data SpidyStereoData;
__declspec(dllexport) native_eyes::FrameData SpidyStereoFrames;
__declspec(dllexport) native_appearance::Data SpidyAppearanceData;
// Update 28.1: EYE LOD's state, reason and last values (tools/eye_lod.py).
__declspec(dllexport) native_view::EyeLodData SpidyEyeLodData;
// Update 31: SHADOW CAMERA's mode, toggle log and the game view's cascades (tools/shadow_camera.py).
__declspec(dllexport) spidy::shadow_camera::Data SpidyShadowCameraData;
// Update 35: FAR SHADOWS' mode and what the cascade fits made (tools/run_game_vr.py).
__declspec(dllexport) spidy::far_shadows::Data SpidyFarShadowsData;
// Update 36: EYE BLOOM's and SHARED LOD's modes and what they saw (eye_bloom.hpp, shared_lod.hpp).
__declspec(dllexport) spidy::eye_bloom::Data SpidyEyeBloomData;
__declspec(dllexport) spidy::shared_lod::Data SpidySharedLodData;
// Update 37: BLOOM FX, LOD FADE, LOD REACH, IMPOSTORS and the bloom block dumps (view_gates.hpp).
__declspec(dllexport) spidy::view_gates::Data SpidyViewGatesData;
// Frames in which the eyes marked what they drew for the game (rested, below).
__declspec(dllexport) volatile LONG64 SpidyDrawnFrames;
// Update 37.1: declared here, at file scope, not inside the anonymous namespace: MSVC
// (C2733) takes an extern "C" declaration there for a different function from the export.
__declspec(dllexport) DWORD WINAPI SpidyEngineTaa(void* on);
__declspec(dllexport) uint32_t SpidyEngineTaaNow();
// Version 0.0.4: the MIRROR VIEW TEST (mirror_view.hpp), asked for and read by game_xr.cpp.
__declspec(dllexport) void SpidyMirrorViewWant(uint32_t on);
__declspec(dllexport) void SpidyMirrorViewRead(spidy::mirror_view::Stats* out);
}
namespace {
using Maintain = void (*)(void*);
// SM2: the view constructor takes its arguments in one block (SM2VR).
struct CreateArgs {
    const char* name;
    float nearZ, farZ, fov;
    uint32_t width, height, kind;
    void* parent;
    uint32_t a, b, c, d;
};
static_assert(sizeof(CreateArgs) == 0x38, "SM2 view constructor arguments");
using Create = void* (*)(void*, const CreateArgs*);
using Submit = void (*)(void*, const Descriptor*);
// SM2 2c76310 takes a third argument: every SM2VR 0.0.27 call passes 0 in r8
// (xor r8d, r8d before its calls at DLL +2046, +7222 and +77f3).
using SetPose = void (*)(Descriptor*, const float*, uintptr_t);
using Lens = void (*)(Descriptor*, float, float, float, float, float, float, float, float, float, float, bool,
                      bool);
using Tone = uint64_t (*)(void*, void*, void*, void*, void*, uint64_t);
using RenderOffscreen = uint8_t (*)();
Maintain originalMaintain{}, originalUpdate{}, originalCopyFinal{};
Tone originalTone{};
Submit originalSubmit{};
RenderOffscreen originalRenderOffscreen{};
void* submitHook{};
void* renderOffscreenHook{};
// The stock camera's latest submit was replaced by the tracked head view.
std::atomic<bool> activeAligned{};
// Update 31: SHADOW CAMERA (shadow_camera.hpp). The XR worker sets the mode
// (SpidyShadowCamera); the render thread reads it once per submit and goes back
// to OFF with a reason when the engine rejects a view built for it.
std::atomic<uint32_t> shadowCameraMode{};
std::atomic<uint32_t> shadowCameraWhy{};
std::atomic<float> flatNearZ{};
uint64_t shadowCameraSegment{}; // frames applied since the last change (under the data's sequence)
SRWLOCK shadowCameraLock = SRWLOCK_INIT;
void shadowCameraLog(uint32_t mode, uint32_t reason) {
    AcquireSRWLockExclusive(&shadowCameraLock);
    auto& d = SpidyShadowCameraData;
    InterlockedIncrement64(reinterpret_cast<volatile LONG64*>(&d.sequence));
    auto& e = d.event[d.events % spidy::shadow_camera::maxEvents];
    e.ms = GetTickCount64();
    e.mode = mode;
    e.reason = reason;
    e.applied = shadowCameraSegment;
    shadowCameraSegment = 0;
    ++d.events;
    ++d.toggles;
    d.mode = mode;
    d.reason = reason;
    InterlockedIncrement64(reinterpret_cast<volatile LONG64*>(&d.sequence));
    ReleaseSRWLockExclusive(&shadowCameraLock);
    char line[200];
    std::snprintf(line, sizeof line, "SHADOW CAMERA: %s at %llu ms%s%s\n", spidy::shadow_camera::modeName(mode),
                  static_cast<unsigned long long>(e.ms), reason ? " - back to OFF: " : "",
                  spidy::shadow_camera::reasonText(reason));
    OutputDebugStringA(line);
}
// Render thread: a view built for the mode was rejected; OFF from the next submit.
void shadowCameraFail(uint32_t reason) {
    if (shadowCameraMode.exchange(0) == 0)
        return;
    shadowCameraWhy = reason;
    shadowCameraLog(0, reason);
}
// Update 32: the flat camera's and the head's poses as last seen by activeView
// (under shadowCameraLock), for FitViews.
float fitRefFlat[16]{}, fitRefHead[16]{};
bool fitRefFlatOk{}, fitRefHeadOk{};
void fitRefs(const float* flat, const float* head) {
    AcquireSRWLockExclusive(&shadowCameraLock);
    if (flat) {
        std::memcpy(fitRefFlat, flat, sizeof fitRefFlat);
        fitRefFlatOk = true;
    }
    if (head) {
        std::memcpy(fitRefHead, head, sizeof fitRefHead);
        fitRefHeadOk = true;
    }
    ReleaseSRWLockExclusive(&shadowCameraLock);
}
void shadowCameraApplied(uint32_t mode) {
    if (mode >= spidy::shadow_camera::modes)
        return;
    AcquireSRWLockExclusive(&shadowCameraLock);
    auto& d = SpidyShadowCameraData;
    InterlockedIncrement64(reinterpret_cast<volatile LONG64*>(&d.sequence));
    ++d.applied[mode];
    ++shadowCameraSegment;
    InterlockedIncrement64(reinterpret_cast<volatile LONG64*>(&d.sequence));
    ReleaseSRWLockExclusive(&shadowCameraLock);
}
using RenderActor = void (*)(void*, void*, uint8_t);
// SM2 2d016d0 has a fifth argument: SM2VR's detour (DLL +9180) copies the
// caller's fifth (stack) argument through to the original.
using SetupDisplay = void (*)(void*, void*, void*, uint64_t, uint64_t);
using Visibility = void (*)(void*);
RenderActor originalRenderActor{};
SetupDisplay originalSetupDisplay{};
void* renderActorHook{};
void* setupDisplayHook{};
std::atomic<uint64_t> playerRecord{}, localActor{};
// latchedHide: immersive, and the eyes must not draw the hero, because the
// body (native_body) is not turning its joints with its head shrunk.
std::atomic<bool> latchedImmersive{}, latchedHide{};
// The hero's render instance and world transform for the frame the main
// thread just prepared. Render jobs read the newest of several slots.
struct HeroFrame {
    uintptr_t instance{};
    float transform[16]{};
};
std::array<HeroFrame, 4> heroFrames{};
std::atomic<uint32_t> heroSlot{};
// Instance whose native visibility this module switched off, and its handle.
std::atomic<uintptr_t> hiddenHero{};
uint32_t hiddenHandle{};
struct RenderOffset {
    uint64_t generation{};
    Vec3 offset{};
    // Game rope starts of the same frame (bit per hand in `ropes`).
    Vec3 ropeStarts[2]{};
    uint32_t ropes{};
};
std::array<RenderOffset, 64> offsets{};
SRWLOCK offsetLock = SRWLOCK_INIT;
// Hero position the active view was placed from this frame (main thread).
Vec3 activeHeroPosition{};
bool activeHeroPlaced{};
// Frames end with view maintenance (main thread). placedFrame is the frame
// whose eye poses were placed last; a job copied in a later frame got a late pose.
uint64_t frameIndex{}, placedFrame{};
// Update 21: the eye camera audit's state (jobLock): the poses given SetPose,
// and the left eye's rendered position, for the eyes' distance apart.
native_eyes::EyeAsked eyeAsked[2]{};
float leftRendered[3]{};
uint64_t leftRenderedSerial{};
bool leftRenderedKnown{};
// Diagnostic comparison only (SpidyEyePlacement): place eyes in maintenance,
// as builds before October 5 did.
std::atomic<bool> lateEyes{};
// Diagnostic comparison only (SpidyEyeExposure): each eye is exposed from its
// own adapted luminance, as builds before October 10's sixth did.
std::atomic<bool> ownExposure{};
using InitBuffers = bool (*)(void*, const void*, const char*);
InitBuffers originalInitBuffers{};
void* initBuffersHook{};
// The eyes' temporal anti-aliasing and upsampling (eyeTemporal below).
// 1874f40 sizes a buffer set: the render size (+12bf0, +12bf4) from its
// arguments, and the size its anti-aliased image has (+12bf8 to +12c04, where
// kTaaTarget0/1, kResolved and their kind are made) from the game's display
// (7a2ef00), whatever the view. An eye's set gets the eye's own size there.
using SizeBuffers = void (*)(void*, const char*, uint32_t, uint32_t, uint32_t, uint8_t);
SizeBuffers originalSizeBuffers{};
void* sizeBuffersHook{};
// The eye whose buffers 1873470 is sizing on this thread: its image's size.
thread_local uint32_t sizingEye[2]{};
// Percent of the eye's size, per side, its scene is rendered at (100: all of
// it), and what the eyes get (bits): 1 a jittered lens, 2 the engine's
// temporal anti-aliasing (which upsamples a smaller scene to the eye's size),
// 4 its render command where the game's settings leave it out, 8 no targets
// for screen-space reflections (as before October 11's second build: the game
// has them off, or a comparison). The percent and bits 2 and 8 are read when
// an eye's buffers are made. (Clearing the job's context +710 in copyJob does
// not stop the reflection pass: tried on October 11, the eyes kept them.)
std::atomic<uint32_t> eyeRenderPercent{100}, eyeTemporal{};
// Update 34: ENGINE TAA (engine_taa.hpp). eyeTemporal 0x10 (-EngineTaa): each eye's
// buffer set is made with the TAA kind in the mask table, so it gets the TAA targets.
// engineTaaLive: the eyes get a jittered lens (eyeJitter) and their job's AA 1
// (copyJob). engineTaaWhy: why it last went off on its own (engine_taa::Reason).
std::atomic<bool> engineTaaLive{};
// Update 37: SpidyEngineTaa(3), on by itself the first time it can be (the eyes made with
// the TAA targets, the game's AA on TAA); cleared by an off or the frame-rate guard.
std::atomic<bool> engineTaaAuto{};
// 0.0.73 test 4 IMAGE STYLE (image_style.hpp; SpidyImageStyle, game_xr sets it): SPIDYVR by default.
// Under SPIDYVR the jitter is never NONE (NONE skips the game's TemporalAa: image_style::jitterMode).
std::atomic<uint32_t> imageStyle{spidy::image_style::defaultStyle};
// (SpidyEngineTaa is declared at file scope, in the extern "C" block above: Update 37.1, MSVC C2733.)
std::atomic<uint32_t> engineTaaWhy{}, engineTaaGameAa{UINT32_MAX}, engineTaaWidened{}, engineTaaMaskSeen{};
std::atomic<float> gameJitter[2]{}, eyeJitterSent[2]{};
// Update 34.2: ENGINE TAA's jitter mode and scale (engine_taa::JitterMode), live, never
// kept; and what it did, for the report (SpidyEngineTaaData, tools/run_game_vr.py).
// Update 37: NONE by default (the run36 closing setting).
std::atomic<uint32_t> engineTaaJitterMode{spidy::engine_taa::noJitter},
    engineTaaJitterScale{spidy::engine_taa::defaultJitterScale};
std::atomic<uint64_t> engineTaaLenses{}, engineTaaFrames{};
std::atomic<uint32_t> engineTaaOns{}, engineTaaRefusals{}, engineTaaOffs{}, engineTaaLastRefusal{},
    engineTaaModeChanges{};
std::atomic<float> engineTaaGameMax[2]{}, engineTaaEyeMax[2]{};
SRWLOCK maskLock = SRWLOCK_INIT;
// A view's scene submit, 19206a0(view, parent, flags, last), returns what it
// made for the frame (0x58 bytes: +18 the view's light set with its shadow
// records, +28 its KeyLightContext, the key light's shadow cascades, +30,
// ...). Given another view's as parent, a view whose +120a is set (every
// view's is, from 189d3d2) copies the parent's KeyLightContext and takes its
// light set instead of making its own (1920eeb, 19212e9): it then renders no
// shadow maps and samples the parent's. The game passes no such parent to an
// offscreen view (186d72a), so each eye rendered every cascade again, 6 cm
// from the other eye's. eyeShadows (bits): the eye submitted second in a
// frame takes from the first 1 the key light's cascades, 2 the light set,
// 4 the part at +30.
// A view's cascades are fitted to the view: 1716270 calls 170fbc0(context,
// view, ...), which reads the pose and the lens from the view's descriptor
// (the view's first 0x440 bytes) and places each cascade's box about that
// lens's part of the scene. Two eyes with mirrored lenses got boxes most of
// a half size apart (13.0 m of 13.2 for the nearest). So the eye submitted
// first fits its cascades to shadowLens, the head's lens covering both eyes
// (placeEyes), and the second takes those.
// The covering lens is wider than an eye's, and its boxes larger (15% a side
// for the load probe's lenses, 54 degrees out and 44 in) over shadow maps of
// the same size: 170fbc0 asks the atlas (774c4d0) for each cascade's map with
// 1740890(atlas, key, size), the size the atlas's first word (1792 here). A
// texel of the shared set so covers 15% more a side than one of an eye's own.
// Asking for larger maps was tried on October 11: the atlas had no room for
// all six, and the game's render code reads a cascade's map without checking
// that it has one (16364df).
// SM2 2d01b30 reads a fifth argument and a full 64-bit fourth one: SM2VR's
// detour (DLL +96f0) keeps r9 whole and passes the caller's fifth (stack)
// argument on at both of its calls to the original (+98e5, +9f21).
using SubmitScene = void* (*)(void*, void*, uint32_t, uint64_t, uint64_t);
using FitCascades = void (*)(void*, void*, void*, void*, void*);
SubmitScene originalSubmitScene{};
FitCascades originalFitCascades{};
void* submitSceneHook{};
void* fitCascadesHook{};
std::atomic<uint32_t> eyeShadows{};
// This frame's offscreen job setup (main thread), and the eye submitted first in it.
uint64_t offscreenPass{};
struct ShadowDonor {
    uint64_t pass{};
    void* view{};
    bool covers{};
    uintptr_t part[11]{};
} shadowDonor;
Descriptor shadowLens{};
uint64_t shadowLensPass = UINT64_MAX;
// Both eyes have one lens and pose this frame (the flat screen): each keeps
// its own set there (sharing it is untried).
bool shadowLensSame{};
// The eye whose scene submit is fitting its cascades to shadowLens.
void* shadowFitting{};
// The offscreen pass each eye's cascades in SpidyShadowData were read in.
uint64_t eyeShadowPass[2]{UINT64_MAX, UINT64_MAX};
// Diagnostic (SpidyCommandTrace): the newest render job of the game's view and of each eye.
std::atomic<uintptr_t> tracedJobs[3]{};
// The game's own view while this module keeps it from rendering (restGameView).
std::atomic<uintptr_t> rested{};
// What the player sees, for the game. Each scene submit clears its view's
// list of drawn instances (1920e54: view +1b80, +1b88 entries) and the view's
// model jobs mark every instance they draw (1799a7d, through job +2b0).
// Gameplay reads the lists to know what is on screen: the
// ViewportUpdateSystem's enter and leave events from the active view's
// (214f150), and "drawn in any view" from the pool views' (18a1890; for an
// instance 159e940: pedestrians, targeting, about 60 callers in all).
// Offscreen views get no list (189ce20 skips kind 2), so the eyes marked
// nothing, and a resting game view submits no scene, so its list kept the last
// frame that view drew: from 0.2.12 on nothing the player saw in VR counted as
// on screen. While the view rests, placeEyes clears its list each frame and
// the eyes' model jobs mark into it (setupModels); SpidyDrawnFrames counts
// those frames.
// The eyes' distances (createViews bit 64 off). A wide lens makes the engine
// draw a model's detail, and the model itself, only within a shorter distance
// and ask for its textures only nearer (native_view::distanceScale): an eye's
// 98 degrees gave 0.59 where the game's camera has 1.06, so windows came onto
// buildings, people into the streets and textures into focus at about half
// the flat game's distance. Each eye gets the 68-degree lens's distances
// (matchDistances, view +1cc8), and its model jobs ask for textures as for its
// image, not for the smaller scene its upscaling renders (setupModels, +2fc).
using SetupModels = uint64_t (*)(void*, void*, void*, uint64_t, uint64_t);
SetupModels originalSetupModels{};
void* setupModelsHook{};
bool distances{};
// The far buildings' full models (createViews bit 128 off). Far buildings are
// impostors, and only the engine's active view (189daa0: the view at
// ViewContextManager +f8) has the impostors it sees draw their full models,
// with the lit windows and the rooms behind them: ImpostorHQModelRegJob
// (16fb630, 16fb750) applies that view's wanted state per impostor, from its
// visibility queries (16faf10), and the impostor submit job (16f8afa) marks
// the view. A resting game view (restGameView) submits no scene, so in VR the
// buildings beyond the nearest stayed dark cards until a menu, which renders
// that view again, lit them. While the game view rests the left eye stands in
// for it at those five checks (isActiveView). One view only: the
// registration applies a view's wanted states over everyone's, and two views
// would undo each other's every frame.
using IsView = bool (*)(void*);
IsView originalIsActiveView{};
void* isActiveViewHook{};
bool impostors{};
// How far away an impostor draws its full model then: the eyes' visibility
// queries (16faf10) carry the game's distance in their result (+88: 145 m from
// 7a89170 or 7a89174, which region changes rewrite), and the impostor jobs
// read it after the queries return. While an eye stands in, both eyes' results
// get impostorReach percent of it (impostorQueries), so both draw alike;
// SpidyImpostorReach sets it.
using ImpostorQueries = void* (*)(void*, void*);
ImpostorQueries originalImpostorQueries{};
void* impostorQueriesHook{};
std::atomic<uint32_t> impostorReach{150};
// Update 38.3 FAR WINDOWS: the night windows' light popping on as you near a building (run382,
// 00:23-26). Two writes, both live, percent of the game's (100 GAME, 150, 200 default, 300):
//  1. the eyes' detail reach (viewScale's LOD REACH multiplier, the SHARED LOD write that is
//     CONFIRMED in SM2): FAR WINDOWS raises it to its percent when lod_reach is lower;
//  2. the impostors' full-model distance (+88 of impostorQueries' result, the hook that SM2
//     installs but whose 145 m bytes SM2 never checked: createViews flag 32 is never set). It is
//     a CANDIDATE: farWindowsReady only when the function's prologue and both +88 stores read
//     the bytes above at 2a6bc60/2a6bcad/2a6bded in this exe; else it never writes (UNAVAILABLE).
std::atomic<uint32_t> farWindowsPercent{100};
// 0.0.73 test 2 CULL MARGIN (recording_fov::cullMarginOf): the degrees the game's view reaches past the head's lens.
std::atomic<uint32_t> cullMarginDegrees{recording_fov::cullMarginChoices[0]};
bool farWindowsReady{};
// Test build 11: write 2 (the CANDIDATE +88) is gated OFF. The Oct 28 build 9 log had it
// writing every eye query (31497 writes, 145 m -> 290 m) and the window lights flickering;
// the game reads +88 per view and region changes rewrite it, so the eyes' far models could
// switch between impostor and full model from one picture to the next. FAR WINDOWS keeps
// write 1 (LOD REACH, CONFIRMED) only.
constexpr bool farWindowsCandidateWrite = false;
// 0.0.73 test 6 WINDOW POP FIX (window_pop.hpp): ON (the default) gives the game's widened view flat play's
// detail distance (activeView sets gameLodFlat, viewScale writes it to the game view's +0x1b28).
std::atomic<bool> windowPopOn{true};
std::atomic<float> gameLodFlat{1.f}, gameLodApplied{1.f};
std::atomic<uintptr_t> gameLodWritten{};
std::atomic<uint64_t> windowPopWrites{};
std::atomic<uint64_t> farWindowsCalls{}, farWindowsWrites{};
std::atomic<float> farWindowsGame{}, farWindowsOurs{};
// The HUD panel (native_hud.hpp): PlayerModelHudFollower's placement 73ab80,
// on the main thread after the camera submits and before 1920240. It ends in
// SetTransform 191c0e0 on the panel's render instance, which writes the
// instance's world transform (+0) and marks it changed for the renderer.
using HudPlace = void (*)(void*);
using SetTransform = void (*)(void*, const float*, void*);
HudPlace originalHudPlace{};
void* hudPlaceHook{};
// The game's placement in this frame: the panel's instance, the scales it
// got, and its distance from the game's camera (follower +48).
struct HudPlacement {
    uintptr_t instance{};
    Vec3 scale{};
    float distance{};
};
// Half the width of the stock camera's view, as a tangent: what the game's
// own view shows of the HUD panel (submit(), main thread).
float stockHalfWidth{};
// The head of one frame (main thread): the eye command the frame's views
// render, the player's travel to the frame, and the head view and lens that
// gives (pose moved by the travel). Latched at the frame's first camera submit
// (activeView), or later at the HUD's placement without one: both come after
// the frame's gameplay has moved the player. The engine's active view, the HUD
// panel and the eyes all take it, so they agree on the head in every frame: a
// transform set any later than the game's own HUD placement (in placeEyes, at
// 1920240) did not reach the frame's render, so the panel is placed in that
// placement's hook, before the eyes.
// The HUD projects its world markers earlier, before the player has moved.
// They take earlyHead(): the newest command's head, moved to where the frame
// will draw the player (heroTravel). They latched the frame's head themselves
// from October 9's fifth build to October 10's third, with the last frame's
// player: the active view, which the monitor shows, and the panel trailed the
// eyes by a frame of travel, up to 1.35 m in a swing.
struct FrameHead {
    uint64_t frame = UINT64_MAX;
    native_eyes::Command command{};
    bool controlled{}, viewed{};
    Vec3 offset{};
    Mat4 pose{};
    native_eyes::Bounds lens{};
    // The head turned the way the HUD panel faces (native_hud::Follow, sent by
    // the XR worker with the command), and whether the worker sent one.
    Mat4 panel{};
    bool followed{};
};
FrameHead frameHead, earlyFrameHead;
// The HUD panel's render instance while the HUD is off and the eyes are shown:
// renderActor leaves it out of the eye views (main thread sets it).
std::atomic<uintptr_t> hiddenHud{};
// The game's main thread (view maintenance's), the only one the frame's head
// is latched and read on.
std::atomic<DWORD> mainThread{};
thread_local bool creatingEye{};
using CopyJob = void (*)(void*, void*, void*, void*, bool);
CopyJob originalCopyJob{};
Maintain originalBeginJob{}, originalEndJob{};
uintptr_t base{};
void* maintainHook{};
void* updateHook{};
void* copyFinalHook{};
void* toneHook{};
void* copyJobHook{};
void* beginJobHook{};
void* endJobHook{};
std::atomic<void*> eyes[2]{};
std::atomic<uint64_t> updates[2]{};
std::atomic<bool> enabled{}, stopRequested{};
std::atomic<uint32_t> live{};
SRWLOCK lifecycle = SRWLOCK_INIT, telemetry = SRWLOCK_INIT, creation = SRWLOCK_INIT;
Config config{};
uint64_t deadline{};
bool hooked{}, attempted{}, retired[2]{};
// Version 0.0.4 MIRROR VIEW TEST: made only where the eyes are made (creation lock), placed with
// them (main thread), retired after 30 s or when frames slow (never deleted, like the eyes).
std::atomic<uint32_t> mirrorWanted{};
std::atomic<void*> mirrorView{};
bool mirrorRetired{};
SRWLOCK mirrorLock = SRWLOCK_INIT;
spidy::mirror_view::Stats mirrorStats; // mirrorLock
LARGE_INTEGER mirrorLastTick{};
void mirrorPlace(const Descriptor& main, const Mat4& head);
SRWLOCK commandLock = SRWLOCK_INIT, jobLock = SRWLOCK_INIT;
native_eyes::Command command{};
std::atomic<uint64_t> commandDeadline{};
std::atomic<float> flatAspect{16.f / 9};
std::atomic<uint64_t> latchedSerial{}, generation{};
EyeJobTable jobs; // jobLock
// Hero rope-update samples already used by eye placement and the active view.
native_eyes::FrameHero placedHero, activeHero;
// Hero positions the frames' heads were placed from (main thread).
native_eyes::HeroTravel heroTravel;
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
    read(p, &v, 8);
    return v;
}
bool owned(void* view) {
    if (!view)
        return false;
    for (unsigned slot = 0; slot < 8; ++slot)
        if (pointer(base + 0xc2ca250 + slot * 8) == reinterpret_cast<uintptr_t>(view))
            return true;
    return false;
}
bool entry(uintptr_t rva, const unsigned char* expected, size_t n) {
    unsigned char b[32]{};
    return n <= sizeof(b) && read(base + rva, b, n) && !std::memcmp(b, expected, n);
}
// Offscreen views are made without the occlusion culling the game's views
// have: their setup (186c1f0) passes 1 as the pool initializer's skip argument,
// so the culling (ModelOcclJob 17935a0 and a dozen other readers of +1208 and
// +1ba0) found no occlusion object and kept every instance in an eye's
// frustum, hidden behind buildings or not. 189fed0 builds the object from the
// view's render buffers (+1640), as 189ce20 does for game views; each eye's
// render job then samples the eye's own depth into it (19206a0). It stores the
// object before constructing it, so it may only run on a new eye that no
// render job has copied yet.
void giveOcclusion(void* view) {
    const auto address = reinterpret_cast<uintptr_t>(view);
    if (!pointer(address + 0x1a00) && pointer(address + 0x18e0))
        reinterpret_cast<void (*)(void*)>(base + 0x2c9b8e0)(view);
}
void appearanceEvent(unsigned eye, bool avatar) {
    AcquireSRWLockExclusive(&telemetry);
    auto& data = SpidyAppearanceData;
    InterlockedIncrement64(&data.sequence);
    if (avatar)
        ++data.hiddenAvatar[eye];
    else
        ++data.srgbOverlay[eye];
    data.playerActor = localActor.load();
    InterlockedIncrement64(&data.sequence);
    ReleaseSRWLockExclusive(&telemetry);
}
bool unitTransform(const float* m) {
    for (int i = 0; i < 16; ++i)
        if (!std::isfinite(m[i]) || std::abs(m[i]) > 1e6f)
            return false;
    for (int row = 0; row < 3; ++row)
        if (std::abs(m[row * 4] * m[row * 4] + m[row * 4 + 1] * m[row * 4 + 1] + m[row * 4 + 2] * m[row * 4 + 2] -
                     1) > .02f)
            return false;
    return true;
}
// 191ba30 resolves an instance handle (index in bits 0..23, generation in
// 24..31) through the 16-byte table at 7a436e8. A live instance is its own
// table entry. DrawOff/DrawOn mark the same index in the table at 7a43710.
bool registered(uintptr_t instance, uint32_t& handle) {
    const auto table = pointer(base + 0xc2d2d08);
    int32_t count{};
    uint8_t slotGeneration{};
    if (!instance || !table || !read(instance + 0x64, &handle, 4) || !(handle >> 24) ||
        !read(base + 0xc2d2d24, &count, 4))
        return false;
    const uint64_t index = handle & 0xffffff;
    return static_cast<int64_t>(index) < count && pointer(table + index * 16) == instance &&
           read(table + index * 16 + 8, &slotGeneration, 1) && slotGeneration == handle >> 24;
}
// Child pieces of a character share its root transform; world props do not.
bool sameRoot(const float* m, const float* hero) {
    const float dx = m[12] - hero[12], dy = m[13] - hero[13], dz = m[14] - hero[14];
    if (dx * dx + dy * dy + dz * dz > .03f * .03f)
        return false;
    for (int row = 0; row < 3; ++row)
        if (m[row * 4] * hero[row * 4] + m[row * 4 + 1] * hero[row * 4 + 1] + m[row * 4 + 2] * hero[row * 4 + 2] <
            .998f)
            return false;
    return true;
}
int eyeView(uintptr_t view) {
    if (!view)
        return -1;
    // 1920e54 passes the persistent view to both 19223e0 and 1793be0. Render
    // copies keep their owner at +1f40, as in the display-setup hook.
    const auto owner = pointer(view + 0x1dc8);
    for (unsigned eye = 0; eye < 2; ++eye)
        if (const auto e = reinterpret_cast<uintptr_t>(eyes[eye].load()); e && (view == e || owner == e))
            return static_cast<int>(eye);
    return -1;
}
bool headMirror(uintptr_t view) {
    const auto primary = pointer(base + 0xc2ca220);
    return activeAligned.load(std::memory_order_relaxed) && primary && view &&
           (view == primary || pointer(view + 0x1dc8) == primary);
}
void nearInstance(unsigned eye, uintptr_t instance, const float* m, const float* hero) {
    // Diagnostic only: identify small pieces still drawn within the body,
    // measured from a point 0.9 m above the hero's feet.
    const float dx = m[12] - hero[12], dy = m[13] - hero[13] - .9f, dz = m[14] - hero[14];
    const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
    float radius{};
    if (distance > 1.1f || !read(instance + 0x4c, &radius, 4) || !std::isfinite(radius) || radius > 2.5f)
        return;
    uint32_t handle{}, flags{};
    read(instance + 0x64, &handle, 4);
    read(instance + 0x5c, &flags, 4);
    const auto model = pointer(instance + 0x88);
    AcquireSRWLockExclusive(&telemetry);
    auto& data = SpidyAppearanceData;
    InterlockedIncrement64(&data.sequence);
    ++data.nearInstances[eye];
    data.nearInstance = instance;
    data.nearModel = model;
    data.nearDistance = distance;
    data.nearRadius = radius;
    data.nearHandle = handle;
    data.nearFlags = flags;
    InterlockedIncrement64(&data.sequence);
    ReleaseSRWLockExclusive(&telemetry);
}
void renderActor(void* context, void* actor, uint8_t visibility) {
    // The HUD off: its panel stays out of the eyes (the game's own view, which
    // the game screen shows, keeps it).
    if (const auto hud = hiddenHud.load(std::memory_order_relaxed);
        enabled && hud && reinterpret_cast<uintptr_t>(actor) == hud &&
        eyeView(pointer(reinterpret_cast<uintptr_t>(context) + 8)) >= 0) {
        native_hud::hiddenDraw();
        return;
    }
    // Update 38.4 MIRROR PROBE: in one frame of 32, which views draw the hero and the pieces on his
    // root transform, before the hiding below (counts only; mirror_probe.cpp).
    if (enabled && actor && spidy::mirror_probe::sampling()) {
        const auto view = pointer(reinterpret_cast<uintptr_t>(context) + 8);
        spidy::mirror_probe::draw(view, view ? pointer(view + 0x1dc8) : 0, reinterpret_cast<uint64_t>(actor));
    }
    // 1796d20 copies the scene view into context +8. The primary hide uses the
    // game's actor visibility switch in maintain(); this view check removes
    // anything still drawn as the hero or on the hero's root transform. It
    // changes no actor flags, animation state, or gameplay visibility. With
    // the player's body on the hero (native_body), the eyes draw it. A desktop
    // view on the tracked head needs the same fallback as the eyes.
    if (enabled && actor && latchedHide.load(std::memory_order_relaxed)) {
        const int eye = eyeView(pointer(reinterpret_cast<uintptr_t>(context) + 8));
        const auto& hero = heroFrames[heroSlot.load(std::memory_order_acquire) % heroFrames.size()];
        const auto instance = reinterpret_cast<uintptr_t>(actor);
        if ((eye >= 0 || headMirror(pointer(reinterpret_cast<uintptr_t>(context) + 8))) && hero.instance) {
            float m[16]{};
            const bool placed = instance != hero.instance && read(instance, m, sizeof(m));
            // The game's web tube and end cone are separate instances; never
            // treat them as avatar pieces.
            const bool web = placed && native_webs::webInstance(instance);
            if (instance == hero.instance || (placed && !web && sameRoot(m, hero.transform))) {
                if (eye >= 0)
                    appearanceEvent(static_cast<unsigned>(eye), true);
                return;
            }
            if (eye >= 0 && placed && !web)
                nearInstance(static_cast<unsigned>(eye), instance, m, hero.transform);
        }
    }
    originalRenderActor(context, actor, visibility);
}
// 1796d20 sets up a view's model job (the view at context +8): its distance
// scale (+2f8, view +1cc4 x +1cc8; +310, 16 times that, for texture requests)
// and how strongly it asks for textures (+2fc, requestScale of the view's
// rendered size). An upscaled eye renders its scene smaller than its image, so
// it asked for textures as for the smaller one: two thirds of the detail the
// headset shows at the default 67%. Its requests are sized for its image, as
// upscalers are meant to have them; what the job leaves out as too small to
// see (+300) stays at the rendered size. While the game's view rests, an eye's
// job marks what it draws in that view's list (+2b0; see rested above).
uint64_t setupModels(void* context, void* job, void* third, uint64_t fourth, uint64_t fifth) {
    const auto result = originalSetupModels(context, job, third, fourth, fifth);
#if SM2_STEREO_RESTED_MARKS
    if (const auto resting = rested.load(std::memory_order_relaxed);
        enabled && resting && eyeView(pointer(reinterpret_cast<uintptr_t>(context) + 8)) >= 0) {
        auto& marks = *reinterpret_cast<uintptr_t*>(static_cast<uint8_t*>(context) + 0x2b0);
        if (const auto drawn = pointer(resting + 0x1b80); drawn && !marks)
            marks = drawn;
    }
#endif
    const auto view = enabled && distances ? pointer(reinterpret_cast<uintptr_t>(context) + 8) : 0;
    float half[2]{};
    if (eyeView(view) < 0 || !read(view + 0x420, half, sizeof(half)) || !(half[0] > 0) || !(half[1] > 0))
        return result;
    const float wanted = native_view::requestScale(static_cast<float>(config.width),
                                                   static_cast<float>(config.height), half[0], half[1]);
    auto& asked = *reinterpret_cast<float*>(static_cast<uint8_t*>(context) + 0x2fc);
    if (!std::isfinite(wanted) || !(asked > 0) || !(wanted > asked))
        return result;
    AcquireSRWLockExclusive(&telemetry);
    InterlockedIncrement64(&SpidyAppearanceData.sequence);
    ++SpidyAppearanceData.requestJobs;
    SpidyAppearanceData.requestRaise = wanted / asked;
    InterlockedIncrement64(&SpidyAppearanceData.sequence);
    ReleaseSRWLockExclusive(&telemetry);
    asked = wanted;
    return result;
}
// Update 35: FAR SHADOWS (far_shadows.hpp). The fit 2a8dc80 gives a view far
// cascades only when IsActiveView says yes at 2a8e145; the eyes never were.
// farShadowsMode is read by isActiveView on the render jobs; csmFit records what
// each fit made and judges a mode over its first eye fits (auto-revert).
using CsmFit = uint64_t (*)(void*, void*, void*, void*);
CsmFit originalCsmFit{};
void* csmFitHook{};
bool farShadowsReady{}; // the fit hooked and both IsActiveView calls as known
std::atomic<uint32_t> farShadowsMode{spidy::far_shadows::defaultMode}, farShadowsWhy{};
std::atomic<uint64_t> farAnsweredFit{}, farAnsweredCache{};
SRWLOCK farLock = SRWLOCK_INIT;
uint64_t farJudgedGame{}, farJudgedGameFar{}; // farLock: the game view's fits since the change
bool farJudging{};                              // farLock
// Any thread: the mode in force (reason: why it changed on its own, else none).
void farShadowsSet(uint32_t mode, uint32_t reason, bool automatic) {
    namespace fs = spidy::far_shadows;
    const uint32_t old = farShadowsMode.exchange(mode);
    farShadowsWhy = reason;
    AcquireSRWLockExclusive(&farLock);
    auto& d = SpidyFarShadowsData;
    InterlockedIncrement64(reinterpret_cast<volatile LONG64*>(&d.sequence));
    d.packed = fs::pack(mode, reason);
    if (old != mode)
        ++d.modeChanges;
    if (automatic) {
        ++d.reverts;
        d.lastRevert = reason;
    }
    d.judgedFits = d.judgedFar = 0;
    farJudgedGame = farJudgedGameFar = 0;
    farJudging = mode != fs::off;
    InterlockedIncrement64(reinterpret_cast<volatile LONG64*>(&d.sequence));
    ReleaseSRWLockExclusive(&farLock);
    char line[160];
    std::snprintf(line, sizeof line, "FAR SHADOWS: %s%s%s\n", fs::modeName(mode),
                  automatic ? " - on its own: " : "", automatic ? fs::reasonText(reason) : "");
    OutputDebugStringA(line);
}
// 0.0.73 test 2 SHADOW MATCH (fix 21, combat38::shadowMatch, EXPERIMENTAL, off by default): SpidyVR's and the
// Miles VR mod's shadow fix - both eyes' sun cascades fitted to ONE lens, the head's lens covering both eyes
// (shadowLens), so the two eyes' shadow maps agree (no shimmer that differs between the eyes). Each eye still
// makes its own maps: nothing is shared between the eyes (sharing crashed the render thread in SM2VR 0.0.4).
// CONFIRMED: the fit 0x2a8dc80 (far_shadows, csmFit) and the head lens (built every frame for the game view).
// CANDIDATE: that 0x2a8dc80 fits to the view descriptor's pose and lens, as SM1's fit did (SpidyVR's swap).
// 0.0.73 LIGHT CHECKS (eye_queries.hpp): 1 on (the default, so eyes made before the wrist's first push
// get it), 0 off. Read when an eye is made. State: 0 no eye yet, 1 widened, 2 off, 3 the clamp's bytes
// differ (the game's 256 kept), 4 a page protection failed (the game's 256 kept).
std::atomic<uint32_t> lightChecksOn{1}, lightChecksState{};
std::atomic<int32_t> lightChecksRoom[2]{};
std::atomic<uint32_t> shadowMatchOn{};
std::atomic<uint64_t> shadowMatchFits{}, shadowMatchSkipped{};
SRWLOCK matchLock = SRWLOCK_INIT;
Descriptor matchLens{};    // matchLock
uint64_t matchLensMs{};     // matchLock: when it was made (GetTickCount64)
bool matchLensUsable{};     // matchLock: false on the flat screen (both eyes one lens)
constexpr uint64_t matchFreshMs = 60;
void publishMatchLens(const Descriptor& lens, bool usable) {
    AcquireSRWLockExclusive(&matchLock);
    matchLens = lens;
    matchLensMs = GetTickCount64();
    matchLensUsable = usable;
    ReleaseSRWLockExclusive(&matchLock);
}
uint64_t csmFit(void* csm, void* view, void* lights, void* settings) {
    namespace fs = spidy::far_shadows;
    uint64_t result{};
    bool matched{};
    if (enabled && view && shadowMatchOn.load(std::memory_order_relaxed) &&
        (view == eyes[0].load(std::memory_order_relaxed) || view == eyes[1].load(std::memory_order_relaxed))) {
        Descriptor lens{}, own{};
        bool have{};
        AcquireSRWLockShared(&matchLock);
        const uint64_t now = GetTickCount64();
        if (matchLensUsable && now >= matchLensMs && now - matchLensMs <= matchFreshMs) {
            lens = matchLens;
            have = true;
        }
        ReleaseSRWLockShared(&matchLock);
        if (have && native_view::valid(lens) && read(reinterpret_cast<uintptr_t>(view), &own, sizeof own)) {
            std::memcpy(view, &lens, sizeof lens);
            result = originalCsmFit(csm, view, lights, settings);
            std::memcpy(view, &own, sizeof own);
            matched = true;
            shadowMatchFits.fetch_add(1, std::memory_order_relaxed);
        } else {
            shadowMatchSkipped.fetch_add(1, std::memory_order_relaxed);
        }
    }
    if (!matched)
        result = originalCsmFit(csm, view, lights, settings);
    if (!enabled || !csm || !view)
        return result;
    void* l = eyes[0].load(std::memory_order_relaxed);
    void* r = eyes[1].load(std::memory_order_relaxed);
    const int eye = view == l ? 0 : view == r ? 1 : -1;
    const bool game = eye < 0 && originalIsActiveView(view);
    const auto c = reinterpret_cast<uintptr_t>(csm);
    uint8_t counts[2]{};
    float lod[4]{}, farLod[4]{};
    uint32_t kind{};
    if (!read(c + fs::baseCount, counts, 2) || !read(c + fs::lodConsts, lod, 16) ||
        !read(c + fs::farLodConsts, farLod, 16))
        return result;
    read(reinterpret_cast<uintptr_t>(view) + 0x1db4, &kind, 4);
    uint32_t failed{};
    uint32_t mode{};
    AcquireSRWLockExclusive(&farLock);
    auto& d = SpidyFarShadowsData;
    InterlockedIncrement64(reinterpret_cast<volatile LONG64*>(&d.sequence));
    d.answeredFit = farAnsweredFit.load(std::memory_order_relaxed);
    d.answeredCache = farAnsweredCache.load(std::memory_order_relaxed);
    if (eye >= 0) {
        auto& e = d.eye[eye];
        ++e.fits;
        e.farFits += counts[1] ? 1 : 0;
        e.base = counts[0];
        e.farCount = counts[1];
        e.kind = kind;
        std::memcpy(e.lod, lod, sizeof lod);
        std::memcpy(e.farLod, farLod, sizeof farLod);
        if (farJudging) {
            ++d.judgedFits;
            d.judgedFar += counts[1] ? 1 : 0;
        }
    } else if (game) {
        ++d.gameFits;
        d.gameBase = counts[0];
        d.gameFar = counts[1];
        std::memcpy(d.gameLod, lod, sizeof lod);
        std::memcpy(d.gameFarLod, farLod, sizeof farLod);
        // Update 37.2: the game view counts toward the verdict only while the eyes are known: at the
        // start (menus, loading) the eyes are not set yet and 37.1 judged that UNSEEN and went OFF.
        if (farJudging && l && r) {
            ++farJudgedGame;
            farJudgedGameFar += counts[1] ? 1 : 0;
        }
    } else {
        ++d.otherFits;
    }
    if (farJudging) {
        failed = fs::verdict(d.judgedFits, d.judgedFar, farJudgedGame, farJudgedGameFar);
        if (d.judgedFar)
            farJudging = false; // kept
        if (failed) {
            farJudging = false;
            mode = fs::modeOf(d.packed);
        }
    }
    InterlockedIncrement64(reinterpret_cast<volatile LONG64*>(&d.sequence));
    ReleaseSRWLockExclusive(&farLock);
    if (failed && farShadowsMode.load() == mode)
        farShadowsSet(fs::afterFailure(mode, failed), failed, true);
    return result;
}
// 189daa0, "is this the active view", where the impostor code asks it (the
// return addresses after its five calls there; impostors above).
std::atomic<uint32_t> lodFadeMode{spidy::view_gates::defaultFade}, lodFadeWhy{};
bool lodFadeKnown{}; // 0x2b4f9b8's bytes as known (install)
std::atomic<uint64_t> fadeCalls{}, fadeEyeAsks{};
std::atomic<bool> impostorsOn{true};
std::atomic<uint32_t> lodReachPercent{spidy::view_gates::defaultReach};
void viewGatesNote(uint64_t spidy::view_gates::Data::*field, unsigned add = 1);
bool isActiveView(void* view) {
    // Update 37: LOD FADE. 0x2b4f8e0 advances an instance's LOD cross-fade for the active
    // view only (its call at 0x2b4f9b8); the eyes answer yes there (view_gates.hpp).
    if (view && lodFadeKnown && enabled &&
        reinterpret_cast<uintptr_t>(_ReturnAddress()) - base == spidy::view_gates::fadeReturnRva) {
        namespace vg = spidy::view_gates;
        const int eye = view == eyes[0].load(std::memory_order_relaxed)   ? 0
                        : view == eyes[1].load(std::memory_order_relaxed) ? 1
                                                                          : -1;
        const uint32_t mode = lodFadeMode.load(std::memory_order_relaxed);
        fadeCalls.fetch_add(1, std::memory_order_relaxed);
        if (eye >= 0) {
            fadeEyeAsks.fetch_add(1, std::memory_order_relaxed);
            if (vg::fadeAnswers(mode, eye)) {
                InterlockedIncrement64(reinterpret_cast<volatile LONG64*>(&SpidyViewGatesData.fadeAnswered[eye]));
                return true;
            }
            InterlockedIncrement64(reinterpret_cast<volatile LONG64*>(&SpidyViewGatesData.fadeAsked[eye]));
        }
    }
    // Update 35: FAR SHADOWS. The eye is "active" to the cascade fit (and, in
    // EYES+CACHE, the left eye to the far cache) only.
    if (view && farShadowsReady && enabled) {
        if (const uint32_t mode = farShadowsMode.load(std::memory_order_relaxed)) {
            void* l = eyes[0].load(std::memory_order_relaxed);
            if (view == l || view == eyes[1].load(std::memory_order_relaxed)) {
                const auto caller = reinterpret_cast<uintptr_t>(_ReturnAddress()) - base;
                if (caller == spidy::far_shadows::fitAskReturn) {
                    farAnsweredFit.fetch_add(1, std::memory_order_relaxed);
                    return true;
                }
                if (mode == spidy::far_shadows::eyesCache && view == l &&
                    caller == spidy::far_shadows::cacheAskReturn) {
                    farAnsweredCache.fetch_add(1, std::memory_order_relaxed);
                    return true;
                }
            }
        }
    }
    if (view && view == eyes[0].load(std::memory_order_relaxed) && impostors && enabled &&
        impostorsOn.load(std::memory_order_relaxed) && rested.load(std::memory_order_relaxed)) {
        const auto caller = reinterpret_cast<uintptr_t>(_ReturnAddress()) - base;
        if (caller == 0x2a6af95 || caller == 0x2a6c34b || caller == 0x2a6c35b || caller == 0x2a6c46c ||
            caller == 0x2a6c47c) {
            if (caller == 0x2a6c34b) {
                AcquireSRWLockExclusive(&telemetry);
                InterlockedIncrement64(&SpidyAppearanceData.sequence);
                ++SpidyAppearanceData.impostorFrames;
                InterlockedIncrement64(&SpidyAppearanceData.sequence);
                ReleaseSRWLockExclusive(&telemetry);
            }
            return true;
        }
    }
    return originalIsActiveView(view);
}
// Runs on the main thread in maintain(), after gameplay updated the hero and
// before render jobs read it. Uses DrawOff 191afb0 / DrawOn 191b880, the
// handlers of the game's ActorDrawAction script node (15a3460). `hide`:
// immersive without the player's body on the hero.
void updateHero(bool immersive, bool hide) {
    HeroFrame frame;
    uint32_t state = native_appearance::heroNoInstance, handle{}, flags{};
    const auto record = playerRecord.load();
    const auto instance = record ? pointer(record) : 0;
    bool drawable{};
    if (instance) {
        state = native_appearance::heroBadTransform;
        if (read(instance, frame.transform, sizeof(frame.transform)) && unitTransform(frame.transform)) {
            // Per-eye hiding and re-anchoring need only this transform. The
            // native visibility switch also requires a registered instance.
            frame.instance = instance;
            drawable = registered(instance, handle) && read(instance + 0x5c, &flags, 4);
            state = drawable ? native_appearance::heroValid : native_appearance::heroUnregistered;
        }
    }
    bool hid{}, restored{};
    if (const auto previous = hiddenHero.load(); previous && (previous != frame.instance || !hide)) {
        uint32_t current{}, previousFlags{};
        // Restore only the same live instance this module switched off.
        if (registered(previous, current) && current == hiddenHandle && read(previous + 0x5c, &previousFlags, 4) &&
            (previousFlags & 0x20)) {
            reinterpret_cast<Visibility>(base + 0x2cfba50)(reinterpret_cast<void*>(previous));
            restored = true;
        }
        hiddenHero = 0;
        hiddenHandle = 0;
        if (previous == frame.instance)
            read(previous + 0x5c, &flags, 4);
    }
    if (hide && drawable && !(flags & 0x20)) {
        // A hero already hidden by the game stays owned by the game.
        reinterpret_cast<Visibility>(base + 0x2cfa6a0)(reinterpret_cast<void*>(frame.instance));
        hiddenHero = frame.instance;
        hiddenHandle = handle;
        hid = true;
        read(frame.instance + 0x5c, &flags, 4);
    }
    const auto slot = (heroSlot.load(std::memory_order_relaxed) + 1) % heroFrames.size();
    heroFrames[slot] = immersive ? frame : HeroFrame{};
    heroSlot.store(static_cast<uint32_t>(slot), std::memory_order_release);
    AcquireSRWLockExclusive(&telemetry);
    auto& data = SpidyAppearanceData;
    InterlockedIncrement64(&data.sequence);
    data.heroState = state;
    data.heroHandle = handle;
    data.heroFlags = flags;
    data.playerActor = instance;
    data.nativeHides += hid;
    data.nativeRestores += restored;
    data.nativeHiddenFrames += hide && drawable && (flags & 0x20);
    const auto webs = native_webs::status();
    data.webState = webs.state;
    data.webLive = webs.live;
    data.webCreates = webs.creates;
    data.webReleases = webs.releases;
    data.webFailures = webs.failures;
    data.webUpdates = webs.updates;
    data.webStartLast = webs.startError;
    data.webStartMax = webs.startErrorMax;
    InterlockedIncrement64(&data.sequence);
    ReleaseSRWLockExclusive(&telemetry);
}
bool heroPosition(Vec3& position);
bool retiring();
// The newest eye command's head for this frame, not yet moved with the player.
FrameHead commandHead() {
    FrameHead head;
    head.frame = frameIndex;
    AcquireSRWLockShared(&commandLock);
    head.command = command;
    head.controlled = head.command.enabled && GetTickCount64() < commandDeadline;
    ReleaseSRWLockShared(&commandLock);
    if (enabled && head.controlled && head.command.enabled == 1 && !retiring() &&
        native_eyes::headView(head.command, head.pose, head.lens)) {
        head.viewed = true;
        Quat relative{};
        head.followed = native_hud::followed(head.command.serial, relative);
        head.panel = native_hud::turned(head.pose, relative);
    }
    return head;
}
// Moves a head with the player to the frame that draws him at `hero`: the
// same render-frame correction placeEyes() gives the eyes.
void moveHead(FrameHead& head, Vec3 hero) {
    native_eyes::reanchorOffset(head.command.anchor, hero, head.offset);
    for (Mat4* m : {&head.pose, &head.panel}) {
        (*m)[12] += head.offset.x;
        (*m)[13] += head.offset.y;
        (*m)[14] += head.offset.z;
    }
}
// This frame's head (FrameHead), latched at its first use in the frame: after
// the frame's gameplay has moved the player.
const FrameHead& latchHead() {
    if (frameHead.frame == frameIndex)
        return frameHead;
    // This frame's rope-update sample of the hero, if one ran since the last
    // frame (taken once a frame, immersive or not, as FrameHero requires).
    uint64_t ropeSamples{};
    Vec3 ropeHero{}, rendered{};
    native_webs::heroSample(ropeSamples, ropeHero);
    const bool shared = activeHero.fresh(ropeSamples, ropeHero);
    frameHead = commandHead();
    // The player is where that sample has him, else at the hero's render transform.
    if (frameHead.viewed && frameHead.command.anchored && (shared || heroPosition(rendered))) {
        activeHeroPosition = shared ? ropeHero : rendered;
        activeHeroPlaced = true;
        moveHead(frameHead, activeHeroPosition);
        heroTravel.placed(frameIndex, activeHeroPosition);
    }
    if (frameHead.viewed && earlyFrameHead.frame == frameIndex && earlyFrameHead.viewed) {
        // This frame's markers went to the head earlyHead() foresaw.
        const auto& early = earlyFrameHead.pose;
        const auto& pose = frameHead.pose;
        const float apart = length(Vec3{early[12] - pose[12], early[13] - pose[13], early[14] - pose[14]});
        native_hud::publish([&](native_hud::Data& d) { d.markerMismatch = std::max(d.markerMismatch, apart); });
    }
    return frameHead;
}
// This frame's head for a use that may come before the frame's gameplay has
// moved the player (the HUD's marker projections): the frame's own once it is
// latched; until then the newest command's, moved to where the frame will draw
// him, or where he is now when the last frame did not place one.
const FrameHead& earlyHead() {
    if (frameHead.frame == frameIndex)
        return frameHead;
    if (earlyFrameHead.frame == frameIndex)
        return earlyFrameHead;
    earlyFrameHead = commandHead();
    Vec3 hero{};
    if (earlyFrameHead.viewed && earlyFrameHead.command.anchored &&
        (heroTravel.ahead(frameIndex, hero) || heroPosition(hero)))
        moveHead(earlyFrameHead, hero);
    return earlyFrameHead;
}
// While immersive, the HUD panel goes in front of this frame's head, facing
// the way the XR worker's Follow turned it.
void placeHud(const HudPlacement& placed, const FrameHead& head) {
    native_hud::Shape shape;
    shape.halfWidth = native_hud::halfWidth(native_hud::size());
    Mat4 transform{};
    uint32_t handle{};
    const bool done = shape.halfWidth > 0 && registered(placed.instance, handle) &&
                      native_hud::panel(head.panel, placed.scale, placed.distance, stockHalfWidth, shape, transform);
    if (done)
        reinterpret_cast<SetTransform>(base + 0x2cfc580)(reinterpret_cast<void*>(placed.instance),
                                                         transform.data(),
                                                         reinterpret_cast<void*>(placed.instance + 0x70));
    const float turn = std::acos(std::clamp(head.pose[8] * head.panel[8] + head.pose[9] * head.panel[9] +
                                                head.pose[10] * head.panel[10],
                                            -1.f, 1.f)) *
                       180 / 3.14159265f;
    native_hud::publish([&](native_hud::Data& d) {
        if (!shape.halfWidth)
            return;
        ++(done ? d.placedFrames : d.rejectedFrames);
        if (done) {
            d.distance = shape.distance;
            d.halfWidth = shape.halfWidth;
            d.followedFrames += head.followed;
            d.followAngle = turn;
            d.followAngleMax = std::max(d.followAngleMax, turn);
        }
    });
}
// The frame whose HUD panel went onto the game screen in stereo (main thread).
uint64_t screenHudFrame = UINT64_MAX;
// On the game screen in stereo the panel goes where both eyes see it in one
// place, at the screen's own distance: toward the game's camera from where the
// game put it, until its face is at the distance the two eyes' images agree at
// (native_eyes::stereoEyes), and smaller by as much, so the camera sees it as
// before. The HUD, the subtitles and the prompts then lie on the screen.
// The panel's face is not at the instance's position: the model's bounding
// sphere (+40: its centre in the model's own axes, then its radius) lies 0.405
// of the model's units behind it, 4.46 m at the game's scale of 11 and 20 m
// (October 11: placed by its position, the HUD showed 9 to 10 of the horizon's
// 51 pixels behind the screen).
void placeScreenHud(const HudPlacement& placed, const native_eyes::Command& cmd) {
    Descriptor main{};
    float m[16]{}, sphere[4]{};
    uint32_t handle{}, flags{};
    // Not a panel the game has switched off (+5c bit 0x20: DrawOff).
    bool done = read(pointer(base + 0xc2ca220), &main, sizeof(main)) && native_view::valid(main) &&
                registered(placed.instance, handle) && read(placed.instance, m, sizeof(m)) &&
                read(placed.instance + 0x5c, &flags, 4) && !(flags & 0x20);
    const auto pose = main.pose();
    const auto eyePair = native_eyes::stereoEyes(
        native_eyes::stereo(cmd),
        {main.values[0x400 / 4], main.values[0x404 / 4], main.values[0x408 / 4], main.values[0x40c / 4]});
    const Vec3 camera{pose[12], pose[13], pose[14]}, forward{pose[8], pose[9], pose[10]};
    const Vec3 panel{m[12], m[13], m[14]};
    Vec3 face = panel;
    if (read(placed.instance + 0x40, sphere, sizeof(sphere)) && std::isfinite(sphere[0] + sphere[1] + sphere[2]) &&
        std::abs(sphere[0]) < 100 && std::abs(sphere[1]) < 100 && std::abs(sphere[2]) < 100)
        for (int row = 0; row < 3; ++row)
            face += Vec3{m[row * 4], m[row * 4 + 1], m[row * 4 + 2]} * sphere[row];
    const float depth = dot(face - camera, forward), scale = eyePair.distance / depth;
    done = done && depth > .1f && std::isfinite(scale) && scale > 1e-4f && scale < 1e4f;
    if (done) {
        for (int i = 0; i < 12; ++i)
            m[i] *= scale;
        const Vec3 moved = camera + (panel - camera) * scale;
        m[12] = moved.x;
        m[13] = moved.y;
        m[14] = moved.z;
        reinterpret_cast<SetTransform>(base + 0x2cfc580)(reinterpret_cast<void*>(placed.instance), m,
                                                         reinterpret_cast<void*>(placed.instance + 0x70));
        screenHudFrame = frameIndex;
        native_hud::onScreen();
    }
    native_hud::publish([&](native_hud::Data& d) {
        ++(done ? d.placedFrames : d.rejectedFrames);
        if (done) {
            d.distance = eyePair.distance;
            d.halfWidth = (main.values[0x404 / 4] - main.values[0x400 / 4]) / 2;
        }
    });
}
// After the game placed the HUD panel in front of its camera: while immersive,
// move it in front of this frame's head.
void hudPlace(void* follower) {
    originalHudPlace(follower);
#if !SM2_STEREO_HUD
    // SM2TODO: never hooked in the SM2 port (see SM2_STEREO_HUD).
    hiddenHud = 0;
    return;
#endif
    if (!enabled || !(config.createViews & 4) || lateEyes) {
        hiddenHud = 0;
        return;
    }
    const auto address = reinterpret_cast<uintptr_t>(follower);
    native_hud::follower(address);
    HudPlacement placed;
    float m[16]{};
    if (pointer(address) != base + 0x38929c8 || !(placed.instance = pointer(pointer(address + 8))) ||
        !read(placed.instance, m, sizeof(m)) || !read(address + 0x48, &placed.distance, 4)) {
        hiddenHud = 0;
        return;
    }
    placed.scale = {length(Vec3{m[0], m[1], m[2]}), length(Vec3{m[4], m[5], m[6]}), length(Vec3{m[8], m[9], m[10]})};
    native_hud::publish([&](native_hud::Data& d) {
        ++d.gameFrames;
        d.follower = address;
        d.instance = placed.instance;
        d.gameScale[0] = placed.scale.x;
        d.gameScale[1] = placed.scale.y;
        d.gameScale[2] = placed.scale.z;
        d.gameDistance = placed.distance;
        d.viewHalfWidth = stockHalfWidth;
    });
    const auto& head = latchHead();
    // The game screen in stereo shows the HUD whatever its setting, as the
    // game's own view does on the flat one.
    const bool screen = head.controlled && head.command.enabled == 3 && !retiring();
    // The HUD off: the eyes (immersive or the flat screen) leave the panel out.
    const bool hide = !native_hud::size() && head.controlled && !screen && !retiring();
    hiddenHud = hide ? placed.instance : 0;
    if (hide)
        native_hud::publish([](native_hud::Data& d) { ++d.hiddenFrames; });
    if (head.viewed)
        placeHud(placed, head);
    else if (screen)
        placeScreenHud(placed, head.command);
}
// `lag` is the hero's render-transform distance from the shared rope-update
// sample the eyes used, or negative when they used the render transform.
// `activeLag` is the distance from the hero position the active view used
// this frame, or negative when that view was not placed.
void recordOffset(const RenderOffset& placed, bool anchored, bool rejected, float lag, float activeLag) {
    AcquireSRWLockExclusive(&offsetLock);
    offsets[placed.generation % offsets.size()] = placed;
    ReleaseSRWLockExclusive(&offsetLock);
    if (!anchored && !rejected)
        return;
    const float distance = length(placed.offset);
    AcquireSRWLockExclusive(&telemetry);
    auto& data = SpidyAppearanceData;
    InterlockedIncrement64(&data.sequence);
    if (anchored) {
        ++data.anchoredFrames;
        data.anchorLast = distance;
        data.anchorMax = std::max(data.anchorMax, distance);
        data.anchorSum += distance;
        if (lag >= 0) {
            ++data.sharedHeroFrames;
            data.heroLagLast = lag;
            data.heroLagMax = std::max(data.heroLagMax, lag);
            data.heroLagSum += lag;
        }
        if (activeLag > .001f) {
            ++data.activeLagFrames;
            data.activeLagLast = activeLag;
            data.activeLagMax = std::max(data.activeLagMax, activeLag);
        }
    }
    data.anchorRejected += rejected;
    InterlockedIncrement64(&data.sequence);
    ReleaseSRWLockExclusive(&telemetry);
}
void setupDisplay(void* view, void* settings, void* profile, uint64_t flags, uint64_t fifth) {
    originalSetupDisplay(view, settings, profile, flags, fifth);
    if (!enabled || !profile || !(config.createViews & 4))
        return;
    const auto address = reinterpret_cast<uintptr_t>(view);
    const auto owner = pointer(address + 0x1dc8);
    int32_t viewIndex{};
    if (!read(address + 0xf94, &viewIndex, sizeof(viewIndex)) || viewIndex >= 0)
        return; // The game already queues this command for primary views.
    for (unsigned eye = 0; eye < 2; ++eye)
        if (auto ownedEye = eyes[eye].load();
            ownedEye && (view == ownedEye || owner == reinterpret_cast<uintptr_t>(ownedEye))) {
            // 19203a2 omits PrepareSrgbOverlay (0x88) for negative view IDs.
            // Its native handler is 18490d0. Restore the same setup bucket
            // immediately before GUI geometry (+15c0), after tone mapping.
            // This retains the HUD and avoids applying scene HDR gain to it.
            using Enqueue = void (*)(void*, uint32_t, uint64_t, void*);
            // SM2: the command is 0xaf on the list at +1850 (SM2VR).
            reinterpret_cast<Enqueue>(base + 0x2c77810)(reinterpret_cast<void*>(address + 0x1850), 0xaf, 0,
                                                        nullptr);
            appearanceEvent(eye, false);
            // 19203c2 queues TemporalAa (0x8a) unless the profile's +68c is set.
            uint8_t left{};
            // SM2: the profile flag is bit 0 of +cc8; TemporalAa is 0xb1 on +18d0 (SM2VR).
            if ((eyeTemporal.load() & 4) && read(reinterpret_cast<uintptr_t>(profile) + 0xcc8, &left, 1) &&
                (left & 1))
                reinterpret_cast<Enqueue>(base + 0x2c77810)(reinterpret_cast<void*>(address + 0x18d0), 0xb1, 1,
                                                            nullptr);
            break;
        }
}
// The size an eye's scene is rendered at: the eye's own, or eyeRenderPercent of
// it per side, in multiples of 8 as the engine's own scaled sizes are.
std::array<uint32_t, 2> eyeRenderSize() {
    // Update 34: always the eye's own size; ENGINE TAA does not upsample (a size that
    // followed a live switch would change under buffers already made).
    return spidy::eyeRenderSize(config.width, config.height, 100);
}
void sizeBuffers(void* buffers, const char* name, uint32_t width, uint32_t height, uint32_t kind, uint8_t align) {
    originalSizeBuffers(buffers, name, width, height, kind, align);
    // As SM2VR 0.0.27's detour (94c0): the eye's size while its buffers are
    // made (initBuffers), and whenever the game sizes an eye's buffers again
    // later (eye +1e38 is its buffer set), e.g. on entering the world. The port
    // did only the first and left +1b990/+1b994 to the game: the likely cause
    // of a headset image with only its top part drawn, and soft.
    uint32_t eyeWidth = sizingEye[0], eyeHeight = sizingEye[1];
    if (!eyeWidth && enabled)
        for (unsigned i = 0; i < 2; ++i)
            if (const auto eye = eyes[i].load();
                eye && pointer(reinterpret_cast<uintptr_t>(eye) + 0x1e38) == reinterpret_cast<uintptr_t>(buffers)) {
                eyeWidth = config.width;
                eyeHeight = config.height;
            }
    if (!eyeWidth)
        return;
    // SM2 +1b978: render width, height; the anti-aliased image's width,
    // height; those in multiples of 8; the output's width, height (multiples
    // of 8 when aligned, SM2VR +1b990/+1b994); ... flags at +1b9b4, 2 among
    // them where the render and output sizes are equal.
    const auto sizes = reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(buffers) + 0x1b978);
    sizes[2] = eyeWidth;
    sizes[3] = eyeHeight;
    sizes[4] = (eyeWidth + 7) & ~7u;
    sizes[5] = (eyeHeight + 7) & ~7u;
    sizes[6] = align ? sizes[4] : eyeWidth;
    sizes[7] = align ? sizes[5] : eyeHeight;
    if (sizes[0] == eyeWidth && sizes[1] == eyeHeight)
        sizes[15] |= 2;
    else
        sizes[15] &= ~2u;
}
// Update 34: an eye's buffer set made with the TAA kind (4) added to the mask table's
// entries 2..7 (0x5d65408, .rdata; target maker 0x2c6eec0 reads it), then the table
// put back. Only when the table is exactly the exe's; under a lock, so two sets made
// at once cannot leave it widened. Records the game's AA (0xa164900): the TAA depth
// targets are made only for 1 or 5..7.
bool engineTaaBuffers(void* buffers, const void* description, const char* name) {
    namespace et = spidy::engine_taa;
    uint32_t aa = UINT32_MAX;
    if (read(base + et::aaRva, &aa, 4))
        engineTaaGameAa = aa;
    const auto mask = reinterpret_cast<uint32_t*>(base + et::maskRva);
    uint32_t stock[8]{};
    AcquireSRWLockExclusive(&maskLock);
    const bool seen = read(base + et::maskRva, stock, sizeof stock);
    const bool same = seen && et::maskIsStock(stock);
    engineTaaMaskSeen = !seen ? 1u : same ? 2u : 3u;
    DWORD protection{};
    const bool widened = same && VirtualProtect(mask, sizeof stock, PAGE_READWRITE, &protection);
    if (widened)
        for (unsigned i = 2; i < 8; ++i)
            mask[i] = stock[i] | et::taaKind;
    const bool made = originalInitBuffers(buffers, description, name);
    if (widened) {
        for (unsigned i = 2; i < 8; ++i)
            mask[i] = stock[i];
        DWORD unused{};
        VirtualProtect(mask, sizeof stock, protection, &unused);
        ++engineTaaWidened;
    }
    ReleaseSRWLockExclusive(&maskLock);
    char line[160];
    std::snprintf(line, sizeof line, "ENGINE TAA: eye buffers made (%s), table %s, game AA %u\n",
                  made ? "ok" : "failed", widened ? "widened for it" : same ? "not writable" : "differs: left alone",
                  aa);
    OutputDebugStringA(line);
    return made;
}
// Update 34: whether an eye view has the TAA targets TemporalAa takes (0x2c98240: the
// job's +0x18e0 set, +0x12648 / +0x12a18 set). The job copies the view; its +0x18e0
// is read, and +0x1e38 (the set the eye's buffers are made in) as well.
bool engineTaaTargets(void* eye) {
    if (!eye)
        return false;
    for (const uint32_t at : {0x18e0u, 0x1e38u})
        if (const auto set = pointer(reinterpret_cast<uintptr_t>(eye) + at)) {
            uint32_t a{}, b{};
            if ((read(set + 0x12648, &a, 4) && a) || (read(set + 0x12a18, &b, 4) && b))
                return true;
        }
    return false;
}
// Why ENGINE TAA cannot go on now (0: it can).
uint32_t engineTaaBlocked() {
    namespace et = spidy::engine_taa;
    void* l = eyes[0].load();
    void* r = eyes[1].load();
    if (l && r && engineTaaTargets(l) && engineTaaTargets(r))
        return et::none;
    if (!(eyeTemporal.load() & 0x10))
        return et::notPrepared;
    if (engineTaaMaskSeen.load() == 3)
        return et::maskDiffers;
    if (!l || !r)
        return et::noEyes;
    if (!engineTaaWidened.load())
        return et::noTargets;
    if (!et::aaMakesTargets(engineTaaGameAa.load()))
        return et::gameAa;
    return et::noTargets;
}
void engineTaaOff(uint32_t why) {
    if (!engineTaaLive.exchange(false))
        return;
    engineTaaWhy = why;
    ++engineTaaOffs;
    char line[128];
    std::snprintf(line, sizeof line, "ENGINE TAA: off, %s\n",
                  why ? spidy::engine_taa::reasonText(why) : "as asked");
    OutputDebugStringA(line);
}
bool initBuffers(void* buffers, const void* description, const char* name) {
    // SM2: a view's render buffers are no longer inside the view; it points at
    // them from +1e38 (SM2VR).
    const auto ownsBuffers = [buffers](void* eye) {
        return eye && pointer(reinterpret_cast<uintptr_t>(eye) + 0x1e38) == reinterpret_cast<uintptr_t>(buffers);
    };
    if (enabled && reinterpret_cast<uintptr_t>(_ReturnAddress()) == base + 0x2c64ce5 &&
        (creatingEye || ownsBuffers(eyes[0].load()) || ownsBuffers(eyes[1].load()))) {
        // The offscreen constructor clamps both dimensions to the desktop
        // buffers. Its parent then clamps them again in 1874f40. Each eye
        // needs private scene buffers at its own resolution.
        alignas(16) uint8_t local[32]{};
        if (read(reinterpret_cast<uintptr_t>(description), local, sizeof(local))) {
            const auto render = eyeRenderSize();
            std::memcpy(local, &render[0], 4);
            std::memcpy(local + 4, &render[1], 4);
            const uint64_t noParent{};
            std::memcpy(local + 0x18, &noParent, 8);
            // As SM2VR does: the eye's sizes (sizeBuffers). Update 34: with -EngineTaa,
            // the TAA target kind as well (engineTaaBuffers).
            sizingEye[0] = config.width;
            sizingEye[1] = config.height;
            const bool sized = (eyeTemporal.load() & 0x10) ? engineTaaBuffers(buffers, local, name)
                                                           : originalInitBuffers(buffers, local, name);
            sizingEye[0] = sizingEye[1] = 0;
            return sized;
        }
    }
    return originalInitBuffers(buffers, description, name);
}
// The game adapts exposure on the GPU, one view at a time. A view's luminance
// pass (18449f0) averages the view's own image, moves the view's "Adapted Lum"
// buffer (+16f0: the buffer, +16f8 its shader view, +1700 its writable view)
// towards it, and binds the shader view for the view's later draws (t67); the
// tone mapper multiplies the image by that buffer (189c710, t11). Each eye had
// its own, so each eye was exposed for what it alone saw: the eye with more
// sky in its lens came out darker than the other (a quarter to a half with a
// level head, twice with the head on its side), and they swapped when the
// head turned round. A render job works on the frame's copy of its view
// (19223e0), so each eye's copy now carries the main view's shader view: both
// eyes take the exposure of the game's own view, which is on the head and
// covers both lenses while immersive. The eye still adapts its own buffer,
// which nothing reads.
bool shareExposure(uintptr_t job, unsigned eye) {
    uint64_t pool{};
    const auto primary = pointer(base + 0xc2ca220);
    if (ownExposure || !(config.createViews & 4) || !read(base + 0xc2ca228, &pool, 8) || !(pool & 1) ||
        !primary || !pointer(job + 0x1968))
        return false;
    // A resting game view (restGameView) adapts nothing: the left eye keeps
    // its own exposure and the right eye takes the left's.
    auto source = primary;
    if (rested.load()) {
        source = reinterpret_cast<uintptr_t>(eyes[0].load());
        if (!eye || !source)
            return false;
    }
    // As 189c710 picks it: the view's own buffer, else its render buffers'.
    const auto buffers = pointer(source + 0x18e0);
    const auto shared =
        pointer(source + 0x1968) ? pointer(source + 0x1970) : buffers ? pointer(buffers + 0x1b7f8) : 0;
    if (!shared)
        return false;
    std::memcpy(reinterpret_cast<void*>(job + 0x1970), &shared, 8);
    return true;
}
// Update 36: EYE BLOOM (eye_bloom.hpp). The eyes' view settings come from
// 0x2c27ec0, which zeroes the bloom (+0xd8) for any view that is not the active
// one (0x2c29102). COPY gives an eye's settings the game view's value before
// copyJob hands them to the eye's render job; GATE jumps over that test.
bool writeBits(uintptr_t p, uint32_t value) {
    __try {
        if (p < 0x10000)
            return false;
        *reinterpret_cast<volatile uint32_t*>(p) = value;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}
std::atomic<uint32_t> bloomMode{spidy::eye_bloom::defaultMode}, bloomWhy{};
bool bloomGateKnown{}; // the gate's bytes as known (checked at install)
std::atomic<bool> bloomGatePatched{};
SRWLOCK bloomLock = SRWLOCK_INIT;
bool bloomJudging{}; // bloomLock
// Writes the gate's first 2 bytes (patched or stock) as one aligned 16-bit store.
SRWLOCK bloomGateLock = SRWLOCK_INIT;
bool bloomGateWriteLocked(bool patched);
bool bloomGateWrite(bool patched) { // one writer at a time (a press and an auto-revert)
    AcquireSRWLockExclusive(&bloomGateLock);
    const bool done = bloomGateWriteLocked(patched);
    ReleaseSRWLockExclusive(&bloomGateLock);
    return done;
}
// LIGHT CHECKS: the kind-2 clamp's immediate in 0x2c98350 (0x100 -> room), only while maintain() makes an
// eye under `creation`. One byte; the five bytes are checked first (else nothing is written).
bool lightChecksByte(uint8_t from, uint8_t to) {
    namespace eq = spidy::eye_queries;
    uint8_t now[sizeof eq::clampBytes]{};
    if (!read(base + eq::clampRva, now, sizeof now) || now[0] != eq::clampBytes[0] || now[2] != from ||
        now[1] != eq::clampBytes[1] || now[3] != eq::clampBytes[3] || now[4] != eq::clampBytes[4])
        return false;
    const auto at = reinterpret_cast<uint8_t*>(base + eq::clampByteRva);
    DWORD protection{};
    if (!VirtualProtect(at, 1, PAGE_EXECUTE_READWRITE, &protection))
        return false;
    std::atomic_ref<uint8_t>(*at).store(to);
    DWORD unused{};
    VirtualProtect(at, 1, protection, &unused);
    FlushInstructionCache(GetCurrentProcess(), at, 1);
    return true;
}
bool bloomGateWriteLocked(bool patched) {
    namespace eb = spidy::eye_bloom;
    if (!bloomGateKnown || bloomGatePatched.load() == patched)
        return bloomGateKnown;
    const auto at = reinterpret_cast<void*>(base + eb::gateRva);
    if (reinterpret_cast<uintptr_t>(at) & 1)
        return false;
    uint16_t now{};
    const uint16_t stock = static_cast<uint16_t>(eb::gateBytes[0] | eb::gateBytes[1] << 8);
    const uint16_t patch = static_cast<uint16_t>(eb::gatePatch[0] | eb::gatePatch[1] << 8);
    if (!read(base + eb::gateRva, &now, 2) || now != (patched ? stock : patch))
        return false;
    DWORD protection{};
    if (!VirtualProtect(at, 2, PAGE_EXECUTE_READWRITE, &protection))
        return false;
    std::atomic_ref<uint16_t>(*static_cast<uint16_t*>(at)).store(patched ? patch : stock);
    DWORD unused{};
    VirtualProtect(at, 2, protection, &unused);
    FlushInstructionCache(GetCurrentProcess(), at, 2);
    bloomGatePatched = patched;
    return true;
}
// Any thread: the mode in force (reason: why it changed on its own, else none).
void bloomSet(uint32_t mode, uint32_t reason, bool automatic) {
    namespace eb = spidy::eye_bloom;
    if (mode == eb::gate && !bloomGateWrite(true)) {
        reason = bloomGateKnown ? eb::writeFailed : eb::noHook;
        mode = eb::off;
        automatic = true;
    }
    if (mode != eb::gate)
        bloomGateWrite(false);
    const uint32_t old = bloomMode.exchange(mode);
    bloomWhy = reason;
    AcquireSRWLockExclusive(&bloomLock);
    auto& d = SpidyEyeBloomData;
    InterlockedIncrement64(reinterpret_cast<volatile LONG64*>(&d.sequence));
    d.packed = eb::pack(mode, reason);
    d.gateKnown = bloomGateKnown ? 1 : 0;
    d.gatePatched = bloomGatePatched.load() ? 1 : 0;
    if (old != mode)
        ++d.modeChanges;
    if (automatic) {
        ++d.reverts;
        d.lastRevert = reason;
    }
    d.judgedCalls = d.judgedEyes = d.judgedHeadOn = d.judgedOwnOn = 0;
    bloomJudging = mode != eb::off;
    InterlockedIncrement64(reinterpret_cast<volatile LONG64*>(&d.sequence));
    ReleaseSRWLockExclusive(&bloomLock);
    char line[160];
    std::snprintf(line, sizeof line, "EYE BLOOM: %s%s%s\n", eb::modeName(mode), automatic ? " - " : "",
                  automatic ? eb::helpText(eb::pack(mode, reason)) : "");
    OutputDebugStringA(line);
}
// copyJob, before the game's: eye is 0/1 for an eye's job, else -1; settings the
// view settings copyJob gives the job (its r8).
void bloomJob(int eye, void* settings) {
    namespace eb = spidy::eye_bloom;
    const uint32_t mode = bloomMode.load(std::memory_order_relaxed);
    if (!mode)
        return;
    uint32_t head{}, own{}, failed{};
    bool headRead{}, wrote{};
    if (eye >= 0 && settings) {
        const auto active = pointer(base + eb::activeSettingsRva);
        headRead = active && active != reinterpret_cast<uintptr_t>(settings) &&
                   read(active + eb::bloomOffset, &head, 4);
        read(reinterpret_cast<uintptr_t>(settings) + eb::bloomOffset, &own, 4);
        if (mode == eb::copy && headRead) {
            wrote = writeBits(reinterpret_cast<uintptr_t>(settings) + eb::bloomOffset, head);
            if (!wrote)
                failed = eb::writeFailed;
        }
    }
    AcquireSRWLockExclusive(&bloomLock);
    auto& d = SpidyEyeBloomData;
    InterlockedIncrement64(reinterpret_cast<volatile LONG64*>(&d.sequence));
    ++d.calls;
    if (eye >= 0 && settings) {
        ++d.eyeJobs;
        d.headOn += head ? 1 : 0;
        d.eyeOwnOn += own ? 1 : 0;
        d.written += wrote ? 1 : 0;
        d.headValue = head;
        d.eyeOwn[eye] = own;
        d.eyeNow[eye] = wrote ? head : own;
    }
    // Update 37.2: judged only while the eyes are known (as FAR SHADOWS): no start-up UNSEEN revert.
    if (bloomJudging && !failed && eyes[0].load(std::memory_order_relaxed) && eyes[1].load(std::memory_order_relaxed)) {
        ++d.judgedCalls;
        if (eye >= 0 && settings) {
            ++d.judgedEyes;
            d.judgedHeadOn += head ? 1 : 0;
            d.judgedOwnOn += own ? 1 : 0;
        }
        failed = eb::verdict(mode, d.judgedCalls, d.judgedEyes, d.judgedHeadOn, d.judgedOwnOn);
        if (failed || d.judgedEyes >= eb::judgeJobs)
            bloomJudging = false; // judged: kept, or changed below
    }
    InterlockedIncrement64(reinterpret_cast<volatile LONG64*>(&d.sequence));
    ReleaseSRWLockExclusive(&bloomLock);
    if (failed && bloomMode.load() == mode)
        bloomSet(eb::afterFailure(mode, failed), failed, true);
}
// Update 37: BLOOM FX (view_gates.hpp). Zeroes, in an eye's view settings, the fields the
// game's own builder zeroes for some views (lens effects that stay fixed to the lens),
// and every dumpEvery-th eye job records the head's and the eye's bloom block and those
// fields (tools/view_gates.py), whatever the mode: the telemetry the next fix starts from.
std::atomic<uint32_t> bloomFxMode{spidy::view_gates::defaultFx};
SRWLOCK gatesLock = SRWLOCK_INIT;
void viewGatesNote(uint64_t spidy::view_gates::Data::*field, unsigned add) {
    AcquireSRWLockExclusive(&gatesLock);
    auto& d = SpidyViewGatesData;
    InterlockedIncrement64(reinterpret_cast<volatile LONG64*>(&d.sequence));
    d.*field += add;
    InterlockedIncrement64(reinterpret_cast<volatile LONG64*>(&d.sequence));
    ReleaseSRWLockExclusive(&gatesLock);
}
void readGroup(uintptr_t at, uint32_t* out) {
    namespace vg = spidy::view_gates;
    for (unsigned i = 0; i < vg::groupFields; ++i) {
        out[i] = 0;
        read(at + vg::fieldAt(i).offset, &out[i], 4);
    }
}
void gatesJob(int eye, void* settings) {
    namespace vg = spidy::view_gates;
    if (eye < 0 || !settings)
        return;
    const auto at = reinterpret_cast<uintptr_t>(settings);
    static std::atomic<uint64_t> gateJobs{};
    const bool dump = gateJobs.fetch_add(1, std::memory_order_relaxed) % vg::dumpEvery == 0;
    uint32_t head[vg::dumpDwords]{}, own[vg::dumpDwords]{}, headGroup[vg::groupFields]{}, ownGroup[vg::groupFields]{};
    if (dump) {
        const auto active = pointer(base + spidy::eye_bloom::activeSettingsRva);
        if (active && active != at) {
            read(active + vg::dumpFrom, head, sizeof head);
            readGroup(active, headGroup);
        }
        read(at + vg::dumpFrom, own, sizeof own);
        readGroup(at, ownGroup);
    }
    const uint32_t mode = bloomFxMode.load(std::memory_order_relaxed);
    unsigned writes{}, failed{};
    for (unsigned i = 0, n = vg::fieldsFor(mode); i < n; ++i) {
        const auto f = vg::fieldAt(i);
        for (unsigned part = 0; part < f.bytes; part += 4)
            (writeBits(at + f.offset + part, 0) ? writes : failed) += 1;
    }
    AcquireSRWLockExclusive(&gatesLock);
    auto& d = SpidyViewGatesData;
    InterlockedIncrement64(reinterpret_cast<volatile LONG64*>(&d.sequence));
    d.fxJobs += mode ? 1 : 0;
    d.fxWrites += writes;
    d.fxFailed += failed;
    if (dump) {
        ++d.dumps;
        std::memcpy(d.headDump, head, sizeof head);
        std::memcpy(d.eyeDump, own, sizeof own);
        std::memcpy(d.headGroup, headGroup, sizeof headGroup);
        std::memcpy(d.eyeGroup, ownGroup, sizeof ownGroup);
    }
    InterlockedIncrement64(reinterpret_cast<volatile LONG64*>(&d.sequence));
    ReleaseSRWLockExclusive(&gatesLock);
    if (failed && bloomFxMode.load() == mode) { // plan B in the same build: back to OFF, said why
        bloomFxMode = vg::fxOff;
        OutputDebugStringA("BLOOM FX: OFF - a write to an eye's settings failed\n");
    }
}
// Update 37: LOD FADE's auto-revert: no eye ever reached 0x2b4f9bd while it was on.
void lodFadeCheck() {
    namespace vg = spidy::view_gates;
    if (lodFadeMode.load() && fadeEyeAsks.load() == 0 && fadeCalls.load() > vg::fadeUnseenCalls) {
        lodFadeMode = vg::fadeOff;
        lodFadeWhy = vg::fadeUnseen;
        OutputDebugStringA("LOD FADE: OFF - no eye reached the game's fade code\n");
    }    static uint32_t every{};
    if (++every % 32)
        return;
    AcquireSRWLockExclusive(&gatesLock);
    auto& d = SpidyViewGatesData;
    InterlockedIncrement64(reinterpret_cast<volatile LONG64*>(&d.sequence));
    d.bloomFx = bloomFxMode.load();
    d.lodFade = lodFadeMode.load();
    d.fadeWhy = lodFadeWhy.load();
    d.lodReach = lodReachPercent.load();
    d.impostorsOn = impostorsOn.load() ? 1 : 0;
    d.fadeGameCalls = fadeCalls.load();
    InterlockedIncrement64(reinterpret_cast<volatile LONG64*>(&d.sequence));
    ReleaseSRWLockExclusive(&gatesLock);
}
// Update 36: SHARED LOD (shared_lod.hpp). 0x2c9a000 sets a submitted view's
// detail scale (+0x1b24) from its own lens; after it, an eye's multiplier
// (+0x1b28, 1.0 from the view's init, written by nothing else) makes the
// product the game view's.
using ViewScale = uint64_t (*)(void*, uint64_t, uint64_t, uint64_t);
ViewScale originalViewScale{};
void* viewScaleHook{};
std::atomic<uint32_t> sharedLodMode{spidy::shared_lod::defaultMode}, sharedLodWhy{};
std::atomic<uintptr_t> sharedLodWritten[2]{}; // the eye view whose +0x1b28 is ours
SRWLOCK lodLock = SRWLOCK_INIT;
bool lodJudging{}; // lodLock
void sharedLodSet(uint32_t mode, uint32_t reason, bool automatic) {
    namespace sl = spidy::shared_lod;
    const uint32_t old = sharedLodMode.exchange(mode);
    sharedLodWhy = reason;
    AcquireSRWLockExclusive(&lodLock);
    auto& d = SpidySharedLodData;
    InterlockedIncrement64(reinterpret_cast<volatile LONG64*>(&d.sequence));
    d.packed = sl::pack(mode, reason);
    if (old != mode)
        ++d.modeChanges;
    if (automatic) {
        ++d.reverts;
        d.lastRevert = reason;
    }
    d.judgedCalls = d.judgedEyes = d.judgedBad = d.judgedHalved = 0;
    lodJudging = mode != sl::off;
    InterlockedIncrement64(reinterpret_cast<volatile LONG64*>(&d.sequence));
    ReleaseSRWLockExclusive(&lodLock);
    char line[160];
    std::snprintf(line, sizeof line, "SHARED LOD: %s%s%s\n", sl::modeName(mode), automatic ? " - " : "",
                  automatic ? sl::helpText(sl::pack(mode, reason)) : "");
    OutputDebugStringA(line);
}
// Puts an eye's multiplier back to the init's 1.0 (OFF, or the stereo's stop).
void sharedLodRestore(unsigned eye) {
    const auto view = sharedLodWritten[eye].exchange(0);
    // Only the eye view it wrote, while it is still that eye (never a freed one).
    if (view && view == reinterpret_cast<uintptr_t>(eyes[eye].load())) {
        const float one = 1.f;
        uint32_t bits{};
        std::memcpy(&bits, &one, 4);
        writeBits(view + spidy::shared_lod::multiplierOffset, bits);
        InterlockedIncrement64(reinterpret_cast<volatile LONG64*>(&SpidySharedLodData.restored));
    }
}
// 0.0.73 test 6 WINDOW POP FIX: what it wrote to `game`'s +0x1b28 (1 when nothing).
float gameWritten(uintptr_t game) {
    return gameLodWritten.load(std::memory_order_relaxed) == game ? gameLodApplied.load(std::memory_order_relaxed) : 1.f;
}
// Puts the game view's multiplier back to 1.0 (the stereo's stop), only on the view it wrote while still active.
void windowPopRestore() {
    const auto view = gameLodWritten.exchange(0);
    if (view && view == pointer(base + spidy::shared_lod::activeViewRva)) {
        const float one = 1.f;
        uint32_t bits{};
        std::memcpy(&bits, &one, 4);
        writeBits(view + spidy::shared_lod::multiplierOffset, bits);
    }
    gameLodApplied.store(1.f);
}
uint64_t viewScale(void* view, uint64_t a, uint64_t b, uint64_t c) {
    namespace sl = spidy::shared_lod;
    const auto result = originalViewScale(view, a, b, c);
    if (!enabled || !view)
        return result;
    void* l = eyes[0].load(std::memory_order_relaxed);
    void* r = eyes[1].load(std::memory_order_relaxed);
    const int eye = view == l ? 0 : view == r ? 1 : -1;
    // 0.0.73 test 6 WINDOW POP FIX (window_pop.hpp): the game's own view (the active view) gets back the stock
    // camera's detail distance its widened lens took (+0x1b28, CONFIRMED field); 1.0 when OFF or not immersive.
    if (eye < 0) {
        const auto game = pointer(base + spidy::shared_lod::activeViewRva);
        if (game && game == reinterpret_cast<uintptr_t>(view)) {
            const float want = windowPopOn.load(std::memory_order_relaxed) ? gameLodFlat.load(std::memory_order_relaxed) : 1.f;
            if (want != 1.f || gameLodWritten.load(std::memory_order_relaxed) == game) {
                uint32_t bits{};
                std::memcpy(&bits, &want, 4);
                if (writeBits(game + spidy::shared_lod::multiplierOffset, bits)) {
                    gameLodApplied.store(want, std::memory_order_relaxed);
                    gameLodWritten.store(want != 1.f ? game : 0, std::memory_order_relaxed);
                    windowPopWrites.fetch_add(1, std::memory_order_relaxed);
                }
            }
        }
    }
    const uint32_t mode = sharedLodMode.load(std::memory_order_relaxed);
    if (!mode) {
        if (eye >= 0)
            sharedLodRestore(static_cast<unsigned>(eye));
        return result;
    }
    const auto v = reinterpret_cast<uintptr_t>(view);
    float eyeScale = NAN, gameScale = NAN, multiplier = 1.f;
    bool bad{}, halved{}, wrote{};
    uint32_t failed{};
    if (eye >= 0) {
        read(v + sl::scaleOffset, &eyeScale, 4);
        const auto game = pointer(base + sl::activeViewRva);
        float gs = NAN, gm = NAN;
        if (game && game != reinterpret_cast<uintptr_t>(l) && game != reinterpret_cast<uintptr_t>(r) &&
            read(game + sl::scaleOffset, &gs, 4) && read(game + sl::multiplierOffset, &gm, 4))
            gameScale = gs * spidy::window_pop::ownMultiplier(gm, gameWritten(game)); // without WINDOW POP FIX's boost
        bad = !sl::sane(gameScale) || !sl::sane(eyeScale);
        halved = !bad && gameScale < .5f * eyeScale;
        multiplier = bad ? 1.f : sl::multiplier(mode, eyeScale, gameScale);
        // Update 37: LOD REACH, the eyes' detail reach past SHARED LOD's (live lod_reach).
        // Update 38.3 FAR WINDOWS: at least its percent (write 1 above).
        if (const uint32_t reach = std::max(lodReachPercent.load(std::memory_order_relaxed),
                                            std::min(farWindowsPercent.load(std::memory_order_relaxed), spidy::view_gates::maxReach));
            !bad && reach != 100) {
            multiplier = spidy::view_gates::reachMultiplier(multiplier, reach);
            InterlockedIncrement64(reinterpret_cast<volatile LONG64*>(&SpidyViewGatesData.reachWrites));
        }
        uint32_t bits{};
        std::memcpy(&bits, &multiplier, 4);
        wrote = writeBits(v + sl::multiplierOffset, bits);
        if (wrote)
            sharedLodWritten[eye] = v;
        else
            failed = sl::writeFailed;
    }
    AcquireSRWLockExclusive(&lodLock);
    auto& d = SpidySharedLodData;
    InterlockedIncrement64(reinterpret_cast<volatile LONG64*>(&d.sequence));
    ++d.calls;
    if (eye >= 0) {
        ++d.eyeCalls;
        d.gameBadCalls += bad ? 1 : 0;
        d.halvedCalls += halved ? 1 : 0;
        d.written += wrote ? 1 : 0;
        d.gameScale = gameScale;
        d.eyeScale[eye] = eyeScale;
        d.eyeMultiplier[eye] = multiplier;
    }
    // Update 37.2: judged only while the eyes are known (as FAR SHADOWS): no start-up UNSEEN revert.
    if (lodJudging && !failed && l && r) {
        ++d.judgedCalls;
        if (eye >= 0) {
            ++d.judgedEyes;
            d.judgedBad += bad ? 1 : 0;
            d.judgedHalved += halved ? 1 : 0;
        }
        failed = sl::verdict(mode, d.judgedCalls, d.judgedEyes, d.judgedBad, d.judgedHalved);
        if (failed || d.judgedEyes >= sl::judgeCalls)
            lodJudging = false;
    }
    InterlockedIncrement64(reinterpret_cast<volatile LONG64*>(&d.sequence));
    ReleaseSRWLockExclusive(&lodLock);
    if (failed && sharedLodMode.load() == mode) {
        const uint32_t next = sl::afterFailure(mode, failed);
        sharedLodSet(next, failed, true);
        if (!next && eye >= 0)
            sharedLodRestore(static_cast<unsigned>(eye));
    }
    return result;
}
// 0.0.74 test 2b TAA HOLD (taa_hold074.hpp): ON by default, live (SpidyTaaHoldSet from game_xr).
__declspec(dllexport) spidy::taa_hold::Data SpidyTaaHoldData;
std::atomic<bool> taaHoldOn{true};
extern "C" void SpidyTaaHoldSet(uint32_t on) { taaHoldOn = on != 0; }
void taaHoldJob(unsigned i, void* view, uint8_t* settings, uintptr_t job) {
    namespace th = spidy::taa_hold;
    static th::Eye eyeState[2];
    auto& d = SpidyTaaHoldData;
    const bool on = taaHoldOn.load(std::memory_order_relaxed);
    d.on = on ? 1 : 0;
    int32_t frame{}, cutFrame{}, stamp{};
    uint8_t cut{};
    if (!read(job + th::jobCut, &cut, 1) || !read(job + th::jobCutFrame, &cutFrame, 4) ||
        !read(job + th::jobStamp, &stamp, 4))
        return;
    // The frame TemporalAa compares with: the context's +0x790 (CANDIDATE), else the global it is copied to.
    int32_t global{};
    std::memcpy(&frame, settings + th::settingsFrame, 4);
    if (base && read(base + spidy::engine_taa::frameRva, &global, 4) &&
        (!frame || (!th::stuck(frame, cutFrame, stamp) && th::stuck(global, cutFrame, stamp))))
        frame = global;
    float rendered[3]{}, previous[3]{};
    float moved = 1e9f;
    const auto v = reinterpret_cast<uintptr_t>(view);
    if (read(v + 0x30, rendered, sizeof rendered) && read(v + 0x560, previous, sizeof previous)) {
        const float dx = rendered[0] - previous[0], dy = rendered[1] - previous[1], dz = rendered[2] - previous[2];
        moved = std::sqrt(dx * dx + dy * dy + dz * dz);
    }
    ++d.jobs[i];
    if (cut)
        ++d.cuts[i];
    if (th::stuck(frame, cutFrame, stamp))
        ++d.zeroAges[i];
    d.lastFrame[i] = frame;
    d.lastCutAge[i] = frame - cutFrame;
    d.lastStampAge[i] = frame - stamp;
    const auto p = th::plan(eyeState[i], on, cut, frame, cutFrame, stamp, moved);
    if (p.hold) {
        std::memcpy(reinterpret_cast<void*>(job + th::jobCutFrame), &p.cutFrame, 4);
        std::memcpy(reinterpret_cast<void*>(job + th::jobStamp), &p.stamp, 4);
        ++d.held[i];
    }
    ++d.sequence;
}
void copyJob(void* view, void* settings, void* a, void* b, bool c) {
    // Update 36: EYE BLOOM, before the job copies the view settings (a, its r8).
    if (enabled) {
        spidy::mirror_probe::job(reinterpret_cast<uint64_t>(view)); // Update 38.4: counts only
        const int eye = view == eyes[0].load() ? 0 : view == eyes[1].load() ? 1 : -1;
        if (bloomMode.load(std::memory_order_relaxed))
            bloomJob(eye, a);
        gatesJob(eye, a); // Update 37: BLOOM FX and the bloom block dumps
        if (eye == 0) {
            lodFadeCheck();
            // Update 37: ENGINE TAA's auto, once nothing blocks it (checked every 64th left job).
            static uint32_t taaEvery{};
            if (engineTaaAuto.load(std::memory_order_relaxed) && !engineTaaLive.load(std::memory_order_relaxed) &&
                ++taaEvery % 64 == 0 && engineTaaBlocked() == spidy::engine_taa::none &&
                SpidyEngineTaa(reinterpret_cast<void*>(uintptr_t{1})) == 0)
                engineTaaAuto = false; // on: from here it is the player's and the guard's
        }
    }
    originalCopyJob(view, settings, a, b, c);
    if (!enabled)
        return;
    if (SpidyCommandData.tracing) {
        const auto job = pointer(reinterpret_cast<uintptr_t>(settings) + 0x650);
        if (reinterpret_cast<uintptr_t>(view) == pointer(base + 0xc2ca220))
            tracedJobs[0] = job;
        for (unsigned i = 0; i < 2; ++i)
            if (view == eyes[i].load())
                tracedJobs[i + 1] = job;
    }
    for (unsigned i = 0; i < 2; ++i)
        if (view == eyes[i].load()) {
            // The job is settings +0x650 in SM2 (SM2VR 0.0.27 copyJob detour,
            // 0x2d66/0x2dfe: [rdx+0x650]). Remastered's +0x4b0 read 0 here, so
            // no eye job was ever recorded, begun, ended or marked
            // (eye_jobs.left_copies 0, gpu.matched_markers 0).
            const auto job = reinterpret_cast<void*>(pointer(reinterpret_cast<uintptr_t>(settings) + 0x650));
            if (!job)
                return;
            // The job's render context (0x7a0 bytes, copied to 7a2e200 while its
            // commands run): +40c is its anti-aliasing, 1 the engine's temporal
            // one, the only value at which TemporalAa (1842e10) runs. With an
            // upscaler chosen in the game's settings it is 0 for every view,
            // and the upscaler (1849e80) leaves offscreen views out: the eyes
            // then had no anti-aliasing at all.
            // SM2 (Update 34): the settings' +0x570, which the frame's copy (0x2c81890 to
            // 0xc2c64a0) puts where TemporalAa 0x2c2dc60 reads it ([0xc2c6a10] == 1).
            if (engineTaaLive.load()) {
                const uint32_t temporal = 1;
                std::memcpy(static_cast<uint8_t*>(settings) + spidy::engine_taa::jobAa, &temporal, 4);
            }
            // 0.0.74 test 2b TAA HOLD (taa_hold074.hpp): the eye's own TAA history kept when the game's stamps
            // follow the frame (share 1 = no history, the shake). Only this job's copy is written.
            if (engineTaaLive.load())
                taaHoldJob(i, view, static_cast<uint8_t*>(settings), reinterpret_cast<uintptr_t>(job));
            if (shareExposure(reinterpret_cast<uintptr_t>(job), i)) {
                AcquireSRWLockExclusive(&telemetry);
                InterlockedIncrement64(&SpidyAppearanceData.sequence);
                ++SpidyAppearanceData.exposureShared;
                InterlockedIncrement64(&SpidyAppearanceData.sequence);
                ReleaseSRWLockExclusive(&telemetry);
            }
            AcquireSRWLockExclusive(&jobLock);
            auto& d = SpidyStereoFrames;
            InterlockedIncrement64(&d.sequence);
            // A copy the game dropped never ends. A fixed table that kept those
            // filled after about 10,000 frames, and every later eye frame went
            // unmarked until presentation stalled. Old copies are reclaimed.
            d.reclaimed += jobs.record({job, latchedSerial.load(), generation.load(), i});
            float rendered[3]{}, previous[3]{};
            if (read(reinterpret_cast<uintptr_t>(view) + 0x30, rendered, sizeof(rendered)) &&
                read(reinterpret_cast<uintptr_t>(view) + 0x560, previous, sizeof(previous)))
            {
                ++(std::memcmp(rendered, previous, sizeof(rendered)) ? d.historyMoved : d.historyStill)[i];
                // Update 21: the camera the game renders this eye from, and its
                // previous-frame camera, against the poses given SetPose (no new
                // offset: +0x30 and +0x560 as above).
                native_eyes::auditEyeCamera(d, i, eyeAsked[i], rendered, previous);
                if (i == 0) {
                    std::memcpy(leftRendered, rendered, sizeof(rendered));
                    leftRenderedSerial = latchedSerial.load();
                    leftRenderedKnown = true;
                } else if (leftRenderedKnown && leftRenderedSerial == latchedSerial.load())
                    native_eyes::noteSeparation(d, leftRendered, rendered);
            }
            ++(placedFrame == frameIndex ? d.sameFramePoses : d.lateFramePoses)[i];
            uint64_t ropeSamples{};
            Vec3 ropeHero{}, ropeStarts[2]{};
            native_webs::heroSample(ropeSamples, ropeHero);
            if (i == 0 && (native_webs::ropeStarts(ropeSamples, ropeStarts) & 1)) {
                ++d.ropeFrames;
                d.ropeFromEye[0] = ropeStarts[0].x - rendered[0];
                d.ropeFromEye[1] = ropeStarts[0].y - rendered[1];
                d.ropeFromEye[2] = ropeStarts[0].z - rendered[2];
            }
            ++d.jobCopies[i];
            d.copiedSerial[i] = latchedSerial;
            d.copiedJob[i] = reinterpret_cast<uint64_t>(job);
            InterlockedIncrement64(&d.sequence);
            ReleaseSRWLockExclusive(&jobLock);
        }
}
void jobEvent(void* view, bool ended) {
    if (!enabled)
        return;
    AcquireSRWLockExclusive(&jobLock);
    if (const auto found = jobs.find(view)) {
        const auto job = *found;
        auto& d = SpidyStereoFrames;
        InterlockedIncrement64(&d.sequence);
        if (ended) {
            ++d.jobEnds[job.eye];
            d.endedSerial[job.eye] = job.serial;
            jobs.finish(view);
        } else {
            ++d.jobBegins[job.eye];
            d.begunSerial[job.eye] = job.serial;
        }
        InterlockedIncrement64(&d.sequence);
    }
    ReleaseSRWLockExclusive(&jobLock);
}
void markJob(void* view, bool ended) {
    AcquireSRWLockShared(&jobLock);
    if (const auto job = jobs.find(view))
        native_gpu::enterMarker(job->eye, job->serial, job->generation, ended);
    ReleaseSRWLockShared(&jobLock);
}
void beginJob(void* view) {
    jobEvent(view, false);
    markJob(view, false);
    originalBeginJob(view);
    native_gpu::leaveMarker();
}
void endJob(void* view) {
    markJob(view, true);
    originalEndJob(view);
    native_gpu::leaveMarker();
    jobEvent(view, true);
}
uint64_t tone(void* a, void* b, void* settings, void* view, void* e, uint64_t genericView) {
    // The native caller derives this switch from view +0xf94 being negative.
    // Our secondary views use the active scene profile and a display target;
    // keep the native display treatment used by the main camera as well.
    const auto owner = pointer(reinterpret_cast<uintptr_t>(view) + 0x1dc8);
    if (enabled && (config.createViews & 4) && owner &&
        (owner == reinterpret_cast<uintptr_t>(eyes[0].load()) ||
         owner == reinterpret_cast<uintptr_t>(eyes[1].load()))) {
        genericView = 0;
        // SM2VR 0.0.27 (tone detour 9f50): a job's copy of an eye view takes
        // the eye's 0x28 bytes at +440 before tone mapping.
        uint8_t block[0x28];
        if (owner != reinterpret_cast<uintptr_t>(view) && read(owner + 0x440, block, sizeof(block)))
            std::memcpy(static_cast<uint8_t*>(view) + 0x440, block, sizeof(block));
    }
    return originalTone(a, b, settings, view, e, genericView);
}
void update(void* view) {
    for (unsigned i = 0; i < 2; ++i)
        if (view == eyes[i].load())
            ++updates[i];
    originalUpdate(view);
    if (enabled && (view == eyes[0].load() || view == eyes[1].load()) && owned(view)) {
        // +1708 and +170c are the processor's copy of the view's adapted and
        // measured luminance: 18a01c0 reads the "Adapted Lum" buffer back into
        // +1720 and +1724, and 189bd30 (above) moves these two towards them.
        // The tone mapper's constants and the game's own code read them. The
        // eyes take the main view's, as their images do (shareExposure); what
        // each eye measured for itself stays in +1720 and +1724.
        // (With the game's view resting, the left eye's own, for the right eye.)
        auto source = pointer(base + 0xc2ca220);
        if (rested.load())
            source = view == eyes[1].load() ? reinterpret_cast<uintptr_t>(eyes[0].load()) : 0;
        float exposure[2]{};
        if (read(source + 0x1980, exposure, sizeof(exposure)) && std::isfinite(exposure[0]) &&
            std::isfinite(exposure[1]) && exposure[0] > 0 && exposure[1] > 0 && exposure[0] < 1e6f &&
            exposure[1] < 1e6f)
            std::memcpy(static_cast<uint8_t*>(view) + 0x1980, exposure, sizeof(exposure));
    }
}
void copyFinal(void* view) {
    originalCopyFinal(view);
    if (!enabled || config.createViews != 7 || (view != eyes[0].load() && view != eyes[1].load()) ||
        !owned(view))
        return;
    const auto address = reinterpret_cast<uintptr_t>(view);
    uint32_t flags{}, bytes{};
    read(address + 0x1dae4, &flags, 4);
    read(address + 0x1dad8, &bytes, 4);
    const auto texture = pointer(address + 0x1e28), buffer = pointer(address + 0x1dad0);
    if ((flags & 7) || !(flags & 0x10) || !texture || !buffer || bytes != config.width * config.height * 4)
        return;
    // Display mode bypasses the stock offscreen copy/readback function. Schedule
    // its existing readback command after CopyToDisplay in the final bucket.
    // The native frame allocator keeps this payload alive for worker execution.
    struct Payload {
        uint64_t staging, source, pixels;
        uint32_t bytes, padding;
        uint64_t completed, reserved;
    };
    static_assert(sizeof(Payload) == 0x30);
    using Allocate = void* (*)(void*, uint32_t, const char*);
    using Enqueue = void (*)(void*, uint32_t, uint64_t, void*);
    auto payload = static_cast<Payload*>(reinterpret_cast<Allocate>(base + 0x2c6bbd0)(
        reinterpret_cast<void*>(base + 0xc15d780), sizeof(Payload), "Spidy display readback"));
    if (!payload)
        return;
    *payload = {address + 0x1da98, texture + 0x40, buffer, bytes, 0, address + 0x1dadc, 0};
    reinterpret_cast<Enqueue>(base + 0x2c77810)(reinterpret_cast<void*>(address + 0x18d0), 0xda, 0x41c,
                                                payload);
}
// Player position from the hero's render transform, as updateHero() reads it.
bool heroPosition(Vec3& position) {
    const auto record = playerRecord.load();
    const auto instance = record ? pointer(record) : 0;
    float m[16]{};
    if (!instance || !read(instance, m, sizeof(m)) || !unitTransform(m))
        return false;
    position = {m[12], m[13], m[14]};
    return true;
}
// The lens of the game's own view on the head: the head's lens covering both
// eyes, or, while RECORDING FOV has that view render for the window
// (restGameView), the wider one of the window's shape that holds it. The
// frame's first submit (activeView) and its last (placeEyes) both take it, so
// what the view culls for and what it renders agree.
native_eyes::Bounds viewLens(const native_eyes::Bounds& head) {
    const int degrees = recordingFov.load(std::memory_order_relaxed);
    // Build 12 WINDOW LIGHTS: never narrower than the head's lens plus the cull margin.
    const auto wide =
        recording_fov::widened(head, static_cast<float>(cullMarginDegrees.load(std::memory_order_relaxed)));
    return degrees && live.load() == 2 && !native_gpu::screenRequested()
               ? recording_fov::covering(wide, degrees, flatAspect.load())
               : wide;
}
// The engine's active view (ViewContextManager +f8) is its first pool view: the
// stock camera. Offscreen eyes are never active, and the "drawn in any view"
// (18a1890) and "sphere in any view" (18a12a0) queries visit pool views only.
// Shaders get the active view's position from the frame-wide GlobalWorldCBuffer
// (m_ActiveViewCtxPos, +330), which also carries the key-light shadow setup.
// All of that was done for the stock camera behind the hero, not the headset.
// While immersive, move the view to the tracked head with a lens covering both
// eyes. The monitor shows this view; the gameplay camera object is unchanged.
bool activeView(const Descriptor* incoming, Descriptor& out) {
    // Every submit of the frame takes the frame's head (latchHead), which also
    // keeps the frame's rope-update sample.
    const auto& head = latchHead();
    uint64_t mask{};
    Descriptor stock{};
    if (!read(base + 0xc2ca228, &mask, 8) || !(mask & 1) ||
        !read(reinterpret_cast<uintptr_t>(incoming), &stock, sizeof(stock)) || !native_view::valid(stock))
        return false;
    // The flat screen keeps the stock camera's shape whichever view is drawn.
    flatAspect = (stock.values[0x404 / 4] - stock.values[0x400 / 4]) /
                 (stock.values[0x40c / 4] - stock.values[0x408 / 4]);
    // What that camera shows of the HUD panel sizes the panel in VR.
    stockHalfWidth = (stock.values[0x404 / 4] - stock.values[0x400 / 4]) / 2;
    gameLodFlat.store(1.f, std::memory_order_relaxed); // WINDOW POP FIX: none unless the head view is placed below
    if (!(config.createViews & 8) || stopRequested || GetTickCount64() >= deadline || live.load() != 2)
        return false;
    if (!head.controlled || head.command.enabled != 1)
        return false;
    fitRefs(stock.values, head.pose.data()); // Update 32: FitViews' references
    // Update 31: SHADOW CAMERA. FLAT: the game view stays the flat camera (the
    // pre-alignment state); its cascades and frame-wide light setup follow it.
    const uint32_t shadowMode = shadowCameraMode.load();
    flatNearZ = stock.nearZ();
    if (shadowCamera::flatGameView(shadowMode))
        return false;
    float nearZ = near_clip::metres();
    if (shadowCamera::useStockNear(shadowMode)) {
        if (shadowCamera::nearUsable(stock.nearZ()))
            nearZ = stock.nearZ();
        else
            shadowCameraFail(shadowCamera::nearOutOfRange);
    }
    const auto& pose = head.pose;
    const auto lens = viewLens(head.lens);
    bool placed = head.viewed;
    gameLodFlat.store(1.f, std::memory_order_relaxed);
    if (placed) {
        out = stock;
        // Keep the stock camera's far plane and temporal jitter. Its near
        // plane, 10 cm, suits a camera behind the hero; on the head it cut
        // off hands held up before the face (NEAR CLIP).
        reinterpret_cast<Lens>(base + 0x2c75560)(&out, nearZ, stock.farZ(), lens.left,
                                                 lens.right, lens.top, lens.bottom, 0, 0,
                                                 stock.values[0x428 / 4], stock.values[0x42c / 4], false,
                                                 false);
        reinterpret_cast<SetPose>(base + 0x2c76310)(&out, pose.data(), 0);
        placed = native_view::valid(out);
        if (!placed && nearZ != near_clip::metres()) {
            // The engine would not take the head view with the flat near plane:
            // back to OFF and this frame's view with NEAR CLIP's, as before.
            shadowCameraFail(shadowCamera::lensRejected);
            out = stock;
            reinterpret_cast<Lens>(base + 0x2c75560)(&out, near_clip::metres(), stock.farZ(), lens.left,
                                                     lens.right, lens.top, lens.bottom, 0, 0,
                                                     stock.values[0x428 / 4], stock.values[0x42c / 4], false,
                                                     false);
            reinterpret_cast<SetPose>(base + 0x2c76310)(&out, pose.data(), 0);
            placed = native_view::valid(out);
        }
    }
    AcquireSRWLockExclusive(&telemetry);
    auto& data = SpidyAppearanceData;
    InterlockedIncrement64(&data.sequence);
    if (placed) {
        // 0.0.73 test 6 WINDOW POP FIX: the stock camera's detail distance for this widened lens.
        gameLodFlat.store(spidy::window_pop::flatMatch(stockHalfWidth,
                                                       std::abs(stock.values[0x40c / 4] - stock.values[0x408 / 4]) / 2,
                                                       (lens.right - lens.left) / 2, std::abs(lens.bottom - lens.top) / 2),
                          std::memory_order_relaxed);
        ++data.activeAligned;
        data.activeBounds[0] = lens.left;
        data.activeBounds[1] = lens.right;
        data.activeBounds[2] = lens.top;
        data.activeBounds[3] = lens.bottom;
        data.activeShift = length(
            Vec3{pose[12] - stock.values[12], pose[13] - stock.values[13], pose[14] - stock.values[14]});
    } else {
        ++data.activeRejected;
    }
    InterlockedIncrement64(&data.sequence);
    ReleaseSRWLockExclusive(&telemetry);
    return placed;
}
// 1646e10 submits the stock camera's descriptor here (return 1647044). Replacing
// it at the source, not in maintain(), gives every later reader the head view;
// the frame dispatch reads the active view for occlusion and 1722b10 before it
// runs view maintenance.
void submit(void* view, const Descriptor* incoming) {
    if (enabled && reinterpret_cast<uintptr_t>(_ReturnAddress()) == base + 0x298bed1 &&
        reinterpret_cast<uintptr_t>(view) == pointer(base + 0xc2ca220)) {
        Descriptor aligned{};
        const bool replaced = activeView(incoming, aligned);
        activeAligned = replaced;
        // Update 27: feed the ADDRESS FINDER the stock and aligned cameras (read-only).
        Descriptor stock{};
        if (incoming && read(reinterpret_cast<uintptr_t>(incoming), &stock, sizeof stock))
            native_address_finder::noteView(0, stock);
        if (replaced)
            native_address_finder::noteView(1, aligned);
        if (replaced) {
            originalSubmit(view, &aligned);
            return;
        }
    }
    originalSubmit(view, incoming);
}
// Stop, expiry, or a lapsed untimed lease: the eyes leave the render list.
bool retiring() {
    return stopRequested || GetTickCount64() >= deadline ||
           (!config.durationMs && GetTickCount64() >= commandDeadline);
}
// This frame's lens jitter for both eyes, as the game's camera gives its own
// view (descriptor +428, +42c): a Halton (2, 3) point within the pixel, twice
// the offset over the render size. Temporal anti-aliasing gathers the samples;
// a scene rendered smaller than the eye needs more of them per eye pixel.
std::array<float, 2> eyeJitter() {
    const bool engine = engineTaaLive.load() && !(eyeTemporal.load() & 1);
    if (!(eyeTemporal.load() & 1) && !engine)
        return {0, 0};
    const auto render = eyeRenderSize();
    const float ratio = static_cast<float>(config.width) / render[0];
    const uint32_t phases = std::clamp<uint32_t>(static_cast<uint32_t>(std::ceil(8 * ratio * ratio)), 8, 32);
    const auto halton = [](uint32_t index, uint32_t radix) {
        float value = 0, fraction = 1.f / radix;
        for (; index; index /= radix, fraction /= radix)
            value += fraction * (index % radix);
        return value;
    };
    const uint32_t phase = static_cast<uint32_t>(frameIndex % phases) + 1;
    std::array<float, 2> j{2 * (halton(phase, 2) - .5f) / render[0], 2 * (halton(phase, 3) - .5f) / render[1]};
    if (engine) {
        // Update 34.2: ENGINE TAA's jitter mode (engine_taa::jitterFor): ours, the game
        // camera's, in render pixels, or none.
        // 0.0.73 test 4 IMAGE STYLE SPIDYVR: NONE is OURS (as SpidyVR; NONE skips the game's TAA).
        spidy::engine_taa::jitterFor(spidy::image_style::jitterMode(imageStyle.load(), engineTaaJitterMode.load()),
                                     halton(phase, 2) - .5f, halton(phase, 3) - .5f,
                                     render[0], render[1], gameJitter[0].load(std::memory_order_relaxed),
                                     gameJitter[1].load(std::memory_order_relaxed), engineTaaJitterScale.load(),
                                     j[0], j[1]);
        engineTaaLenses.fetch_add(1, std::memory_order_relaxed);
        for (unsigned i = 0; i < 2; ++i) {
            if (std::abs(j[i]) > engineTaaEyeMax[i].load(std::memory_order_relaxed))
                engineTaaEyeMax[i].store(std::abs(j[i]), std::memory_order_relaxed);
            const float g = std::abs(gameJitter[i].load(std::memory_order_relaxed));
            if (std::isfinite(g) && g > engineTaaGameMax[i].load(std::memory_order_relaxed))
                engineTaaGameMax[i].store(g, std::memory_order_relaxed);
        }
    }
    eyeJitterSent[0].store(j[0], std::memory_order_relaxed);
    eyeJitterSent[1].store(j[1], std::memory_order_relaxed);
    return j;
}
// While immersive the game renders three scene views: the eyes and its own,
// which Spidy moved to the head (activeView) for the work the game does for
// its active view alone. That work needs the view's camera, not its image:
// the image only went to the monitor, and cost as much as an eye (5.1 of a
// 19.6 ms frame on the GPU, and three of every ten render commands). 19206a0
// (SubmitScene) leaves at once for a view whose +1208 is clear, as the engine
// itself has it for a view that is not ready; the frame then has no job for
// the view, and everything queued on the view itself still runs (the HUD's
// Scaleform command, effects' readback, streaming video). The view's camera
// stays where activeView put it. The game still presents its window every
// frame, so the GPU bridge draws the left eye there (native_gpu::mirror).
// `rested` (above) is the view whose +1208 this module cleared.
bool writeByte(uintptr_t p, uint8_t value) {
    __try {
        *reinterpret_cast<volatile uint8_t*>(p) = value;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}
bool writeFloat(uintptr_t p, float value) {
    __try {
        *reinterpret_cast<volatile float*>(p) = value;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}
// Clears a view's list of drawn instances, as its scene submit does (1920e54).
bool clearDrawn(uintptr_t view) {
#if !SM2_STEREO_RESTED_MARKS
    (void)view; // SM2TODO: drawn list offsets unknown for SM2 (SM2VR removed this).
    return false;
#else
    __try {
        const auto drawn = *reinterpret_cast<uint8_t* volatile*>(view + 0x1b80);
        const auto count = *reinterpret_cast<volatile uint32_t*>(view + 0x1b88);
        if (!drawn || !count || count >= (1u << 26))
            return false;
        std::memset(drawn, 0, count);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
#endif
}
// An eye's level-of-detail multiplier for the lens it was just given
// (distances); the engine multiplies it into the eye's scale in the eye's
// scene submit (189e4b0 sets +1cc4 there, 1796d20 reads both).
// Each eye's lens as the game's constructor stored it (+400 left, +404 right,
// +408 top, +40c bottom tangents) beside the tangents asked of it from the
// headset's field of view: the report's eye_lens shows whether the game kept
// an asymmetric lens as given.
void recordLens(unsigned index, const Descriptor& lens, const std::array<float, 4>& asked) {
    AcquireSRWLockExclusive(&telemetry);
    auto& data = SpidyAppearanceData;
    InterlockedIncrement64(&data.sequence);
    for (unsigned i = 0; i < 4; ++i) {
        data.eyeLens[index][i] = lens.values[0x400 / 4 + i];
        data.eyeAsked[index][i] = asked[i];
    }
    data.lensFrames += index == 0;
    InterlockedIncrement64(&data.sequence);
    ReleaseSRWLockExclusive(&telemetry);
}
void matchDistances(void* eye, const Descriptor& lens, unsigned index) {
    if (!distances)
        return;
    const float detail = native_view::eyeDetail(lens);
    if (!writeFloat(reinterpret_cast<uintptr_t>(eye) + 0x1cc8, detail) || index)
        return;
    AcquireSRWLockExclusive(&telemetry);
    auto& data = SpidyAppearanceData;
    InterlockedIncrement64(&data.sequence);
    data.eyeScale = native_view::distanceScale(lens);
    data.eyeDetail = detail;
    ++data.detailFrames;
    InterlockedIncrement64(&data.sequence);
    ReleaseSRWLockExclusive(&telemetry);
}
// Update 28: EYE LOD, a live hologram press (game_xr.cpp, SpidyEyeLod), off
// unless pressed and never kept. matchDistances' Spider-Man 2 form: each eye's
// distance scale (native_view::sm2DistanceScaleOffset, an ADDRESS FINDER
// candidate) is raised toward the 68-degree lens's. Update 28.1: the value is
// read first and judged by range and steadiness (native_view::eyeLodStep), not
// by an exact match. A failed read or write, or a value out of range or never
// steady, turns EYE LOD off for the session (state 2): it gives back what it
// replaced and records its reason once in SpidyEyeLodData (the launcher prints
// it and writes it to the report) and the debugger log; nothing else changes.
std::atomic<uint32_t> eyeLodWanted{}; // 0 off, 1 on (SpidyEyeLod)
std::atomic<uint32_t> eyeLodState{};  // 0 off, 1 on, 2 failed (SpidyEyeLodNow)
std::atomic<uint32_t> eyeLodWhy{};    // native_view::EyeLodReason once failed
struct EyeLodSlot {
    uintptr_t view{};
    native_view::EyeLodTrack track;
};
EyeLodSlot eyeLodSlots[2]{}; // render thread only (placeEyes)
template <class F> void eyeLodPublish(F&& change) {
    auto& data = SpidyEyeLodData;
    InterlockedIncrement64(&data.sequence);
    change(data);
    InterlockedIncrement64(&data.sequence);
}
void eyeLodGiveBack(EyeLodSlot& slot) {
    float now{};
    if (slot.view && slot.track.ours > 0 && read(slot.view + native_view::sm2DistanceScaleOffset, &now, 4) &&
        now == slot.track.ours)
        writeFloat(slot.view + native_view::sm2DistanceScaleOffset, slot.track.base);
    slot = {};
}
void eyeLodFail(native_view::EyeLodReason why, unsigned index, float value) {
    for (auto& slot : eyeLodSlots)
        eyeLodGiveBack(slot);
    eyeLodWanted = 0;
    eyeLodWhy = static_cast<uint32_t>(why);
    eyeLodState = 2;
    eyeLodPublish([&](native_view::EyeLodData& d) {
        d.state = 2;
        d.reason = static_cast<uint32_t>(why);
        d.eye = index;
        d.lastRead = value;
    });
    char line[160];
    std::snprintf(line, sizeof line, "EYE LOD: off for this session: %s (eye %u, +1b24 %g)\n",
                  native_view::eyeLodReasonText(static_cast<uint32_t>(why)), index, value);
    OutputDebugStringA(line);
}
void eyeLod(void* eye, const Descriptor& lens, const Descriptor& game, unsigned index) {
    if (index > 1 || eyeLodState.load() == 2)
        return;
    auto& slot = eyeLodSlots[index];
    const auto view = reinterpret_cast<uintptr_t>(eye);
    // Update 36: SHARED LOD sets the eyes' detail scale itself while it is on.
    if (!eyeLodWanted.load() || distances || sharedLodMode.load()) { // off, or the Remastered path already scales
        if (slot.view)
            eyeLodGiveBack(slot);
        if (eyeLodState.exchange(0) == 1)
            eyeLodPublish([](native_view::EyeLodData& d) { d.state = 0; });
        return;
    }
    if (eyeLodState.exchange(1) != 1)
        eyeLodPublish([](native_view::EyeLodData& d) { d.state = 1; });
    if (slot.view != view) { // a new eye view (a load): forget the old one, never touch it
        slot = {};
        slot.view = view;
    }
    float current{};
    if (!read(view + native_view::sm2DistanceScaleOffset, &current, 4))
        return eyeLodFail(native_view::EyeLodReason::readFailed, index, 0);
    const float flat = native_view::distanceScale(game);
    const auto step = native_view::eyeLodStep(slot.track, current, lens, flat);
    using K = native_view::EyeLodStep;
    if (step.kind == K::outOfRange)
        return eyeLodFail(native_view::EyeLodReason::outOfRange, index, current);
    if (step.kind == K::unsteady)
        return eyeLodFail(native_view::EyeLodReason::unsteady, index, current);
    const bool writing = step.kind == K::write || step.kind == K::held;
    if (writing && current != step.value) {
        if (!writeFloat(view + native_view::sm2DistanceScaleOffset, step.value))
            return eyeLodFail(native_view::EyeLodReason::writeFailed, index, current);
    }
    if (writing)
        slot.track.ours = step.value;
    if (index)
        return;
    eyeLodPublish([&](native_view::EyeLodData& d) {
        ++d.frames;
        d.lastRead = current;
        d.flat = flat;
        d.eyeScale = native_view::distanceScale(lens);
        if (step.kind == K::write)
            ++d.writes;
        else if (step.kind == K::held)
            ++d.held;
        else
            ++d.settling;
        if (writing)
            d.base = slot.track.base, d.target = step.value;
    });
    if (!writing)
        return;
    AcquireSRWLockExclusive(&telemetry);
    auto& data = SpidyAppearanceData;
    InterlockedIncrement64(&data.sequence);
    data.eyeScale = slot.track.base;
    data.eyeDetail = slot.track.base > 0 ? step.value / slot.track.base : 0;
    ++data.detailFrames;
    InterlockedIncrement64(&data.sequence);
    ReleaseSRWLockExclusive(&telemetry);
}
// A view's impostor visibility queries (impostorReach above).
void* impostorQueries(void* view, void* cull) {
    const auto result = originalImpostorQueries(view, cull);
    float reach{};
    if (result && impostors && enabled && impostorsOn.load(std::memory_order_relaxed) &&
        rested.load(std::memory_order_relaxed) &&
        eyeView(reinterpret_cast<uintptr_t>(view)) >= 0 &&
        read(reinterpret_cast<uintptr_t>(result) + 0x88, &reach, 4) && std::isfinite(reach) && reach > 0)
        writeFloat(reinterpret_cast<uintptr_t>(result) + 0x88, reach * impostorReach.load() / 100.f);
    else if (const uint32_t farPercent = farWindowsPercent.load(std::memory_order_relaxed);
             farWindowsCandidateWrite && result && farWindowsReady && enabled && farPercent > 100 && eyeView(reinterpret_cast<uintptr_t>(view)) >= 0) {
        // Update 38.3 FAR WINDOWS (CANDIDATE, byte-checked above).
        farWindowsCalls.fetch_add(1, std::memory_order_relaxed);
        if (read(reinterpret_cast<uintptr_t>(result) + 0x88, &reach, 4) && std::isfinite(reach) && reach > 1.f &&
            reach < 5000.f) {
            const float ours = reach * static_cast<float>(farPercent) / 100.f;
            if (writeFloat(reinterpret_cast<uintptr_t>(result) + 0x88, ours)) {
                farWindowsWrites.fetch_add(1, std::memory_order_relaxed);
                farWindowsGame.store(reach, std::memory_order_relaxed);
                farWindowsOurs.store(ours, std::memory_order_relaxed);
            }
        }
    }
    return result;
}
void restGameView(uintptr_t primary, bool rest) {
    if (const auto was = rested.load(); was && (was != primary || !rest)) {
        // A load replaces the pool's views: only the same view is given back.
        if (was == primary)
            writeByte(was + 0x13f8, 1);
        rested = 0;
    }
    uint8_t renders{};
    if (rest && primary && !rested.load() && read(primary + 0x13f8, &renders, 1) && renders == 1 &&
        writeByte(primary + 0x13f8, 0))
        rested = primary;
    native_gpu::mirror(rested.load() != 0);
}
// Diagnostic: 187e130 queues a render command (rdx) on a view's list (rcx).
using EnqueueCommand = void (*)(void*, uint32_t, uint64_t, void*);
EnqueueCommand originalEnqueue{};
void* enqueueHook{};
void enqueueCommand(void* list, uint32_t commandId, uint64_t order, void* payload) {
    if (SpidyCommandData.tracing && commandId < 256) {
        const auto at = reinterpret_cast<uintptr_t>(list);
        const uintptr_t views[3] = {pointer(base + 0xc2ca220), reinterpret_cast<uintptr_t>(eyes[0].load()),
                                    reinterpret_cast<uintptr_t>(eyes[1].load())};
        unsigned owner = 0;
        for (unsigned i = 0; i < 3 && !owner; ++i) {
            const auto job = tracedJobs[i].load(std::memory_order_relaxed);
            if ((views[i] && at >= views[i] && at < views[i] + 0x1e20) || (job && at >= job && at < job + 0x1e20))
                owner = i + 1;
        }
        InterlockedIncrement64(reinterpret_cast<volatile LONG64*>(&SpidyCommandData.counts[owner][commandId]));
    }
    originalEnqueue(list, commandId, order, payload);
}
// Places both eyes for the frame whose render jobs are about to be set up.
// The frame function 175a060 shifts every view's camera history (18a13c0 ->
// 189ee00: previous = current), runs gameplay, sets up the offscreen views'
// jobs (1920240 -> 186d050 -> 19206a0, which copies each view in 19223e0),
// then the game views', and only then runs view maintenance (187e320 ->
// 18a0bb0). Until October 5 the eyes were placed in maintenance, so a pose was
// rendered one frame later. Each eye then trailed its frame's scene by one
// step of player travel, which drew the game's web lines that far ahead of
// the tracked hands (0.7 m at 32 m/s), and the next frame's history shift made
// each eye's previous camera equal to its current one, so motion vectors and
// temporal anti-aliasing saw no camera motion.
void placeEyes() {
    Descriptor main{};
    const auto primary = pointer(base + 0xc2ca220);
    const bool render = read(primary, &main, sizeof(main)) && native_view::valid(main) && !retiring();
    // This frame's hero rope update, if one ran since the previous frame.
    uint64_t ropeSamples{};
    Vec3 ropeHero{};
    native_webs::heroSample(ropeSamples, ropeHero);
    const bool shared = placedHero.fresh(ropeSamples, ropeHero);
    const bool activePlaced = activeHeroPlaced;
    activeHeroPlaced = false;
    // Update 34: the game camera's own jitter (a candidate for ENGINE TAA's units: logged).
    if (render) {
        gameJitter[0].store(main.values[0x428 / 4], std::memory_order_relaxed);
        gameJitter[1].store(main.values[0x42c / 4], std::memory_order_relaxed);
    }
    native_eyes::Command desired{};
    bool controlled{};
    if (render && frameHead.frame == frameIndex) {
        // The active view and the HUD went to this command's head (latchHead).
        desired = frameHead.command;
        controlled = frameHead.controlled;
    } else if (render) {
        AcquireSRWLockShared(&commandLock);
        desired = command;
        controlled = desired.enabled && GetTickCount64() < commandDeadline;
        ReleaseSRWLockShared(&commandLock);
    }
    latchedImmersive = render && enabled && controlled && desired.enabled == 1;
    // The game's view is on the head and both eyes render: it need not, unless
    // RECORDING FOV wants it in the window. 0.0.73 STREAMER MODE: the window shows the
    // headset's image instead, so RECORDING FOV gives way to it (no third view to pay for).
    restGameView(primary, latchedImmersive && (config.createViews & 32) && live.load() == 2 &&
                              !native_gpu::screenRequested() &&
                              !(recordingFov.load(std::memory_order_relaxed) && !native_gpu::streamerOn()));
    // The eyes' jobs of this frame mark into the resting view's list (see rested).
    if (const auto resting = rested.load(); resting && clearDrawn(resting))
        InterlockedIncrement64(&SpidyDrawnFrames);
    // The HUD's second movie goes on its panel while the eyes are shown.
    // On the game screen in stereo only in a frame whose panel went onto
    // that screen (placeScreenHud, earlier in the frame): without the panel
    // (a pause menu: the game places none) the movie stays in the game's own
    // frame, which the headset then shows.
    if (render && enabled && controlled && (config.createViews & 4) &&
        (desired.enabled != 3 || screenHudFrame == frameIndex))
        native_hud::eyesShown(desired.enabled == 3);
    latchedHide = latchedImmersive && !native_body::drawn();
    if ((config.createViews & 4) && (latchedImmersive || hiddenHero.load()))
        updateHero(latchedImmersive, latchedHide);
    if (!render)
        return;
    latchedSerial = controlled ? desired.serial : 0;
    localActor = playerRecord.load() ? pointer(playerRecord.load()) : 0;
    if (!activeAligned) // otherwise main carries the head lens; submit() kept the stock shape
        flatAspect = (main.values[0x404 / 4] - main.values[0x400 / 4]) /
                     (main.values[0x40c / 4] - main.values[0x408 / 4]);
    RenderOffset placed{};
    placed.generation = ++generation;
    placedFrame = frameIndex;
    AcquireSRWLockExclusive(&jobLock);
    InterlockedIncrement64(&SpidyStereoFrames.sequence);
    SpidyStereoFrames.latched = latchedSerial;
    InterlockedIncrement64(&SpidyStereoFrames.sequence);
    ReleaseSRWLockExclusive(&jobLock);
    // Tracking sampled the player before this frame's gameplay moved it. Move
    // both eyes with the player to the frame being rendered, so the camera,
    // world, and body come from one simulation state. Use the position the
    // game's web lines started from this frame; they then leave the wrists.
    bool anchored{}, rejected{};
    float lag = -1, activeLag = -1;
    if (latchedImmersive && desired.anchored) {
        const auto& hero = heroFrames[heroSlot.load(std::memory_order_acquire) % heroFrames.size()];
        const Vec3 transform{hero.transform[12], hero.transform[13], hero.transform[14]};
        const Vec3 rendered = shared ? ropeHero : transform;
        anchored = hero.instance && native_eyes::reanchorOffset(desired.anchor, rendered, placed.offset);
        rejected = !anchored;
        if (shared) {
            lag = length(transform - ropeHero);
            placed.ropes = native_webs::ropeStarts(ropeSamples, placed.ropeStarts);
        }
        if (activePlaced)
            activeLag = length(activeHeroPosition - rendered);
    }
    recordOffset(placed, anchored, rejected, lag, activeLag);
    if (latchedImmersive && frameHead.frame == frameIndex && frameHead.viewed) {
        // The HUD and the eyes should have moved with the player alike.
        const float mismatch = length(placed.offset - frameHead.offset);
        native_hud::publish([&](native_hud::Data& d) { d.offsetMismatch = std::max(d.offsetMismatch, mismatch); });
    }
    // The early submit sets up culling and lighting. Gameplay can still move
    // the hero after it: use the eyes' final render-frame placement for the
    // monitor too, before either view's render job is copied. Otherwise the
    // desktop can look into the back while the eyes are already at the head.
    // Do not replace the deliberately selected stock monitor or flat screen.
    shadowLensSame = controlled && desired.enabled >= 2;
    if (latchedImmersive) {
        Mat4 pose{};
        native_eyes::Bounds lens{};
        if (native_eyes::headView(desired, pose, lens)) {
            pose[12] += placed.offset.x;
            pose[13] += placed.offset.y;
            pose[14] += placed.offset.z;
            const auto onHead = [&](const native_eyes::Bounds& b, float nearZ) {
                auto view = main;
                reinterpret_cast<Lens>(base + 0x2c75560)(
                    &view, nearZ, main.farZ(), b.left, b.right, b.top, b.bottom, 0, 0,
                    main.values[0x428 / 4], main.values[0x42c / 4], false, false);
                reinterpret_cast<SetPose>(base + 0x2c76310)(&view, pose.data(), 0);
                return view;
            };
            const auto aligned = onHead(lens, near_clip::metres());
            if (native_view::valid(aligned)) {
                // Covers both eyes: what the first eye fits its shadow cascades to.
                shadowLens = aligned;
                shadowLensPass = offscreenPass;
                publishMatchLens(aligned, !shadowLensSame);
                // The game's view takes RECORDING FOV's wider lens (viewLens), as
                // at the frame's first submit. The eyes' shadows keep this one:
                // rendering, the view fits cascades of its own to its own lens.
                // Update 31: SHADOW CAMERA. FLAT leaves the game view on the flat
                // camera; STOCK NEAR gives the game view (not the eyes) its near plane.
                const uint32_t shadowMode = shadowCameraMode.load();
                const float gameNear = shadowCamera::useStockNear(shadowMode) &&
                                               shadowCamera::nearUsable(flatNearZ.load())
                                           ? flatNearZ.load()
                                           : near_clip::metres();
                // Build 12 WINDOW LIGHTS: always viewLens (the cull margin), as at the
                // first submit; the eyes' shadow lens stays `aligned`.
                auto viewed = onHead(viewLens(lens), gameNear);
                if (gameNear != near_clip::metres() && !native_view::valid(viewed)) {
                    shadowCameraFail(shadowCamera::lensRejected);
                    viewed = onHead(viewLens(lens), near_clip::metres());
                }
                if (!native_view::valid(viewed))
                    viewed = aligned;
                if (shadowCamera::flatGameView(shadowMode)) {
                    shadowCameraApplied(shadowMode);
                } else if ((config.createViews & 8) && native_view::valid(viewed)) {
                    originalSubmit(reinterpret_cast<void*>(primary), &viewed);
                    activeAligned = true;
                    shadowCameraApplied(shadowMode);
                }
            }
        }
    }
    for (auto& slot : eyes)
        // A load can replace the native view pool between frames. maintain()
        // forgets an eye that left it; until then, never write to one.
        if (auto eye = slot.load(); eye && owned(eye)) {
            auto descriptor = main;
            const unsigned index = eye == eyes[0].load() ? 0 : 1;
            if (controlled && desired.enabled >= 2) {
                // The game screen in stereo: the game's camera set apart
                // sideways, each lens shifted the other way (stereoEyes).
                const bool stereo = desired.enabled == 3;
                // The stock camera's jitter is in its own render size's pixels.
                if (stereo || (eyeTemporal.load() & 1)) {
                    const auto jitter = (eyeTemporal.load() & 1)
                                            ? eyeJitter()
                                            : std::array<float, 2>{main.values[0x428 / 4], main.values[0x42c / 4]};
                    auto pose = main.pose();
                    float shift{};
                    if (stereo) {
                        const auto apart = native_eyes::stereoEyes(
                            native_eyes::stereo(desired), {main.values[0x400 / 4], main.values[0x404 / 4],
                                                           main.values[0x408 / 4], main.values[0x40c / 4]});
                        const float side = index == 0 ? -apart.separation / 2 : apart.separation / 2;
                        for (int i = 0; i < 3; ++i)
                            pose[12 + i] += pose[i] * side;
                        shift = index == 0 ? apart.shift : -apart.shift;
                    }
                    reinterpret_cast<Lens>(base + 0x2c75560)(
                        &descriptor, main.nearZ(), main.farZ(), main.values[0x400 / 4] + shift,
                        main.values[0x404 / 4] + shift, main.values[0x408 / 4], main.values[0x40c / 4], 0, 0,
                        jitter[0], jitter[1], false, false);
                    reinterpret_cast<SetPose>(base + 0x2c76310)(&descriptor, pose.data(), 0);
                }
                reinterpret_cast<Submit>(base + 0x2c95200)(eye, &descriptor);
                matchDistances(eye, descriptor, index);
                eyeLod(eye, descriptor, main, index);
                // Update 29: the ADDRESS FINDER also tries the eyes' cameras (read-only).
                if (index < 2)
                    native_address_finder::noteView(2 + index, descriptor);
                continue;
            }
            auto pose = controlled
                            ? desired.eyes[index].world
                            : native_view::relativePose(main.pose(), {{index == 0 ? -.032f : .032f, 0, 0}, {}});
            pose[12] += placed.offset.x;
            pose[13] += placed.offset.y;
            pose[14] += placed.offset.z;
            // A square diagnostic image needs a square 90-degree frustum.
            // The main camera has an ultrawide desktop projection. Rebuild all
            // derived matrices through the verified native lens constructor.
            const float aspect = static_cast<float>(config.width) / config.height;
            const auto& fov = desired.eyes[index].fov;
            const auto jitter = eyeJitter();
            const float nearZ = controlled && desired.enabled == 1 ? near_clip::metres() : main.nearZ();
            reinterpret_cast<Lens>(base + 0x2c75560)(
                &descriptor, nearZ, main.farZ(), controlled ? std::tan(fov[0]) : -aspect,
                controlled ? std::tan(fov[1]) : aspect, controlled ? -std::tan(fov[3]) : -1,
                controlled ? -std::tan(fov[2]) : 1, 0, 0, jitter[0], jitter[1], false, false);
            if (controlled && desired.enabled == 1)
                recordLens(index, descriptor,
                           {std::tan(fov[0]), std::tan(fov[1]), -std::tan(fov[3]), -std::tan(fov[2])});
            reinterpret_cast<SetPose>(base + 0x2c76310)(&descriptor, pose.data(), 0);
            reinterpret_cast<Submit>(base + 0x2c95200)(eye, &descriptor);
            matchDistances(eye, descriptor, index);
            eyeLod(eye, descriptor, main, index);
            if (index < 2)
                native_address_finder::noteView(2 + index, descriptor);
            // Update 21: the eye camera audit (copyJob, native_eye_frame.hpp auditEyeCamera).
            AcquireSRWLockExclusive(&jobLock);
            native_eyes::noteAsked(eyeAsked[index], frameIndex, {pose[12], pose[13], pose[14]});
            ReleaseSRWLockExclusive(&jobLock);
        }
    if (render && mirrorView.load())
        mirrorPlace(main, controlled ? desired.eyes[0].world : main.pose());
}
// A KeyLightContext's cascades: +d and +f count them, +150 holds one each
// 0x3e0 bytes, with its box's half sizes at +170 and its transform at +198.
unsigned cascades(uintptr_t context, uint8_t counts[2]) {
    return context && read(context + 0xd, &counts[0], 1) && read(context + 0xf, &counts[1], 1)
               ? std::min<unsigned>(counts[0] + counts[1], 15)
               : 0;
}
// Update 32: FitViews. What the view 0x2dd8d20 runs for is (flat camera, head,
// other), who called it, and a sample every 2 s. Reads through read() only; an
// unreadable view is counted as such.
void fitViewSeen(void* view, uintptr_t returnAddress) {
    namespace sc = spidy::shadow_camera;
    Descriptor d{};
    const bool valid = view && read(reinterpret_cast<uintptr_t>(view), &d, sizeof d) && native_view::valid(d);
    const uint32_t caller =
        returnAddress > base && returnAddress - base < 0x10000000 ? static_cast<uint32_t>(returnAddress - base) : 0;
    const uint64_t now = GetTickCount64();
    AcquireSRWLockExclusive(&shadowCameraLock);
    auto& data = SpidyShadowCameraData;
    auto& f = data.fit;
    InterlockedIncrement64(reinterpret_cast<volatile LONG64*>(&data.sequence));
    const uint32_t kind = sc::classifyFit(valid, d.values, fitRefFlatOk ? fitRefFlat : nullptr,
                                          fitRefHeadOk ? fitRefHead : nullptr);
    ++f.byKind[kind];
    bool fits = sc::countDistinct<uint32_t>(f.caller, f.callerCalls, sc::fitSlots, caller);
    fits = sc::countDistinct<uint64_t>(f.view, f.viewCalls, sc::fitSlots, reinterpret_cast<uintptr_t>(view)) && fits;
    f.overflow += fits ? 0 : 1;
    if (!f.samples || now - f.lastSampleMs >= sc::fitSampleMs) {
        auto& s = f.sample[f.samples % sc::fitSamples];
        s = {};
        s.ms = now;
        s.kind = kind;
        s.caller = caller;
        if (valid) {
            sc::compact(d.values, s.view);
            s.nearZ = d.nearZ();
            s.farZ = d.farZ();
        }
        if (fitRefFlatOk)
            sc::compact(fitRefFlat, s.flat);
        if (fitRefHeadOk)
            sc::compact(fitRefHead, s.head);
        ++f.samples;
        f.lastSampleMs = now;
    }
    InterlockedIncrement64(reinterpret_cast<volatile LONG64*>(&data.sequence));
    ReleaseSRWLockExclusive(&shadowCameraLock);
}
void fitCascades(void* context, void* view, void* third, void* fourth, void* fifth) {
    {
        // Update 31: who 0x2dd8d20 runs for (evidence it is the cascade fit).
        auto& d = SpidyShadowCameraData;
        const auto v = reinterpret_cast<uintptr_t>(view);
        if (v && v == pointer(base + 0xc2ca220))
            InterlockedIncrement64(reinterpret_cast<volatile LONG64*>(&d.fitGame));
        else if (view && (view == eyes[0].load() || view == eyes[1].load()))
            InterlockedIncrement64(reinterpret_cast<volatile LONG64*>(&d.fitEye));
        else
            InterlockedIncrement64(reinterpret_cast<volatile LONG64*>(&d.fitOther));
        fitViewSeen(view, reinterpret_cast<uintptr_t>(_ReturnAddress()));
    }
    if (!view || view != shadowFitting) {
        originalFitCascades(context, view, third, fourth, fifth);
        return;
    }
    Descriptor own;
    std::memcpy(&own, view, sizeof(own));
    std::memcpy(view, &shadowLens, sizeof(shadowLens));
    originalFitCascades(context, view, third, fourth, fifth);
    std::memcpy(view, &own, sizeof(own));
    ++SpidyShadowData.covered;
}
// Update 31: the game view's own key-light cascades every frame (counts, box half
// sizes, centres, the head and the near plane), whether or not the eyes made any.
void sampleGameCascades(void* view, void* result) {
    uintptr_t made[11]{};
    uint8_t counts[2]{};
    spidy::shadow_camera::GameCascades g{};
    const bool have = result && read(reinterpret_cast<uintptr_t>(result), made, sizeof(made));
    const unsigned count = have ? cascades(made[5], counts) : 0;
    g.readable = have ? 1 : 0;
    g.counts[0] = counts[0];
    g.counts[1] = counts[1];
    Descriptor d{};
    if (read(reinterpret_cast<uintptr_t>(view), &d, sizeof d) && native_view::valid(d)) {
        g.headPos[0] = d.values[12];
        g.headPos[1] = d.values[13];
        g.headPos[2] = d.values[14];
        g.nearZ = d.nearZ();
    }
    for (unsigned i = 0; i < 4 && i < count; ++i) {
        const auto entry = made[5] + 0x150 + i * 0x3e0;
        float pose[16]{};
        read(entry + 0x170, g.extent[i], sizeof(g.extent[i]));
        if (read(entry + 0x198, pose, sizeof(pose))) {
            g.centre[i][0] = pose[12];
            g.centre[i][1] = pose[13];
            g.centre[i][2] = pose[14];
        }
    }
    AcquireSRWLockExclusive(&shadowCameraLock);
    auto& data = SpidyShadowCameraData;
    InterlockedIncrement64(reinterpret_cast<volatile LONG64*>(&data.sequence));
    g.samples = data.game.samples + 1;
    data.game = g;
    InterlockedIncrement64(reinterpret_cast<volatile LONG64*>(&data.sequence));
    ReleaseSRWLockExclusive(&shadowCameraLock);
}
// Update 25: the game view's own nearest key-light cascade against each eye's
// (read this pass), in metres. Reads only; any unreadable part skips it.
void compareGameShadows(void* result) {
    uintptr_t made[11]{};
    uint8_t counts[2]{};
    float game[16]{};
    if (!result || !read(reinterpret_cast<uintptr_t>(result), made, sizeof(made)) || !cascades(made[5], counts) ||
        !read(made[5] + 0x150 + 0x198, game, sizeof(game)))
        return;
    auto& data = SpidyShadowData;
    bool any{};
    float delta[2]{};
    for (unsigned e = 0; e < 2; ++e) {
        const auto& sample = data.eyes[e];
        if (eyeShadowPass[e] != offscreenPass || !(sample.cascades[0] + sample.cascades[1]))
            continue;
        const float dx = sample.pose[0][12] - game[12], dy = sample.pose[0][13] - game[13],
                    dz = sample.pose[0][14] - game[14];
        delta[e] = std::sqrt(dx * dx + dy * dy + dz * dz);
        any = std::isfinite(delta[e]);
    }
    if (!any)
        return;
    InterlockedIncrement64(&data.sequence);
    ++data.gameCompared;
    for (unsigned e = 0; e < 2; ++e) {
        data.gameDelta[e] = std::isfinite(delta[e]) ? delta[e] : 0.f;
        data.gameDeltaMost[e] = std::max(data.gameDeltaMost[e], data.gameDelta[e]);
    }
    InterlockedIncrement64(&data.sequence);
}
void* submitScene(void* view, void* parent, uint32_t flags, uint64_t last, uint64_t fifth) {
    const int eye = !view ? -1 : view == eyes[0].load() ? 0 : view == eyes[1].load() ? 1 : -1;
    if (eye < 0) {
        const auto result = originalSubmitScene(view, parent, flags, last, fifth);
        // Update 25: the game's own view (the one on the monitor, and the one
        // the frame-wide lighting constants are set up for).
        if (enabled && view && (flags & 2) && reinterpret_cast<uintptr_t>(view) == pointer(base + 0xc2ca220)) {
            sampleGameCascades(view, result); // Update 31
            compareGameShadows(result);
        }
        return result;
    }
    const auto mode = eyeShadows.load();
    const bool second = shadowDonor.pass == offscreenPass && shadowDonor.view != view;
    const bool shares = (mode & 1) && (flags & 2) && shadowDonor.pass != offscreenPass && !shadowLensSame &&
                        shadowLensPass == offscreenPass;
    const bool cover = shares;
    uintptr_t given[11]{};
    const bool take = (mode & 7) && (flags & 2) && second && shadowDonor.part[5] && shadowDonor.covers &&
                      (!parent || read(reinterpret_cast<uintptr_t>(parent), given, sizeof(given)));
    if (take) {
        if (mode & 1)
            given[5] = shadowDonor.part[5];
        if (mode & 2)
            given[3] = shadowDonor.part[3];
        if (mode & 4)
            given[6] = shadowDonor.part[6];
    }
    shadowFitting = cover ? view : nullptr;
    const auto result = originalSubmitScene(view, take ? given : parent, flags, last, fifth);
    shadowFitting = nullptr;
    uintptr_t made[11]{};
    const bool have = result && read(reinterpret_cast<uintptr_t>(result), made, sizeof(made));
    auto& data = SpidyShadowData;
    auto& sample = data.eyes[eye];
    InterlockedIncrement64(&data.sequence);
    eyeShadowPass[eye] = offscreenPass;
    ++sample.submits;
    sample.taken += take;
    sample.flags = flags;
    sample.thread = GetCurrentThreadId();
    sample.order = second;
    read(reinterpret_cast<uintptr_t>(view) + 0x13fa, &sample.mayTake, 1);
    uint16_t lights{}, slot{};
    sample.lights = have && made[3] && read(made[3] + 0x34, &lights, 2) ? lights : 0;
    const auto count = have ? cascades(made[5], sample.cascades) : 0;
    const auto map = count ? pointer(made[5] + 0x150 + 0x3c8) : 0;
    sample.slot = map && read(map + 0x10, &slot, 2) ? slot & 0x7fff : 0;
    for (unsigned i = 0; i < 4 && i < count; ++i) {
        const auto entry = made[5] + 0x150 + i * 0x3e0;
        read(entry + 0x170, sample.extent[i], sizeof(sample.extent[i]));
        read(entry + 0x198, sample.pose[i], sizeof(sample.pose[i]));
    }
    if (have && !take && second && shadowDonor.part[5] && !shadowDonor.covers) {
        // Both eyes made their own: how far apart are their cascades?
        uint8_t counts[2]{};
        const auto both = std::min(count, cascades(shadowDonor.part[5], counts));
        float poseDelta{}, extentDelta{};
        for (unsigned i = 0; i < both; ++i) {
            float mine[19]{}, other[19]{};
            const auto a = made[5] + 0x150 + i * 0x3e0, b = shadowDonor.part[5] + 0x150 + i * 0x3e0;
            if (!read(a + 0x198, mine, 64) || !read(b + 0x198, other, 64) || !read(a + 0x170, mine + 16, 12) ||
                !read(b + 0x170, other + 16, 12))
                continue;
            for (unsigned k = 0; k < 19; ++k)
                (k < 16 ? poseDelta : extentDelta) =
                    std::max(k < 16 ? poseDelta : extentDelta, std::fabs(mine[k] - other[k]));
        }
        ++data.compared;
        data.poseDelta = poseDelta;
        data.extentDelta = extentDelta;
        data.poseDeltaMost = std::max(data.poseDeltaMost, poseDelta);
        data.extentDeltaMost = std::max(data.extentDeltaMost, extentDelta);
    }
    InterlockedIncrement64(&data.sequence);
    if (have && !take && !second && (flags & 2)) {
        shadowDonor.pass = offscreenPass;
        shadowDonor.view = view;
        shadowDonor.covers = shares;
        std::memcpy(shadowDonor.part, made, sizeof(made));
    }
    return result;
}
// 1920240 sets up this frame's render jobs for the offscreen views.
uint8_t renderOffscreen() {
    ++offscreenPass;
    ++SpidyShadowData.passes;
    native_body::renderFrame();
    if (!lateEyes && (enabled || live.load() || hiddenHero.load()))
        placeEyes();
    return originalRenderOffscreen();
}
// Update 34: ENGINE TAA goes off on its own when VR ends or an eye loses its targets
// (the command then returns without them anyway); every 2 s on, the jitter is logged.
// (SpidyEngineTaaNow is declared at file scope, in the extern "C" block above.)
// Update 34.2: ENGINE TAA's record for the launcher's report and console (it only went to
// OutputDebugString in Update 34/34.1: run341's report could not say if it was on).
extern "C" {
__declspec(dllexport) spidy::engine_taa::Data SpidyEngineTaaData;
// 0.0.73 TAA SHAKE: what each eye's view holds where TemporalAa reads it (engine_taa::Eyes).
__declspec(dllexport) spidy::engine_taa::Eyes SpidyEngineTaaEyes;
// 0.0.74 test 2b TAA DIAGNOSTICS (taa_diag074.hpp): every frame's per-eye record while the option is ON.
__declspec(dllexport) spidy::taa_diag::Diag SpidyTaaDiag;
}
std::atomic<bool> taaDiagOn{};
// 0.0.74 test 2b TAA DIAGNOSTICS: game_xr sets it from the option (0 off, 1 on); read() only, never a write to the game.
extern "C" void SpidyTaaDiagnosticsSet(uint32_t on) { taaDiagOn = on != 0; }
// One frame of TAA DIAGNOSTICS: what each eye's view holds for TemporalAa this frame (read() only).
void taaDiagFrame(void* const views[2]) {
    namespace et = spidy::engine_taa;
    namespace td = spidy::taa_diag;
    static int32_t lastStamp[2]{};
    static bool haveLast{};
    auto& d = SpidyTaaDiag;
    d.bytes = sizeof(td::Diag);
    d.on = 1;
    td::Frame f{};
    int32_t frame{};
    const bool haveFrame = base && read(base + et::frameRva, &frame, 4);
    f.frame = static_cast<uint32_t>(frame);
    f.sent[0] = eyeJitterSent[0].load(std::memory_order_relaxed);
    f.sent[1] = eyeJitterSent[1].load(std::memory_order_relaxed);
    uintptr_t sets[2]{};
    int32_t stamps[2]{};
    for (unsigned i = 0; i < 2; ++i) {
        const auto v = reinterpret_cast<uintptr_t>(views[i]);
        if (!v)
            return;
        float stored[2]{};
        if (read(v + et::jobJitter, stored, sizeof stored)) {
            f.stored[2 * i] = stored[0];
            f.stored[2 * i + 1] = stored[1];
        }
        read(v + et::viewSet, &sets[i], sizeof sets[i]);
        uint8_t cut{};
        int32_t cutFrame{}, stamp{};
        if (haveFrame && read(v + et::viewCut, &cut, 1) && read(v + et::viewCutFrame, &cutFrame, 4) &&
            read(v + et::viewStamp, &stamp, 4)) {
            f.cutAge[i] = frame - cutFrame;
            f.stampAge[i] = frame - stamp;
            f.weight[i] = et::currentWeight(cut, f.cutAge[i], f.stampAge[i]);
            stamps[i] = stamp;
        }
    }
    td::add(d, f, sets[0], sets[1], stamps[0], stamps[1], lastStamp, haveLast);
}
// 0.0.73 TAA SHAKE: read back (read() only) what TemporalAa 0x2c2dc60 takes from each eye: the stored
// jitter, the size it takes it back with, the history stamps and the current frame's weight from them.
void engineTaaEyesPublish(void* const views[2]) {
    namespace et = spidy::engine_taa;
    auto& e = SpidyEngineTaaEyes;
    int32_t frame{};
    const bool haveFrame = base && read(base + et::frameRva, &frame, 4);
    for (unsigned i = 0; i < 2; ++i) {
        const auto v = reinterpret_cast<uintptr_t>(views[i]);
        if (!v)
            continue;
        float stored[2]{};
        if (read(v + et::jobJitter, stored, sizeof stored)) {
            e.stored[2 * i] = stored[0];
            e.stored[2 * i + 1] = stored[1];
        }
        uintptr_t set{};
        uint32_t size[2]{};
        if (read(v + et::viewSet, &set, sizeof set) && set && read(set + et::setTaaWidth, size, sizeof size)) {
            e.taaSize[2 * i] = size[0];
            e.taaSize[2 * i + 1] = size[1];
        }
        const auto render = eyeRenderSize();
        // The handler takes the set's size when its width is set, else its own target's (render size here).
        const uint32_t w = size[0] ? size[0] : render[0], h = size[0] ? size[1] : render[1];
        et::takenBack(e.stored[2 * i], e.stored[2 * i + 1], w, h, e.pixels[2 * i], e.pixels[2 * i + 1]);
        uint8_t cut{};
        int32_t cutFrame{}, stamp{};
        if (haveFrame && read(v + et::viewCut, &cut, 1) && read(v + et::viewCutFrame, &cutFrame, 4) &&
            read(v + et::viewStamp, &stamp, 4)) {
            const int32_t cutAge = frame - cutFrame, stampAge = frame - stamp;
            e.cut[i] = cut;
            e.cutAge[i] = static_cast<uint32_t>(cutAge > 0 ? cutAge : 0);
            e.stampAge[i] = static_cast<uint32_t>(stampAge > 0 ? stampAge : 0);
            e.weight[i] = et::currentWeight(cut, cutAge, stampAge);
            if (e.weight[i] >= 1)
                ++e.resetSamples[i];
        }
    }
    const auto render = eyeRenderSize();
    e.render[0] = render[0];
    e.render[1] = render[1];
    e.mode = spidy::image_style::jitterMode(imageStyle.load(), engineTaaJitterMode.load());
    e.scale = engineTaaJitterScale.load();
    ++e.samples;
    ++e.sequence;
}
void engineTaaPublish() {
    auto& d = SpidyEngineTaaData;
    d.packed = SpidyEngineTaaNow();
    // 0.0.73 AA: the jitter in force (GAME TAA SPIDYVR makes NONE OURS); test 5's console said "jitter NONE"
    // while OURS ran (the asked mode).
    d.mode = spidy::image_style::jitterMode(imageStyle.load(), engineTaaJitterMode.load());
    d.scalePercent = engineTaaJitterScale.load();
    d.gameAa = engineTaaGameAa.load();
    d.maskSeen = engineTaaMaskSeen.load();
    d.widened = engineTaaWidened.load();
    d.turnedOn = engineTaaOns.load();
    d.refused = engineTaaRefusals.load();
    d.wentOff = engineTaaOffs.load();
    d.lastRefusal = engineTaaLastRefusal.load();
    d.lastOff = engineTaaWhy.load();
    const auto render = eyeRenderSize();
    d.renderWidth = render[0];
    d.renderHeight = render[1];
    d.jitteredLenses = engineTaaLenses.load();
    d.onFrames = engineTaaFrames.load();
    for (unsigned i = 0; i < 2; ++i) {
        d.eyeJitter[i] = eyeJitterSent[i].load(std::memory_order_relaxed);
        d.gameJitter[i] = gameJitter[i].load(std::memory_order_relaxed);
        d.eyeJitterMax[i] = engineTaaEyeMax[i].load(std::memory_order_relaxed);
        d.gameJitterMax[i] = engineTaaGameMax[i].load(std::memory_order_relaxed);
    }
    d.modeChanges = engineTaaModeChanges.load();
    ++d.sequence;
}
void engineTaaWatch() {
    namespace et = spidy::engine_taa;
    static uint64_t lastPublish{};
    if (const uint64_t t = GetTickCount64(); t - lastPublish >= 250) {
        lastPublish = t;
        void* l = eyes[0].load();
        void* r = eyes[1].load();
        const bool usable = !retired[0] && !retired[1] && !retiring() && !stopRequested;
        SpidyEngineTaaData.targetsLeft = usable && l && engineTaaTargets(l) ? 1 : 0;
        SpidyEngineTaaData.targetsRight = usable && r && engineTaaTargets(r) ? 1 : 0;
        engineTaaPublish();
        if (usable && engineTaaLive.load() && l && r) {
            void* const views[2]{l, r};
            engineTaaEyesPublish(views);
        }
    }
    // 0.0.74 test 2b TAA DIAGNOSTICS: every frame (not every 250 ms) while ON and ENGINE TAA is on.
    if (taaDiagOn.load() && engineTaaLive.load()) {
        void* l = eyes[0].load();
        void* r = eyes[1].load();
        if (l && r && !retired[0] && !retired[1] && !retiring() && !stopRequested) {
            void* const views[2]{l, r};
            taaDiagFrame(views);
        }
    } else {
        SpidyTaaDiag.on = 0;
    }
    if (!engineTaaLive.load())
        return;
    engineTaaFrames.fetch_add(1, std::memory_order_relaxed);
    void* l = eyes[0].load();
    void* r = eyes[1].load();
    if (!l || !r || retired[0] || retired[1] || retiring() || stopRequested)
        return engineTaaOff(et::vrEnded);
    if (!engineTaaTargets(l) || !engineTaaTargets(r))
        return engineTaaOff(et::targetsGone);
    static uint64_t lastLog{};
    if (const uint64_t now = GetTickCount64(); now - lastLog >= 2000) {
        lastLog = now;
        const auto render = eyeRenderSize();
        char line[200];
        std::snprintf(line, sizeof line,
                      "ENGINE TAA: on; eye jitter %.6f %.6f (eye %ux%u), game camera jitter %.6f %.6f\n",
                      static_cast<double>(eyeJitterSent[0].load()), static_cast<double>(eyeJitterSent[1].load()),
                      render[0], render[1], static_cast<double>(gameJitter[0].load()),
                      static_cast<double>(gameJitter[1].load()));
        OutputDebugStringA(line);
    }
}
// Version 0.0.4 MIRROR VIEW TEST. The eyes' confirmed constructor (2c9c590) and arguments with
// its own name and a small portrait size; giveOcclusion as the eyes get it; none of their flags
// (0x480), no readback (2c651b0) and no GPU slot: nothing reads its image, the MIRROR PROBE counts
// what the game draws in its slot. Under the creation lock.
void mirrorCreate(void* manager, const Descriptor& main) {
    namespace mv = spidy::mirror_view;
    if (mirrorView.load())
        return;
    uint32_t freeSlots = 0;
    for (unsigned slot = 0; slot < 8; ++slot)
        freeSlots += pointer(base + 0xc2ca250 + slot * 8) ? 0 : 1;
    AcquireSRWLockExclusive(&mirrorLock);
    auto& stats = mirrorStats;
    stats.wanted = mirrorWanted.load();
    stats.freeSlots = freeSlots;
    const bool create = stats.test.shouldCreate(stats.wanted != 0, true, freeSlots);
    ReleaseSRWLockExclusive(&mirrorLock);
    if (!create)
        return;
    const CreateArgs args{mv::viewName, main.nearZ(), main.farZ(), 70.f, mv::width, mv::height,
                          28, nullptr, 0x100, 2, 0xffffffffu, 0};
    // Not creatingEye: the game sizes its buffers (320x480), not initBuffers at the eyes' size.
    void* view = reinterpret_cast<Create>(base + 0x2c9c590)(manager, &args);
    if (view && (config.createViews & 16))
        giveOcclusion(view);
    // MIRROR TEST (private): the eyes' own 0x480 (0x80 the active post profile, 0x400 the display
    // conversion into the view's own output image) - CONFIRMED for the eyes, CANDIDATE here - so its
    // image (+1e28, read as the eyes' in mirrorFrame) holds the finished picture. No readback.
    if (view && (config.createViews & 4))
        InterlockedOr(reinterpret_cast<volatile LONG*>(reinterpret_cast<uintptr_t>(view) + 0x1daec), 0x480);
    uint32_t slot = ~0u;
    for (unsigned i = 0; view && i < 8; ++i)
        if (pointer(base + 0xc2ca250 + i * 8) == reinterpret_cast<uintptr_t>(view))
            slot = i;
    AcquireSRWLockExclusive(&mirrorLock);
    mirrorStats.test.created(view && slot < 8, slot);
    mirrorStats.texture = view ? static_cast<uint32_t>(pointer(reinterpret_cast<uintptr_t>(view) + 0x1e28) != 0) : 0;
    ReleaseSRWLockExclusive(&mirrorLock);
    if (view && slot >= 8) // made but not in the pool: never touch it again
        return;
    mirrorView = view;
    mirrorRetired = false;
    QueryPerformanceCounter(&mirrorLastTick);
    char line[160];
    std::snprintf(line, sizeof line, "VERSION 0.0.4 MIRROR VIEW TEST: %s (slot %u, %u of 8 free)\n",
                  view ? "view made" : "the game made no view", slot, freeSlots);
    OutputDebugStringA(line);
}
void mirrorRetire() {
    native_gpu::registerMirror(nullptr); // MIRROR TEST: the picture stops first
    if (auto view = mirrorView.load(); view && !mirrorRetired) {
        if (owned(view))
            InterlockedOr(reinterpret_cast<volatile LONG*>(reinterpret_cast<uintptr_t>(view) + 0x1dae4), 1);
        mirrorRetired = true;
        OutputDebugStringA("VERSION 0.0.4 MIRROR VIEW TEST: view retired\n");
    }
}
// Once per frame, after the game's maintain: time the frame, run the test, retire when done.
void mirrorFrame() {
    LARGE_INTEGER now{}, frequency{};
    QueryPerformanceCounter(&now);
    QueryPerformanceFrequency(&frequency);
    const float ms = mirrorLastTick.QuadPart && frequency.QuadPart
                         ? static_cast<float>(static_cast<double>(now.QuadPart - mirrorLastTick.QuadPart) * 1000.0 /
                                              static_cast<double>(frequency.QuadPart))
                         : 0.f;
    mirrorLastTick = now;
    auto view = mirrorView.load();
    const bool viewOwned = view && owned(view);
    bool retire = false, reopen = false, show = false;
    const auto gpu = native_gpu::mirrorGpu();
    const bool wanted = mirrorWanted.load() != 0;
    const bool vrUp = enabled && !stopRequested && !retiring() && eyes[0].load() && eyes[1].load() &&
                      !retired[0] && !retired[1];
    AcquireSRWLockExclusive(&mirrorLock);
    auto& stats = mirrorStats;
    namespace mv = spidy::mirror_view;
    stats.gpuFault = gpu.fault;
    stats.gpuImage = gpu.image;
    stats.gpuCopies = gpu.copies;
    if (stats.test.phase == mv::running && !mirrorRetired) {
        retire = stats.test.runFrame(ms / 1000.f, ms, viewOwned);
        if (stats.test.phase != mv::running && !viewOwned)
            mirrorView = nullptr; // left the pool: forget it, never write to it
        // MIRROR TEST: a picture fault stops it (it goes OFF); OFF on the wrist closes it.
        if (stats.test.phase == mv::running && gpu.fault)
            stats.test.pictureFailed();
        else if (stats.test.phase == mv::running && !wanted)
            stats.test.close();
        retire = retire || stats.test.phase != mv::running || retiring() || stopRequested;
        show = !retire && viewOwned;
    } else if (stats.test.phase == mv::done && mirrorRetired && wanted && viewOwned && vrUp && !gpu.fault) {
        // MIRROR TEST: ON again in the same session: the kept view comes back (as a retired eye does).
        reopen = stats.test.reopen();
    } else if (stats.test.phase == mv::off || stats.test.phase == mv::waiting) {
        stats.test.idleFrame(ms);
        // Asked for after the eyes were made: it waits for the next start (a restart).
        stats.tooLate = mirrorWanted.load() && attempted && eyes[0].load() ? 1u : 0u;
    }
    if (stats.test.slot < 8)
        spidy::mirror_probe::slotDraws(stats.test.slot, stats.heroDraws, stats.otherDraws);
    ReleaseSRWLockExclusive(&mirrorLock);
    if (retire) {
        AcquireSRWLockExclusive(&mirrorLock);
        if (mirrorStats.test.phase == mv::running)
            mirrorStats.test.phase = mv::done;
        ReleaseSRWLockExclusive(&mirrorLock);
        if (viewOwned)
            mirrorRetire();
        else
            native_gpu::registerMirror(nullptr);
        return;
    }
    if (reopen) {
        InterlockedAnd(reinterpret_cast<volatile LONG*>(reinterpret_cast<uintptr_t>(view) + 0x1dae4), ~1L);
        mirrorRetired = false;
        OutputDebugStringA("MIRROR TEST: view back\n");
        return; // its image is taken from the next frame
    }
    // MIRROR TEST: its image, read exactly as the eyes' (+1e28 -> +40, the confirmed texture
    // object type 8324540, +38 -> +8 the ID3D12Resource); native_gpu copies it into both eyes.
    // Anything else: no picture this frame (nothing is registered).
    if (show) {
        const auto address = reinterpret_cast<uintptr_t>(view);
        const auto texture = pointer(address + 0x1e28);
        const auto object = texture ? pointer(texture + 0x40) : 0;
        const auto resource = object && pointer(object) == base + 0x8324540 ? pointer(pointer(object + 0x38) + 8) : 0;
        native_gpu::registerMirror(reinterpret_cast<ID3D12Resource*>(resource));
    }
}
// In placeEyes, after the eyes: the mirror camera faces the head from 1.2 m.
void mirrorPlace(const Descriptor& main, const Mat4& head) {
    namespace mv = spidy::mirror_view;
    auto view = mirrorView.load();
    if (!view || mirrorRetired || !owned(view))
        return;
    Mat4 pose{};
    if (!mv::mirrorPose(head, pose, mv::fullBodyMetres, mv::fullBodyDrop)) // MIRROR TEST: the whole body
        return;
    auto descriptor = main;
    const float x = mv::horizontalTan(), y = mv::verticalTan;
    reinterpret_cast<Lens>(base + 0x2c75560)(&descriptor, main.nearZ(), main.farZ(), -x, x, -y, y, 0, 0, 0, 0,
                                             false, false);
    reinterpret_cast<SetPose>(base + 0x2c76310)(&descriptor, pose.data(), 0);
    reinterpret_cast<Submit>(base + 0x2c95200)(view, &descriptor);
    AcquireSRWLockExclusive(&mirrorLock);
    ++mirrorStats.posed;
    ReleaseSRWLockExclusive(&mirrorLock);
}
void SpidyMirrorViewWantImpl(uint32_t on) { mirrorWanted = on ? 1u : 0u; }
void maintain(void* manager) {
    const bool ours = reinterpret_cast<uintptr_t>(manager) == base + 0xc2ca220;
    Descriptor main{};
    auto primary = pointer(base + 0xc2ca220);
    const bool valid = ours && read(primary, &main, sizeof(main)) && native_view::valid(main);
    AcquireSRWLockExclusive(&creation);
    // A load can replace the native view pool. Check ownership before any write.
    if (ours)
        for (unsigned i = 0; i < 2; ++i)
            if (auto eye = eyes[i].load(); eye && !owned(eye)) {
                eyes[i] = nullptr;
                if (!retired[i])
                    --live;
                retired[i] = false;
                stopRequested = true;
                AcquireSRWLockExclusive(&telemetry);
                InterlockedIncrement64(&SpidyStereoData.sequence);
                SpidyStereoData.error = 2003;
                InterlockedIncrement64(&SpidyStereoData.sequence);
                ReleaseSRWLockExclusive(&telemetry);
            }
    if (ours && enabled && !stopRequested && (config.createViews & 1) &&
        (!attempted || retired[0] || retired[1]) && valid && GetTickCount64() < deadline &&
        (config.durationMs || GetTickCount64() < commandDeadline)) {
        attempted = true;
        static const char* names[] = {"Spidy left eye test", "Spidy right eye test"};
        for (unsigned i = 0; i < 2; ++i) {
            if (!eyes[i]) {
                // LIGHT CHECKS: the eye's query pool as big as the game view's (at least 1024), not 256.
                namespace eq = spidy::eye_queries;
                int32_t gamePool{};
                read(primary + eq::capacityOffset, &gamePool, sizeof gamePool);
                const uint32_t room = eq::room(gamePool);
                const uint8_t stockByte = eq::clampBytes[2];
                bool widened = false;
                if (!lightChecksOn.load())
                    lightChecksState = 2;
                else if (!(widened = lightChecksByte(stockByte, eq::roomByte(room))))
                    lightChecksState = 3;
                creatingEye = true;
                const CreateArgs args{names[i], main.nearZ(), main.farZ(), 90.f, config.width, config.height,
                                      28, nullptr, 0x100, 2, widened ? room : 0xffffffffu, 0};
                eyes[i] = reinterpret_cast<Create>(base + 0x2c9c590)(manager, &args);
                creatingEye = false;
                if (widened) {
                    lightChecksState = lightChecksByte(eq::roomByte(room), stockByte) ? 1u : 4u;
                    // 4: the byte stays widened for later kind 2/4/5 views (bigger pools only, never smaller).
                }
                int32_t eyePool{};
                if (eyes[i])
                    read(reinterpret_cast<uintptr_t>(eyes[i].load()) + eq::capacityOffset, &eyePool, sizeof eyePool);
                lightChecksRoom[i] = eyePool;
                // Render jobs copy views from the next frame on (1920240).
                if (eyes[i] && (config.createViews & 16))
                    giveOcclusion(eyes[i].load());
                AcquireSRWLockExclusive(&telemetry);
                InterlockedIncrement64(&SpidyStereoData.sequence);
                if (eyes[i]) {
                    ++live;
                    ++SpidyStereoData.created;
                    if (config.createViews & 4)
                        // 0x80 selects the active postprocessing profile in
                        // 183e9a0. Secondary views otherwise receive defaults,
                        // including a different exposure mode. 0x400 sends the
                        // display conversion into our output target at +0xfa0.
                        InterlockedOr(reinterpret_cast<volatile LONG*>(
                                          reinterpret_cast<uintptr_t>(eyes[i].load()) + 0x1daec),
                                      0x480);
                    // Use the engine's own asynchronous GPU readback path.
                    // This is enabled once, before deferred view initialization.
                    if (config.createViews & 2)
                        reinterpret_cast<Maintain>(base + 0x2c651b0)(eyes[i].load());
                } else {
                    SpidyStereoData.error = 2001 + i;
                    stopRequested = true;
                }
                InterlockedIncrement64(&SpidyStereoData.sequence);
                ReleaseSRWLockExclusive(&telemetry);
            } else if (retired[i]) {
                InterlockedAnd(
                    reinterpret_cast<volatile LONG*>(reinterpret_cast<uintptr_t>(eyes[i].load()) + 0x1dae4),
                    ~1L);
                retired[i] = false;
                ++live;
            }
        }
        if (eyes[0] && eyes[1] && !stopRequested)
            mirrorCreate(manager, main);
    }
    ReleaseSRWLockExclusive(&creation);
    if (ours && retiring())
        for (unsigned i = 0; i < 2; ++i)
            if (auto eye = eyes[i].load(); eye && !retired[i]) {
                // Exclude from new render jobs but retain storage for already queued
                // work. The game's normal shutdown owns final resource destruction.
                // Immediate native deletion caused a worker to use a null resource.
                InterlockedOr(reinterpret_cast<volatile LONG*>(reinterpret_cast<uintptr_t>(eye) + 0x1dae4),
                              1);
                retired[i] = true;
                --live;
                AcquireSRWLockExclusive(&telemetry);
                InterlockedIncrement64(&SpidyStereoData.sequence);
                ++SpidyStereoData.retired;
                InterlockedIncrement64(&SpidyStereoData.sequence);
                ReleaseSRWLockExclusive(&telemetry);
            }
    // The engine initializes queued resources here. Eye poses are placed in
    // placeEyes(), before the next frame's render jobs copy the views.
    if (ours && lateEyes)
        placeEyes();
    originalMaintain(manager);
    if (!ours)
        return;
    ++frameIndex;
    mirrorFrame();
    engineTaaWatch();
    if (SpidyCommandData.tracing)
        ++SpidyCommandData.frames;
    AcquireSRWLockExclusive(&telemetry);
    auto& d = SpidyStereoData;
    InterlockedIncrement64(&d.sequence);
    ++d.frames;
    d.primary = primary;
    d.thread = GetCurrentThreadId();
    mainThread = d.thread;
    for (unsigned i = 0; i < 2; ++i) {
        if (!eyes[i])
            continue;
        bool present = false;
        for (unsigned slot = 0; slot < 8; ++slot)
            present |= pointer(base + 0xc2ca250 + slot * 8) == reinterpret_cast<uintptr_t>(eyes[i].load());
        if (!present) {
            eyes[i] = nullptr;
            if (!retired[i])
                --live;
            retired[i] = false;
            d.error = 2003;
            continue;
        }
        auto& e = d.eyes[i];
        e.view = reinterpret_cast<uintptr_t>(eyes[i].load());
        e.texture = pointer(e.view + 0x1e28);
        e.textureObject = pointer(e.texture + 0x40);
        // Register before the render worker records this frame. GPU state is
        // learned from actual submitted transitions, never an assumed UAV state.
        if (e.textureObject && pointer(e.textureObject) == base + 0x8324540)
            native_gpu::registerEye(
                i, reinterpret_cast<ID3D12Resource*>(pointer(pointer(e.textureObject + 0x38) + 8)));
        e.updates = updates[i].load();
        read(e.view + 0x1dae4, &e.flags, 4);
        e.occlusion = pointer(e.view + 0x1a00) != 0;
        read(e.view + 0x1da78, &e.width, 4);
        read(e.view + 0x1da7c, &e.height, 4);
        read(pointer(e.view + 0x1e38) + 0x1b978, &e.renderWidth, 4);
        read(pointer(e.view + 0x1e38) + 0x1b97c, &e.renderHeight, 4);
        read(e.view + 0x460, &e.viewportWidth, 4);
        read(e.view + 0x464, &e.viewportHeight, 4);
        read(e.view, e.pose, sizeof(e.pose));
        e.readback = pointer(e.view + 0x1dad0);
        read(e.view + 0x1dad8, &e.readbackBytes, 4);
        read(e.view + 0x1dadc, &e.readbackCompleted, 4);
    }
    InterlockedIncrement64(&d.sequence);
    ReleaseSRWLockExclusive(&telemetry);
    // Update 38.4: the MIRROR PROBE's frame (read-only; stops itself when slow or on a fault).
    if (enabled) {
        spidy::mirror_probe::Frame probe;
        probe.base = base;
        probe.primary = primary;
        probe.left = reinterpret_cast<uint64_t>(eyes[0].load());
        probe.right = reinterpret_cast<uint64_t>(eyes[1].load());
        probe.record = playerRecord.load();
        probe.portHidden = hiddenHero.load();
        probe.portHides = latchedHide.load(std::memory_order_relaxed);
        probe.bodyDrawn = latchedImmersive.load(std::memory_order_relaxed) && !probe.portHides;
        { // Update 38.5 PROBE v2: the port's body solves (is the suits menu's collapsed preview ours?)
            const auto body = native_body::status();
            probe.bodySolved = body.solved;
            probe.bodyHeroJobs = body.heroJobs;
            probe.bodyRigSwitches = body.rigSwitches;
        }
        spidy::mirror_probe::frame(probe);
    }
}
// SM2: a hook left out (nullptr) is skipped; MinHook would take nullptr as
// "every hook".
MH_STATUS enableHook(void* hook) {
    return hook ? MH_EnableHook(hook) : MH_OK;
}
MH_STATUS disableHook(void* hook) {
    return hook ? MH_DisableHook(hook) : MH_ERROR_DISABLED;
}
void removeHook(void* hook) {
    if (hook)
        MH_RemoveHook(hook);
}
} // namespace
extern "C" __declspec(dllexport) DWORD WINAPI SpidySetEyes(void* input) {
    native_eyes::Command incoming{};
    if (!read(reinterpret_cast<uintptr_t>(input), &incoming, sizeof(incoming)) ||
        !native_eyes::valid(incoming))
        return 4001;
    if (!enabled || stopRequested || GetTickCount64() >= deadline)
        return 4003;
    AcquireSRWLockExclusive(&commandLock);
    if (incoming.serial <= command.serial) {
        ReleaseSRWLockExclusive(&commandLock);
        return 4002;
    }
    command = incoming;
    commandDeadline = GetTickCount64() + incoming.leaseMs;
    ReleaseSRWLockExclusive(&commandLock);
    AcquireSRWLockExclusive(&jobLock);
    InterlockedIncrement64(&SpidyStereoFrames.sequence);
    SpidyStereoFrames.accepted = incoming.serial;
    InterlockedIncrement64(&SpidyStereoFrames.sequence);
    ReleaseSRWLockExclusive(&jobLock);
    return 0;
}
extern "C" __declspec(dllexport) DWORD WINAPI SpidyStart(void* value) {
    AcquireSRWLockExclusive(&lifecycle);
    DWORD result{};
    do {
        if (enabled)
            break;
        base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
        Config incoming{};
        if (!read(reinterpret_cast<uintptr_t>(value), &incoming, sizeof(incoming))) {
            result = 1001;
            break;
        }
        if ((eyes[0] || eyes[1]) && (incoming.width != config.width || incoming.height != config.height ||
                                     incoming.createViews != config.createViews)) {
            result = 1003;
            break;
        }
        config = incoming;
        // createViews bits: 1 eye views, 2 native readback, 4 display/VR path,
        // 8 (VR path only) move the engine's active view to the head while immersive,
        // 16 (VR path only) occlusion culling for each eye (giveOcclusion),
        // 32 (with 8) the game's own view is not rendered while immersive
        // (restGameView): the left eye goes to the game's window instead,
        // 64 the eyes keep the distances their own lenses give (no
        // matchDistances or setupModels; builds before October 19),
        // 128 no eye stands in for the resting game view's impostors (no
        // isActiveView; builds before October 19's second).
        const auto views = config.createViews & ~(16u | 32u | 64u | 128u);
        if (!GetModuleHandleW(L"Spider-Man2.exe") || config.magic != 0x53534346 || config.version != 1 ||
            config.bytes != sizeof(config) || config.pid != GetCurrentProcessId() || config.base != base ||
            config.durationMs > 30000 || (config.durationMs && config.durationMs < 500) ||
            (views != 0 && views != 1 && views != 3 && views != 5 && views != 7 && views != 13) ||
            ((config.createViews & 16) && !(views & 4)) || ((config.createViews & 32) && !(views & 8)) ||
            !validEyeSize(config.width) ||
            !validEyeSize(config.height)) {
            result = 1001;
            break;
        }
        const unsigned char maintainBytes[] = {0x48, 0x89, 0x5c, 0x24, 0x08, 0x48, 0x89, 0x6c, 0x24, 0x10};
        const unsigned char createBytes[] = {0x48, 0x89, 0x5c, 0x24, 0x10, 0x48, 0x89};
        const unsigned char updateBytes[] = {0x48, 0x89, 0x5c, 0x24, 0x08, 0x48, 0x89, 0x74, 0x24, 0x10};
        const unsigned char readbackBytes[] = {0x48, 0x89, 0x5c, 0x24, 0x08, 0x48, 0x89, 0x74, 0x24, 0x10};
        const unsigned char copyFinalBytes[] = {0x40, 0x53, 0x48, 0x83, 0xec, 0x30, 0xf6, 0x81, 0xe4, 0xda, 0x01, 0x00, 0x20};
        const unsigned char toneBytes[] = {0x48, 0x8b, 0xc4, 0x48, 0x89, 0x50, 0x10};
        const unsigned char lensBytes[] = {0x48, 0x8b, 0xc4, 0x48, 0x89, 0x58, 0x08, 0x48};
        const unsigned char copyJobBytes[] = {0x48, 0x8b, 0xc4, 0x48, 0x89, 0x68, 0x18, 0x48};
        const unsigned char beginJobBytes[] = {0x48, 0x89, 0x5c, 0x24, 0x08, 0x57, 0x48, 0x83, 0xec, 0x30};
        const unsigned char endJobBytes[] = {0x48, 0x89, 0x5c, 0x24, 0x08, 0x48, 0x89, 0x74, 0x24, 0x10};
        const unsigned char initBuffersBytes[] = {0x40, 0x53, 0x48, 0x83, 0xec, 0x30, 0x44, 0x8b, 0x4a, 0x04};
        // A buffer set's sizes; the test for which targets its kind gets, and
        // the kinds' masks it reads (3d11b18: an offscreen view's is 0x20);
        // TemporalAa up to its test of the render context's anti-aliasing.
        const unsigned char sizeBuffersBytes[] = {0x48, 0x89, 0x5c, 0x24, 0x08, 0x48, 0x89, 0x6c, 0x24, 0x10, 0x48, 0x89, 0x74, 0x24, 0x20, 0x57, 0x41, 0x54};
        const unsigned char renderActorBytes[] = {0x48, 0x8b, 0xc4, 0x44, 0x88, 0x40, 0x18, 0x48,
                                                  0x89, 0x50, 0x10, 0x48, 0x89, 0x48, 0x08};
        // A view's model job setup; there, its reads of the view's +1cc8 x
        // +1cc4 and of its rendered size and half tangents into +2fc and +300;
        // the lens scale's read of +41c and store to +1cc4 (189e4b0) with its
        // two numerators and 16:9.
        const unsigned char setupModelsBytes[] = {0x48, 0x89, 0x5c, 0x24, 0x08, 0x48, 0x89, 0x74, 0x24, 0x10, 0x57, 0x48, 0x83, 0xec, 0x30, 0x48, 0x8b, 0x02, 0x48, 0x8b, 0xf9, 0x48, 0x89, 0x01, 0x48, 0x8b, 0x4a, 0x08, 0xc5};
        const unsigned char modelScaleBytes[] = {0xf3, 0x41, 0x0f, 0x10, 0x8a, 0xc8, 0x1c, 0x00, 0x00,
                                                 0xf3, 0x41, 0x0f, 0x59, 0x8a, 0xc4, 0x1c, 0x00, 0x00};
        const unsigned char modelRequestBytes[] = {0xf3, 0x41, 0x0f, 0x10, 0x8a, 0x4c, 0x04, 0x00, 0x00,
                                                   0xf3, 0x41, 0x0f, 0x5e, 0x8a, 0x24, 0x04, 0x00, 0x00,
                                                   0xf3, 0x41, 0x0f, 0x10, 0x82, 0x48, 0x04, 0x00, 0x00,
                                                   0xf3, 0x41, 0x0f, 0x5e, 0x82, 0x20, 0x04, 0x00, 0x00};
        const unsigned char modelRequestStoreBytes[] = {0xf3, 0x0f, 0x11, 0x8f, 0xfc, 0x02, 0x00, 0x00,
                                                        0xf3, 0x0f, 0x11, 0x97, 0x00, 0x03, 0x00, 0x00};
        const unsigned char lensAspectBytes[] = {0xf3, 0x0f, 0x10, 0x81, 0x1c, 0x04, 0x00, 0x00};
        const unsigned char lensScaleBytes[] = {0xf3, 0x0f, 0x11, 0xb3, 0xc4, 0x1c, 0x00, 0x00};
        const unsigned char numeratorBytes[] = {0x29, 0x42, 0xc2, 0x3e, 0x97, 0xac, 0x2c, 0x3f};
        const unsigned char widescreenBytes[] = {0x39, 0x8e, 0xe3, 0x3f};
        // "Is this the active view" (view == ViewContextManager +f8), and its
        // five calls in the impostor code (the impostor submit job 2a6af10,
        // both ImpostorHQModelRegJob launchers 2a6c2e0 and 2a6c410). Update 36:
        // these and reachLast were SM1's addresses, so `impostors` was always
        // off here; see docs/main-view-gates-audit.md.
        const unsigned char isActiveViewBytes[] = {0x48, 0x3b, 0x0d, 0xc9, 0x1c, 0x63, 0x09, 0x0f, 0x94, 0xc0, 0xc3};
        const struct {
            uintptr_t rva;
            unsigned char bytes[5];
        } impostorCalls[] = {{0x2a6af90, {0xe8, 0x0b, 0xe2, 0x22, 0x00}}, {0x2a6c346, {0xe8, 0x55, 0xce, 0x22, 0x00}},
                             {0x2a6c356, {0xe8, 0x45, 0xce, 0x22, 0x00}}, {0x2a6c467, {0xe8, 0x34, 0xcd, 0x22, 0x00}},
                             {0x2a6c477, {0xe8, 0x24, 0xcd, 0x22, 0x00}}};
        // The impostor visibility queries; their result's distance, set to 145 m
        // first and to the lens's last, and the result returned.
        const unsigned char impostorQueriesBytes[] = {0x48, 0x89, 0x5c, 0x24, 0x08, 0x48, 0x89, 0x6c, 0x24, 0x10,
                                                      0x48, 0x89, 0x74, 0x24, 0x18, 0x57, 0x41, 0x56, 0x41, 0x57};
        const unsigned char reachFirstBytes[] = {0xc7, 0x80, 0x88, 0x00, 0x00, 0x00, 0x00, 0x00, 0x11, 0x43};
        const unsigned char reachLastBytes[] = {0xc5, 0xfa, 0x11, 0x83, 0x88, 0x00, 0x00, 0x00, 0x48, 0x8b, 0xc3};
        const unsigned char setupDisplayBytes[] = {0x4d, 0x85, 0xc0, 0x0f, 0x84, 0x49, 0x04, 0x00, 0x00};
        const unsigned char enqueueBytes[] = {0x48, 0x89, 0x5c, 0x24, 0x08, 0x48, 0x89, 0x6c, 0x24, 0x10};
        // ActorDrawAction's DrawOff/DrawOn: toggle flag 0x20 at +5c and mark
        // the instance for the renderer. Called only from maintain().
        const unsigned char drawOffBytes[] = {0x8b, 0x41, 0x5c, 0xa8, 0x20, 0x75, 0x41, 0x83, 0xc8, 0x20, 0x89, 0x41, 0x5c};
        const unsigned char drawOnBytes[] = {0x8b, 0x41, 0x5c, 0xa8, 0x20, 0x74, 0x41, 0x83, 0xe0, 0xdf, 0x89, 0x41, 0x5c};
        // View submit, the stock camera's call to it in 1646e10, and the pose setter.
        const unsigned char submitBytes[] = {0x48, 0x3b, 0xd1, 0x0f, 0x84, 0x92, 0x00, 0x00};
        const unsigned char cameraSubmitBytes[] = {0xe8, 0x2f, 0x93, 0x30, 0x00};
        const unsigned char poseBytes[] = {0x48, 0x89, 0x5c, 0x24, 0x08, 0x57, 0x48, 0x83, 0xec};
        // Offscreen render setup, its only call (in the frame function 175a060,
        // after the history shift 18a13c0 and before view maintenance), and the
        // history shift's per-view copy 189ee00 that placeEyes() relies on.
        const unsigned char renderOffscreenBytes[] = {0x48, 0x89, 0x5c, 0x24, 0x08, 0x57, 0x48, 0x83, 0xec, 0x20, 0x33, 0xdb, 0x48, 0x8d, 0x3d, 0x0d, 0x8c, 0x5c};
        const unsigned char frameOffscreenBytes[] = {0xe8, 0x66, 0x62, 0x20, 0x00};
        const unsigned char submitSceneBytes[] = {0x48, 0x8b, 0xc4, 0x48, 0x89, 0x58, 0x10, 0x48, 0x89, 0x70, 0x18, 0x48, 0x89, 0x78, 0x20, 0x55, 0x41, 0x54, 0x41, 0x55, 0x41};
        const unsigned char fitCascadesBytes[] = {0x48, 0x89, 0x5c, 0x24, 0x08, 0x55, 0x56, 0x57, 0x41, 0x54, 0x41, 0x55, 0x41, 0x56, 0x41, 0x57};
        const unsigned char historyBytes[] = {0x40, 0x57, 0x48, 0x83, 0xec, 0x20, 0x8b, 0x05, 0x8c, 0x8b, 0x68, 0x09, 0x48, 0x8b, 0xf9, 0x39, 0x81, 0x90, 0x0f};
        // A view's occlusion builder, up to its test of +1ba0.
        const unsigned char occlusionBytes[] = {0x48, 0x89, 0x5c, 0x24, 0x10, 0x48, 0x89, 0x74, 0x24, 0x18, 0x57, 0x48, 0x83, 0xec, 0x40, 0x48, 0x8b, 0xf9, 0x48, 0x83, 0xb9, 0x00, 0x1a, 0x00, 0x00, 0x00};
        // The HUD follower's placement and the render instance's transform setter.
        const unsigned char hudPlaceBytes[] = {0x40, 0x55, 0x53, 0x48, 0x8d, 0xac, 0x24, 0xc8, 0xfe,
                                               0xff, 0xff, 0x48, 0x81, 0xec, 0x38, 0x02, 0x00, 0x00};
        const unsigned char setTransformBytes[] = {0x48, 0x8b, 0xc4, 0x48, 0x89, 0x58, 0x08, 0x48, 0x89, 0x70, 0x10};
        // A view's adapted luminance for shaders: its own buffer at +16f0.
        const unsigned char adaptedLumBytes[] = {0x48, 0x8d, 0x81, 0x68, 0x19, 0x00, 0x00, 0x4c, 0x8b, 0x00};
        if (!entry(0x2c9c780, maintainBytes, sizeof(maintainBytes)) ||
            !entry(0x2c97b10, adaptedLumBytes, sizeof(adaptedLumBytes)) ||
            !entry(0x2c95200, submitBytes, sizeof(submitBytes)) ||
            !entry(0x298becc, cameraSubmitBytes, sizeof(cameraSubmitBytes)) ||
            !entry(0x2c76310, poseBytes, sizeof(poseBytes)) ||
            !entry(0x2d01600, renderOffscreenBytes, sizeof(renderOffscreenBytes)) ||
            !entry(0x2d01b30, submitSceneBytes, sizeof(submitSceneBytes)) ||
            !entry(0x2afb395, frameOffscreenBytes, sizeof(frameOffscreenBytes)) ||
            !entry(0x2c9abf0, historyBytes, sizeof(historyBytes)) ||
            !entry(0x2c9c590, createBytes, sizeof(createBytes)) ||
            !entry(0x2c96f80, updateBytes, sizeof(updateBytes)) ||
            ((config.createViews & 2) && !entry(0x2c651b0, readbackBytes, sizeof(readbackBytes))) ||
            ((config.createViews & 16) && !entry(0x2c9b8e0, occlusionBytes, sizeof(occlusionBytes))) ||
            !entry(0x2c656d0, copyFinalBytes, sizeof(copyFinalBytes)) ||
            !entry(0x2c32a70, toneBytes, sizeof(toneBytes)) ||
            !entry(0x2c75560, lensBytes, sizeof(lensBytes)) ||
            !entry(0x2d03240, copyJobBytes, sizeof(copyJobBytes)) ||
            !entry(0x2c99e30, beginJobBytes, sizeof(beginJobBytes)) ||
            !entry(0x2c99ec0, endJobBytes, sizeof(endJobBytes)) ||
            !entry(0x2c6cb70, initBuffersBytes, sizeof(initBuffersBytes)) ||
            !entry(0x2c6ebf0, sizeBuffersBytes, sizeof(sizeBuffersBytes)) ||
            !entry(0x2b59660, renderActorBytes, sizeof(renderActorBytes)) ||
            !entry(0x2b555e0, setupModelsBytes, sizeof(setupModelsBytes)) ||
            !entry(0x2c991a0, isActiveViewBytes, sizeof(isActiveViewBytes)) ||
            !entry(0x2a6bc60, impostorQueriesBytes, sizeof(impostorQueriesBytes)) ||
            !entry(0x2d016d0, setupDisplayBytes, sizeof(setupDisplayBytes)) ||
            !entry(0x2c77810, enqueueBytes, sizeof(enqueueBytes)) ||
            !entry(0x2cfa6a0, drawOffBytes, sizeof(drawOffBytes)) ||
            !entry(0x2cfba50, drawOnBytes, sizeof(drawOnBytes)) ||
            (SM2_STEREO_HUD && !entry(0x9e53d0, hudPlaceBytes, sizeof(hudPlaceBytes))) || // SM2TODO: unknown
            !entry(0x2cfc580, setTransformBytes, sizeof(setTransformBytes))) {
            result = 1002;
            break;
        }
        // The eyes' distances need the job's and the lens scale's layout as
        // read here; another game build keeps the eye lenses' own.
        distances = !(config.createViews & 64) && (config.createViews & 4) &&
                    entry(0x1ca1dbd, modelScaleBytes, sizeof(modelScaleBytes)) &&
                    entry(0x1ca1edf, modelRequestBytes, 18) &&
                    entry(0x1ca1f1d, modelRequestStoreBytes, sizeof(modelRequestStoreBytes)) &&
                    entry(0x1da9b17, lensAspectBytes, sizeof(lensAspectBytes)) &&
                    entry(0x1da9ba8, lensScaleBytes, sizeof(lensScaleBytes)) &&
                    entry(0x4413c08, numeratorBytes, sizeof(numeratorBytes)) &&
                    entry(0x3dcf678, widescreenBytes, sizeof(widescreenBytes));
        impostors = !(config.createViews & 128) && (config.createViews & 32);
        for (const auto& call : impostorCalls)
            impostors = impostors && entry(call.rva, call.bytes, sizeof(call.bytes));
        impostors = impostors && entry(0x2a6bcad, reachFirstBytes, sizeof(reachFirstBytes)) &&
                    entry(0x2a6bded, reachLastBytes, sizeof(reachLastBytes));
        // Update 38.3 FAR WINDOWS write 2: checked on its own (SM2 never sets flag 32).
        farWindowsReady = entry(0x2a6bc60, impostorQueriesBytes, sizeof(impostorQueriesBytes)) &&
                          entry(0x2a6bcad, reachFirstBytes, sizeof(reachFirstBytes)) &&
                          entry(0x2a6bded, reachLastBytes, sizeof(reachLastBytes));
        if (!hooked) {
            auto s = MH_Initialize();
            if (s != MH_OK && s != MH_ERROR_ALREADY_INITIALIZED) {
                result = 1100 + s;
                break;
            }
            maintainHook = reinterpret_cast<void*>(base + 0x2c9c780);
            updateHook = reinterpret_cast<void*>(base + 0x2c96f80);
            copyFinalHook = reinterpret_cast<void*>(base + 0x2c656d0);
            toneHook = reinterpret_cast<void*>(base + 0x2c32a70);
            copyJobHook = reinterpret_cast<void*>(base + 0x2d03240);
            beginJobHook = reinterpret_cast<void*>(base + 0x2c99e30);
            endJobHook = reinterpret_cast<void*>(base + 0x2c99ec0);
            initBuffersHook = reinterpret_cast<void*>(base + 0x2c6cb70);
            sizeBuffersHook = reinterpret_cast<void*>(base + 0x2c6ebf0);
            renderActorHook = reinterpret_cast<void*>(base + 0x2b59660);
            setupModelsHook = reinterpret_cast<void*>(base + 0x2b555e0);
            isActiveViewHook = reinterpret_cast<void*>(base + 0x2c991a0);
            impostorQueriesHook = reinterpret_cast<void*>(base + 0x2a6bc60);
            setupDisplayHook = reinterpret_cast<void*>(base + 0x2d016d0);
            submitHook = reinterpret_cast<void*>(base + 0x2c95200);
            renderOffscreenHook = reinterpret_cast<void*>(base + 0x2d01600);
            hudPlaceHook = SM2_STEREO_HUD ? reinterpret_cast<void*>(base + 0x9e53d0) : nullptr; // SM2TODO: bytes, vtable
            submitSceneHook = reinterpret_cast<void*>(base + 0x2d01b30);
            // SM2: optional; hooked only where its entry matches SM2VR's bytes.
            fitCascadesHook = entry(0x2dd8d20, fitCascadesBytes, sizeof(fitCascadesBytes))
                                  ? reinterpret_cast<void*>(base + 0x2dd8d20)
                                  : nullptr;
            {   // Update 35: FAR SHADOWS, only where the fit and its two asks are as known.
                namespace fs = spidy::far_shadows;
                farShadowsReady = entry(fs::fitRva, fs::fitBytes, sizeof(fs::fitBytes)) &&
                                  entry(fs::fitAskRva, fs::fitAskBytes, sizeof(fs::fitAskBytes)) &&
                                  entry(fs::cacheAskRva, fs::cacheAskBytes, sizeof(fs::cacheAskBytes));
                csmFitHook = farShadowsReady ? reinterpret_cast<void*>(base + fs::fitRva) : nullptr;
            }
            {   // Update 36: SHARED LOD's hook and EYE BLOOM's gate, only where their bytes are as known.
                namespace sl = spidy::shared_lod;
                namespace eb = spidy::eye_bloom;
                viewScaleHook = entry(sl::scaleRva, sl::scaleBytes, sizeof(sl::scaleBytes))
                                    ? reinterpret_cast<void*>(base + sl::scaleRva)
                                    : nullptr;
                bloomGateKnown = entry(eb::gateRva, eb::gateBytes, sizeof(eb::gateBytes));
                // Update 37: LOD FADE, only where the fade's call to IsActiveView is as known
                // (and IsActiveView itself, hooked above, is).
                lodFadeKnown = entry(spidy::view_gates::fadeCallRva, spidy::view_gates::fadeCallBytes,
                                     sizeof(spidy::view_gates::fadeCallBytes)) &&
                               entry(0x2c991a0, isActiveViewBytes, sizeof(isActiveViewBytes));
            }
            s = MH_CreateHook(maintainHook, reinterpret_cast<void*>(maintain),
                              reinterpret_cast<void**>(&originalMaintain));
            if (s == MH_OK)
                s = MH_CreateHook(updateHook, reinterpret_cast<void*>(update),
                                  reinterpret_cast<void**>(&originalUpdate));
            if (s == MH_OK)
                s = MH_CreateHook(copyFinalHook, reinterpret_cast<void*>(copyFinal),
                                  reinterpret_cast<void**>(&originalCopyFinal));
            if (s == MH_OK)
                s = MH_CreateHook(toneHook, reinterpret_cast<void*>(tone),
                                  reinterpret_cast<void**>(&originalTone));
            if (s == MH_OK)
                s = MH_CreateHook(copyJobHook, reinterpret_cast<void*>(copyJob),
                                  reinterpret_cast<void**>(&originalCopyJob));
            if (s == MH_OK)
                s = MH_CreateHook(beginJobHook, reinterpret_cast<void*>(beginJob),
                                  reinterpret_cast<void**>(&originalBeginJob));
            if (s == MH_OK)
                s = MH_CreateHook(endJobHook, reinterpret_cast<void*>(endJob),
                                  reinterpret_cast<void**>(&originalEndJob));
            if (s == MH_OK)
                s = MH_CreateHook(initBuffersHook, reinterpret_cast<void*>(initBuffers),
                                  reinterpret_cast<void**>(&originalInitBuffers));
            if (s == MH_OK)
                s = MH_CreateHook(sizeBuffersHook, reinterpret_cast<void*>(sizeBuffers),
                                  reinterpret_cast<void**>(&originalSizeBuffers));
            if (s == MH_OK)
                s = MH_CreateHook(renderActorHook, reinterpret_cast<void*>(renderActor),
                                  reinterpret_cast<void**>(&originalRenderActor));
            if (s == MH_OK)
                s = MH_CreateHook(setupModelsHook, reinterpret_cast<void*>(setupModels),
                                  reinterpret_cast<void**>(&originalSetupModels));
            if (s == MH_OK)
                s = MH_CreateHook(isActiveViewHook, reinterpret_cast<void*>(isActiveView),
                                  reinterpret_cast<void**>(&originalIsActiveView));
            if (s == MH_OK)
                s = MH_CreateHook(impostorQueriesHook, reinterpret_cast<void*>(impostorQueries),
                                  reinterpret_cast<void**>(&originalImpostorQueries));
            if (s == MH_OK)
                s = MH_CreateHook(setupDisplayHook, reinterpret_cast<void*>(setupDisplay),
                                  reinterpret_cast<void**>(&originalSetupDisplay));
            if (s == MH_OK)
                s = MH_CreateHook(submitHook, reinterpret_cast<void*>(submit),
                                  reinterpret_cast<void**>(&originalSubmit));
            if (s == MH_OK)
                s = MH_CreateHook(renderOffscreenHook, reinterpret_cast<void*>(renderOffscreen),
                                  reinterpret_cast<void**>(&originalRenderOffscreen));
            if (s == MH_OK && hudPlaceHook)
                s = MH_CreateHook(hudPlaceHook, reinterpret_cast<void*>(hudPlace),
                                  reinterpret_cast<void**>(&originalHudPlace));
            if (s == MH_OK)
                s = MH_CreateHook(submitSceneHook, reinterpret_cast<void*>(submitScene),
                                  reinterpret_cast<void**>(&originalSubmitScene));
            if (s == MH_OK && fitCascadesHook)
                s = MH_CreateHook(fitCascadesHook, reinterpret_cast<void*>(fitCascades),
                                  reinterpret_cast<void**>(&originalFitCascades));
            if (s == MH_OK && csmFitHook)
                s = MH_CreateHook(csmFitHook, reinterpret_cast<void*>(csmFit),
                                  reinterpret_cast<void**>(&originalCsmFit));
            if (s == MH_OK && viewScaleHook)
                s = MH_CreateHook(viewScaleHook, reinterpret_cast<void*>(viewScale),
                                  reinterpret_cast<void**>(&originalViewScale));
            if (s != MH_OK) {
                result = 1200 + s;
                for (auto hook : {maintainHook, updateHook, copyFinalHook, toneHook, copyJobHook, beginJobHook,
                                  endJobHook, initBuffersHook, sizeBuffersHook, renderActorHook, setupModelsHook,
                                  isActiveViewHook, impostorQueriesHook, setupDisplayHook, submitHook,
                                  renderOffscreenHook, hudPlaceHook, submitSceneHook, fitCascadesHook, csmFitHook, viewScaleHook})
                    removeHook(hook);
                break;
            }
            hooked = true;
        }
        attempted = false;
        stopRequested = false;
        deadline = config.durationMs ? GetTickCount64() + config.durationMs : UINT64_MAX;
        auto s = MH_EnableHook(updateHook);
        if (s == MH_OK)
            s = MH_EnableHook(copyFinalHook);
        if (s == MH_OK)
            s = MH_EnableHook(toneHook);
        for (auto hook : {copyJobHook, beginJobHook, endJobHook, initBuffersHook, sizeBuffersHook, renderActorHook,
                          setupModelsHook, isActiveViewHook, impostorQueriesHook, setupDisplayHook, submitHook,
                          renderOffscreenHook, hudPlaceHook, submitSceneHook, fitCascadesHook, csmFitHook, viewScaleHook})
            if (s == MH_OK)
                s = enableHook(hook);
        if (s == MH_OK)
            s = MH_EnableHook(maintainHook);
        if (s != MH_OK) {
            for (auto hook : {updateHook, copyFinalHook, toneHook, copyJobHook, beginJobHook, endJobHook,
                              initBuffersHook, sizeBuffersHook, renderActorHook, setupModelsHook, isActiveViewHook,
                              impostorQueriesHook, setupDisplayHook, submitHook, renderOffscreenHook, hudPlaceHook,
                              submitSceneHook, fitCascadesHook, csmFitHook, viewScaleHook})
                disableHook(hook);
            result = 1300 + s;
            break;
        }
        enabled = true;
        native_hud::panelHooked(hudPlaceHook != nullptr); // 0.0.70: never in SM2 (SM2_STEREO_HUD 0)
        SpidyShadowCameraData.fitHook = fitCascadesHook ? 1 : 0; // Update 31
        SpidyFarShadowsData.hooked = csmFitHook ? 1 : 0; // Update 35
        if (!csmFitHook && farShadowsMode.load())
            farShadowsSet(spidy::far_shadows::off, spidy::far_shadows::noHook, true);
        else if (csmFitHook && farShadowsMode.load())
            farShadowsSet(farShadowsMode.load(), spidy::far_shadows::none, false); // judge it
        SpidySharedLodData.hooked = viewScaleHook ? 1 : 0; // Update 36
        if (!viewScaleHook && sharedLodMode.load())
            sharedLodSet(spidy::shared_lod::off, spidy::shared_lod::noHook, true);
        else if (sharedLodMode.load())
            sharedLodSet(sharedLodMode.load(), spidy::shared_lod::none, false); // judge it
        if (bloomMode.load())
            bloomSet(bloomMode.load(), spidy::eye_bloom::none, false); // gate again, judge it
        {   // Update 37
            auto& g = SpidyViewGatesData;
            g.fadeReady = lodFadeKnown ? 1 : 0;
            g.impostorsInstalled = impostors ? 1 : 0;
            if (!lodFadeKnown && lodFadeMode.load()) {
                lodFadeMode = spidy::view_gates::fadeOff;
                lodFadeWhy = spidy::view_gates::fadeNoHook;
            }
        }
        // The rest of the HUD on the panel (native_hud.cpp). Without it the
        // panel still goes in front of the head; SpidyHudData says why not.
        if (SM2_STEREO_HUD && (config.createViews & 4)) // SM2TODO: native_hud addresses unknown
            native_hud::start(base);
        // Update 38.4: the MIRROR PROBE, read-only (mirror_probe.cpp), from here to the end.
        spidy::mirror_probe::start(base);
    } while (false);
    AcquireSRWLockExclusive(&telemetry);
    InterlockedIncrement64(&SpidyStereoData.sequence);
    SpidyStereoData.state = result ? 3 : 1;
    SpidyStereoData.error = result;
    InterlockedIncrement64(&SpidyStereoData.sequence);
    ReleaseSRWLockExclusive(&telemetry);
    ReleaseSRWLockExclusive(&lifecycle);
    return result;
}
extern "C" __declspec(dllexport) DWORD WINAPI SpidyStop(void*) {
    AcquireSRWLockExclusive(&lifecycle);
    stopRequested = true;
    DWORD result{};
    AcquireSRWLockExclusive(&commandLock);
    commandDeadline = 0;
    ReleaseSRWLockExclusive(&commandLock);
    AcquireSRWLockExclusive(&creation);
    enabled = false;
    native_hud::panelHooked(false);
    ReleaseSRWLockExclusive(&creation);
    hiddenHud = 0;
    // Keep maintenance installed until both owned views are excluded from jobs
    // and the main thread has restored the hero's native visibility.
    const auto limit = GetTickCount64() + 5000;
    while ((live || hiddenHero.load()) && GetTickCount64() < limit)
        Sleep(10);
    // The game's own view renders again (placeEyes no longer runs).
    restGameView(pointer(base + 0xc2ca220), false);
    // The HUD's texture goes back to the game's size while frames still run
    // (SpidyHudData's restored says whether it did).
    native_hud::stop(1000);
    native_shadow_distance::stop(1500); // Update 33: the game's shadow value back
    // Update 36: the game's bloom gate and the eyes' detail multipliers back (the
    // modes stay for the next start, which applies them again).
    bloomGateWrite(false);
    sharedLodRestore(0);
    sharedLodRestore(1);
    windowPopRestore(); // 0.0.73 test 6 WINDOW POP FIX
    if (live)
        result = 1401;
    else if (hooked) {
        enabled = false;
        for (auto hook : {maintainHook, updateHook, copyFinalHook, toneHook, copyJobHook, beginJobHook,
                          endJobHook, initBuffersHook, sizeBuffersHook, renderActorHook, setupModelsHook,
                          isActiveViewHook, impostorQueriesHook, setupDisplayHook, submitHook, renderOffscreenHook,
                          hudPlaceHook, submitSceneHook, fitCascadesHook, csmFitHook, viewScaleHook}) {
            auto s = disableHook(hook);
            if (s != MH_OK && s != MH_ERROR_DISABLED)
                result = 1500 + s;
        }
    }
    // Maintenance normally restores the avatar. If the game stopped running
    // frames (for example during a load), restore the same live instance here.
    if (const auto previous = hiddenHero.load()) {
        Sleep(50); // let an in-flight maintenance call leave the detour
        uint32_t handle{}, flags{};
        if (registered(previous, handle) && handle == hiddenHandle && read(previous + 0x5c, &flags, 4) &&
            (flags & 0x20)) {
            reinterpret_cast<Visibility>(base + 0x2cfba50)(reinterpret_cast<void*>(previous));
            AcquireSRWLockExclusive(&telemetry);
            InterlockedIncrement64(&SpidyAppearanceData.sequence);
            ++SpidyAppearanceData.nativeRestores;
            InterlockedIncrement64(&SpidyAppearanceData.sequence);
            ReleaseSRWLockExclusive(&telemetry);
        }
        hiddenHero = 0;
        hiddenHandle = 0;
    }
    AcquireSRWLockExclusive(&telemetry);
    InterlockedIncrement64(&SpidyStereoData.sequence);
    SpidyStereoData.state = result ? 3 : 2;
    SpidyStereoData.error = result;
    InterlockedIncrement64(&SpidyStereoData.sequence);
    ReleaseSRWLockExclusive(&telemetry);
    ReleaseSRWLockExclusive(&lifecycle);
    return result;
}
BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH)
        DisableThreadLibraryCalls(instance);
    return TRUE;
}
// Diagnostic: 1 places the eyes in view maintenance, as before October 5; 0
// restores placement before the frame's render jobs. tools/probe_eye_frames.py
// compares the two in the running game.
extern "C" __declspec(dllexport) DWORD WINAPI SpidyEyePlacement(void* late) {
    lateEyes = late != nullptr;
    return 0;
}
// Diagnostic: 1 exposes each eye from its own adapted luminance, as before
// October 10's sixth build; 0 shares the main view's again.
// tools/probe_exposure.py compares the two in the running game.
extern "C" __declspec(dllexport) DWORD WINAPI SpidyEyeExposure(void* own) {
    ownExposure = own != nullptr;
    return 0;
}
// Diagnostic: 1 counts the render commands queued from now on (SpidyCommandData,
// cleared first); 0 stops. The hook stays in place once made.
extern "C" __declspec(dllexport) DWORD WINAPI SpidyCommandTrace(void* on) {
    if (!base)
        return 4201;
    if (on && !enqueueHook) {
        enqueueHook = reinterpret_cast<void*>(base + 0x2c77810);
        auto s = MH_CreateHook(enqueueHook, reinterpret_cast<void*>(enqueueCommand),
                               reinterpret_cast<void**>(&originalEnqueue));
        if (s == MH_OK)
            s = MH_EnableHook(enqueueHook);
        if (s != MH_OK) {
            enqueueHook = nullptr;
            return 4200 + s;
        }
    }
    SpidyCommandData.tracing = 0;
    if (on) {
        std::memset(SpidyCommandData.counts, 0, sizeof(SpidyCommandData.counts));
        SpidyCommandData.frames = 0;
        SpidyCommandData.tracing = 1;
    }
    return 0;
}
// The eyes' render size and temporal anti-aliasing: bits 0-7 the percent of
// the eye's size per side its scene is rendered at (25-100; taken when the
// eyes are made, 0 leaves it), bits 8-15 eyeTemporal's bits. The VR worker
// sets them before it starts the views, as tools/probe_vr_load.py does.
// The eyes' shadow sets (eyeShadows above): 0 each eye makes its own, as
// before October 11; bits 1, 2 and 4 give the eye submitted second the first
// one's key-light cascades, light set and +30 part. Takes effect at the next
// frame. SpidyShadowData has what each eye made or took.
extern "C" __declspec(dllexport) DWORD WINAPI SpidyEyeShadows(void* mode) {
    const auto value = reinterpret_cast<uintptr_t>(mode);
    if (value > 7)
        return 4301;
    // SM2 port: SM2VR 0.0.27's export (DLL +b020) checks the value the same
    // way, records it, and always stores 0 here: each eye makes its own shadows
    // (sharing them crashed the render thread; SM2VR 0.0.4).
    eyeShadows = 0;
    SpidyShadowData.mode = static_cast<uint32_t>(value);
    return 0;
}
// How far away the far buildings draw their full models while an eye stands in
// for the resting game view (impostorReach above): percent of the game's own
// distance, 50-400; 150 unless set. Takes effect at the next frame.
// Update 15: the reach in force, for the wrist hologram (game_xr.cpp).
extern "C" uint32_t SpidyImpostorReachNow() {
    return impostorReach.load();
}
// Update 38.3 FAR WINDOWS (farWindowsPercent above): 100-300; 4402 out of range. Live.
extern "C" __declspec(dllexport) DWORD WINAPI SpidyFarWindows(void* percent) {
    const auto value = reinterpret_cast<uintptr_t>(percent);
    if (value < 100 || value > 300)
        return 4402;
    farWindowsPercent = static_cast<uint32_t>(value);
    return 0;
}
// 0.0.73 test 6 WINDOW POP FIX (window_pop.hpp): 0 off, 1 on (the default); 4475 otherwise. Live.
extern "C" __declspec(dllexport) DWORD WINAPI SpidyWindowPopOn(void* on) {
    const auto value = reinterpret_cast<uintptr_t>(on);
    if (value > 1)
        return 4475;
    windowPopOn = value != 0;
    return 0;
}
// Its writes and the boost in force (the console's line).
extern "C" uint64_t SpidyWindowPopWrites() { return windowPopWrites.load(); }
extern "C" float SpidyWindowPopBoost() { return gameLodApplied.load(); }
// 0.0.73 LIGHT CHECKS (eye_queries.hpp): 0 off, 1 on (the default); 4470 otherwise. Read when the eyes are made.
extern "C" __declspec(dllexport) DWORD WINAPI SpidyLightChecksOn(void* on) {
    const auto value = reinterpret_cast<uintptr_t>(on);
    if (value > 1)
        return 4470;
    lightChecksOn = static_cast<uint32_t>(value);
    return 0;
}
// 0.0.73 GLOW CHECK (eye_queries.hpp glow_check): 0 GAME, 1 ALWAYS, 2 HIDDEN; live, any thread. 4471 for a
// bad value, 4472 when a site's bytes are neither the game's nor ours (nothing written, GAME kept there).
SRWLOCK glowLock = SRWLOCK_INIT;
extern "C" __declspec(dllexport) DWORD WINAPI SpidyGlowCheckMode(void* mode) {
    namespace gc = spidy::glow_check;
    const auto value = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(mode));
    if (reinterpret_cast<uintptr_t>(mode) >= gc::modes)
        return 4471;
    AcquireSRWLockExclusive(&glowLock);
    DWORD result = 0;
    // Put back first, then patch, so the two sites are never both jumps.
    for (const uintptr_t rva : value == gc::hidden ? std::array{gc::alwaysRva, gc::hiddenRva}
                                                   : std::array{gc::hiddenRva, gc::alwaysRva}) {
        const auto at = reinterpret_cast<uint16_t*>(base + rva);
        uint16_t now{};
        const uint16_t want = gc::wantAt(rva, value);
        const uint16_t stock = rva == gc::alwaysRva ? gc::alwaysStock : gc::hiddenStock;
        if ((reinterpret_cast<uintptr_t>(at) & 1) || !read(base + rva, &now, 2) || (now != stock && now != gc::jump)) {
            result = 4472;
            continue;
        }
        if (now == want)
            continue;
        DWORD protection{};
        if (!VirtualProtect(at, 2, PAGE_EXECUTE_READWRITE, &protection)) {
            result = 4472;
            continue;
        }
        std::atomic_ref<uint16_t>(*at).store(want);
        DWORD unused{};
        VirtualProtect(at, 2, protection, &unused);
        FlushInstructionCache(GetCurrentProcess(), at, 2);
    }
    ReleaseSRWLockExclusive(&glowLock);
    return result;
}
// LIGHT CHECKS readback: the state (low byte, see lightChecksState) and the left / right eyes' query pools
// (bits 8-19 and 20-31, each capped at 4095) as the game sized them.
extern "C" __declspec(dllexport) DWORD WINAPI SpidyLightChecksNow(void*) {
    const auto pool = [](int i) { return static_cast<uint32_t>(std::clamp(lightChecksRoom[i].load(), 0, 4095)); };
    return (lightChecksState.load() & 0xff) | pool(0) << 8 | pool(1) << 20;
}
// 0.0.73 test 2 SHADOW MATCH: 0 off, 1 on; 4404 otherwise. Live; *fits / *skipped for the COMBAT record.
extern "C" __declspec(dllexport) DWORD WINAPI SpidyShadowMatchOn(void* on) {
    const auto value = reinterpret_cast<uintptr_t>(on);
    if (value > 1)
        return 4404;
    shadowMatchOn = static_cast<uint32_t>(value);
    return 0;
}
extern "C" void SpidyShadowMatchStats(uint64_t* fits, uint64_t* skipped) {
    if (fits)
        *fits = shadowMatchFits.load();
    if (skipped)
        *skipped = shadowMatchSkipped.load();
}
// 0.0.73 test 2 CULL MARGIN: 0-30 degrees on every side of the head's lens; 4403 out of range. Live.
extern "C" __declspec(dllexport) DWORD WINAPI SpidyCullMarginDegrees(void* degrees) {
    const auto value = reinterpret_cast<uintptr_t>(degrees);
    if (value > recording_fov::cullMarginMaxDegrees)
        return 4403;
    cullMarginDegrees = static_cast<uint32_t>(value);
    return 0;
}
// Its counts for the COMBAT record; *calls bit 63: write 2's bytes matched (farWindowsReady).
extern "C" void SpidyFarWindowsStats(uint64_t* calls, uint64_t* writes, float* gameMetres, float* ourMetres) {
    if (calls)
        *calls = farWindowsCalls.load() | (farWindowsReady ? 1ull << 63 : 0);
    if (writes)
        *writes = farWindowsWrites.load();
    if (gameMetres)
        *gameMetres = farWindowsGame.load();
    if (ourMetres)
        *ourMetres = farWindowsOurs.load();
}
extern "C" __declspec(dllexport) DWORD WINAPI SpidyImpostorReach(void* percent) {
    const auto value = reinterpret_cast<uintptr_t>(percent);
    if (value < 50 || value > 400)
        return 4401;
    impostorReach = static_cast<uint32_t>(value);
    return 0;
}
// Update 28: EYE LOD (eyeLod above): 1 on, 0 off. A failed EYE LOD stays off
// for the session. SpidyEyeLodNow: 0 off, 1 on, 2 off (failed), for the hologram.
extern "C" uint32_t SpidyEyeLodNow() {
    return eyeLodState.load() == 2 ? 2 : eyeLodWanted.load() ? 1 : 0;
}
// Update 28.1: why EYE LOD failed (native_view::EyeLodReason), for the hologram.
extern "C" uint32_t SpidyEyeLodReason() {
    return eyeLodState.load() == 2 ? eyeLodWhy.load() : 0;
}
extern "C" __declspec(dllexport) DWORD WINAPI SpidyEyeLod(void* on) {
    const auto value = reinterpret_cast<uintptr_t>(on);
    if (value > 1)
        return 4402;
    if (eyeLodState.load() == 2)
        return 4403;
    eyeLodWanted = static_cast<uint32_t>(value);
    return 0;
}
// Update 31: SHADOW CAMERA (shadow_camera.hpp): 0 OFF, 1 STOCK NEAR, 2 FLAT.
// Each change is logged (time, mode, frames the last mode was applied). A press
// clears the reason it last went back to OFF on its own.
extern "C" __declspec(dllexport) DWORD WINAPI SpidyShadowCamera(void* mode) {
    const auto value = reinterpret_cast<uintptr_t>(mode);
    if (value >= shadowCamera::modes)
        return 4501;
    shadowCameraWhy = 0;
    shadowCameraMode = static_cast<uint32_t>(value);
    shadowCameraLog(static_cast<uint32_t>(value), 0);
    return 0;
}
// Update 35: FAR SHADOWS (far_shadows.hpp): 0 OFF, 1 EYES, 2 EYES+CACHE; 3 the next
// (the wrist's press); 4 OFF because the frame rate halved (game_xr's guard). Refused
// (4701) above, and (4702) where the fit differs. Live, never kept.
extern "C" __declspec(dllexport) DWORD WINAPI SpidyFarShadows(void* mode) {
    namespace fs = spidy::far_shadows;
    const auto value = reinterpret_cast<uintptr_t>(mode);
    if (value > fs::slowOff)
        return 4701;
    if (value == fs::slowOff) {
        if (farShadowsMode.load())
            farShadowsSet(fs::off, fs::slow, true);
        return 0;
    }
    if (!farShadowsReady && hooked)
        return 4702;
    farShadowsSet(value == fs::nextMode ? fs::next(farShadowsMode.load()) : static_cast<uint32_t>(value), fs::none,
                  false);
    return 0;
}
// far_shadows::pack(mode, why it last changed on its own).
extern "C" uint32_t SpidyFarShadowsNow() {
    return spidy::far_shadows::pack(farShadowsMode.load(), farShadowsWhy.load() & 0xff);
}
// Update 36: EYE BLOOM (eye_bloom.hpp): 0 OFF, 1 COPY, 2 GATE; 3 the next (the
// wrist's press). Refused (4801) above. GATE where the gate's bytes differ goes
// back to OFF (N/A) on its own. Live, never kept.
extern "C" __declspec(dllexport) DWORD WINAPI SpidyEyeBloom(void* mode) {
    namespace eb = spidy::eye_bloom;
    const auto value = reinterpret_cast<uintptr_t>(mode);
    if (value > eb::nextMode)
        return 4801;
    bloomSet(value == eb::nextMode ? eb::next(bloomMode.load()) : static_cast<uint32_t>(value), eb::none, false);
    return 0;
}
// eye_bloom::pack(mode, why it last changed on its own).
extern "C" uint32_t SpidyEyeBloomNow() {
    return spidy::eye_bloom::pack(bloomMode.load(), bloomWhy.load() & 0xff);
}
// Update 36: SHARED LOD (shared_lod.hpp): 0 OFF, 1 GAME VIEW, 2 HIGHER; 3 the next
// (the wrist's press). Refused (4901) above, and (4902) where 0x2c9a000 differs.
// Live, never kept.
extern "C" __declspec(dllexport) DWORD WINAPI SpidySharedLod(void* mode) {
    namespace sl = spidy::shared_lod;
    const auto value = reinterpret_cast<uintptr_t>(mode);
    if (value > sl::nextMode)
        return 4901;
    if (!viewScaleHook && hooked)
        return 4902;
    sharedLodSet(value == sl::nextMode ? sl::next(sharedLodMode.load()) : static_cast<uint32_t>(value), sl::none,
                 false);
    return 0;
}
// Update 37 (view_gates.hpp), all live, never kept by the DLL (live.txt keeps them).
// SpidyViewGate(selector << 16 | value): 0 BLOOM FX (0 OFF, 1 LENS, 2 LENS+, 3 next),
// 1 LOD FADE (0 OFF, 1 LEFT, 2 BOTH, 3 next), 2 LOD REACH (100-300 percent), 3 IMPOSTORS
// (0/1). Refused: 5001 (bad selector or value), 5002 LOD FADE where its bytes differ,
// 5003 IMPOSTORS where the stand-in was not installed.
extern "C" __declspec(dllexport) DWORD WINAPI SpidyViewGate(void* argument) {
    namespace vg = spidy::view_gates;
    const auto packed = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(argument));
    const uint32_t selector = packed >> 16, value = packed & 0xffff;
    switch (selector) {
    case 0:
        if (value > vg::nextFx)
            return 5001;
        bloomFxMode = value == vg::nextFx ? vg::fxNext(bloomFxMode.load()) : value;
        return 0;
    case 1:
        if (value > vg::nextFade)
            return 5001;
        if (hooked && !lodFadeKnown)
            return 5002;
        lodFadeMode = value == vg::nextFade ? vg::fadeNext(lodFadeMode.load()) : value;
        lodFadeWhy = vg::fadeFine;
        fadeCalls = 0;
        return 0;
    case 2:
        if (value < vg::minReach || value > vg::maxReach)
            return 5001;
        lodReachPercent = value;
        return 0;
    case 3:
        if (value > 1)
            return 5001;
        if (hooked && !impostors)
            return 5003;
        impostorsOn = value != 0;
        return 0;
    default:
        return 5001;
    }
}
// The four: fx | fade << 4 | fadeWhy << 8 | impostorsOn << 12 | impostors installed << 13 |
// fade ready << 14 | reach << 16.
extern "C" uint32_t SpidyViewGatesNow() {
    return bloomFxMode.load() | lodFadeMode.load() << 4 | (lodFadeWhy.load() & 0xf) << 8 |
           (impostorsOn.load() ? 1u : 0u) << 12 | (impostors ? 1u : 0u) << 13 | (lodFadeKnown ? 1u : 0u) << 14 |
           lodReachPercent.load() << 16;
}
// shared_lod::pack(mode, why it last changed on its own).
extern "C" uint32_t SpidySharedLodNow() {
    return spidy::shared_lod::pack(sharedLodMode.load(), sharedLodWhy.load() & 0xff);
}
// The mode, and in bits 8-15 why it last went back to OFF (shadow_camera::Reason).
extern "C" uint32_t SpidyShadowCameraNow() {
    return shadowCameraMode.load() | (shadowCameraWhy.load() & 0xff) << 8;
}
extern "C" __declspec(dllexport) DWORD WINAPI SpidyEyeTemporal(void* packed) {
    const auto value = reinterpret_cast<uintptr_t>(packed);
    const uint32_t percent = value & 0xff;
    if (percent && (percent < 25 || percent > 100))
        return 4101;
    if (percent)
        eyeRenderPercent = percent;
    // SM2 port: SM2VR 0.0.27's export (DLL +b040) keeps the size and always
    // stores 0 here: no jittered lens, no eye temporal AA, no extra reflection
    // targets (SM2VR 0.0.3/0.0.5). Update 34: bit 0x10 (-EngineTaa) is kept: the
    // eyes' buffers get the TAA targets for ENGINE TAA (engineTaaBuffers).
    eyeTemporal = static_cast<uint32_t>(value >> 8) & 0x10;
    return 0;
}
// Update 34: ENGINE TAA. 1 on (refused, 4602, when engineTaaBlocked() says why), 0
// off, 2 off because the frame rate halved (game_xr's guard). SpidyEngineTaaNow:
// engine_taa::pack(state, reason).
extern "C" __declspec(dllexport) DWORD WINAPI SpidyEngineTaa(void* on) {
    namespace et = spidy::engine_taa;
    const auto value = reinterpret_cast<uintptr_t>(on);
    if (value > 3)
        return 4601;
    if (value == 3) { // Update 37: auto
        engineTaaAuto = true;
        return 0;
    }
    if (value != 1) {
        engineTaaAuto = false;
        engineTaaOff(value == 2 ? et::slow : et::none);
        if (value == 0)
            engineTaaWhy = 0;
        return 0;
    }
    if (const uint32_t why = engineTaaBlocked()) {
        engineTaaWhy = why;
        ++engineTaaRefusals;
        engineTaaLastRefusal = why;
        char line[128];
        std::snprintf(line, sizeof line, "ENGINE TAA: refused, %s\n", et::reasonText(why));
        OutputDebugStringA(line);
        return 4602;
    }
    engineTaaWhy = 0;
    if (!engineTaaLive.exchange(true)) {
        ++engineTaaOns;
        OutputDebugStringA("ENGINE TAA: on\n");
    }
    return 0;
}
// Update 34.2: ENGINE TAA's jitter (engine_taa::JitterMode), live, never kept: 0 ours,
// 1 the game camera's, 2 in render pixels, 3 none; 4 the next one (the wrist's press:
// Update 35, OURS <-> NONE only).
// Refused (4603) otherwise. SpidyEngineTaaJitterNow: the mode in force.
extern "C" __declspec(dllexport) DWORD WINAPI SpidyEngineTaaJitter(void* mode) {
    namespace et = spidy::engine_taa;
    const auto value = reinterpret_cast<uintptr_t>(mode);
    if (value > et::jitterModes)
        return 4603;
    // 0.0.73 TAA SHAKE: the wrist's press (value 4) steps TAA JITTER FULL -> HALF -> OFF (the scale, OURS);
    // live.txt taa_jitter 0..3 still sets the mode, taa_jitter_scale the scale.
    const bool press = value == et::jitterModes;
    const uint32_t next = press ? static_cast<uint32_t>(et::ours) : static_cast<uint32_t>(value);
    if (engineTaaJitterMode.exchange(next) != next)
        ++engineTaaModeChanges;
    if (press) {
        const uint32_t scale = et::pressNextScale(engineTaaJitterScale.load());
        engineTaaJitterScale = scale;
        ++engineTaaModeChanges;
    }
    char level[16], line[96];
    et::levelText(spidy::image_style::jitterMode(imageStyle.load(), next), engineTaaJitterScale.load(), level,
                  sizeof level);
    std::snprintf(line, sizeof line, "ENGINE TAA: jitter %s (%s, %u%%)\n", level, et::jitterName(next),
                  engineTaaJitterScale.load());
    OutputDebugStringA(line);
    return 0;
}
// The mode in force (0.0.73 test 4) and, 0.0.73 TAA SHAKE, the scale (engine_taa::packJitter).
extern "C" uint32_t SpidyEngineTaaJitterNow() {
    return spidy::engine_taa::packJitter(spidy::image_style::jitterMode(imageStyle.load(), engineTaaJitterMode.load()),
                                         engineTaaJitterScale.load());
}
// 0.0.73 test 4 IMAGE STYLE, shown as GAME TAA (image_style.hpp): 0 SPIDYVR, 1 OFF; refused (4605) otherwise.
// game_xr's controller sends it when the option changes; it does the rest (ENGINE TAA, the clean picture).
extern "C" __declspec(dllexport) DWORD WINAPI SpidyImageStyle(void* style) {
    const auto value = reinterpret_cast<uintptr_t>(style);
    if (value >= spidy::image_style::styles)
        return 4605;
    if (imageStyle.exchange(static_cast<uint32_t>(value)) != value)
        OutputDebugStringA(value == spidy::image_style::spidyvr ? "GAME TAA: SPIDYVR (jitter never NONE)\n"
                                                                : "GAME TAA: OFF\n");
    return 0;
}
// 0.0.73 test 4: why ENGINE TAA could not go on now (engine_taa::Reason; none: it could), without
// trying (SpidyEngineTaa(1) counts and logs every refusal).
extern "C" uint32_t SpidyEngineTaaBlocked() {
    return engineTaaBlocked();
}
// Update 34.2: the jitter's scale, percent (0-200, 100 as made); refused (4604) above.
extern "C" __declspec(dllexport) DWORD WINAPI SpidyEngineTaaJitterScale(void* percent) {
    const auto value = reinterpret_cast<uintptr_t>(percent);
    if (value > spidy::engine_taa::maximumJitterScale)
        return 4604;
    engineTaaJitterScale = static_cast<uint32_t>(value);
    return 0;
}
extern "C" uint32_t SpidyEngineTaaNow() {
    namespace et = spidy::engine_taa;
    if (engineTaaLive.load())
        return et::pack(et::on, 0);
    const uint32_t blocked = engineTaaBlocked();
    if (blocked && blocked != et::noEyes)
        return et::pack(et::unavailable, blocked);
    return et::pack(et::off, engineTaaWhy.load());
}
extern "C" float SpidyFlatAspect() {
    return flatAspect.load();
}
uint32_t spidy::native_hud::eyes(Head& out) {
    if (!enabled || !(config.createViews & 4) || lateEyes || GetCurrentThreadId() != mainThread.load())
        return 0;
    // The game projects its markers before it moves the player (earlyHead).
    const auto& head = earlyHead();
    if (!head.controlled)
        return 0;
    if (head.command.enabled >= 2)
        return 2;
    if (!head.viewed)
        return 0;
    out.pose = head.pose;
    out.panel = head.panel;
    out.halfWidth = halfWidth(size());
    return 1;
}
void spidy::native_appearance::setPlayerRecord(uint64_t record) {
    playerRecord = record;
}
bool spidy::native_eyes::renderOffset(uint64_t frame, Vec3& offset) {
    AcquireSRWLockShared(&offsetLock);
    const auto entry = offsets[frame % offsets.size()];
    ReleaseSRWLockShared(&offsetLock);
    offset = entry.generation == frame ? entry.offset : Vec3{};
    return frame && entry.generation == frame;
}
bool spidy::native_eyes::renderWebStart(uint64_t frame, unsigned hand, Vec3& start) {
    AcquireSRWLockShared(&offsetLock);
    const auto entry = offsets[frame % offsets.size()];
    ReleaseSRWLockShared(&offsetLock);
    if (!frame || entry.generation != frame || hand >= 2 || !(entry.ropes & (1u << hand)))
        return false;
    start = entry.ropeStarts[hand];
    return true;
}
void spidy::native_eyes::reportWebGap(float metres) {
    if (!std::isfinite(metres) || metres < 0)
        return;
    AcquireSRWLockExclusive(&telemetry);
    auto& data = SpidyAppearanceData;
    InterlockedIncrement64(&data.sequence);
    ++data.webGapFrames;
    data.webGapLast = metres;
    data.webGapMax = std::max(data.webGapMax, metres);
    data.webGapSum += metres;
    InterlockedIncrement64(&data.sequence);
    ReleaseSRWLockExclusive(&telemetry);
}
// Version 0.0.4 MIRROR VIEW TEST: the option (game_xr.cpp) and the record's copy.
extern "C" void SpidyMirrorViewWant(uint32_t on) { SpidyMirrorViewWantImpl(on); }
extern "C" void SpidyMirrorViewRead(spidy::mirror_view::Stats* out) {
    if (!out)
        return;
    AcquireSRWLockShared(&mirrorLock);
    *out = mirrorStats;
    ReleaseSRWLockShared(&mirrorLock);
    out->wanted = mirrorWanted.load();
}
