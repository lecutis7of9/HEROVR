// HERO VR 0.0.77 CUTFIX (include/spidy/cutscene077_gate.hpp), pure.
#include "spidy/cutscene077.hpp"
#include "spidy/cutscene077_gate.hpp"
#include "spidy/presentation_gate.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string_view>

using namespace spidy;
using namespace spidy::cutscene077;

namespace {
int failures = 0;
void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++failures;
    }
}
} // namespace

int main() {
    // Camera motion: pose or lens, beyond the epsilon; non-finite never.
    float a[16]{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 355.3f, 223.4f, -1325.3f, 1};
    float b[16];
    std::memcpy(b, a, sizeof(a));
    float la[4]{-.315f, .315f, -.177f, .177f}, lb[4]{-.315f, .315f, -.177f, .177f};
    check(!cameraMoved(a, la, b, lb), "a still camera does not move");
    b[12] += .01f;
    check(cameraMoved(a, la, b, lb), "a moved position moves");
    std::memcpy(b, a, sizeof(a));
    lb[0] = -.3f;
    check(cameraMoved(a, la, b, lb), "an animated lens moves");
    lb[0] = la[0];
    b[3] = std::numeric_limits<float>::quiet_NaN();
    check(!cameraMoved(a, la, b, lb), "a NaN camera is no move");

    // The clock with the game-time hook: its own answer, as before.
    {
        MotionClock m;
        check(m.advancing(true, true, 0, true, 1000) && m.source() == clockGameTime, "hooked time advances");
        check(!m.advancing(true, false, 50, true, 2000), "hooked paused time does not, camera or not");
    }
    // Without it (SM2): the camera, after the screen settles.
    {
        MotionClock m;
        check(!m.advancing(false, false, 1, true, 1000) && m.source() == clockCameraMotion, "screen just up");
        check(!m.advancing(false, false, 2, true, 1000 + motionSettleMs - 1), "still settling");
        check(m.advancing(false, false, 3, true, 1000 + motionSettleMs), "moving after the settle");
        check(!m.advancing(false, false, 3, true, 1000 + motionSettleMs + 10), "a still frame does not advance");
        check(m.advancing(false, false, 4, true, 1000 + motionSettleMs + 20), "moving again");
        check(!m.advancing(false, false, 9, false, 5000), "no screen (gameplay): never");
        check(!m.advancing(false, false, 10, true, 5010), "the screen back: settles again");
        check(m.advancing(false, false, 11, true, 5010 + motionSettleMs), "then runs");
    }
    // The whole chain on the cut1 report's case: no hook, the screen up, the camera moving each frame -> the
    // scene clock runs and ImmersiveHold starts; a pause (still camera and the Menu button) -> the screen.
    {
        MotionClock m;
        SceneClock clock;
        ImmersiveHold hold;
        uint64_t moves = 0;
        bool started = false, shown = false;
        for (uint64_t t = 0; t <= 2000; t += 11) {
            const bool runs = clock.running(m.advancing(false, false, ++moves, true, 1 + t), 1 + t);
            shown = hold.update(true, runs, false, 1 + t);
            started = started || hold.started();
        }
        check(started && shown, "an in-engine scene goes immersive without the game-time hook");
        bool menu = false;
        for (uint64_t t = 2011; t <= 4000; t += 11)
            shown = hold.update(true, clock.running(m.advancing(false, false, moves, true, t), t), t == 2099, t);
        menu = hold.menu();
        check(!shown && menu, "a pause (camera still, Menu) is the screen");
        // Old behaviour, for the record: without the hook the clock never ran.
        SceneClock old;
        ImmersiveHold oldHold;
        bool oldShown = false;
        for (uint64_t t = 0; t <= 2000; t += 11)
            oldShown = oldShown || oldHold.update(true, old.running(false, t), false, t);
        check(!oldShown, "cut1: the hook's clock alone never started a scene on SM2");
    }
    // A still camera from the start (a pause menu, a hint card, a pre-rendered video): never immersive.
    {
        MotionClock m;
        SceneClock clock;
        ImmersiveHold hold;
        bool shown = false;
        for (uint64_t t = 0; t <= 5000; t += 11)
            shown = shown || hold.update(true, clock.running(m.advancing(false, false, 7, true, 1 + t), 1 + t), false,
                                         1 + t);
        check(!shown, "a still camera stays on the screen");
    }

    // Refusal reasons, per gate.
    {
        GateInput g;
        g.mode = immersive;
        g.screen = g.hero = g.bridge = g.head = g.clockRuns = g.depthOn = true;
        check(refusals(g) == 0, "nothing in the way");
        g.clockRuns = false;
        check(refusals(g) == refuseClock, "hooked clock stopped");
        g.cameraClock = true;
        check(refusals(g) == refuseStill, "camera still (pause, hint, video)");
        g.clockRuns = true;
        g.cameraClock = false;
        g.hero = false;
        g.bridge = false;
        check(refusals(g) == (refuseNoPlayer | refuseNoBridge), "no player, no bridge");
        g.hero = g.bridge = true;
        g.gameplay = g.viewScreen = true;
        check(refusals(g) == (refuseGameplay | refuseViewScreen), "gameplay, VIEW: GAME SCREEN");
        g.gameplay = g.viewScreen = false;
        g.head = false;
        g.eyesLost = g.fault = g.menu = true;
        check(refusals(g) == (refuseNoHead | refuseEyesLost | refuseFault | refuseMenu), "head, eyes, fault, menu");
        GateInput d;
        d.mode = screen3d;
        d.screen = d.hero = d.bridge = d.clockRuns = true;
        check(refusals(d) == refuseDepthOff, "3d with depth off");
        d.head = false;
        d.depthOn = true;
        check(refusals(d) == 0, "3d needs no head pose");
    }
    {
        RefusalLog log;
        char text[200];
        log.reasons(text, sizeof(text));
        check(std::string_view(text) == "none", "an empty log says none");
        log.add(0);
        log.add(refuseStill);
        log.add(refuseStill | refuseNoPlayer);
        log.reasons(text, sizeof(text));
        check(log.frames() == 3 && log.refused() == 2 && log.count(8) == 2 && log.count(1) == 1, "counted");
        check(std::string_view(text) == "no_player 1, camera_still 2", text);
        char tiny[12];
        log.reasons(tiny, sizeof(tiny));
        check(std::strlen(tiny) < sizeof(tiny), "a short buffer stays terminated");
        check(std::string_view(refusalName(99)) == "unknown", "unknown name");
    }
    check(std::string_view(videoNote).find("flat screen") != std::string_view::npos, "the video note");

    // CUTFIX2: the headset's one-line note.
    {
        char line[160];
        noteLine(immersive, refuseStill, line, sizeof(line));
        check(std::string_view(line) == "IMMERSIVE NOT SHOWN: CAMERA STILL (PAUSE, CARD OR VIDEO)", line);
        noteLine(screen3d, refuseNoPlayer | refuseStill | refuseMenu, line, sizeof(line));
        check(std::string_view(line) == "3D NOT SHOWN: NO PLAYER +2", line);
        noteLine(immersive, 0, line, sizeof(line));
        check(line[0] == 0, "no refusal, no note");
        noteLine(flat, refuseStill, line, sizeof(line));
        check(line[0] == 0, "flat, no note");
        for (unsigned b = 0; b < refusalCount; ++b)
            for (const char* c = refusalLabel(b); *c; ++c)
                check(*c != '_', "labels drawable (no underscore)");
        RefusalNote n;
        check(!n.update(true, refuseStill, 1000), "not at once");
        check(!n.update(true, refuseStill, 1000 + noteSettleMs - 1), "settling");
        check(n.update(true, refuseStill, 1000 + noteSettleMs), "shown after the settle");
        check(!n.update(true, 0, 2000), "gone when the scene shows");
        check(!n.update(true, refuseStill, 2001), "settles again");
        check(!n.update(false, refuseStill, 5000), "gone with the screen");
    }
    // CUTFIX2: a shown scene rides a short gap in the player record; nothing starts without one.
    {
        HeroGrace g;
        check(!g.present(false, false, 100), "no player, nothing shown");
        check(g.present(true, false, 200), "a player");
        check(g.present(false, true, 200 + heroGraceMs), "a shown scene, a gap within the grace");
        check(!g.present(false, true, 201 + heroGraceMs), "past the grace");
        check(!g.present(false, false, 300 + heroGraceMs), "not shown: no grace");
        check(g.present(true, true, 9000) && g.present(false, true, 9500), "again");
        check(!g.present(false, false, 9600), "the scene ended");
        check(!g.present(false, true, 9700), "no grace once it ended");
    }
    if (failures)
        return 1;
    std::printf("cutscene077_gate: all passed\n");
    return 0;
}
