// HERO VR 0.0.75 SM1: tests for Mulder's Heroes add-on core (iilyn's Spider-Man VR mod is Spidy by Ilya
// Mezerowsky, GPL-3.0, https://github.com/IlyaMez/SpidyVR). Made by Agent Mulder.
#include "spidy/sm1_addon.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>

using namespace spidy;
using namespace spidy::sm1;

namespace {
int failures = 0;
void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++failures;
    }
}

HostInfo goodHost() {
    HostInfo h;
    h.exeName = "Spider-Man.exe";
    h.version = "4.630.0.0";
    for (bool& b : h.spidy)
        b = true;
    h.vr = true;
    return h;
}

void hostTests() {
    check(judge(goodHost()) == Verdict::ok, "the expected host passes");
    HostInfo h = goodHost();
    h.exeName = "SPIDER-MAN.EXE";
    check(judge(h) == Verdict::ok, "exe name is case-insensitive");
    h.exeName = "Spider-Man2.exe";
    check(judge(h) == Verdict::wrongGame && !runs(judge(h)), "Spider-Man 2 is not the host");
    h = goodHost();
    h.version = "4.629.0.0";
    check(judge(h) == Verdict::otherVersion && runs(judge(h)), "another version only warns");
    h = goodHost();
    h.spidy[2] = false;
    check(judge(h) == Verdict::spidyMissing && !runs(judge(h)), "a missing module of his stops the add-on");
    h = goodHost();
    h.vr = false;
    check(judge(h) == Verdict::vrNotStarted && !runs(judge(h)), "waits for his VR module");
    for (Verdict v : {Verdict::ok, Verdict::wrongGame, Verdict::otherVersion, Verdict::spidyMissing, Verdict::vrNotStarted})
        check(std::strlen(verdictText(v)) > 10, "every verdict has words");
    check(std::strlen(hostSha256) == 64, "the host hash is a sha256");
    for (const char* m : spidyModules)
        check(std::strncmp(m, "spidy_", 6) == 0, "his module names");
}

void featureTests() {
    int candidates = 0;
    for (const Feature& f : features) {
        check(std::strlen(f.note) > 5, "every feature has a note");
        if (f.evidence == Evidence::candidate) {
            ++candidates;
            check(f.state == State::stub, "every CANDIDATE feature is a no-op stub");
        }
    }
    check(candidates >= 4, "the game-changing features are CANDIDATE");
    GameLink link;
    check(!link.applyHero(1) && !link.applyFlight(FlightState{}) && !link.drawWristMenu(WristMenu{}), "the game link does nothing in 0.0.75");
}

void configTests() {
    Config d = parseConfig("");
    check(d.heroes && d.wristMenu && d.flight && d.hero == 0, "defaults");
    Config c = parseConfig("# c\r\nheroes=0\r\nflight = off\nwrist_menu=yes\nhero=3\nnoise\nhero=99\n[x]\n");
    check(!c.heroes && !c.flight && c.wristMenu && c.hero == 3, "parsed, bad hero ignored");
    Config bad = parseConfig("hero=-1\nhero=1e3\nheroes=maybe\n");
    check(bad.hero == 0 && bad.heroes, "bad values keep defaults");
    Config round = parseConfig(configText(c));
    check(round.heroes == c.heroes && round.flight == c.flight && round.wristMenu == c.wristMenu && round.hero == c.hero, "round trip");
}

void wristTests() {
    WristMenu m;
    WristInput in;
    in.dt = .1f;
    in.palmUp = .9f;
    in.trigger = true; // held while opening: must not pick
    for (int i = 0; i < 3; ++i)
        m.step(in);
    check(!m.open, "not open before the hold");
    m.step(in);
    check(m.open, "opens after 0.35 s palm up");
    check(m.step(in) == Pick::none, "a trigger held from before never picks");
    in.trigger = false;
    m.step(in);
    in.stickY = -.9f;
    m.step(in);
    m.step(in);
    check(m.row == 1, "one flick, one row");
    in.stickY = 0;
    m.step(in);
    in.trigger = true;
    check(m.step(in) == Pick::hero && m.row == 1, "picks the row's hero");
    in.trigger = false;
    m.step(in);
    // to the page row (last) and on to FLIGHT
    in.stickY = .9f;
    m.step(in); // up from 1 -> 0
    in.stickY = 0;
    m.step(in);
    in.stickY = .9f;
    m.step(in); // up from 0 wraps to the page row
    in.stickY = 0;
    m.step(in);
    check(m.row == m.rows(), "wraps to the page row");
    in.trigger = true;
    m.step(in);
    check(m.page == Page::flight && m.row == 0, "next page");
    in.trigger = false;
    m.step(in);
    in.trigger = true;
    check(m.step(in) == Pick::flightToggle, "flight toggle");
    in.trigger = false;
    in.palmUp = 0;
    m.step(in);
    m.step(in);
    check(m.step(in) == Pick::closeMenu, "closes after 0.25 s palm down");
    check(!m.open, "closed");
    in.palmUp = std::numeric_limits<float>::quiet_NaN();
    in.dt = std::numeric_limits<float>::infinity();
    m.step(in);
    check(!m.open, "NaN input never opens");
    for (uint32_t p = 0; p < static_cast<uint32_t>(Page::count); ++p)
        check(std::strlen(pageName(static_cast<Page>(p))) > 0, "page names");
}

void flightTests() {
    FlightState s;
    FlightInput in;
    in.on = true;
    in.dt = 1.f / 90;
    in.facing = {0, 0, 1};
    in.leftAim = in.rightAim = {0, 0, 1};
    in.leftThrust = in.rightThrust = 1;
    for (int i = 0; i < 90 * 20; ++i)
        stepFlight(s, in);
    check(length(s.velocity) <= 40.01f && s.velocity.z > 10, "accelerates forward, capped");
    check(s.lean > .5f, "leans forward at speed");
    in.leftThrust = in.rightThrust = 0;
    for (int i = 0; i < 90 * 10; ++i)
        stepFlight(s, in);
    check(length(s.velocity) < 1, "drag slows to a hover");
    in.on = false;
    s.velocity = {5, 0, 0};
    stepFlight(s, in);
    check(length(s.velocity) == 0, "off stops");
    s.velocity = {std::numeric_limits<float>::quiet_NaN(), 0, 0};
    in.on = true;
    in.leftThrust = std::numeric_limits<float>::infinity();
    stepFlight(s, in);
    check(finite(s.velocity) && std::isfinite(s.lean), "NaN never spreads");
}
} // namespace

int main() {
    hostTests();
    featureTests();
    configTests();
    wristTests();
    flightTests();
    if (failures) {
        std::printf("%d failure(s)\n", failures);
        return 1;
    }
    std::printf("sm1 add-on tests passed\n");
    return 0;
}
