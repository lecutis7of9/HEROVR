// HERO VR 0.0.79.1: enemy_grip_split edges (strike-first, opt-in). Standalone: no Windows headers.
#include "spidy/enemy_grip_split.hpp"
#include <cstdio>
#include <cstdlib>
#define CHECK(c) do { if(!(c)) { std::fprintf(stderr, "FAILED %s:%d %s\n", __FILE__, __LINE__, #c); std::exit(1); } } while(0)
using namespace spidy::enemy_grip_split;
static Input base() { Input f{}; f.enabled=true; f.seconds=1.f/90.f; f.forward={true,true}; f.targets={7,7}; return f; }
int main() {
    { // Strike on the press frame; Y lasts zipPulse; never re-sent while held.
        Gesture g; auto f=base(); f.grips={true,false};
        auto o=g.update(f);
        CHECK(o.zip && o.strikeStarted && o.claimed==1 && o.hand==0 && o.target==7);
        int frames=1;
        for(int k=0;k<60;++k){ o=g.update(f); CHECK(!o.strikeStarted); CHECK(o.claimed==1); frames+=o.zip; }
        CHECK(frames>=11 && frames<=14);
        f.grips={false,false}; o=g.update(f); CHECK(o.claimed==0 && !o.zip);
    }
    { // Right hand works the same.
        Gesture g; auto f=base(); f.grips={false,true};
        auto o=g.update(f); CHECK(o.zip && o.strikeStarted && o.claimed==2 && o.hand==1);
    }
    { // Both grips on one frame: strike now, pull on the next frame.
        Gesture g; auto f=base(); f.grips={true,true};
        auto o=g.update(f); CHECK(o.zip && o.strikeStarted && !o.pullStarted);
        o=g.update(f); CHECK(o.pullStarted && o.pullActive && o.claimed==3);
        o=g.update(f); CHECK(o.pullActive && !o.pullStarted && !o.zip);
    }
    { // Second hand after the window: no pull; it is held off the swing until released.
        Gesture g; auto f=base(); f.grips={true,false}; g.update(f);
        for(int k=0;k<30;++k) g.update(f); // 0.33 s
        f.grips={true,true}; auto o=g.update(f);
        CHECK(!o.pullStarted && o.claimed==3);
        f.grips={false,true}; o=g.update(f); CHECK(o.claimed==2 && !o.zip);
        f.grips={false,false}; o=g.update(f); CHECK(o.claimed==0);
        f.grips={true,false}; o=g.update(f); CHECK(o.strikeStarted);
    }
    { // Second hand on a DIFFERENT enemy: no pull.
        Gesture g; auto f=base(); f.grips={true,false}; g.update(f);
        f.grips={true,true}; f.targets={7,9}; auto o=g.update(f);
        CHECK(!o.pullStarted && !o.pullActive);
    }
    { // Not reached forward / no target: never claimed (ordinary swing grip).
        Gesture g; auto f=base(); f.forward={false,false}; f.grips={true,false};
        auto o=g.update(f); CHECK(o.claimed==0 && !o.zip);
        f=base(); f.targets={0,0}; f.grips={true,false}; g.reset();
        o=g.update(f); CHECK(o.claimed==0 && !o.zip);
    }
    { // Held grip from before enabling never strikes; disabling mid-strike clears.
        Gesture g; auto f=base(); f.enabled=false; f.grips={true,false}; g.update(f);
        f.enabled=true; auto o=g.update(f); CHECK(!o.zip && o.claimed==0);
        f.grips={false,false}; g.update(f); f.grips={true,false}; CHECK(g.update(f).zip);
        f.enabled=false; o=g.update(f); CHECK(!o.zip && o.claimed==0);
    }
    { // Release during the pull: the remaining hand stays off the swing until let go.
        Gesture g; auto f=base(); f.grips={true,true}; g.update(f); g.update(f);
        f.grips={true,false}; auto o=g.update(f); CHECK(!o.pullActive && o.claimed==1);
        f.grips={false,false}; o=g.update(f); CHECK(o.claimed==0);
    }
    std::puts("enemy grip split tests passed");
}
