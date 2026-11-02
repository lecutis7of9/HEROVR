// HERO VR 0.0.76 MELEE SNAP (include/spidy/melee076.hpp) and the BALL GAME watch (include/spidy/ball_sure.hpp), pure.
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
    std::vector<Foe> foes{{{0, 1, -1.9f}}, {{1.9f, 1, -.2f}}};
    auto s = pick(foes, head, ahead, ahead, false);
    check(s && s.index == 0 && !s.pointBlank, "A fist thrown ahead snaps to the enemy ahead");
    check(std::abs(s.metres - 1.9f) < .01f, "Its distance is flat from the head");
    std::vector<Foe> twoHalf{{{0, 1, -2.5f}}};
    check(!pick(twoHalf, head, ahead, ahead, false), "2.5 m is past the 2 m snap (the dash's)");
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
    std::vector<Foe> below{{{0, -.1f, -.6f}}}; // 1.9 m in 3D, 72 deg below the flat ahead
    check(static_cast<bool>(pick(below, head, ahead, ahead, false)), "On the ground the height does not count");
    check(!pick(below, head, ahead, ahead, true), "In the air the fist must go toward him in 3D");
    check(static_cast<bool>(pick(below, head, Vec3{0, -1, -.3f}, ahead, true)), "In the air a punch down at him snaps");
    std::vector<Foe> bad{{{NAN, 1, -2}}};
    check(!pick(bad, head, ahead, ahead, false), "A NaN enemy is ignored");
    check(!pick(foes, Vec3{NAN, 0, 0}, ahead, ahead, false), "A NaN head snaps nothing");
}
void lungeTests() {
    std::vector<Foe> near{{{0, 1, -1.9f}}}, mid{{{0, 1, -2.3f}}}, hidden{{{0, 1, -1}, false}};
    check(closeFoe(near, head, false), "An enemy within 2 m shuts the lunge");
    check(!closeFoe(mid, head, false), "Past 2 m the lunge may open");
    check(!closeFoe(hidden, head, false), "A hidden enemy does not shut it");
}
void dashTests() {
    // 0.0.76 PUNCH DASH: past 2 m and within 15 m, inside 35 deg of the fist's way.
    std::vector<Foe> foes{{{0, 1, -6}}, {{0, 1, -12}}, {{4, 1, -4}}};
    auto d = dash(foes, head, ahead, false);
    check(d && d.index == 0 && std::abs(d.metres - 6) < .01f, "Thrown ahead: the dash goes to the one ahead, the nearer");
    d = dash(foes, head, Vec3{1, 0, -1}, false);
    check(d && d.index == 2, "Thrown to the right front: the one there, not the lock ahead");
    check(!dash(foes, head, Vec3{-1, 0, 0}, false), "Thrown where no one is: no dash");
    std::vector<Foe> close{{{0, 1, -1.5f}}}, far{{{0, 1, -16}}}, hidden{{{0, 1, -5}, false}};
    check(!dash(close, head, ahead, false), "Within 2 m: never the dash (the snap's punch)");
    check(!dash(far, head, ahead, false), "Past 15 m: no dash");
    check(!dash(hidden, head, ahead, false), "A wall between: no dash");
    std::vector<Foe> both{{{0, 1, -1.5f}}, {{0, 1, -6}}};
    check(pick(both, head, ahead, ahead, false).index == 0, "A close one in the fist's way is the punch's");
    std::vector<Foe> offAxis{{{3, 1, -5}}}; // ~31 deg
    check(static_cast<bool>(dash(offAxis, head, ahead, false)), "31 deg off: inside the 35 deg cone");
    std::vector<Foe> wide{{{5, 1, -5}}}; // 45 deg
    check(!dash(wide, head, ahead, false), "45 deg off: outside it");
    check(!dash(foes, head, Vec3{}, false), "The fist's way unknown: no dash");
    check(!dash(foes, head, Vec3{NAN, 0, 0}, false), "A NaN way: no dash");
    std::vector<Foe> below{{{0, -4, -5}}};
    check(!dash(below, head, ahead, true), "In the air the way counts in 3D");
    check(static_cast<bool>(dash(below, head, Vec3{0, -5.7f, -5}, true)), "In the air a punch down at him dashes");
}
void ballTests() {
    using namespace spidy::ball_sure;
    check(outOfCombat(0, 5.f), "No enemies: out of combat");
    check(outOfCombat(3, 1e9f), "Enemies unknown far: out of combat");
    check(outOfCombat(2, 45.f), "All past 30 m: out of combat");
    check(!outOfCombat(2, 12.f), "One at 12 m: in combat");
    check(outOfCombat(1, NAN), "NaN distance: out of combat");
    const float dt = 1.f / 72;
    {   // BALL GAME watch: in combat the game's shot is seen
        Sure s;
        In in;
        in.seconds = dt;
        in.hooked = true;
        in.webCalls = 40;
        s.update(in);
        in.start[0] = true;
        s.update(in);
        in.start[0] = false;
        in.webCalls = 41;
        for (int f = 0; f < 60; ++f)
            s.update(in);
        check(s.inCombat().presses == 1 && s.inCombat().shots == 1 && s.inCombat().unseen == 0, "In combat: its shot seen");
        check(s.outCombat().presses == 0, "Nothing counted out of combat");
    }
    {   // out of combat the game starts nothing: counted unseen after 0.3 s, once
        Sure s;
        In in;
        in.seconds = dt;
        in.hooked = true;
        in.outOfCombat = true;
        in.webCalls = 7;
        s.update(in);
        in.start[1] = true;
        s.update(in);
        in.start[1] = false;
        int frame = -1;
        for (int f = 0; f < 90; ++f) {
            s.update(in);
            if (frame < 0 && s.outCombat().unseen == 1)
                frame = f;
        }
        check(s.outCombat().presses == 1 && s.outCombat().unseen == 1 && frame >= 19 && frame <= 22,
              "Out of combat, no muzzle ask: unseen at 0.3 s");
        check(s.outCombat().shots == 0 && s.inCombat().presses == 0, "Counted on the right side");
    }
    {   // unhooked: counted, never waited on
        Sure s;
        In in;
        in.seconds = dt;
        in.start[0] = in.start[1] = true;
        s.update(in);
        in.start[0] = in.start[1] = false;
        for (int f = 0; f < 60; ++f)
            s.update(in);
        check(s.inCombat().unhooked == 2 && s.inCombat().unseen == 0 && s.presses() == 2, "Unhooked: counted only");
    }
    {   // a NaN frame time never counts early or hangs
        Sure s;
        In in;
        in.hooked = true;
        in.seconds = NAN;
        in.start[0] = true;
        s.update(in);
        in.start[0] = false;
        for (int f = 0; f < 10; ++f)
            s.update(in);
        check(s.inCombat().unseen == 0, "NaN time: not unseen early");
        in.seconds = dt;
        for (int f = 0; f < 30; ++f)
            s.update(in);
        check(s.inCombat().unseen == 1, "Then it is");
    }
}
} // namespace

int main() {
    snapTests();
    lungeTests();
    dashTests();
    ballTests();
    if (failures)
        return 1;
    std::printf("melee076: all passed\n");
    return 0;
}
