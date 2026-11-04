// HERO VR 0.0.77 WAYPOINT (include/spidy/story077g.hpp, src/waypoint_mark.cpp), pure.
#include "spidy/story077g.hpp"
#include "spidy/waypoint_mark.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

using namespace spidy;
using namespace spidy::story077g;

namespace {
int failures = 0;
void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++failures;
    }
}
uint32_t k(const char* cls) { return static_cast<uint32_t>(story077d::kindOf(cls)); }
} // namespace

int main() {
    const Vec3 hero{100, 0, 100};
    // cut3: only the game's own objective POI types; every guess (mission starts, interactables) is gone.
    check(sourceOfType(".?AVPOITypeObjective@@") == objective && sourceOfType(".?AVPOITypeQuest@@") == quest &&
              sourceOfType(".?AVPOITypeZipToObjective@@") == zipToObjective &&
              sourceOfType(".?AVPOITypeCustomWaypoint@@") == customWaypoint &&
              sourceOfType(".?AVPOITypeMissionStart@@") == none && sourceOfType(".?AVPOITypeCrime@@") == none &&
              sourceOfType(nullptr) == none,
          "sources: the objective's POI types only");
    check(isPoiType(".?AVPOITypeObjective@@") && !isPoiType(".?AVPOIConfig@@") && poiHolder(".?AVPOIConfig@@") &&
              poiComponent(".?AVPointOfInterestPrius@@") && !poiComponent(".?AVMissionLocationProxy@@") &&
              !poiComponent(".?AVVehiclePushable@@"),
          "POI names");
    check(poiComponentKind(k(".?AVPointOfInterestPrius@@")) && !poiComponentKind(k(".?AVProximityOffer@@")),
          "POI component kinds");
    Candidate s[4]{};
    s[0] = {customWaypoint, 1, {120, 0, 100}};  // 20 m
    s[1] = {quest, 2, {100, 0, 400}};           // 300 m
    s[2] = {objective, 3, {100, 0, 900}};       // 800 m
    s[3] = {objective, 4, {100, 0, 600}};       // 500 m
    Mark m = pick(s, 4, hero, true, waypointsScan);
    check(m.shown && m.source == objective && m.id == 4 && std::fabs(m.metres - 500) < .1f, "the nearest objective first");
    m = pick(s, 2, hero, true, waypointsAlways);
    check(m.shown && m.source == quest && m.id == 2, "then a quest");
    m = pick(s, 1, hero, true, waypointsScan);
    check(m.shown && m.source == customWaypoint, "then his map waypoint");
    check(!pick(s, 4, hero, true, waypointsOff).shown, "off: none");
    check(!pick(s, 4, hero, false, waypointsScan).shown && !pick(nullptr, 3, hero, true, waypointsScan).shown,
          "unknown: none");
    check(!pick(s, 0, hero, true, waypointsScan).shown, "no objective POI: nothing (never a guess)");
    Candidate near[4]{};
    near[0] = {objective, 7, {101, 0, 100}}; // 1 m: you are there
    near[1] = {objective, 8, {0, 0, 0}};     // the origin: no place
    near[2] = {objective, 9, {NAN, 0, 0}};
    near[3] = {none, 10, {200, 0, 100}};     // not the objective
    check(!pick(near, 4, hero, true, waypointsScan).shown, "too near / origin / NaN / not objective: none");
    // cut3 (pt1803u): after a scan, about 8 s, fading out.
    check(scanAlpha(waypointsScan, 0) == 1 && scanAlpha(waypointsScan, 6) == 1, "scan: shown");
    check(scanAlpha(waypointsScan, 7.25f) > .4f && scanAlpha(waypointsScan, 7.25f) < .6f, "scan: fading");
    check(scanAlpha(waypointsScan, 8) == 0 && scanAlpha(waypointsScan, 1e9f) == 0 && scanAlpha(waypointsScan, -1) == 0,
          "scan: hidden after 8 s / before any scan");
    check(scanAlpha(waypointsAlways, 1e9f) == 1 && scanAlpha(waypointsOff, 0) == 0, "always / off");
    // View: ahead in view, behind out of it.
    check(!outOfView({0, 0, -10}, {0, 0, -1}) && outOfView({10, 0, 0}, {0, 0, -1}) && outOfView({0, 0, 10}, {0, 0, -1}),
          "view cone");
    float x{}, y{};
    check(edgeWay({10, 0, 0}, {1, 0, 0}, {0, 1, 0}, x, y) && x > .99f, "edge arrow right");
    char text[48];
    label(text, sizeof text, objective, 123.4f);
    check(!std::strcmp(text, "OBJECTIVE 123 M"), "label metres");
    label(text, sizeof text, customWaypoint, 1234.f);
    check(!std::strcmp(text, "WAYPOINT 1.2 KM"), "label km");
    label(text, sizeof text, objective, NAN);
    check(!text[0], "label NaN: none");
    // Drawing: small, some vertices; nonsense none.
    std::vector<Vertex> v;
    check(waypoint_mark::diamond(v, "OBJECTIVE 40 M", {0, 0, -12}, {1, 0, 0}, {0, 1, 0}, 12 * diamondPerMetre, 1) > 12,
          "a diamond draws");
    bool small = true;
    for (const auto& q : v)
        small &= finite(q.position) && std::fabs(q.position.x) < 2.f && std::fabs(q.position.y) < 1.f;
    check(small, "a diamond is small (not a screen)");
    v.clear();
    check(waypoint_mark::diamond(v, "", {NAN, 0, 0}, {1, 0, 0}, {0, 1, 0}, .2f, 1) == 0 && v.empty(), "NaN: none");
    check(waypoint_mark::arrow(v, {0, 0, -1.4f}, {1, 0, 0}, {0, 1, 0}, 1, 0, edgeSize, 1) == 6, "an arrow draws");
    check(waypoint_mark::arrow(v, {0, 0, -1.4f}, {1, 0, 0}, {0, 1, 0}, 0, 0, edgeSize, 1) == 0, "no way: none");
    check(waypointsScan == 0, "default scan");
    if (failures)
        std::printf("%d failure(s)\n", failures);
    else
        std::printf("story077g: all passed\n");
    return failures ? 1 : 0;
}
