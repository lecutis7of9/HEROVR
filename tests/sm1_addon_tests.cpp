// HERO VR 0.0.76 SM1: tests for Mulder's Heroes add-on core (iilyn's Spider-Man VR mod is Spidy by Ilya
// Mezerowsky, GPL-3.0, https://github.com/IlyaMez/SpidyVR). Made by Agent Mulder.
#include "spidy/sm1_addon.hpp"
#include "spidy/wrist_hud.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>

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
    int stubs = 0;
    for (const Feature& f : features) {
        check(std::strlen(f.note) > 5, "every feature has a note");
        if (f.evidence == Evidence::candidate)
            check(f.state == State::stub, "every CANDIDATE feature is a stub, never called done");
        stubs += f.state == State::stub;
        if (f.state == State::untested)
            check(f.evidence == Evidence::confirmed, "a built feature only uses CONFIRMED exports");
    }
    check(stubs >= 1, "hero models are still a stub");
    for (State st : {State::ready, State::untested, State::stub})
        check(std::strlen(stateText(st)) > 3, "state words");
    check(std::strcmp(stateText(State::untested), "ready") != 0, "untested is never shown as ready");
}

void configTests() {
    Config d = parseConfig("");
    check(d.heroes && d.wristMenu && d.flight && d.hero == 0, "defaults");
    Config c = parseConfig("# c\r\nheroes=0\r\nflight = off\nwrist_menu=yes\nhero=3\nnoise\nhero=99\n[x]\noutfit.1=2\noutfit.4=99\noutfit.20=1\n");
    check(!c.heroes && !c.flight && c.wristMenu && c.hero == 3, "parsed, bad hero ignored");
    check(c.outfit[1] == 2 && c.outfit[4] == 0, "outfits parsed, out of range ignored");
    Config bad = parseConfig("hero=-1\nhero=1e3\nheroes=maybe\n");
    check(bad.hero == 0 && bad.heroes, "bad values keep defaults");
    Config round = parseConfig(configText(c));
    check(round.heroes == c.heroes && round.flight == c.flight && round.wristMenu == c.wristMenu && round.hero == c.hero &&
              round.outfit[1] == 2,
          "round trip");
    Live l = parseLive("powers=0\nbeams=off\npower_damage=500\nmenu_size=10\n");
    check(!l.powers && l.heroWebs && !l.beams && l.damage == 200 && l.menuSize == 60, "live parsed and clamped");
    const std::string text = liveText(l);
    Live back = parseLive(text);
    check(back.powers == l.powers && back.beams == l.beams && back.damage == l.damage, "live round trip");
    for (const LiveSwitch& w : liveSwitches) {
        check(std::strstr(w.subtext, "no restart") != nullptr, "every live switch says it needs no restart");
        check(text.find(w.subtext) != std::string::npos, "live.txt carries each subtext");
    }
}

void outfitTests() {
    check(outfitCount(hero_modes::ironMan) >= 4, "IRON MAN has his outfits");
    const OutfitInfo mk85 = outfitOf(hero_modes::ironMan, 1);
    check(mk85.name == "MARK 85" && mk85.artist.find("Nihar Arora") != std::string::npos, "MARK 85 and its artist");
    check(outfitOf(hero_modes::ironMan, 99).name.empty(), "past the end: empty");
    for (uint32_t h = 0; h < heroCount; ++h)
        check(outfitCount(h) >= 1, "every hero has an outfit");
}

void powerTests() {
    check(powerOf(0).source == Source::none, "SPIDER-MAN: his mod's webs, no power of ours");
    for (uint32_t h = 1; h < heroCount; ++h) {
        const Power& p = powerOf(h);
        check(p.source != Source::none && p.damage >= 7 && p.damage <= 28, "every hero's power hits like his punches");
        check(p.knockback >= 0 && p.knockback <= 9 && p.range > 0 && p.cooldown > 0, "power numbers");
    }
    check(powerOf(hero_modes::superman).source == Source::eyes, "heat vision from the eyes");
    check(aimedAt({0, 0, 0}, {0, 0, 1}, {0, 0, 10}, 15, 30), "straight ahead hits");
    check(aimedAt({0, 0, 0}, {0, 0, 1}, {2, 0, 10}, 15, 30), "within 15 degrees");
    check(!aimedAt({0, 0, 0}, {0, 0, 1}, {5, 0, 10}, 15, 30), "outside the cone");
    check(!aimedAt({0, 0, 0}, {0, 0, 1}, {0, 0, 40}, 15, 30), "out of range");
    check(!aimedAt({0, 0, 0}, {0, 0, 1}, {0, 0, std::numeric_limits<float>::quiet_NaN()}, 15, 30), "NaN never hits");
    check(flies(hero_modes::ironMan) && flies(hero_modes::superman) && flies(hero_modes::thor) &&
              flies(hero_modes::greenLantern) && !flies(0) && !flies(hero_modes::hulk),
          "the flying heroes");
}

