// HERO VR 0.0.78 cut19 tests, from report 223845 (cut18 0ee482f, session 223845; the owner: "oh look immersive
//  cutscenes gone again"): LOADED GAME. The boot, the title (VR-play blips of 1953 and 110 ms kept it flat), slot 3
//  loaded from the title with only ~0.57 s of no record (39.34-39.91 s), the loaded game's record on the screen 0.5 s,
//  the screen down 0.85 s, 1235 ms of VR play on the loaded record (cut18: TITLE BLIP #3, MENU SCREEN #3 the front end,
//  every scene after it flat), then a 4.3 s scene with the screen up and no buttons, then the menu button (the pause
//  menu) and the quit to the title. The scene must go immersive; the title, the pause menu and the title after the
//  quit stay flat. Report 212843's cases are in tests/cut17_tests.cpp and must still pass.
#include "spidy/cutscene077.hpp"
#include "spidy/cutscene077_gate.hpp"
#include "spidy/presentation_gate.hpp"

#include <cstdint>
#include <cstdio>

using namespace spidy;

namespace {
int failures = 0;
void check(bool ok, const char* what) {
    std::printf("%s: %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok)
        ++failures;
}
// report 223845: the title's record, slot 3's record, and the title's record after a quit from the pause menu
constexpr uint64_t title = 0x21431703080ull, blink = 0x21431700000ull, gameRecord = 0x21431dab5c0ull,
                   title2 = 0x21431709990ull;

struct Session {
    uint32_t mode = cutscene077::defaultMode;
    cutscene077::MotionClock motion;
    SceneClock clock;
    cutscene077::HeroGrace hero;
    cutscene077::MenuScreen menu;
    cutscene077::ImmersiveHold hold;
    bool wasShown{};
    uint64_t ms = 1000, moves{}, immersiveFrames{};
    struct Out {
        bool menu, immersive;
    };
    Out frame(bool screen, uint64_t record, bool heroOn, bool camMoves, bool menuPress = false, bool vrPlay = false,
              bool gameplay = false, bool viewing = true) {
        ms += 11;
        if (camMoves)
            ++moves;
        cutscene077::FrameInput f;
        f.mode = mode;
        f.screen = screen;
        f.viewing = viewing;
        f.vrPlay = vrPlay;
        f.gameplay = gameplay;
        f.menuPressed = menuPress;
        f.timeRuns = clock.running(motion.advancing(false, false, moves, screen, ms), ms);
        f.heroOk = hero.present(heroOn, wasShown, ms);
        f.bridge = true;
        f.head = true;
        f.record = record;
        f.nowMs = ms;
        const bool m = cutscene077::menuScreenFrame(menu, f);
        const bool im = cutscene077::immersiveFrame(hold, m, f);
        wasShown = im;
        immersiveFrames += im;
        return {m, im};
    }
    bool flat(int n, uint64_t record, bool heroOn, bool camMoves, bool press = false) {
        bool ok = true;
        for (int i = 0; i < n; ++i) {
            const Out o = frame(true, record, heroOn, camMoves, press && i == 0);
            ok &= o.menu && !o.immersive;
        }
        return ok;
    }
    void play(int n, uint64_t record) {
        for (int i = 0; i < n; ++i)
            frame(false, record, true, true, false, true, true);
    }
    // The cut16 boot and title up to the blip (samples 224-293 of the report).
    bool boot(bool transient) {
        bool ok = flat(900, 0, false, false); // ~10 s of boot, no player record
        if (transient)
            ok &= flat(3, blink, true, false) && flat(200, 0, false, false); // a record for 3 frames
        for (int i = 0; i < 60; ++i) // the screen down with no headset view: the title's record arrives
            frame(false, i < 20 ? 0 : title, i >= 20, false, false, false, false, false);
        ok &= flat(150, title, true, true); // the title
        return ok;
    }
    // The blip: VR play on the title's own record for n frames, then the 250 ms screen delay (screen down, no play).
    void blip(int n, uint64_t record = title) {
        play(n, record);
        for (int i = 0; i < 23; ++i)
            frame(false, record, true, true);
    }
};
} // namespace

int main() {
    // --- LOADED GAME: report 223845 ---
    {
        Session s;
        bool ok = s.boot(false);
        s.blip(178); // TITLE BLIP #1: 1953 ms on the title's record
        ok &= s.flat(500, title, true, true);
        ok &= s.flat(1, title, true, true, true) && s.flat(1800, title, true, true); // the title's menus, the saves
        check(ok && s.immersiveFrames == 0 && s.menu.frontEnd(), "223845: the title and its menus stay flat");
        // A on slot 3: no record ~0.57 s, the loaded game's record on the screen 0.5 s (a loading screen: flat)
        ok = s.flat(52, 0, false, false) && s.flat(46, gameRecord, true, false);
        check(ok && s.immersiveFrames == 0, "223845: the load (0.57 s gap) and its first 0.5 s stay flat");
        for (int i = 0; i < 77; ++i) // the screen down, a 110 ms VR-play blip in it (TITLE BLIP #2)
            s.frame(false, gameRecord, true, true, false, i >= 30 && i < 40, i >= 30 && i < 40);
        s.play(112, gameRecord); // 1235 ms of VR play (cut18: TITLE BLIP #3, the front end kept)
        check(!s.menu.frontEnd(), "223845: VR play on the loaded game's record ends the front end");
        uint64_t im = 0;
        for (int i = 0; i < 390; ++i) // 42.6-46.9 s: the scene, the screen up, no buttons
            im += s.frame(true, gameRecord, true, true).immersive;
        check(im > 300, "223845: the scene after the load is immersive (cut18: flat)");
        // 46.9 s: the menu button - the pause menu flat, then the quit to the title: flat again
        ok = s.flat(1, gameRecord, true, false, true) && s.flat(300, gameRecord, true, true);
        ok &= s.flat(200, 0, false, false) && s.flat(400, title2, true, true);
        check(ok, "223845: the pause menu and the quit to the title flat");
        s.blip(110, title2); // a 1.2 s blip on the new title's own record: still the front end
        const uint64_t before = s.immersiveFrames;
        ok = s.flat(1000, title2, true, true);
        check(ok && s.immersiveFrames == before && s.menu.frontEnd(), "223845: the title after the quit stays flat");
    }
    // the same short load straight into a scene (no VR play first): immersive once the record is read 1 s
    {
        Session s;
        bool ok = s.boot(false);
        ok &= s.flat(1000, title, true, true);
        ok &= s.flat(52, 0, false, false) && s.flat(80, gameRecord, true, true);
        uint64_t im = 0;
        for (int i = 0; i < 400; ++i)
            im += s.frame(true, gameRecord, true, true).immersive;
        check(ok && !s.menu.frontEnd() && im > 0, "a 0.57 s load gap straight into a scene: immersive after 1 s");
    }
    // the title's record dropping for a moment and coming back (no load): the title stays flat
    {
        Session s;
        bool ok = s.boot(false);
        ok &= s.flat(500, title, true, true) && s.flat(30, 0, false, false) && s.flat(1500, title, true, true);
        s.blip(150);
        ok &= s.flat(500, title, true, true);
        check(ok && s.immersiveFrames == 0 && s.menu.frontEnd(), "a title record gap and a blip: the title stays flat");
    }

    std::printf(failures ? "CUT19 FAILED: %d\n" : "CUT19 PASSED\n", failures);
    return failures ? 1 : 0;
}
