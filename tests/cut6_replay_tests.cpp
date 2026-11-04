// HERO VR 0.0.77 CUT6 REPLAY: the owner's cut5 session (report game-vr-20261103-234713.json, extracted into
// tests/data/cut6_234713_samples.txt) fed sample by sample through the real decision code the mod runs each frame:
// swing077::StartGate (the native swing's start), cutscene077::MenuScreen (the menu on the VR screen),
// game_pad::mappingFor (which pad path the controllers take), swing077::standing + game_pad::ChargeJump (the
// charged jump). The game's answers are modelled from the log: SpidySwingStart refuses 1002 while the mover is not
// live (the record gone within 0.3 s, as at sample 402 -> 403, console line 119), and starts otherwise.
#include "spidy/cutscene077.hpp"
#include "spidy/game_pad.hpp"
#include "spidy/swing077.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

using namespace spidy;

namespace {
int failures = 0;
void check(bool ok, const char* what) {
    std::printf("%s: %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok)
        ++failures;
}
struct Sample {
    int index;
    double seconds;
    int presentation; // 0 none, 1 game_screen, 2 immersive (VR play), 3 cutscene_immersive
    uint64_t record;
    bool menu;
    long tracked;
};
std::vector<Sample> load(const char* path) {
    std::vector<Sample> v;
    FILE* f = std::fopen(path, "r");
    if (!f)
        return v;
    char line[256];
    while (std::fgets(line, sizeof(line), f)) {
        if (line[0] == '#')
            continue;
        Sample s{};
        char rec[32]{};
        int menu{};
        if (std::sscanf(line, "%d %lf %d %31s %d %ld", &s.index, &s.seconds, &s.presentation, rec, &menu,
                        &s.tracked) == 6) {
            s.record = std::strtoull(rec, nullptr, 16);
            s.menu = menu != 0;
            v.push_back(s);
        }
    }
    std::fclose(f);
    return v;
}
// The game's answer to SpidySwingStart at sample i: 1002 while the mover is not live (the record is gone within
// 0.3 s - the title's record at sample 402), else 0.
uint32_t startAnswer(const std::vector<Sample>& v, size_t i) {
    for (size_t j = i; j < v.size() && v[j].seconds - v[i].seconds <= .3; ++j)
        if (v[j].record != v[i].record)
            return 1002;
    return 0;
}
struct Run {
    uint64_t startedOn[8]{};
    int starts{}, refusals{};
    bool offForSession{};
    bool swingAtLedge{}, swingAfterScene{};
    double ledgeStart{-1}, afterSceneStart{-1};
};
constexpr uint64_t titleRecord = 0x21151c92b40, ledgeRecord = 0x2115163b300, afterSceneRecord = 0x211523b4700;

// cut5 / 0.0.76: the first VR-play frame starts it and any refusal is off for the session.
Run replayOld(const std::vector<Sample>& v) {
    Run r;
    bool started{}, failed{};
    for (size_t i = 0; i < v.size(); ++i) {
        const bool ready = v[i].presentation == 2 && v[i].record;
        if (ready && !started && !failed) {
            if (startAnswer(v, i)) {
                failed = true;
                ++r.refusals;
            } else {
                started = true;
                ++r.starts;
            }
        }
        if (started && v[i].presentation == 2) {
            r.swingAtLedge |= v[i].record == ledgeRecord;
            r.swingAfterScene |= v[i].record == afterSceneRecord && v[i].seconds >= 96.1;
        }
    }
    r.offForSession = failed;
    return r;
}
// cut6: the real gate; retargetRefused: every retarget of a running swing to a new record is refused (the worst
// case: the follow stops it and the gate starts it again).
Run replayNew(const std::vector<Sample>& v, bool retargetRefused) {
    Run r;
    swing077::StartGate gate;
    bool swingStarted{}, swingFailed{};
    uint64_t swingRecord{};
    uint64_t attached{};
    for (size_t i = 0; i < v.size(); ++i) {
        const Sample& s = v[i];
        // attach(): another player record (the follow lambda): a running swing is retargeted.
        if (s.record != attached) {
            attached = s.record;
            if (swingStarted && s.record) {
                const uint32_t code = retargetRefused ? 1002u : startAnswer(v, i);
                if (code) {
                    swingStarted = false; // stopSwing
                    gate.restart();
                } else {
                    swingRecord = s.record;
                }
            }
        }
        const bool ready = s.presentation == 2; // raysStarted && motion.active && !flatScreen && controlsVisible
        if (!swingStarted && !swingFailed && gate.update(s.seconds, ready, s.record, s.record /* mover */)) {
            const uint32_t code = startAnswer(v, i);
            const bool off = gate.result(s.seconds, code);
            if (off)
                swingFailed = true;
            else if (gate.started()) {
                swingStarted = true;
                swingRecord = s.record;
                if (r.starts < 8)
                    r.startedOn[r.starts] = s.record;
                ++r.starts;
            } else
                ++r.refusals;
        }
        if (swingStarted && ready && swingRecord == s.record) {
            if (s.record == ledgeRecord && !r.swingAtLedge) {
                r.swingAtLedge = true;
                r.ledgeStart = s.seconds;
            }
            if (s.record == afterSceneRecord && s.seconds >= 91.0 && !r.swingAfterScene) {
                r.swingAfterScene = true;
                r.afterSceneStart = s.seconds;
            }
        }
    }
    r.offForSession = swingFailed;
    return r;
}
} // namespace

int main(int argc, char** argv) {
    const char* path = argc > 1 ? argv[1] : "tests/data/cut6_234713_samples.txt";
    const auto v = load(path);
    check(v.size() == 2095, "the 234713 fixture: 2095 samples");
    if (v.size() != 2095)
        return 1;
    check(v[402].presentation == 2 && v[402].record == titleRecord && v[403].record == 0,
          "sample 402: one VR-play frame on the title's record, gone at 403");

    // 1. The cut5 / 0.0.76 start reproduces the owner's session: refused 1002 at sample 402, off for good.
    const Run old = replayOld(v);
    check(old.offForSession && old.refusals == 1 && old.starts == 0, "cut5 logic: 1002 on the title frame, swing off for the session (reproduced)");
    check(!old.swingAtLedge && !old.swingAfterScene, "cut5 logic: no swing on the ledge save nor after the scene (reproduced)");

    // 2. Acceptance: the cut6 gate starts the swing on the ledge save's record and after the immersive scene.
    for (int worst = 0; worst < 2; ++worst) {
        const Run r = replayNew(v, worst != 0);
        std::printf("      cut6 (%s): starts %d, refusals %d, ledge swing from %.1f s, after-scene swing from %.1f s\n",
                    worst ? "every retarget refused" : "retargets as the game answers", r.starts, r.refusals,
                    r.ledgeStart, r.afterSceneStart);
        check(!r.offForSession, "cut6: never off for the session on a 'not now' refusal");
        check(r.swingAtLedge && r.ledgeStart < 31.0,
              "ACCEPTANCE (2) ledge save: the native swing (and game_warp / game_offers with it) runs on record 2115163b300 within 1 s of VR play");
        check(r.swingAfterScene && r.afterSceneStart < 92.0,
              "ACCEPTANCE (1) after the immersive scene: the native swing runs on record 211523b4700 (RT web swing in the air)");
        bool titleStart{};
        for (int k = 0; k < r.starts && k < 8; ++k)
            titleStart |= r.startedOn[k] == titleRecord;
        check(!titleStart, "cut6: never started on the title's one-frame record");
    }

    // 3. MenuScreen: never on in VR gameplay, and no stick input exists for it.
    {
        cutscene077::MenuScreen ms;
        bool onInPlay{}, onInLedge{}, onAfterScene{};
        int count{};
        for (const auto& s : v) {
            const bool screen = s.presentation != 2; // gameScreen: the game's screen up (not VR play)
            const bool on = ms.update(screen, s.record, s.menu);
            onInPlay |= on && s.presentation == 2;
            onInLedge |= on && s.record == ledgeRecord && s.presentation == 2;
            onAfterScene |= on && s.record == afterSceneRecord && s.seconds >= 96.1;
            count = static_cast<int>(ms.count());
        }
        std::printf("      MenuScreen stretches: %d\n", count);
        check(!onInPlay && !onInLedge && !onAfterScene, "MenuScreen never on in VR gameplay (ledge or after the scene)");
    }

    // 4. The pad path: every VR-play sample takes the gameplay mapping (the swing, walk and jump), never menus.
    {
        bool headSeen{}, allGameplay{true};
        int playSamples{};
        for (const auto& s : v) {
            headSeen = headSeen || s.tracked > 0;
            const bool screen = s.presentation != 2;
            const auto m = game_pad::mappingFor(s.presentation != 0, headSeen, screen, false);
            if (s.presentation == 2 && s.seconds > 30) {
                ++playSamples;
                allGameplay = allGameplay && m == game_pad::Mapping::gameplay;
            }
        }
        std::printf("      VR-play samples checked: %d\n", playSamples);
        check(playSamples > 900 && allGameplay, "gameplay pad path chosen in every VR-play sample (ledge and after the scene)");
    }

    // 5. The charged jump: with the swing running its ground is real (report 20261101-164301 swing_samples
    // grounded 1, 213206 "grounded 1"): standing resolves and a held A charges; without the swing, a plain jump.
    {
        swing077::Footing on{true, true, 1, true, false, false, false, false, false, false, false};
        check(swing077::standing(on), "swing running, grounded: standing");
        game_pad::ChargeJump cj;
        bool charged{};
        for (int k = 0; k < 60; ++k)
            cj.update(1.f / 90, true, swing077::standing(on));
        charged = cj.charges() > 0;
        check(charged, "held A while standing: the charge begins");
        swing077::Footing off = on;
        off.swingStatus = 0;
        off.grounded = false;
        check(!swing077::standing(off), "no swing: no charge (0.0.76's plain jump)");
        game_pad::ChargeJump plain;
        const auto k = plain.update(1.f / 90, true, swing077::standing(off));
        check(k.jump && k.leap == 0, "no swing: A is the game's jump at once");
    }

    // 6. The gate itself.
    {
        swing077::StartGate g;
        check(!g.update(0, true, 1, 1) && !g.update(.4, true, 1, 1) && g.update(.5, true, 1, 1), "settles 0.5 s");
        check(!g.result(.5, 1002) && !g.failed(), "1002: not now");
        check(!g.update(1.0, true, 1, 1) && g.update(1.5, true, 1, 1), "again 1 s later");
        check(!g.update(1.6, true, 2, 2) && g.update(2.1, true, 2, 2), "another record: its own 0.5 s, no wait");
        check(!g.result(2.1, 0) && g.started() && !g.update(3, true, 2, 2), "started");
        swing077::StartGate h;
        h.update(0, true, 1, 1);
        h.update(.6, true, 1, 1);
        check(h.result(.6, 1003) && h.failed() && !h.update(5, true, 1, 1), "1003 (bytes): off for the session");
        swing077::StartGate k;
        k.update(0, true, 1, 1);
        k.update(.6, true, 1, 1);
        k.result(.6, 0);
        check(!k.stopped(1004) && !k.started() && !k.failed(), "a transient stop restarts");
        check(k.stopped(2002) && k.failed(), "the bridge's own stop: off");
    }
    if (failures) {
        std::printf("%d failure(s)\n", failures);
        return 1;
    }
    std::printf("cut6 replay: all passed\n");
    return 0;
}
