#include "spidy/webline_gesture.hpp"
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
}
