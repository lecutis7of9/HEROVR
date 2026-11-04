#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
// Made by Agent Mulder.
//
// HERO VR 0.0.78 WORLD LIGHT (players: "the heroes look low-res and out of place"). Until now every model the mod
// draws itself (the suits, the hammers, the shield, the claws, the guns, the van with its interior and wheels,
// the Batmobiles, the bikes) was lit by one fixed studio light, normalized(.3, 1, .25), .32 + .68 |n.l|, at
// every hour and in every street. Now the game's own frame light lights them:
//
// CONFIRMED (read from the game, Spider-Man2.exe):
//   - g_World (cb6, "GlobalWorldCBuffer", 1184 bytes): the field names and offsets are the exe's own shader
//     reflection (RDEF of its DXBC shaders): m_TimeOfDay 0x0c, m_FogColor 0xe0, m_AdaptedLuminance 0x10c,
//     m_KeyLightColorLinear 0x110, m_PenumbraRadius 0x11c, m_KeyLightVec 0x120, m_ActualLuminance 0x12c,
//     m_LightGridColorModulation 0x370, m_EnvProbeColorModulation 0x3b0, m_ActiveViewCtxPos 0x3f0,
//     m_ToneMapAnchor 0x404.
//   - The function that fills it, 0x2c67f40 (rcx: the buffer, rdx: the frame job), called from 0x2c6b73a (the
//     buffer the GPU reads, allocated 0x4a0 bytes as constant buffer 6) and 0x2c51f5c (a CPU copy). Its
//     disassembly: KeyLightVec is minus the key light record's direction (so it points toward the sun),
//     (0, 1, 0) with a black colour when there is no key light; the colour is the record's +0x10;
//     AdaptedLuminance = 2^(EV - 13) 2^-(compensation) (the game's own exposure); FogColor is the job's +0x7b8.
//   - The overlay draws in the game's world space (mp_world.hpp: rendered.anchor + travel, Y up), so
//     KeyLightVec is used as it is.
// CANDIDATE (the calibration from those numbers to the overlay's display colours, tunable live):
//   - direct = KeyLightColorLinear / AdaptedLuminance / pi x world_light_gain, a soft shoulder over 1;
//   - sky    = FogColor x EnvProbeColorModulation / AdaptedLuminance, with a floor of a share of the direct
//     light, brighter on faces turned up than down (a hemisphere);
//   - the hero's own shadow side: one-sided n.l on smooth normals (both sides on a face normal, whose winding
//     the models do not promise).
// Not read: the game's shadow atlas (t64) and light probes (t58/t60) are GPU resources no CPU path to was
// found; the screen-space sun shadows (d3d12_renderer.cpp) march the game's own scene depth instead.
//
// STUDIO (live.txt world_light 2, and whenever no fresh frame light was read): the 0.0.77 fixed light, said so
// in the log as NOT world light.
#include "spidy/math.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

