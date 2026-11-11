#pragma once
// HERO VR 0.0.79.1: keep targeted single-grip web strikes separate from
// two-grip enemy pulls without interfering with ordinary swing grips.
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
    bool zip{};          // short native Y pulse to request web strike
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
            if(rise[0]&&rise[1]&&(in.targets[0]||in.targets[1])) {
                int h=in.targets[0]?0:1;
                if(in.forward[0]&&in.forward[1]&&(!in.targets[1-h]||in.targets[1-h]==in.targets[h]))
                    beginPull(h,in.targets[h],out);
                else { phase_=Phase::blocked;out.claimed=3; }
            } else for(int h=0;h<2;++h) {
                if(rise[h]&&!in.grips[1-h]&&in.forward[h]&&in.targets[h]) {
                    hand_=h;target_=in.targets[h];age_=0;phase_=Phase::pending;
                    out.claimed=1u<<h;out.target=target_;out.hand=h;break;
                }
            }
            return out;
        }
        if(phase_==Phase::pending) {
            if(!in.grips[hand_]||!in.targets[hand_]||in.targets[hand_]!=target_) {
                phase_=Phase::blocked;out.claimed=(in.grips[0]?1u:0u)|(in.grips[1]?2u:0u);return out;
            }
            const int other=1-hand_;
            if(in.grips[other]) {
                if(in.forward[other]&&(!in.targets[other]||in.targets[other]==target_))
                    beginPull(hand_,target_,out);
                else {phase_=Phase::blocked;out.claimed=3;}
                return out;
            }
            out.claimed=1u<<hand_;out.target=target_;out.hand=hand_;
            age_+=dt;
            if(age_>=secondHandWindow) {
                phase_=Phase::zip;zipLeft_=zipPulse;out.zip=true;
            }
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
        if(phase_==Phase::zip) {
            if(in.grips[hand_]) {
                out.claimed=1u<<hand_;out.target=target_;out.hand=hand_;
                out.zip=zipLeft_>0.f;zipLeft_=std::max(0.f,zipLeft_-dt);
            } else phase_=Phase::blocked;
            return out;
        }
        if(!in.grips[0]&&!in.grips[1])clear();
        else out.claimed=(in.grips[0]?1u:0u)|(in.grips[1]?2u:0u);
        return out;
    }
    void reset(){previous_={};clear();}
private:
    enum class Phase{idle,pending,zip,pull,blocked};
    void clear(){phase_=Phase::idle;hand_=-1;target_=0;age_=zipLeft_=0;}
    void beginPull(int h,uint64_t id,Output& out){
        hand_=h;target_=id;phase_=Phase::pull;
        out.claimed=3;out.target=id;out.hand=h;
        out.pullStarted=true;out.pullActive=true;
    }
    Phase phase_=Phase::idle;
    std::array<bool,2> previous_{};
    int hand_=-1;
    uint64_t target_{};
    float age_{},zipLeft_{};
};
} // namespace spidy::enemy_grip_split
