// Update 38.2 REPLAY TESTS: each 38.2 fix replayed on the real recorded sessions
// (tests/replay/*.csv, tools/replay_extract.py from run36, run371, run372, run38, run381, run382,
// and Update 38.5's run384: the user's 38.3 session of October 24).
// The recorded ~19 Hz samples are resampled to 90 Hz (positions linear, orientations nlerp;
// a gap over .15 s is untracked) and fed to the same classes the DLL runs (wrist_hud::Toggle,
// combat_input::Machine, combat38::RingHold / drawnWrist / grabTarget / GrabWeb). Each test
// prints BEFORE (38.1's setting, and the recorded number where the session has it) and AFTER
// (38.2's default) and fails the gate when AFTER misses its bar.
// Not recorded (synthesised, said so in each line): the triggers (a pull at each `presses`
// step of the pointing hand, the right), the grip, and the enemies (none in run381's log).
#include "spidy/auto_perf.hpp"
#include "spidy/combat38.hpp"
#include "spidy/rope_grab.hpp"
#include "spidy/combat_input.hpp"
#include "spidy/gadget_wheel.hpp"
#include "spidy/game_pad.hpp"
#include "spidy/soft_shapes.hpp"
#include "spidy/tech_marks.hpp"
#include "spidy/web_visual.hpp"
#include "spidy/wrist_hud.hpp"
#include "spidy/wrist_muzzle.hpp"
#include "spidy/tracking.hpp"

#include <algorithm>
#include <cstring>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdarg>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#ifndef SPIDY_REPLAY_DIR
#define SPIDY_REPLAY_DIR "tests/replay"
#endif

using namespace spidy;

namespace {
int failures = 0;
void check(bool ok, const char* what) {
    std::printf("  %s %s\n", ok ? "PASS" : "FAIL", what);
    failures += !ok;
}

struct Row {
    double t{};
    Pose head{}, hand[2]{};
    bool headOk{}, handOk[2]{};
    uint32_t pad{}, presses{}, toggles{};
    float dist{-1}, el{-1}, er{-1};
    bool play{};
    double sub{}, uniq{};
};
bool finiteRow(const Pose& p) { return finite(p.position) && std::isfinite(p.orientation.w); }

std::vector<Row> load(const std::string& name) {
    std::vector<Row> rows;
    std::ifstream in(std::string(SPIDY_REPLAY_DIR) + "/" + name + ".csv");
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#')
            continue;
        std::vector<double> v;
        std::stringstream ss(line);
        std::string cell;
        while (std::getline(ss, cell, ','))
            v.push_back(cell == "nan" ? NAN : std::strtod(cell.c_str(), nullptr));
        if (v.size() < 32)
            continue;
        Row r;
        r.t = v[0];
        const auto pose = [&](size_t i) {
            return Pose{{float(v[i]), float(v[i + 1]), float(v[i + 2])},
                        {float(v[i + 3]), float(v[i + 4]), float(v[i + 5]), float(v[i + 6])}};
        };
        r.head = pose(1);
        // The sample's hands: the grip position with the aim orientation; the grip's own
        // orientation back from it (aim = grip * gripToAim, run381's fit).
        for (unsigned h = 0; h < 2; ++h) {
            Pose p = pose(8 + 7 * h);
            p.orientation = p.orientation * combat38::gripToAim().conjugate();
            r.hand[h] = p;
        }
        r.headOk = finiteRow(r.head) && length(r.head.position) > 1e-6f;
        for (unsigned h = 0; h < 2; ++h)
            r.handOk[h] = finiteRow(r.hand[h]) && length(r.hand[h].position) > 1e-6f;
        r.pad = uint32_t(v[22]);
        r.presses = uint32_t(v[23]);
        r.toggles = uint32_t(v[24]);
        r.dist = float(v[25]);
        r.play = v[26] > .5;
        r.el = float(v[27]);
        r.er = float(v[28]);
        r.sub = v[30];
        r.uniq = v[31];
        rows.push_back(r);
    }
    return rows;
}

Quat nlerp(Quat a, Quat b, float s) {
    if (a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w < 0)
        b = Quat{-b.x, -b.y, -b.z, -b.w};
    Quat q{a.x + (b.x - a.x) * s, a.y + (b.y - a.y) * s, a.z + (b.z - a.z) * s, a.w + (b.w - a.w) * s};
    const float n = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
    return n > 1e-6f ? Quat{q.x / n, q.y / n, q.z / n, q.w / n} : a;
}
Pose lerp(const Pose& a, const Pose& b, float s) {
    return Pose{a.position + (b.position - a.position) * s, nlerp(a.orientation, b.orientation, s)};
}

// One 90 Hz frame between two samples.
struct Tick {
    double t{};
    float dt{};
    size_t row{};          // the sample at or before t
    Pose head{}, hand[2]{};
    bool headOk{}, handOk[2]{}, play{};
};
template <typename F>
void replay(const std::vector<Row>& rows, F&& each, double from = -1e18, double to = 1e18) {
    constexpr double step = 1.0 / 90.0;
    if (rows.size() < 2)
        return;
    double t = std::max(rows.front().t, from);
    size_t i = 0;
    while (i + 1 < rows.size() && rows[i + 1].t <= t)
        ++i;
    const double end = std::min(rows.back().t, to);
    for (; t <= end; t += step) {
        while (i + 1 < rows.size() && rows[i + 1].t <= t)
            ++i;
        const Row& a = rows[i];
        const Row& b = i + 1 < rows.size() ? rows[i + 1] : rows[i];
        const double gap = b.t - a.t;
        const float s = gap > 1e-6 ? float(std::clamp((t - a.t) / gap, 0.0, 1.0)) : 0.f;
        const bool joined = gap < .15;
        Tick k;
        k.t = t;
        k.dt = float(step);
        k.row = i;
        k.play = a.play;
        k.headOk = a.headOk && (!joined || b.headOk);
        k.head = joined && b.headOk ? lerp(a.head, b.head, s) : a.head;
        for (unsigned h = 0; h < 2; ++h) {
            k.handOk[h] = a.handOk[h] && joined && b.handOk[h];
            k.hand[h] = k.handOk[h] ? lerp(a.hand[h], b.hand[h], s) : a.hand[h];
        }
        each(k);
    }
}

// The pointing hand's pulls (the right's `presses` steps): the trigger down from just before
// the sample that counted it, for `hold` seconds.
std::vector<double> pressTimes(const std::vector<Row>& rows) {
    std::vector<double> p;
    for (size_t i = 1; i < rows.size(); ++i)
        if (rows[i].presses > rows[i - 1].presses)
            p.push_back(rows[i].t - .03);
    return p;
}
float triggerAt(const std::vector<double>& presses, double t, double hold) {
    const auto it = std::upper_bound(presses.begin(), presses.end(), t);
    if (it == presses.begin())
        return 0.f;
    return t - *(it - 1) < hold ? 1.f : 0.f;
}

const char* runs[] = {"run384", "run382", "run381", "run38", "run372", "run371", "run36"};

// ---- 1. HOLO: a forearm tap opens it -----------------------------------------------------------
void holo() {
    std::printf("\n[1] HOLO OPEN: the pointing hand's pull with its fingertip at the hologram forearm (<= %.2f m)\n",
                wrist_hud::Toggle::forearmReach);
    uint32_t allNear = 0, allOpened = 0;
    for (const char* run : runs) {
        const auto rows = load(run);
        const auto presses = pressTimes(rows);
        // Recorded: the presses in reach and the toggles that came of them.
        uint32_t nearRec = 0, toggledRec = 0;
        for (size_t i = 1; i < rows.size(); ++i)
            if (rows[i].presses > rows[i - 1].presses && rows[i].play && rows[i].dist >= 0 &&
                rows[i].dist <= wrist_hud::Toggle::forearmReach) {
                ++nearRec;
                toggledRec += rows[i].toggles > rows[i - 1].toggles;
            }
        uint32_t toggles[2]{}, clicksNear[2]{}, swallowed[2]{}, fast[2]{};
        double distErr = 0;
        uint32_t distN = 0;
        for (int m = 0; m < 2; ++m) {
            wrist_hud::Toggle toggle;
            toggle.setMode(m == 0 ? 0u : 4u); // 38.1's TAP (palm look) / 38.2's FOREARM
            uint32_t lastPresses = 0;
            replay(rows, [&](const Tick& k) {
                toggle.look(k.head, k.headOk);
                const Pose grips[2]{k.hand[0], k.hand[1]};
                const bool tracked[2]{k.handOk[0], k.handOk[1]};
                const float trig[2]{0.f, triggerAt(presses, k.t, .15)};
                toggles[m] += toggle.update(k.dt, k.play, 0, grips, tracked, trig);
                const auto& d = toggle.diagnostics();
                if (m == 1 && d.presses > lastPresses) {
                    lastPresses = d.presses;
                    const float rec = rows[std::min(k.row + 1, rows.size() - 1)].dist;
                    if (d.pressDistance >= 0 && rec >= 0 && rec < .5f) {
                        distErr += std::fabs(d.pressDistance - rec);
                        ++distN;
                    }
                    clicksNear[m] += d.pressDistance >= 0 && d.pressDistance <= wrist_hud::Toggle::forearmReach;
                }
            });
            swallowed[m] = toggle.diagnostics().zoneSwallowed;
            fast[m] = toggle.diagnostics().forearmFast;
            if (m == 1)
                clicksNear[1] = clicksNear[1];
        }
        std::printf("  %-7s presses in reach %3u: recorded toggles %3u (%3.0f%%) | replay 38.1 TAP %3u | 38.2 FOREARM %3u "
                    "(%3.0f%%), clicks in reach (replay) %u, held off as fast %u, zone swallowed %u; replay vs "
                    "recorded distance %.1f cm mean (%u)\n",
                    run, nearRec, toggledRec, nearRec ? 100.0 * toggledRec / nearRec : 0.0, toggles[0], toggles[1],
                    nearRec ? 100.0 * std::min(toggles[1], nearRec) / nearRec : 0.0, clicksNear[1], fast[1], swallowed[1],
                    distN ? 100 * distErr / distN : 0.0, distN);
        allNear += clicksNear[1];
        allOpened += std::min(toggles[1], clicksNear[1]);
        if (std::string(run) == "run381") {
            check(toggles[1] > toggles[0] * 2 && toggles[1] >= nearRec * 8 / 10,
                  "run381: FOREARM toggles >= 80% of the in-reach presses and more than twice 38.1's TAP");
            check(distN > 0 && distErr / distN < .03, "run381: the replay's forearm distance within 3 cm of the recorded");
        }
    }
    check(allNear > 0 && allOpened * 10 >= allNear * 9, "all runs: >= 90% of in-reach clicks toggle under FOREARM");
}

// ---- 2. RING / HOLO FLICKER: hides per second -----------------------------------------------
void flicker() {
    std::printf("\n[2] RING / HOLO FLICKER: hide toggles per second (38.1 NEWEST hid with the image %.0f ms older than the "
                "newest frame or its world %.0f m off)\n",
                combat38::ringStaleSeconds * 1000, combat38::ringJumpMetres);
    for (const char* run : runs) {
        const auto rows = load(run);
        // 38.1's test (game_xr.cpp 5679-5684): latestShown / latestToWorld are set when the game
        // starts a frame (5288-5295); `saved` is the newest FINISHED image (copyLatest, 5628). The
        // game starts frame N+1 while N is on screen, so the age is at least one game-frame
        // interval plus the frames N has been reshown. From the recorded counters: within each
        // sample, `d` frames shown with `u` new images; a sample with no frame is a stall.
        uint64_t hides = 0, hiddenFrames = 0, frames = 0;
        bool hidden = false;
        double since = 0, interval = 1.0 / 45, seconds = 0;
        combat38::RingHold hold;
        uint64_t holdHidden = 0;
        for (size_t i = 1; i < rows.size(); ++i) {
            const Row& r = rows[i];
            const Row& q = rows[i - 1];
            const double d = r.sub - q.sub, u = r.uniq - q.uniq, dt = r.t - q.t;
            if (!(dt > 0) || dt > 1 || !r.play)
                continue;
            seconds += dt;
            if (!(d > 0)) {
                since += dt; // the headset held the picture: the next image is older
                continue;
            }
            const float speed = r.headOk && q.headOk ? length(r.head.position - q.head.position) / float(dt) : 0.f;
            const double frame = dt / d;
            const double every = u > 0 ? d / u : 1e9;
            for (int f = 0; f < int(d); ++f) {
                ++frames;
                if (u > 0 && std::fmod(double(f), every) < 1.0) {
                    interval = since + frame;
                    since = 0;
                } else {
                    since += frame;
                }
                const double age = since + interval;
                const bool stale = combat38::ringStale(float(age), speed * float(age));
                hiddenFrames += stale;
                hides += stale && !hidden;
                hidden = stale;
                // HOLD on the same frames: the image's own wrist, the last good pose kept.
                hold.update(r.hand[0], r.handOk[0]);
                holdHidden += !hold.has;
            }
        }
        const double before = seconds > 0 ? hides / seconds : 0;
        std::printf("  %-7s %.0f s play, %llu frames: 38.1 NEWEST %.2f hides/s (%.1f%% of frames hidden) | 38.2 HOLD 0.00 "
                    "hides/s (%llu frames before the first pose), %llu kept through bad samples\n",
                    run, seconds, (unsigned long long)frames, before, frames ? 100.0 * hiddenFrames / frames : 0.0,
                    (unsigned long long)holdHidden, (unsigned long long)hold.held);
        if (std::string(run) == "run381")
            check(before > 1 && holdHidden < 200, "run381: NEWEST flickered (> 1 hide/s); HOLD hides 0 times a second");
    }
}

