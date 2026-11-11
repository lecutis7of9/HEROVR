#include "spidy/webline_gesture.hpp"
#include "spidy/webline_native.hpp"
#include <cstdio>
#include <cstdlib>
#define CHECK(c) do { if(!(c)) { std::fprintf(stderr, "FAILED %s:%d %s\n", __FILE__, __LINE__, #c); std::exit(1); } } while(0)
#include <cassert>
#include <cmath>
using namespace spidy;
struct Surfaces final : WorldQueries {
    std::optional<RayHit> raycast(Vec3, Vec3 direction, float) const override {
        if (direction.x > .5f) return RayHit{{10, 4, 0},{-1,0,0},11,true};
        if (direction.x < -.5f) return RayHit{{-10,4,0},{1,0,0},22,true};
        return {};
    }
    bool exists(std::uint64_t id) const override {return id==11||id==22;}
};
int main(){
    Surfaces w;
    webline_gesture::Gesture g;
    webline_gesture::Input i{};
    i.enabled=true;i.world=&w;i.hand={0,4,0};i.aim={1,0,0};
    i.grip=true;
    auto a=g.update(i);
    assert(a.started&&a.claimed&&!a.commit);
    i.aim={-1,0,0};
    a=g.update(i);
    assert(a.preview&&a.claimed&&!a.commit);
    i.grip=false;
    a=g.update(i);
    assert(a.commit&&a.firstSurface==11&&a.secondSurface==22);
    i.grip=true;i.aim={1,0,0};
    assert(g.update(i).started);
    i.aim={0,1,0};i.grip=false;
    a=g.update(i);
    assert(a.cancelled&&!a.commit);
    i.occupied=true;i.grip=true;
    assert(!g.update(i).claimed);

    // ---- 79.1 web-mechanics test: the game adapter (SpidyAimSample hits, no surface ids) ----
    {
        webline_gesture::Gesture game(35.f, 2.f, false); // SM2: one building may hold both ends
        webline_gesture::Input in{};
        in.enabled=true; in.sampled=true; in.hand={0,1.5f,0}; in.aim={0,0,-1};
        const RayHit wallA{{0,3,-8},{0,0,1},1,true}, wallB{{6,3,-8},{0,0,1},1,true};
        // Grip with no valid surface: nothing claimed, the swing keeps the grip.
        in.grip=true; in.hit.reset();
        auto o=game.update(in);
        CHECK(!o.claimed && !o.started && !game.holding());
        in.grip=false; game.update(in);
        // Grip on a surface: started, claimed before the swing sees the press.
        in.grip=true; in.hit=wallA;
        o=game.update(in);
        CHECK(o.started && o.claimed && game.active());
        // Same building, far enough apart: preview, then commit on release.
        in.hit=wallB; o=game.update(in);
        CHECK(o.preview && o.claimed && !o.commit);
        in.grip=false; o=game.update(in);
        CHECK(o.commit && !o.cancelled && !game.holding());
        CHECK(std::fabs(o.from.x-0)<1e-4f && std::fabs(o.to.x-6)<1e-4f);
        // Too short a span: release cancels, no commit.
        in.grip=true; in.hit=wallA; game.update(in);
        in.hit=RayHit{{1,3,-8},{0,0,1},1,true};
        in.grip=false; o=game.update(in);
        CHECK(o.cancelled && !o.commit);
        // Release into open air (no hit): clean cancel.
        in.grip=true; in.hit=wallA; game.update(in);
        in.hit.reset(); in.grip=false; o=game.update(in);
        CHECK(o.cancelled && !o.commit && !game.holding());
        // Non-fixed hit (a prop or character) never anchors.
        in.grip=true; in.hit=RayHit{{0,1,-3},{0,0,1},1,false};
        o=game.update(in);
        CHECK(!o.started && !o.claimed);
        in.grip=false; game.update(in);
        // Beyond range never anchors.
        in.grip=true; in.hit=RayHit{{0,3,-80},{0,0,1},1,true};
        CHECK(!game.update(in).started);
        in.grip=false; game.update(in);
        // Occupied mid-gesture (an enemy grab, a swing web): cancel, and the
        // still-held grip stays swallowed until let go (no late swing press).
        in.grip=true; in.hit=wallA; CHECK(game.update(in).started);
        in.occupied=true; o=game.update(in);
        CHECK(o.cancelled && o.claimed && game.holding() && !game.active());
        in.occupied=false; in.hit=wallB; o=game.update(in);
        CHECK(o.claimed && !o.started && !o.commit && !o.preview);
        in.grip=false; o=game.update(in);
        CHECK(!o.claimed && !o.commit && !game.holding());
        // Disabled mid-gesture (menu, became airborne in mode on): same.
        in.grip=true; in.hit=wallA; CHECK(game.update(in).started);
        in.enabled=false; o=game.update(in);
        CHECK(o.cancelled && o.claimed);
        in.grip=false; o=game.update(in);
        CHECK(!o.claimed && !game.holding());
        in.enabled=true;
        // Holding a grip from before enable never starts (needs a fresh press).
        in.grip=true; in.enabled=false; game.update(in);
        in.enabled=true; o=game.update(in);
        CHECK(!o.started && !o.claimed);
        in.grip=false; game.update(in);
        // Untracked aim: no start.
        in.aim={0,0,0}; in.grip=true; in.hit=wallA;
        CHECK(!game.update(in).started);
        in.grip=false; game.update(in); in.aim={0,0,-1};
        // reset() drops everything.
        in.grip=true; game.update(in); game.reset();
        CHECK(!game.holding());
    }
    // ---- the native driver: L2 held while the gesture is, then L2 + Y once ----
    {
        using namespace webline_native;
        CHECK(allowed(modeOn,false) && !allowed(modeOn,true) && !allowed(modeOff,false) &&
              allowed(modeAnywhere,true) && !allowed(99u,false));
        Driver d;
        const float dt=1.f/90.f;
        Pad p=d.update(dt,true,false,false,false);
        CHECK(!p.aim && !p.y && !d.busy());
        p=d.update(dt,true,true,false,false);
        CHECK(p.aim && !p.y);
        // Commit: exactly one Y pulse, always with L2 held, then everything released.
        p=d.update(dt,true,false,true,false);
        CHECK(p.aim && !p.y && d.sends()==1);
        int yFrames=0, aimFrames=0, frames=0; bool yWithoutAim=false;
        for(;frames<200 && d.busy();++frames){
            p=d.update(dt,true,false,false,false);
            yFrames+=p.y; aimFrames+=p.aim; yWithoutAim|=p.y&&!p.aim;
        }
        CHECK(!d.busy() && yFrames>=9 && yFrames<=13 && aimFrames>yFrames && !yWithoutAim && frames<40);
        p=d.update(dt,true,false,false,false);
        CHECK(!p.aim && !p.y);
        // Cancel: L2 released at once, no Y.
        d.update(dt,true,true,false,false);
        p=d.update(dt,true,false,false,true);
        CHECK(!p.aim && !p.y && !d.busy() && d.sends()==1);
        // Disabled mid-send drops everything (menu opened, hero switched).
        d.update(dt,true,true,false,false);
        d.update(dt,true,false,true,false);
        p=d.update(dt,false,false,false,false);
        CHECK(!p.aim && !p.y && !d.busy());
        // A NaN frame time cannot stall or spam.
        d.update(dt,true,false,true,false);
        for(int k=0;k<100 && d.busy();++k) d.update(dt,true,false,false,false);
        CHECK(!d.busy());
        d.update(NAN,true,false,false,false);
        CHECK(!d.busy());
    }
    std::puts("webline gesture + native driver tests passed");
}
