#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
// Made by Agent Mulder.
//
// 0.0.74 test 3 COCKPIT GLOW (owner: "the Batmobile interior feels generic ... darker, moody ambient light
// (low cool blue / amber accents), every switch, gauge and dial a slight natural glow, not neon").
// CONFIRMED (code): the mod's model pass lights every triangle on the CPU into its corner colours
// (hero007 drawTextured / van_fps drawPrepared: tint x (.32 + .68 |n.l|)), and mesh_model kept no emissive data,
// so the cockpit's gauges, screens and red switch caps were lit like leather. Now mesh_model gives each
// triangle a ROLE from its material (the converter's emissiveFactor, or its name: BC_Gauges / BC_Screen* a lamp,
// BC_RedCap a switch tip, HERO_handle_pad_mat the baked handle marker) and the cockpit and steering wheel
// (Mesh::cabin) shade through here:
//   COCKPIT GLOW ON   a cabin corner: tint (lit x dark + cool from above + amber from below)
//                     a lamp: tint x lampLevel (unlit: the dials / numbers / needles' backlight)
//                     a switch tip: the cabin's dark light + tint x tipGlow (a soft glow, not neon)
//   COCKPIT GLOW OFF  test 2b exactly (every role lit as before).
// CANDIDATE: the glow is constant; the mod reads no time of day - at night the game's exposure makes the same
// glow read brighter, in daylight weaker (the previews show both).
//
// 0.0.74 test 3 DASH INSIDE (owner screenshot: the dash panel and its cyan handle marker through the shell).
// CONFIRMED (diag renders, t3car): the shared cockpit (wider and taller than the FOREVER / FOREVER 2 cabins, a
// little over the 1989 / ODAMI canopies) was drawn from every side. ON: the Batmobile cockpit, its wheel and the
// dash screen (the van's too) are drawn only while the eye is inside the car's box - outside, the shell and its
// Roof piece are the car - and the baked HERO_handle_pad (a 18 x 5 cm blue slab) is never drawn: a small soft
// ring shows at the handle only while a hand is near it (handleNear), brighter once a grip opens the door.
#include "spidy/math.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <string>