// ---- 3. RING AT THE WRIST: error from the drawn wrist ----------------------------------------
void ringError() {
    std::printf("\n[3] RING AT THE WRIST: distance from the drawn wrist (body hand_error_m: the IK wrist short of "
                "the controller)\n");
    // 38.1 HAND/NEWEST put the ring on the controller's spot: its error IS the body's shortfall.
    // 38.2 WRIST moves it toward the shoulder by that shortfall (capped .3 m). The direction is the
    // model's: an arm the IK cannot stretch falls short along the shoulder-hand line, checked here
    // as how much of the shortfall the reach past the arm explains.
    for (const char* run : runs) {
        const auto rows = load(run);
        std::vector<float> before, explained;
        std::vector<std::pair<float, float>> pairs; // reach, shortfall
        for (const auto& r : rows) {
            if (!r.play || !r.handOk[0] || !r.headOk || !(r.el >= 0))
                continue;
            before.push_back(r.el);
            const Vec3 spot = combat38::webShooterSpot(r.hand[0], 0);
            pairs.push_back({length(spot - combat38::shoulderOf(r.head, 0)), r.el});
        }
        if (before.empty())
            continue;
        // The arm length that explains the shortfall best: el ~ max(0, reach - arm).
        float bestArm = 0, bestRes = 1e9f;
        for (float arm = .40f; arm <= .80f; arm += .005f) {
            double res = 0;
            uint32_t n = 0;
            for (const auto& [reach, e] : pairs)
                if (e > .02f) {
                    res += std::fabs(e - std::max(0.f, reach - arm));
                    ++n;
                }
            if (n && res / n < bestRes) {
                bestRes = float(res / n);
                bestArm = arm;
            }
        }
        std::vector<float> after;
        for (const auto& [reach, e] : pairs) {
            // WRIST: the spot moved by the live shortfall (capped): what is left is what the cap cuts
            // off; the reach model's residual where the body fell short (direction check).
            const float capped = std::min(e, .3f);
            after.push_back(e - capped);
            if (e > .02f)
                explained.push_back(std::fabs(e - std::max(0.f, reach - bestArm)));
        }
        const auto pct = [](std::vector<float> v, double p) {
            if (v.empty())
                return 0.f;
            std::sort(v.begin(), v.end());
            return v[std::min(v.size() - 1, size_t(p * double(v.size())))];
        };
        const auto over2 = std::count_if(after.begin(), after.end(), [](float e) { return e > .02f; });
        const auto over2b = std::count_if(before.begin(), before.end(), [](float e) { return e > .02f; });
        std::printf("  %-7s > 2 cm off the drawn wrist: 38.1 %td samples (%.1f%%) | 38.2 %td (%.2f%%)\n", run, over2b,
                    100. * double(over2b) / double(before.size()), over2, 100. * double(over2) / double(after.size()));
        std::printf("  %-7s n %zu: 38.1 HAND p50 %.1f cm p95 %.1f cm max %.1f cm | 38.2 WRIST p95 %.1f cm max %.1f cm; "
                    "shortfall > 2 cm on %zu samples, reach past a %.0f cm arm explains it to %.1f cm p50 / %.1f cm p95\n",
                    run, before.size(), 100 * pct(before, .5), 100 * pct(before, .95), 100 * pct(before, 1),
                    100 * pct(after, .95), 100 * pct(after, 1), explained.size(), 100 * bestArm,
                    100 * pct(explained, .5), 100 * pct(explained, .95));
        check(double(over2) <= .001 * double(after.size()),
              (std::string(run) + ": WRIST is within 2 cm of the drawn wrist on >= 99.9% of samples (the .3 m cap: a broken body frame)").c_str());
        if (std::string(run) == "run381")
            check(pct(before, .95) > .05f && pct(after, .95) < .02f,
                  "run381: HAND p95 > 5 cm from the drawn wrist; WRIST p95 < 2 cm (shortfall along the arm)");
    }
    // HOLD's follow: 1 (no lag); 38.1's easing lagged.
    combat38::RingHold hold;
    hold.update(Pose{{0, 0, 0}, {0, 0, 0, 1}}, true);
    const Pose p = hold.update(Pose{{.02f, 0, 0}, {0, 0, 0, 1}}, true);
    check(std::fabs(p.position.x - .02f) < 1e-5f, "HOLD follows the wrist with no lag (2 cm step at 90 Hz: 0 mm behind)");
}

// ---- configs ------------------------------------------------------------------------------
combat_input::Config config381() {
    combat_input::Config c; // 38.1's combatConfig() at its defaults
    c.zipMode = combat38::zipSpidy;
    c.punchMode = 0;
    c.punchTargeted = false;
    c.assistSpidy = true;
    c.assistSmart = false;
    c.assistDegrees = 0;
    c.meleePunchOnly = false;
    c.gadgetEasy = false;
    c.lungeMode = combat38::lungeGameButton;
    c.ballOnPress = true;
    c.lungeFrom = combat38::lungeFromMetres(0);
    c.lungeSpeed = combat38::lungeSpeedOf(0);
    c.lungeSteady = true;
    c.punchForward = combat38::punchForwardOf(0);
    c.ballLeadAlways = false;
    return c;
}
combat_input::Config config382() {
    combat_input::Config c = config381();
    c.punchTargeted = true;
    c.assistSmart = true;
    c.assistSpidy = false;
    c.meleePunchOnly = true;
    c.gadgetEasy = true;
    const auto s = combat38::gadgetSlots(0, {});
    c.grabberSlot = s.grabber;
    c.sonicSlot = s.sonic;
    c.upshotSlot = s.upshot;
    c.ricochetSlot = s.ricochet;
    return c;
}
// Update 38.4's combatConfig() at its defaults (every COMBAT option 0), and 38.5's (farLunge).
combat_input::Config config384() {
    combat_input::Config c = config382();
    c.lookQuick = true;
    c.strikeSteer = true;
    return c;
}
combat_input::Config config385() {
    combat_input::Config c = config384();
    c.farLunge = true;
    return c;
}
combat_input::Frame frameOf(const Tick& k) {
    combat_input::Frame f;
    f.seconds = k.dt;
    f.play = k.play && k.headOk;
    f.head = k.head.position;
    f.forward = k.head.orientation.rotate({0, 0, -1});
    for (unsigned i = 0; i < 2; ++i) {
        auto& h = f.hands[i];
        h.tracked = k.handOk[i];
        h.free = true;
        h.position = k.hand[i].position;
        h.aim = combat38::aimOf(k.hand[i], k.hand[i], combat38::axisHand);
        h.palm = k.hand[i].orientation.rotate({0, -1, 0});
    }
    return f;
}
struct Pulses {
    uint32_t edges{};
    double last = -1e9, minGap = 1e9;
    uint32_t chained{}; // a new X within .65 s of the last one's end: the game's combo (kicks)
    bool was{};
    double endedAt = -1e9;
    void add(bool x, double t) {
        if (x && !was) {
            ++edges;
            if (last > -1e8)
                minGap = std::min(minGap, t - endedAt);
            chained += t - endedAt < combat38::meleeSpacing;
            last = t;
        }
        if (!x && was)
            endedAt = t;
        was = x;
    }
};

// ---- 4. KICKS / ARM WAVING: X with no enemy; PUNCH ONLY's spacing -------------------------------
void kicks() {
    std::printf("\n[4] ARM-WAVING ATTACKS: the game's X (melee) from the real hand motion\n");
    for (const char* run : runs) {
        const auto rows = load(run);
        uint32_t recorded = 0;
        for (size_t i = 1; i < rows.size(); ++i)
            recorded += (rows[i].pad & 0x4000) && !(rows[i - 1].pad & 0x4000);
        Pulses none[2], near[2];
        uint32_t neutralBad = 0, menuX = 0;
        for (int v = 0; v < 2; ++v) {
            const auto c = v ? config382() : config381();
            combat_input::Machine idle, fight, menu;
            replay(rows, [&](const Tick& k) {
                auto f = frameOf(k);
                none[v].add(idle.update(f, c).buttons & combat_input::padX, k.t);
                // An enemy 1.2 m ahead of the head (synthesised) for the in-combat spacing.
                auto g = f;
                const Vec3 ahead = combat_input::level(g.forward);
                g.enemies[0] = g.head + ahead * 1.2f + Vec3{0, -1.65f, 0};
                g.enemyIds[0] = 7;
                g.enemyCount = 1;
                g.enemyMetres = 1.2f;
                const auto o = fight.update(g, c);
                near[v].add(o.buttons & combat_input::padX, k.t);
                if (v && (o.buttons & combat_input::padX) && !o.stickNeutral)
                    ++neutralBad;
                auto m = g;
                m.menuOpen = true;
                if (v)
                    menuX += (menu.update(m, c).buttons & combat_input::padX) != 0;
            });
        }
        std::printf("  %-7s no enemy: recorded X %u | replay 38.1 BOTH %u | 38.2 TARGETED %u.  enemy at 1.2 m: 38.1 X %u "
                    "(%u chained < %.2f s, min gap %.2f s) | 38.2 PUNCH ONLY %u (%u chained, min gap %.2f s, stick "
                    "not neutral %u); holo/mirror open X frames %u\n",
                    run, recorded, none[0].edges, none[1].edges, near[0].edges, near[0].chained, combat38::meleeSpacing,
                    near[0].minGap < 1e8 ? near[0].minGap : 0.0, near[1].edges, near[1].chained,
                    near[1].minGap < 1e8 ? near[1].minGap : 0.0, neutralBad, menuX);
        if (std::string(run) == "run381") {
            check(none[0].edges > 0 && none[1].edges == 0, "run381: arm waving with no enemy - 38.1 attacked, 38.2 never");
            check(near[1].chained == 0 && neutralBad == 0 && near[1].edges > 0,
                  "run381: PUNCH ONLY - every X single, >= .65 s apart, stick neutral");
            check(menuX == 0, "run381: no X with the hologram or the mirror open");
        }
    }
    // The video's window: run381 0:20-0:26 after the first sample, and the densest X burst.
    const auto rows = load("run381");
    for (const auto& [from, to, what] : {std::tuple{20.0, 26.0, "0:20-0:26 (from the first sample)"},
                                         std::tuple{404.5, 414.6, "404.5-414.6 s (14 recorded X in 10 s)"}}) {
        const double t0 = rows.front().t;
        const double a = std::string(what).rfind("0:20", 0) == 0 ? t0 + from : t0 + from, b = t0 + to;
        Pulses p[2];
        uint32_t rec = 0;
        for (size_t i = 1; i < rows.size(); ++i)
            if (rows[i].t >= a && rows[i].t <= b)
                rec += (rows[i].pad & 0x4000) && !(rows[i - 1].pad & 0x4000);
        for (int v = 0; v < 2; ++v) {
            combat_input::Machine m;
            const auto c = v ? config382() : config381();
            replay(rows, [&](const Tick& k) { p[v].add(m.update(frameOf(k), c).buttons & combat_input::padX, k.t); },
                   a - 2, b);
        }
        std::printf("  run381 %s: recorded X %u | replay 38.1 %u | 38.2 %u (no enemy in the log)\n", what, rec,
                    p[0].edges, p[1].edges);
        if (std::string(what).rfind("404", 0) == 0)
            check(p[0].edges > 0 && p[1].edges == 0, "run381 burst: 38.1 attacked, 38.2 zero");
    }
}

// ---- 5. GADGETS: recognised from the real motion after each pull --------------------------------
void gadgets() {
    std::printf("\n[5] GADGETS: each recorded pull held %.1f s (synthesised), the real hand motion after it\n", .6);
    uint32_t tot[2]{}, deliberate = 0, rec[2]{}, upN = 0, upOk[2]{}, pullN = 0, pullOk[2]{}, flickN = 0, flickOk[2]{};
    std::vector<float> lat[2];
    uint32_t calm = 0, calmFired[2]{}, ballFirst[2]{}, calmBall[2]{}, early = 0, earlyBallFirst[2]{};
    std::vector<float> ballLat[2];
    for (const char* run : runs) {
        const auto rows = load(run);
        const auto presses = pressTimes(rows);
        for (const double p : presses) {
            // Label the window by the hand's own motion (head-relative, 90 Hz).
            float peak = 0, peakUp = 0, peakPull = 0, peakFlick = 0, onset = 9;
            Vec3 was{};
            bool set = false;
            replay(rows, [&](const Tick& k) {
                if (!k.handOk[1] || !k.headOk || !k.play) {
                    set = false;
                    return;
                }
                const Vec3 rel = k.hand[1].position - k.head.position;
                if (set) {
                    const Vec3 v = (rel - was) / k.dt;
                    const float s = length(v);
                    if (s < 8.f) {
                        peak = std::max(peak, s);
                        if (s >= combat_input::Config::easyBallHold && onset > 8)
                            onset = float(k.t - p);
                        if (v.y >= .6f * s)
                            peakUp = std::max(peakUp, v.y);
                        const Vec3 body = combat_input::level(k.head.position - k.hand[1].position);
                        if (length(body) > .5f && dot(v, body) >= .6f * s)
                            peakPull = std::max(peakPull, dot(v, body));
                        const Vec3 aim = combat38::aimOf(k.hand[1], k.hand[1], combat38::axisHand);
                        if (length(aim) > .5f && dot(v, normalized(aim)) >= .6f * s)
                            peakFlick = std::max(peakFlick, dot(v, normalized(aim)));
                    }
                }
                was = rel;
                set = true;
            }, p, p + .6);
            const bool isUp = peakUp >= 1.2f, isPull = peakPull >= 1.3f, isFlick = peakFlick >= 1.4f;
            const bool delib = isUp || isPull;
            deliberate += delib;
            upN += isUp && !isPull;
            pullN += isPull && !isUp;
            flickN += isFlick;
            const bool isCalm = peak < .6f;
            calm += isCalm;
            for (int v = 0; v < 2; ++v) {
                for (int withGrip = 0; withGrip < 2; ++withGrip) {
                    if (withGrip && !isFlick)
                        continue;
                    const auto c = v ? config382() : config381();
                    combat_input::Machine m;
                    uint32_t got = combat_input::none;
                    float at = -1, ballAt = -1;
                    replay(rows, [&](const Tick& k) {
                        auto f = frameOf(k);
                        f.hands[1].button = k.t >= p ? 1.f : 0.f;
                        // Ricochet: the grip squeezed first (38.1 needed it after the trigger).
                        f.hands[1].grip = withGrip ? 1.f : 0.f;
                        const auto o = m.update(f, c);
                        if (ballAt < 0 && o.fired[1] == combat_input::webShooter && k.t >= p)
                            ballAt = float(k.t - p);
                        if (got == combat_input::none && o.fired[1] != combat_input::none &&
                            o.fired[1] != combat_input::webShooter && k.t >= p) {
                            got = o.fired[1];
                            at = float(k.t - p);
                        }
                    }, p - .3, p + .6);
                    if (withGrip) {
                        flickOk[v] += got == combat_input::ricochetWeb;
                        continue;
                    }
                    tot[v] += got != combat_input::none;
                    const bool first = delib && got != combat_input::none && ballAt >= 0 && ballAt <= at;
                    ballFirst[v] += first;
                    // The move was already under way before the ball's grace ran out.
                    const bool isEarly = delib && onset < combat_input::Config::easyGrace;
                    early += v == 0 && isEarly;
                    earlyBallFirst[v] += isEarly && (got == combat_input::none || first);
                    if (std::getenv("REPLAY_DEBUG") && v && first)
                        std::printf("    dbgball %s %.2f got %u at %.3f ball %.3f onset %.3f\n", run, p, got, at, ballAt, onset);
                    if (isCalm && ballAt >= 0) {
                        ++calmBall[v];
                        ballLat[v].push_back(ballAt);
                    }
                    if (delib && got != combat_input::none) {
                        ++rec[v];
                        lat[v].push_back(at);
                    }
                    upOk[v] += isUp && !isPull && got == combat_input::upshot;
                    if (std::getenv("REPLAY_DEBUG") && isUp && !isPull)
                        std::printf("    dbg %s %.2f v%d got %u at %.3f up %.2f pull %.2f flick %.2f peak %.2f\n", run, p, v,
                                    got, at, peakUp, peakPull, peakFlick, peak);
                    pullOk[v] += isPull && !isUp && got == combat_input::webGrabber;
                    calmFired[v] += isCalm && got != combat_input::none;
                }
            }
        }
    }
    const auto med = [](std::vector<float> v) {
        if (v.empty())
            return 0.f;
        std::sort(v.begin(), v.end());
        return v[v.size() / 2];
    };
    std::printf("  deliberate gadget motions (up >= 1.2 or pull >= 1.3 m/s): %u | recognised 38.1 GAME %u (%.0f%%, median "
                "%.0f ms) | EASY (38.3) %u (%.0f%%, median %.0f ms)\n",
                deliberate, rec[0], deliberate ? 100.0 * rec[0] / deliberate : 0.0, 1000 * med(lat[0]), rec[1],
                deliberate ? 100.0 * rec[1] / deliberate : 0.0, 1000 * med(lat[1]));
    std::printf("  up-only -> Upshot %u/%u vs %u/%u; pull-only -> Grabber %u/%u vs %u/%u; flick with the grip first -> "
                "Ricochet %u/%u vs %u/%u; a still hand fired %u/%u vs %u/%u\n",
                upOk[0], upN, upOk[1], upN, pullOk[0], pullN, pullOk[1], pullN, flickOk[0], flickN, flickOk[1], flickN,
                calmFired[0], calm, calmFired[1], calm);
    check(deliberate > 0 && rec[1] * 10 >= deliberate * 8 && rec[1] > rec[0],
          "EASY recognises >= 80% of the deliberate motions, more than 38.1");
    check(calmFired[1] == 0, "EASY: a still hand with the trigger held fires no gadget");
    check(upN == 0 || upOk[1] * 10 >= upN * 8, "EASY: up motions are the Upshot (>= 80%)");
    std::printf("  a ball fired before the gadget: 38.1 %u/%u vs EASY (38.3) %u/%u; still-hand pulls still fire the ball %u/%u vs "
                "%u/%u (median %.0f vs %.0f ms)\n",
                ballFirst[0], rec[0], ballFirst[1], rec[1], calmBall[0], calm, calmBall[1], calm, 1000 * med(ballLat[0]),
                1000 * med(ballLat[1]));
    std::printf("  moves already under way within 0.2 s of the pull: %u; the ball beat (or replaced) the gadget 38.1 %u/%u vs EASY (38.3) %u/%u\n",
                early, earlyBallFirst[0], early, earlyBallFirst[1], early);
    check(early == 0 || (earlyBallFirst[1] * 10 <= early && earlyBallFirst[1] < earlyBallFirst[0]),
          "EASY: a move under way at the pull is the gadget, not a ball first (<= 10%, fewer than 38.1)");
    check(ballFirst[1] < ballFirst[0], "EASY: fewer balls before the gadget than 38.1 overall");
    check(calmBall[1] * 100 >= calm * 95 && med(ballLat[1]) <= .25f, "EASY: a still pull still fires the ball (>= 95%, <= 250 ms)");
    check(pullN == 0 || pullOk[1] * 10 >= pullN * 8, "EASY: pulls are the Grabber (>= 80%)");
    check(flickN == 0 || flickOk[1] > flickOk[0], "EASY: Ricochet with the grip pressed first (38.1 needed it after)");
}

