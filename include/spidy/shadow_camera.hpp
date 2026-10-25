#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
//
// Update 31: SHADOW CAMERA, a live GRAPHICS-page test (off by default, never kept).
// The eyes draw shadows from the frame-wide key-light setup the game fits for its
// own view; immersive, that view is on the head with a lens covering both eyes and
// NEAR CLIP's near plane (2 cm by default; the flat game's is 10 cm). Each press
// steps to the next way of fitting that view, so the hard shadow line can be
// watched while it changes:
//   OFF         as before: the head view, NEAR CLIP's near plane.
//   STOCK NEAR  the head view with the flat camera's own near plane (the eyes keep
//               NEAR CLIP): tests whether cascade splits follow the near plane.
//   FLAT        the game view stays the flat camera behind the hero (as before
//               the port aligned it): tests whether coverage follows the game view.
//               Culling then follows the flat camera too, so it is a test only.
// What the old idea (rewrite HEAD-ONLY view-proj records at upload fn 0x2ccbfc0)
// could not do is written in BUILD-SM2.md section 41.
//
// Update 32: the Oct 23 run applied every mode (11 changes, no reverts) and the
// picture did not change, and 0x2dd8d20 never ran for the game view or an eye
// (4950 calls, all "other"). SHADOW CAMERA left the hologram and live.txt (BUILD-SM2.md
// section 42); the native modes stay for the record (default OFF, nothing sets them).
// FitViews records what the views 0x2dd8d20 does run for look like: their pose
// against the flat camera's and the head's, who calls it, and how its camera moves.
#include <cmath>
#include <cstdint>