namespace spidy::cabin_look {

// ---- the per-triangle role (mesh_model::Mesh::role) ------------------------------------------------------
enum Role : uint8_t { plain = 0, lamp = 1, tip = 2, marker = 3 };

// A material's role: its name first (the shipped cockpit has no emissiveFactor), else its emissive strength
// (the max of emissiveFactor x KHR_materials_emissive_strength; <0: none).
inline uint8_t roleOf(const std::string& name, float emissive) {
    const auto has = [&](const char* s) { return name.find(s) != std::string::npos; };
    if (has("HERO_handle_pad"))
        return marker;
    if (has("BC_Gauges") || has("BC_Screen"))
        return lamp;
    if (has("BC_RedCap") || has("BC_Tip"))
        return tip;
    if (std::isfinite(emissive) && emissive >= .5f)
        return lamp;
    if (std::isfinite(emissive) && emissive > .02f)
        return tip;
    return plain;
}

// ---- the switches (game_xr sets them each frame from the options; both default ON) --------------------------
inline std::atomic<bool>& glowSwitch() {
    static std::atomic<bool> on{true};
    return on;
}
inline std::atomic<bool>& insideSwitch() {
    static std::atomic<bool> on{true};
    return on;
}
inline bool glowOn() { return glowSwitch().load(std::memory_order_relaxed); }
inline bool insideOn() { return insideSwitch().load(std::memory_order_relaxed); }
inline void setGlow(bool on) { glowSwitch().store(on, std::memory_order_relaxed); }
inline void setInside(bool on) { insideSwitch().store(on, std::memory_order_relaxed); }
// The options' values (0 ON, the default; 1 OFF).
inline bool optionOn(uint32_t v) { return v == 0; }

// ---- COCKPIT GLOW shading ------------------------------------------------------------------------------------
inline constexpr float dark = .52f;                       // the cabin's own light, x the lit term
inline constexpr Vec3 coolFromAbove{.035f, .060f, .120f}; // a roof strip's cool blue on faces turned up
inline constexpr Vec3 amberFromBelow{.110f, .060f, .018f}; // the footwell's amber on faces turned down
inline constexpr float lampLevel = .92f;                  // a backlit dial / screen: its own colour, unlit
inline constexpr float tipGlow = .42f;                    // a switch tip's soft glow (over the dark light)

// The lit term's multiplier for one corner: `lit` the plain pass's (.32 + .68 |n.l|), `up` the corner normal's
// world-up component (either winding: |n| taken as is, the accent follows the face). Returns the colour
// multiplier per channel (x tint by the caller). Non-finite input: the plain lit term.
inline Vec3 cabinLight(float lit, float up) {
    if (!std::isfinite(lit) || !std::isfinite(up))
        return {lit, lit, lit};
    up = std::clamp(up, -1.f, 1.f);
    const float a = std::max(up, 0.f), b = std::max(-up, 0.f);
    const float d = lit * dark;
    return {d + coolFromAbove.x * a + amberFromBelow.x * b, d + coolFromAbove.y * a + amberFromBelow.y * b,
            d + coolFromAbove.z * a + amberFromBelow.z * b};
}

// One corner's colour under COCKPIT GLOW: `tint` the triangle's colour, `lit` and `up` as above, its role.
// `glow` false (or a non-cabin model): tint x lit, the plain pass.
inline Vec3 corner(Vec3 tint, float lit, float up, uint8_t role, bool cabin, bool glow) {
    if (!glow || !cabin)
        return tint * lit;
    if (role == lamp)
        return tint * lampLevel;
    const Vec3 m = cabinLight(lit, up);
    Vec3 c{tint.x * m.x, tint.y * m.y, tint.z * m.z};
    if (role == tip)
        c = c + tint * tipGlow;
    return c;
}

// The draw passes' form: `c` the corner's colour as the plain (or CAR PAINT satin) pass made it, `tint` the
// triangle's colour, `up` its normal's world-up component. Called for a cabin model with COCKPIT GLOW on only.
inline Vec3 relit(Vec3 c, Vec3 tint, float up, uint8_t role) {
    if (role == lamp)
        return tint * lampLevel;
    if (!std::isfinite(up))
        up = 0;
    up = std::clamp(up, -1.f, 1.f);
    const float a = std::max(up, 0.f), b = std::max(-up, 0.f);
    const Vec3 accent = coolFromAbove * a + amberFromBelow * b;
    Vec3 out = c * dark + Vec3{tint.x * accent.x, tint.y * accent.y, tint.z * accent.z};
    if (role == tip)
        out = out + tint * tipGlow;
    return out;
}
// Whether the draw passes skip a triangle of this role (the baked handle marker, DASH INSIDE on).
inline bool skipped(uint8_t role) { return role == marker && insideOn(); }

// ---- DASH INSIDE -------------------------------------------------------------------------------------------
// Whether `viewer` (world) is outside the car's box lo..hi (car frame: x right, y up, -z forward from `at`;
// `forward` its -z in the world), margin m metres. A bad frame: inside (draw as before).
inline bool outsideBox(Vec3 at, Vec3 forward, Vec3 lo, Vec3 hi, Vec3 viewer, float m = .05f) {
    const Vec3 rel = viewer - at;
    const float fl = std::sqrt(forward.x * forward.x + forward.z * forward.z);
    if (!(fl > 1e-4f) || !std::isfinite(rel.x + rel.y + rel.z) || !std::isfinite(lo.x + lo.y + lo.z + hi.x + hi.y + hi.z))
        return false;
    const Vec3 fw{forward.x / fl, 0, forward.z / fl}, right{-fw.z, 0, fw.x};
    const float x = rel.x * right.x + rel.z * right.z, y = rel.y, z = -(rel.x * fw.x + rel.z * fw.z);
    return !(x > std::min(lo.x, hi.x) - m && x < std::max(lo.x, hi.x) + m && y > std::min(lo.y, hi.y) - m &&
             y < std::max(lo.y, hi.y) + m && z > std::min(lo.z, hi.z) - m && z < std::max(lo.z, hi.z) + m);
}

// The handle marker: nothing past showFrom metres from the nearer hand; fading in to fullAt; `ready` (within the
// door's reach) brighter. Returns 0..1 (0: not drawn).
inline constexpr float showFrom = .45f, fullAt = .22f, markerRadius = .018f;
inline float handleNear(float distance, float reach) {
    if (!std::isfinite(distance) || distance >= showFrom)
        return 0.f;
    const float a = std::clamp((showFrom - distance) / (showFrom - fullAt), 0.f, 1.f);
    const bool ready = std::isfinite(reach) && distance <= reach;
    return ready ? 1.f : .55f * a;
}

} // namespace spidy::cabin_look