// ---- 6. AIM ASSIST: web balls from the real wrist poses at synthesised enemies -------------------
uint32_t lcg = 12345;
float rnd() {
    lcg = lcg * 1664525u + 1013904223u;
    return float(lcg >> 8) / float(1u << 24);
}
Vec3 turned(Vec3 d, float degrees, float roll) {
    Vec3 side = normalized(cross(d, std::fabs(d.y) < .9f ? Vec3{0, 1, 0} : Vec3{1, 0, 0}));
    const Vec3 up = cross(side, d);
    const Vec3 axis = side * std::cos(roll) + up * std::sin(roll);
    const float a = degrees * .0174532925f;
    return normalized(d * std::cos(a) + axis * std::sin(a));
}
float missMetres(Vec3 from, Vec3 dir, Vec3 chest) {
    // The ball's ray against the torso (chest +-.45 m vertical): the least distance.
    float best = 1e9f;
    for (float s = -.45f; s <= .45f; s += .05f) {
        const Vec3 c = chest + Vec3{0, s, 0};
        const float t = std::max(0.f, dot(c - from, dir));
        best = std::min(best, length(from + dir * t - c));
    }
    return best;
}
void aim() {
    std::printf("\n[6] AIM ASSIST: a web ball at each recorded pull from the real wrist ray; the enemy the player "
                "meant 0-15 deg off it, 3-20 m, two others 20-50 deg off (synthesised)\n");
    uint32_t shots = 0, hits[3]{}, wrong[3]{};
    for (const char* run : runs) {
        const auto rows = load(run);
        for (size_t i = 1; i < rows.size(); ++i) {
            if (!(rows[i].presses > rows[i - 1].presses) || !rows[i].handOk[1] || !rows[i].headOk || !rows[i].play)
                continue;
            const Pose& hand = rows[i].hand[1];
            const Vec3 ray = combat38::aimOf(hand, hand, combat38::axisHand);
            if (!(length(ray) > .5f))
                continue;
            std::array<Vec3, combat38::maxEnemies> chests{};
            chests[0] = hand.position + turned(ray, 15.f * rnd(), 6.2832f * rnd()) * (3.f + 17.f * rnd());
            for (int e = 1; e < 3; ++e)
                chests[size_t(e)] = hand.position + turned(ray, 20.f + 30.f * rnd(), 6.2832f * rnd()) * (2.f + 18.f * rnd());
            ++shots;
            for (int v = 0; v < 3; ++v) {
                Vec3 dir = ray;
                if (v) {
                    const auto c = v == 2 ? config382() : config381();
                    combat_input::Machine m;
                    combat_input::Frame f;
                    f.play = true;
                    f.head = rows[i].head.position;
                    f.forward = rows[i].head.orientation.rotate({0, 0, -1});
                    for (unsigned h = 0; h < 2; ++h) {
                        f.hands[h].tracked = true;
                        f.hands[h].free = true;
                        f.hands[h].position = rows[i].hand[h].position;
                        f.hands[h].aim = combat38::aimOf(rows[i].hand[h], rows[i].hand[h], combat38::axisHand);
                    }
                    f.enemyCount = 3;
                    for (uint32_t e = 0; e < 3; ++e) {
                        f.enemies[e] = chests[e] - Vec3{0, combat38::centreMass, 0};
                        f.enemyIds[e] = 100 + e;
                        f.enemySight[e] = combat_spidy::sightClear;
                    }
                    f.enemyMetres = length(f.enemies[0] - f.head);
                    // the pull held past the Sonic Burst's two-hand grace (the ball fires then)
                    for (int step = 0; step < 60; ++step) {
                        f.seconds = 1.f / 90.f;
                        f.hands[1].button = step >= 2 ? 1.f : 0.f;
                        const auto o = m.update(f, c);
                        if (o.fired[1] == combat_input::webShooter) {
                            dir = o.ballDirection[1];
                            break;
                        }
                    }
                }
                const float miss = missMetres(hand.position, normalized(dir), chests[0]);
                hits[v] += miss <= .30f;
                bool other = false;
                for (int e = 1; e < 3; ++e)
                    other |= missMetres(hand.position, normalized(dir), chests[size_t(e)]) <= .30f;
                wrong[v] += other;
            }
        }
    }
    std::printf("  %u shots: no assist hits %u (%.0f%%) | 38.1 SPIDYVR %u (%.0f%%), wrong enemy %u | 38.2 SMART %u "
                "(%.0f%%), wrong enemy %u\n",
                shots, hits[0], shots ? 100.0 * hits[0] / shots : 0.0, hits[1], shots ? 100.0 * hits[1] / shots : 0.0,
                wrong[1], hits[2], shots ? 100.0 * hits[2] / shots : 0.0, wrong[2]);
    check(shots > 50 && hits[2] * 100 >= shots * 90 && hits[2] > hits[1],
          "SMART hits the meant enemy's centre mass >= 90% of shots, more than SPIDYVR");
    check(wrong[2] * 100 <= shots * 3, "SMART picks another enemy on <= 3% of shots");
}

// ---- 7. GRAB WEB on an enemy -----------------------------------------------------------------
void grab() {
    std::printf("\n[7] GRAB WEB: the grip's web ray from each recorded pull's wrist at an enemy 3-20 m off it by "
                "0-0.6 m (synthesised)\n");
    uint32_t tries = 0, shouldHit = 0, hit = 0, falseHit = 0, yanks = 0, pulls = 0, yankNoPull = 0, holds = 0;
    for (const char* run : runs) {
        const auto rows = load(run);
        for (size_t i = 1; i < rows.size(); ++i) {
            if (!(rows[i].presses > rows[i - 1].presses) || !rows[i].handOk[1] || !rows[i].play || !rows[i].headOk)
                continue;
            const Pose& g = rows[i].hand[1];
            const Vec3 from = combat38::webShooterSpot(g, 1);
            const Vec3 ray = combat38::aimOf(g, g, combat38::axisHand);
            const float d = 3.f + 17.f * rnd(), off = .6f * rnd();
            Vec3 side = normalized(cross(ray, Vec3{0, 1, 0}));
            if (!finite(side))
                continue;
            const Vec3 chest = from + normalized(ray) * d + side * off;
            std::array<Vec3, combat38::maxEnemies> feet{};
            feet[0] = chest - Vec3{0, combat38::centreMass, 0};
            ++tries;
            const bool want = off <= combat38::grabRadius - .02f;
            const bool far = off >= combat38::grabRadius + .02f;
            float metres = -1;
            const int e = combat38::grabTarget(from, ray, feet, 1, &metres);
            shouldHit += want;
            hit += want && e == 0;
            falseHit += far && e == 0;
            if (e != 0)
                continue;
            // Held 1.5 s with the grip: the real motion after the pull, a yank once.
            combat38::GrabWeb web;
            web.attach(0, from + normalized(ray) * metres);
            Vec3 was{};
            bool set = false;
            float peakPull = 0;
            bool yanked = false;
            replay(rows, [&](const Tick& k) {
                if (!k.handOk[1] || !k.headOk)
                    return;
                const Vec3 rel = k.hand[1].position - k.head.position;
                const Vec3 v = set ? (rel - was) / k.dt : Vec3{};
                was = rel;
                const bool s = set;
                set = true;
                const Vec3 toBody = k.head.position + Vec3{0, -.35f, 0} - k.hand[1].position;
                if (s && length(toBody) > .05f && length(v) < 8.f)
                    peakPull = std::max(peakPull, dot(v, normalized(toBody)));
                yanked |= web.update(k.dt, true, true, s ? v : Vec3{}, toBody);
            }, rows[i].t, rows[i].t + 1.5);
            holds += web.on || web.released == 0;
            const bool pull = peakPull >= combat38::GrabWeb::yankSpeed + .3f;
            pulls += pull;
            yanks += pull && yanked;
            yankNoPull += !pull && yanked && peakPull < combat38::GrabWeb::yankSpeed - .3f;
        }
    }
    std::printf("  %u grabs: on the capsule %u/%u (38.1: 0 - the swing web went through, game_swing.cpp notPorted); "
                "beside it (> .47 m) caught %u; held through the motion %u; real pulls %u yanked %u (CANDIDATE input); "
                "a yank with no pull %u\n",
                tries, hit, shouldHit, falseHit, holds, pulls, yanks, yankNoPull);
    check(shouldHit > 0 && hit == shouldHit && falseHit == 0, "GRAB WEB: every ray on the capsule attaches, none beside it");
    check(pulls == 0 || yanks == pulls, "GRAB WEB: every real pull past the yank speed yanks once");
    check(yankNoPull == 0, "GRAB WEB: no yank without a pull");
}

// ---- 9. Update 38.3 LOOK LOCK: the lock follows a deliberate look (real head poses) --------------
// Every 15 s of each run, three enemies 6 m out at -20, 0 and +20 degrees of the head's level
// forward (synthesised: no log has enemy positions); the middle one is locked by a blow and a blow
// lands on whoever is locked every second (a fight). A deliberate look: the head within 6 degrees
// of another enemy in the lock's 10 m reach for 0.4 s. 38.2: LockConfig's defaults plus STEADY's 0.5 s hold; 38.3 QUICK.
void lookLock() {
    std::printf("\n[9] LOOK LOCK: the real head turns at three synthesised enemies 6 m out, -20/0/+20 degrees\n");
    uint32_t looks = 0, looksQ = 0, followed[2]{}, stray[2]{}, switches[2]{};
    for (const char* run : runs) {
        const auto rows = load(run);
        if (rows.size() < 2)
            continue;
        for (double from = rows.front().t; from + 15 <= rows.back().t; from += 15) {
            for (int quick = 0; quick < 2; ++quick) {
                LockConfig lc{};
                if (quick) {
                    lc.dwell = combat_input::Config::quickDwell;
                    lc.aside = combat_input::Config::quickAside;
                    lc.margin = combat_input::Config::quickMargin;
                    lc.square = combat_input::Config::quickSquare;
                }
                Lock lock(lc);
                std::array<FightTarget, 3> enemies{};
                bool placed = false;
                double sinceBlow = 0, lookFor = 0, raw0 = 0;
                uint64_t shown = 0, rawLast = 0, lookAt = 0;
                bool lookCounted = false, lookFollowed = false, lookOther = false, pendingFollow = false;
                float holdFor = 0;
                replay(rows, [&](const Tick& k) {
                    if (!k.headOk || !k.play)
                        return;
                    const Vec3 look = k.head.orientation.rotate({0, 0, -1});
                    if (!placed) {
                        Vec3 ahead = look;
                        ahead.y = 0;
                        if (length(ahead) < .3f)
                            return;
                        ahead = normalized(ahead);
                        for (int e = 0; e < 3; ++e) {
                            const float a = (e - 1) * 20.f * .0174532925f;
                            const Vec3 d{ahead.x * std::cos(a) - ahead.z * std::sin(a), 0,
                                         ahead.x * std::sin(a) + ahead.z * std::cos(a)};
                            enemies[e].id = 1 + e;
                            enemies[e].feet = k.head.position + d * 6.f - Vec3{0, 1.7f, 0};
                        }
                        lock.update(k.dt, k.head.position, look, enemies);
                        lock.take(2);
                        shown = rawLast = 2;
                        placed = true;
                        return;
                    }
                    const uint64_t raw = lock.update(k.dt, k.head.position, look, enemies);
                    if ((sinceBlow += k.dt) >= 1 && raw) {
                        lock.take(raw);
                        sinceBlow = 0;
                    }
                    // 38.2's STEADY hold on a switch (combat_input.hpp), not with QUICK.
                    uint64_t now = raw;
                    if (!quick && raw != shown && shown && (holdFor += k.dt) < combat38::steadyLockHold)
                        now = shown;
                    else
                        holdFor = 0;
                    // Which enemy the head is on (within 6 degrees), for how long.
                    uint64_t on = 0;
                    float best = 6.f;
                    for (const auto& t : enemies) {
                        if (length(t.middle() - k.head.position) > lc.reach)
                            continue; // the lock's 10 m reach: the player swings away from the fight
                        const float off = std::acos(std::clamp(dot(normalized(t.middle() - k.head.position), look), -1.f, 1.f)) * 57.29578f;
                        if (off < best) {
                            best = off;
                            on = t.id;
                        }
                    }
                    if (on != lookAt) {
                        lookAt = on;
                        lookFor = 0;
                        lookCounted = lookFollowed = false;
                        lookOther = on && on != shown; // began on another enemy than the lock
                    }
                    lookFor += k.dt;
                    if (now != shown && now) {
                        ++switches[quick];
                        // A switch to an enemy the head is not on (within 10 degrees) is a stray.
                        const auto& t = enemies[now - 1];
                        const float off = std::acos(std::clamp(dot(normalized(t.middle() - k.head.position), look), -1.f, 1.f)) * 57.29578f;
                        stray[quick] += off > 10.f;
                    }
                    shown = now;
                    if (lookOther && lookFor >= .4 && !lookCounted) {
                        lookCounted = true; // a deliberate look at another enemy than the lock
                        looks += quick == 0;
                        looksQ += quick == 1;
                    }
                    if (lookOther && !lookFollowed && shown == lookAt && lookFor <= 1.5) {
                        lookFollowed = true; // counted once the look proves deliberate
                        pendingFollow = true;
                    }
                    if (pendingFollow && lookCounted) {
                        pendingFollow = false;
                        ++followed[quick];
                    }
                    (void)raw0;
                    (void)rawLast;
                }, from, from + 15);
            }
        }
    }
    std::printf("  deliberate looks at an unlocked enemy (0.4 s within 6 deg, in 10 m): lock followed within 1.5 s 38.2 "
                "%u/%u -> 38.3 QUICK %u/%u; switches %u -> %u; stray switches (head > 10 deg off the new one) %u -> %u\n",
                followed[0], looks, followed[1], looksQ, switches[0], switches[1], stray[0], stray[1]);
    check(looksQ > 10 && followed[1] * 10 >= looksQ * 8 && followed[1] * looks > followed[0] * looksQ,
          "LOOK LOCK QUICK: the lock follows >= 80% of deliberate looks, more than 38.2");
    check(stray[1] * 20 <= switches[1] + 1, "LOOK LOCK QUICK: <= 5% of its switches go to an enemy the head is not on");
}

