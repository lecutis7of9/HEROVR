// HERO VR 0.0.76 MELEE SNAP (include/spidy/melee076.hpp) and BALL SURE (include/spidy/ball_sure.hpp), pure.
// Made by Agent Mulder.
#include "spidy/ball_sure.hpp"
#include "spidy/melee076.hpp"

#include <cmath>
#include <cstdio>
#include <vector>

using namespace spidy;
using namespace spidy::melee076;

namespace {
int failures = 0;
void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++failures;
    }
}
const Vec3 head{0, 1.7f, 0}, ahead{0, 0, -1};

void snapTests() {
    std::vector<Foe> foes{{{0, 1, -2.5f}}, {{2.5f, 1, -.2f}}};
    auto s = pick(foes, head, ahead, ahead, false);
    check(s && s.index == 0 && !s.pointBlank, "A fist thrown ahead snaps to the enemy ahead");
    check(std::abs(s.metres - 2.5f) < .01f, "Its distance is flat from the head");
    s = pick(foes, head, Vec3{1, 0, -.1f}, ahead, false);
    check(s && s.index == 1, "A hook to the right snaps to the enemy on the right");
    s = pick(foes, head, Vec3{1, 0, -1}, ahead, false); // 45 deg: both inside 70; enemy 0 at 45, enemy 1 at ~40
    check(s && s.index == 1, "The smaller angle wins");
    std::vector<Foe> far{{{0, 1, -5}}};
    check(!pick(far, head, ahead, ahead, false), "Past reach: no snap");
    std::vector<Foe> behind{{{0, 1, 1.2f}}};
    check(!pick(behind, head, ahead, ahead, false), "Behind him: no snap");
    std::vector<Foe> close{{{.6f, 1, -1.f}}};
    s = pick(close, head, Vec3{-1, 0, .2f}, ahead, false);
    check(s && s.pointBlank, "Point blank: the nearest in front even when the fist goes elsewhere");
    s = pick(close, head, Vec3{}, ahead, false);
    check(s && s.pointBlank, "Point blank with the fist's way unknown");
    std::vector<Foe> blocked{{{0, 1, -2}, false}};
    check(!pick(blocked, head, ahead, ahead, false), "A wall between: no snap");
    std::vector<Foe> below{{{0, -1.f, -.8f}}};
    check(static_cast<bool>(pick(below, head, ahead, ahead, false)), "On the ground the height does not count");
    check(!pick(below, head, ahead, ahead, true), "In the air the fist must go toward him in 3D");
    check(static_cast<bool>(pick(below, head, Vec3{0, -1, -.3f}, ahead, true)), "In the air a punch down at him snaps");
    std::vector<Foe> bad{{{NAN, 1, -2}}};
    check(!pick(bad, head, ahead, ahead, false), "A NaN enemy is ignored");
    check(!pick(foes, Vec3{NAN, 0, 0}, ahead, ahead, false), "A NaN head snaps nothing");
}
void lungeTests() {
    std::vector<Foe> near{{{0, 1, -3.4f}}}, mid{{{0, 1, -3.7f}}}, hidden{{{0, 1, -1}, false}};
    check(closeFoe(near, head, false), "An enemy within 3.5 m shuts the lunge");
    check(!closeFoe(mid, head, false), "Past 3.5 m the lunge may open");
    check(!closeFoe(hidden, head, false), "A hidden enemy does not shut it");
}
void ballTests() {
    using namespace spidy::ball_sure;
    check(outOfCombat(0, 5.f), "No enemies: out of combat");
    check(outOfCombat(3, 1e9f), "Enemies unknown far: out of combat");
    check(outOfCombat(2, 45.f), "All past 30 m: out of combat");
    check(!outOfCombat(2, 12.f), "One at 12 m: in combat");
    check(outOfCombat(1, NAN), "NaN distance: out of combat");
    const float dt = 1.f / 72;
    {   // the game fires: no backup
        Sure s;
        In in;
        in.seconds = dt;
        in.hooked = true;
        in.webCalls = 40;
        s.update(in);
        in.start[0] = true;
        check(!s.update(in).backup[0], "Hooked: no ball at once");
        in.start[0] = false;
        in.webCalls = 41;
        bool any = false;
        for (int f = 0; f < 60; ++f)
            any |= s.update(in).backup[0];
        check(!any && s.gameShots() == 1 && s.backups() == 0, "The game's shot seen: no backup");
    }
    {   // the game drops it: one backup after 0.3 s
        Sure s;
        In in;
        in.seconds = dt;
        in.hooked = true;
        in.webCalls = 7;
        s.update(in);
        in.start[1] = true;
        s.update(in);
        in.start[1] = false;
        int frame = -1, count = 0;
        for (int f = 0; f < 90; ++f)
            if (s.update(in).backup[1]) {
                ++count;
                if (frame < 0)
                    frame = f;
            }
        check(count == 1 && frame >= 19 && frame <= 22, "No game shot: one backup at 0.3 s");
        check(s.backups() == 1 && s.presses() == 1, "Counted");
    }
    {   // unhooked: the backup goes at once
        Sure s;
        In in;
        in.seconds = dt;
        in.start[0] = in.start[1] = true;
        const auto o = s.update(in);
        check(o.backup[0] && o.backup[1] && s.unhookedBalls() == 2, "Unhooked: ours at once from each wrist");
        in.start[0] = in.start[1] = false;
        bool any = false;
        for (int f = 0; f < 60; ++f) {
            const auto p = s.update(in);
            any |= p.backup[0] || p.backup[1];
        }
        check(!any, "Never twice");
    }
    {   // a NaN frame time never fires early or hangs
        Sure s;
        In in;
        in.hooked = true;
        in.seconds = NAN;
        in.start[0] = true;
        s.update(in);
        in.start[0] = false;
        bool any = false;
        for (int f = 0; f < 10; ++f)
            any |= s.update(in).backup[0];
        check(!any, "NaN time: no early backup");
        in.seconds = dt;
        for (int f = 0; f < 30; ++f)
            any |= s.update(in).backup[0];
        check(any, "Then the backup still comes");
    }
}
} // namespace

int main() {
    snapTests();
    lungeTests();
    ballTests();
    if (failures)
        return 1;
    std::printf("melee076: all passed\n");
    return 0;
}
