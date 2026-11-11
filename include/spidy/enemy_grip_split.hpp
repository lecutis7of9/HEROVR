#pragma once
// HERO VR 0.0.79.1: keep targeted single-grip web strikes separate from
// two-grip enemy pulls without interfering with ordinary swing grips.
//
// OPT-IN (live.txt enemy_grip_split = on; default off). Off, game_xr runs the
// pre-9d58d65 combat path exactly (grip on an enemy = GRAB WEB at once, pull
// to yank; physical Y in the air = the game's web strike).
//
// Strike first, never delayed: the first aimed grip on an enemy sends the
// native web-strike Y on THAT frame. Until 9d58d65's rework this waited
// secondHandWindow (0.24 s) for a possible second hand, which made combat feel
// sluggish (user report t2145). A second grip within the window now UPGRADES
// the strike to the two-hand grab/pull; it never holds the strike back. Both
// grips on the same frame: strike now, pull on the next frame.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
namespace spidy::enemy_grip_split {
struct Input {
    float seconds{};
    bool enabled{};
    std::array<bool,2> grips{};          // free, tracked grip state
    std::array<bool,2> forward{};        // physically extended toward the scene
    std::array<uint64_t,2> targets{};    // IDs from existing grab ray/capsule
};
struct Output {
    uint32_t claimed{};
    uint64_t target{};
    int hand = -1;
    bool zip{};          // native Y (web strike), from the press frame on, zipPulse long
    bool strikeStarted{};// the frame the strike went out
    bool pullStarted{};  // attach to pre-existing GrabWeb using chosen ID
    bool pullActive{};
};
class Gesture {
public:
    static constexpr float secondHandWindow=.24f, zipPulse=.14f;
    Output update(const Input& in) {
        const std::array<bool,2> rise{in.grips[0]&&!previous_[0],in.grips[1]&&!previous_[1]};
        previous_=in.grips;
        Output out{};
        if(!in.enabled){ clear();return out; }
        const float dt=std::isfinite(in.seconds)?std::clamp(in.seconds,0.f,.1f):0.f;
        if(phase_==Phase::idle) {
            for(int h=0;h<2;++h) {
                if(rise[h]&&in.forward[h]&&in.targets[h]) {
                    hand_=h;target_=in.targets[h];age_=0;phase_=Phase::strike;zipLeft_=zipPulse;
                    // The other hand pressed on this same frame, at this enemy or at
                    // nothing: the grab follows on the next frame.
                    const int o=1-h;
                    upgrade_=rise[o]&&in.forward[o]&&(!in.targets[o]||in.targets[o]==target_);
                    if(rise[o]&&!upgrade_) phase_=Phase::strikeBlocked;
                    out.claimed=1u<<h; out.target=target_; out.hand=h;
                    out.zip=true; out.strikeStarted=true;
                    zipLeft_-=dt;
                    return out;
                }
            }
            return out;
        }
        if(phase_==Phase::strike||phase_==Phase::strikeBlocked) {
            age_+=dt;
            const int other=1-hand_;
            if(!in.grips[hand_]) {
                if(!in.grips[other]) clear(); else { phase_=Phase::blocked; out.claimed=1u<<other; }
                return out;
            }
            if(phase_==Phase::strike && in.grips[other] && (upgrade_||rise[other]) && age_<=secondHandWindow+dt &&
               in.forward[other] && (!in.targets[other]||in.targets[other]==target_)) {
                beginPull(hand_,target_,out);
                return out;
            }
            out.claimed=1u<<hand_;out.target=target_;out.hand=hand_;
            if(in.grips[other]&&!(phase_==Phase::strike&&upgrade_)) {
                // A late or mis-aimed second grip: held off the swing until let go.
                out.claimed|=1u<<other; phase_=Phase::strikeBlocked;
            }
            out.zip=zipLeft_>0.f; zipLeft_=std::max(0.f,zipLeft_-dt);
            upgrade_=false;
            return out;
        }
        if(phase_==Phase::pull) {
            if(in.grips[0]&&in.grips[1]) {
                out.claimed=3;out.target=target_;out.hand=hand_;out.pullActive=true;
            } else {
                phase_=Phase::blocked;
                out.claimed=(in.grips[0]?1u:0u)|(in.grips[1]?2u:0u);
            }
            return out;
        }
        if(!in.grips[0]&&!in.grips[1])clear();
        else out.claimed=(in.grips[0]?1u:0u)|(in.grips[1]?2u:0u);
        return out;
    }
    void reset(){previous_={};clear();}
private:
    enum class Phase{idle,strike,strikeBlocked,pull,blocked};
    void clear(){phase_=Phase::idle;hand_=-1;target_=0;age_=zipLeft_=0;upgrade_=false;}
    void beginPull(int h,uint64_t id,Output& out){
        hand_=h;target_=id;phase_=Phase::pull;upgrade_=false;
        out.claimed=3;out.target=id;out.hand=h;
        out.pullStarted=true;out.pullActive=true;
    }
    Phase phase_=Phase::idle;
    std::array<bool,2> previous_{};
    int hand_=-1;
    uint64_t target_{};
    float age_{},zipLeft_{};
    bool upgrade_{};
};
} // namespace spidy::enemy_grip_split