// ---- 10. Update 38.3 GRAB WEB ASSIST: the 12-degree cone ----------------------------------------
void grabAssist() {
    std::printf("\n[10] GRAB WEB ASSIST: each recorded pull's grip ray at one enemy 3-15 m away, 0-20 degrees off "
                "(synthesised)\n");
    uint32_t inCone = 0, outCone = 0, caught[2]{}, wrong[2]{};
    for (const char* run : runs) {
        const auto rows = load(run);
        for (size_t i = 1; i < rows.size(); ++i) {
            if (!(rows[i].presses > rows[i - 1].presses) || !rows[i].handOk[1] || !rows[i].play)
                continue;
            const Pose& g = rows[i].hand[1];
            const Vec3 from = combat38::webShooterSpot(g, 1);
            const Vec3 ray = normalized(combat38::aimOf(g, g, combat38::axisHand));
            const float off = 20.f * rnd(), d = 3.f + 12.f * rnd();
            const Vec3 chest = from + turned(ray, off, 6.2832f * rnd()) * d;
            std::array<Vec3, combat38::maxEnemies> feet{};
            feet[0] = chest - Vec3{0, combat38::centreMass, 0};
            const float trueOff = combat38::degreesOff(from, ray, chest);
            const bool want = trueOff <= combat38::grabAssistDegrees - .6f, never = trueOff >= combat38::grabAssistDegrees + 1.1f;
            inCone += want;
            outCone += never;
            for (int a = 0; a < 2; ++a) {
                const int e = combat38::grabAssistTarget(from, ray, feet, 1, a == 1);
                caught[a] += want && e == 0;
                // Past the cone only the capsule itself (EXACT) may take him.
                wrong[a] += never && e == 0 && combat38::grabTarget(from, ray, feet, 1) != 0;
            }
        }
    }
    std::printf("  %u grabs within 12 deg: caught EXACT (38.2) %u -> ASSIST %u; %u past 13 deg: caught off the capsule "
                "%u -> %u\n",
                inCone, caught[0], caught[1], outCone, wrong[0], wrong[1]);
    check(inCone > 50 && caught[1] == inCone && caught[1] > caught[0], "GRAB WEB ASSIST: every enemy within 12 degrees is caught");
    check(wrong[1] == 0, "GRAB WEB ASSIST: none past the cone off the capsule");
}

// [8] Yellow squares: every in-world mark of ours, drawn at the real hands and head of each run, then
// rasterised as the eye sees it (1 mrad pixels over a grey world); a yellow / gold patch whose shape fills
// its own box and is about as wide as tall is a square. The 38.1 "gold squares" were the game's golden
// web style (docs/combat38.md); this checks that none of ours look like it.
struct Raster {
    static constexpr float px = .001f;
    int n = 420;
    std::vector<Vec3> c;
    explicit Raster(int size = 420) : n(size), c(size_t(size) * size, Vec3{.35f, .35f, .37f}) {}
};
bool isYellow(Vec3 c) {
    return c.x > .55f && c.y > .42f && c.z < .35f && c.y > .55f * c.x && c.x - c.z > .3f;
}

void rasterise(Raster& r, const std::vector<Vertex>& v, Vec3 eye, Vec3 at) {
    const Vec3 f = normalized(at - eye), rt = normalized(cross(f, Vec3{0, 1, 0})), up = cross(rt, f);
    const auto proj = [&](Vec3 p, float& x, float& y) {
        const Vec3 d = p - eye;
        const float z = dot(d, f);
        if (z < .05f)
            return false;
        x = dot(d, rt) / z / Raster::px + r.n / 2;
        y = dot(d, up) / z / Raster::px + r.n / 2;
        return true;
    };
    for (size_t i = 0; i + 2 < v.size(); i += 3) {
        float x[3], y[3];
        if (!proj(v[i].position, x[0], y[0]) || !proj(v[i + 1].position, x[1], y[1]) || !proj(v[i + 2].position, x[2], y[2]))
            continue;
        const float area = (x[1] - x[0]) * (y[2] - y[0]) - (x[2] - x[0]) * (y[1] - y[0]);
        if (std::abs(area) < 1e-6f)
            continue;
        const int x0 = std::max(0, int(std::floor(std::min({x[0], x[1], x[2]})))),
                  x1 = std::min(r.n - 1, int(std::ceil(std::max({x[0], x[1], x[2]}))));
        const int y0 = std::max(0, int(std::floor(std::min({y[0], y[1], y[2]})))),
                  y1 = std::min(r.n - 1, int(std::ceil(std::max({y[0], y[1], y[2]}))));
        for (int py = y0; py <= y1; ++py)
            for (int pxl = x0; pxl <= x1; ++pxl) {
                const float sx = pxl + .5f, sy = py + .5f;
                const float w0 = ((x[1] - sx) * (y[2] - sy) - (x[2] - sx) * (y[1] - sy)) / area;
                const float w1 = ((x[2] - sx) * (y[0] - sy) - (x[0] - sx) * (y[2] - sy)) / area;
                const float w2 = 1 - w0 - w1;
                if (w0 < 0 || w1 < 0 || w2 < 0)
                    continue;
                const Vec3 col = v[i].color * w0 + v[i + 1].color * w1 + v[i + 2].color * w2;
                const float a = std::clamp(v[i].alpha * w0 + v[i + 1].alpha * w1 + v[i + 2].alpha * w2, 0.f, 1.f);
                Vec3& d = r.c[py * r.n + pxl];
                d = d * (1 - a) + col * a;
            }
    }
}
// Yellow patches at least `minPx` across that fill >= 85% of their box with an aspect of 0.75..1.33.
int clippedPatches = 0;
int yellowSquares(const Raster& r, int minPx, int& patches) {
    std::vector<int> seen(r.n * r.n, 0);
    int squares = 0;
    patches = 0;
    for (int i = 0; i < r.n * r.n; ++i) {
        if (seen[i] || !isYellow(r.c[i]))
            continue;
        std::vector<int> stack{i};
        seen[i] = 1;
        int count = 0, ax = r.n, bx = -1, ay = r.n, by = -1;
        while (!stack.empty()) {
            const int j = stack.back();
            stack.pop_back();
            ++count;
            const int x = j % r.n, y = j / r.n;
            ax = std::min(ax, x), bx = std::max(bx, x), ay = std::min(ay, y), by = std::max(by, y);
            const int nb[4] = {x > 0 ? j - 1 : -1, x + 1 < r.n ? j + 1 : -1, y > 0 ? j - r.n : -1,
                               y + 1 < r.n ? j + r.n : -1};
            for (const int k : nb)
                if (k >= 0 && !seen[k] && isYellow(r.c[k])) {
                    seen[k] = 1;
                    stack.push_back(k);
                }
        }
        const int w = bx - ax + 1, h = by - ay + 1;
        if (std::max(w, h) < minPx)
            continue;
        ++patches;
        const float aspect = float(w) / float(h), fill = float(count) / float(w * h);
        if (std::getenv("REPLAY_ASCII") && w >= 8 && aspect >= .75f && aspect <= 1.33f && fill >= .85f && ax > 0 && ay > 0 && bx < r.n - 1 && by < r.n - 1)
            for (int yy = by + 3; yy >= ay - 3; --yy) {
                std::string line;
                for (int xx = ax - 6; xx <= bx + 6; ++xx)
                    line += xx < 0 || yy < 0 || xx >= r.n || yy >= r.n ? ' ' : isYellow(r.c[yy * r.n + xx]) ? '#' : r.c[yy * r.n + xx].x + r.c[yy * r.n + xx].y > 1.2f ? '+' : '.';
                std::printf("      %s\n", line.c_str());
            }
        // A square's corners are filled too (a disc or an oval fills ~80% of its box but not its corners).
        const auto corner = [&](int cx, int cy, int dx, int dy) {
            int got = 0;
            for (int i2 = 0; i2 < 2; ++i2)
                for (int j2 = 0; j2 < 2; ++j2)
                    got += isYellow(r.c[(cy + j2 * dy) * r.n + cx + i2 * dx]);
            return got >= 3;
        };
        const bool clipped = ax == 0 || ay == 0 || bx == r.n - 1 || by == r.n - 1;
        clippedPatches += clipped;
        const bool sq = !clipped && aspect >= .75f && aspect <= 1.33f && fill >= .85f && corner(ax, ay, 1, 1) &&
                        corner(bx, ay, -1, 1) && corner(ax, by, 1, -1) && corner(bx, by, -1, -1);
        if (sq && std::getenv("REPLAY_DEBUG"))
            std::printf("      sq %dx%d fill %.2f at %d,%d\n", w, h, fill, ax, ay);
        squares += sq;
    }
    return squares;
}
void yellowMarks() {
    std::printf("\n[8] YELLOW SQUARES: every mark of ours (aim markers x4 kinds x2 hands x2 styles, web ball, impacts x6, "
                "cues x5 + finisher, KO, Spider-Sense x3, gadget wheel) drawn at the real hands/head, rasterised at 1 mrad\n");
    {   // The detector itself: a gold square 2 cm at 1 m (the 38.1 look) must count, a ring of the same gold must not.
        std::vector<Vertex> sq, ring, disc;
        const Vec3 gold{1, .78f, .15f};
        soft::quad(sq, {{-.01f, -.01f, -1}, gold}, {{.01f, -.01f, -1}, gold}, {{.01f, .01f, -1}, gold}, {{-.01f, .01f, -1}, gold});
        soft::softArc(ring, {0, 0, -1}, {1, 0, 0}, {0, 1, 0}, .007f, .01f, 0, 6.2832f, gold, 1, .001f, 32);
        soft::softArc(disc, {0, 0, -1}, {1, 0, 0}, {0, 1, 0}, 0, .01f, 0, 6.2832f, gold, 1, .001f, 32);
        Raster a, b, d;
        rasterise(d, disc, {0, 0, 0}, {0, 0, -1});
        rasterise(a, sq, {0, 0, 0}, {0, 0, -1});
        rasterise(b, ring, {0, 0, 0}, {0, 0, -1});
        int p = 0;
        const int qa = yellowSquares(a, 4, p), qb = yellowSquares(b, 4, p), qd = yellowSquares(d, 4, p);
        std::printf("  detector: a gold square 2 cm at 1 m counts %d, a gold ring %d, a gold disc %d\n", qa, qb, qd);
        check(qa == 1 && qb == 0 && qd == 0, "YELLOW: the detector sees a gold square and not a gold ring or disc");
        clippedPatches = 0;
    }
    int scenes = 0, squares = 0, patches = 0;
    std::vector<std::string> where;
    for (const char* run : runs) {
        const auto rows = load(run);
        int tick = 0;
        replay(rows, [&](const Tick& k) {
            if (!k.headOk || !k.handOk[1] || !k.handOk[0] || ++tick % 2700)
                return;
            const Vec3 eye = k.head.position;
            for (const UiStyle style : {UiStyle::comic, UiStyle::tech}) {
                for (const float dist : {2.f, 6.f, 15.f}) {
                    const Vec3 aim = normalized(combat38::aimOf(k.hand[1], k.hand[1], combat38::axisHand));
                    if (!finite(aim))
                        continue;
                    const Vec3 at = k.hand[1].position + aim * dist;
                    std::vector<std::pair<const char*, std::vector<Vertex>>> marks;
                    for (unsigned hand = 0; hand < 2; ++hand)
                        for (const AimMark kind : {AimMark::anchor, AimMark::air, AimMark::blocked, AimMark::target}) {
                            AimMarker m;
                            m.kind = kind, m.point = at, m.radius = .5f, m.hand = hand, m.spin = float(tick) * .01f;
                            marks.push_back({"aim marker", {}});
                            appendAimMarker(marks.back().second, m, eye, Raster::px, style);
                        }
                    marks.push_back({"web ball", {}});
                    appendWebBall(marks.back().second, at, at - aim * .6f, eye, Raster::px, 1);
                    for (int b = 0; b <= int(BlowKind::Finisher); ++b)
                        for (const float age : {.02f, .1f, .3f, .5f}) {
                            ImpactView v;
                            v.point = at, v.strength = b ? 1.f : .4f, v.kind = BlowKind(b), v.count = 3, v.age = age;
                            marks.push_back({"impact", {}});
                            appendImpact(marks.back().second, v, eye, Raster::px, style);
                        }
                    for (int w = 0; w <= int(CueWay::Down); ++w)
                        for (const bool fin : {false, true}) {
                            CueView v;
                            v.point = at, v.way = CueWay(w), v.finisher = fin, v.left = .6f, v.age = .4f;
                            marks.push_back({"cue", {}});
                            appendCue(marks.back().second, v, eye, Raster::px, style);
                        }
                    for (const float age : {.05f, .3f}) {
                        KnockoutView v;
                        v.point = at, v.age = age;
                        marks.push_back({"KO", {}});
                        appendKnockout(marks.back().second, v, eye, Raster::px, style);
                    }
                    for (const SenseAnswer a : {SenseAnswer::Counter, SenseAnswer::Dodge, SenseAnswer::Jump})
                        for (const float left : {1.f, .05f}) {
                            SenseView v;
                            v.point = at, v.answer = a, v.left = left, v.timed = true, v.age = .5f;
                            marks.push_back({"spider-sense", {}});
                            appendSense(marks.back().second, v, eye, Raster::px, style);
                        }
                    if (dist == 2.f) {
                        GadgetWheelView v;
                        v.open = true, v.age = 1, v.litAge = 1, v.lit = 2, v.equipped = 1;
                        const Vec3 fwd = normalized(at - eye);
                        v.frame.centre = eye + fwd * .5f;
                        v.frame.out = fwd * -1.f;
                        v.frame.right = normalized(cross(fwd, Vec3{0, 1, 0}));
                        v.frame.up = cross(v.frame.right, fwd);
                        for (int i = 0; i < gadgetPlaces; ++i) {
                            v.places[i] = Gadget(1 + i);
                            v.ammo[i] = uint8_t(i % 4), v.most[i] = 3;
                        }
                        marks.push_back({"gadget wheel", {}});
                        appendGadgetWheel(marks.back().second, v, eye, Raster::px, style);
                    }
                    for (const auto& [name, verts] : marks) {
                        if (verts.empty())
                            continue;
                        Vec3 centre{};
                        for (const Vertex& x : verts)
                            centre = centre + x.position;
                        centre = centre / float(verts.size());
                        // Size the image to hold the whole mark (it keeps a readable size up close).
                        const Vec3 fw = normalized(centre - eye), rw = normalized(cross(fw, Vec3{0, 1, 0})), uw = cross(rw, fw);
                        float reach = 0;
                        for (const Vertex& x : verts) {
                            const Vec3 d = x.position - eye;
                            const float z = std::max(dot(d, fw), .05f);
                            reach = std::max({reach, std::abs(dot(d, rw)) / z, std::abs(dot(d, uw)) / z});
                        }
                        Raster r(std::clamp(int(2 * reach / Raster::px) + 24, 64, 3000));
                        rasterise(r, verts, eye, centre);
                        int p = 0;
                        const int clippedBefore = clippedPatches;
                        const int q = yellowSquares(r, 8, p);
                        if (clippedPatches > clippedBefore && std::getenv("REPLAY_DEBUG"))
                            std::printf("      clipped %s %d m\n", name, int(dist));
                        ++scenes;
                        patches += p;
                        squares += q;
                        if (q && std::getenv("REPLAY_DEBUG"))
                            std::printf("      ^ %s %s %d m tick %d\n", name, style == UiStyle::tech ? "tech" : "comic", int(dist), tick);
                        if (q && where.size() < 12)
                            where.push_back(std::string(name) + (style == UiStyle::tech ? " (tech)" : " (comic)") + " at " +
                                            std::to_string(int(dist)) + " m, " + run);
                    }
                }
            }
        });
    }
    std::printf("  %d mark images, %d yellow patches >= 8 px (0.46 deg) (%d cut by the image edge, not judged), yellow squares %d\n",
                scenes, patches, clippedPatches, squares);
    for (const auto& w : where)
        std::printf("    square: %s\n", w.c_str());
    check(scenes > 1000, "YELLOW: the marks were drawn at the real poses");
    check(squares == 0, "YELLOW: none of our marks draws a yellow / gold square");
}
} // namespace

