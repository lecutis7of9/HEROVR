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
story077d::Seen at(uint64_t id, Vec3 w, float age, const char* cls) {
    return story077d::Seen{id, w, age, k(cls), true, true};
}
} // namespace

int main() {
    const Vec3 hero{100, 0, 100};
    check(sourceOf(k(".?AVAddPOIAction@@")) == poi && sourceOf(k(".?AVVehiclePushable@@")) == interactable &&
              sourceOf(k(".?AVMissionLocationProxy@@")) == missionStart &&
              sourceOf(k(".?AVGrabAndThrowable@@")) == none && sourceOf(k(".?AVProximityOffer@@")) == none,
          "sources");
    story077d::Seen s[5]{};
    s[0] = at(1, {400, 0, 100}, 5, ".?AVMissionLocationProxy@@");  // 300 m
    s[1] = at(2, {140, 0, 100}, 50, ".?AVVehiclePushable@@");      // 40 m
    s[2] = at(3, {120, 0, 100}, 50, ".?AVPickupCarryableOffer@@"); // 20 m
    s[3] = at(4, {100, 0, 900}, 9, ".?AVAddPOIAction@@");          // 800 m, newer
    s[4] = at(5, {100, 0, 600}, 3, ".?AVAddPOIAction@@");          // 500 m, newest
    Mark m = pick(s, 5, hero, true, waypointsOn);
    check(m.shown && m.source == poi && m.id == 5 && std::fabs(m.metres - 500) < .1f, "the newest POI first");
    m = pick(s, 3, hero, true, waypointsOn);
    check(m.shown && m.source == interactable && m.id == 3, "then the nearest objective interactable");
    m = pick(s, 1, hero, true, waypointsOn);
    check(m.shown && m.source == missionStart && m.id == 1, "then a mission start");
    check(!pick(s, 1, hero, true, waypointsObjective).shown, "objective: no mission starts");
    check(!pick(s, 5, hero, true, waypointsOff).shown, "off: none");
    check(!pick(s, 5, hero, false, waypointsOn).shown && !pick(nullptr, 3, hero, true, waypointsOn).shown, "unknown: none");
    story077d::Seen near[3]{};
    near[0] = at(7, {101, 0, 100}, 1, ".?AVWaterPipe@@"); // 1 m: you are there
    near[1] = at(8, {0, 0, 0}, 1, ".?AVAddPOIAction@@");  // the origin: no place
    near[2] = at(9, {NAN, 0, 0}, 1, ".?AVAddPOIAction@@");
    check(!pick(near, 3, hero, true, waypointsOn).shown, "too near / origin / NaN: none");
    // View: ahead in view, behind out of it.
    check(!outOfView({0, 0, -10}, {0, 0, -1}) && outOfView({10, 0, 0}, {0, 0, -1}) && outOfView({0, 0, 10}, {0, 0, -1}),
          "view cone");
    float x{}, y{};
    check(edgeWay({10, 0, 0}, {1, 0, 0}, {0, 1, 0}, x, y) && x > .99f, "edge arrow right");
    char text[48];
    label(text, sizeof text, poi, 123.4f);
    check(!std::strcmp(text, "OBJECTIVE 123 M"), "label metres");
    label(text, sizeof text, missionStart, 1234.f);
    check(!std::strcmp(text, "MISSION 1.2 KM"), "label km");
    label(text, sizeof text, poi, NAN);
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
    check(waypointsOn == 0, "default on");
    if (failures)
        std::printf("%d failure(s)\n", failures);
    else
        std::printf("story077g: all passed\n");
    return failures ? 1 : 0;
}
