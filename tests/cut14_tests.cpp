// HERO VR 0.0.78 CUT14 tests: IMMERSIVE BY DEFAULT keeps the main menu flat (the owner: "must NOT break the main menu").
// The mod's per-frame chain (game_xr.cpp, word for word: MotionClock -> SceneClock -> HeroGrace ->
// cutscene077::menuScreenFrame -> cutscene077::immersiveFrame) replays the player's own sessions with cutscenes at the
// new default, immersive, from the very first frame (no live.txt line, cutscene077::defaultMode):
//  A. report 174648 (cut13b, console game-vr-20261104-174648-console.log): CUT7 BOOT SCREEN "the VR screen, viewing"
//     at 2..19 s (lines 58-121), then "VR play" from 21 s (line 161). The boot and title must be the VR screen on every
//     frame from frame 0 (cut9's TITLE FLAT), even with a hero record and a moving title camera.
//  B. report 025037 (cut8, console 01M6JRZ69XVK2SH9KPHDNN03AA.log, explicit cutscenes = immersive): boot (MENU SCREEN
//     #1, line 54), the title's 7.8 s (line 109), its menus, a game loaded from the title (an immersive scene may
//     follow), the menu button in a scene (MENU SCREEN #2 the pause, line 104), back to the title from the pause menu
//     (MENU SCREEN #4 / #5 "no player record", lines 385-394).
//  C. the pause menu in VR gameplay, D. a pre-rendered video (no 3D camera: the camera never moves), E. explicit flat
//     and 3d (a player's own choice) are never immersive.
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
constexpr uint64_t title = 0x1f2a40001ull, gameRecord = 0x1f2a40777ull;

// One session: the state game_xr.cpp keeps across frames.
struct Session {
    uint32_t mode = cutscene077::defaultMode;
    cutscene077::MotionClock motion;
    SceneClock clock;
    cutscene077::HeroGrace hero;
    cutscene077::MenuScreen menu;
    cutscene077::ImmersiveHold hold;
    bool wasShown{};
    uint64_t ms = 1000, moves{};
    uint64_t frames{}, menuFrames{}, immersiveFrames{};
    // One headset frame (11 ms). screen: the game's screen is up; heroOn: a player hero; camMoves: the game's camera
    // moved this frame (a pre-rendered video or a menu: it does not).
    struct Out {
        bool menu, immersive;
    };
    Out frame(bool screen, uint64_t record, bool heroOn, bool camMoves, bool menuPress = false, bool vrPlay = false,
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
        ++frames;
        menuFrames += m;
        immersiveFrames += im;
        return {m, im};
    }
    // Frames on the game's screen; true when every one was the normal VR screen and none immersive.
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
};
} // namespace

