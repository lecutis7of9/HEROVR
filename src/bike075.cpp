// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
// Made by Agent Mulder.
// Version 0.0.75 MOTORCYCLE (bike075.hpp).
#include "spidy/bike075.hpp"

namespace spidy::bike075 {

namespace {
// MEASURED by tools/bike075/convert.py (layout.json): 2.85 x .72 x 1.13 m, 2.23 m between the axles, the seat's
// top .574 m, the grips .843 m up and .134 m behind the axles' middle, .577 m from the pivot (CANDIDATE in game).
constexpr Layout table = {2.8531f, .7172f, 1.1268f, 2.23f,
                          {0, 1.3043f, .30f},          // eye: the seat's top + .73 (CANDIDATE)
                          {0, .8432f, -.40f},          // pivot: where the yoke meets the cowl (CANDIDATE)
                          {-.2202f, .8432f, .1343f}, {.2181f, .8432f, .1343f},
                          {-.3586f, .6243f, .40f},     // get off: a grip beside the seat's left
                          {-.2202f, .8432f, .1343f},   // get on: the left grip
                          {-1.0086f, 0, .30f},         // step off .65 m out of the left side
                          {0, 1.0537f, -.15f}};        // the screen over the yoke's middle
} // namespace

const Layout& layout() { return table; }

van072::Tune tune() {
    const Layout& l = layout();
    van072::Tune t{};
    t.handlebar = true;
    t.handleOut = l.handleOut;
    t.handleIn = l.handleIn;
    t.eye = l.eye;
    t.hub = l.hub;
    t.axis = {0, 1, 0};
    t.exit = l.exit;
    t.length = l.length;
    t.width = l.width;
    t.height = l.height;
    t.wheelBase = l.wheelBase;
    // the grips' circle round the pivot (the bars' half span seen from above)
    t.wheelRadius = .5772f;
    t.rimNear = .10f;
    t.wheelGuard = .05f;
    // handlebars: half a radian either way (a car's wheel: 1.2 turns), the front wheel .35 rad at full lock
    t.wheelLock = .5f;
    t.steerMost = .35f;
    t.topSpeed = 22.f;
    t.accel = 7.f;
    t.brake = 12.f;
    return t;
}

} // namespace spidy::bike075