namespace spidy::world_light {

// The fill function and its first 16 bytes (mov rax,rsp; push rbp; push rdi; lea rbp,[rax-28h]; sub rsp,118h).
inline constexpr uintptr_t fillRva = 0x2c67f40;
inline constexpr unsigned char fillBytes[16] = {0x48, 0x8b, 0xc4, 0x55, 0x57, 0x48, 0x8d, 0x68,
                                                0xd8, 0x48, 0x81, 0xec, 0x18, 0x01, 0x00, 0x00};
// Its callers' return addresses: the GPU's constant buffer (slot 0) and the CPU copy (slot 1).
inline constexpr uintptr_t gpuCallerReturn = 0x2c6b73f, cpuCallerReturn = 0x2c51f61;
inline constexpr uint32_t bufferBytes = 0x4a0;
// g_World offsets (the exe's RDEF).
inline constexpr uint32_t offTimeOfDay = 0x0c, offFogColor = 0xe0, offAdaptedLuminance = 0x10c,
                          offKeyLightColor = 0x110, offPenumbraRadius = 0x11c, offKeyLightVec = 0x120,
                          offActualLuminance = 0x12c, offLightGridModulation = 0x370, offEnvProbeModulation = 0x3b0,
                          offActiveViewPos = 0x3f0, offToneMapAnchor = 0x404;
// The two spans read from it (each one copy under SEH): 0x00..0x130 and 0x370..0x408.
inline constexpr uint32_t spanA = 0x130, spanBFrom = 0x370, spanBBytes = 0x408 - 0x370;

// One frame's numbers as the game wrote them.
struct Sample {
    float toLight[3]{};   // m_KeyLightVec (unit, toward the sun / moon)
    float color[3]{};     // m_KeyLightColorLinear
    float fog[3]{};       // m_FogColor
    float probe[3]{};     // m_EnvProbeColorModulation
    float grid[3]{};      // m_LightGridColorModulation
    float viewPos[3]{};   // m_ActiveViewCtxPos
    float adapted = 0;    // m_AdaptedLuminance (exposure)
    float actual = 0;     // m_ActualLuminance
    float penumbra = 0;   // m_PenumbraRadius
    float timeOfDay = 0;  // m_TimeOfDay
    float toneAnchor = 0; // m_ToneMapAnchor
    uint32_t slot = 0;    // 0 the GPU's buffer, 1 the CPU copy, 2 another caller
};
static_assert(sizeof(Sample) == 96);
// Sample from the two spans as copied (a: 0x00..0x130, b: 0x370..0x408).
inline Sample fromSpans(const unsigned char* a, const unsigned char* b, uint32_t slot) {
    Sample s;
    auto f = [](const unsigned char* p, uint32_t at, float* out, int n) {
        for (int i = 0; i < n; ++i) {
            float v;
            std::memcpy(&v, p + at + 4 * i, 4);
            out[i] = v;
        }
    };
    f(a, offKeyLightVec, s.toLight, 3);
    f(a, offKeyLightColor, s.color, 3);
    f(a, offFogColor, s.fog, 3);
    f(a, offAdaptedLuminance, &s.adapted, 1);
    f(a, offActualLuminance, &s.actual, 1);
    f(a, offPenumbraRadius, &s.penumbra, 1);
    f(a, offTimeOfDay, &s.timeOfDay, 1);
    f(b, offEnvProbeModulation - spanBFrom, s.probe, 3);
    f(b, offLightGridModulation - spanBFrom, s.grid, 3);
    f(b, offActiveViewPos - spanBFrom, s.viewPos, 3);
    f(b, offToneMapAnchor - spanBFrom, &s.toneAnchor, 1);
    s.slot = slot;
    return s;
}

// The hook's record (stereo_probe.cpp SpidyWorldLightData): the newest sample of each slot and their counts.
inline constexpr uint32_t dataMagic = 0x4c444c57; // WLDL
struct Data {
    uint32_t magic = dataMagic, version = 1, bytes = 0, hooked = 0; // hooked: 1 on, 2 bytes differ, 3 hook failed
    uint64_t calls[3]{}, readFaults{};
    uint64_t stampMs[3]{};
    Sample last[3]{};
    uint64_t sequence{};
};

// Why a frame is not world lit (0: it is).
enum Why : uint32_t { lit = 0, studioChosen, noHook, stale, badNumbers, whys };
inline constexpr const char* whyNames[whys] = {"world light", "STUDIO chosen", "no frame light hook", "no fresh frame light",
                                              "frame light out of range"};

// The light the models are lit by: display colours, linear.
struct Light {
    Vec3 toSun = normalized(Vec3{.3f, 1.f, .25f});
    Vec3 sun{.68f, .68f, .68f};                         // the direct term at n.l = 1
    Vec3 skyUp{.32f, .32f, .32f}, skyDown{.32f, .32f, .32f}; // the rest, on faces turned up / down
    Vec3 specular{1, 1, 1};                             // car paint's highlight scale (sun / .68)
    Vec3 env{1, 1, 1};                                  // car paint's street / sky reflection scale
    float shadow = 0;                                   // the GPU sun shadows' strength (0 none)
    bool world = false;                                 // lit by the game's frame light
    bool oneSided = false;                              // n.l clamped at 0 (else |n.l|, the studio's)
    uint32_t why = noHook;
};
inline Light studio(uint32_t why = studioChosen) {
    Light l;
    l.why = why;
    return l;
}

struct Tune {
    float gain = 1.f;        // world_light_gain / 100
    float skyFloor = .22f;   // the sky's least, as a share of the direct light's brightness
    float skyLeast = .035f;  // and its least at all (a night street is not black)
    float groundShare = .45f; // a face turned down sees this share of the sky
    float shoulder = 1.6f;   // where the soft shoulder ends (Reinhard extended white)
};

inline bool finite3(const float* v) { return std::isfinite(v[0]) && std::isfinite(v[1]) && std::isfinite(v[2]); }
inline float luminance(Vec3 c) { return .2126f * c.x + .7152f * c.y + .0722f * c.z; }
inline Vec3 mul(Vec3 a, Vec3 b) { return {a.x * b.x, a.y * b.y, a.z * b.z}; }
inline Vec3 v3(const float* v) { return {v[0], v[1], v[2]}; }
// The soft shoulder: linear near 0, 1.6 reaches 1 (extended Reinhard), per channel and keeping the hue.
inline Vec3 shoulder(Vec3 c, float white) {
    const float l = std::max({c.x, c.y, c.z});
    if (!(l > 0))
        return {0, 0, 0};
    const float w2 = white * white, m = l * (1 + l / w2) / (1 + l);
    return c * (m / l);
}

// The game's numbers as the overlay's light; `why` says when they could not be used (the studio light then).
inline Light derive(const Sample& s, const Tune& tune) {
    if (!finite3(s.toLight) || !finite3(s.color) || !finite3(s.fog) || !finite3(s.probe) || !std::isfinite(s.adapted))
        return studio(badNumbers);
    const Vec3 toLight = v3(s.toLight), color = v3(s.color);
    const float tl = length(toLight);
    if (!(tl > .9f && tl < 1.1f) || !(s.adapted > 1e-6f && s.adapted < 1e6f) || color.x < 0 || color.y < 0 || color.z < 0 ||
        std::max({color.x, color.y, color.z}) > 1e7f)
        return studio(badNumbers);
    const float exposure = 1.f / s.adapted;
    const float gain = std::clamp(tune.gain, .05f, 8.f);
    Light l;
    l.world = true;
    l.oneSided = true;
    l.why = lit;
    l.toSun = toLight * (1.f / tl);
    l.sun = shoulder(color * (exposure * gain * (1.f / 3.14159265f)), tune.shoulder);
    Vec3 probe = v3(s.probe);
    if (!(probe.x >= 0 && probe.y >= 0 && probe.z >= 0) || std::max({probe.x, probe.y, probe.z}) > 16.f)
        probe = {1, 1, 1};
    Vec3 fog = v3(s.fog);
    fog = {std::max(fog.x, 0.f), std::max(fog.y, 0.f), std::max(fog.z, 0.f)};
    Vec3 sky = shoulder(mul(fog, probe) * (exposure * gain), tune.shoulder);
    // The floor: the sky's hue (or grey when it has none) at a share of the direct light, and a least at all.
    const float want = std::max(tune.skyFloor * luminance(l.sun), tune.skyLeast), have = luminance(sky);
    if (have < want)
        sky = have > 1e-5f ? sky * (want / have) : Vec3{want, want, want};
    l.skyUp = sky;
    l.skyDown = sky * std::clamp(tune.groundShare, 0.f, 1.f);
    l.specular = l.sun * (1.f / .68f);
    l.env = sky * (1.f / .32f);
    return l;
}

// n.l as this light takes it.
inline float facing(const Light& l, float nl) { return l.oneSided ? std::max(nl, 0.f) : std::abs(nl); }
// The rest of the light (no sun) at a face whose normal's world up is `up` (-1..1).
inline Vec3 sky(const Light& l, float up) {
    const float t = std::clamp(.5f + .5f * up, 0.f, 1.f);
    return l.skyDown + (l.skyUp - l.skyDown) * t;
}
// A corner's diffuse light: `d` from facing(), `up` its normal's world y.
inline Vec3 diffuse(const Light& l, float d, float up) { return sky(l, up) + l.sun * d; }
// The share of a corner's light from the sun (what the GPU sun shadow takes away, shadowAlpha); 1 in STUDIO.
inline float sunShare(const Light& l, float d, float up) {
    if (!l.world)
        return 1.f;
    const float s = luminance(l.sun) * d, all = s + luminance(sky(l, up));
    return all > 1e-6f ? std::clamp(s / all, 0.f, 1.f) : 0.f;
}
// One corner: tint x the light at unit normal n (smooth: one-sided in world light; a face normal: both sides).
inline Vec3 shade(const Light& l, Vec3 tint, Vec3 n, bool smoothNormal, float* share = nullptr) {
    const float nl = dot(n, l.toSun);
    const float d = smoothNormal ? facing(l, nl) : std::abs(nl);
    if (share)
        *share = sunShare(l, d, n.y);
    return mul(tint, diffuse(l, d, n.y));
}

// The light for this frame (game_xr sets it once a frame; every model draw takes a copy).
Light current();
void setCurrent(const Light& l);
// The GPU sun shadow strength wanted (0 off); and whether to give the eyes' depth for it every frame.
float shadowStrength(const Light& l, uint32_t shadowMode);
std::string describe(const Light& l, const Sample& s);

} // namespace spidy::world_light

