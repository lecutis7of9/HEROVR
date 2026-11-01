// HERO VR 0.0.76 SM1: the fight's pure part (sm1_combat.hpp). Made by Agent Mulder.
#include "spidy/sm1_combat.hpp"

#include <algorithm>
#include <cmath>

namespace spidy::sm1::combat {
namespace {
struct Rule {
    const char* name;
    uint32_t bits;
};
// His game_targets.cpp rules, the ones the enemies need (decorated MSVC names).
constexpr Rule rules[] = {
    {".?AVBotMoverManager@@", bot},
    {".?AVHoverMoverManager@@", hover},
    {".?AVThugBot@@", thug},
    {".?AVCivilianBot@@", civilian},
    {".?AVAllyBot@@", ally},
    {".?AVMissionFollowBot@@", ally},
    {".?AVStatusEffectTrackerWebbed@@", webbable},
    {".?AVBirdBot@@", neutral},
    {".?AVHelicopter@@", neutral},
    {".?AVSilverSableCraftBot@@", neutral},
    {".?AVHealth@@", health},
    {".?AVBotHealth@@", botHealth},
    {".?AVBotStateDeathAnim@@", down},
    {".?AVBotStateRagdoll@@", down},
    {".?AVBotStateWallStuck@@", down},
    {".?AVBotStateAttachToActor@@", held},
    {".?AVBotStateCarried@@", held},
    {".?AVBotStatePlayCinematic@@", held},
    {".?AVBotStateNull@@", held},
    {".?AVBotStatePlaySyncedAnimGuest@@", held},
    {".?AVBotStateSlottedInteract@@", held},
    {".?AVBotStateFrozen@@", held},
    {".?AVBotStateRequestTakedownYank@@", held},
};
// The body's axis: its feet plus low .. high.
void axis(const Target& t, Vec3& a, Vec3& b) {
    a = t.feet + Vec3{0, t.size.low + t.size.radius, 0};
    b = t.feet + Vec3{0, std::max(t.size.low + t.size.radius, t.size.high - t.size.radius), 0};
}
float pointSegment(Vec3 p, Vec3 a, Vec3 b) {
    const Vec3 ab = b - a;
    const float n = dot(ab, ab);
    const float s = n > 1e-9f ? std::clamp(dot(p - a, ab) / n, 0.f, 1.f) : 0.f;
    return length(p - (a + ab * s));
}
// The closest distance between two segments (Ericson, Real-Time Collision Detection 5.1.9).
float segmentSegment(Vec3 p1, Vec3 q1, Vec3 p2, Vec3 q2) {
    const Vec3 d1 = q1 - p1, d2 = q2 - p2, r = p1 - p2;
    const float a = dot(d1, d1), e = dot(d2, d2), f = dot(d2, r);
    float s = 0, t = 0;
    if (a <= 1e-9f && e <= 1e-9f)
        return length(r);
    if (a <= 1e-9f) {
        t = std::clamp(f / e, 0.f, 1.f);
    } else {
        const float c = dot(d1, r);
        if (e <= 1e-9f) {
            s = std::clamp(-c / a, 0.f, 1.f);
        } else {
            const float b = dot(d1, d2), den = a * e - b * b;
            s = den > 1e-9f ? std::clamp((b * f - c * e) / den, 0.f, 1.f) : 0.f;
            t = (b * s + f) / e;
            if (t < 0) {
                t = 0;
                s = std::clamp(-c / a, 0.f, 1.f);
            } else if (t > 1) {
                t = 1;
                s = std::clamp((b - c) / a, 0.f, 1.f);
            }
        }
    }
    return length((p1 + d1 * s) - (p2 + d2 * t));
}
bool usable(const Target& t) {
    return t.record && finite(t.feet) && !(t.hp == 0);
}
} // namespace

uint32_t ruleBits(std::string_view decorated) {
    uint32_t bits = 0;
    for (const auto& r : rules)
        if (decorated == r.name)
            bits |= r.bits;
    return bits;
}
float bodyDistance(const Target& t, Vec3 p) {
    Vec3 a, b;
    axis(t, a, b);
    return std::max(0.f, pointSegment(p, a, b) - t.size.radius);
}
float segmentDistance(const Target& t, Vec3 p, Vec3 q) {
    Vec3 a, b;
    axis(t, a, b);
    return std::max(0.f, segmentSegment(p, q, a, b) - t.size.radius);
}
int aimed(const std::vector<Target>& list, Vec3 from, Vec3 dir, float range, float coneDegrees, float slack) {
    const Vec3 d = normalized(dir);
    if (!(length(d) > .5f) || !finite(from))
        return -1;
    const float cosCone = std::cos(coneDegrees * 3.14159265f / 180);
    int best = -1;
    float bestScore = 1e30f;
    for (size_t i = 0; i < list.size(); ++i) {
        const Target& t = list[i];
        if (!usable(t))
            continue;
        const Vec3 to = t.chest() - from;
        const float dist = length(to);
        if (!(dist > .05f) || dist > range + t.size.radius)
            continue;
        const float c = dot(to, d) / dist;
        // On the line: the body within `slack` of the shot's line, in front.
        const bool onLine = dot(to, d) > 0 && segmentDistance(t, from, from + d * range) <= slack;
        if (c < cosCone && !onLine)
            continue;
        const float angle = std::acos(std::clamp(c, -1.f, 1.f));
        const float score = (onLine ? 0.f : angle) * 10 + dist * .01f;
        if (score < bestScore) {
            bestScore = score;
            best = static_cast<int>(i);
        }
    }
    return best;
}
std::vector<int> nearby(const std::vector<Target>& list, Vec3 p, float reach, size_t most) {
    std::vector<std::pair<float, int>> hits;
    for (size_t i = 0; i < list.size(); ++i)
        if (usable(list[i])) {
            const float d = bodyDistance(list[i], p);
            if (d <= reach)
                hits.push_back({d, static_cast<int>(i)});
        }
    std::sort(hits.begin(), hits.end());
    std::vector<int> out;
    for (size_t i = 0; i < hits.size() && out.size() < most; ++i)
        out.push_back(hits[i].second);
    return out;
}
std::vector<int> along(const std::vector<Target>& list, Vec3 a, Vec3 b, float reach, size_t most) {
    std::vector<std::pair<float, int>> hits;
    for (size_t i = 0; i < list.size(); ++i)
        if (usable(list[i]) && segmentDistance(list[i], a, b) <= reach)
            hits.push_back({length(list[i].chest() - a), static_cast<int>(i)});
    std::sort(hits.begin(), hits.end());
    std::vector<int> out;
    for (size_t i = 0; i < hits.size() && out.size() < most; ++i)
        out.push_back(hits[i].second);
    return out;
}
bool HitLimiter::allow(uint64_t record, double now, double every) {
    if (last_.size() > 512)
        for (auto it = last_.begin(); it != last_.end();)
            it = now - it->second > 30 ? last_.erase(it) : std::next(it);
    auto [it, fresh] = last_.try_emplace(record, now);
    if (fresh)
        return true;
    if (now - it->second < every)
        return false;
    it->second = now;
    return true;
}
} // namespace spidy::sm1::combat
