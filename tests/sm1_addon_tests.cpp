// HERO VR 0.0.76 SM1: tests for Mulder's Heroes add-on core (iilyn's Spider-Man VR mod is Spidy by Ilya
// Mezerowsky, GPL-3.0, https://github.com/IlyaMez/SpidyVR). Made by Agent Mulder.
#include "spidy/sm1_addon.hpp"
#include "spidy/sm1_combat.hpp"
#include "spidy/sm1_grapple.hpp"
#include "spidy/sm1_holo.hpp"
#include "spidy/sm1_props.hpp"
#include "spidy/thor_grip.hpp"
#include "spidy/eye_color.hpp"
#include "spidy/sm1_scan.hpp"
#include "spidy/wrist_hud.hpp"
#include "spidy/native_view.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
#include <chrono>
#include <thread>
#include <vector>

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
    check(Live{}.scan && !parseLive("sm1scan=0\n").scan && parseLive("powers=1\n").scan, "sm1scan: on unless set to 0");
    check(parseLive(liveText(Live{})).scan, "sm1scan round trip");
    const std::string old = "powers=0\nhero_webs=1\nbeams=1\npower_damage=80\nmenu_size=100\n";
    const std::string add = missingLive(old);
    check(add.find("sm1scan=1") != std::string::npos && add.find(liveSwitches[5].subtext) != std::string::npos &&
              add.find("powers") == std::string::npos,
          "an older live.txt gets only the missing switch, with its subtext");
    const Live merged = parseLive(old + add);
    check(!merged.powers && merged.damage == 80 && merged.scan, "the player's values stay");
    check(missingLive(liveText(Live{})).empty(), "nothing missing from a current live.txt");
    check(Live{}.props && !parseLive("props=0\n").props && !parseLive(liveText(parseLive("props=0\n"))).props, "props: on unless set to 0");
    check(missingLive("powers=1\ncolor_vignette=0\n").find("props=1") != std::string::npos, "an older live.txt gets props=1");
}

