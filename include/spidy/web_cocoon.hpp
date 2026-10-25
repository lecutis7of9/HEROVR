#pragma once
// Version 0.0.3 WEB BALL HIT (replaces 0.0.2's OURS splash-only hit): SpidyVR's web-ball rules on
// the mod's own ball. In SpidyVR (Spider-Man Remastered) a web ball that strikes a thug deals the
// game's webbing: a kWebImpact blow with 11 webbing (kWebEncase) a hit against a street thug's
// threshold of 30, so the third quick hit webs him up, and the game draws its webbed-up look.
//
// SM2: the same enum names exist (Spider-Man2.exe 2.810.0.0 static, the damage-type name table at
// .data 0x9ca9400: kWebEncase index 21, kWebImpact 22; CANDIDATE values), and the classes:
//   CANDIDATE 0x54be208 ShotWebShooter vtable (RTTI), slot +0xd8 = 0x1694790 (Remastered's hit handler slot)
//   CANDIDATE 0x53c1d98 WeaponWebShooter vtable (RTTI), slot +0x100 = 0x18897e0 (Remastered's options fill)
//   CANDIDATE 0x5367ed0 StatusEffectTrackerWebbed vtable (RTTI): the per-enemy webbing tracker
// NOT FOUND for SM2: the damage call that delivers a blow (Remastered's native_bodies::damage). So
// nothing here writes to the game: the webbing is counted by the mod with SpidyVR's numbers and the
// webbed-up look is the mod's own cocoon of web strands round the enemy (honestly: a mod look; the
// game's AI does not know he is webbed). Pure, allocation-free, tested (core_tests).
#include "math.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace spidy::web_cocoon {
// Version 0.0.4 (rep0030: 4 hits, 0 webbed up): 15 a hit, so the second hit webs him up (SpidyVR's 11
// needed three quick ones); the threshold stays a street thug's 30.
inline constexpr float perHit = 15.f, threshold = 30.f;
inline constexpr float decay = 3.f;                       // webbing lost a second while not encased
inline constexpr float encasedSeconds = 10.f;             // a webbed-up enemy stays wrapped this long
inline constexpr float lostSeconds = 1.5f;                // not in the enemy list this long: gone
inline constexpr float bodyHeight = 1.75f, bodyRadius = .30f;
// The game's candidates, for the record and the probe (read-only; never called).
inline constexpr uint64_t shotWebShooterVtable = 0x54be208, shotHitSlotRva = 0x1694790;
inline constexpr uint64_t webShooterVtable = 0x53c1d98, webShooterOptionsRva = 0x18897e0;
inline constexpr uint64_t webbedTrackerVtable = 0x5367ed0, damageNamesRva = 0x9ca9400;
inline constexpr uint32_t webEncaseIndex = 21, webImpactIndex = 22;

struct Cocoon {
    bool on{};
    uint64_t id{};
    float webbing{};     // 0..threshold
    float encased{};     // seconds left wrapped (0: not encased)
    float unseen{};      // seconds since the enemy list last had him
    float seed{};
    float age{};         // seconds since the first hit
    Vec3 feet{};
    // 0..1: how much of him the web covers (1 webbed up).
    float level() const { return encased > 0 ? 1.f : std::clamp(webbing / threshold, 0.f, 1.f); }
};

class Set {
  public:
    static constexpr int most = 6;
    // A ball struck enemy `id` standing at `feet`: returns true when this hit webbed him up.
    bool hit(uint64_t id, Vec3 feet) {
        if (!finite(feet))
            return false;
        Cocoon* c = find(id);
        if (!c) {
            c = &list_[0];
            for (auto& x : list_) // a free slot, else the oldest
                if (!x.on || (c->on && x.age > c->age))
                    c = &x;
            for (auto& x : list_)
                if (!x.on) {
                    c = &x;
                    break;
                }
            *c = Cocoon{};
            c->on = true;
            c->id = id;
            c->seed = static_cast<float>((id * 2654435761ull) % 1000u) / 1000.f;
        }
        c->feet = feet;
        c->unseen = 0;
        ++hits_;
        if (c->encased > 0) {
            c->encased = encasedSeconds; // another hit keeps him wrapped
            return false;
        }
        c->webbing = std::min(threshold, c->webbing + perHit);
        if (c->webbing >= threshold - 1e-3f) {
            c->encased = encasedSeconds;
            ++webbedUp_;
            return true;
        }
        return false;
    }
    // A frame: each enemy's feet from this frame's list (ids, feet, count).
    void update(float dt, const uint64_t* ids, const Vec3* feet, int count) {
        if (!std::isfinite(dt) || dt < 0)
            dt = 0;
        dt = std::min(dt, .1f);
        for (auto& c : list_) {
            if (!c.on)
                continue;
            c.age += dt;
            bool seen = false;
            for (int i = 0; i < count && ids && feet; ++i)
                if (ids[i] == c.id && finite(feet[i])) {
                    c.feet = feet[i];
                    seen = true;
                    break;
                }
            c.unseen = seen ? 0.f : c.unseen + dt;
            if (c.encased > 0) {
                c.encased = std::max(0.f, c.encased - dt);
                if (c.encased == 0)
                    c.webbing = 0;
            } else
                c.webbing = std::max(0.f, c.webbing - decay * dt);
            if (c.unseen > lostSeconds || (c.encased == 0 && c.webbing <= 0))
                c.on = false;
        }
    }
    const Cocoon& at(int i) const { return list_[i]; }
    uint64_t hits() const { return hits_; }
    uint64_t webbedUp() const { return webbedUp_; }
    int active() const {
        int n = 0;
        for (const auto& c : list_)
            n += c.on;
        return n;
    }
    void clear() {
        for (auto& c : list_)
            c = Cocoon{};
    }