int main() {
    check(cutscene077::defaultMode == cutscene077::immersive, "the default (no live.txt cutscenes line) is immersive");

    // --- A. report 174648 (cut13b), immersive from frame 0 ---
    {
        Session s;
        // boot: the game's screen up from the first frame, no record (BOOT SCREEN at 2 s / 4 s / 6 s)
        bool ok = s.flat(550, 0, false, false);
        // the boot movie / logos (the camera still) and the title with an animated camera and a hero record
        ok &= s.flat(400, title, false, true);
        ok &= s.flat(700, title, true, true);
        check(ok && s.immersiveFrames == 0, "report 174648: boot and title flat on every frame from frame 0 (no press)");
        check(s.menu.count() == 1 && s.menu.why() == 1, "report 174648: one MENU SCREEN, the front end, from boot");
        // the title's menus (the record flickers) still flat
        ok = s.flat(50, 0, false, false) && s.flat(100, title, true, true);
        check(ok, "report 174648: the title's menus stay flat");
        // the save loads (no record >= 1.5 s), then VR play from 21 s (line 161)
        s.flat(200, 0, false, false);
        s.play(300, gameRecord);
        check(!s.menu.frontEnd(), "report 174648: VR gameplay ends the front end");
    }

    // --- B. report 025037 (cut8), now at the default ---
    {
        Session s;
        bool ok = s.flat(460, 0, false, false);      // boot (MENU SCREEN #1, the front end)
        ok &= s.flat(710, title, true, true);        // the title's 7.8 s (cut8 showed it immersive: line 109)
        check(ok && s.immersiveFrames == 0, "report 025037: boot and the title's 7.8 s flat (the cut8 bug gone)");
        // a game loaded from the title: the gap, then the game's opening scene (its camera moving) may be immersive
        s.flat(200, 0, false, false);
        uint64_t shown = 0;
        for (int i = 0; i < 300; ++i)
            shown += s.frame(true, gameRecord, true, true).immersive;
        check(shown > 200, "report 025037: the game's opening scene is immersive (in-game scenes still work)");
        // the menu button in that scene: the pause menu on the VR screen (MENU SCREEN #2, line 104)
        bool paused = s.flat(1, gameRecord, true, false, true);
        paused &= s.flat(300, gameRecord, true, false);  // the pause menu: the camera still
        check(paused, "report 025037: the menu button in a scene -> the pause menu flat (SCENE END: menu)");
        check(s.menu.why() == 2, "report 025037: that stretch is the pause (why 2)");
        // Quit to the main menu from the pause menu: no player record, then the title (MENU SCREEN #4/#5)
        ok = s.flat(200, 0, false, false) && s.flat(400, title, false, true);
        check(ok, "report 025037: back to the title from the pause menu: flat");
    }

    // --- C. the pause menu in VR gameplay ---
    {
        Session s;
        s.flat(460, 0, false, false);
        s.flat(300, title, true, true);
        s.flat(200, 0, false, false);
        s.play(600, gameRecord);
        // the menu button: the game's screen up, the camera still behind the pause menu
        bool ok = s.flat(1, gameRecord, true, false, true) && s.flat(600, gameRecord, true, false);
        // the pause menu's own animated background (the camera moving) while still paused
        ok &= s.flat(300, gameRecord, true, true);
        check(ok, "gameplay -> pause menu: flat for its whole stretch, the camera moving or not");
        s.play(100, gameRecord);
        // a hint card or the map (no press, the camera still): the screen, never immersive
        uint64_t im = 0;
        for (int i = 0; i < 400; ++i)
            im += s.frame(true, gameRecord, true, false).immersive;
        check(im == 0, "a hint card / map after gameplay (the camera still): never immersive");
    }

    // --- D. a pre-rendered video (a movie: no 3D camera, the game's camera never moves) ---
    {
        Session s;
        s.flat(460, 0, false, false);
        s.flat(300, title, true, true);
        s.flat(200, 0, false, false);
        s.play(300, gameRecord);
        uint64_t im = 0;
        for (int i = 0; i < 3000; ++i) // 33 s of video
            im += s.frame(true, gameRecord, true, false).immersive;
        check(im == 0, "a pre-rendered video after gameplay: always the flat screen");
        // a video at boot / on the title (the intro movie): the front end, flat
        Session b;
        check(b.flat(2000, 0, false, false) && b.flat(2000, title, false, false), "the boot / title video: flat");
    }

    // --- E. a player's own flat or 3d: never immersive ---
    for (uint32_t mode : {static_cast<uint32_t>(cutscene077::flat), static_cast<uint32_t>(cutscene077::screen3d)}) {
        Session s;
        s.mode = mode;
        s.flat(0, 0, false, false);
        uint64_t im = 0;
        for (int i = 0; i < 400; ++i)
            im += s.frame(true, gameRecord, true, true).immersive;
        check(im == 0, mode == cutscene077::flat ? "cutscenes = flat (his choice): never immersive"
                                                 : "cutscenes = 3d (his choice): never immersive");
    }

    std::printf(failures ? "CUT14 FAILED: %d\n" : "CUT14 PASSED\n", failures);
    return failures ? 1 : 0;
}