void scanTests() {
    using namespace spidy::sm1::scan;
    // His nameHash (native_body.cpp) and the values it gives.
    check(jointHash("") == 0xedb88320u, "joint hash: the seed, no final inversion");
    check(jointHash("LF_wrist") == 0x5cdc16fbu && jointHash("RT_wrist") == 0xc5acb805u, "joint hash: the wrists");
    bool wrists = false;
    for (const auto& n : jointNames())
        wrists |= n == "RT_wrist";
    check(wrists && jointNames().size() >= 20, "the hand joints are listed");
    check(className(".?AVHeroHealth@@") == "HeroHealth", "rtti: a plain class");
    check(className(".?AVBar@Foo@@") == "Foo::Bar", "rtti: a namespace");
    check(className(".?AUThing@@") == "Thing" && className("HeroHealth").empty(), "rtti: a struct; not a name");
    Area a{};
    check(interesting("VehicleSeatComponent", a) && a == Area::spawning, "a seat class is a spawning lead");
    check(interesting("HeroRopeManager", a) && a == Area::grapple, "the rope manager is a grapple lead");
    check(!interesting("TextureStreamer", a), "unrelated classes are left out");
    check(botClass("ThugBot") && botClass("BotMoverManagerGame") && !botClass("HeroHealth"), "bot classes");
    check(suitClass("SkinManager") && suitClass("Game::HeroCharacterManager") && suitClass("SunsetVanityEquippedInventory") &&
              suitClass("ItemLoadoutComponent") && !suitClass("SkinManagerHelper") && !suitClass("XSkinManager"),
          "suit classes (dump mode)");
    check(vehicleClass("VehicleComponent") && vehicleClass("TrafficManager") && !vehicleClass("CardDeck") && !vehicleClass("Carried"),
          "vehicle classes (dump mode)");
    // Eye targets (dump mode): records 105 and 106 sit at the set's +0xdcc8 and +0xdeb0 (his VALIDATION.md).
    check(setRecordOffset(105) - eyeSetAt == 0xdcc8 && setRecordOffset(106) - eyeSetAt == 0xdeb0, "eye target record offsets");
    check(depthFormat(40) && depthFormat(39) && depthFormat(45) && depthFormat(20) && !depthFormat(28) && !depthFormat(10),
          "depth formats");
    check(!std::strcmp(eyeTargetVerdict(40, 2016, 2240, 2016, 2240), "depthAtEyeSize") &&
              !std::strcmp(eyeTargetVerdict(40, 1008, 1120, 2016, 2240), "depthOtherSize") &&
              !std::strcmp(eyeTargetVerdict(28, 2016, 2240, 2016, 2240), "atEyeSize") &&
              !std::strcmp(eyeTargetVerdict(40, 2016, 2240, 0, 0), "depthOtherSize"),
          "eye target verdicts");
    {
        std::vector<EyeDepthTally> t(3);
        t[0] = {12, "kMrtDepth", 4, false};
        t[1] = {40, "kHiZ", 1, false};
        t[2] = {50, "", 0, false};
        check(eyeDepthStatus(t, 2) == "CONFIRMED record 12 (kMrtDepth)", "eye depth: one record every pass is CONFIRMED");
        check(eyeDepthStatus(t, 1).rfind("CANDIDATE", 0) == 0, "eye depth: one pass stays CANDIDATE");
        t[0].shared = true;
        check(eyeDepthStatus(t, 2) == "CANDIDATE records 12 (kMrtDepth), 40 (kHiZ)", "eye depth: a shared resource stays CANDIDATE");
        t[0].shared = false;
        t[1].atEyeSize = 4;
        check(eyeDepthStatus(t, 2).rfind("CANDIDATE", 0) == 0, "eye depth: two records every pass stay CANDIDATE");
        check(eyeDepthStatus({}, 3) == "not found", "eye depth: none");
    }
    const float id[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 5, 6, 7, 1};
    float bad[16];
    std::memcpy(bad, id, sizeof(bad));
    bad[0] = 3;
    check(actorTransform(id) && !actorTransform(bad), "his transform check");
    const uint8_t buf[] = {1, 2, 0x48, 0x8b, 0xc4, 9, 0x48, 0x8b, 0xc4};
    const uint8_t pat[] = {0x48, 0x8b, 0xc4};
    std::vector<uint64_t> hits;
    findAll(buf, sizeof(buf), 0x1000, pat, sizeof(pat), hits, 8);
    check(hits.size() == 2 && hits[0] == 0x1002 && hits[1] == 0x1006, "signature hits");
    hits.clear();
    findAll(buf, sizeof(buf), 0, pat, sizeof(pat), hits, 1);
    check(hits.size() == 1, "signature hits are capped");
    check(hex(pat, 3) == "488bc4" && quoted("a\"b\n") == "\"a\\\"b\\u000a\"", "hex and json text");
    check(number(std::numeric_limits<double>::infinity()) == "null", "a json number");
    check(handleOf(3, 5) == (3u << 20 | 5u), "his handle layout");
    check(sessionName(2026, 11, 1, 1, 2, 3) == "sm1scan-20261101-010203", "session name");
    const auto drop = oldSessions({"sm1scan-20260101-000000.jsonl", "sm1scan-20260101-000000.bin",
                                   "sm1scan-20260102-000000.jsonl", "sm1scan-20260103-000000.jsonl", "other.txt"},
                                  2);
    check(drop.size() == 2 && drop[0] == "sm1scan-20260101-000000.bin", "the oldest session goes");
    // Every anchor: a name, a source in his sources, an RVA inside a 4.630 image, and the area it serves.
    bool areas[static_cast<int>(Area::count)] = {};
    bool sane = true;
    for (const auto& an : anchors()) {
        sane &= an.name && *an.name && an.source && *an.source && an.rva > 0x1000 && an.rva < 0x10000000;
        sane &= !an.expect || (an.expectBytes >= 8 && an.kind == Kind::code);
        areas[static_cast<int>(an.area)] = true;
    }
    bool every = true;
    for (const bool b : areas)
        every &= b;
    check(sane && every, "anchors: named, sourced, every area covered");
    const Limits lim;
    check(lim.runMs <= 20 * 60 * 1000 && lim.maxFileBytes <= (64ull << 20) && lim.keepSessions >= 2, "the scan's caps");
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
    b = classify("/user/hand/right/input/a/click");
    check(b.role == Role::faceLower && b.side == 1, "0.0.76: the right A (read, hidden only in flight)");
    check(classify("/user/hand/right/input/b/click").role == Role::faceUpper, "0.0.76: the right B");
    check(classify("/user/hand/right/input/a/touch").role == Role::none, "a touch is no press");
    check(classify("/user/hand/left/input/x/click").role == Role::faceLower, "the left X (Touch)");
    check(classify("/user/hand/right/input/x/click").role == Role::none &&
              classify("/user/hand/right/input/y/click").role == Role::none,
          "Steam Frame's X and Y on the right are not A and B");
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
    {
        // 0.0.76 test: the arm came in with the power's trigger still held and his game fired its own shot.
        MaskInput held;
        held.spiderMan = false;
        held.handOut[1] = true;
        Mask first = maskFor(held);
        bool ours = keepTrigger(1.f, first.trigger[1]);
        check(ours, "a power's press is kept while it is held");
        held.handOut[1] = false;
        held.triggerOurs[1] = ours;
        check(maskFor(held).trigger[1], "the arm comes in, the press is still hidden from his reads");
        check(!keepTrigger(0.f, true), "let go: the next press is judged afresh");
        held.triggerOurs[1] = false;
        check(!maskFor(held).trigger[1], "a press begun with the arm in is his");
        check(!keepTrigger(std::numeric_limits<float>::quiet_NaN(), true), "no trigger value keeps nothing");
    }
    mi.flightArmed = true;
    m = maskFor(mi);
    check(!m.grip[0] && !m.grip[1] && !m.trigger[0] && !m.faceLower[1],
          "0.0.76: a flying hero on the ground: grips web, A jumps (its hold takes off)");
    mi.flying = true;
    m = maskFor(mi);
    check(m.stick[0] && !m.stickY[1] && !m.stick[1] && m.faceLower[1] && m.faceUpper[1] && !m.faceLower[0],
          "0.0.76 flying: the left stick, the right A and B are ours; the right stick stays his");
    mi.flying = false;
    mi.aHeld = true;
    check(maskFor(mi).faceLower[1], "0.0.76: an A held from the flight is no jump on landing");
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
    {
        // 0.0.76 test: the beams went out behind the hand. SpidyVR 0.7.0 writes each hand into SpidyXrData through
        // native_view::relativePose (the aim pose in the renderer's rows: right, down, forward).
        const Mat4 basis = {1, 0, 0, 0, 0, -1, 0, 0, 0, 0, -1, 0, 0, 0, 0, 1};
        Pose aimPose;
        aimPose.orientation = fromBasis({0, 0, 1}, {0, 1, 0}, {-1, 0, 0}); // pointing along +X
        aimPose.position = {1, 2, 3};
        const Mat4 row = native_view::relativePose(basis, aimPose);
        const Vec3 points = aimPose.orientation.rotate({0, 0, -1});
        const Vec3 f = xrDataForward(row.data());
        check(length(f - points) < 1e-4f && f.x > .99f, "a hand's forward in SpidyXrData is where its aim points");
        // the game head looking along +X (right +Z... its rows right, down, forward): straight ahead is -Z head-local
        const Vec3 local = gameToHeadLocal({1, 0, 0}, {0, 0, 1}, {0, -1, 0}, {1, 0, 0});
        check(length(local - Vec3{0, 0, -1}) < 1e-5f, "the game head's forward is OpenXR's -Z");
        const Vec3 upLocal = gameToHeadLocal({0, 1, 0}, {0, 0, 1}, {0, -1, 0}, {1, 0, 0});
        check(length(upLocal - Vec3{0, 1, 0}) < 1e-5f, "the game's up is the headset's up");
    }
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
    // HERO VR 0.0.76 FLIGHT: fly076's feel on his SpidyMotionSubmit.
    const float dt = 1.f / 90;
    FlightState s;
    FlightInput in;
    in.dt = dt;
    in.allowed = true;
    in.grounded = true;
    in.headForward = {0, 0, 1};
    in.headRight = {1, 0, 0};
    in.a = true;
    FlightEvent e = FlightEvent::none;
    int frames = 0;
    while (e == FlightEvent::none && frames < 200) {
        e = stepFlight(s, in);
        ++frames;
    }
    check(e == FlightEvent::tookOff && s.on && frames >= 35 && frames <= 38, "A held 0.4 s takes off");
    check(s.velocity.y > 0, "lifts off");
    in.grounded = false;
    for (int i = 0; i < 90; ++i)
        stepFlight(s, in);
    check(s.on, "the same A held never also comes down");
    in.a = false;
    for (int i = 0; i < 90 * 3; ++i)
        stepFlight(s, in);
    check(s.on && s.velocity.z > 12 && std::fabs(s.velocity.y) < 2, "flies on by itself along the head's way");
    in.steer = {1, 0, 0};
    for (int i = 0; i < 90 * 4; ++i)
        stepFlight(s, in);
    check(s.velocity.x > 12 && std::fabs(s.velocity.z) < 4, "the arms steer");
    in.throttle = 1;
    for (int i = 0; i < 90 * 3; ++i)
        stepFlight(s, in);
    check(s.velocity.x > 26 && length(s.velocity) <= 28.01f && std::fabs(s.velocity.y) < .5f,
          "full forward: SUPERSONIC, speed only, never down");
    in.throttle = -1;
    for (int i = 0; i < 90 * 4; ++i)
        stepFlight(s, in);
    check(s.on && length(s.velocity) < 1, "stick back: a hover");
    in.throttle = 0;
    for (int i = 0; i < 90; ++i)
        stepFlight(s, in);
    in.b = true;
    for (int i = 0; i < 90 * 2; ++i)
        stepFlight(s, in);
    check(s.on && length(s.velocity) < .5f, "B: the air brake");
    in.b = false;
    in.a = true;
    for (int i = 0; i < 60; ++i)
        stepFlight(s, in);
    check(s.on && s.velocity.y < -3, "A held in flight comes down");
    in.grounded = true;
    for (int i = 0; i < 90 * 2 && s.on; ++i)
        e = stepFlight(s, in);
    check(!s.on && e == FlightEvent::landed, "and lands on the ground");
    for (int i = 0; i < 90; ++i)
        stepFlight(s, in);
    check(!s.on, "the A still down never takes off again");
    in.a = false;
    in.armsUp = true;
    for (int i = 0; i < 40 && !s.on; ++i)
        stepFlight(s, in);
    check(s.on, "both arms up take off");
    in.armsUp = false;
    in.allowed = false;
    check(stepFlight(s, in) == FlightEvent::stopped && !s.on, "a hero who does not fly stops");
    in.allowed = true;
    s = {};
    in.throttle = std::numeric_limits<float>::quiet_NaN();
    in.steer = {std::numeric_limits<float>::infinity(), 0, 0};
    in.a = true;
    for (int i = 0; i < 60; ++i)
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

void suitTests() {
    check(suitIdOf(0) == nullptr, "SPIDER-MAN keeps the game's own suits");
    check(std::string(suitIdOf(9)) == "herovr_thor", "THOR's suit id");
    check(suitIdOf(heroCount) == nullptr, "out of range");
    const std::string done = "{\"tool_version\": 1, \"pending\": false, \"archive\": \"HeroVR\\\\herovr_suits\", "
                             "\"suits\": [\"herovr_ironman\", \"herovr_thor\"], \"assets\": [[\"herovr_hulk\"]]}";
    check(suitInstalled(done, 9), "THOR listed");
    check(suitInstalled(done, 1), "IRON MAN listed");
    check(!suitInstalled(done, 3), "HULK only outside the suits list");
    check(!suitInstalled(done, 0), "SPIDER-MAN never");
    std::string pending = done;
    pending.replace(pending.find("false"), 5, "true");
    check(!suitInstalled(pending, 9), "a half-done install counts as not installed");
    check(!suitInstalled("", 9), "no file: not installed");
    check(suitHint(9, true) == "SUIT: PICK THOR IN THE GAME'S SUITS MENU", "installed hint");
    check(suitHint(9, false) == "SUIT: INSTALL HERO SUITS IN THE LAUNCHER", "not installed hint");
    check(suitHint(0, false).find("SPIDER-MAN") != std::string::npos, "SPIDER-MAN hint");
    check(folderOf("C:\\g\\Spider-Man.exe") == "C:\\g\\", "game folder");
    check(folderOf("x.exe").empty(), "no folder");
    bool stub = false;
    for (const auto& f : features)
        if (std::string(f.name) == "holo suit switch")
            stub = f.state == State::stub;
    check(stub, "the live suit switch is reported as not done");
    MenuView a, b;
    b.notice = suitHint(9, true);
    check(!sameView(a, b), "the hint repaints the holo");
    Canvas c(menuWidth, menuHeight);
    paintMenu(c, b);
}
void combatTests() {
    using namespace spidy::sm1::combat;
    check(ruleBits(".?AVBotMoverManager@@") == bot, "BotMoverManager marks a bot");
    check(ruleBits(".?AVCivilianBot@@") == civilian, "CivilianBot marks a civilian");
    check(ruleBits(".?AVBotHealth@@") == botHealth && ruleBits(".?AVHealth@@") == health, "the health classes");
    check(ruleBits(".?AVBotStateRagdoll@@") == down && ruleBits(".?AVBotStateCarried@@") == held, "down and held states");
    check(ruleBits(".?AVSomethingElse@@") == 0 && ruleBits("") == 0, "an unknown class has no bits");
    check(enemy(bot | botHealth | thug), "a thug with health is an enemy");
    check(enemy(bot | health | hover), "a drone with health is an enemy (every type)");
    check(!enemy(bot | botHealth | civilian), "a civilian is never an enemy");
    check(!enemy(bot | botHealth | ally) && !enemy(bot | botHealth | neutral), "allies and neutrals are not enemies");
    check(!enemy(bot), "a bot without health is not hit");
    check(!enemy(botHealth), "health alone is not a bot");
    Size sz;
    check(std::fabs(sz.height(.4f) - sz.low) < 1e-5f && std::fabs(sz.height(1.6f) - sz.high) < 1e-5f,
          "a thug's heights come back unchanged");
    std::vector<Target> list(3);
    list[0].record = 1;
    list[0].feet = {0, 0, -10};
    list[1].record = 2;
    list[1].feet = {3, 0, -5};
    list[2].record = 3;
    list[2].feet = {0, 0, 10}; // behind
    const Vec3 from{0, 1.2f, 0}, ahead{0, 0, -1};
    check(aimed(list, from, ahead, 30, 15) == 0, "the enemy straight ahead is aimed at");
    check(aimed(list, from, normalized(Vec3{3, 0, -5}), 30, 15) == 1, "the one to the right when aiming there");
    check(aimed(list, from, ahead, 5, 15) == -1, "out of range: none");
    check(aimed(list, from, Vec3{0, 0, 1}, 30, 15) == 2, "behind only when turned around");
    list[0].hp = 0;
    check(aimed(list, from, ahead, 30, 15) != 0, "a knocked out enemy is not aimed at");
    list[0].hp = 50;
    check(bodyDistance(list[0], {0, 1, -10}) == 0, "inside the body");
    check(std::fabs(bodyDistance(list[0], {2, 1, -10}) - (2 - sz.radius)) < 1e-4f, "beside the body");
    const auto n = nearby(list, {0, 1, -9}, 1.5f, 5);
    check(n.size() == 1 && n[0] == 0, "nearby finds the close one only");
    const auto a = along(list, {0, 1, 0}, {0, 1, -20}, .2f, 5);
    check(a.size() == 1 && a[0] == 0, "a throw down the street passes through the first");
    const auto a2 = along(list, {0, 1, 0}, {6, 1, -10}, .5f, 5);
    check(a2.size() == 1 && a2[0] == 1, "an angled throw passes the second");
    HitLimiter lim;
    check(lim.allow(7, 0, .5) && !lim.allow(7, .2, .5) && lim.allow(7, .6, .5) && lim.allow(8, .2, .5),
          "one enemy is hit at most once per period");
}
void grappleTests() {
    Binding j = classify("/user/hand/right/input/a/click");
    check(j.role == Role::faceLower && j.side == 1, "right A is his jump (the grapple's jump out reads it)");
    const Binding ix = classify("/user/hand/left/input/a/click");
    check(ix.role == Role::faceLower && ix.side == 0, "a left a/click (Index X) is the left hand's, never the jump");
    check(parseLive("grapple=0").grapple == false && parseLive("").grapple, "the grapple switch, on by default");
    check(missingLive("powers=1\n").find("grapple=1") != std::string::npos, "an older live.txt gets grapple=1");
    MaskInput mi;
    mi.spiderMan = false;
    mi.grappleArmed = true;
    mi.grip[1] = true;
    Mask m = maskFor(mi);
    check(m.grip[0] && m.grip[1] && m.trigger[1] && !m.trigger[0], "armed: grips hidden, a held grip's trigger hidden");
    GrappleDriver g;
    GrappleInput in;
    in.allowed = true;
    in.body = {0, 0, 0};
    in.dt = .016f;
    in.grip[1] = 1;
    in.aimOk[1] = true;
    in.aimKind[1] = aimAnchor;
    in.aimPoint[1] = {0, 10, -20};
    in.aimNormal[1] = {0, 0, 1};
    GrappleStep s = g.step(in); // grip only
    check(!s.started && !s.drive, "a grip alone fires nothing");
    in.trigger[1] = 1;
    s = g.step(in);
    check(s.started && s.drive && s.line == 1 && s.velocity.z < 0 && s.velocity.y > 0, "grip + trigger pulls to the wall");
    for (int i = 0; i < 10; ++i)
        s = g.step(in);
    check(s.drive && length(s.velocity) > 5, "the pull keeps going");
    in.jump = true;
    s = g.step(in);
    check(s.jumped && s.drive && s.velocity.y >= 9, "A jumps out upward");
    in.jump = false;
    for (int i = 0; i < 60 && g.driving(); ++i)
        s = g.step(in);
    check(!g.driving(), "the coast ends");
    GrappleDriver h;
    GrappleInput miss = in;
    miss.aimKind[1] = 2; // air
    miss.trigger[1] = 0;
    h.step(miss);
    miss.trigger[1] = 1;
    s = h.step(miss);
    check(s.missed && !s.drive, "open air does not hook");
    GrappleDriver k;
    GrappleInput c = in;
    c.trigger[1] = 0;
    k.step(c);
    c.trigger[1] = 1;
    k.step(c);
    for (int i = 0; i < 5; ++i)
        k.step(c);
    c.grip[1] = 0;
    s = k.step(c);
    check(s.cancelled && s.drive, "letting go of the grip coasts on");
    c.allowed = false;
    s = k.step(c);
    check(!s.drive && !k.driving(), "not allowed: stops at once");
}
} // namespace