  private:
    Cocoon* find(uint64_t id) {
        for (auto& c : list_)
            if (c.on && c.id == id)
                return &c;
        return nullptr;
    }
    Cocoon list_[most]{};
    uint64_t hits_{}, webbedUp_{};
};

// The cocoon's strands (segments a[i]-b[i], at most `max`; 160 holds them all): wraps round a body standing at
// `feet` (y up), more of him as `level` grows: diagonal wraps climbing from the chest (a hit's
// splat) out to the whole body, and when webbed up, ground anchors pinning him like the game's.
inline uint32_t strands(const Cocoon& c, Vec3* a, Vec3* b, uint32_t max) {
    if (!c.on || !finite(c.feet) || !a || !b)
        return 0;
    const float level = c.level();
    if (!(level > .01f))
        return 0;
    const auto h = [&](uint32_t i) {
        uint32_t x = static_cast<uint32_t>(c.seed * 1e6f) ^ (i * 0x9e3779b9u);
        x ^= x >> 16;
        x *= 0x7feb352du;
        x ^= x >> 15;
        return static_cast<float>(x & 0xffffu) / 65535.f;
    };
    const float pi2 = 6.2831853f;
    // The covered band: the chest first (1.0-1.35 m), out to 0.15-1.75 m webbed up.
    const float grown = level * std::sqrt(level); // a hit's splat first, the whole body once webbed up
    const float mid = 1.15f, half = .14f + .76f * grown;
    const float lo = std::max(.12f, mid - half), hi = std::min(bodyHeight, mid + half * .7f);
    const uint32_t wraps = static_cast<uint32_t>(3.f + 21.f * grown);
    uint32_t n = 0;
    auto ring = [&](float y, float angle) {
        // The body's radius: narrower at the knees and the head.
        const float r = bodyRadius * (y > 1.45f ? .7f : y < .55f ? .65f : 1.f) * (1.05f + .1f * level);
        return c.feet + Vec3{r * std::cos(angle), y, r * std::sin(angle)};
    };
    for (uint32_t w = 0; w < wraps && n + 4 <= max; ++w) {
        // One wrap: a diagonal turn round him in 4 pieces.
        const float y0 = lo + (hi - lo) * h(w * 7 + 1), y1 = lo + (hi - lo) * h(w * 7 + 2);
        const float a0 = pi2 * h(w * 7 + 3);
        for (int k = 0; k < 4; ++k) {
            const float u0 = static_cast<float>(k) / 4.f, u1 = static_cast<float>(k + 1) / 4.f;
            a[n] = ring(y0 + (y1 - y0) * u0, a0 + pi2 * .8f * u0);
            b[n] = ring(y0 + (y1 - y0) * u1, a0 + pi2 * .8f * u1);
            ++n;
        }
    }
    if (c.encased > 0) // webbed up: tight hoops, wrapped from the knees to the shoulders
        for (uint32_t r = 0; r < 10 && n + 6 <= max; ++r) {
            const float y = .35f + 1.15f * (static_cast<float>(r) + .5f * h(300 + r)) / 10.f;
            const float a0 = pi2 * h(310 + r);
            for (int k = 0; k < 6; ++k) {
                a[n] = ring(y, a0 + pi2 * static_cast<float>(k) / 6.f);
                b[n] = ring(y + .03f, a0 + pi2 * static_cast<float>(k + 1) / 6.f);
                ++n;
            }
        }
    if (c.encased > 0) // webbed up: anchors from his sides to the ground
        for (uint32_t k = 0; k < 6 && n < max; ++k) {
            const float ang = pi2 * (static_cast<float>(k) / 6.f + .1f * h(200 + k));
            const float y = .5f + .7f * h(210 + k);
            a[n] = ring(y, ang);
            const float out = .55f + .3f * h(220 + k);
            b[n] = c.feet + Vec3{out * std::cos(ang), .02f, out * std::sin(ang)};
            ++n;
        }
    return n;
}
} // namespace spidy::web_cocoon
