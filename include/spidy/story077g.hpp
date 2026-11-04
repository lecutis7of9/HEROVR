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
// cut3 (pt1802u: "it shows up but it doesnt take you to your missions. also why is it one you made? it should just
// be the game own one working"): his cut2 report had 0 POI picks - the diamond pointed at the nearest
// MissionLocationProxy (a mission start 236-570 m off), never the objective. Those guesses are gone. The only source
// now is the game's own HUD marker data: a PointOfInterest / POIPrius component (the game's POI, the thing its HUD
// draws a marker for) whose POI type (an object it points at, found by its RTTI name) is the objective's:
//   POITypeObjective, POITypeQuest, POITypeZipToObjective (the game's objective markers), POITypeCustomWaypoint
//   (the waypoint he sets on the game's map). Every class name is CONFIRMED in Spider-Man2.exe's RTTI; that the
//   components are in the scan's registry and point at their type is CANDIDATE (the diagnostics count both).
// Nothing found: nothing shown (never a guess). And only after a scan (pt1803u: "it should ONLY appear when the
// player does the scan"): waypoints = scan shows it about 8 s after each scan, fading out. CUT5: off by default -
// the POI source is not proven yet (the POI DUMP diagnostics stay), so no diamond unless waypoints = scan or always.
// Pure and allocation-free.
#include "math.hpp"
#include "story077d.hpp"
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace spidy::story077g {
// live.txt waypoints: 0 scan (shown for showSeconds after each scan), 1 always, 2 off (the default since CUT5).
enum Waypoints : uint32_t { waypointsScan = 0, waypointsAlways = 1, waypointsOff = 2 };
// The game's own POI types that are the objective, best first.
enum Source : uint32_t { none = 0, objective = 1, quest = 2, zipToObjective = 3, customWaypoint = 4, sources = 5 };
inline const char* sourceName(uint32_t s) {
    switch (s) {
    case objective: return "POITypeObjective";
    case quest: return "POITypeQuest";
    case zipToObjective: return "POITypeZipToObjective";
    case customWaypoint: return "POITypeCustomWaypoint";
    default: return "none";
    }
}
inline const char* sourceLabel(uint32_t s) {
    switch (s) {
    case none: return "";
    case customWaypoint: return "WAYPOINT";
    default: return "OBJECTIVE";
    }
}
// The POI component classes the scan reads the type of (story077d marker kinds).
inline bool poiComponent(const char* cls) {
    if (!cls)
        return false;
    constexpr const char* names[] = {".?AVPointOfInterest@@", ".?AVPointOfInterestPrius@@", ".?AVPOIPrius@@",
                                     ".?AVPOIPriusBase@@", ".?AVPOIGamePrius@@", ".?AVSyncedPOIObject@@",
                                     ".?AVPOIGame@GUI2@@", ".?AVPOI@GUI2@@"};
    for (const char* n : names)
        if (!std::strcmp(cls, n))
            return true;
    return false;
}
inline bool poiComponentKind(uint32_t k) {
    const auto* d = story077d::kind(k);
    return d && poiComponent(d->cls);
}
inline bool isPoiType(const char* name) { return name && !std::strncmp(name, ".?AVPOIType", 11); }
// An object the POI points at that may hold its type one step further (its config / library entry).
inline bool poiHolder(const char* name) {
    return name && (std::strstr(name, "POIConfig") || std::strstr(name, "POIGameConfig") ||
                    std::strstr(name, "POILibraryEntry") || std::strstr(name, "POIInstanceData"));
}
// A POI type's source; none for every other type (a mission start, a crime, a collectible, combat brackets).
inline uint32_t sourceOfType(const char* name) {
    if (!name)
        return none;
    if (!std::strcmp(name, ".?AVPOITypeObjective@@")) return objective;
    if (!std::strcmp(name, ".?AVPOITypeQuest@@")) return quest;
    if (!std::strcmp(name, ".?AVPOITypeZipToObjective@@")) return zipToObjective;
    if (!std::strcmp(name, ".?AVPOITypeCustomWaypoint@@")) return customWaypoint;
    return none;
}
inline constexpr float nearest = 2.f, farthest = 6000.f;
struct Candidate {
    uint32_t source{};
    uint64_t id{};
    Vec3 world{};
};
inline bool somewhere(Vec3 w) {
    return finite(w) && (std::fabs(w.x) + std::fabs(w.y) + std::fabs(w.z)) > 1.f;
}
struct Mark {
    bool shown{};
    uint32_t source{}, kind{};
    uint64_t id{};
    Vec3 world{};
    float metres{};
};
// The best source, then the nearest; a candidate 2-6000 m away. Off, or no hero: none.
inline Mark pick(const Candidate* c, uint32_t n, Vec3 hero, bool heroKnown, uint32_t mode) {
    Mark best;
    if (mode == waypointsOff || !heroKnown || !finite(hero) || !c)
        return best;
    float bestScore = 1e30f;
    for (uint32_t i = 0; i < n; ++i) {
        if (c[i].source == none || c[i].source >= sources || !somewhere(c[i].world))
            continue;
        const float m = length(c[i].world - hero);
        if (!std::isfinite(m) || m < nearest || m > farthest)
            continue;
        const float score = c[i].source * 1e5f + m;
        if (score < bestScore) {
            bestScore = score;
            best = Mark{true, c[i].source, 0, c[i].id, c[i].world, m};
        }
    }
    return best;
}
// After a scan (waypoints = scan): fully shown showSeconds - fadeSeconds, then fading to nothing at showSeconds.
inline constexpr float showSeconds = 8.f, fadeSeconds = 1.5f;
inline float scanAlpha(uint32_t mode, float sinceScan) {
    if (mode == waypointsAlways)
        return 1.f;
    if (mode != waypointsScan || !std::isfinite(sinceScan) || sinceScan < 0 || sinceScan >= showSeconds)
        return 0.f;
    const float left = showSeconds - sinceScan;
    return left >= fadeSeconds ? 1.f : left / fadeSeconds;
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
// The readout under the diamond: "OBJECTIVE 123 M" (or "WAYPOINT 1.2 KM").
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
