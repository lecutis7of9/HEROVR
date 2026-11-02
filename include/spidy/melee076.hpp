#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
// Made by Agent Mulder.
//
// HERO VR 0.0.76 MELEE SNAP (test 801ce57: "hits are still hard to land as spiderman"; the owner: punches must reliably
// connect with the nearest enemy he swings at, never a whiff with an enemy reasonably close). What the run showed:
// - in combat most swings were held back as "too slow" (the lunge owned them: a lock on an enemy past LUNGE FROM), and
//   a few as "not forward" / "not extended" (the punch direction measured toward the lock, not the enemy swung at);
// - the lunge's aim pick turned punches at a thug 1.2 - 2.7 m away into the game's Y web strike at someone else.
// The snap (pure, the mod's own state only; no game address): among the enemies in sight within `reach` (flat from the
// head; 3D in the air), the one the fist is thrown toward (within `coneDegrees` of the fist's way, nearer ones and
// smaller angles first), or, with none in that cone, the nearest one within `pointBlank` in front of the look. A swing
// with a snap target is a punch at HIM: its forward and reach measured toward him (the usual PUNCH SENSITIVITY needs,
// so an arm swing while walking is still no punch), never the lunge's, never the zip strike; the blow (PUNCH HITS' CONFIRMED path) and the
// game's X go to him. `closeLunge`: any enemy in sight this near keeps the lunge (the game's Y) from opening at all.
//
// HERO VR 0.0.76 PUNCH DASH (test 32bd648: "hits land more but now you have to run to them, before if they were far
// and you hit in their direction you would kinda dash right to them with the punch"). The snap's reach is now `reach`
// (2 m): only an enemy that close is a plain punch. A punch thrown toward an enemy in sight past `reach` and within
// `dashReach` (inside `dashDegrees` of the fist's way; the angle first, then the nearer) is the dash: the game's own
// web strike (Y) at HIM - the enemy the fist points at, never the lock or one farther off. The swing is the dash's
// (never thrown away as too slow); a close enemy the fist is thrown at is always the punch, never the dash.
#include "spidy/math.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <span>

namespace spidy::melee076 {

inline constexpr float reach = 2.0f, pointBlank = 1.8f, coneDegrees = 70.f, closeLunge = reach;
inline constexpr float dashReach = 15.f, dashDegrees = 35.f; // 0.0.76 PUNCH DASH

struct Foe {
    Vec3 middle{};
    bool seen = true; // false: his sight ray is blocked (a wall between)
};
struct Snap {
    int index = -1; // into the foes; -1: none
    Vec3 way{};     // unit, from the head to him (flat on the ground, 3D in the air)
    float metres{};
    bool pointBlank{}; // picked as the nearest in front, not by the fist's way
    explicit operator bool() const { return index >= 0; }
};

inline Vec3 flat(Vec3 v) { return {v.x, 0, v.z}; }

// head: the eyes; fistWay: the fist's velocity (any length; zero: unknown); look: the head's look; airborne: 3D.
inline Snap pick(std::span<const Foe> foes, Vec3 head, Vec3 fistWay, Vec3 look, bool airborne) {
    Snap best;
    if (!finite(head))
        return best;
    const auto shape = [&](Vec3 v) { return airborne ? v : flat(v); };
    const Vec3 fist = finite(fistWay) ? shape(fistWay) : Vec3{};
    const bool fistKnown = length(fist) > .3f;
    const Vec3 fw = fistKnown ? normalized(fist) : Vec3{};
    const Vec3 lk = finite(look) && length(flat(look)) > .1f ? normalized(flat(look)) : Vec3{};
    const float cone = std::cos(coneDegrees * .0174533f);
    float bestScore = 1e9f;
    // 1: thrown toward him
    for (size_t e = 0; fistKnown && e < foes.size(); ++e) {
        if (!foes[e].seen || !finite(foes[e].middle))
            continue;
        const Vec3 to = shape(foes[e].middle - head);
        const float d = length(to);
        if (!(d <= reach) || d < 1e-3f)
            continue;
        const float c = dot(fw, to / d);
        if (c < cone)
            continue;
        const float score = (1.f - c) * 4.f + d * .25f; // the angle first, then the nearer
        if (score < bestScore) {
            bestScore = score;
            best.index = static_cast<int>(e);
            best.way = to / d;
            best.metres = d;
        }
    }
    if (best)
        return best;
    // 2: point blank - the nearest within pointBlank in front of the look (or anywhere when the look is unknown)
    float nearest = pointBlank;
    for (size_t e = 0; e < foes.size(); ++e) {
        if (!foes[e].seen || !finite(foes[e].middle))
            continue;
        const Vec3 to = shape(foes[e].middle - head);
        const float d = length(to);
        if (!(d <= nearest) || d < 1e-3f)
            continue;
        if (length(lk) > .5f && dot(flat(to), lk) < 0)
            continue; // behind him
        nearest = d;
        best.index = static_cast<int>(e);
        best.way = to / d;
        best.metres = d;
        best.pointBlank = true;
    }
    return best;
}

// 0.0.76 PUNCH DASH: the enemy in sight past `reach` and within `dashReach` the fist is thrown toward (inside
// `dashDegrees` of its way; the angle first, then the nearer). Flat from the head; 3D in the air. Unknown way: none.
inline Snap dash(std::span<const Foe> foes, Vec3 head, Vec3 fistWay, bool airborne) {
    Snap best;
    if (!finite(head) || !finite(fistWay))
        return best;
    const auto shape = [&](Vec3 v) { return airborne ? v : flat(v); };
    const Vec3 fist = shape(fistWay);
    if (length(fist) <= .3f)
        return best;
    const Vec3 fw = normalized(fist);
    const float cone = std::cos(dashDegrees * .0174533f);
    float bestScore = 1e9f;
    for (size_t e = 0; e < foes.size(); ++e) {
        if (!foes[e].seen || !finite(foes[e].middle))
            continue;
        const Vec3 to = shape(foes[e].middle - head);
        const float d = length(to);
        if (!(d > reach && d <= dashReach))
            continue;
        const float c = dot(fw, to / d);
        if (c < cone)
            continue;
        const float score = (1.f - c) * 20.f + d * .05f; // the angle first, then the nearer
        if (score < bestScore) {
            bestScore = score;
            best.index = static_cast<int>(e);
            best.way = to / d;
            best.metres = d;
        }
    }
    return best;
}

// Any enemy in sight within closeLunge (flat from the head, 3D in the air): the lunge stays shut (punches are X).
inline bool closeFoe(std::span<const Foe> foes, Vec3 head, bool airborne) {
    for (const auto& f : foes) {
        if (!f.seen || !finite(f.middle) || !finite(head))
            continue;
        const Vec3 to = f.middle - head;
        if (length(airborne ? to : flat(to)) <= closeLunge)
            return true;
    }
    return false;
}

} // namespace spidy::melee076
