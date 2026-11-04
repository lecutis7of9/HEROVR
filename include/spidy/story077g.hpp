#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
// HERO VR 0.0.77 INTERACT CARDS AND WAYPOINT, after the player's report of 0.0.77 story8 (pt1797u: the truck to
// move for the firefighters, the water pipe to break, the things to grab for a mission - "the prompts dont show up
// in VR neither do the inworld waypoints for objectives, so youre never sure where exactly to stand").
// Root cause (CONFIRMED from his logs): prompt_cards knew only the QTE / web strike / throwable classes - 0 object
// cards in an 870 s session - and the objective's yellow waypoint is the game's cohtml HUD, which VR does not show.
// The interact kinds are in story077d.hpp's table; this is the WAYPOINT: a yellow diamond the mod draws (never a
// picture of the game) over the current objective's CANDIDATE place, with its distance, over the game's picture
// (walls do not hide it), and an arrow at the edge of the view while it is out of sight.
// Sources, in order (each CANDIDATE that it is the objective; the positions are the records' own matrices):
//   1. an AddPOIAction / AddInteractPOIAction (the script action that adds a map / HUD marker) with a place, newest
//      first - the objective marker the game's HUD would show (CANDIDATE: none logged yet);
//   2. the nearest objective-like interactable 3-150 m away (the truck, the pipe, a hold offer - a carry, a pickup,
//      a stand-here interaction - an InteractLocation);
//   3. (waypoints = on only) the nearest MissionLocationProxy within 3 km: the next mission's start (its place is
//      CONFIRMED readable: FALCON_CHASE_BROOKLYN, MYSTERIO_CHALLENGE_05 in the 20261103 world trace).
// Pure and allocation-free.
#include "math.hpp"
#include "story077d.hpp"
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace spidy::story077g {
// live.txt waypoints: 0 on (the default: objective, else the next mission's start), 1 objective (no mission
// starts), 2 off.
enum Waypoints : uint32_t { waypointsOn = 0, waypointsObjective = 1, waypointsOff = 2 };
enum Source : uint32_t { none = 0, poi = 1, interactable = 2, missionStart = 3 };
inline const char* sourceName(uint32_t s) {
    switch (s) {
    case poi: return "POI action";
    case interactable: return "objective interactable";
    case missionStart: return "mission start";
    default: return "none";
    }
}
inline const char* sourceLabel(uint32_t s) {
    switch (s) {
    case missionStart: return "MISSION";
    case none: return "";
    default: return "OBJECTIVE";
    }
}
inline bool isClass(uint32_t k, const char* cls) {
    const auto* d = story077d::kind(k);
    return d && !std::strcmp(d->cls, cls);
}
// Which source a kind can be.
inline uint32_t sourceOf(uint32_t k) {
    if (isClass(k, ".?AVAddPOIAction@@") || isClass(k, ".?AVAddInteractPOIAction@@"))
        return poi;
    if (isClass(k, ".?AVMissionLocationProxy@@"))
        return missionStart;
    if (isClass(k, ".?AVInteractLocation@@") || isClass(k, ".?AVVehiclePushable@@") || isClass(k, ".?AVWaterPipe@@") ||
        isClass(k, ".?AVInteractLocationOffer@@") || isClass(k, ".?AVPickupCarryableOffer@@") ||
        isClass(k, ".?AVHoldOffer@@"))
        return interactable;
    return none;
}
inline constexpr float poiFarthest = 3000.f, interactNearest = 3.f, interactFarthest = 150.f,
                       missionFarthest = 3000.f, poiNearest = 2.f;
// Not a place: the origin, or no read.
inline bool somewhere(const story077d::Seen& s) {
    return s.placed && finite(s.world) && (std::fabs(s.world.x) + std::fabs(s.world.y) + std::fabs(s.world.z)) > 1.f;
}
struct Mark {
    bool shown{};
    uint32_t source{}, kind{};
    uint64_t id{};
    Vec3 world{};
    float metres{};
};
inline Mark pick(const story077d::Seen* seen, uint32_t n, Vec3 hero, bool heroKnown, uint32_t mode) {
    Mark best;
    if (mode == waypointsOff || !heroKnown || !finite(hero) || !seen)
        return best;
    float bestScore = 1e30f;
    for (uint32_t i = 0; i < n; ++i) {
        const auto& s = seen[i];
        const uint32_t src = sourceOf(s.kind);
        if (src == none || !somewhere(s))
            continue;
        const float m = length(s.world - hero);
        if (!std::isfinite(m))
            continue;
        float score;
        if (src == poi) {
            if (m < poiNearest || m > poiFarthest || !std::isfinite(s.age) || s.age < 0)
                continue;
            score = s.age; // newest first, ahead of every other source
        } else if (src == interactable) {
            if (m < interactNearest || m > interactFarthest)
                continue;
            score = 1e6f + m;
        } else {
            if (mode != waypointsOn || m > missionFarthest)
                continue;
            score = 1e8f + m;
        }
        if (score < bestScore) {
            bestScore = score;
            best = Mark{true, src, s.kind, s.id, s.world, m};
        }
    }
    return best;
}
// Drawing: the diamond sits along the line to the objective at most drawAhead metres away and keeps about the same
// size in view; out of the view cone (edgeDegrees from the look) an arrow at the view's edge points the way.
inline constexpr float drawAhead = 12.f, diamondPerMetre = .022f, edgeDegrees = 38.f, edgeAhead = 1.4f,
                       edgeOut = .32f, edgeSize = .05f;
inline bool outOfView(Vec3 toObjective, Vec3 ahead) {
    const float m = length(toObjective);
    if (!std::isfinite(m) || m < 1e-3f)
        return false;
    const float c = dot(toObjective, ahead) / m;
    return !(c >= std::cos(edgeDegrees * 3.14159265f / 180.f));
}
// The way to the objective in the view's plane (right / up components, unit); false: straight ahead or behind.
inline bool edgeWay(Vec3 toObjective, Vec3 right, Vec3 up, float& x, float& y) {
    x = dot(toObjective, right);
    y = dot(toObjective, up);
    const float l = std::sqrt(x * x + y * y);
    if (!std::isfinite(l) || l < 1e-4f) {
        x = 0, y = -1; // straight behind: point down (turn round)
        return true;
    }
    x /= l, y /= l;
    return true;
}
// The readout under the diamond: "OBJECTIVE 123 M" (or "MISSION 1.2 KM").
inline void label(char* out, size_t size, uint32_t source, float metres) {
    if (!out || !size)
        return;
    out[0] = 0;
    if (!std::isfinite(metres) || metres < 0)
        return;
    const char* what = sourceLabel(source);
    if (metres >= 1000.f)
        std::snprintf(out, size, "%s %.1f KM", what, metres / 1000.f);
    else
        std::snprintf(out, size, "%s %d M", what, static_cast<int>(metres + .5f));
}
} // namespace spidy::story077g
