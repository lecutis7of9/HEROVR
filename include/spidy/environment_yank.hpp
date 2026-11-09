#pragma once
// Spider-Man 2 VR: contextual environmental object yank.
// Never move the game's Havok props directly: the SM2 physical WebGrab
// adapter is not yet ported. Instead, a deliberate one-hand grip and pull
// sends the game's EXISTING L1+R1 environment throw action when the game's
// HUD has an eligible L1+R1 interaction prompt.
//
// The caller must zero the claimed hand's web input before the swing module
// sees it. No unprompted grab: ordinary grip swinging still works.
#include "math.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace spidy::environment_yank {
struct Input {
    float seconds{};
    bool gameplay{}, eligible{};  // game itself advertises L1+R1
    Vec3 head{};
    std::array<Vec3, 2> hands{}, aim{}; // OpenXR tracking-space poses
    std::array<bool, 2> tracked{}, free{};
    std::array<float, 2> squeeze{};
};
struct Output {
    uint32_t claimed{}; // bit per grip taken before game_swing::submit
    bool nativeThrow{}; // hold game's L1+R1 for a brief deliberate pulse
    int hand = -1;
};
class GripPull {
  public:
    static constexpr float press = .7f, release = .3f;
    static constexpr float minPull = .17f, minBack = .10f, maxTime = 1.4f;
    static constexpr float throwSeconds = .35f;
    Output update(const Input& in) {
        const float dt = std::isfinite(in.seconds) ? std::clamp(in.seconds, 0.f, .1f) : 0.f;
        Output result;
        if (!in.gameplay) {
            reset();
            for (unsigned i = 0; i < 2; ++i)
                down_[i] = std::isfinite(in.squeeze[i]) && in.squeeze[i] > press;
            return result;
        }
        pulse_ = std::max(0.f, pulse_ - dt);
        result.nativeThrow = pulse_ > 0;
        result.hand = fired_;
        for (unsigned i = 0; i < 2; ++i) {
            const float squeeze = in.squeeze[i];
            const bool held = std::isfinite(squeeze) && squeeze > (down_[i] ? release : press);
            const bool rising = held && !down_[i];
            down_[i] = held;
            if (!held || !in.eligible || !in.tracked[i] || !in.free[i] ||
                !finite(in.head) || !finite(in.hands[i]) || !finite(in.aim[i])) {
                caught_[i] = false;
                continue;
            }
            if (rising && !caught_[i]) {
                caught_[i] = true;
                origin_[i] = in.hands[i] - in.head;
                direction_[i] = normalized(in.aim[i]);
                age_[i] = 0.f;
            }
            if (!caught_[i])
                continue;
            age_[i] += dt;
            if (age_[i] > maxTime || length(direction_[i]) < .5f) {
                caught_[i] = false;
                continue;
            }
            result.claimed |= 1u << i;
            const Vec3 pulled = origin_[i] - (in.hands[i] - in.head);
            // A pull toward the player and away from the point originally
            // aimed at, not small controller tremors, triggers the native throw.
            if (!firedInGrip_[i] && length(pulled) >= minPull &&
                dot(pulled, direction_[i]) >= minBack) {
                pulse_ = throwSeconds;
                fired_ = static_cast<int>(i);
                firedInGrip_[i] = true;
                result.nativeThrow = true;
                result.hand = static_cast<int>(i);
            }
        }
        for (unsigned i = 0; i < 2; ++i)
            if (!down_[i])
                firedInGrip_[i] = false;
        return result;
    }
    void reset() {
        down_ = {};
        caught_ = {};
        firedInGrip_ = {};
        pulse_ = 0;
        fired_ = -1;
        age_ = {};
    }
  private:
    std::array<bool, 2> down_{}, caught_{}, firedInGrip_{};
    std::array<Vec3, 2> origin_{}, direction_{};
    std::array<float, 2> age_{};
    float pulse_{};
    int fired_ = -1;
};
} // namespace spidy::environment_yank
