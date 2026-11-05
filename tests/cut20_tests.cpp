// HERO VR 0.0.78 cut20 tests: PAUSE RESUME and the both-thumbsticks pause.
//  (a) report 232334 (cut19 284711b, the owner: "opening cutscene goes back and fourth from immersive to 2d"): the
//      title, slot loaded, 1.1 s of VR play on the loaded record (26.64-27.78 s), the opening scene immersive from
//      28.20 s, the menu button at 30.83 s (the pause menu, the clock frozen), B at 31.91 s, the scene running on to
//      56.64 s, a second pause (B at 57.55 s) and the scene to the session's end. cut19 kept the scene flat after the
//      first pause to the end; it must go immersive again after each pause, the pause menus flat.
//  (b) report 233224 (a live v0.0.77 player under SteamVR, record 0x1ff577fe0c0): both thumbsticks held in
//      28.17-28.98 s (SteamVR keeps the menu button for its dashboard), the game's Start at 28.98 s (the pause),
//      A 30.48 s and 31.84 s, the quit to the title at 32.39 s. In an immersive scene that pause never reached the
//      cutscene code (cut19: held as a QTE freeze up to 15 s, the pause menu shown immersive); it must be flat at once.
//  (c) a pause with no close, a pause whose A opens a moving menu (Suits), and a pause from VR play stay flat.
// Build with -DCUT20_OLD against 284711b's cutscene077.hpp (no cut20 fields) to see (a) and (b) fail there.
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
// report 232334's title and loaded-game records; report 233224's game record
constexpr uint64_t title = 0x20ab210dd00ull, gameRecord = 0x20ab26ba540ull, liveRecord = 0x1ff577fe0c0ull;

struct Press {
    bool menu, stick, b, a;
};
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
    Out frame(bool screen, uint64_t record, bool heroOn, bool camMoves, Press p = {}, bool vrPlay = false,
              bool gameplay = false) {
        ms += 11;
        if (camMoves)
            ++moves;
        cutscene077::FrameInput f;
        f.mode = mode;
        f.screen = screen;
        f.viewing = true;
        f.vrPlay = vrPlay;
        f.gameplay = gameplay;
        f.menuPressed = p.menu;
#ifndef CUT20_OLD
        f.stickPause = p.stick;
        f.closePressed = p.b; // game_xr: buttonB (A only picks an item)
        f.shownBefore = wasShown;
#endif
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
    // n frames on the screen; true when every one was the flat VR screen
    bool flat(int n, uint64_t record, bool camMoves, Press first = {}) {
        bool ok = true;
        for (int i = 0; i < n; ++i) {
            const Out o = frame(true, record, record != 0, camMoves, i == 0 ? first : Press{});
            ok &= o.menu && !o.immersive;
        }
        return ok;
    }
    // n frames of a scene on the screen; the immersive frames
    uint64_t scene(int n, uint64_t record, bool camMoves = true, Press first = {}) {
        uint64_t im = 0;
        for (int i = 0; i < n; ++i)
            im += frame(true, record, true, camMoves, i == 0 ? first : Press{}).immersive;
        return im;
    }
    void down(int n, uint64_t record) { // the screen down, no VR play (the 250 ms screen delay, a load)
        for (int i = 0; i < n; ++i)
            frame(false, record, record != 0, false);
    }
    void play(int n, uint64_t record) {
        for (int i = 0; i < n; ++i)
            frame(false, record, true, true, {}, true, true);
    }
    // the boot (no record) and the title (its record read with the screen down, then the title up)
    bool boot(uint64_t titleRecord) {
        bool ok = flat(900, 0, false);
        for (int i = 0; i < 60; ++i)
            frame(false, i < 20 ? 0 : titleRecord, i >= 20, false);
        return ok && flat(1500, titleRecord, true);
    }
    // a pause: the press, the pause menu (the clock frozen) for menuFrames, then B held closeFrames (still frozen)
    bool pause(int menuFrames, uint64_t record, Press press, int closeFrames = 9) {
        bool ok = flat(1, record, false, press) && flat(menuFrames, record, false);
        Press b;
        b.b = true;
        for (int i = 0; i < closeFrames; ++i) {
            const Out o = frame(true, record, true, false, b);
            ok &= o.menu && !o.immersive;
        }
        return ok;
    }
};
// (a) report 232334 up to the opening scene's first pause: the scene immersive (frames)
uint64_t opening232334(Session& s, bool& ok) {
    ok = s.boot(title);
    ok &= s.flat(1, title, true, Press{true}) && s.flat(1200, title, true); // the title's menus, the saves
    s.down(27, 0);                     // 24.92-25.22 s: no record, the screen down
    ok &= s.flat(25, 0, false);        // 25.22-25.50 s: the loading screen, no record
    ok &= s.flat(56, gameRecord, false); // 25.50-26.12 s: the loaded record, the camera still
    s.down(47, gameRecord);            // 26.12-26.64 s
    s.play(104, gameRecord);           // 26.64-27.78 s: VR play (the first flip; the front end ends)
    return s.scene(277, gameRecord);   // 27.78-30.83 s: the opening scene
}
} // namespace