struct FarTable {
    double all0{}, all1{}, left0{}, left1{}, right0{}, right1{}, air0{}, air1{}, far0{}, far1{};
    uint32_t n{};
} g_far;
struct BallTable {
    uint32_t presses{}, one{}, wrong{}, extra{}, edges{}, pulses{};
} g_balls;
struct WristTable {
    uint32_t n{}, ok{};
    double before{}, after{};
} g_wrist;

// ---- 12. Update 38.5 FAR PUNCH: a real throw at an enemy 4-18 m off along its arm --------------
// The throws are the recorded hands' own (head-relative speed over the lunge speed, going out
// from the neck); the enemy is synthesised along that arm's line (0-20 degrees off it) 1.5 s
// before, in sight. Each throw on a fresh machine (the first throw at him), and again after
// a mid-air swing whose web let go 0.25 s before the throw (synthesised: run384's video).
struct Throw {
    double t{};
    unsigned hand{};
    Vec3 arm{}; // neck to fist at the throw
    Vec3 neck{}; // the neck at the throw (the enemy is placed from it: the player moves)
};
std::vector<Throw> throwsOf(const std::vector<Row>& rows) {
    std::vector<Throw> out;
    Vec3 was[2]{};
    bool set[2]{};
    float fastFor[2]{};
    double last[2]{-1e9, -1e9};
    replay(rows, [&](const Tick& k) {
        for (unsigned h = 0; h < 2; ++h) {
            if (!k.handOk[h] || !k.headOk || !k.play) {
                set[h] = false;
                fastFor[h] = 0;
                continue;
            }
            const Vec3 rel = k.hand[h].position - k.head.position;
            if (set[h]) {
                const Vec3 v = (rel - was[h]) / k.dt;
                const Vec3 neck = k.head.position - Vec3{0, .25f, 0};
                const Vec3 arm = k.hand[h].position - neck;
                const bool fast = length(v) >= 2.8f && length(arm) > .2f && dot(normalized(v), normalized(arm)) >= .5f;
                fastFor[h] = fast ? fastFor[h] + k.dt : 0.f;
                if (fastFor[h] >= .05f && k.t - last[h] > 1.0) {
                    last[h] = k.t;
                    out.push_back({k.t, h, normalized(arm), neck});
                }
            }
            was[h] = rel;
            set[h] = true;
        }
    });
    return out;
}
std::string fmt(const char* f, ...) {
    char b[512];
    va_list a;
    va_start(a, f);
    std::vsnprintf(b, sizeof b, f, a);
    va_end(a);
    return b;
}
void farPunch() {
    std::printf("\n[12] FAR PUNCH (Update 38.5): each real throw (>= 2.8 m/s out from the neck) at one enemy 4-18 m "
                "along that arm, 0-20 deg off it (synthesised); 38.4 (Update 38.4's defaults) -> 38.5 (farLunge)\n");
    uint32_t n[2]{}, zips[2][2][2]{}, airN[2]{}, airZips[2][2]{}; // [hand], [version][hand][bands]
    uint32_t nearN = 0, nearZip[2]{}, farN = 0, farZip[2]{};
    for (const char* run : runs) {
        const auto rows = load(run);
        const auto throws = throwsOf(rows);
        uint32_t runN = 0, runZip[2]{};
        for (const auto& th : throws) {
            const float d = 4.f + 14.f * rnd(), off = 20.f * rnd(), roll = 6.2832f * rnd();
            Vec3 chest{};
            bool placed = false;
            for (int air = 0; air < 2; ++air) {
                for (int v = 0; v < 2; ++v) {
                    const auto c = v ? config385() : config384();
                    combat_input::Machine m;
                    bool zipped = false;
                    double early = 0, other = 0;
                    std::string dbg;
                    Vec3 dvLast{};
                    bool dvSet = false;
                    replay(rows, [&](const Tick& k) {
                        auto f = frameOf(k);
                        if (!placed) {
                            chest = th.neck + turned(th.arm, off, roll) * d;
                            placed = true;
                        }
                        // The enemy (fixed in the world: the recording's tracking space).
                        f.enemyCount = 1;
                        f.enemies[0] = chest - Vec3{0, combat38::centreMass, 0};
                        f.enemyIds[0] = 9;
                        f.enemySight[0] = combat_spidy::sightClear;
                        f.enemyMetres = length(f.enemies[0] - f.head);
                        if (air) {
                            f.airborne = true;
                            if (k.t < th.t - .25) { // the swing's web, let go 0.25 s before the throw
                                f.hands[th.hand].free = false;
                                f.hands[th.hand].busy = true;
                                f.hands[th.hand].button = 1.f;
                            }
                        }
                        const auto o = m.update(f, c);
                        zipped |= o.zip && o.lungeHand == int(th.hand) && k.t >= th.t - .2;
                        if (o.zip && !early && k.t < th.t - .2) early = k.t - th.t;
                        {
                            const Vec3 rel = f.hands[th.hand].position - f.head;
                            const Vec3 vel = dvSet ? (rel - dvLast) / std::max(1e-3f, float(k.dt)) : Vec3{};
                            dvLast = rel;
                            dvSet = true;
                            if (std::getenv("FARDBG2") && v && !air && k.t >= th.t - .25 && k.t <= th.t + .1) {
                                const Vec3 nk = f.head - Vec3{0, .25f, 0}, mid = chest;
                                const float sp = length(vel);
                                const Vec3 way = sp > 0 ? vel / sp : Vec3{};
                                dbg += fmt("      %.3f spd %.2f out %.2f point %.2f reach %.2f dist %.1f btn %.2f lock %d\n", k.t - th.t, sp,
                                           dot(way, normalized(f.hands[th.hand].position - nk)), dot(way, normalized(mid - f.hands[th.hand].position)),
                                           dot(normalized(f.hands[th.hand].position - nk), normalized(mid - nk)), length(mid - (f.head - Vec3{0, .3f, 0})),
                                           f.hands[th.hand].button, int(o.lockTarget != 0));
                            }
                        }
                        if (o.zip && o.lungeHand != int(th.hand)) other = k.t - th.t;
                    }, th.t - 1.5, th.t + .35);
                    if (std::getenv("FARDBG") && v && !air && !zipped)
                        std::printf("    MISS %s t %.2f hand %u d %.1f off %.0f early %.2f other %.2f\n%s", run, th.t, th.hand, d, off, early, other, dbg.c_str());
                    if (air)
                        airZips[v][th.hand] += zipped;
                    else {
                        zips[v][th.hand][0] += zipped;
                        runZip[v] += zipped;
                        if (d < 10) {
                            nearZip[v] += zipped;
                        } else {
                            farZip[v] += zipped;
                        }
                    }
                }
            }
            ++n[th.hand];
            ++airN[th.hand];
            ++runN;
            if (d < 10)
                ++nearN;
            else
                ++farN;
        }
        std::printf("  %-7s %3u throws: zipped 38.4 %3u (%3.0f%%) -> 38.5 %3u (%3.0f%%)\n", run, runN, runZip[0],
                    runN ? 100.0 * runZip[0] / runN : 0.0, runZip[1], runN ? 100.0 * runZip[1] / runN : 0.0);
    }
    const auto pc = [](uint32_t a, uint32_t b) { return b ? 100.0 * a / b : 0.0; };
    const uint32_t all = n[0] + n[1];
    std::printf("  ALL: first throw at him zips 38.4 %u/%u (%.0f%%) -> 38.5 %u/%u (%.0f%%)\n", zips[0][0][0] + zips[0][1][0],
                all, pc(zips[0][0][0] + zips[0][1][0], all), zips[1][0][0] + zips[1][1][0], all,
                pc(zips[1][0][0] + zips[1][1][0], all));
    std::printf("  by hand: LEFT 38.4 %.0f%% -> 38.5 %.0f%% (%u) | RIGHT 38.4 %.0f%% -> 38.5 %.0f%% (%u)\n",
                pc(zips[0][0][0], n[0]), pc(zips[1][0][0], n[0]), n[0], pc(zips[0][1][0], n[1]), pc(zips[1][1][0], n[1]), n[1]);
    std::printf("  by distance: 4-10 m 38.4 %.0f%% -> 38.5 %.0f%% (%u) | 10-18 m 38.4 %.0f%% -> 38.5 %.0f%% (%u)\n",
                pc(nearZip[0], nearN), pc(nearZip[1], nearN), nearN, pc(farZip[0], farN), pc(farZip[1], farN), farN);
    std::printf("  mid-air (web let go 0.25 s before): LEFT 38.4 %.0f%% -> 38.5 %.0f%% | RIGHT 38.4 %.0f%% -> 38.5 %.0f%%\n",
                pc(airZips[0][0], airN[0]), pc(airZips[1][0], airN[0]), pc(airZips[0][1], airN[1]),
                pc(airZips[1][1], airN[1]));
    g_far = {pc(zips[0][0][0] + zips[0][1][0], all), pc(zips[1][0][0] + zips[1][1][0], all), pc(zips[0][0][0], n[0]),
             pc(zips[1][0][0], n[0]), pc(zips[0][1][0], n[1]), pc(zips[1][1][0], n[1]),
             pc(airZips[0][0] + airZips[0][1], airN[0] + airN[1]), pc(airZips[1][0] + airZips[1][1], airN[0] + airN[1]),
             pc(farZip[0], farN), pc(farZip[1], farN), all};
    check(all > 100, "FAR PUNCH: over 100 real throws replayed");
    check(pc(zips[1][0][0] + zips[1][1][0], all) >= 90, "FAR PUNCH 38.5: >= 90% of first throws at an enemy 4-18 m zip");
    check(pc(zips[1][0][0], n[0]) >= 85 && pc(zips[1][1][0], n[1]) >= 85, "FAR PUNCH 38.5: each hand >= 85%");
    check(pc(farZip[1], farN) >= 85, "FAR PUNCH 38.5: 10-18 m (38.4 reached 10 m) >= 85%");
    check(pc(airZips[1][0] + airZips[1][1], airN[0] + airN[1]) >= 85, "FAR PUNCH 38.5: mid-air throws >= 85%");
    check(zips[1][0][0] + zips[1][1][0] > zips[0][0][0] + zips[0][1][0], "FAR PUNCH: 38.5 zips more first throws than 38.4");
}

// ---- 13. Update 38.5 FAR PUNCH stays a punch: no zip from arm waving with nobody there ----------
void farNoEnemy() {
    std::printf("\n[13] FAR PUNCH safety: the same throws with the enemy 8 m behind that arm (a zip by that hand from 0.2 s before the throw)\n");
    uint32_t n = 0, bad[2]{};
    for (const char* run : runs) {
        const auto rows = load(run);
        for (const auto& th : throwsOf(rows)) {
            for (int v = 0; v < 2; ++v) {
                combat_input::Machine m;
                const auto c = v ? config385() : config384();
                bool zipped = false;
                replay(rows, [&](const Tick& k) {
                    auto f = frameOf(k);
                    f.enemyCount = 1; // 8 m behind the arm's line
                    f.enemies[0] = th.neck - th.arm * 8.f - Vec3{0, combat38::centreMass, 0};
                    f.enemyIds[0] = 9;
                    f.enemySight[0] = combat_spidy::sightClear;
                    f.enemyMetres = 8;
                    const auto o = m.update(f, c);
                    zipped |= o.zip && o.lungeHand == int(th.hand) && k.t >= th.t - .2;
                }, th.t - 1.5, th.t + .35);
                bad[v] += zipped;
            }
            ++n;
        }
    }
    std::printf("  %u throws away from the enemy: zipped 38.4 %u -> 38.5 %u\n", n, bad[0], bad[1]);
    check(bad[1] * 100 <= n * 2, "FAR PUNCH 38.5: a throw away from the enemy zips <= 2% of the time");
}

