#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
//
// Version 0.0.7 HERO PROFILES: BATMAN, WOLVERINE, SUPERMAN and PUNISHER (hero_modes::Hero 4-7).
// Every hit here is a blow on sm2_damage's path (DirectDamage, CONFIRMED in rep0050: 34 queued, 34
// sent, 0 faults); the amounts are the mod's own choices (estimates, like 0.0.6's batonHit).
//  - BATMAN: a batarang in each hand (hero_modes::Baton, two in all). The trigger throws that hand's
//    batarang at the enemy the strong aim assist picks; every hit is a batarangHit blow; after each
//    hit it rolls BATARANG CHAIN (default 25%) to bounce to the nearest other enemy within 5 m - at
//    most 3 enemies in all (hero_modes::batonMaxChains), then it comes back. GRAPPLE: the game's
//    own zip / point launch (LT + RT) is kept for him (hero_modes::stripPad); every other web is off.
//    KNOCKDOWN and DISARM rolls are NOT built: no CONFIRMED game function for either (research).
//  - WOLVERINE: CLAWS - hold the trigger and the hand's claws are out; their sweep hits like the
//    0.0.62 baton swing (hero_modes::BatonSwing) with clawHit. RAPID HEALING (Healing): after
//    healDelay seconds without losing health, health climbs healShare of the most a second, into
//    the CONFIRMED HeroHealth record only, never above the game's own cap and never a revive
//    (extras385::invincible::refill's rules).
//  - SUPERMAN: HEAT VISION (HeatVision) - both triggers held (not while FLY MODE is flying: the
//    triggers are flight's) burn along the gaze; heatTick blows every heatTickSeconds to each enemy
//    the ray passes; 3 s at most, then it cools for 2 s. COLD BREATH: the same with a hand at the
//    mouth - freezeTick blows and a frost look only (no CONFIRMED freeze: nobody is frozen). SUPER
//    STRENGTH: a landed punch's extra superPunch blow. SUPERSPEED: FLY MODE's SUPERSONIC dash only
//    (no CONFIRMED ground-speed control).
//  - PUNISHER: TWO PISTOLS (one on each hip, each trigger fires the gun in that hand) and a SAWED-OFF
//    on the back (both barrels, a pellet spread), drawn and holstered with holster.hpp's Rack. Ammo
//    and reload (Gun): EASY RELOAD AUTO refills an empty gun after a short wait; REALISTIC drops the
//    magazine (or breaks the sawed-off open) with a quick downward flick of the gun and reloads it
//    when the gun touches your hip. Spent cases fly out (Shells, a look, capped at shellsMost).
//  - BATMAN's SMOKE: a puff where a batarang strikes (Smoke) - a look only, it hides nothing.
// Pure; no game memory, no address.
#include "spidy/extras385.hpp"
#include "spidy/hero_modes.hpp"
#include "spidy/math.hpp"
#include "spidy/sm2_damage.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>