namespace spidy::shadow_camera {

enum Mode : uint32_t { off = 0, stockNear = 1, flat = 2 };
inline constexpr uint32_t modes = 3;

inline uint32_t next(uint32_t mode) { return (mode + 1) % modes; }
inline const char* modeName(uint32_t mode) {
    switch (mode) {
    case stockNear:
        return "STOCK NEAR";
    case flat:
        return "FLAT CAMERA";
    default:
        return "OFF";
    }
}
inline bool flatGameView(uint32_t mode) { return mode == flat; }
inline bool useStockNear(uint32_t mode) { return mode == stockNear; }

// Why it went back to OFF on its own.
enum Reason : uint32_t { none = 0, lensRejected = 1, nearOutOfRange = 2 };
inline const char* reasonText(uint32_t reason) {
    switch (reason) {
    case lensRejected:
        return "the engine rejected the head view";
    case nearOutOfRange:
        return "the flat near plane was out of range";
    default:
        return "";
    }
}
// The flat camera's near plane is used only within this range (metres).
inline bool nearUsable(float nearZ) { return nearZ >= .005f && nearZ <= 1.f; }

inline constexpr const char* help = "Test: fit shadows to the head view, the flat near plane, or the flat camera.";

struct Event {
    uint64_t ms{};      // GetTickCount64 at the change
    uint32_t mode{};    // the mode it changed to
    uint32_t reason{};  // Reason when it went back to OFF on its own
    uint64_t applied{}; // frames the previous mode was applied
};

// The game view's own key-light cascades (submitScene result +28 KeyLightContext,
// counts at +d/+f, box half sizes at +170 and transform at +198 of each 0x3e0
// entry from +150). Offsets from the Remastered port; unconfirmed in SM2 (a zero
// count says so). centre is the transform's translation.
struct GameCascades {
    uint64_t samples{};
    uint32_t counts[2]{};
    uint32_t readable{}, pad{};
    float headPos[3]{};
    float nearZ{}; // the game view's near plane this frame
    float extent[4][3]{};
    float centre[4][3]{};
};

inline constexpr uint32_t maxEvents = 32;

// Update 32: what a view passed to 0x2dd8d20 is. Poses are the renderer's rows
// (right, down, forward, position): pos = m[12..14], forward = m[8..10].
enum FitKind : uint32_t { fitUnreadable = 0, fitFlat = 1, fitHead = 2, fitOther = 3 };
inline constexpr uint32_t fitKinds = 4;
inline const char* fitKindName(uint32_t k) {
    switch (k) {
    case fitFlat:
        return "flat";
    case fitHead:
        return "head";
    case fitOther:
        return "other";
    default:
        return "unreadable";
    }
}
// Same camera: within 25 cm and 2 degrees.
inline bool samePose(const float* a, const float* b) {
    const float dx = a[12] - b[12], dy = a[13] - b[13], dz = a[14] - b[14];
    const float d = std::sqrt(dx * dx + dy * dy + dz * dz);
    const float c = a[8] * b[8] + a[9] * b[9] + a[10] * b[10];
    return std::isfinite(d) && d <= .25f && c >= .99939f;
}
// flat / head may be null (not known yet); flat wins a tie (head and flat agree
// only on the flat screen).
inline uint32_t classifyFit(bool valid, const float* view, const float* flat, const float* head) {
    if (!valid || !view)
        return fitUnreadable;
    if (flat && samePose(view, flat))
        return fitFlat;
    if (head && samePose(view, head))
        return fitHead;
    return fitOther;
}
// Compact pose: position and forward.
inline void compact(const float* m, float out[6]) {
    for (int i = 0; i < 3; ++i) {
        out[i] = m[12 + i];
        out[3 + i] = m[8 + i];
    }
}
struct FitSample {
    uint64_t ms{};
    float view[6]{}, flat[6]{}, head[6]{}; // position, forward (zeros: unknown)
    float nearZ{}, farZ{};
    uint32_t kind{}, caller{}; // FitKind; caller RVA (return address into the game)
};
inline constexpr uint32_t fitSlots = 8, fitSamples = 32;
inline constexpr uint64_t fitSampleMs = 2000;
struct FitViews {
    uint64_t byKind[fitKinds]{};
    uint32_t caller[fitSlots]{}, callerCalls[fitSlots]{}; // distinct callers (RVAs)
    uint64_t view[fitSlots]{};
    uint32_t viewCalls[fitSlots]{};
    uint32_t samples{}, overflow{}; // samples ever taken (ring sample[i % fitSamples]); callers/views past the slots
    uint64_t lastSampleMs{};
    FitSample sample[fitSamples]{};
};
// Count v in a small distinct-value table; false when it is full (overflow).
template <typename T> inline bool countDistinct(T* values, uint32_t* calls, uint32_t slots, T v) {
    for (uint32_t i = 0; i < slots; ++i) {
        if (calls[i] && values[i] == v) {
            ++calls[i];
            return true;
        }
        if (!calls[i]) {
            values[i] = v;
            calls[i] = 1;
            return true;
        }
    }
    return false;
}

// SpidyShadowCameraData (stereo_probe.cpp), read by tools/shadow_camera.py.
struct Data {
    uint32_t magic = 0x4d414353, version = 2, bytes = sizeof(Data), mode{};
    int64_t sequence{};
    uint32_t reason{}, toggles{};
    uint64_t applied[modes]{};                 // frames each mode was applied while immersive
    uint64_t fitGame{}, fitEye{}, fitOther{};  // fitCascades (0x2dd8d20) calls by view
    uint32_t events{}, fitHook{};              // events ever logged; 1 if 0x2dd8d20 was hooked
    Event event[maxEvents]{};                  // ring: event[i % maxEvents]
    GameCascades game{};
    FitViews fit{}; // Update 32
};
static_assert(sizeof(Event) == 24 && sizeof(GameCascades) == 136 && sizeof(FitSample) == 96);
static_assert(sizeof(FitViews) == 32 + 64 + 64 + 32 + 16 + 32 * 96 && sizeof(Data) == 992 + sizeof(FitViews));

} // namespace spidy::shadow_camera