void colorTests() {
    // OFF by default: the grade is neutral, so the layer does no GPU work.
    const Live d{};
    check(!d.colorFx && eye_color::neutral(eye_color::params(colorConfig(d))), "COLOR is off and neutral by default");
    Live on = d;
    on.colorFx = true;
    check(eye_color::neutral(eye_color::params(colorConfig(on))), "COLOR on with the NONE look and no sliders: neutral");
    // live.txt round trip, the look by key or by number.
    Live l = parseLive("color_fx=1\ncolor_filter=gotham\ncolor_exposure=-5\ncolor_warmth=40\ncolor_vignette=30\n");
    check(l.colorFx && l.colorExposure == -5 && l.colorWarmth == 40 && l.colorVignette == 30, "color keys parse");
    check(l.colorFilter != 0 && std::string(eye_color::filterKeys[l.colorFilter]) == "gotham", "a look by its key");
    check(!eye_color::neutral(eye_color::params(colorConfig(l))), "GOTHAM grades");
    const Live back = parseLive(liveText(l));
    check(back.colorFx == l.colorFx && back.colorFilter == l.colorFilter && back.colorExposure == l.colorExposure &&
              back.colorWarmth == l.colorWarmth && back.colorVignette == l.colorVignette &&
              back.colorStrength == l.colorStrength && back.colorContrast == l.colorContrast,
          "live.txt keeps the colour settings");
    check(parseLive("color_filter=3\n").colorFilter == 3, "a look by its number");
    check(parseLive("color_filter=99\ncolor_exposure=500\n").colorFilter == 0 &&
              parseLive("color_exposure=500\n").colorExposure <= 20,
          "out-of-range values never pass");
    const std::string added = missingLive("powers=1\n");
    check(added.find("color_fx=") != std::string::npos && added.find("color_vignette=") != std::string::npos,
          "the colour keys are added to an older live.txt");
    // The menu rows: a toggle, a look that cycles, sliders that step and wrap.
    Live m{};
    adjustColor(m, 0, 1);
    check(m.colorFx, "row 0 turns COLOR on");
    adjustColor(m, 1, -1);
    check(m.colorFilter == static_cast<uint32_t>(eye_color::filters) - 1, "the look wraps backwards");
    adjustColor(m, 1, 1);
    check(m.colorFilter == 0, "and forwards");
    for (int i = 0; i < colorRows; ++i) {
        check(std::strlen(colorRowName(i)) > 2, "every colour row has a name");
        check(!colorRowValue(m, i).empty(), "every colour row has a value");
        Live w = m;
        for (int k = 0; k < 200; ++k)
            adjustColor(w, i, 1);
        const Live r = parseLive(liveText(w));
        check(r.colorStrength <= 100 && r.colorExposure >= -20 && r.colorExposure <= 20 && r.colorContrast >= 50 &&
                  r.colorContrast <= 150 && r.colorSaturation <= 200 && r.colorVibrance <= 100 &&
                  r.colorWarmth >= -100 && r.colorWarmth <= 100 && r.colorTint >= -100 && r.colorTint <= 100 &&
                  r.colorVignette <= 100,
              "stepping never leaves the limits");
    }
    // 0.0.76: the COLOR rows are on the HEROES page under the heroes (no COLOR page): up/down picks the row, a
    // left / right flick steps it, the last row goes on.
    WristMenu w;
    WristInput in;
    in.toggled = true;
    w.step(in);
    in.toggled = false;
    w.step(in);
    check(w.page == Page::heroes && w.rows() == static_cast<int>(heroCount) + colorRows, "the heroes and the COLOR rows");
    for (uint32_t p = 0; p < static_cast<uint32_t>(Page::count); ++p)
        check(std::string(pageName(static_cast<Page>(p))) != "COLOR", "no separate COLOR page");
    w.row = 1;
    in.stickX = 1;
    check(w.step(in) == Pick::none && w.colorRow() == -1, "a flick on a hero row changes nothing");
    in.stickX = 0;
    w.step(in);
    w.row = static_cast<int>(heroCount) + 2;
    check(w.colorRow() == 2, "the COLOR row under the heroes");
    in.stickX = 1;
    check(w.step(in) == Pick::color && w.delta == 1, "a right flick steps up");
    check(w.step(in) == Pick::none, "held: once");
    in.stickX = 0;
    w.step(in);
    in.stickX = -1;
    check(w.step(in) == Pick::color && w.delta == -1, "a left flick steps down");
    in.stickX = 0;
    w.step(in);
    in.trigger = true;
    check(w.step(in) == Pick::color && w.delta == 1 && w.colorRow() == 2, "the trigger steps up");
    in.trigger = false;
    w.step(in);
    w.row = static_cast<int>(heroCount) - 1;
    in.trigger = true;
    check(w.step(in) == Pick::hero, "the last hero row still picks the hero");
    in.trigger = false;
    w.step(in);
    w.row = w.rows();
    in.trigger = true;
    w.step(in);
    check(w.page == Page::outfits && w.colorRow() == -1, "the HEROES page's last row goes on to OUTFITS");
    in.trigger = false;
    w.step(in);
    w.page = Page::addon;
    w.row = w.rows();
    in.trigger = true;
    w.step(in);
    check(w.page == Page::heroes, "ADD-ON's last row goes back to HEROES");
    for (int i = 0; i < colorRows; ++i)
        check(std::strlen(colorRowSubtext(i)) > 20, "every COLOR row keeps its subtext");
    // Drawn: the heroes page with a COLOR row selected paints its subtext (more dim text than with a hero row).
    Canvas c1, c2;
    MenuView v1;
    v1.row = 0;
    for (int i = 0; i < colorRows; ++i)
        v1.color[i] = colorRowValue(m, i);
    MenuView v2 = v1;
    v2.row = static_cast<int>(heroCount) + 1;
    paintMenu(c1, v1);
    paintMenu(c2, v2);
    check(c1.pixels != c2.pixels, "the selected COLOR row repaints");
}