namespace spidy::hero007 {

using hero_modes::BatonEnemy;

// ---- blows (sm2_damage's CONFIRMED path; amounts are estimates) -------------------------------
inline sm2_damage::Blow batarangHit() { return {20.f, 5, 80.f}; }  // ~5 a thug (a batarang is light)
inline sm2_damage::Blow clawHit() { return {24.f, 5, 90.f}; }      // ~4 slashes a thug
inline sm2_damage::Blow heatTick() { return {12.f, 0, 0.f}; }      // no knock: a burn, ~1.7 s a thug
inline sm2_damage::Blow pistolHit() { return {18.f, 5, 60.f}; }    // ~6 shots a thug

// ---- BATMAN ------------------------------------------------------------------------------------
// BATARANG CHAIN values (combat38::batarangChain): 25% (0, the default), 50%, 75%, 100%, OFF.
inline float batarangChance(uint32_t value) {
    static constexpr float chances[5] = {.25f, .5f, .75f, 1.f, 0.f};
    return value < 5 ? chances[value] : .25f;
}
inline constexpr uint32_t batarangsInAll = 2; // one in each hand

// ---- WOLVERINE ---------------------------------------------------------------------------------
// The claws from the hand along its aim: grip 5 cm ahead of the knuckles, tips 33 cm out (the
// comics' ~30 cm blades).
inline void clawShaft(Vec3 hand, Vec3 aim, Vec3& grip, Vec3& tip) {
    const Vec3 a = length(aim) > 1e-4f ? normalized(aim) : Vec3{0, 0, -1};
    grip = hand + a * .05f;
    tip = hand + a * .33f;
}
inline constexpr float healDelay = 3.f;   // s without losing health before healing starts
inline constexpr float healShare = .10f;  // of the most a second (10 s from 0 to full)
struct Healing {
    float quiet = 0;    // s since health last fell
    float hpWas = -1;   // last health seen (-1: none yet)
    uint64_t heals{};   // frames that wrote
    // What to write into the CONFIRMED health record this frame, or a negative number for nothing.
    float step(float dt, float hp, float most, float reduce) {
        const float t = std::isfinite(dt) ? std::clamp(dt, 0.f, .1f) : 0.f;
        if (!std::isfinite(hp)) {
            hpWas = -1;
            quiet = 0;
            return -1;
        }
        if (hpWas >= 0 && hp < hpWas - .01f)
            quiet = 0; // hurt: wait again
        else
            quiet += t;
        hpWas = hp;
        const float cap = extras385::invincible::refill(hp, most, reduce); // never a revive, never above
        if (cap < 0 || quiet < healDelay)
            return -1;
        const float to = std::min(cap, hp + healShare * most * t);
        if (!(to > hp))
            return -1;
        hpWas = to;
        ++heals;
        return to;
    }
};

// ---- SUPERMAN ----------------------------------------------------------------------------------
inline constexpr float heatTickSeconds = .2f, heatMaxOn = 3.f, heatCool = 2.f;
inline constexpr float heatRange = 30.f, heatRadius = .5f, heatAssistDeg = 12.f;
struct HeatVision {
    float on = 0, tickIn = 0, coolLeft = 0;
    bool firing{}, needRelease{};
    uint32_t ticks{}, overheats{};
    // One frame; held: both triggers. Returns true on a damage tick (the first one at once).
    bool update(float dt, bool held) {
        const float t = std::isfinite(dt) ? std::clamp(dt, 0.f, .1f) : 0.f;
        if (coolLeft > 0)
            coolLeft = std::max(0.f, coolLeft - t);
        if (!held)
            needRelease = false;
        if (!held || coolLeft > 0 || needRelease) {
            firing = false;
            on = tickIn = 0;
            return false;
        }
        firing = true;
        on += t;
        if (on > heatMaxOn) { // too long: it cools, and a fresh hold is needed
            ++overheats;
            coolLeft = heatCool;
            needRelease = true;
            firing = false;
            on = tickIn = 0;
            return false;
        }
        tickIn -= t;
        if (tickIn > 0)
            return false;
        tickIn += heatTickSeconds;
        if (tickIn <= 0)
            tickIn = heatTickSeconds;
        ++ticks;
        return true;
    }
};
// The eyes and the gaze: 8 cm ahead of the head along its forward (pitch kept).
inline bool heatRay(Vec3 head, Vec3 forward, Vec3& from, Vec3& dir) {
    if (!finite(head) || !finite(forward) || length(forward) < 1e-3f)
        return false;
    dir = normalized(forward);
    from = head + dir * .08f;
    return true;
}
// The gaze bent onto the chest nearest it within heatAssistDeg and heatRange (-1 none: dir kept).
inline int heatAim(Vec3 from, Vec3& dir, const BatonEnemy* list, uint32_t count) {
    if (!list || !finite(dir) || length(dir) < 1e-3f)
        return -1;
    const Vec3 d = normalized(dir);
    const float cosMax = std::cos(heatAssistDeg * 3.14159265f / 180.f);
    int best = -1;
    float bestCos = cosMax;
    for (uint32_t i = 0; i < count; ++i) {
        const Vec3 to = list[i].chest - from;
        const float m = length(to);
        if (!list[i].id || list[i].down || !(m > .3f) || m > heatRange)
            continue;
        const float c = dot(to / m, d);
        if (c > bestCos) {
            bestCos = c;
            best = static_cast<int>(i);
        }
    }
    if (best >= 0)
        dir = normalized(list[best].chest - from);
    return best;
}
// The enemies the ray burns: body (chest -1.0 .. +0.6 m) within heatRadius, 0.3 m to heatRange.
inline int heatHits(Vec3 from, Vec3 dir, const BatonEnemy* list, uint32_t count, int* hit, int most) {
    int n = 0;
    if (!finite(from) || !finite(dir) || !list || !hit)
        return 0;
    const Vec3 end = from + normalized(dir) * heatRange;
    for (uint32_t i = 0; i < count && n < most; ++i) {
        const Vec3 c = list[i].chest;
        if (!list[i].id || list[i].down || !finite(c))
            continue;
        if (hero_modes::segmentGap(from + normalized(dir) * .3f, end, c - Vec3{0, 1.f, 0}, c + Vec3{0, .6f, 0}) <=
            heatRadius)
            hit[n++] = static_cast<int>(i);
    }
    return n;
}

// ---- PUNISHER ----------------------------------------------------------------------------------
enum Weapon : uint32_t { pistol = 0, sawedOff = 1 };
struct GunSpec {
    uint32_t rounds;    // a full magazine (or both barrels)
    float cooldown;     // s between shots
    float range;        // m
    uint32_t pellets;   // per shot
    float spreadDeg;    // the pellets' cone (half angle)
    float amount;       // hit points a pellet (at full strength)
    float knockAmount;  // knock a pellet
    float autoReload;   // s: EASY RELOAD AUTO's refill
};
inline constexpr GunSpec pistolSpec{15, .18f, 60.f, 1, 0.f, 18.f, 60.f, 1.2f};
// The sawed-off: two barrels, one shell each fired per pull; 9 pellets of 00 buck in a 7 deg cone.
inline constexpr GunSpec sawedOffSpec{2, .35f, 20.f, 9, 7.f, 9.f, 25.f, 1.0f};
inline const GunSpec& specOf(uint32_t w) { return w == sawedOff ? sawedOffSpec : pistolSpec; }
inline constexpr float gunHitRadius = .3f, pistolAssistDeg = 4.f;
// The pellet directions about `dir` (unit): the centre, then rings (a fixed pattern: repeatable).
inline int pelletDirs(Vec3 dir, uint32_t pellets, float spreadDeg, Vec3* out, int most) {
    if (!finite(dir) || length(dir) < 1e-4f || !out || most <= 0)
        return 0;
    const Vec3 d = normalized(dir);
    const Vec3 up = std::abs(d.y) < .95f ? Vec3{0, 1, 0} : Vec3{1, 0, 0};
    const Vec3 r = normalized(cross(d, up)), u = cross(r, d);
    int n = 0;
    out[n++] = d;
    const float spread = std::tan(spreadDeg * 3.14159265f / 180.f);
    for (uint32_t k = 1; k < pellets && n < most; ++k) {
        const float ring = k <= 4 ? .5f : 1.f; // 4 inside, the rest on the rim
        const float a = static_cast<float>(k) * 2.39996f; // the golden angle
        out[n++] = normalized(d + (r * std::cos(a) + u * std::sin(a)) * (spread * ring));
    }
    return n;
}
// Pellet strength by distance: full to 6 m, down to 40% at the range (the pistol: full).
inline float falloff(const GunSpec& g, float metres) {
    if (g.pellets <= 1 || metres <= 6.f)
        return 1.f;
    return std::clamp(1.f - .6f * (metres - 6.f) / std::max(g.range - 6.f, 1.f), .4f, 1.f);
}
// One shot: each enemy's hit points from it (amount[i], indices into list; 0: missed). A pellet
// stops at the first body it passes (within gunHitRadius). The pistol's lone round snaps to a chest
// within pistolAssistDeg when it passes nobody. Returns how many enemies were hit.
inline int shoot(const GunSpec& g, Vec3 from, Vec3 dir, const BatonEnemy* list, uint32_t count, float* amount) {
    if (!list || !amount || !finite(from))
        return 0;
    for (uint32_t i = 0; i < count; ++i)
        amount[i] = 0;
    Vec3 dirs[16];
    const int n = pelletDirs(dir, g.pellets, g.spreadDeg, dirs, 16);
    for (int p = 0; p < n; ++p) {
        int best = -1;
        float bestAlong = g.range;
        for (uint32_t i = 0; i < count; ++i) {
            const Vec3 c = list[i].chest;
            if (!list[i].id || list[i].down || !finite(c))
                continue;
            const float along = dot(c - from, dirs[p]);
            if (along < .2f || along > bestAlong)
                continue;
            if (hero_modes::segmentGap(from, from + dirs[p] * g.range, c - Vec3{0, 1.f, 0}, c + Vec3{0, .6f, 0}) <=
                gunHitRadius) {
                best = static_cast<int>(i);
                bestAlong = along;
            }
        }
        if (best < 0 && g.pellets == 1) { // the pistol's small assist
            const float cosMax = std::cos(pistolAssistDeg * 3.14159265f / 180.f);
            float bestCos = cosMax;
            for (uint32_t i = 0; i < count; ++i) {
                const Vec3 to = list[i].chest - from;
                const float m = length(to);
                if (!list[i].id || list[i].down || !(m > .2f) || m > g.range)
                    continue;
                const float c = dot(to / m, dirs[p]);
                if (c > bestCos) {
                    bestCos = c;
                    best = static_cast<int>(i);
                    bestAlong = m;
                }
            }
        }
        if (best >= 0)
            amount[best] += g.amount * falloff(g, bestAlong);
    }
    int hit = 0;
    for (uint32_t i = 0; i < count; ++i)
        hit += amount[i] > 0;
    return hit;
}
// A shot's blow for one enemy (the pellets that hit him summed: one blow, never a flood).
inline sm2_damage::Blow shotBlow(const GunSpec& g, float amount) {
    const float pellets = g.amount > 0 ? amount / g.amount : 1.f;
    return {amount, 5, std::min(g.knockAmount * std::max(pellets, 1.f), 300.f)};
}

// ---- RELOAD ------------------------------------------------------------------------------------
// EASY RELOAD values (combat38::easyReload): AUTO (0, the default) or REALISTIC (1).
inline constexpr uint32_t reloadAuto = 0, reloadRealistic = 1;
// REALISTIC's gestures. A flick: the gun hand moving down faster than flickSpeed. The hip: the hand
// at least hipBelowHead under the head, beside the body (hipSide off the centre line), not far
// in front or behind (hipReach).
inline constexpr float flickSpeed = 2.5f, hipBelowHead = .55f, hipSide = .12f, hipReach = .35f;
inline constexpr float seatSeconds = .35f; // a magazine (or shells) seating at the hip
inline bool flickDown(Vec3 handVelocity) { return finite(handVelocity) && handVelocity.y < -flickSpeed; }
inline bool atHip(Vec3 hand, Vec3 head, Vec3 forward) {
    Vec3 f{forward.x, 0, forward.z};
    if (!finite(hand) || !finite(head) || !finite(f) || length(f) < 1e-3f)
        return false;
    f = normalized(f);
    const Vec3 right{-f.z, 0, f.x};
    const Vec3 rel = hand - head;
    const float below = -rel.y, side = std::abs(dot(rel, right)), ahead = dot(rel, f);
    return below >= hipBelowHead && below <= 1.2f && side >= hipSide && side <= .45f && std::abs(ahead) <= hipReach;
}
enum GunState : uint32_t { ready = 0, empty, opened, seating, autoLoading };
inline constexpr const char* gunStateNames[] = {"READY", "EMPTY", "MAG OUT", "SEATING", "RELOADING"};
struct Gun {
    uint32_t weapon = pistol;
    uint32_t rounds = pistolSpec.rounds;
    uint32_t state = ready;
    float cool = 0, wait = 0;
    bool wasDown{};
    uint64_t shots{}, dryPulls{}, reloads{}, drops{};
    void arm(uint32_t w) {
        *this = Gun{};
        weapon = w == sawedOff ? sawedOff : pistol;
        rounds = specOf(weapon).rounds;
    }
    const GunSpec& spec() const { return specOf(weapon); }
    // One frame. trigger: held now; flick / hip: REALISTIC's gestures this frame. Returns true on
    // the frame a round is fired.
    bool update(float dt, bool trigger, uint32_t mode, bool flick, bool hip) {
        const float t = std::isfinite(dt) ? std::clamp(dt, 0.f, .1f) : 0.f;
        const bool press = trigger && !wasDown;
        wasDown = trigger;
        cool = std::max(0.f, cool - t);
        const GunSpec& g = spec();
        if (mode != reloadRealistic && (state == opened || state == seating || state == empty)) {
            state = autoLoading; // switched to AUTO mid-reload: finish it the easy way
            wait = g.autoReload;
        }
        switch (state) {
        case autoLoading:
            wait -= t;
            if (wait <= 0) {
                rounds = g.rounds;
                state = ready;
                ++reloads;
            }
            return false;
        case seating:
            wait -= t;
            if (wait <= 0) {
                rounds = g.rounds;
                state = ready;
                ++reloads;
            }
            return false;
        case opened:
            if (hip) {
                state = seating;
                wait = seatSeconds;
            }
            if (press)
                ++dryPulls;
            return false;
        case empty:
            if (press)
                ++dryPulls;
            if (flick) {
                state = opened;
                ++drops;
            }
            return false;
        default: // ready
            if (mode == reloadRealistic && flick && rounds < g.rounds) { // a tactical drop
                state = opened;
                rounds = 0;
                ++drops;
                return false;
            }
            if (!press || cool > 0)
                return false;
            if (rounds == 0) { // should not happen; keep it consistent
                state = mode == reloadRealistic ? empty : autoLoading;
                wait = g.autoReload;
                return false;
            }
            --rounds;
            ++shots;
            cool = g.cooldown;
            if (rounds == 0) {
                state = mode == reloadRealistic ? empty : autoLoading;
                wait = g.autoReload;
            }
            return true;
        }
    }
};

// ---- SUPERMAN: COLD BREATH, SUPER STRENGTH, SUPERSPEED ----------------------------------------
// COLD BREATH: both triggers with a hand at the mouth (within mouthReach of the eyes' point,
// ahead of the head). Damage only (freezeTick, no knock) and a visual: there is NO CONFIRMED way to
// freeze an enemy in place, so nobody is frozen. Same clock and overheat rules as HEAT VISION.
inline sm2_damage::Blow freezeTick() { return {8.f, 0, 0.f}; }
inline constexpr float mouthReach = .2f, freezeRange = 12.f, freezeRadius = .8f;
inline bool handAtMouth(Vec3 hand, const Pose& head) {
    if (!finite(hand) || !finite(head.position))
        return false;
    const Vec3 mouth = head.position + head.orientation.rotate(Vec3{0, -.09f, -.08f});
    return length(hand - mouth) <= mouthReach;
}
// SUPER STRENGTH: a landed punch gets one more blow, like DAREDEVIL PUNCH's extra (2 normal
// punches' worth more) with a bigger knock - not HULK's one-punch knockout.
inline sm2_damage::Blow superPunch() { return {2.f * hero_modes::punchUnitHp, 5, 400.f}; }
// SUPERSPEED: a movement boost only - SUPERMAN keeps FLY MODE's SUPERSONIC dash (fly_boost.hpp).
// There is no CONFIRMED ground-speed control, so nothing on foot changes.

// ---- BATMAN: SMOKE (a visual only) ----------------------------------------------------------------
// A puff where a batarang strikes: grey soft discs swelling and fading over smokeSeconds. It hides
// nothing from enemies (no CONFIRMED stealth or blind function): looks only.
inline constexpr float smokeSeconds = 1.6f;
inline constexpr uint32_t smokeMost = 6;
struct Smoke {
    struct Puff {
        Vec3 at{};
        float age = 1e9f;
    };
    Puff puffs[smokeMost]{};
    uint32_t next{}, made{};
    void add(Vec3 at) {
        if (!finite(at))
            return;
        puffs[next] = {at, 0.f};
        next = (next + 1) % smokeMost; // the oldest goes first: never more than smokeMost
        ++made;
    }
    void update(float dt) {
        const float t = std::isfinite(dt) ? std::clamp(dt, 0.f, .1f) : 0.f;
        for (auto& p : puffs)
            p.age += t;
    }
    uint32_t live() const {
        uint32_t n = 0;
        for (const auto& p : puffs)
            n += p.age < smokeSeconds;
        return n;
    }
};

// ---- PUNISHER: SHELL EJECTION (a visual, capped) -------------------------------------------------
// Each shot throws a spent case out of the gun's right side (the sawed-off: two shells when it is
// broken open). They fall under gravity and vanish after shellSeconds; never more than shellsMost
// at once (the oldest goes first), so a long fight costs nothing more.
inline constexpr uint32_t shellsMost = 24;
inline constexpr float shellSeconds = 1.2f;
struct Shells {
    struct Shell {
        Vec3 at{}, velocity{};
        float age = 1e9f;
        bool shotgun{};
    };
    Shell list[shellsMost]{};
    uint32_t next{}, thrown{};
    void eject(Vec3 at, Vec3 right, bool shotgun) {
        if (!finite(at) || !finite(right))
            return;
        const float jitter = static_cast<float>(thrown % 5) * .08f;
        list[next] = {at, right * (1.6f + jitter) + Vec3{0, 1.4f, 0}, 0.f, shotgun};
        next = (next + 1) % shellsMost;
        ++thrown;
    }
    void update(float dt) {
        const float t = std::isfinite(dt) ? std::clamp(dt, 0.f, .1f) : 0.f;
        for (auto& s : list) {
            if (s.age >= shellSeconds)
                continue;
            s.age += t;
            s.velocity.y -= 9.8f * t;
            s.at = s.at + s.velocity * t;
        }
    }
    uint32_t live() const {
        uint32_t n = 0;
        for (const auto& s : list)
            n += s.age < shellSeconds;
        return n;
    }
};

// ---- the ammo readout -----------------------------------------------------------------------------
// The text the gun's side shows: "12/15", "EMPTY", "MAG OUT", "RELOAD...".
inline void ammoText(const Gun& g, char* out, size_t size) {
    if (!out || size < 2)
        return;
    switch (g.state) {
    case empty: std::snprintf(out, size, "EMPTY"); break;
    case opened: std::snprintf(out, size, g.weapon == sawedOff ? "OPEN" : "MAG OUT"); break;
    case seating:
    case autoLoading: std::snprintf(out, size, "RELOAD"); break;
    default: std::snprintf(out, size, "%u/%u", g.rounds, g.spec().rounds); break;
    }
}

} // namespace spidy::hero007