void inputTests() {
    Binding b = classify("/user/hand/right/input/trigger/value");
    check(b.role == Role::trigger && b.side == 1, "right trigger");
    b = classify("/user/hand/left/input/squeeze/value");
    check(b.role == Role::grip && b.side == 0, "left grip");
    check(classify("/user/hand/left/input/thumbstick").role == Role::stick, "stick");
    check(classify("/user/hand/left/input/grip/pose").role == Role::gripPose, "grip pose");
    check(classify("/user/hand/right/input/aim/pose").role == Role::aimPose, "aim pose");
    check(classify("/user/hand/right/input/a/click").role == Role::none, "buttons are his");
    check(classify("/user/head/input/trigger/value").role == Role::none, "only hands");
    MaskInput mi;
    Mask m = maskFor(mi);
    check(!m.trigger[0] && !m.trigger[1] && !m.grip[0] && !m.stick[1], "Spider-Man, menu shut: nothing hidden");
    mi.menuOpen = true;
    m = maskFor(mi);
    check(m.trigger[1] && m.stick[1] && !m.trigger[0] && !m.grip[1], "menu: right trigger and stick only");
    mi = {};
    mi.spiderMan = false;
    mi.handOut[0] = true;
    m = maskFor(mi);
    check(m.trigger[0] && !m.trigger[1], "a hero's outstretched hand: its trigger is the power");
    mi.grip[0] = true;
    m = maskFor(mi);
    check(!m.trigger[0], "swinging (grip held, hero webs on): the trigger reels as his");
    mi.flightArmed = true;
    m = maskFor(mi);
    check(m.grip[0] && m.grip[1] && m.trigger[0], "a flying hero: grips fly, never web");
    mi.flying = true;
    m = maskFor(mi);
    check(m.stick[0] && m.stickY[1] && !m.stick[1], "flying: left stick and right up/down are ours, his turn stays");
}