// 0.0.76 SM1 HOLO: the SM2 build's own panel, painted into the quad's picture.
Vertex vtx(float x, float y, Vec3 c, float a) {
    Vertex v;
    v.position = {x, y, 0};
    v.color = c;
    v.alpha = a;
    return v;
}
uint32_t at(const Canvas& c, int x, int y) {
    return c.pixels[static_cast<size_t>(y) * static_cast<size_t>(c.width) + static_cast<size_t>(x)];
}
void holoTests() {
    using namespace spidy::sm1::holo;
    // a white square of two triangles over a 10x10 picture of a 1x1 area: every pixel once, no seam
    std::vector<Vertex> sq = {vtx(-.5f, -.5f, {1, 1, 1}, .5f), vtx(.5f, -.5f, {1, 1, 1}, .5f), vtx(.5f, .5f, {1, 1, 1}, .5f),
                              vtx(-.5f, -.5f, {1, 1, 1}, .5f), vtx(.5f, .5f, {1, 1, 1}, .5f), vtx(-.5f, .5f, {1, 1, 1}, .5f)};
    Canvas c(10, 10);
    raster(sq, {}, nullptr, .5f, .5f, c);
    bool same = true;
    for (uint32_t p : c.pixels)
        same = same && p == c.pixels[0];
    check(same, "two triangles of one square blend each pixel once (no seam)");
    check((c.pixels[0] >> 24) == 128 && (c.pixels[0] & 0xffffff) == 0xffffff, "half white: straight white, alpha 128");
    // over: red at .5 over the white half - straight colour sRGB-encoded, alpha .75
    std::vector<Vertex> two = sq;
    for (Vertex v : sq) {
        v.color = {1, 0, 0};
        two.push_back(v);
    }
    raster(two, {}, nullptr, .5f, .5f, c);
    const uint32_t p = at(c, 5, 5);
    check((p >> 24) == 191, "red over white: alpha .75");
    check((p & 0xff) == 255 && ((p >> 8) & 0xff) > 150 && ((p >> 8) & 0xff) < 200, "red over white: linear blend, sRGB out");
    // a triangle covers only its half
    std::vector<Vertex> tri = {vtx(-.5f, .5f, {0, 1, 0}, 1), vtx(.5f, .5f, {0, 1, 0}, 1), vtx(-.5f, -.5f, {0, 1, 0}, 1)};
    raster(tri, {}, nullptr, .5f, .5f, c);
    check(at(c, 1, 1) == 0xff00ff00u && at(c, 8, 8) == 0, "a triangle fills its side only (y up, top row first)");
    // either winding draws
    std::swap(tri[1], tri[2]);
    raster(tri, {}, nullptr, .5f, .5f, c);
    check(at(c, 1, 1) == 0xff00ff00u, "either winding draws");
    // a textured triangle samples its picture, tinted
    Portraits pics;
    pics.ok = true;
    pics.layers.resize(1);
    pics.layers[0].width = pics.layers[0].height = 2;
    pics.layers[0].rgba.assign(16, 255);
    std::vector<Vertex> tex = sq;
    for (Vertex& v : tex) {
        v.alpha = 1;
        v.shape = {v.position.x + .5f, .5f - v.position.y, 0, 1};
    }
    raster({}, tex, &pics, .5f, .5f, c);
    check(at(c, 4, 4) == 0xffffffffu, "a portrait's pixels are drawn");
    raster({}, tex, nullptr, .5f, .5f, c);
    check(at(c, 4, 4) == 0, "no portraits: no textured pixels");
    check(hashOf(sq, {}) != hashOf(two, {}) && hashOf(sq, {}) == hashOf(sq, {}) && hashOf(sq, {}) != hashOf({}, sq),
          "the picture's key follows its vertices");

    // what it shows: each hero only what works
    Now now;
    holo_roster::Data d = rosterData(now);
    check(d.sm1.on && d.healthNa && d.suitsKnown && !d.comingSoon, "SM1 data: no health, no SOON, the outfits known");
    check(d.sm1.switches[hero_modes::spiderMan].empty() && d.sm1.power[hero_modes::spiderMan].empty(),
          "SPIDER-MAN: no switches, no power (iilyn's)");
    const auto has = [&](uint32_t h, int key) {
        for (const auto& w : d.sm1.switches[h])
            if (w.key == key)
                return true;
        return false;
    };
    check(has(hero_modes::ironMan, swPowers) && has(hero_modes::ironMan, swFlight) && !has(hero_modes::ironMan, swGrapple),
          "IRON MAN: POWERS, FLIGHT, no GRAPPLE");
    check(has(hero_modes::captainAmerica, swGrapple) && !has(hero_modes::captainAmerica, swFlight),
          "CAPTAIN AMERICA: GRAPPLE, no FLIGHT");
    check(has(hero_modes::daredevil, swGrapple), "DAREDEVIL: GRAPPLE");
    check(d.sm1.power[hero_modes::superman] == std::string("HEAT VISION"), "SUPERMAN: HEAT VISION");
    for (uint32_t h = 0; h < heroCount; ++h) {
        check(d.sm1.lines[h].size() <= holo_roster::sm1LinesMost && d.sm1.switches[h].size() <= holo_roster::sm1SwitchesMost,
              "the rows fit");
        check(d.suits.count[h] > 0, "every hero has outfits");
    }
    now.config.outfit[hero_modes::batman] = 1;
    d = rosterData(now);
    check(d.suits.rows[hero_modes::batman][1].status == roster_page::Status::inUse &&
              d.suits.rows[hero_modes::batman][0].status != roster_page::Status::inUse,
          "the saved outfit is the one marked");
    now.live.powers = false;
    d = rosterData(now);
    check(!d.sm1.switches[hero_modes::ironMan][0].on && d.sm1.switches[hero_modes::ironMan][0].key == swPowers,
          "the POWERS switch shows the live value");
    check(!playLines(hero_modes::ironMan, now).empty() && !playLines(hero_modes::spiderMan, now).empty(), "PLAY AS says what works");

    // COLOR: the SM2 build's mapping, round trip; every filter (none first)
    Live l;
    l.colorExposure = -7;
    l.colorWarmth = 30;
    l.colorContrast = 120;
    Live back;
    takeColor(colorSettings(l), back);
    check(back.colorExposure == -7 && back.colorWarmth == 30 && back.colorContrast == 120 && back.colorStrength == 100,
          "COLOR sliders round trip");
    l.colorFx = true;
    l.colorFilter = 3;
    check(colorBits(l) == (1u | 3u << 8), "COLOR bits as the SM2 build packs them");
    check(nextFilter(eye_color::filters - 1) == 0 && nextFilter(0) == 1, "FILTER steps through every look");
    check(!Live{}.colorFx, "COLOR is off by default");

    // the whole hologram: the SM2 panel with the heroes' tiles draws, and the hero picked changes it
    wrist_hud::setSm1Mode(true);
    now = Now{};
    d = rosterData(now);
    wrist_hud::Menu menu;
    menu.setPage(wrist_hud::powersPage);
    wrist_hud::Settings set = colorSettings(now.live);
    wrist_hud::Menu::Input in;
    in.active = true;
    in.heroTiles = true;
    in.rosterData = &d;
    in.hero = 0;
    menu.update(in, set);
    const auto paint = [&](uint32_t hero, Canvas& out) {
        wrist_hud::View v;
        v.page = menu.page();
        v.scroll = menu.scroll();
        v.roster = menu.roster();
        v.heroTiles = true;
        v.rosterData = &d;
        v.settings = set;
        v.color = colorBits(now.live);
        d.playing = hero;
        std::vector<Vertex> tr, tx;
        panel(v, tr, tx);
        out = Canvas(width, height);
        raster(tr, tx, nullptr, halfWidth, halfHeight, out);
        return tr.size();
    };
    Canvas a, b;
    check(paint(0, a) > 1000, "the hologram has the SM2 panel's geometry");
    size_t lit = 0;
    for (uint32_t px : a.pixels)
        lit += (px >> 24) > 20;
    check(lit > a.pixels.size() / 10, "the hologram's picture is drawn");
    paint(hero_modes::ironMan, b);
    check(a.pixels != b.pixels, "the hero played shows on the hologram");
    wrist_hud::setSm1Mode(false);
    std::vector<Vertex> n;
    note("PLAYING IRON MAN", playLines(hero_modes::ironMan, now), n);
    check(!n.empty(), "the PLAY AS note draws");
    Canvas dot;
    paintDot(dot);
    check(dot.width == dotSize && (at(dot, dotSize / 2, dotSize / 2) >> 24) > 200 && at(dot, 0, 0) == 0, "the pointer's dot");
    // the painter thread: what it is asked, once
    Painter painter;
    painter.ask(7, sq, {}, .5f, .5f, 10, 10);
    Canvas got;
    uint64_t key = 0;
    for (int i = 0; i < 2000 && !painter.take(got, key); ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    check(key == 7 && got.width == 10 && (got.pixels[0] >> 24) == 128, "the painter paints off the frame");
    // 0.0.76 test: the hologram reopened with the same panel asked for the picture it had already taken; the
    // painter answered nothing and the hologram stayed invisible. A taken picture is painted again when asked.
    painter.ask(7, sq, {}, .5f, .5f, 10, 10);
    got = Canvas();
    key = 0;
    for (int i = 0; i < 2000 && !painter.take(got, key); ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    check(key == 7 && got.width == 10, "a reopened hologram gets its picture again");
    painter.stop();
}

bool near3(Vec3 a, Vec3 b, float e = 1e-3f) { return length(a - b) <= e; }

void propsTests() {
    namespace pr = spidy::sm1::props;
    check(pr::modelsFor(hero_modes::thor).size() == 1 && pr::modelsFor(hero_modes::thor)[0] == pr::hammer, "THOR holds the hammer");
    check(pr::modelsFor(hero_modes::captainAmerica)[0] == pr::shield, "CAP holds the shield");
    check(pr::modelsFor(hero_modes::wolverine).size() == 2, "WOLVERINE has claws on both hands");
    check(!pr::hasProps(hero_modes::spiderMan) && !pr::hasProps(hero_modes::ironMan), "Spider-Man and Iron Man hold nothing");
    check(pr::thrownBy(hero_modes::thor, 1) == pr::hammer && pr::thrownBy(hero_modes::thor, 0) == pr::none,
          "the right trigger throws THOR's hammer; the left keeps the lightning");
    check(pr::slotOf(pr::hammer) == mesh_model::slotHammer && pr::slotOf(pr::shield) == mesh_model::slotShield, "the SM2 model slots");

    // The SM2 appendModel frame: world = at + right x + up y - forward z.
    const pr::Place k = pr::placed({1, 2, 3}, {0, 0, -2}, {0, 1, .3f});
    check(k.ok && near3(pr::apply(k, {0, 0, -1}), {1, 2, 2}) && near3(pr::apply(k, {0, 1, 0}), {1, 3, 3}) &&
              near3(pr::apply(k, {1, 0, 0}), {2, 2, 3}),
          "a placed model: -z forward, +y up, +x right");
    check(!pr::placed({0, 0, 0}, {0, 1, 0}, {0, 2, 0}).ok, "a degenerate frame places nothing");

    // THOR's hammer in the right fist: its handle end gripAlong below the palm, +y along the handle.
    Pose grip{{.2f, 1.1f, -.3f}, Quat::around({1, 0, 0}, -.4f)};
    const pr::Place h = pr::hammerInHand(grip, ControllerKind::touch);
    const thor_grip::Held held = thor_grip::held(grip, ControllerKind::touch);
    check(h.ok && held.ok && near3(pr::apply(h, {0, thor_grip::gripAlong, 0}), grip.position), "the hammer's grip is in the palm");
    check(near3(normalized(h.y), held.handle) && near3(h.z * -1.f, held.face), "the hammer's handle and face from thor_grip");
    const pr::Place spun = pr::hammerPlace(held.at, held.handle, held.face, 3.14159265f);
    check(spun.ok && near3(spun.y, held.handle * -1.f), "half a turn of spin points the handle back");
    check(!pr::hammerInHand(Pose{{0, 0, 0}, Quat{0, 0, 0, 0}}, ControllerKind::touch).ok, "a broken grip pose draws no hammer");

    // CAP's shield: shieldPlace is captain::place(forSide(m)) for both arms.
    const captain::ShieldPose arm = pr::shieldOnForearm(grip, 1, ControllerKind::touch);
    check(arm.ok, "the shield on the right forearm");
    const Vec3 low{-.3f, -.42f, -.05f}, high{.3f, .2f, .04f};
    for (uint32_t side : {0u, 1u}) {
        const pr::Place s = pr::shieldPlace(arm, side, captain::pivotLeft, low, high, 0);
        bool same = s.ok;
        for (Vec3 m : {Vec3{0, 0, 0}, Vec3{.1f, -.2f, .03f}, Vec3{-.25f, .1f, -.04f}})
            same = same && near3(pr::apply(s, m), captain::place(arm, captain::forSide(m, side)));
        check(same, side ? "the right arm's shield is captain::place" : "the left arm's shield is captain::place");
        const pr::Place r = pr::shieldPlace(arm, side, captain::pivotLeft, low, high, 1.3f);
        const Vec3 mid = (low + high) * .5f;
        check(r.ok && near3(pr::apply(r, mid), pr::apply(s, mid)), "a spinning shield turns about its disc's middle");
    }
    const pr::Place fly = pr::shieldFlying({4, 2, -6}, {0, 0, -1}, 1, captain::pivotLeft, low, high, .7f);
    check(fly.ok && near3(pr::apply(fly, (low + high) * .5f), {4, 2, -6}) && near3(normalized(fly.z * -1.f), {0, 1, 0}),
          "a flying shield: flat, face up, its middle on the flight");

    // The other held props: placed off the grip, finite, near the hand.
    for (unsigned hand : {0u, 1u}) {
        const pr::Place c = pr::clawsPlace(grip, hand, ControllerKind::touch);
        const pr::Place b = pr::batonPlace(grip, hand, ControllerKind::touch, {-.03f, -.03f, -.4f}, {.03f, .03f, .17f});
        const pr::Place g = pr::pistolPlace(grip, hand, ControllerKind::touch);
        check(c.ok && length(c.at - grip.position) < .1f, "the claws at the knuckles");
        check(b.ok && length(b.at - grip.position) < .6f, "the baton in the fist");
        check(g.ok && length(g.at - grip.position) < .1f, "the pistol in the palm");
    }
    const pr::Place l = pr::clawsPlace(grip, 0, ControllerKind::touch), r = pr::clawsPlace(grip, 1, ControllerKind::touch);
    check(near3(l.y, r.y * -1.f) && near3(l.z, r.z), "each hand's claws out of its own back of the hand, along the punch");
    check(std::abs(pr::clawReach({0, 0, -.3f}) - .3f) < 1e-6f && pr::clawReach({0, 0, .2f}) == .05f, "the claws' reach");
    const pr::Place bat = pr::batarangFlying({0, 1, -3}, {0, 0, -1}, 0, {-.2f, -.01f, -.1f}, {.2f, .01f, .1f});
    check(bat.ok && near3(pr::apply(bat, {0, 0, 0}), {0, 1, -3}) && std::abs(length(bat.x) - .4f) < 1e-3f,
          "a flying batarang: centred, 16 cm across");

    // The throw: out to the enemy (the hit once, on arrival), back to the moving hand, caught. Never moves the player.
    pr::Flight f;
    const Vec3 enemy{0, 1, -10};
    check(pr::launch(f, {0, 1, 0}, {0, 0, -1}, &enemy, pr::hammerThrow) && f.phase == pr::Flight::out, "the hammer is thrown");
    check(!pr::launch(f, {0, 1, 0}, {0, 0, -1}, &enemy, pr::hammerThrow), "a hammer in the air cannot be thrown again");
    int arrivals = 0, steps = 0;
    bool caught = false;
    Vec3 hand{.2f, 1, .1f};
    float arrivedAt = 0;
    while (!caught && steps < 2000) {
        hand = hand + Vec3{.001f, 0, 0};
        const pr::FlightStep st = pr::step(f, pr::hammerThrow, hand, &enemy, 1.f / 90);
        arrivals += st.arrived;
        if (st.arrived)
            arrivedAt = f.age;
        caught = st.caught;
        ++steps;
    }
    check(arrivals == 1, "the hammer hits once");
    check(arrivedAt > .4f && arrivedAt < .5f, "out at 22 m/s: 10 m in under half a second");
    check(caught && f.phase == pr::Flight::held && near3(f.pos, hand), "the hammer comes back into the hand");
    check(steps < 2 * 90, "the hammer is back within two seconds");
    pr::Flight miss;
    pr::launch(miss, {0, 1, 0}, {1, 0, 0}, nullptr, pr::hammerThrow);
    float furthest = 0;
    for (int i = 0; i < 1000 && miss.phase != pr::Flight::held; ++i) {
        const pr::FlightStep st = pr::step(miss, pr::hammerThrow, {0, 1, 0}, nullptr, 1.f / 90);
        check(!st.arrived, "a throw with no enemy hits nothing");
        furthest = std::max(furthest, miss.pos.x);
    }
    check(furthest > 20 && furthest < 23.5f && miss.phase == pr::Flight::held, "an unaimed throw flies its second and comes back");
    pr::Flight b;
    pr::launch(b, {0, 1, 0}, {0, 0, -1}, nullptr, pr::batarangThrow);
    bool gone = false;
    for (int i = 0; i < 200 && !gone; ++i)
        gone = pr::step(b, pr::batarangThrow, {0, 1, 0}, nullptr, 1.f / 90).caught;
    check(gone && b.phase == pr::Flight::held, "the batarang does not come back: it ends");
    pr::Flight stuck;
    pr::launch(stuck, {0, 1, 0}, {0, 0, -1}, nullptr, pr::shieldThrow);
    for (int i = 0; i < 90 * 7 && stuck.phase != pr::Flight::held; ++i)
        pr::step(stuck, pr::shieldThrow, {1e6f, 0, 0}, nullptr, 1.f / 90);
    check(stuck.phase == pr::Flight::held, "a prop that cannot reach the hand is recalled");

    // Game <-> OpenXR through the head: round trip, and the game head's forward is the headset's -z.
    const Pose xrHead{{.1f, 1.6f, -.2f}, Quat::yaw(.7f)};
    const pr::HeadMap m = pr::headMap({100, 20, -50}, {0, 0, 1}, {0, -1, 0}, {-1, 0, 0}, xrHead);
    check(m.ok, "a head map");
    const Vec3 g{103, 19, -48};
    check(near3(pr::toGame(m, pr::toXr(m, g)), g), "game -> OpenXR -> game");
    check(near3(pr::toXr(m, Vec3{99, 20, -50}), xrHead.position + xrHead.orientation.rotate({0, 0, -1})),
          "a metre ahead of the game head is a metre ahead of the headset");
    check(near3(pr::dirToXr(m, {0, 1, 0}), xrHead.orientation.rotate({0, 1, 0})), "the game's up is the head's up");
    check(!pr::headMap({0, 0, 0}, {0, 0, 0}, {0, -1, 0}, {0, 0, 1}, xrHead).ok, "no head axes: no map");

    // The GPU's vertices: one a corner, the texture layer (cutout flagged) and the tint.
    mesh_model::Mesh tri;
    tri.corners = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {0, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    tri.colors = {{.5f, .5f, .5f}, {.2f, .3f, .4f}};
    tri.uvs = {0, 0, 1, 0, 0, 1, 0, 0, 1, 1, 0, 1};
    tri.tints = {{1, .5f, .25f}, {1, 1, 1}};
    tri.image = {0, -1};
    tri.images.resize(1);
    tri.images[0].cutout = true;
    tri.images[0].layer = 3;
    std::vector<pr::GpuVertex> verts;
    check(pr::pack(tri, verts) && verts.size() == 6, "three vertices a triangle");
    check(verts[0].layer == 3 + pr::cutoutLayer && verts[0].colour[1] == .5f, "a cutout texture's layer and tint");
    check(verts[3].layer == -1 && verts[3].colour[2] == .4f, "an untextured triangle's own colour");
    check(std::abs(verts[0].normal[2] - 1) < 1e-5f && std::abs(verts[3].normal[0] - 1) < 1e-5f, "the face normals");
    mesh_model::Mesh broken;
    broken.corners = {{0, 0, 0}};
    broken.colors = {{1, 1, 1}};
    check(!pr::pack(broken, verts) && verts.size() == 6, "a broken mesh adds nothing");

    // The eye: a point a metre straight ahead is in the middle of the picture, in depth 0..1.
    const Pose eye{{0, 1.6f, 0}, Quat::yaw(.5f)};
    const Mat4 vp = pr::eyeViewProjection(eye, -.8f, .8f, .8f, -.8f);
    const Vec3 ahead = eye.position + eye.orientation.rotate({0, 0, -1});
    float clip[4]{};
    for (int r = 0; r < 4; ++r)
        clip[r] = vp[r * 4] * ahead.x + vp[r * 4 + 1] * ahead.y + vp[r * 4 + 2] * ahead.z + vp[r * 4 + 3];
    check(clip[3] > 0 && std::abs(clip[0] / clip[3]) < 1e-4f && std::abs(clip[1] / clip[3]) < 1e-4f && clip[2] / clip[3] > 0 &&
              clip[2] / clip[3] < 1,
          "the eye's projection");
    float model[12], normal[9];
    pr::Place scaled;
    scaled.at = {1, 2, 3}, scaled.x = {2, 0, 0}, scaled.y = {0, 1, 0}, scaled.z = {0, 0, 1}, scaled.ok = true;
    check(pr::modelMatrices(scaled, model, normal) && model[0] == 2 && model[3] == 1 && model[11] == 3 && std::abs(normal[0] - .5f) < 1e-6f,
          "the model and normal matrices");
    scaled.x = {0, 0, 0};
    check(!pr::modelMatrices(scaled, model, normal), "a flat place has no normal matrix");
}

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
    suitTests();
    scanTests();
    combatTests();
    grappleTests();
    colorTests();
    holoTests();
    propsTests();
    if (failures) {
        std::printf("%d failure(s)\n", failures);
        return 1;
    }
    std::printf("sm1 add-on tests passed\n");
    return 0;
}
