#pragma once
// VR webline placement gesture: first valid fixed-surface grip anchors one end;
// while held the free hand may aim elsewhere; release confirms a second fixed
// surface. No game input is synthesized here: caller invokes native webline
// placement only on Output::commit. Target/mission adapters own validation.
//
// 79.1 web-mechanics test: in the game there is no WorldQueries; the caller
// passes the hand's own aim sample (SpidyAimSample: the swing bridge's world
// rays, AimKind::anchor = a surface a web holds on) as Input::sampled/hit.
// The game's hit carries no surface id, so the game adapter turns off the
// distinct-surface rule (two points on one building are legal in SM2) and
// keeps only the minimum span. A grip the gesture took stays swallowed until
// it is let go, even after a cancel, so the swing never sees a late press.
#include "spidy/swing.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
namespace spidy::webline_gesture {
struct Input {
    bool enabled{}, grip{}, occupied{};
    spidy::Vec3 hand{}, aim{};
    const spidy::WorldQueries* world{};
    // No world: the caller's own ray result along `aim` this frame.
    bool sampled{};
    std::optional<spidy::RayHit> hit{};
};
struct Output {
    bool claimed{}, started{}, commit{}, cancelled{}, preview{};
    spidy::Vec3 from{}, to{};
    std::uint64_t firstSurface{}, secondSurface{};
};
class Gesture {
public:
    explicit Gesture(float range=35.f, float minSpan=2.f, bool distinctSurfaces=true)
        : range_(range), minSpan_(minSpan), distinct_(distinctSurfaces) {}
    Output update(const Input& in) {
        Output out{};
        const bool rising=in.grip&&!previous_;
        const bool falling=!in.grip&&previous_;
        previous_=in.grip;
        if(!in.grip) swallow_=false;
        if (!in.enabled || in.occupied || (!in.world && !in.sampled)) {
            if(active_) { out.cancelled=true; resetActive(); swallow_=in.grip; }
            out.claimed=swallow_;
            return out;
        }
        if(swallow_) { out.claimed=true; return out; }
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
        if(end && (!distinct_ || end->surface!=surface_) && spidy::length(end->point-start_)>=minSpan_) {
            out.preview=true;out.to=end->point;out.secondSurface=end->surface;
        }
        if(falling) {
            if(out.preview) out.commit=true;
            else out.cancelled=true;
            resetActive();
        }
        return out;
    }
    void reset(){previous_=false;swallow_=false;resetActive();}
    bool active() const {return active_;}
    // True while a grip it took (active, or swallowed after a cancel) is held.
    bool holding() const {return active_||swallow_;}
private:
    std::optional<spidy::RayHit> pick(const Input& in) const {
        if(!spidy::finite(in.hand)||!spidy::finite(in.aim))return {};
        const auto direction=spidy::normalized(in.aim);
        if(spidy::length(direction)<.5f)return {};
        if(!in.world) {
            const auto& hit=in.hit;
            if(!hit||!hit->fixed||!spidy::finite(hit->point)||spidy::length(hit->point-in.hand)>range_)return {};
            return hit;
        }
        auto hit=in.world->raycast(in.hand,direction,range_);
        if(!hit||!hit->fixed||!spidy::finite(hit->point)||!in.world->exists(hit->surface))return {};
        return hit;
    }
    void resetActive(){active_=false;surface_=0;start_={};}
    float range_,minSpan_;
    bool distinct_{true};
    bool previous_{},active_{},swallow_{};
    spidy::Vec3 start_{};
    std::uint64_t surface_{};
};
} // namespace spidy::webline_gesture
