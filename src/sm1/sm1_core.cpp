// HERO VR 0.0.75 SM1: Mulder's Heroes add-on core (pure, no game memory). See include/spidy/sm1_addon.hpp.
// iilyn's Spider-Man VR mod is Spidy by Ilya Mezerowsky (GPL-3.0, https://github.com/IlyaMez/SpidyVR).
#include "spidy/sm1_addon.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace spidy::sm1 {
const char* evidenceText(Evidence e) {
    return e == Evidence::confirmed ? "CONFIRMED" : "CANDIDATE";
}
const char* stateText(State s) {
    return s == State::ready ? "ready" : "no-op until SM1 research";
}

static char lowerAscii(char c) {
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}
bool sameFileName(std::string_view a, std::string_view b) {
    if (a.size() != b.size())
        return false;
    for (size_t i = 0; i < a.size(); ++i)
        if (lowerAscii(a[i]) != lowerAscii(b[i]))
            return false;
    return true;
}

Verdict judge(const HostInfo& h) {
    if (!sameFileName(h.exeName, hostExe))
        return Verdict::wrongGame;
    for (bool loaded : h.spidy)
        if (!loaded)
            return Verdict::spidyMissing;
    if (!h.vr)
        return Verdict::vrNotStarted;
    if (h.version != hostVersion)
        return Verdict::otherVersion;
    return Verdict::ok;
}
bool runs(Verdict v) {
    return v == Verdict::ok || v == Verdict::otherVersion;
}
const char* verdictText(Verdict v) {
    switch (v) {
    case Verdict::ok:
        return "the game and iilyn's mod are as expected";
    case Verdict::wrongGame:
        return "this is not Spider-Man.exe; the add-on stays idle";
    case Verdict::otherVersion:
        return "the game is not version 4.630.0.0 (iilyn's supported build); the add-on runs but stays read only";
    case Verdict::spidyMissing:
        return "iilyn's modules are not all loaded; the add-on stays idle";
    case Verdict::vrNotStarted:
        return "iilyn's VR module is not loaded yet; the add-on stays idle";
    }
    return "unknown";
}

static std::string_view trim(std::string_view s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t'))
        s.remove_prefix(1);
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r'))
        s.remove_suffix(1);
    return s;
}
static bool parseBool(std::string_view v, bool& out) {
    if (v == "1" || v == "true" || v == "on" || v == "yes") {
        out = true;
        return true;
    }
    if (v == "0" || v == "false" || v == "off" || v == "no") {
        out = false;
        return true;
    }
    return false;
}
Config parseConfig(std::string_view text) {
    Config c;
    while (!text.empty()) {
        const size_t nl = text.find('\n');
        std::string_view line = trim(text.substr(0, nl));
        text = nl == std::string_view::npos ? std::string_view{} : text.substr(nl + 1);
        if (line.empty() || line.front() == '#' || line.front() == ';' || line.front() == '[')
            continue;
        const size_t eq = line.find('=');
        if (eq == std::string_view::npos)
            continue;
        const std::string_view key = trim(line.substr(0, eq)), value = trim(line.substr(eq + 1));
        bool b = false;
        if (key == "heroes" && parseBool(value, b))
            c.heroes = b;
        else if (key == "wrist_menu" && parseBool(value, b))
            c.wristMenu = b;
        else if (key == "flight" && parseBool(value, b))
            c.flight = b;
        else if (key == "hero" && !value.empty() && value.size() < 4 &&
                 std::all_of(value.begin(), value.end(), [](char ch) { return ch >= '0' && ch <= '9'; })) {
            const unsigned long n = std::strtoul(std::string(value).c_str(), nullptr, 10);
            if (n < heroCount)
                c.hero = static_cast<uint32_t>(n);
        }
    }
    return c;
}
std::string configText(const Config& c) {
    std::string s = "# Mulder's Heroes add-on (HERO VR) for iilyn's Spider-Man VR mod. The HERO VR launcher writes this file.\n";
    s += std::string("heroes=") + (c.heroes ? "1" : "0") + "\n";
    s += std::string("wrist_menu=") + (c.wristMenu ? "1" : "0") + "\n";
    s += std::string("flight=") + (c.flight ? "1" : "0") + "\n";
    s += "hero=" + std::to_string(c.hero) + "\n";
    return s;
}

const char* pageName(Page p) {
    switch (p) {
    case Page::heroes:
        return "HEROES";
    case Page::flight:
        return "FLIGHT";
    case Page::addon:
        return "ADD-ON";
    case Page::count:
        break;
    }
    return "";
}
int WristMenu::rows() const {
    switch (page) {
    case Page::heroes:
        return static_cast<int>(heroCount);
    case Page::flight:
        return 1;
    case Page::addon:
        return 1;
    case Page::count:
        break;
    }
    return 1;
}
Pick WristMenu::step(const WristInput& in) {
    const float dt = std::isfinite(in.dt) ? std::clamp(in.dt, 0.f, .25f) : 0.f;
    const float palm = std::isfinite(in.palmUp) ? in.palmUp : 0.f;
    if (!open) {
        held = palm >= openDot ? held + dt : 0;
        if (held >= openHold) {
            open = true;
            held = 0;
            stickArmed = triggerArmed = false; // a stick or trigger already held never picks on opening
        }
        return Pick::none;
    }
    held = palm <= closeDot ? held + dt : 0;
    if (held >= closeHold) {
        open = false;
        held = 0;
        return Pick::closeMenu;
    }
    const float stick = std::isfinite(in.stickY) ? in.stickY : 0.f;
    if (std::fabs(stick) < .3f)
        stickArmed = true;
    else if (stickArmed && std::fabs(stick) > .6f) {
        stickArmed = false;
        const int n = rows() + 1; // the last row moves to the next page
        row = ((row + (stick > 0 ? -1 : 1)) % n + n) % n;
    }
    if (!in.trigger) {
        triggerArmed = true;
        return Pick::none;
    }
    if (!triggerArmed)
        return Pick::none;
    triggerArmed = false;
    if (row == rows()) {
        page = static_cast<Page>((static_cast<uint32_t>(page) + 1) % static_cast<uint32_t>(Page::count));
        row = 0;
        return Pick::none;
    }
    if (page == Page::heroes)
        return Pick::hero;
    if (page == Page::flight)
        return Pick::flightToggle;
    return Pick::none;
}

void stepFlight(FlightState& s, const FlightInput& in, const FlightTuning& t) {
    const float dt = std::isfinite(in.dt) ? std::clamp(in.dt, 0.f, .1f) : 0.f;
    if (!finite(s.velocity) || !std::isfinite(s.lean))
        s = {};
    if (!in.on) {
        s.velocity = {};
        s.lean += (0 - s.lean) * std::min(1.f, fly_lean::ease * dt);
        return;
    }
    const auto thrust = [](float v) { return std::isfinite(v) ? std::clamp(v, 0.f, 1.f) : 0.f; };
    Vec3 push = normalized(finite(in.leftAim) ? in.leftAim : Vec3{}) * thrust(in.leftThrust) +
                normalized(finite(in.rightAim) ? in.rightAim : Vec3{}) * thrust(in.rightThrust);
    push = limited(push, 1.f);
    s.velocity += push * (t.accel * dt);
    s.velocity = s.velocity * std::max(0.f, 1 - t.drag * dt);
    s.velocity = limited(s.velocity, t.maxSpeed);
    const float goal = fly_lean::target(s.velocity, in.facing);
    s.lean += (goal - s.lean) * std::min(1.f, fly_lean::ease * dt);
}
} // namespace spidy::sm1
