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
constexpr Layout table[cars] = {
    {6.398f, 2.538f, 1.417f, {0, .092f, .0644f}, {-.37f, 1.112f, .2444f}, {-.36f, .832f, -.2656f}, {0, .342f, .94f},
     {-.665f, .692f, -.1556f}, {-.5993f, .892f, -.0356f}, {-2.019f, 0, .2444f}, {0, .782f, -.5161f}},
    {6.077f, 2.423f, 1.409f, {0, .03f, 0}, {-.37f, 1.05f, .18f}, {-.36f, .77f, -.33f}, {0, .342f, .94f},
     {-.665f, .63f, -.22f}, {-.7041f, .83f, -.1f}, {-1.962f, 0, .18f}, {0, .72f, -.5805f}},
    {6.506f, 2.117f, 2.475f, {0, .444f, -.407f}, {-.37f, 1.464f, -.227f}, {-.36f, 1.184f, -.737f}, {0, .342f, .94f},
     {-.665f, 1.044f, -.627f}, {-.5439f, 1.244f, -.507f}, {-1.809f, 0, -.227f}, {0, 1.134f, -.9875f}},
    {6.381f, 2.316f, 2.859f, {0, .1344f, .084f}, {-.37f, 1.1544f, .264f}, {-.36f, .8744f, -.246f}, {0, .342f, .94f},
     {-.665f, .7344f, -.136f}, {-.7015f, .9344f, -.016f}, {-1.908f, 0, .264f}, {0, .8244f, -.4965f}},
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
