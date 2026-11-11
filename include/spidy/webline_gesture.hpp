#pragma once
// VR webline placement gesture: first valid fixed-surface grip anchors one end;
// while held the free hand may aim elsewhere; release confirms a second fixed
// surface. No game input is synthesized here: caller invokes native webline
// placement only on Output::commit. Target/mission adapters own validation.
#include "spidy/swing.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace spidy::webline_gesture {
struct Input {
    bool enabled{}, grip{}, occupied{}, grounded{};
    spidy::Vec3 hand{}, aim{};
    const spidy::WorldQueries* world{};
};
struct Output {
    bool claimed{}, started{}, commit{}, cancelled{}, preview{};
    spidy::Vec3 from{}, to{};
    std::uint64_t firstSurface{}, secondSurface{};
};
class Gesture {
public:
    explicit Gesture(float range=35.f, float minSpan=2.f) : range_(range), minSpan_(minSpan) {}
    Output update(const Input& in) {
        Output out{};
        const bool rising=in.grip&&!previous_;
        const bool falling=!in.grip&&previous_;
        previous_=in.grip;
        if (!in.enabled || in.occupied || !in.grounded || !in.world) {
            if(active_) { out.cancelled=true; resetActive(); }
            return out;
        }
        if(rising && !active_) {
            auto hit=pick(in);
            if(hit) {
                active_=true; start_=hit->point; surface_=hit->surface;
                out.started=true; out.claimed=true;
            }
        }
        if(!active_) return out;
        out.claimed=true;out.from=start_;out.firstSurface=surface_;
        auto end=pick(in);
        if(end && end->surface!=surface_ && spidy::length(end->point-start_)>=minSpan_) {
            out.preview=true;out.to=end->point;out.secondSurface=end->surface;
        }
        if(falling) {
            if(out.preview) out.commit=true;
            else out.cancelled=true;
            resetActive();
        }
        return out;
    }
    void reset(){previous_=false;resetActive();}
private:
    std::optional<spidy::RayHit> pick(const Input& in) const {
        if(!spidy::finite(in.hand)||!spidy::finite(in.aim))return {};
        const auto direction=spidy::normalized(in.aim);
        if(spidy::length(direction)<.5f)return {};
        auto hit=in.world->raycast(in.hand,direction,range_);
        if(!hit||!hit->fixed||!spidy::finite(hit->point)||!in.world->exists(hit->surface))return {};
        return hit;
    }
    void resetActive(){active_=false;surface_=0;start_={};}
    float range_,minSpan_;
    bool previous_{},active_{};
    spidy::Vec3 start_{};
    std::uint64_t surface_{};
};
} // namespace spidy::webline_gesture
