// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
// Made by Agent Mulder.
// Version 0.0.74 test 1 BATMOBILE (batmobile074.hpp).
#include "spidy/batmobile074.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace spidy::batmobile074 {

namespace {
// MEASURED by tools/batmobile074/convert.py (each car's layout.json); exit x = -(width / 2 + .75). Owner t1164u:
// every car's glass and canopy as the model came (FOREVER 2's canopy back: its width and outer handle re-measured).
// 0.0.74 test 2 (owner: "the seat is low"; t1171u): the eyes 12 cm higher (1989 1.232 m: over the cabin cut,
// clear of the wheel by .365 m), the far handle at -.83 m; from tools/batmobile074/convert.py out3.
// 0.0.74 test 3 review (ROOF): each car's own eye height under its canopy (the head was IN the canopy at 1.14 over
// the floor) and the cockpit moved where the canopy leaves head room; from convert.py out5 (Made by Agent Mulder).
constexpr Layout table[cars] = {
    {6.398f, 2.538f, 1.417f, {.0699f, .092f, .3643f}, {-.3001f, 1.082f, .6143f}, {-.2901f, .832f, .0343f}, {0, .342f, .94f},
     {-.5951f, .692f, .1443f}, {-.9258f, .892f, .2643f}, {-2.019f, 0, .6143f}, {.0699f, .782f, -.2162f}},
    {6.077f, 2.423f, 1.409f, {0, .03f, .3f}, {-.37f, 1.12f, .55f}, {-.36f, .77f, -.03f}, {0, .342f, .94f},
     {-.665f, .63f, .08f}, {-.83f, .83f, .2f}, {-1.9615f, 0, .55f}, {0, .72f, -.2805f}},
    {6.506f, 2.117f, 2.475f, {.0703f, .1998f, -.407f}, {-.2997f, 1.1698f, -.157f}, {-.2897f, .9398f, -.737f}, {0, .342f, .94f},
     {-.5947f, .7998f, -.627f}, {-.7597f, .9998f, -.507f}, {-1.8085f, 0, -.157f}, {.0703f, .8898f, -.9875f}},
    {6.381f, 2.316f, 2.859f, {.11f, .1344f, .6838f}, {-.26f, 1.2044f, .9338f}, {-.25f, .8744f, .3538f}, {0, .342f, .94f},
     {-.555f, .7344f, .4638f}, {-.72f, .9344f, .5838f}, {-1.908f, 0, .9338f}, {.11f, .8244f, .1033f}},
};
} // namespace

const Layout& layout(uint32_t car) { return table[clampCar(car)]; }

van072::Tune tune(uint32_t car) {
    const Layout& l = layout(car);
    van072::Tune t{};
    t.handleOut = l.handleOut;
    t.handleIn = l.handleIn;
    t.eye = l.eye;
    t.hub = l.hub;
    t.axis = l.axis;
    t.exit = l.exit;
    t.length = l.length;
    t.width = l.width;
    // the box the summon and the bumper check: the body to its roof (FOREVER's fins included)
    t.height = l.height;
    t.wheelBase = l.length * .56f;
    t.wheelRadius = .185f;
    return t;
}

} // namespace spidy::batmobile074