int main() {
    // --- (a) report 232334: the opening scene, two pauses ---
    {
        Session s;
        bool ok = false;
        const uint64_t im = opening232334(s, ok);
        check(ok && !s.menu.frontEnd() && im > 200, "232334: the title flat, the opening scene immersive (238 frames)");
        Press menu;
        menu.menu = true;
        ok = s.pause(98, gameRecord, menu); // 30.83 s menu, the pause menu to B at 31.91 s
        check(ok, "232334: the first pause menu is the flat VR screen (MENU SCREEN #4)");
        uint64_t before = s.immersiveFrames;
        const bool menuFirst = s.flat(80, gameRecord, true); // the first 0.9 s back: still the screen
        const uint64_t im2 = s.scene(2185, gameRecord);      // to 56.64 s
        check(menuFirst && im2 > 2000, "232334: the scene after the first pause goes immersive again (cut19: flat)");
        ok = s.pause(74, gameRecord, menu); // 56.64 s menu, B at 57.55 s
        check(ok, "232334: the second pause menu is the flat VR screen");
        before = s.immersiveFrames;
        const uint64_t im3 = s.scene(2500, gameRecord); // to the session's end (86 s)
        check(im3 > 2300 && s.immersiveFrames - before == im3,
              "232334: the scene after the second pause immersive again");
#ifndef CUT20_OLD
        check(s.menu.resumes() == 2, "232334: two PAUSE RESUME lines");
#endif
    }
    // --- (b) report 233224: SteamVR's both-thumbsticks pause in an immersive scene ---
    {
        Session s;
        bool ok = s.boot(0x1ff57466140ull);
        ok &= s.flat(1, 0x1ff57466140ull, true, Press{true}) && s.flat(300, 0x1ff57466140ull, true);
        s.down(21, 0);                       // 17.72-17.95 s: no record
        ok &= s.flat(93, 0, false);          // 17.95-18.97 s: the loading screen
        s.down(105, liveRecord);             // 18.97-20.12 s: the loaded record, the screen down
        s.play(110, liveRecord);             // 20.12-21.3 s: VR play on the loaded record
        const uint64_t im = s.scene(600, liveRecord);
        check(ok && im > 500, "233224: a scene on the live player's record immersive (they had cutscenes = flat)");
        Press stick;
        stick.stick = true; // 28.98 s: Start from the stick hold (pausePress.pauses() +1), the menu button never down
        bool flatNow = true;
        for (int i = 0; i < 135; ++i) { // 28.98-30.48 s: the pause menu, the clock frozen
            const Session::Out o = s.frame(true, liveRecord, true, false, i == 0 ? stick : Press{});
            flatNow &= o.menu && !o.immersive;
        }
        check(flatNow, "233224: the both-thumbsticks pause is the flat VR screen at once (cut19: held immersive)");
        Press a;
        a.a = true;
        ok = s.flat(1, liveRecord, false, a) && s.flat(120, liveRecord, false) && s.flat(1, liveRecord, false, a);
        ok &= s.flat(45, liveRecord, false) && s.flat(300, 0, false) && s.flat(600, 0x1ff5705a9c0ull, true);
        check(ok && s.menu.frontEnd(), "233224: A, A: the quit to the title stays flat (no resume)");
    }
#ifndef CUT20_OLD
    // --- (c) what must stay flat ---
    {
        Session s;
        bool ok = false;
        opening232334(s, ok);
        Press menu;
        menu.menu = true;
        ok = s.flat(1, gameRecord, false, menu) && s.flat(300, gameRecord, false) && s.flat(600, gameRecord, true);
        check(ok && s.menu.resumes() == 0, "a scene's pause with no B: the menu stays flat even when its camera moves");
    }
    {
        Session s;
        bool ok = false;
        opening232334(s, ok);
        Press menu, a;
        menu.menu = true;
        a.a = true;
        ok = s.pause(60, gameRecord, menu);        // pause, B in the pause menu (back from nothing: still frozen)
        ok &= s.flat(60, gameRecord, false);       // still the pause menu (frozen): the close taken back
        ok &= s.flat(1, gameRecord, false, a) && s.flat(900, gameRecord, true); // A: Suits, its camera moving
        check(ok && s.menu.resumes() == 0, "a scene's pause, B then the pause menu, A into a moving menu: flat");
        ok = s.pause(60, gameRecord, Press{}, 9); // B out of Suits to the pause menu (frozen)
        ok &= s.flat(60, gameRecord, false);
        Press b;
        b.b = true;
        ok &= s.flat(1, gameRecord, false, b); // B: the pause menu closes
        const uint64_t im = s.scene(300, gameRecord);
        check(ok && im > 150 && s.menu.resumes() == 1, "...then B out of the pause menu: the scene immersive again");
    }
    {
        Session s;
        bool ok = false;
        opening232334(s, ok);
        s.play(300, gameRecord); // the scene over, VR play
        Press menu;
        menu.menu = true;
        ok = s.flat(1, gameRecord, false, menu) && s.flat(200, gameRecord, false); // a pause from VR play
        ok &= s.pause(1, gameRecord, Press{}) && s.flat(400, gameRecord, true);
        check(ok && s.menu.resumes() == 0, "a pause from VR play (the 3 s rule) stays flat after B");
    }
    {
        Session s;
        bool ok = false;
        opening232334(s, ok);
        Press menu;
        menu.menu = true;
        ok = s.pause(98, gameRecord, menu) && s.flat(100, 0, false) && s.flat(400, title, true); // quit to the title
        s.down(30, title);
        ok &= s.flat(1500, title, true);
        check(ok && s.menu.resumes() == 0 && s.menu.frontEnd(), "a scene's pause, B, the quit to the title: flat");
    }
#endif

    std::printf(failures ? "CUT20 FAILED: %d\n" : "CUT20 PASSED\n", failures);
    return failures ? 1 : 0;
}
