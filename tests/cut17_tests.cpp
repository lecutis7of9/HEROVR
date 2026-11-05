// HERO VR 0.0.78 cut17 tests, from report 212843 (cut16 5f8429b, session 212843):
//  TITLE BLIP: the boot (record 0, ~10 s), the screen down while the title's record 0x26978ed3080 arrives, the title,
//  the VR loop stall and a short VR play on the title's own record (presentation none twice, 0.4 s each with the
//  250 ms screen delay), then the title's moving camera for 25 s. The menu must stay the flat VR screen on every frame
//  (cut16 went immersive from 11 s to the pause press at 37 s). A record read for 3 frames at boot must not be taken for the
//  title's. Real gameplay and a game loaded from the title still end the front end; the pause menu stays flat.
//  PHASE GATE: throws 13-17 and Y #4/#5 of the same session (233.3/250, then 1/250 - the Y landed).
#include "spidy/cut16.hpp"
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
constexpr uint64_t title = 0x26978ed3080ull, blink = 0x26978ed0000ull, gameRecord = 0x26978ee1111ull;

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
    void blip(int n) {
        play(n, title);
        for (int i = 0; i < 23; ++i)
            frame(false, title, true, true);
    }
};
} // namespace

int main() {
    // --- TITLE BLIP: report 212843 ---
    {
        Session s;
        bool ok = s.boot(false);
        s.blip(15); // ~165 ms of VR play (the stall's follow commits)
        ok &= s.flat(500, title, true, true);
        s.blip(15); // the second 'none' stretch
        ok &= s.flat(2300, title, true, true); // the title's camera until 37 s
        check(ok && s.immersiveFrames == 0, "212843: two VR play blips on the title's record - the title stays flat");
        check(s.menu.frontEnd() && s.menu.blips() == 2, "212843: the front end kept, two TITLE BLIP lines");
        // the menu button on the title (the saved games): still flat
        check(s.flat(1, title, true, true, true) && s.flat(400, title, true, true), "212843: title menus flat");
        // he loads a game: VR play for real - the front end ends, and a scene after it may be immersive
        s.play(400, title);
        check(!s.menu.frontEnd(), "212843: 4.4 s of VR play ends the front end");
        uint64_t im = 0;
        for (int i = 0; i < 400; ++i)
            im += s.frame(true, title, true, true).immersive;
        check(im > 0, "212843: a cutscene after real gameplay is immersive");
        // the pause menu in gameplay: flat
        s.play(100, title);
        check(s.flat(1, title, true, false, true) && s.flat(300, title, true, true), "212843: the pause menu flat");
    }
    // a blip just under frontEndPlayMs: kept; a transient boot record: the title still flat
    {
        Session s;
        bool ok = s.boot(true);
        s.blip(170); // 1.87 s
        ok &= s.flat(1500, title, true, true);
        check(ok && s.immersiveFrames == 0 && s.menu.frontEnd(),
              "a 3-frame boot record and a 1.9 s VR play on the title: the title stays flat");
    }
    // a game loaded from the title (a new record after a gap, the screen up): the front end ends as before
    {
        Session s;
        bool ok = s.boot(false);
        ok &= s.flat(300, 0, false, false); // the load: no record 3.3 s
        s.frame(true, gameRecord, true, false);
        check(ok && !s.menu.frontEnd(), "a game loaded from the title: the front end ends at once, as before");
    }

    // --- PHASE GATE: throws 13-17 and Y #4/#5 of report 212843 ---
    {
        cut16::BossGate g;
        float hp = 245.5f;
        const float most = 250.f;
        auto frames = [&](float to, int n) {
            bool started = false;
            for (int i = 0; i < n; ++i) {
                hp = to;
                g.hp(true, hp);
                started |= g.phase(true, hp, most, true);
            }
            return started;
        };
        check(!frames(245.5f, 50) && !g.holding(), "245.5/250: no gate");
        check(!g.thrown(true, 245.5f, 241.f, most) && !frames(241.f, 50), "throw 13 hit: 241, no gate");
        check(!g.thrown(true, 241.f, 236.5f, most) && !frames(236.5f, 50), "throw 15 hit: 236.5, no gate");
        check(!g.thrown(true, 236.5f, 233.3f, most), "throw 17 (a hit) is no throw gate");
        check(frames(233.3f, 90) && g.holding() && g.phaseHolding() && g.gates == 1,
              "233.3/250 (most x 14/15): the phase gate holds, the Y card shows, once");
        cut16::BossGate::YReport out;
        g.yPressed(true, hp, 120.f, false, out);
        check(g.y.gated, "Y #5 at 233.3: gate held (cut16 logged 'gate not held')");
        frames(1.f, 30); // 233.3 -> 1.0: the Y landed
        check(!g.holding(), "the Y landed (1/250): the gate ends, the card goes");
        check(!frames(1.f, 30) && g.gates == 1, "no second gate at 1/250");
        // not the Sandman, or no HP read: never
        cut16::BossGate o;
        check(!o.phase(true, 233.3f, 250.f, false) && !o.phase(false, 233.3f, 250.f, true) && !o.holding(),
              "another boss or no HP read: no phase gate");
        // the throw gate (public 0.0.77) is unchanged: two no-hit full throws at a non-phase HP still hold
        cut16::BossGate t;
        check(!t.thrown(true, 240.f, 240.f, most) && t.thrown(true, 240.f, 240.f, most) && t.holding(),
              "the throw gate unchanged");
        g.lost();
        check(!g.holding(), "boss lost: no gate");
    }

    std::printf(failures ? "CUT17 FAILED: %d\n" : "CUT17 PASSED\n", failures);
    return failures ? 1 : 0;
}
