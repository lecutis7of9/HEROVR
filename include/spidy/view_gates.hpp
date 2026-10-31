#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
//
// Update 37: the eyes' remaining view gates, each live with a plan B in the same build.
// Exe-confirmed (Spider-Man2.exe 2.810.0.0, by disassembly; docs/main-view-gates-audit.md):
// - BLOOM FX. The view-settings builder 0x2c27ec0 ends (0x2c29090..0x2c2947b) with two
//   groups of fields the GAME ITSELF zeroes for some views: the "view flag" group under
//   [r15+0x438] bit 1 (0x2c293ca..0x2c293fa: +0x2b8, +0x2c4, +0x2cc, +0x2fc, +0x328,
//   +0x34c, +0x358 qword) and the per-view "bl" group (+0x1e0, +0x1f0 qword, +0x2d4,
//   +0x2dc). Both are combinations the engine already renders, so zeroing them in an eye's
//   settings (rebuilt every frame) is safe and undone by turning it off. CANDIDATE: which
//   screen effect each field is (lens dirt, flares, vignette are likely: the effects that
//   stay fixed to the lens and so "rotate with the head" in both EYE BLOOM modes).
//   LENS zeroes the first group, LENS+ both. Plan B: EYE BLOOM OFF and the game's own
//   Lens Flares Off (its graphics menu).
// - LOD FADE. 0x2b4f8e0 asks IsActiveView 0x2c991a0 at 0x2b4f9b8 (e8 e3 97 14 00,
//   returning to 0x2b4f9bd, then test al,al / je): only the active view advances an
//   instance's LOD cross-fade, so in the eyes trees and props pop and "grow". LEFT answers
//   yes for the left eye there (the fade advances once a frame, as for the game view),
//   BOTH for both eyes (plan B: twice a frame, faster fades). Auto: N/A where the bytes
//   differ; back to OFF if no eye ever reaches that call.
// - LOD REACH. 0x2b4f918 multiplies the view's +0x1b28 by +0x1b24 (SHARED LOD's pair):
//   the eyes' multiplier is scaled by this percent (clamped to shared_lod's 0.25..4).
// - IMPOSTORS. The eyes' stand-in for the game view's impostors (stereo_probe isActiveView
//   at its five impostor calls, installed with createViews 32 and not 128), now live.
// - EYE OCCLUSION. giveOcclusion (0x2c9b8e0) runs only when an eye view is created, so it
//   cannot change in a session: live.txt eye_occlusion applies at the next VR start
//   (a restart notice says so).
// - The query cap (#7, [0xc35950c]) stays out: unconfirmed.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>

