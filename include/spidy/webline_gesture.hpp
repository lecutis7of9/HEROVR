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
// 79.5: why the free hand's aim is not (yet) a valid second anchor, for the marker's colour and the cancel note.
enum class Reason : std::uint8_t { none, noSurface, notFixed, tooFar, sameSurface, tooClose, badAim };
inline const char* reasonText(Reason r) {
    switch (r) {
    case Reason::none: return "ok";
    case Reason::noSurface: return "the hand was not aimed at any surface";
    case Reason::notFixed: return "the hand was aimed at a prop or a character, not a surface a web holds on";
    case Reason::tooFar: return "the second spot was over the reach (35 m)";
    case Reason::sameSurface: return "the second spot was on the same surface";
    case Reason::tooClose: return "the second spot was under 2 m from the first";
    case Reason::badAim: return "the hand's aim was not tracked";
    }
    return "?";
}
struct Input {
    // 79.5: this frame's seconds. 0 (older callers, tests): no release latch.
    float seconds{};
    bool enabled{}, grip{}, occupied{};
    spidy::Vec3 hand{}, aim{};
    const spidy::WorldQueries* world{};
    // No world: the caller's own ray result along `aim` this frame.
    bool sampled{};
    std::optional<spidy::RayHit> hit{};
};
struct Output {
    bool claimed{}, started{}, commit{}, cancelled{}, preview{};
    // 79.5: while a gesture holds a grip: hasTarget = the free aim meets a surface at `target` (valid when
    // `valid`, else `reason` says why not, for a red marker); latched = the commit used the last valid second
    // anchor from up to latchWindow seconds before the release (the release jerk moves the aim off it);
    // reason on a cancel = why the release found no valid second anchor.
    bool hasTarget{}, valid{}, latched{};
    Reason reason{};
    spidy::Vec3 target{};
    spidy::Vec3 from{}, to{};
    std::uint64_t firstSurface{}, secondSurface{};
};
class Gesture {
public:
    // 79.5: a release within this long of the last valid second anchor commits it.
    static constexpr float latchWindow = .3f;
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
            Reason why{};
            auto hit=pick(in,why);
            if(hit) {
                active_=true; start_=hit->point; surface_=hit->surface;
                latched_=false; latchAge_=0; lastWhy_=Reason::none;
                out.started=true; out.claimed=true;
            }
        }
        if(!active_) return out;
        out.claimed=true;out.from=start_;out.firstSurface=surface_;
        Reason why{};
        auto end=pick(in,why);
        if(end) {
            out.hasTarget=true; out.target=end->point;
            if(distinct_ && end->surface==surface_) why=Reason::sameSurface;
            else if(spidy::length(end->point-start_)<minSpan_) why=Reason::tooClose;
            else {
                out.preview=true; out.valid=true; out.to=end->point; out.secondSurface=end->surface; why=Reason::none;
            }
        }
        out.reason=why;
        if(out.preview) {
            latched_=true; latchAge_=0; latchTo_=end->point; latchSurface_=end->surface; lastWhy_=Reason::none;
        } else {
            lastWhy_=why;
            if(latched_) latchAge_+=std::isfinite(in.seconds)&&in.seconds>0?in.seconds:1e9f;
        }
        if(falling) {
            if(out.preview) out.commit=true;
            else if(latched_ && latchAge_<=latchWindow) {
                out.commit=true; out.latched=true; out.preview=true; out.valid=true;
                out.to=latchTo_; out.secondSurface=latchSurface_;
            } else out.cancelled=true;
            resetActive();
        }
        return out;
    }
    void reset(){previous_=false;swallow_=false;resetActive();}
    bool active() const {return active_;}
    // True while a grip it took (active, or swallowed after a cancel) is held.
    bool holding() const {return active_||swallow_;}
private:
    std::optional<spidy::RayHit> pick(const Input& in, Reason& why) const {
        why=Reason::none;
        if(!spidy::finite(in.hand)||!spidy::finite(in.aim)){why=Reason::badAim;return {};}
        const auto direction=spidy::normalized(in.aim);
        if(spidy::length(direction)<.5f){why=Reason::badAim;return {};}
        if(!in.world) {
            const auto& hit=in.hit;
            if(!hit||!spidy::finite(hit->point)){why=Reason::noSurface;return {};}
            if(!hit->fixed){why=Reason::notFixed;return {};}
            if(spidy::length(hit->point-in.hand)>range_){why=Reason::tooFar;return {};}
            return hit;
        }
        auto hit=in.world->raycast(in.hand,direction,range_);
        if(!hit||!spidy::finite(hit->point)){why=Reason::noSurface;return {};}
        if(!hit->fixed||!in.world->exists(hit->surface)){why=Reason::notFixed;return {};}
        return hit;
    }
    void resetActive(){active_=false;surface_=0;start_={};latched_=false;latchAge_=0;}
    float range_,minSpan_;
    bool distinct_{true};
    bool previous_{},active_{},swallow_{};
    spidy::Vec3 start_{}, latchTo_{};
    std::uint64_t surface_{}, latchSurface_{};
    bool latched_{};
    float latchAge_{};
    Reason lastWhy_{};
};
} // namespace spidy::webline_gesture
