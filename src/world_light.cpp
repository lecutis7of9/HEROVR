// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
// Made by Agent Mulder.
// HERO VR 0.0.78 WORLD LIGHT: the frame's light (world_light.hpp).
#include "spidy/world_light.hpp"

#include <cstdio>
#include <mutex>

namespace spidy::world_light {
namespace {
std::mutex lock;
Light now = studio(noHook);
} // namespace

Light current() {
    std::lock_guard g(lock);
    return now;
}
void setCurrent(const Light& l) {
    std::lock_guard g(lock);
    now = l;
}
float shadowStrength(const Light& l, uint32_t shadowMode) {
    if (!l.world || shadowMode == shadowOff)
        return 0.f;
    // A sun below the horizon casts nothing on the street (the moon's light is the same record: kept).
    const float up = l.toSun.y;
    return up > -.05f ? std::clamp((up + .05f) * 8.f, 0.f, 1.f) * .85f : 0.f;
}
std::string describe(const Light& l, const Sample& s) {
    char line[420];
    std::snprintf(line, sizeof line,
                  "%s: to sun (%.2f %.2f %.2f), key colour (%.3g %.3g %.3g), adapted luminance %.4g, actual %.4g, fog (%.3g %.3g "
                  "%.3g), probe x(%.2f %.2f %.2f), time %.3f, penumbra %.3f -> sun (%.2f %.2f %.2f) sky (%.2f %.2f %.2f), "
                  "slot %u",
                  whyNames[l.why < whys ? l.why : 0], s.toLight[0], s.toLight[1], s.toLight[2], s.color[0], s.color[1],
                  s.color[2], s.adapted, s.actual, s.fog[0], s.fog[1], s.fog[2], s.probe[0], s.probe[1], s.probe[2],
                  s.timeOfDay, s.penumbra, l.sun.x, l.sun.y, l.sun.z, l.skyUp.x, l.skyUp.y, l.skyUp.z, s.slot);
    return line;
}
} // namespace spidy::world_light
