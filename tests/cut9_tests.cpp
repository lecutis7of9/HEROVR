// HERO VR 0.0.78 CUT9 tests.
// 1. TITLE FLAT: report 025037 (cut8, console 01M6JRZ69XVK2SH9KPHDNN03AA.log) replayed through the real
//    cutscene077::MenuScreen the mod runs each frame. The log: boot frames with the game's screen down while no hero
//    was played (no VR gameplay yet), then MENU SCREEN #1 "no player record" (line 54) instead of the front end, and
//    the title's record then went immersive ("a 7.8 s screen stretch, IMMERSIVE shown", line 67) until the menu
//    button (MENU SCREEN #2, the pause, line 104). Fixed: the title and its menus stay the normal VR screen from the
//    first frame with no press; a game loaded from the title (a gap with no record >= 1.5 s, then a new record) and
//    VR gameplay still end the front end, so in-game scenes stay immersive.
// 2. GAME VIEW LIFT (shared_lod::gameViewLift): the beacon fix's multiplier.
#include "spidy/cutscene077.hpp"
#include "spidy/shared_lod.hpp"
#include "spidy/hero007.hpp"
#include "spidy/hero072_play.hpp"
#include "spidy/sfx.hpp"
#include "spidy/story077b.hpp"

#include <cmath>
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

// Feeds the report's opening: boot frames (screen down, `bootPlaying` as the mod judged it), the boot screen with no
// record, then the title's record for 7.8 s. Returns whether every title frame stayed on the VR screen.
bool replayTitle(cutscene077::MenuScreen& m, bool bootPlaying, uint64_t& ms, int& whyFirst, uint64_t& countAtTitle) {
    for (int i = 0; i < 120; ++i, ms += 11) // boot: the screen down, the headset not viewing / no hero played
        m.update(false, 0, false, ms, bootPlaying);
    bool allOn = true;
    for (int i = 0; i < 400; ++i, ms += 11) // BOOT SCREEN at 2 s / 4 s: no record
        allOn &= m.update(true, 0, false, ms);
    whyFirst = m.why();
    for (int i = 0; i < 700; ++i, ms += 11) // the title (its record), the 7.8 s stretch of line 67
        allOn &= m.update(true, title, false, ms);
    countAtTitle = m.count();
    return allOn;
}
} // namespace

