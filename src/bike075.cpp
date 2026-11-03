// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
// Made by Agent Mulder.
// Version 0.0.75 MOTORCYCLE, 0.0.76 HARLEYS (bike075.hpp).
#include "spidy/bike075.hpp"
#include "spidy/launcher_text.hpp"

#include <algorithm>
#include <exception>

namespace spidy::bike075 {

namespace {
bool fine(float f) { return std::isfinite(f); }
bool fine(Vec3 v) { return fine(v.x) && fine(v.y) && fine(v.z); }

bool number(const launcher::Json& j, const char* key, float& out, std::string& why) {
    const launcher::Json* v = j.get(key);
    if (!v || v->type != launcher::Json::Type::number || !std::isfinite(v->number) || std::abs(v->number) > 100) {
        why = std::string("layout.json: no usable number \"") + key + "\"";
        return false;
    }
    out = static_cast<float>(v->number);
    return true;
}

bool point(const launcher::Json& j, const char* key, Vec3& out, std::string& why) {
    const launcher::Json* v = j.get(key);
    if (!v || v->type != launcher::Json::Type::array || v->items.size() != 3) {
        why = std::string("layout.json: no [x, y, z] \"") + key + "\"";
        return false;
    }
    float c[3];
    for (int k = 0; k < 3; ++k) {
        const launcher::Json& n = v->items[static_cast<size_t>(k)];
        if (n.type != launcher::Json::Type::number || !std::isfinite(n.number) || std::abs(n.number) > 100) {
            why = std::string("layout.json: \"") + key + "\" is not three numbers";
            return false;
        }
        c[k] = static_cast<float>(n.number);
    }
    out = {c[0], c[1], c[2]};
    return true;
}

// Rodrigues: v turned `a` radians round unit k.
Vec3 turned(Vec3 v, Vec3 k, float a) {
    const float c = std::cos(a), s = std::sin(a);
    return v * c + cross(k, v) * s + k * (dot(k, v) * (1 - c));
}

float horizontal(Vec3 a, Vec3 b) { return std::hypot(a.x - b.x, a.z - b.z); }
} // namespace

bool parseLayout(std::string_view text, Layout& out, std::string& why) {
    why.clear();
    try {
        if (text.empty() || text.size() > mostLayoutBytes) {
            why = "layout.json: empty or over 64 KB";
            return false;
        }
        const auto json = launcher::parseJson(text);
        if (!json || json->type != launcher::Json::Type::object) {
            why = "layout.json: not a JSON object";
            return false;
        }
        const launcher::Json& j = *json;
        Layout l;
        if (!number(j, "length", l.length, why) || !number(j, "width", l.width, why) || !number(j, "height", l.height, why) ||
            !number(j, "wheelBase", l.wheelBase, why) || !number(j, "gripRadius", l.gripRadius, why) || !point(j, "eye", l.eye, why) ||
            !point(j, "hub", l.hub, why) || !point(j, "gripLeft", l.gripLeft, why) || !point(j, "gripRight", l.gripRight, why) ||
            !point(j, "handleIn", l.handleIn, why) || !point(j, "handleOut", l.handleOut, why) || !point(j, "exit", l.exit, why) ||
            !point(j, "screen", l.screen, why))
            return false;
        if (j.get("steerAxis") && !point(j, "steerAxis", l.steerAxis, why))
            return false;
        const float axisLength = length(l.steerAxis);
        if (!fine(axisLength) || axisLength < .5f || axisLength > 2.f) {
            why = "layout.json: steerAxis is not a direction";
            return false;
        }
        l.steerAxis = l.steerAxis * (1 / axisLength);
        if (l.steerAxis.y < std::cos(mostRake) || std::abs(l.steerAxis.x) > .02f) {
            why = "layout.json: steerAxis leans over " + std::to_string(static_cast<int>(mostRake * 57.3f)) + " degrees or sideways";
            return false;
        }
        if (j.get("tracker")) {
            if (!point(j, "tracker", l.tracker, why))
                return false;
        } else {
            l.tracker = l.screen + Vec3{0, trackerOverScreen, 0};
        }
        // the bike's size
        if (!(l.length >= leastLength && l.length <= mostLength && l.width > .3f && l.width <= mostWidth && l.height > .6f &&
              l.height <= mostHeight && l.wheelBase > 1.f && l.wheelBase < l.length)) {
            why = "layout.json: the size is not a motorcycle's (" + std::to_string(l.length) + " m long)";
            return false;
        }
        // every point in (or, for the step-off and the get-off grip, beside) the bike's box
        const float hx = l.width / 2, hz = l.length / 2;
        const auto inBox = [&](Vec3 p, float slack) {
            return fine(p) && std::abs(p.x) <= hx + slack && p.y >= -.01f && p.y <= l.height + slack && std::abs(p.z) <= hz + slack;
        };
        if (!inBox(l.hub, 0) || !inBox(l.gripLeft, 0) || !inBox(l.gripRight, 0) || !inBox(l.handleOut, 0) || !inBox(l.handleIn, .4f) ||
            !inBox(l.exit, 1.5f) || !inBox(l.eye, .8f) || !inBox(l.screen, .1f) || !inBox(l.tracker, .2f)) {
            why = "layout.json: a point is outside the bike";
            return false;
        }
        // the grips: left and right, at the hub's height, on its rear half, on the circle of gripRadius
        if (!(l.gripLeft.x < -.1f && l.gripRight.x > .1f && l.gripRadius > .15f && l.gripRadius < 1.f)) {
            why = "layout.json: the grips are not either side";
            return false;
        }
        for (const Vec3 g : {l.gripLeft, l.gripRight})
            if (std::abs(g.y - l.hub.y) > .08f || g.z < l.hub.z || std::abs(horizontal(g, l.hub) - l.gripRadius) > .08f) {
                why = "layout.json: a grip is off the bars' circle round the hub";
                return false;
            }
        // the screen and the tracker: between the grips, the eye behind them (they face him)
        const float gl = l.gripLeft.x, gr = l.gripRight.x;
        if (!(l.screen.x > gl && l.screen.x < gr && l.tracker.x > gl && l.tracker.x < gr && l.eye.z > l.screen.z && l.eye.z > l.tracker.z &&
              l.eye.y > l.hub.y)) {
            why = "layout.json: the screen or tracker is not between the bars, before the eye";
            return false;
        }
        out = l;
        return true;
    } catch (const std::exception&) {
        why = "layout.json: could not be read (out of memory?)";
        return false;
    }
}

van072::Tune tune(const Layout& l) {
    van072::Tune t{};
    t.handlebar = true;
    t.handleOut = l.handleOut;
    t.handleIn = l.handleIn;
    t.eye = l.eye;
    t.hub = l.hub;
    t.axis = {0, 1, 0}; // the hands' circle: round +y (the steer part is drawn round the rake: steerAxes)
    t.exit = l.exit;
    t.length = l.length;
    t.width = l.width;
    t.height = l.height;
    t.wheelBase = l.wheelBase;
    // the grips' circle round the pivot (the bars' half span seen from above)
    t.wheelRadius = l.gripRadius;
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

void steerAxes(float angle, const Layout& l, Vec3& up, Vec3& toward) {
    const float a = std::isfinite(angle) ? angle : 0.f;
    const float n = length(l.steerAxis);
    const Vec3 k = fine(l.steerAxis) && fine(n) && n > .5f ? l.steerAxis * (1 / n) : Vec3{0, 1, 0};
    // van072::wheelAxes (handlebar): toward {-sin a, 0, cos a} - a turn of -a round +y
    up = turned(Vec3{0, 1, 0}, k, -a);
    toward = turned(Vec3{0, 0, 1}, k, -a);
}

} // namespace spidy::bike075
