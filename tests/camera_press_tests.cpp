// 79.9 CAMERA GESTURE: the left trigger at the left temple. Standalone: no Windows headers.
#include "spidy/game_pad.hpp"
#include <cstdio>
#include <cstdlib>
#define CHECK(c) do { if(!(c)) { std::fprintf(stderr, "FAILED %s:%d %s\n", __FILE__, __LINE__, #c); std::exit(1); } } while(0)
using namespace spidy;
int main() {
    const float dt = 1.f / 90;
    const Pose head{{0, 1.7f, 0}, {}};
    const Pose leftTemple{{-.17f, 1.72f, .02f}, {}}, rightTemple{{.17f, 1.72f, .02f}, {}}, chest{{-.2f, 1.2f, -.3f}, {}};
    { // the left trigger at the left temple: held, claimed, counted
        game_pad::CameraPress c;
        CHECK(!c.update(dt, true, head, leftTemple, true, 0));
        CHECK(c.update(dt, true, head, leftTemple, true, 1) && c.claimsTrigger() && c.opens() == 1);
        for (int i = 0; i < 40; ++i)
            CHECK(c.update(dt, true, head, chest, true, 1) && c.claimsTrigger()); // holds wherever the hand goes
        c.update(dt, true, head, chest, true, 0);
        CHECK(!c.claimsTrigger() && c.held()); // the minimum hold carries a quick pull
        int frames = 0;
        while (c.update(dt, true, head, chest, true, 0))
            ++frames;
        CHECK(frames > 5 && frames < 20 && c.opens() == 1 && !c.held());
    }
    { // anywhere else the left trigger is a web ball as before: the chest, the right temple, a trigger already down
        game_pad::CameraPress c;
        CHECK(!c.update(dt, true, head, chest, true, 1) && !c.claimsTrigger());
        game_pad::CameraPress r;
        CHECK(!r.update(dt, true, head, rightTemple, true, 1) && !r.claimsTrigger() && r.opens() == 0);
        game_pad::CameraPress late;
        late.update(dt, true, head, chest, true, 1);
        CHECK(!late.update(dt, true, head, leftTemple, true, 1) && late.opens() == 0);
    }
    { // not outside VR play, not untracked; a trigger held into play is no camera
        game_pad::CameraPress menu;
        CHECK(!menu.update(dt, false, head, leftTemple, true, 1));
        CHECK(!menu.update(dt, true, head, leftTemple, true, 1) && menu.opens() == 0);
        game_pad::CameraPress lost;
        CHECK(!lost.update(dt, true, head, leftTemple, false, 1));
    }
    { // TemplePress is untouched: the right hand at the right temple scans; the left hand at the left one still would
        game_pad::TemplePress scan;
        const Pose grips[2]{leftTemple, rightTemple};
        const bool both[2]{true, true};
        float triggers[2]{};
        scan.update(dt, true, head, grips, both, triggers);
        triggers[1] = 1;
        CHECK(scan.update(dt, true, head, grips, both, triggers) && scan.hands() == 2 && scan.scans() == 1);
    }
    { // what it sends: D-pad Up for Walk::camera, the left trigger for Walk::aim, nothing else
        game_pad::Walk walk;
        walk.camera = true;
        CHECK(game_pad::fromControllers(XrFrame{}, game_pad::Mapping::gameplay, walk).buttons == game_pad::dpadUp);
        game_pad::Walk aim;
        aim.aim = true;
        const auto s = game_pad::fromControllers(XrFrame{}, game_pad::Mapping::gameplay, aim);
        CHECK(s.leftTrigger == 255 && s.buttons == 0 && s.rightTrigger == 0);
    }
    std::puts("camera press ok");
    return 0;
}