namespace spidy::view_gates {
// ---- BLOOM FX ----
enum Fx : uint32_t { fxOff = 0, lens = 1, lensPlus = 2, fxModes = 3 };
inline constexpr uint32_t defaultFx = fxOff, nextFx = 3; // Update 37.2: OFF (as Update 36)
inline uint32_t fxNext(uint32_t m) { return (m + 1) % fxModes; }
struct Field {
    uint16_t offset;
    uint8_t bytes;
};
// The view-flag group (0x2c293ca..0x2c293fa) and the bl group (0x2c29113..0x2c291d6).
inline constexpr Field lensFields[] = {{0x2b8, 4}, {0x2c4, 4}, {0x2cc, 4}, {0x2fc, 4},
                                       {0x328, 4}, {0x34c, 4}, {0x358, 8}};
inline constexpr Field plusFields[] = {{0x1e0, 4}, {0x1f0, 8}, {0x2d4, 4}, {0x2dc, 4}};
inline constexpr unsigned lensCount = sizeof(lensFields) / sizeof(Field), plusCount = sizeof(plusFields) / sizeof(Field);
inline constexpr unsigned groupFields = lensCount + plusCount; // 11
inline Field fieldAt(unsigned i) { return i < lensCount ? lensFields[i] : plusFields[i - lensCount]; }
inline unsigned fieldsFor(uint32_t mode) { return mode == lens ? lensCount : mode == lensPlus ? groupFields : 0; }
inline const char* fxName(uint32_t m) {
    switch (m) {
    case lens: return "LENS";
    case lensPlus: return "LENS+";
    default: return "OFF";
    }
}
inline constexpr const char* fxHelp = "Removes lens effects from the eyes that turn with your head.";
inline constexpr const char* fxPlanB = "Still turning? EYE BLOOM OFF, then Lens Flares Off in the game.";
inline constexpr uint32_t dumpFrom = 0xd8, dumpDwords = 16, dumpEvery = 64;

// ---- LOD FADE ----
inline constexpr uint32_t fadeCallRva = 0x2b4f9b8, fadeReturnRva = 0x2b4f9bd, isActiveViewRva = 0x2c991a0;
inline constexpr unsigned char fadeCallBytes[] = {0xe8, 0xe3, 0x97, 0x14, 0x00, 0x84, 0xc0, 0x0f, 0x84};
// The call's target from its bytes (rel32 after the e8), for the test that it is IsActiveView.
inline constexpr uint32_t callTarget(uint32_t callRva, const unsigned char* b) {
    const int32_t rel = static_cast<int32_t>(static_cast<uint32_t>(b[1]) | static_cast<uint32_t>(b[2]) << 8 |
                                             static_cast<uint32_t>(b[3]) << 16 | static_cast<uint32_t>(b[4]) << 24);
    return static_cast<uint32_t>(static_cast<int64_t>(callRva) + 5 + rel);
}
static_assert(callTarget(fadeCallRva, fadeCallBytes) == isActiveViewRva);
enum Fade : uint32_t { fadeOff = 0, leftEye = 1, bothEyes = 2, fadeModes = 3 };
// Build 12 WINDOW LIGHTS: OFF by default. LEFT let the left eye (its own lens and
// LOD scale) advance the instances' shared LOD cross-fade state beside the game's
// view every frame, two views writing one state; live lod_fade 1/2 still offer it.
inline constexpr uint32_t defaultFade = fadeOff, nextFade = 3;
inline uint32_t fadeNext(uint32_t m) { return (m + 1) % fadeModes; }
enum FadeWhy : uint32_t { fadeFine = 0, fadeNoHook = 1, fadeUnseen = 2 };
// Calls through isActiveView (any caller) after which no eye has reached 0x2b4f9bd: back to OFF.
inline constexpr uint64_t fadeUnseenCalls = 200000;
inline bool fadeAnswers(uint32_t mode, int eye) { return eye == 0 ? mode != fadeOff : eye == 1 && mode == bothEyes; }
inline const char* fadeName(uint32_t m) {
    switch (m) {
    case leftEye: return "LEFT";
    case bothEyes: return "BOTH";
    default: return "OFF";
    }
}
inline constexpr const char* fadeHelp = "Lets trees and props fade in for the eyes instead of popping.";
inline const char* fadeHelpText(uint32_t mode, uint32_t why) {
    if (!mode && why == fadeNoHook)
        return "N/A: this game build differs, so LOD fades are left as they are.";
    if (!mode && why == fadeUnseen)
        return "Back to OFF: no eye reached the game's fade code.";
    return mode == bothEyes ? "Plan B: both eyes advance the fade (faster fades)." : fadeHelp;
}
inline void fadeValue(uint32_t mode, uint32_t why, char* out, unsigned size) {
    std::snprintf(out, size, "%s", !mode && why == fadeNoHook ? "OFF (N/A)" : !mode && why ? "OFF (REVERTED)"
                                                                                             : fadeName(mode));
}

// ---- LOD REACH ----
inline constexpr uint32_t minReach = 100, maxReach = 300, defaultReach = 100;
inline float reachMultiplier(float multiplier, uint32_t percent) {
    const uint32_t p = std::clamp(percent, minReach, maxReach);
    const float m = std::isfinite(multiplier) && multiplier > 0 ? multiplier : 1.f;
    return std::clamp(m * static_cast<float>(p) / 100.f, .25f, 4.f);
}

// ---- IMPOSTORS / EYE OCCLUSION ----
inline const char* impostorHelp(bool installed, bool on) {
    if (!installed)
        return "N/A this session: the eyes' impostor stand-in was not installed.";
    return on ? "Far buildings drawn as full models for the eyes." : "Far buildings left to the game's impostors.";
}
inline constexpr const char* occlusionNotice =
    "RESTART NOTICE: eye_occlusion takes effect the next time VR starts.";

// ---- telemetry: SpidyViewGatesData (tools/view_gates.py) ----
inline constexpr uint32_t dataMagic = 0x54414756; // VGAT
struct Data {
    uint32_t magic = dataMagic, version = 1, bytes = 352, bloomFx = defaultFx;
    uint32_t lodFade = defaultFade, fadeWhy{}, fadeReady{}, lodReach = defaultReach;
    uint32_t impostorsInstalled{}, impostorsOn = 1, dumps{}, pad{};
    uint64_t fxJobs{}, fxWrites{}, fxFailed{};
    uint64_t fadeAsked[2]{}, fadeAnswered[2]{}, fadeGameCalls{}, reachWrites{}, impostorWithheld{};
    uint32_t headDump[dumpDwords]{}, eyeDump[dumpDwords]{};
    uint32_t headGroup[groupFields]{}, eyeGroup[groupFields]{};
    uint64_t sequence{};
};
static_assert(sizeof(Data) == 352);
} // namespace spidy::view_gates