void wristTests() {
    WristMenu m;
    WristInput in;
    in.dt = .1f;
    in.trigger = true; // the click's own pull: must not pick
    for (int i = 0; i < 3; ++i)
        m.step(in);
    check(!m.open, "no wrist click: stays closed (the palm-up gesture is gone)");
    in.toggled = true;
    check(m.step(in) == Pick::none && m.open, "the SM2 wrist click opens it");
    in.toggled = false;
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
    in.stickY = .9f;
    m.step(in); // 1 -> 0
    in.stickY = 0;
    m.step(in);
    in.stickY = .9f;
    m.step(in); // 0 wraps to the page row
    in.stickY = 0;
    m.step(in);
    check(m.row == m.rows(), "wraps to the page row");
    in.trigger = true;
    m.step(in);
    check(m.page == Page::outfits && m.row == 0, "next page: outfits");
    in.trigger = false;
    m.step(in);
    in.trigger = true;
    check(m.step(in) == Pick::outfit, "outfit pick");
    in.trigger = false;
    m.step(in);
    m.row = m.rows();
    in.trigger = true;
    m.step(in);
    check(m.page == Page::powers, "powers page");
    in.trigger = false;
    m.step(in);
    m.row = static_cast<int>(Toggle::flight);
    in.trigger = true;
    check(m.step(in) == Pick::toggle && m.row == 1, "flight toggle");
    in.trigger = false;
    m.step(in);
    m.row = m.rows();
    in.trigger = true;
    m.step(in);
    check(m.page == Page::addon && m.rows() == 0, "add-on page: status only");
    in.trigger = false;
    m.step(in);
    m.step(in);
    check(m.open, "stays open without a click");
    in.toggled = true;
    check(m.step(in) == Pick::closeMenu, "the same wrist click closes it");
    check(!m.open, "closed");
    in.toggled = false;
    in.stickY = std::numeric_limits<float>::quiet_NaN();
    in.dt = std::numeric_limits<float>::infinity();
    in.trigger = true;
    check(m.step(in) == Pick::none && !m.open, "closed: NaN, the stick and the trigger never open or pick");
    in.stickY = 0;
    in.trigger = false;

    // The SM2 detector itself (wrist_hud::Toggle, FOREARM mode 4, as the layer runs it).
    spidy::wrist_hud::Toggle t;
    t.setMode(4);
    Pose grips[2];
    grips[0].position = {-.2f, 1.1f, -.3f};
    grips[1].position = {.3f, 1.1f, -.3f};
    const bool tracked[2]{true, true};
    float triggers[2]{0, 0};
    const float squeezes[2]{0, 0};
    for (int i = 0; i < 10; ++i)
        t.update(.011f, true, 0, grips, tracked, triggers, squeezes);
    triggers[1] = 1;
    t.update(.011f, true, 0, grips, tracked, triggers, squeezes);
    check(!t.on(), "SM2 click: a right trigger away from the wrist never opens");
    triggers[1] = 0;
    for (int i = 0; i < 40; ++i)
        t.update(.011f, true, 0, grips, tracked, triggers, squeezes);
    grips[1].position = spidy::wrist_hud::wrist(grips[0], 0) + Vec3{.03f, 0, 0};
    for (int i = 0; i < 40; ++i)
        t.update(.011f, true, 0, grips, tracked, triggers, squeezes);
    triggers[1] = 1;
    for (int i = 0; i < 3; ++i)
        t.update(.011f, true, 0, grips, tracked, triggers, squeezes);
    check(t.on(), "SM2 click: right hand on the left wrist plus the right trigger opens");
    check((t.hands() >> 1 & 1u) != 0, "SM2 click: that pull is the menu's, not his web's");
    MaskInput hm;
    hm.holoHands = t.hands();
    check(maskFor(hm).trigger[1], "the mask hides the click's trigger from him");
    for (uint32_t p = 0; p < static_cast<uint32_t>(Page::count); ++p)
        check(std::strlen(pageName(static_cast<Page>(p))) > 0, "page names");
}

void drawTests() {
    Canvas c;
    MenuView v;
    v.hero = hero_modes::ironMan;
    v.outfit = 1;
    v.powerState = "READY";
    paintMenu(c, v);
    check(c.width == menuWidth && c.height == menuHeight, "menu size");
    int lit = 0, red = 0, clear = 0;
    for (uint32_t p : c.pixels) {
        const uint32_t r = p & 255, g = p >> 8 & 255, b = p >> 16 & 255;
        lit += r > 200 && g > 200 && b > 200;
        red += r > 200 && g < 110 && (p >> 24) > 200;
        clear += (p >> 24) == 0;
    }
    check(lit > 2000, "the menu has bright text");
    check(red > 1500, "the SM2 holo's red frame");
    check(clear > 500 && (c.pixels[0] >> 24) == 0, "the cut corners are see-through, as SM2's");
    MenuView w = v;
    check(sameView(v, w), "same view");
    w.row = 3;
    check(!sameView(v, w), "a moved row repaints");
    for (uint32_t p = 0; p < static_cast<uint32_t>(Page::count); ++p) {
        v.page = static_cast<Page>(p);
        paintMenu(c, v); // every page paints
    }
    Canvas beam(16, 64);
    paintBeam(beam, 0xff0000);
    check((beam.pixels[8] >> 24) > 200 && (beam.pixels[0] >> 24) < 80, "the beam: bright core, soft edge");
    check(Canvas::textWidth("AB", 2) > 0, "text width");
}

void quadTests() {
    const Quat q = fromBasis({0, 0, -1}, {0, 1, 0}, {1, 0, 0});
    const Vec3 x = q.rotate({1, 0, 0});
    check(std::fabs(x.z + 1) < 1e-4f, "basis to turn");
    Pose grip;
    grip.position = {0, 1, -.3f};
    const Pose menu = menuPose(grip, {0, 1.6f, 0}, .15f);
    const Vec3 z = menu.orientation.rotate({0, 0, 1});
    check(menu.position.y > 1.1f && z.z > .99f, "menu over the wrist, facing the head");
    const Pose beam = beamPose({0, 1, 0}, {0, 0, -1}, 10, {0, 1.6f, .2f});
    const Vec3 y = beam.orientation.rotate({0, 1, 0});
    check(std::fabs(beam.position.z + 5) < 1e-3f && y.z < -.99f, "beam along the aim, centred");
}

