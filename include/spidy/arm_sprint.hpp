#pragma once
#include "math.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>

// HEROVR: grounded Spider-Man sprint with real alternating arm motion.
// Left thumbstick forward is the player's explicit locomotion intent; only
// repeated opposite forward/back hand movement can activate a virtual L3.
// No headset/bone movement is made, and a tracking loss releases sprint.
namespace spidy::arm_sprint {
struct Input {
    float seconds{}, forwardStick{};
    bool enabled{}, tracked[2]{};
    Vec3 head{}, headForward{}, hands[2]{};
};
class Detector {
  public:
    static constexpr float stickThreshold = .72f, armVelocity = .30f,
                           maxBeatGap = .7f, keepAlive = .60f;
    bool update(const Input& in) {
        const float dt = std::isfinite(in.seconds) ? std::clamp(in.seconds, .001f, .1f) : 0.f;
        Vec3 axis{in.headForward.x, 0, in.headForward.z};
        const float len = length(axis);
        const bool good = in.enabled && dt > 0 && std::isfinite(in.forwardStick) &&
                          in.forwardStick >= stickThreshold && in.tracked[0] && in.tracked[1] &&
                          finite(in.head) && finite(in.hands[0]) && finite(in.hands[1]) &&
                          finite(axis) && len > .3f;
        if (!good) {
            reset();
            return false;
        }
        axis = axis / len;
        const Vec3 rel[2]{in.hands[0] - in.head, in.hands[1] - in.head};
        if (!seen_) {
            previous_[0] = rel[0];
            previous_[1] = rel[1];
            seen_ = true;
            return false;
        }
        const float v0 = dot(rel[0] - previous_[0], axis) / dt;
        const float v1 = dot(rel[1] - previous_[1], axis) / dt;
        previous_[0] = rel[0];
        previous_[1] = rel[1];
        sinceBeat_ += dt;
        // Moving both hands together (e.g. leaning the whole body), static
        // hands, or incidental controller jitter never count as running.
        const bool counterphase = std::isfinite(v0) && std::isfinite(v1) &&
                                  std::abs(v0) >= armVelocity && std::abs(v1) >= armVelocity &&
                                  v0 * v1 < 0.f;
        if (counterphase) {
            const int direction = v0 > 0.f ? 1 : -1;
            if (direction != lastDirection_) {
                if (lastDirection_ != 0 && sinceBeat_ <= maxBeatGap)
                    beats_ = std::min(beats_ + 1, 8u);
                else
                    beats_ = 0;
                lastDirection_ = direction;
                sinceBeat_ = 0.f;
            }
        }
        if (sinceBeat_ > keepAlive) {
            beats_ = 0;
            lastDirection_ = 0;
            return false;
        }
        return beats_ >= 2;
    }
    void reset() {
        seen_ = false;
        previous_[0] = previous_[1] = {};
        sinceBeat_ = 0.f;
        beats_ = 0;
        lastDirection_ = 0;
    }
  private:
    Vec3 previous_[2]{};
    bool seen_{};
    int lastDirection_{};
    unsigned beats_{};
    float sinceBeat_{};
};
} // namespace spidy::arm_sprint