namespace spidy::world_light {
// live.txt world_light: 0 GAME (the default), 1 GAME SOFT (the sky floor at 45 % of the sun, faces turned down 70 % of the sky: softer shadow sides), 2 STUDIO.
enum Mode : uint32_t { modeGame = 0, modeGameSoft = 1, modeStudio = 2 };
// live.txt world_shadow: 0 ON (the default: the eyes' depth watched every frame for it), 1 VEHICLES (only while
// the see-through car depth is watched anyway, no extra cost), 2 OFF.
enum ShadowMode : uint32_t { shadowOn = 0, shadowVehicles = 1, shadowOff = 2 };
inline bool wantsDepthAlways(uint32_t shadowMode, const Light& l) { return shadowMode == shadowOn && l.world; }
// The tune for a mode and world_light_gain (percent, 25..400; 100 the default).
inline Tune tuneFor(uint32_t mode, uint32_t gainPercent) {
    Tune t;
    t.gain = std::clamp(static_cast<float>(gainPercent), 25.f, 400.f) / 100.f;
    if (mode == modeGameSoft)
        t.skyFloor = .45f, t.groundShare = .7f;
    return t;
}
// The newest fresh frame light the hook read (stereo_probe.cpp; Windows builds): the CPU copy's when fresh,
// else the GPU buffer's. False: none within maxAgeMs.
bool newest(Sample& out, uint64_t maxAgeMs);
// The hook's state for the log.
std::string hookLine();
} // namespace spidy::world_light