void flightTests() {
    FlightState s;
    FlightInput in;
    in.dt = 1.f / 90;
    in.allowed = true;
    in.grip[1] = true;
    in.aim[1] = {0, 0, 1};
    FlightEvent e = FlightEvent::none;
    int frames = 0;
    while (e == FlightEvent::none && frames < 200) {
        e = stepFlight(s, in);
        ++frames;
    }
    check(e == FlightEvent::tookOff && s.on && frames >= 35 && frames <= 38, "a held grip takes off after 0.4 s");
    check(s.velocity.y > 3, "lifts off");
    for (int i = 0; i < 90; ++i)
        stepFlight(s, in);
    check(std::fabs(s.velocity.z) < .5f, "the take-off grip never boosts until let go");
    in.grip[1] = false;
    for (int i = 0; i < 90 * 3; ++i)
        stepFlight(s, in);
    check(s.on && length(s.velocity) < .5f, "stays flying after release, hovering");
    in.moveY = 1;
    for (int i = 0; i < 90 * 3; ++i)
        stepFlight(s, in);
    check(s.velocity.z > 11, "left stick forward flies along the head's facing");
    in.headForward = {1, 0, 0};
    in.headRight = {0, 0, -1};
    for (int i = 0; i < 90 * 3; ++i)
        stepFlight(s, in);
    check(s.velocity.x > 11, "turning the head turns the flight (body relative)");
    in.moveY = 0;
    in.grip[1] = true;
    for (int i = 0; i < 90 * 3; ++i)
        stepFlight(s, in);
    check(s.velocity.z > 10 && length(s.velocity) <= 28.01f, "a held grip boosts along that hand, capped");
    in.grip[1] = false;
    in.grounded = true;
    in.climb = 0;
    for (int i = 0; i < 90 * 3 && s.on; ++i)
        e = stepFlight(s, in);
    check(!s.on && e == FlightEvent::landed, "touching the ground lands");
    in.grounded = false;
    in.grip[0] = true;
    for (int i = 0; i < 60; ++i)
        stepFlight(s, in);
    check(s.on, "takes off again");
    in.allowed = false;
    check(stepFlight(s, in) == FlightEvent::stopped && !s.on, "a hero who does not fly stops");
    in.allowed = true;
    in.grip[0] = true;
    s = {};
    s.on = true;
    s.velocity = {std::numeric_limits<float>::quiet_NaN(), 0, 0};
    in.moveX = std::numeric_limits<float>::infinity();
    stepFlight(s, in);
    check(finite(s.velocity), "NaN never spreads");
}

void handoverTests() {
    Handover h;
    check(h.his(1) == 1 && h.his(2) == 2, "before any flight his serials pass as they are");
    check(h.sampleSerial(2) == 2, "his telemetry as it is");
    h.take();
    const uint64_t a = h.ours(), b = h.ours();
    check(a >= Handover::base && b == a + 1, "ours rise from far above his");
    check(h.his(3) == 0, "his wait while a hero flies");
    check(h.sampleSerial(b) == 0, "a step we drove is not his");
    h.release();
    const uint64_t c = h.his(4);
    check(c > b, "his next one goes out above ours (never refused)");
    check(h.his(5) == c + 1, "and keeps rising one by one");
    check(h.sampleSerial(c + 1) == 5, "his telemetry gets his own serial back");
    h.take();
    const uint64_t d = h.ours();
    check(d > c + 1, "ours again above his");
    h.refused();
    check(h.ours() > d + 1000, "a refusal jumps further up");
    h.release();
    check(h.his(6) > d, "his again above");
}
} // namespace

int main() {
    hostTests();
    featureTests();
    configTests();
    outfitTests();
    powerTests();
    inputTests();
    wristTests();
    drawTests();
    quadTests();
    flightTests();
    handoverTests();
    if (failures) {
        std::printf("%d failure(s)\n", failures);
        return 1;
    }
    std::printf("sm1 add-on tests passed\n");
    return 0;
}