// ---- 14. Update 38.5 ONE BALL PER PULL, from the pulled hand -------------------------------------
// The user: exactly one web ball per trigger pull, from the hand pulled - no bursts, no repeat
// while held, no alternating hands, no second ball. Each recorded pull (synthesised on either
// hand in turn, the real poses held still: a ball, not a gadget) as a tap, a 1.5 s hold, a
// rapid 5-pull burst 0.12 s apart, and both hands 0.35 s apart; each pull's RB pulses counted.
struct BallCount {
    uint32_t presses{}, exactlyOne{}, wrongHand{}, extra{}, none{}, rbEdges{}, pulses{};
};
void oneBallRun(const std::vector<Row>& rows, double at, const std::vector<std::pair<double, unsigned>>& pulls,
                double hold, const combat_input::Config& c, BallCount& b, bool enemy) {
    combat_input::Machine m;
    size_t row = 0;
    while (row + 1 < rows.size() && rows[row + 1].t <= at)
        ++row;
    const Row& r = rows[row];
    std::vector<uint32_t> got(pulls.size()), from(pulls.size());
    bool rbWas = false;
    const double end = pulls.back().first + hold + .8;
    for (double t = at; t <= end; t += 1.0 / 90.0) {
        combat_input::Frame f;
        f.seconds = 1.f / 90.f;
        f.play = true;
        f.head = r.head.position;
        f.forward = r.head.orientation.rotate({0, 0, -1});
        for (unsigned h = 0; h < 2; ++h) {
            auto& hh = f.hands[h];
            hh.tracked = true;
            hh.free = true;
            hh.position = r.hand[h].position;
            hh.aim = combat38::aimOf(r.hand[h], r.hand[h], combat38::axisHand);
            hh.palm = r.hand[h].orientation.rotate({0, -1, 0});
            for (const auto& [p, hand] : pulls)
                if (hand == h && t >= p && t < p + hold)
                    hh.button = 1.f;
        }
        if (enemy) {
            f.enemyCount = 1;
            f.enemies[0] = r.hand[1].position + combat38::aimOf(r.hand[1], r.hand[1], combat38::axisHand) * 9.f -
                           Vec3{0, combat38::centreMass, 0};
            f.enemyIds[0] = 5;
            f.enemySight[0] = combat_spidy::sightClear;
            f.enemyMetres = 9;
        }
        const auto o = m.update(f, c);
        const bool rb = (o.buttons & combat_input::padRb) != 0;
        b.rbEdges += rb && !rbWas;
        rbWas = rb;
        for (unsigned h = 0; h < 2; ++h)
            if (o.ballStart[h]) {
                ++b.pulses;
                // the pull it belongs to: that hand's oldest pull (at or before now) with no
                // ball yet, in order; none left: an extra on its latest
                int best = -1, latest = -1;
                for (size_t q = 0; q < pulls.size(); ++q)
                    if (pulls[q].first <= t + 1e-9 && pulls[q].second == h) {
                        latest = int(q);
                        if (best < 0 && got[q] == 0)
                            best = int(q);
                    }
                if (best < 0)
                    best = latest;
                if (best < 0) {
                    ++b.wrongHand; // a ball from a hand with no pull
                    continue;
                }
                ++got[size_t(best)];
                if (o.ballHand != int(h))
                    ++b.wrongHand;
            }
    }
    for (size_t q = 0; q < pulls.size(); ++q) {
        ++b.presses;
        b.exactlyOne += got[q] == 1;
        b.extra += got[q] > 1 ? got[q] - 1 : 0;
        b.none += got[q] == 0;
    }
}
void oneBall() {
    std::printf("\n[14] ONE BALL PER PULL (Update 38.5): RB pulses per trigger pull and their hand, at each recorded "
                "pull's real (still) poses\n");
    const char* kinds[] = {"tap 0.1 s", "hold 1.5 s", "5 pulls 0.12 s apart", "left then right 0.35 s",
                           "right then left 0.35 s", "tap, enemy (assisted)"};
    BallCount k[6];
    unsigned flip = 0;
    for (const char* run : runs) {
        const auto rows = load(run);
        uint32_t used = 0;
        for (size_t i = 1; i < rows.size() && used < 60; ++i) {
            if (!(rows[i].presses > rows[i - 1].presses) || !rows[i].handOk[0] || !rows[i].handOk[1] || !rows[i].headOk)
                continue;
            ++used;
            const double t = rows[i].t;
            const unsigned h = flip++ & 1;
            const auto c = config385();
            oneBallRun(rows, t, {{t + .1, h}}, .1, c, k[0], false);
            oneBallRun(rows, t, {{t + .1, h}}, 1.5, c, k[1], false);
            std::vector<std::pair<double, unsigned>> burst;
            for (int q = 0; q < 5; ++q)
                burst.push_back({t + .1 + .12 * q, h});
            oneBallRun(rows, t, burst, .06, c, k[2], false);
            oneBallRun(rows, t, {{t + .1, 0u}, {t + .45, 1u}}, .1, c, k[3], false);
            oneBallRun(rows, t, {{t + .1, 1u}, {t + .45, 0u}}, .1, c, k[4], false);
            oneBallRun(rows, t, {{t + .1, h}}, .1, c, k[5], true);
        }
    }
    uint32_t presses = 0, one = 0, wrong = 0, extra = 0, none = 0, pulses = 0, edges = 0;
    for (int q = 0; q < 6; ++q) {
        std::printf("  %-24s pulls %4u: exactly one ball %4u (%5.1f%%), extra %u, none %u, other hand %u; RB edges %u\n",
                    kinds[q], k[q].presses, k[q].exactlyOne, k[q].presses ? 100.0 * k[q].exactlyOne / k[q].presses : 0.0,
                    k[q].extra, k[q].none, k[q].wrongHand, k[q].rbEdges);
        presses += k[q].presses;
        one += k[q].exactlyOne;
        wrong += k[q].wrongHand;
        extra += k[q].extra;
        none += k[q].none;
        pulses += k[q].pulses;
        edges += k[q].rbEdges;
    }
    g_balls = {presses, one, wrong, extra, edges, pulses};
    check(presses > 500 && one == presses, "ONE BALL: every pull fires exactly one ball (100%)");
    check(wrong == 0, "ONE BALL: every ball is the pulled hand's (100%)");
    check(edges == pulses, "ONE BALL: one RB press per ball, never a held RB spraying");
}

// ---- 15. Update 38.5 BALL FROM THE WRIST: the muzzle rule (wrist_muzzle::decide) ---------------
void wristMuzzle() {
    std::printf("\n[15] BALL WRIST (Update 38.5): the web shooter's muzzle moved to the pulled hand's wrist (the "
                "rule the hook runs), at each recorded pull; the game's spot synthesised 0.3-1.2 m off (run384's "
                "\"top left / top right of the view\")\n");
    namespace wm = spidy::wrist_muzzle;
    uint32_t n = 0, moved = 0, wristOk = 0, otherCalls = 0, otherMoved = 0;
    double before = 0, after = 0;
    for (const char* run : runs) {
        const auto rows = load(run);
        for (size_t i = 1; i < rows.size(); ++i) {
            if (!(rows[i].presses > rows[i - 1].presses) || !rows[i].handOk[0] || !rows[i].handOk[1])
                continue;
            const unsigned h = unsigned(i & 1);
            wm::Wrist w[2];
            for (unsigned q = 0; q < 2; ++q)
                w[q] = {webShooter(rows[i].hand[q], q).position, 10.f, true};
            const Vec3 game = w[h].position + turned({0, 1, 0}, 90.f * rnd(), 6.2832f * rnd()) * (.3f + .9f * rnd());
            wm::Shot shot{int(h), 10.f};
            Vec3 out{};
            const auto v = wm::decide(shot, w, 10.05f, game, out);
            ++n;
            moved += v == wm::moved;
            wristOk += length(out - w[h].position) < 1e-4f;
            before += length(game - w[h].position);
            after += length(out - w[h].position);
            // A web-shooter call long after the press (a swing web, the game's own) is left alone.
            ++otherCalls;
            otherMoved += wm::decide(shot, w, 10.f + wm::window + .2f, game, out) == wm::moved;
        }
    }
    std::printf("  %u balls: moved %u; at the pulled wrist %u; mean distance from the wrist %.2f m -> %.3f m; calls "
                "outside the press window moved %u of %u\n",
                n, moved, wristOk, n ? before / n : 0.0, n ? after / n : 0.0, otherMoved, otherCalls);
    g_wrist = {n, wristOk, n ? before / n : 0.0, n ? after / n : 0.0};
    check(n > 200 && wristOk == n, "BALL WRIST: every ball starts at the pulled hand's web shooter");
    check(otherMoved == 0, "BALL WRIST: no web-shooter call outside the press window is touched");
    Vec3 out{};
    const wm::Wrist far[2]{{{0, 0, 0}, 0, true}, {{0, 0, 0}, 0, true}};
    check(wm::decide({0, 0}, far, .01f, {5, 0, 0}, out) == wm::tooFar && out.x == 5,
          "BALL WRIST: a muzzle 5 m from the wrist (not this ball) is left alone");
    const wm::Wrist old[2]{{{0, 0, 0}, -1, true}, {{0, 0, 0}, -1, true}};
    check(wm::decide({0, 0}, old, .01f, {.5f, 0, 0}, out) == wm::stale && out.x == .5f,
          "BALL WRIST: a wrist not published for 0.25 s is not used");
}

// ---- Update 38.5 PROMPT HANDLING AUTO never latches in combat ---------------------------------------
// Worst case: AUTO forced on (as if SM2's prompts were hooked) and a pad-needing prompt asked for on
// EVERY frame. A fight is any time within 2 s of a recorded X (the game's melee) press; game_xr passes
// allowed = playing && !inFight (the combat camera). Not one frame may be latched in a fight.
void promptsInCombat() {
    std::printf("\n[38.5 PROMPTS] PROMPT HANDLING AUTO in the recorded fights (forced on, a prompt every frame)\n");
    uint32_t allFight = 0, allLatched = 0, allCalm = 0;
    for (const char* run : runs) {
        const auto rows = load(run);
        std::vector<double> xs;
        for (size_t i = 1; i < rows.size(); ++i)
            if ((rows[i].pad & 0x4000) && !(rows[i - 1].pad & 0x4000))
                xs.push_back(rows[i].t);
        game_pad::PadLayer layer;
        uint32_t fight = 0, latchedInFight = 0, calmLatched = 0;
        replay(rows, [&](const Tick& k) {
            const auto it = std::upper_bound(xs.begin(), xs.end(), k.t);
            const bool inFight = it != xs.begin() && k.t - *(it - 1) < 2.0;
            layer.update(k.dt, k.play, false, false, false);
            layer.prompt(k.dt, true, k.play && !inFight, true);
            if (inFight && k.play) {
                ++fight;
                latchedInFight += layer.latched();
            } else if (k.play) {
                calmLatched += layer.latched();
            }
        });
        std::printf("  %-7s fight frames %6u: latched %u; calm frames latched %u\n", run, fight, latchedInFight, calmLatched);
        allFight += fight;
        allLatched += latchedInFight;
        allCalm += calmLatched;
    }
    check(allFight > 1000 && allLatched == 0, "PROMPTS: AUTO never latches the pad in a recorded fight");
    check(allCalm > 0, "PROMPTS: the same worst case does latch outside fights (the test can fail)");
}

// The table the summary prints (Update 38.5's DIAGNOSIS.md takes it from there).
void summary() {
    std::printf("\n[38.5 SUMMARY] far punch first-throw 38.4 %.0f%% -> 38.5 %.0f%% (%u throws); left %.0f%% -> %.0f%%; "
                "right %.0f%% -> %.0f%%; mid-air %.0f%% -> %.0f%%; 10-18 m %.0f%% -> %.0f%%\n",
                g_far.all0, g_far.all1, g_far.n, g_far.left0, g_far.left1, g_far.right0, g_far.right1, g_far.air0,
                g_far.air1, g_far.far0, g_far.far1);
    std::printf("[38.5 SUMMARY] balls per pull: %u pulls, exactly one %u, other hand %u, extra %u, RB edges %u / "
                "pulses %u\n", g_balls.presses, g_balls.one, g_balls.wrong, g_balls.extra, g_balls.edges, g_balls.pulses);
    std::printf("[38.5 SUMMARY] ball origin: %u balls at the pulled wrist of %u; %.2f m -> %.3f m from it\n",
                g_wrist.ok, g_wrist.n, g_wrist.before, g_wrist.after);
}

// ---- Version 0.0.3 ROPE GRAB: the real hands hold a tether (synthesised enemy, no log has enemies) ----
// At each recorded right-grip press: an enemy 3-9 m down the hand's aim; held 2 s of the real motion.
// A release by fling or slam needs the real pole end to move that fast; nothing goes non-finite.
// ---- P. 0.0.3 PUNCHES REGISTER: rep0023 ("a lot of punches just don't land") ----------------------
// 0.0.2's PUNCH ONLY spacer, as shipped: a blow waiting over 0.25 s for its slot was dropped, one
// asked during a tap was ignored.
struct Spacer002 {
    float from = -1e9f, until = -1e9f, askedAt{};
    bool waiting{};
    uint32_t taps{}, dropped{}, ignored{};
    void update(float now, bool want) {
        const bool tapping = now >= from && now < until;
        if (want && (waiting || tapping))
            ++ignored;
        if (want && !waiting && !tapping) {
            waiting = true;
            askedAt = now;
        }
        if (waiting) {
            if (now >= until + combat38::meleeSpacing) {
                waiting = false;
                from = now;
                until = now + combat38::meleeTapSeconds;
                ++taps;
            } else if (now - askedAt > .25f) {
                waiting = false;
                ++dropped;
            }
        }
    }
};
void punchesRegister() {
    std::printf("\n[PR] 0.0.3 PUNCHES REGISTER: rep0023's real fists against an enemy ahead (synthesised: the log has "
                "no enemy positions), 0.0.2's defaults\n");
    const auto rows = load("rep0023");
    check(rows.size() > 5000, "rep0023: tests/replay/rep0023.csv is there");
    uint32_t recordedPad = 0, recordedOwn = 0;
    for (size_t i = 1; i < rows.size(); ++i)
        recordedPad += (rows[i].pad & 0x4000) && !(rows[i - 1].pad & 0x4000);
    (void)recordedOwn;
    for (const float metres : {1.2f, 3.4f}) {
        const auto c = config385();
        combat_input::Machine m;
        Spacer002 old;
        Pulses x;
        uint32_t blows = 0, attacks = 0;
        replay(rows, [&](const Tick& k) {
            auto f = frameOf(k);
            const Vec3 ahead = combat_input::level(f.forward);
            f.enemies[0] = f.head + ahead * metres + Vec3{0, -1.65f, 0};
            f.enemyIds[0] = 7;
            f.enemyCount = 1;
            f.enemyMetres = metres;
            f.enemySight[0] = combat_spidy::sightClear;
            const auto o = m.update(f, c);
            const bool blow = !o.zip && (o.punch[0] || o.punch[1]);
            blows += blow;
            attacks += o.zip || o.punch[0] || o.punch[1];
            old.update(static_cast<float>(k.t - rows.front().t), blow);
            x.add(o.buttons & combat_input::padX, k.t);
        });
        const auto& sp = m.melee();
        std::printf("  enemy %.1f m: %u attacks (%u blows for X, the rest lunges or zips); 0.0.2's spacer: %u X, %u dropped, "
                    "%u asked mid-tap or waiting (%.0f%% of the blows never became an X) | 0.0.3: %llu X, %llu rode a "
                    "waiting tap, %llu dropped, %u chained (< %.2f s), min gap %.2f s; held off %llu (%llu enemy past "
                    "reach)\n",
                    static_cast<double>(metres), attacks, blows, old.taps, old.dropped, old.ignored,
                    blows ? 100.0 * double(blows - old.taps) / double(blows) : 0.0,
                    static_cast<unsigned long long>(sp.taps()), static_cast<unsigned long long>(sp.merged()),
                    static_cast<unsigned long long>(sp.dropped()), x.chained, static_cast<double>(combat38::meleeSpacing),
                    x.minGap < 1e8 ? x.minGap : 0.0, static_cast<unsigned long long>(m.targetedHeldOff()),
                    static_cast<unsigned long long>(m.heldOffFar()));
        check(sp.dropped() == 0, "rep0023: 0.0.3 drops no blow");
        check(sp.taps() + sp.merged() == blows, "rep0023: every blow is an X or rides one (none lost silently)");
        check(x.chained == 0, "rep0023: no two X inside the combo window (no kicks)");
        check(attacks > 0 && (metres > 2 || sp.taps() > 0), "rep0023: the fists attack the enemy ahead (X near, the lunge far)");
        if (metres < 2) {
            check(old.dropped > 0, "rep0023: 0.0.2's spacer drops blows on the same fists (the bug, reproduced)");
            check(sp.taps() >= old.taps, "rep0023: 0.0.3 sends at least 0.0.2's X");
        } else {
            check(m.targetedHeldOff() == 0, "rep0023: no swing at an enemy 3.4 m ahead is held off (the far lunge takes it)");
        }
    }
    std::printf("  recorded pad X presses in rep0023 (the mod's taps plus the controller's own X): %u\n", recordedPad);
}


