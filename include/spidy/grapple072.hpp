#pragma once
// Version 0.0.72 GRAPPLE FOR EVERY HERO WHO HAS ONE (the user, 11:09): BATMAN (his grapple gun), the
// PUNISHER (his chest-rig hook launcher, the same grapple-gun model) and DAREDEVIL (his baton's cable).
//  - The pull is cape_glide::Grapple on the swing bridge (game_swing.cpp): BATMAN with his cape
//    (askBatman), the others without (askGrappleOnly, sent EVERY frame the hero is live so the pull
//    carries on to the point; the hand's askGrapple bit only on the frame it fires).
//    0.0.71 root cause: game_xr never sent askGrappleOnly, so DAREDEVIL's cast and the PUNISHER's hook
//    fired nothing at all. A pull from the ground now takes off by the game's jump first
//    (cape_glide::liftFirst) and the pull's clock scales with the distance (no tug then drop).
//  - Never a web: no web zip, no thwip (the fire is sfx::batonWhoosh), the line is the grapple line.
//  - The gun is held with its barrel ALONG THE AIM (the ray the bridge fires on), its grip in the palm.
// Game hooks: the bridge's world raycast and velocity command are the webs' own (CONFIRMED: the
// webs use them); the game's jump for a takeoff from the ground is the webs' (CONFIRMED); that the
// jump lifts a grounded body for a grapple's pull as it does for a web's is CANDIDATE (not seen in game).
#include "spidy/cape_glide.hpp"
#include "spidy/hero_modes.hpp"
#include "spidy/math.hpp"

#include <cmath>
#include <cstdint>

namespace spidy::grapple072 {

enum class Device : uint32_t { none = 0, gun, baton };
// Version 0.0.73 (fix 17): CAPTAIN AMERICA has the grapple gun too (grip draws it, trigger fires).
inline Device deviceOf(uint32_t hero) {
    return hero == hero_modes::batman || hero == hero_modes::punisher || hero == hero_modes::captainAmerica ? Device::gun
           : hero == hero_modes::daredevil                            ? Device::baton
                                                                      : Device::none;
}
inline bool grappler(uint32_t hero) { return deviceOf(hero) != Device::none; }
inline const char* heroName(uint32_t hero) {
    return hero == hero_modes::batman ? "BATMAN" : hero == hero_modes::punisher ? "PUNISHER" : hero == hero_modes::daredevil ? "DAREDEVIL"
           : hero == hero_modes::captainAmerica ? "CAPTAIN AMERICA" : "";
}
// The swing command's asks bits for this hero's grapple (live: the hero's input reaches the bridge).
inline uint32_t asks(uint32_t hero, bool live, bool left, bool right) {
    if (!live || !grappler(hero))
        return 0;
    if (hero == hero_modes::batman)
        return cape_glide::pack(true, left, right);
    return cape_glide::askGrappleOnly | (left ? cape_glide::askGrapple[0] : 0u) | (right ? cape_glide::askGrapple[1] : 0u);
}

// The held grapple gun: its grip in the palm (at), its barrel along the aim, its top as near the
// grip pose's top as the aim allows. ok false: no usable aim (the caller draws nothing).
struct Held {
    Vec3 at{}, forward{0, 0, -1}, up{0, 1, 0};
    bool ok{};
};
inline bool fine(Vec3 v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }
inline Held alongAim(Vec3 at, Vec3 aim, Vec3 gripUp) {
    Held h;
    if (!fine(at) || !fine(aim) || length(aim) < 1e-3f)
        return h;
    h.at = at;
    h.forward = normalized(aim);
    Vec3 up = fine(gripUp) ? gripUp - h.forward * dot(gripUp, h.forward) : Vec3{};
    if (length(up) < .2f) {
        up = Vec3{0, 1, 0} - h.forward * h.forward.y;
        if (length(up) < .2f)
            up = Vec3{0, 0, 1} - h.forward * h.forward.z;
    }
    h.up = normalized(up);
    h.ok = fine(h.up) && std::abs(dot(h.up, h.forward)) < 1e-3f;
    return h;
}
// DAREDEVIL: the cable leaves his baton's tip (the baton held along the aim).
inline constexpr float batonTip = .52f;
inline Vec3 cableFrom(Vec3 hand, Vec3 aim) {
    return fine(hand) && fine(aim) && length(aim) > 1e-3f ? hand + normalized(aim) * batonTip : hand;
}
inline constexpr const char* fireNote[3] = {"", "VERSION 0.0.72 GRAPPLE: fired from the grapple gun",
                                            "VERSION 0.0.72 GRAPPLE: fired from the baton's cable"};

} // namespace spidy::grapple072
