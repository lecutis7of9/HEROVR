// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
#pragma once
// 0.0.73 SL WATCH: what the game hands Streamline (DLSS / DLAA) each frame, logged in a normal session.
//
// Offsets CONFIRMED against the Streamline SDK headers (NVIDIAGameWorks/Streamline, main, read October 28 2026):
// - sl_struct.h BaseStructure { BaseStructure* next; StructType structType (GUID, 16); size_t structVersion; } = 32.
// - sl_consts.h Constants (kStructVersion2, GUID {DCD35AD7-4E4A-4BAD-...}): five row-major float4x4 from +32
//   (cameraViewToClip, clipToCameraView, clipToLensClip, clipToPrevClip, prevClipToClip), then jitterOffset +352,
//   mvecScale +360, cameraPinholeOffset +368, cameraPos +376, cameraUp +388, cameraRight +400, cameraFwd +412,
//   cameraNear +424, cameraFar +428, cameraFOV +432 (radians), cameraAspectRatio +436, motionVectorsInvalidValue
//   +440, then sl::Boolean (char: 0 false, 1 true, 2 invalid) depthInverted +444, cameraMotionIncluded +445,
//   motionVectors3D +446, reset +447, orthographicProjection +448, motionVectorsDilated +449,
//   motionVectorsJittered +450; v2 minRelativeLinearDepthObjectSeparation +452; 456 bytes.
//   The outside review's bool order (jittered third) was wrong: jittered is the LAST bool, +450.
// - sl_core_api.h: slSetConstants(const Constants&, const FrameToken&, const ViewportHandle&): a0, a1, a2.
//   slSetTag(const ViewportHandle&, const ResourceTag*, uint32_t, CommandBuffer*);
//   slSetTagForFrame(const FrameToken&, const ViewportHandle&, const ResourceTag*, uint32_t, CommandBuffer*);
//   slEvaluateFeature(Feature, const FrameToken&, const BaseStructure** inputs, uint32_t numInputs, CommandBuffer*).
// - sl_core_types.h: ViewportHandle : BaseStructure { uint32_t value; } so the id is at +32.
//   ResourceTag : BaseStructure { Resource* +32; BufferType type +40; lifecycle +44; Extent +48 } = 64 bytes.
//   BufferType: 0 depth, 1 motion vectors, 2 HUD-less color, 3 scaling input color, 4 scaling output color.
//   FrameToken : BaseStructure with a pure virtual operator uint32_t(): with MSVC the vtable pointer is at +0 and
//   the base at +8 (GUID {830A0F35-...} at +16). The index comes from calling that virtual (slot 0), as the SDK's
//   own samples do; the dword at +40 (the interposer's own field) is also logged, CANDIDATE.
// The game's shipped sl.interposer.dll may be an older 2.x: structVersion is logged with every sample.
#include <array>
#include <cstdint>
#include <cstring>

namespace spidy::sl_watch {
inline constexpr uint32_t baseBytes = 32, guidAt = 8, versionAt = 24;
inline constexpr uint32_t constantsGuid = 0xdcd35ad7, frameTokenGuid = 0x830a0f35, viewportGuid = 0x171b6435;
inline constexpr uint32_t jitterAt = 352, mvecScaleAt = 360, nearAt = 424, farAt = 428, fovAt = 432,
                          aspectAt = 436, mvInvalidAt = 440, flagsAt = 444, flagCount = 7, constantsBytes = 456;
inline constexpr uint32_t viewportValueAt = 32, tokenGuidAt = 16, tokenRawAt = 40;
inline constexpr uint32_t tagBytes = 64, tagTypeAt = 40, maxTags = 16, maxInputs = 16;
// Flag order in Sample::flags (as in Constants): depthInverted, cameraMotionIncluded, motionVectors3D, reset,
// orthographicProjection, motionVectorsDilated, motionVectorsJittered.
enum Flag : uint32_t { depthInverted, cameraMotionIncluded, motionVectors3D, reset, orthographic, dilated, jittered };
inline constexpr uint32_t defaultEvery = 60; // frames between samples
inline constexpr uint32_t noFrame = 0xffffffffu;

inline bool on(uint32_t v) { return v == 0; } // option value 0 ON (default), 1 OFF

struct Sample {
    uint32_t call{}, frame{noFrame}, frameRaw{}, viewport{noFrame};
    float jitter[2]{}, mvecScale[2]{};
    float nearZ{}, farZ{}, fov{}, aspect{}, mvInvalid{};
    uint8_t flags[flagCount]{};
    uint8_t structVersion{};
    uint32_t callerRva{};
};
static_assert(sizeof(Sample) == 64);

struct View {
    uint32_t id{noFrame}, constants{}, tagCalls{}, tagMask{}, evaluates{}, lastFrame{noFrame}, resets{}, pad{};
};
static_assert(sizeof(View) == 32);

inline constexpr uint32_t dataMagic = 0x54574c53; // SLWT
inline constexpr uint32_t ringSize = 16, viewSlots = 4;
// hooked bits
inline constexpr uint32_t hookConstants = 1, hookEvaluate = 2, hookTag = 4, hookTagForFrame = 8;
// Shared with tools (SpidySlWatchData).
struct Data {
    uint32_t magic = dataMagic, version = 1, bytes = 1232, on = 1;
    uint32_t hooked{}, every = defaultEvery, samples{}, next{};
    uint64_t constantsCalls{}, evaluateCalls{}, tagCalls{}, foreign{}; // foreign: a0 not a Constants GUID
    View views[viewSlots]{};
    uint32_t overflow{}, distinctFrames{};
    Sample ring[ringSize]{};
    uint64_t sequence{};
};
static_assert(sizeof(Data) == 1232);

inline uint32_t u32(const uint8_t* p) {
    uint32_t v;
    std::memcpy(&v, p, 4);
    return v;
}
inline float f32(const uint8_t* p) {
    float v;
    std::memcpy(&v, p, 4);
    return v;
}
// A Constants block (constantsBytes read from a0) into a sample; false when the GUID is not Constants'.
inline bool decode(const uint8_t* c, Sample& s) {
    if (u32(c + guidAt) != constantsGuid)
        return false;
    s.structVersion = c[versionAt];
    s.jitter[0] = f32(c + jitterAt);
    s.jitter[1] = f32(c + jitterAt + 4);
    s.mvecScale[0] = f32(c + mvecScaleAt);
    s.mvecScale[1] = f32(c + mvecScaleAt + 4);
    s.nearZ = f32(c + nearAt);
    s.farZ = f32(c + farAt);
    s.fov = f32(c + fovAt);
    s.aspect = f32(c + aspectAt);
    s.mvInvalid = f32(c + mvInvalidAt);
    for (uint32_t i = 0; i < flagCount; ++i)
        s.flags[i] = c[flagsAt + i];
    return true;
}
// Sample every `every` frames (all viewports of that frame); without a frame index, every `every` calls.
inline bool wantSample(uint32_t frame, uint64_t call, uint32_t every) {
    if (every == 0)
        return false;
    return frame != noFrame ? frame % every == 0 : call % every == 0;
}
// The slot for a viewport id: its own, else the first free, else -1 (counted as overflow).
inline int slotFor(const View* views, uint32_t id) {
    for (uint32_t i = 0; i < viewSlots; ++i)
        if (views[i].id == id)
            return static_cast<int>(i);
    for (uint32_t i = 0; i < viewSlots; ++i)
        if (views[i].id == noFrame)
            return static_cast<int>(i);
    return -1;
}
inline uint32_t tagBit(uint32_t type) { return type < 32 ? 1u << type : 0u; }
} // namespace spidy::sl_watch