// ---- P4. Version 0.0.4 EVERY PUNCH REGISTERS: rep0030 + rep0023 ("hardly any register ... i punch
// fast it needs to register every single hit") -----------------------------------------------------
// The real fists against an enemy 1.2 m ahead (synthesised: the logs have no enemy positions).
// A thrown fist (the ground truth, independent of the Machine): a hand going over punchSpeed
// toward the enemy (0.45 of its speed forward), counted again only once it slowed under the
// swing speed or came back. 0.0.3 (SPACED + SpidyVR re-arm) against 0.0.4 (EVERY HIT + quick
// re-arm): blows asked, X taps, blows that rode another blow's tap, and the delay fist -> X.
struct FistCount {
    bool open[2]{}, seen[2]{};
    Vec3 prev[2]{};
    uint32_t thrown{};
    void add(const combat_input::Frame& f, float dt, float punchSpeed, float share = .45f) {
        const Vec3 ahead = combat_input::level(f.enemies[0] + Vec3{0, 1.1f, 0} - f.head);
        for (unsigned i = 0; i < 2; ++i) {
            const Vec3 rel = f.hands[i].position - f.head;
            const bool ok = f.hands[i].tracked && f.play && dt > 0;
            const Vec3 v = ok && seen[i] ? (rel - prev[i]) * (1.f / dt) : Vec3{};
            seen[i] = ok;
            prev[i] = rel;
            const float sp = length(v);
            const Vec3 out0 = combat_input::level(rel);
            const bool back = length(out0) > .05f && dot(v, normalized(out0)) <= -combat_input::Config::pullBack;
            if (open[i] && (sp < .6f * punchSpeed || back || !ok))
                open[i] = false;
            if (!open[i] && ok && sp >= punchSpeed && sp < 20.f && length(ahead) > .5f &&
                dot(v, normalized(ahead)) >= share * sp && dot(rel, normalized(ahead)) >= .2f) {
                open[i] = true;
                ++thrown;
            }
        }
    }
};
struct EveryHitRun {
    uint32_t thrown{}, blows{}, taps{}, merged{}, closeBlows{};
    double meanDelay{}, worstDelay{};
    uint64_t rearmRejects{}, slowRejects{};
};
EveryHitRun everyHitRun(const std::vector<Row>& rows, bool v004, float speed = 0, int feel = -1, float truthShare = .45f,
                        float truthSpeedSet = 0) {
    auto c = config385();
    const float truthSpeed = truthSpeedSet > 0 ? truthSpeedSet : c.punchSpeed; // the ground truth keeps 2.4 m/s
    if (speed > 0)
        c.punchSpeed = speed;
    c.meleeEveryHit = v004;
    c.meleePunchOnly = !v004;
    c.quickRearm = v004;
    c.closeJabs = v004;
    if (feel >= 0)
        combat_input::applyPunchFeel(c, static_cast<uint32_t>(feel));
    combat_input::Machine m;
    FistCount fists;
    Pulses x;
    EveryHitRun r;
    std::vector<double> blowAt, tapAt;
    replay(rows, [&](const Tick& k) {
        auto f = frameOf(k);
        const Vec3 ahead = combat_input::level(f.forward);
        f.enemies[0] = f.head + ahead * 1.2f + Vec3{0, -1.65f, 0};
        f.enemyIds[0] = 7;
        f.enemyCount = 1;
        f.enemyMetres = 1.2f;
        f.enemySight[0] = combat_spidy::sightClear;
        const auto o = m.update(f, c);
        fists.add(f, k.dt, truthSpeed, truthShare);
        if (!o.zip && (o.punch[0] || o.punch[1])) {
            ++r.blows;
            blowAt.push_back(k.t);
        }
        const bool xNow = o.buttons & combat_input::padX;
        if (xNow && !x.was)
            tapAt.push_back(k.t);
        x.add(xNow, k.t);
    });
    r.thrown = fists.thrown;
    r.taps = x.edges;
    r.rearmRejects = m.swingRejects(combat38::cooldown);
    r.slowRejects = m.swingRejects(combat38::tooSlow);
    // Each blow's X: the first tap at or after it not taken by an earlier blow; a blow whose
    // tap is someone else's (none left) rode it.
    size_t j = 0;
    double sum = 0;
    uint32_t n = 0;
    for (size_t b = 0; b < blowAt.size(); ++b) {
        while (j < tapAt.size() && tapAt[j] < blowAt[b] - 1e-6)
            ++j;
        if (j >= tapAt.size() || (b + 1 < blowAt.size() && tapAt[j] > blowAt[b + 1] + .2)) {
            ++r.merged;
            continue;
        }
        const double d = tapAt[j] - blowAt[b];
        sum += d;
        ++n;
        r.worstDelay = std::max(r.worstDelay, d);
        ++j;
        if (b > 0 && blowAt[b] - blowAt[b - 1] < .3)
            ++r.closeBlows;
    }
    r.merged = static_cast<uint32_t>(blowAt.size() > tapAt.size() ? blowAt.size() - tapAt.size() : 0);
    r.meanDelay = n ? sum / n : 0;
    if (v004) {
        const auto& t = m.everyHit();
        r.merged = static_cast<uint32_t>(t.merged());
        r.worstDelay = t.worstDelay();
        r.meanDelay = t.meanDelay();
    } else {
        r.merged = static_cast<uint32_t>(m.melee().merged());
        r.worstDelay = m.melee().worstDelay();
        r.meanDelay = m.melee().meanDelay();
    }
    return r;
}
void everyHit() {
    std::printf("\n[P4] 0.0.4 EVERY PUNCH REGISTERS: rep0030 and rep0023's real fists, an enemy 1.2 m ahead\n");
    for (const char* run : {"rep0030", "rep0023"}) {
        const auto rows = load(run);
        check(rows.size() > 5000, "EVERY HIT: the run's csv is there");
        const auto a = everyHitRun(rows, false), b = everyHitRun(rows, true);
        std::printf("  %s fists thrown %u | 0.0.3 SPACED: %u blows, %u X, %u rode another's X, delay mean %.0f ms worst %.0f "
                    "ms, %llu swings refused not re-armed | 0.0.4 EVERY HIT: %u blows, %u X, %u rode, delay mean %.0f ms "
                    "worst %.0f ms, %llu refused not re-armed\n",
                    run, b.thrown, a.blows, a.taps, a.merged, a.meanDelay * 1000, a.worstDelay * 1000,
                    static_cast<unsigned long long>(a.rearmRejects), b.blows, b.taps, b.merged, b.meanDelay * 1000,
                    b.worstDelay * 1000, static_cast<unsigned long long>(b.rearmRejects));
        check(b.merged == 0, "EVERY HIT: no blow rides another blow's X");
        check(b.taps == b.blows, "EVERY HIT: one X tap per blow");
        check(b.worstDelay <= combat38::tapDown + combat38::tapUp + .02, "EVERY HIT: no blow waits longer than one tap");
        check(b.meanDelay <= .1, "EVERY HIT: the mean fist-to-X delay is under 100 ms");
        check(b.blows >= a.blows, "EVERY HIT: at least 0.0.3's blows");
        check(b.blows * 100 >= b.thrown * 90, "EVERY HIT: 90% or more of the thrown fists are blows");
        check(a.merged > 0, "EVERY HIT: 0.0.3's spacer merges blows on the same fists (the complaint, reproduced)");
    }
    // CLOSE JABS never wake TARGETED with no enemy in reach, nor a menu: waving sends nothing.
    for (const char* run : {"rep0030", "rep0023", "run381", "run38"}) {
        const auto rows = load(run);
        auto c = config385();
        c.meleeEveryHit = c.quickRearm = c.closeJabs = true;
        c.meleePunchOnly = false;
        combat_input::Machine idle, menu;
        Pulses none, open;
        replay(rows, [&](const Tick& k) {
            auto f = frameOf(k);
            none.add(idle.update(f, c).buttons & combat_input::padX, k.t);
            f.enemies[0] = f.head + combat_input::level(f.forward) * 1.2f + Vec3{0, -1.65f, 0};
            f.enemyIds[0] = 7;
            f.enemyCount = 1;
            f.enemyMetres = 1.2f;
            f.menuOpen = true;
            open.add(menu.update(f, c).buttons & combat_input::padX, k.t);
        });
        std::printf("  %s 0.0.4 waving: no enemy X %u, menu open X %u\n", run, none.edges, open.edges);
        check(open.edges == 0, "EVERY HIT: no X with a menu open");
        if (std::string(run).rfind("run", 0) == 0)
            check(none.edges == 0, "EVERY HIT + CLOSE JABS: arm waving with no enemy sends no X");
    }
    // Synthetic: blows 0.15 s apart, 40 of them: 40 taps, none rides another, each within 0.13 s.
    combat38::PunchTaps t;
    uint32_t edges = 0;
    bool was = false;
    for (int k = 0; k < 90 * 8; ++k) {
        const float now = k / 90.f;
        const bool want = k % 14 == 0 && k < 14 * 40; // every 0.156 s
        const bool x = t.update(now, want);
        edges += x && !was;
        was = x;
    }
    check(edges == 40 && t.merged() == 0 && t.worstDelay() <= .14f,
          "EVERY HIT: 40 blows 0.156 s apart are 40 X taps, none merged, none waits over 0.14 s");
}

// ---- P5. Version 0.0.5 PUNCH SENSITIVITY (the 0.0.4 headset test: "punches still don't register
// enough") ---------------------------------------------------------------------------------------
// (1) rep0030 and rep0023's real fists, an enemy 1.2 m ahead, against a SLOPPY ground truth (a hand
// over 1.7 m/s, 0.3 of it toward him): FORGIVING (the default) against 0.0.4. (2) Synthetic fists:
// fast, slow, angled, uppercut, a flurry with partial pull-backs: each exactly one blow and one X.
// (3) Synthetic everyday motions with the enemy right there: none is a punch.
struct Motion {
    const char* name;
    int hand;
    std::vector<std::pair<Vec3, float>> path; // offsets from the hand's rest, each reached in t s
    uint32_t want;                              // blows it must make
};
uint32_t runMotion(const Motion& m, uint32_t feel, uint32_t& taps, float& worstDelay) {
    auto c = config385();
    c.meleeEveryHit = c.quickRearm = c.closeJabs = true;
    c.meleePunchOnly = false;
    combat_input::applyPunchFeel(c, feel);
    combat_input::Machine mc;
    const Vec3 head{0, 1.65f, 0};
    const Vec3 rest[2] = {{-.2f, 1.35f, -.22f}, {.2f, 1.35f, -.22f}};
    const float dt = 1.f / 90.f;
    Vec3 at = {};
    std::vector<std::pair<Vec3, float>> path = m.path;
    path.insert(path.begin(), {Vec3{}, .5f}); // still first
    path.push_back({Vec3{}, .6f});            // back to rest, still after
    uint32_t blows = 0;
    taps = 0;
    worstDelay = 0;
    bool was = false;
    double t = 0, lastBlow = -1;
    for (const auto& [to, sec] : path) {
        const Vec3 from = at;
        const int n = std::max(1, static_cast<int>(sec / dt + .5f));
        for (int k = 1; k <= n; ++k) {
            const float u = static_cast<float>(k) / static_cast<float>(n), e = u * u * (3 - 2 * u);
            at = from + (to - from) * e;
            combat_input::Frame f;
            f.seconds = dt;
            f.play = true;
            f.head = head;
            f.forward = {0, 0, -1};
            for (int i = 0; i < 2; ++i) {
                auto& h = f.hands[static_cast<size_t>(i)];
                h.tracked = h.free = true;
                h.position = rest[i] + (i == m.hand ? at : Vec3{});
                h.aim = {0, 0, -1};
                h.palm = {0, -1, 0};
            }
            f.enemies[0] = {0, 0, -1.2f};
            f.enemyIds[0] = 7;
            f.enemyCount = 1;
            f.enemyMetres = 1.2f;
            f.enemySight[0] = combat_spidy::sightClear;
            const auto o = mc.update(f, c);
            t += dt;
            if (std::getenv("P5DBG2") && o.candidate[m.hand] && std::strstr(m.name, std::getenv("P5DBG2")))
                std::printf("    cand t=%.3f z=%.3f speed=%.2f reject=%u punch=%d\n", t, at.z, o.speed[m.hand], o.reject[m.hand],
                            (int)o.punch[m.hand]);
            if (!o.zip && (o.punch[0] || o.punch[1])) {
                ++blows;
                lastBlow = t;
                if (std::getenv("P5DBG"))
                    std::printf("    dbg %s t=%.3f at=(%.2f %.2f %.2f) contact=%d speed=%.2f\n", m.name, t, at.x, at.y, at.z, (int)o.contact[m.hand],
                                o.speed[m.hand]);
            }
            const bool x = o.buttons & combat_input::padX;
            if (x && !was) {
                ++taps;
                if (lastBlow >= 0)
                    worstDelay = std::max(worstDelay, static_cast<float>(t - lastBlow));
            }
            was = x;
        }
    }
    return blows;
}
void punchSense() {
    std::printf("\n[P5] 0.0.5 PUNCH SENSITIVITY: FORGIVING (default) against 0.0.4, real and synthetic fists\n");
    check(combat38::punchFeelOf(0).speedK < 1 && combat38::punchFeelOf(0).forwardK < 1 &&
              combat38::punchFeelOf(0).backSpeed < combat_input::Config::pullBack,
          "PUNCH SENSITIVITY: FORGIVING (the default) is more forgiving than 0.0.4 (speed, angle, re-arm)");
    for (const char* run : {"rep0030", "rep0023"}) {
        const auto rows = load(run);
        const auto old = everyHitRun(rows, true, 0, 2, .3f, 1.6f), now = everyHitRun(rows, true, 0, 0, .3f, 1.6f);
        std::printf("  %s sloppy fists thrown %u | 0.0.4: %u blows (%u%%), %u X | 0.0.5 FORGIVING: %u blows (%u%%), %u X, "
                    "%u rode, delay mean %.0f ms worst %.0f ms\n",
                    run, now.thrown, old.blows, old.thrown ? old.blows * 100 / old.thrown : 0, old.taps, now.blows,
                    now.thrown ? now.blows * 100 / now.thrown : 0, now.taps, now.merged, now.meanDelay * 1000,
                    now.worstDelay * 1000);
        check(now.blows > old.blows, "PUNCH SENSITIVITY: FORGIVING lands more of the real fists than 0.0.4");
        check(now.blows * 100 >= now.thrown * 92, "PUNCH SENSITIVITY: 92% or more of the sloppy thrown fists are blows");
        // Counted twice? Against every forward fist over 1.2 m/s (the slowest FORGIVING takes).
        const auto any = everyHitRun(rows, true, 0, 0, .3f, 1.2f);
        std::printf("  %s every forward fist over 1.2 m/s: %u | FORGIVING blows %u\n", run, any.thrown, any.blows);
        check(any.blows * 100 <= any.thrown * 105, "PUNCH SENSITIVITY: no fist counts twice (blows at most 105% of all fists)");
        check(now.taps == now.blows && now.merged == 0, "PUNCH SENSITIVITY: one X tap per blow, none rides another");
        check(now.worstDelay <= combat38::tapDown + combat38::tapUp + .02, "PUNCH SENSITIVITY: no blow waits longer than one tap");
    }
    const Vec3 F{0, 0, -1};
    auto dir = [](float yawDeg, float pitchDeg) {
        const float y = yawDeg * 3.14159265f / 180, p = pitchDeg * 3.14159265f / 180;
        return Vec3{std::sin(y) * std::cos(p), std::sin(p), -std::cos(y) * std::cos(p)};
    };
    // smoothstep: peak speed 1.5 x distance / time
    const std::vector<Motion> punches = {
        {"fast jab 3.5 m/s", 1, {{F * .35f, .15f}, {Vec3{}, .3f}}, 1},
        {"slow jab 1.6 m/s", 1, {{F * .32f, .3f}, {Vec3{}, .35f}}, 1},
        {"lazy jab 1.75 m/s, 45 degrees aside", 1, {{dir(45, -5) * .3f, .26f}, {Vec3{}, .35f}}, 1},
        {"short tired jab 1.7 m/s", 0, {{dir(-20, 10) * .2f, .18f}, {Vec3{}, .3f}}, 1},
        {"sloppy jab 40 degrees aside", 0, {{dir(-40, 0) * .35f, .22f}, {Vec3{}, .3f}}, 1},
        {"rising jab 35 degrees up", 1, {{dir(10, 35) * .35f, .2f}, {Vec3{}, .3f}}, 1},
        {"uppercut", 1, {{Vec3{0, -.12f, .02f}, .25f}, {Vec3{0, .25f, -.22f}, .17f}, {Vec3{}, .3f}}, 1},
        {"re-punch after a 10 cm pull-back", 1, {{F * .32f, .16f}, {F * .22f, .09f}, {F * .36f, .12f}, {Vec3{}, .3f}}, 2},
        {"flurry: 5 jabs, 8 cm pull-backs", 1,
         {{F * .3f, .13f}, {F * .22f, .09f}, {F * .32f, .1f}, {F * .24f, .09f}, {F * .34f, .1f}, {F * .26f, .09f},
          {F * .36f, .1f}, {F * .28f, .09f}, {F * .38f, .1f}, {Vec3{}, .3f}},
         5},
    };
    const std::vector<Motion> everyday = {
        {"side-to-side wave 2.5 m/s", 1, {{Vec3{.3f, 0, 0}, .18f}, {Vec3{-.3f, 0, 0}, .36f}, {Vec3{.3f, 0, 0}, .36f}, {Vec3{}, .2f}}, 0},
        {"hand dropped to the side", 1, {{Vec3{.12f, -.5f, .05f}, .25f}, {Vec3{}, .7f}}, 0},
        {"pull back to the chest", 1, {{F * .3f, .8f}, {Vec3{0, 0, .2f}, .2f}, {Vec3{}, .6f}}, 0},
        {"raise overhead", 1, {{Vec3{0, .55f, .05f}, .3f}, {Vec3{}, .6f}}, 0},
        {"slow reach out 0.8 m/s", 1, {{F * .35f, .65f}, {Vec3{}, .8f}}, 0},
        {"walking arm swing", 0,
         {{Vec3{0, -.45f, 0}, .6f}, {Vec3{0, -.45f, -.12f}, .25f}, {Vec3{0, -.45f, .12f}, .5f}, {Vec3{0, -.45f, -.12f}, .5f},
          {Vec3{0, -.45f, .12f}, .5f}, {Vec3{}, .6f}},
         0},
        {"scratch the head", 1, {{Vec3{0, .32f, .2f}, .4f}, {Vec3{}, .5f}}, 0},
    };
    uint32_t okP = 0, ok404 = 0, okE = 0;
    for (const auto& m : punches) {
        uint32_t taps = 0, taps4 = 0;
        float worst = 0, worst4 = 0;
        const uint32_t b = runMotion(m, 0, taps, worst), b4 = runMotion(m, 2, taps4, worst4);
        std::printf("  punch %-40s FORGIVING %u blow(s) %u X (worst %.0f ms) | 0.0.4 %u\n", m.name, b, taps, worst * 1000, b4);
        okP += b == m.want && taps == b && worst <= .15f;
        ok404 += b4 == m.want;
        char what[160];
        std::snprintf(what, sizeof what, "PUNCH SENSITIVITY: %s is %u punch(es), one X each, at once", m.name, m.want);
        check(b == m.want && taps == b && worst <= .15f, what);
    }
    for (const auto& m : everyday)
        for (uint32_t feel : {0u, 1u}) {
            // MAX takes any forward hand over about 1.1 m/s: a hand swung forward from the head can count there
            if (feel == 1 && std::strstr(m.name, "head")) {
                uint32_t taps = 0;
                float worst = 0;
                std::printf("  everyday %-34s MAX: %u blow(s) (shown only: MAX takes any forward hand)\n", m.name,
                            runMotion(m, 1, taps, worst));
                ++okE;
                continue;
            }
            uint32_t taps = 0;
            float worst = 0;
            const uint32_t b = runMotion(m, feel, taps, worst);
            std::printf("  everyday %-34s %s: %u blow(s) %u X\n", m.name, feel ? "MAX" : "FORGIVING", b, taps);
            okE += b == 0 && taps == 0;
            char what[160];
            std::snprintf(what, sizeof what, "PUNCH SENSITIVITY %s: %s (enemy 1.2 m ahead) sends no X", feel ? "MAX" : "FORGIVING", m.name);
            check(b == 0 && taps == 0, what);
        }
    std::printf("  PUNCH SENSITIVITY: %u/%zu punches land (0.0.4: %u/%zu), %u/%zu everyday motions send nothing\n", okP,
                punches.size(), ok404, punches.size(), okE, everyday.size() * 2);
    // With no enemy, or a menu open, FORGIVING still sends nothing for the recorded waving.
    for (const char* run : {"run381", "run38"}) {
        const auto rows = load(run);
        auto c = config385();
        c.meleeEveryHit = c.quickRearm = c.closeJabs = true;
        c.meleePunchOnly = false;
        combat_input::applyPunchFeel(c, 0);
        combat_input::Machine idle;
        Pulses none;
        replay(rows, [&](const Tick& k) { none.add(idle.update(frameOf(k), c).buttons & combat_input::padX, k.t); });
        std::printf("  %s FORGIVING waving with no enemy: X %u\n", run, none.edges);
        check(none.edges == 0, "PUNCH SENSITIVITY FORGIVING: arm waving with no enemy sends no X");
    }
}