namespace spidy::world_light {
// n turned to the side seen from `toEye` (a zero toEye: n as it is).
inline Vec3 seenSide(Vec3 n, Vec3 toEye) { return dot(n, toEye) < 0 ? n * -1.f : n; }
} // namespace spidy::world_light

namespace spidy::world_light {
// A flat-lit face (the untextured fallbacks, the shield's discs): unit normal n, toward-eye toEye (may be zero).
// STUDIO: tint (.32 + .68 |n.l|), the 0.0.77 colour.
inline Vec3 faceColour(const Light& l, Vec3 tint, Vec3 n, Vec3 toEye) {
    if (!l.world)
        return tint * (.32f + .68f * std::abs(dot(n, l.toSun)));
    const bool seen = toEye.x != 0 || toEye.y != 0 || toEye.z != 0;
    return shade(l, tint, seen ? seenSide(n, toEye) : n, seen);
}
} // namespace spidy::world_light

namespace spidy::world_light {
// The model vertex's alpha (the model shaders draw opaque, so it is free): the share of its light the GPU sun
// shadow leaves when the game's depth says the sun is blocked. 1 (nothing taken) in STUDIO and for every vertex
// not lit here (the hideouts' rooms), so only world-lit models are ever darkened.
inline float shadowAlpha(const Light& l, float share) { return l.world ? std::clamp(1.f - share, 0.f, 1.f) : 1.f; }
} // namespace spidy::world_light
