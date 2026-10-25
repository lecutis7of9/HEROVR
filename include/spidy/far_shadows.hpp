#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
//
// Update 35: FAR SHADOWS (live, GRAPHICS page, default EYES, never kept).
// The hard shadow line 1-2 blocks ahead, everything past it lit, sliding with head
// pitch and deaf to SHADOW DISTANCE. Exe-confirmed (Spider-Man2.exe 2.810.0.0, by
// disassembly; BUILD-SM2.md section 45):
// - SubmitScene 0x2d01b30 makes each view's key-light cascades (a 0x5060 object) in
//   0x2a938b0, which calls the fit 0x2a8dc80 (rcx the cascades, rdx the view).
// - The fit asks IsActiveView 0x2c991a0 (view == ViewContextManager +0xf8) at
//   0x2a8e145 (return 0x2a8e14a). Only for the active view (or a debug byte) does it
//   take the far cascades from the far cache 0x2ad11b0 (static cache 0xbf57ce0): their
//   count is cache +0x216c, stored at cascades +0xe; LodConsts (+0xc0) .w = base + far,
//   .z = base; FarLodConsts (+0xd0) .xyz the cache centre less the view's position.
// - The shader (cb6 GlobalWorldCBuffer, LodConsts @0x130) fades a pixel to unshadowed
//   past the base cascades' box when LodConsts.w <= .z (no far cascades).
// The eyes are never the active view (the head view is), so each eye got base
// cascades only, fitted to its own frustum: lit past its box, moving with the head.
// FAR SHADOWS answers "active" for an eye at that one call (EYES). The far cache then
// sees the eye as not active and hands back what the active view last made (it makes
// it itself when that is 2 frames stale). EYES+CACHE (plan B) also answers "active"
// for the left eye inside the cache (0x2ad1238, return 0x2ad123d), so it refits it.
// Candidates (not confirmed): that nothing else in the frame needs the eye's far
// cascades drawn again; the cost.
#include <cstdint>
#include <cstdio>

namespace spidy::far_shadows {
inline constexpr uint32_t fitRva = 0x2a8dc80, isActiveRva = 0x2c991a0;
inline constexpr uint32_t fitAskRva = 0x2a8e145, fitAskReturn = 0x2a8e14a;
inline constexpr uint32_t cacheAskRva = 0x2ad1238, cacheAskReturn = 0x2ad123d;
// The fit's entry and its two calls to IsActiveView, as the exe has them.
inline constexpr unsigned char fitBytes[] = {0x48, 0x8b, 0xc4, 0x4c, 0x89, 0x48, 0x20, 0x4c, 0x89, 0x40,
                                             0x18, 0x48, 0x89, 0x50, 0x10, 0x48, 0x89, 0x48, 0x08};
inline constexpr unsigned char fitAskBytes[] = {0xe8, 0x56, 0xb0, 0x20, 0x00};
inline constexpr unsigned char cacheAskBytes[] = {0xe8, 0x63, 0x7f, 0x1c, 0x00};
// The cascades object the fit fills: counts and constants.
inline constexpr uint32_t baseCount = 0xd, farCount = 0xe, lodConsts = 0xc0, farLodConsts = 0xd0;

enum Mode : uint32_t { off = 0, eyes = 1, eyesCache = 2, modes = 3 };
inline constexpr uint32_t defaultMode = eyesCache; // Update 37: the run36 closing setting
// SpidyFarShadows's argument past the modes.
inline constexpr uint32_t nextMode = 3, slowOff = 4;
inline uint32_t next(uint32_t mode) { return (mode + 1) % modes; }
inline const char* modeName(uint32_t mode) {
    switch (mode) {
    case eyes: return "EYES";
    case eyesCache: return "EYES+CACHE";
    default: return "OFF";
    }
}
// Why it went back to OFF on its own.
enum Reason : uint32_t {
    none = 0,
    noHook = 1,   // the fit or its calls differ in this game build
    noFar = 2,    // the eyes still got no far cascades (the game view had some)
    gameNone = 3, // neither the eyes nor the game view had far cascades
    slow = 4,     // the frame rate halved
    unseen = 5,   // no eye fit came through 0x2a8dc80 (the game view's did)
    reasons = 6
};
inline constexpr uint32_t pack(uint32_t mode, uint32_t reason) { return mode | reason << 8; }
inline uint32_t modeOf(uint32_t packed) { return packed & 0xff; }
inline uint32_t reasonOf(uint32_t packed) { return packed >> 8 & 0xff; }
inline const char* reasonText(uint32_t reason) {
    switch (reason) {
    case noHook: return "this game build differs, so it was left alone";
    case noFar: return "the eyes still got no far shadows";
    case gameNone: return "the game made no far shadows here either";
    case slow: return "the frame rate halved";
    case unseen: return "the eyes' shadows are fitted elsewhere";
    default: return "";
    }
}
inline constexpr const char* help = "Shadows past the first blocks for both eyes; EYES+CACHE is the plan B.";
inline void valueText(uint32_t packed, char* out, unsigned size) {
    const uint32_t m = modeOf(packed), r = reasonOf(packed);
    if (!m && r)
        std::snprintf(out, size, r == noHook ? "OFF (N/A)" : "OFF (REVERTED)");
    else
        std::snprintf(out, size, "%s", modeName(m));
}
inline constexpr const char* widestValue() { return "OFF (REVERTED)"; }

// The auto-revert: judged over the first eye fits after a mode went on (about 2 s).
// Any far cascade in them keeps it. EYES that made none steps on to EYES+CACHE (plan
// B, judged again); EYES+CACHE that made none goes back to OFF.
inline constexpr uint64_t judgeFits = 240;
// none while undecided or fine; else why the mode failed.
inline uint32_t verdict(uint64_t eyeFits, uint64_t eyeFarFits, uint64_t gameFits, uint64_t gameFarFits) {
    if (!eyeFits && gameFits >= judgeFits)
        return unseen;
    if (eyeFits < judgeFits || eyeFarFits)
        return none;
    return gameFarFits ? noFar : gameNone;
}
// The mode after a failed one: plan B only when the eyes alone were short.
inline uint32_t afterFailure(uint32_t mode, uint32_t reason) {
    return mode == eyes && reason == noFar ? eyesCache : off;
}

// Shared with tools/far_shadows.py (SpidyFarShadowsData): what the fits made.
inline constexpr uint32_t dataMagic = 0x53524146; // FARS
inline constexpr uint32_t dataVersion = 1;
struct EyeFit {
    uint64_t fits{}, farFits{};      // fits, and those with far cascades
    uint32_t base{}, farCount{}, kind{}, pad{}; // the newest: counts; the view's kind (+0x1db4)
    float lod[4]{}, farLod[4]{};      // the newest LodConsts, FarLodConsts
};
static_assert(sizeof(EyeFit) == 64);
struct Data {
    uint32_t magic = dataMagic, version = dataVersion, bytes = 256, packed = pack(defaultMode, none);
    uint32_t hooked{}, modeChanges{}, reverts{}, lastRevert{};
    uint64_t answeredFit{}, answeredCache{}; // "active" answers given for an eye
    uint64_t gameFits{}, otherFits{};        // the active view's fits, everyone else's
    uint32_t gameBase{}, gameFar{};          // the active view's newest counts
    float gameLod[4]{}, gameFarLod[4]{};
    uint64_t judgedFits{}, judgedFar{};      // eye fits since it went on, those with far
    EyeFit eye[2]{};
    uint64_t sequence{};
};
static_assert(sizeof(Data) == 256);
} // namespace spidy::far_shadows