void ropeGrab() {
    std::printf("\n[R] 0.0.3 ROPE GRAB: tethers held through each recorded grip's next 2 s (enemy synthesised)\n");
    uint32_t tries = 0, picked = 0, nonFinite = 0, longRope = 0, byWhy[rope_grab::releases]{}, yanks = 0, heldOn = 0;
    float fastest = 0;
    std::vector<float> peaks;
    for (const char* run : runs) {
        const auto rows = load(run);
        for (size_t i = 1; i < rows.size(); ++i) {
            if (!(rows[i].presses > rows[i - 1].presses) || !rows[i].handOk[1] || !rows[i].play || !rows[i].headOk)
                continue;
            const Pose& g = rows[i].hand[1];
            const Vec3 aim = combat38::aimOf(g, g, combat38::axisHand);
            const Vec3 chest = g.position + normalized(aim) * (3.f + 6.f * rnd());
            ++tries;
            if (rope_grab::pick(g.position, aim, &chest, 1) != 0)
                continue;
            ++picked;
            rope_grab::Tether t;
            t.attach(0, 7, g.position, chest, 0);
            uint32_t why = rope_grab::none;
            float peakHand = 0;
            replay(rows, [&](const Tick& k) {
                if (!t.on || !k.handOk[1])
                    return;
                const Vec3 a = combat38::aimOf(k.hand[1], k.hand[1], combat38::axisHand);
                why = t.update(k.dt, true, true, k.hand[1].position, a);
                nonFinite += !finite(t.target);
                longRope += length(t.target - k.hand[1].position) > rope_grab::maxRange * 2.5f + 3.f;
                fastest = std::max(fastest, length(t.velocity));
                if (t.age > .25f)
                    peakHand = std::max(peakHand, length(t.handVelocity));
            }, rows[i].t, rows[i].t + 2.0);
            heldOn += t.on;
            peaks.push_back(peakHand);
            if (t.on)
                why = t.update(1.f / 90, false, true, t.hand0, Vec3{0, 0, -1});
            if (why < rope_grab::releases)
                ++byWhy[why];
            yanks += rope_grab::yanks(why, t.releaseVelocity);
        }
    }
    std::printf("  %u grips, %u picked; held the full 2 s %u; releases: fling %u, slam %u, let go %u, lost %u, timeout "
                "%u; %u would send the yank; fastest pole end %.1f m/s; non-finite %u, overlong %u\n",
                tries, picked, heldOn, byWhy[rope_grab::fling], byWhy[rope_grab::slam], byWhy[rope_grab::letGo],
                byWhy[rope_grab::lost], byWhy[rope_grab::timeout], yanks, fastest, nonFinite, longRope);
    std::sort(peaks.begin(), peaks.end());
    if (!peaks.empty())
        std::printf("  the hand's peak speed while held (until release): median %.1f, 75%% %.1f, 90%% %.1f m/s\n",
                    peaks[peaks.size() / 2], peaks[peaks.size() * 3 / 4], peaks[peaks.size() * 9 / 10]);
    check(picked == tries && tries > 0, "ROPE GRAB: an enemy straight down the aim was not picked");
    check(nonFinite == 0 && longRope == 0, "ROPE GRAB: the pole end went non-finite or ran away");
    check(byWhy[rope_grab::lost] == 0 && byWhy[rope_grab::timeout] == 0, "ROPE GRAB: a held tether was lost or timed out in 2 s");
}

// ---- Version 0.0.3 AUTO PERFORMANCE on the user's real FPS (the game log's once-a-minute fps) ------
// The frame time follows the recorded FPS (linear between samples, +-6% jitter, a 1 s 35% spike every
// 41 s). Each rung's gain is unknown (CANDIDATE): run with none (worst case) and with 6% per rung.
void autoPerf() {
    std::printf("\n[P] 0.0.3 AUTO PERFORMANCE: the controller on the user's recorded FPS (tests/replay/perf_fps.csv)\n");
    std::ifstream in(std::string(SPIDY_REPLAY_DIR) + "/perf_fps.csv");
    struct Sample { std::string run; double t; float fps; };
    std::vector<Sample> samples;
    for (std::string line; std::getline(in, line);) {
        if (line.empty() || line[0] == '#')
            continue;
        std::stringstream ss(line);
        Sample s{};
        std::string a, b, c;
        std::getline(ss, a, ',');
        std::getline(ss, b, ',');
        std::getline(ss, c, ',');
        s.run = a;
        s.t = std::atof(b.c_str());
        s.fps = static_cast<float>(std::atof(c.c_str()));
        samples.push_back(s);
    }
    check(samples.size() >= 100, "AUTO PERF: tests/replay/perf_fps.csv missing");
    using namespace auto_perf;
    const Knobs user{250, 2, 2};
    uint32_t sessions = 0, pastFloor = 0, notRestored = 0, quickFlips = 0, dropsNoMiss = 0, raisesTotal = 0, dropsTotal = 0;
    double busiest = 0;
    for (int gainModel = 0; gainModel < 2; ++gainModel)
        for (uint32_t floor = 0; floor < floors; ++floor)
            for (size_t i = 0; i < samples.size();) {
                size_t j = i;
                while (j < samples.size() && samples[j].run == samples[i].run)
                    ++j;
                if (j - i >= 3) {
                    ++sessions;
                    Controller c;
                    c.setUser(user);
                    double clock = 0, lastChangeAt = -100, lastUpAt = -100;
                    std::vector<double> changes;
                    uint32_t lastRung = 0, frames = 0;
                    const double end = samples[j - 1].t;
                    size_t k = i;
                    std::vector<std::pair<double, bool>> window; // the last 0.5 s of frames
                    while (clock < end) {
                        while (k + 1 < j && samples[k + 1].t <= clock)
                            ++k;
                        const Sample& a = samples[k];
                        const Sample& b = samples[std::min(k + 1, j - 1)];
                        const double f = b.t > a.t ? (clock - a.t) / (b.t - a.t) : 0;
                        float fps = static_cast<float>(a.fps + (b.fps - a.fps) * std::clamp(f, 0.0, 1.0));
                        fps *= 1.f + .06f * (2 * rnd() - 1);
                        if (std::fmod(clock, 41.0) < 1.0)
                            fps *= .65f;
                        const float gain = gainModel ? .06f * static_cast<float>(c.rung()) : 0.f;
                        const float ms = std::max(2.f, 1000.f / std::max(5.f, fps) * (1.f - gain));
                        const float dt = std::min(.1f, ms / 1000.f);
                        window.emplace_back(clock, ms > 1000.f / 90 * Controller::slack);
                        while (!window.empty() && window.front().first < clock - 1.0)
                            window.erase(window.begin());
                        size_t missedIn = 0;
                        for (const auto& w : window)
                            missedIn += w.second;
                        if (c.frame(true, dt, ms, 90, 1, floor)) {
                            const uint32_t r = c.rung();
                            if (r > lastRung) {
                                ++dropsTotal;
                                dropsNoMiss += missedIn * 10 < window.size(); // under 10% of the last 1 s missed
                                if (clock - lastUpAt < .95) { // the controller waits 1 s (float clocks): a drop then bans that raise
                                    ++quickFlips;
                                    std::printf("  QUICK DROP %s floor %u gain %d at %.3f after a raise at %.3f\n",
                                                samples[i].run.c_str(), floor, gainModel, clock, lastUpAt);
                                }
                            } else if (r < lastRung) {
                                ++raisesTotal;
                                lastUpAt = clock;
                                if (clock - lastChangeAt < Controller::raiseAfter - .01) {
                                    ++quickFlips;
                                    std::printf("  QUICK RAISE %s floor %u gain %d at %.3f after a change at %.3f\n",
                                                samples[i].run.c_str(), floor, gainModel, clock, lastChangeAt);
                                }
                            }
                            lastChangeAt = clock;
                            lastRung = r;
                            changes.push_back(clock);
                        }
                        pastFloor += c.rung() > deepest(floor);
                        clock += dt;
                        ++frames;
                    }
                    for (size_t m = 0; m < changes.size(); ++m) {
                        size_t n = m;
                        while (n < changes.size() && changes[n] - changes[m] < 60)
                            ++n;
                        busiest = std::max(busiest, static_cast<double>(n - m));
                    }
                    c.frame(false, .011f, 11, 90, 1, floor);
                    notRestored += !(c.now() == user);
                }
                i = j;
            }
    // The cost: a million frames of the controller.
    Controller z;
    z.setUser(user);
    const auto t0 = std::chrono::steady_clock::now();
    uint32_t sink = 0;
    for (int n = 0; n < 1000000; ++n)
        sink += z.frame(true, 1.f / 90, n % 7 ? 10.f : 14.f, 90, static_cast<uint32_t>(n / 5000), floorLow);
    const double ns = std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - t0).count() / 1e6;
    std::printf("  %u session replays: %u drops, %u raises, most changes in any 60 s %.0f; past the floor %u frames; a "
                "drop with under 10%% of the last 1 s missed %u; quick flips %u; OFF restored all but %u; %.0f ns a frame (%u)\n",
                sessions, dropsTotal, raisesTotal, busiest, pastFloor, dropsNoMiss, quickFlips, notRestored, ns, sink % 2);
    check(sessions > 0 && dropsTotal > 0, "AUTO PERF: the recorded low FPS caused no drop");
    check(pastFloor == 0, "AUTO PERF: went past the QUALITY FLOOR");
    check(notRestored == 0, "AUTO PERF: OFF did not restore the user's values exactly");
    check(dropsNoMiss == 0, "AUTO PERF: dropped with under 10% of the last second missed");
    check(quickFlips == 0 && busiest <= 12, "AUTO PERF: oscillated");
    check(ns < 2000, "AUTO PERF: the controller costs over 2 us a frame");
}

int main() {
    std::printf("Update 38.5 replay tests (%s)\n", SPIDY_REPLAY_DIR);
    if (load("run381").size() < 1000) {
        std::printf("FAIL: tests/replay/run381.csv missing\n");
        return 1;
    }
    holo();
    flicker();
    ringError();
    kicks();
    gadgets();
    aim();
    grab();
    yellowMarks();
    lookLock();
    grabAssist();
    farPunch();
    farNoEnemy();
    oneBall();
    wristMuzzle();
    promptsInCombat();
    ropeGrab();
    punchesRegister();
    everyHit();
    punchSense();
    autoPerf();
    summary();
    std::printf("\n%s: %d failure(s)\n", failures ? "REPLAY FAILED" : "REPLAY PASSED", failures);
    return failures ? 1 : 0;
}
