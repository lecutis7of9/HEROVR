#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky. Made by Agent Mulder.
//
// Version 0.0.62 HIT INDICATOR (combat38 hitIndicator, OFF by default - the user does not want it;
// players asked for it): a brief white "X" of four ticks where a hit of yours lands - a punch with
// an enemy in reach, a baton, a repulsor bolt or beam, a web ball - gone in 0.22 s. Drawn with the
// webs (one draw, no new pass), the same eye-facing soft shapes; nothing is drawn while it is OFF.
// Pure (tests): the marks keep tracking-space points; the caller turns them into this frame's world.
#include "spidy/soft_shapes.hpp"
#include "spidy/vertex.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace spidy::hit_marks {

inline constexpr int most = 8;
inline constexpr float life = .22f;
struct Mark {
    Vec3 at{};
    float age = life;
};
class Marks {
  public:
    // A hit at `at`; the oldest mark gives way when all are showing.
    void add(Vec3 at) {
        if (!finite(at))
            return;
        int s = 0;
        for (int i = 1; i < most; ++i)
            if (marks_[static_cast<size_t>(i)].age > marks_[static_cast<size_t>(s)].age)
                s = i;
        marks_[static_cast<size_t>(s)] = {at, 0.f};
        ++added_;
    }
    void update(float seconds) {
        const float dt = std::isfinite(seconds) ? std::clamp(seconds, 0.f, .1f) : 0.f;
        for (auto& m : marks_)
            m.age = std::min(life, m.age + dt);
    }
    void clear() {
        for (auto& m : marks_)
            m.age = life;
    }
    // 1 when made, 0 once gone.
    float fade(int i) const {
        const Mark& m = marks_[static_cast<size_t>(std::clamp(i, 0, most - 1))];
        return std::clamp(1.f - m.age / life, 0.f, 1.f);
    }
    Vec3 at(int i) const { return marks_[static_cast<size_t>(std::clamp(i, 0, most - 1))].at; }
    int showing() const {
        int n = 0;
        for (int i = 0; i < most; ++i)
            n += fade(i) > 0;
        return n;
    }
    uint64_t added() const { return added_; }

  private:
    std::array<Mark, most> marks_{};
    uint64_t added_{};
};

// The mark: four ticks in an X facing the viewer, about 1.6 degrees across wherever it is (so a
// far hit reads as well as a near one), white with a warm edge, popping slightly larger at first.
// 3 quads a tick (soft edges), 72 vertices.
inline void appendMark(std::vector<Vertex>& out, Vec3 at, Vec3 viewer, float pixelAngle, float fade) {
    if (!finite(at) || !finite(viewer) || !std::isfinite(pixelAngle) || !std::isfinite(fade))
        return;
    const float a = std::clamp(fade, 0.f, 1.f);
    const float range = length(at - viewer);
    if (a < .01f || range < .1f)
        return;
    const Vec3 v = normalized(viewer - at);
    const Vec3 s1 = soft::perpendicular(v), s2 = cross(v, s1);
    const float size = range * .014f * (1.f + .35f * a * a);      // ~1.6 deg across (plus the pop)
    const float floor = 1.5f * std::max(pixelAngle, 1e-5f) * range; // never under 1.5 px
    const float w = std::max(size * .09f, floor), edge = std::max(w * .8f, floor);
    const Vec3 white{1, 1, 1}, warm{1, .55f, .35f};
    const Vec3 p = at + v * .02f; // a hair toward the eye
    for (int k = 0; k < 4; ++k) {
        const float t = (.25f + .5f * static_cast<float>(k)) * 3.14159265f;
        const Vec3 dir = s1 * std::cos(t) + s2 * std::sin(t);
        const Vec3 side = s1 * -std::sin(t) + s2 * std::cos(t);
        const Vec3 q0 = p + dir * (size * .35f), q1 = p + dir * size;
        soft::quad(out, {q0 - side * (w + edge), warm, 0}, {q1 - side * (w + edge), warm, 0}, {q1 - side * w, warm, a},
                   {q0 - side * w, warm, a});
        soft::quad(out, {q0 - side * w, white, a}, {q1 - side * w, white, a}, {q1 + side * w, white, a},
                   {q0 + side * w, white, a});
        soft::quad(out, {q0 + side * w, warm, a}, {q1 + side * w, warm, a}, {q1 + side * (w + edge), warm, 0},
                   {q0 + side * (w + edge), warm, 0});
    }
}

} // namespace spidy::hit_marks