int main() {
    // --- the fix: boot frames are not VR gameplay (playing false) ---
    {
        cutscene077::MenuScreen m;
        uint64_t ms = 1000, count{};
        int why{};
        const bool on = replayTitle(m, false, ms, why, count);
        check(why == 1, "report 025037: MENU SCREEN #1 is the front end (boot, title, its menus), not 'no player record'");
        check(on, "report 025037: every boot and title frame stays the normal VR screen (flat) with no button press");
        check(count == 1, "report 025037: one screen stretch from boot through the title (no immersive gap)");
        check(m.frontEnd(), "report 025037: still the front end on the title");
        // the title's menus: the record flickers off and back (its submenus)
        bool menus = true;
        for (int i = 0; i < 50; ++i, ms += 11)
            menus &= m.update(true, 0, false, ms);
        for (int i = 0; i < 100; ++i, ms += 11)
            menus &= m.update(true, title, false, ms);
        check(menus && m.frontEnd(), "title menus (a short record gap, the same record back): still flat");
        // Continue / New Game: the load (no record >= 1.5 s), then the game's record -> the front end ends
        for (int i = 0; i < 200; ++i, ms += 11)
            m.update(true, 0, false, ms);
        bool loadOn = true;
        for (int i = 0; i < 3; ++i, ms += 11)
            loadOn &= m.update(true, gameRecord, false, ms);
        check(!m.frontEnd() && !loadOn, "a game loaded from the title: the front end ends, its opening scene may be immersive");
        // VR gameplay, then an in-game scene: immersive (not the screen)
        for (int i = 0; i < 300; ++i, ms += 11)
            m.update(false, gameRecord, false, ms, true);
        bool sceneOn = false;
        for (int i = 0; i < 300; ++i, ms += 11)
            sceneOn |= m.update(true, gameRecord, false, ms);
        check(!sceneOn, "an in-game scene after VR gameplay: not forced to the screen (stays immersive)");
        // the pause: the screen (as before)
        const bool paused = m.update(true, gameRecord, true, ms += 11);
        check(paused && m.why() == 2, "the menu button in a scene: the pause menu on the VR screen");
    }
    // --- the old behaviour (every screen-down frame ended the front end) fails the same replay ---
    {
        cutscene077::MenuScreen m;
        uint64_t ms = 1000, count{};
        int why{};
        const bool on = replayTitle(m, true, ms, why, count);
        check(why == 3 && !on && !m.frontEnd(),
              "the cut8 behaviour reproduces report 025037: MENU #1 'no player record', the title's record goes immersive");
    }
    // --- headset not on at boot, then VR gameplay before any title: the front end ends only with play ---
    {
        cutscene077::MenuScreen m;
        uint64_t ms = 5000;
        for (int i = 0; i < 50; ++i, ms += 11)
            m.update(false, gameRecord, false, ms, true);
        check(!m.frontEnd(), "VR gameplay still ends the front end (a save loaded straight into play)");
    }
    // --- GAME VIEW LIFT ---
    {
        using namespace shared_lod;
        // report 025037's window probe: eyes 98 deg, the game view widened to 135 deg -> ~2.9x lower scale
        const float g = .35f, e0 = 1.0f, e1 = 1.02f;
        const float lift = gameViewLift(higher, g, e0, e1);
        check(std::fabs(lift - e0 / g) < 1e-4f, "HIGHER: the game view lifted to the lower eye's LOD scale");
        check(gameViewLift(higher, .1f, 1.f, 1.f) == maxMultiplier, "HIGHER: the lift capped at maxMultiplier");
        check(gameViewLift(higher, 1.f, 1.f, 1.005f) == 1.f, "HIGHER: no lift when the game view already matches the eyes");
        check(gameViewLift(higher, 1.2f, 1.f, 1.f) == 1.f, "HIGHER: never lowered");
        check(gameViewLift(off, g, e0, e1) == 1.f && gameViewLift(shared_lod::game, g, e0, e1) == 1.f,
              "OFF / GAME VIEW: the game view left at 1.0");
        check(gameViewLift(higher, NAN, e0, e1) == 1.f && gameViewLift(higher, g, NAN, e1) == 1.f &&
                  gameViewLift(higher, 0.f, e0, e1) == 1.f && gameViewLift(higher, g, e0, INFINITY) == 1.f,
              "insane scales (NaN, 0, inf, unread eye): 1.0");
    }
    // --- NO GHOST SCREEN (cut9 D) ---
    {
        using cutscene077::waitShowsScreen;
        check(!waitShowsScreen(10000, 9000, 0), "a pair 1.0 s ago: the missing pair is a frame with no layer, never the screen");
        check(!waitShowsScreen(10000, 0, 9990), "no pair yet, waiting 10 ms: no layer");
        check(waitShowsScreen(12000, 0, 10000), "no pair for 2 s: the screen comes back (VR never black, the fault path)");
        check(!waitShowsScreen(0, 0, 0), "no times at all: no layer");
        check(!waitShowsScreen(10000, 10500, 0), "a clock that ran back: no layer");
        cutscene077::QteCard card;
        check(!card.update(true, 1000) && !card.update(true, 1200), "a freeze under 250 ms: no card");
        check(card.update(true, 1250), "the QTE's freeze held 250 ms: the card");
        check(!card.update(false, 1260) && !card.update(true, 1300), "the freeze gone: the card off and timed again");
    }
    { // 0.0.78 CLAWS: stab or slash, and the claw blood
        check(hero007::clawIsStab({0, 0, -.05f}, {0, 0, -.28f}), "the tip driven along the blades: a stab");
        check(!hero007::clawIsStab({.05f, 0, 0}, {0, 0, -.28f}), "the tip across the blades: a slash");
        check(!hero007::clawIsStab({0, 0, .05f}, {0, 0, -.28f}), "pulled back: a slash, never a stab");
        check(!hero007::clawIsStab({}, {0, 0, -.28f}), "no move read: a slash");
        hero072::Blood b;
        b.claw(hero072::bloodExtreme, {0, 1.2f, -1}, {0, 0, -1}, 0.f);
        check(b.liveDrops() == hero072::clawDropsPerHit[0] && b.livePools() == hero072::clawPoolsPerHit[0],
              "an EXTREME claw hit: 160 drops and 5 pools");
        hero072::Blood off;
        off.claw(hero072::bloodOff, {0, 1.2f, -1}, {0, 0, -1}, 0.f);
        check(off.liveDrops() == 0 && off.livePools() == 0, "RAGE BLOOD off: no claw blood");
        for (int i = 0; i < 20; ++i)
            b.claw(hero072::bloodExtreme, {0, 1.2f, -1}, {1, 0, 0}, 0.f);
        check(b.liveDrops() <= hero072::bloodDropsMost && b.livePools() <= hero072::bloodPoolsMost, "the claw blood is capped");
        check(sfx::levelGain(sfx::pistolShot) < .2f && sfx::levelGain(sfx::clawStab) > 1.f && sfx::levelGain(sfx::vanStart) == 1.f,
              "the levels: the loud pistol down, the quiet stab up, the van as it was");
    }
    { // 0.0.78 STORY RUN: the game's own RT prompt turns MJ's run on; a web (Spider-Man swinging) turns it off
        story077b::StoryRun r;
        check(!r.update(true, false, false), "no RT prompt: off (Spider-Man's controls as they are)");
        check(r.update(true, true, false) && r.update(true, false, false), "the game asked for RT: on for the segment");
        check(r.update(false, false, false), "a QTE / scene in between: still on");
        check(!r.update(true, false, true) && r.ends == 1, "a web on the player: off");
        check(!r.update(false, true, false), "a prompt off gameplay: not started");
    }
    std::printf("%s (%d failures)\n", failures ? "FAILED" : "ALL OK", failures);
    return failures ? 1 : 0;
}
