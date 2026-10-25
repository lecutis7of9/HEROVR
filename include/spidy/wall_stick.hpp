#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
//
// Version 0.0.3 WALL STICK (rope-grab-spec.md, the WALL-STICK VIDEO; EXPERIMENTAL, OFF by default):
// an enemy that hits a wall or the floor at speed is webbed to it, a big messy round net over his
// torso and upper legs, for as long as he stays there.
//
// The game has its own (BotStateWallStuck, BotWallStuckEvent: RTTI names, CANDIDATE vtables
// 0x530fad0 / 0x58f7398) but no confirmed way to start it, and punches are not delivered by the
// port, so 0.0.3 draws a MOD WEB SPLAT where the impact is seen; it does not knock him down and
// does not hold him (the game decides that). Read-only: it only watches the 10 Hz enemy list.
// LOW HEALTH ONLY: enemy health is not readable yet, so it stands in "stays down": the net comes
// only when he lies still 0.8 s after the impact. ALWAYS: at the impact.
#include "spidy/math.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace spidy::wall_stick {

enum Mode : uint32_t { off = 0, lowHealth = 1, always = 2, modes = 3 };
inline constexpr const char* modeNames[modes] = {"OFF", "LOW HEALTH ONLY", "ALWAYS"};
inline constexpr float impactSpeed = 7.f, stopSpeed = 1.5f, maxSpeed = 40.f, maxGap = .5f, minGap = .05f;
inline constexpr float stillFor = .8f, stillMetres = .3f, keepMetres = .6f, keepSeconds = 20.f, armSeconds = 2.5f;
inline constexpr uint32_t tracked = 16, nets = 6;

struct Net {
    bool on{};
    uint64_t id{};
    Vec3 at{}, normal{0, 1, 0};
    float age{}, fade{}, seed{};
    bool shown{}; // LOW HEALTH ONLY: false until he has lain still
};

class Detector {
public:
    // A throw, slam or yank just happened: impacts count for the next armSeconds (the guard that
    // keeps a running enemy, or the list's own jumps, from making nets).
    void arm() {
        armed_ = armSeconds;
    }
    // One enemy sample (world feet), with the time since the detector began. Call it when the
    // 10 Hz list refreshed; samples closer than 0.05 s to the last are skipped.
    void sample(uint64_t id, Vec3 feet, float now, uint32_t mode) {
        if (mode == off || !finite(feet))
            return;
        Track& t = track(id);
        if (t.have && now - t.when < minGap && now >= t.when)
            return; // the same 10 Hz sample again
        const float gap = now - t.when;
        if (t.have && gap >= minGap && gap <= maxGap) {
            const Vec3 v = (feet - t.at) / gap;
            const float speed = length(v);
            if (speed > maxSpeed) { // a jump in the list, not a flight
                t.speed = 0;
            } else {
                if (armed_ > 0 && t.speed >= impactSpeed && speed <= stopSpeed)
                    impact(id, feet, t.velocity, mode);
                t.velocity = v;
                t.speed = speed;
            }
        } else {
            t.speed = 0;
        }
        t.at = feet;
        t.when = now;
        t.have = true;
        // The net stays while he stays.
        for (auto& n : nets_)
            if (n.on && n.id == id) {
                const Vec3 rest = n.normal.y > .7f ? feet + Vec3{0, .25f, 0} : feet + Vec3{0, 1.f, 0};
                const float moved = length(rest - n.at);
                if (moved > keepMetres || (!n.shown && moved > stillMetres)) {
                    n.on = false;
                    ++unstuck_;
                }
            }
    }
    void frame(float dt, uint32_t mode) {
        if (!std::isfinite(dt) || dt < 0)
            dt = 0;
        armed_ = std::max(0.f, armed_ - dt);
        for (auto& n : nets_) {
            if (!n.on) {
                n.fade = std::max(0.f, n.fade - dt * 2);
                continue;
            }
            n.age += dt;
            if (mode == off || n.age > keepSeconds) {
                n.on = false;
                continue;
            }
            if (!n.shown && (mode == always || n.age >= stillFor)) {
                n.shown = true;
                ++shown_;
            }
            n.fade = n.shown ? std::min(1.f, n.fade + dt * 8) : 0.f;
        }
    }
    const std::array<Net, nets>& netsNow() const {
        return nets_;
    }
    uint64_t impacts() const {
        return impacts_;
    }
    uint64_t shown() const {
        return shown_;
    }
    uint64_t walls() const {
        return walls_;
    }
    uint64_t floors() const {
        return floors_;
    }
    uint64_t unstuck() const {
        return unstuck_;
    }
    float armed() const {
        return armed_;
    }

private:
    struct Track {
        uint64_t id{};
        Vec3 at{}, velocity{};
        float when{}, speed{};
        bool have{};
        uint32_t used{};
    };
    Track& track(uint64_t id) {
        Track* oldest = &tracks_[0];
        for (auto& t : tracks_) {
            if (t.used && t.id == id) {
                t.used = ++stamp_;
                return t;
            }
            if (!t.used || t.used < oldest->used)
                oldest = &t;
        }
        *oldest = {};
        oldest->id = id;
        oldest->used = ++stamp_;
        return *oldest;
    }
    void impact(uint64_t id, Vec3 feet, Vec3 velocity, uint32_t) {
        ++impacts_;
        // The surface faces back along the flight: a mostly downward flight is the floor.
        const Vec3 dir = normalized(velocity);
        const bool floor = dir.y < -.6f;
        Vec3 normal = floor ? Vec3{0, 1, 0} : normalized(Vec3{-dir.x, 0, -dir.z});
        if (!finite(normal) || length(normal) < .5f)
            normal = {0, 1, 0};
        (floor ? floors_ : walls_)++;
        Net* slot = &nets_[0];
        for (auto& n : nets_) {
            if (n.on && n.id == id) {
                slot = &n;
                break;
            }
            if (!n.on && slot->on)
                slot = &n;
            else if (n.on && slot->on && n.age > slot->age)
                slot = &n;
        }
        *slot = {};
        slot->on = true;
        slot->id = id;
        slot->normal = normal;
        slot->at = floor ? feet + Vec3{0, .25f, 0} : feet + Vec3{0, 1.f, 0}; // the torso
        slot->seed = static_cast<float>((id * 2654435761u) % 1000u) / 1000.f;
    }
    std::array<Track, tracked> tracks_{};
    std::array<Net, nets> nets_{};
    uint32_t stamp_{};
    float armed_{};
    uint64_t impacts_{}, shown_{}, walls_{}, floors_{}, unstuck_{};
};

// The net's strands, as segments (a, b) on the surface plane: `spokes` messy radial strands
// from the middle and rings of chords between them, about `radius` across (torso and upper
// legs). Returns the segment count written (at most max).
inline uint32_t strands(const Net& n, float radius, Vec3* a, Vec3* b, uint32_t max) {
    if (!n.on && n.fade <= 0)
        return 0;
    const Vec3 up = std::abs(n.normal.y) > .9f ? Vec3{0, 0, 1} : Vec3{0, 1, 0};
    const Vec3 u = normalized(cross(up, n.normal)), v = normalized(cross(n.normal, u));
    if (!finite(u) || !finite(v))
        return 0;
    constexpr uint32_t spokes = 12;
    float r[spokes]{}, ang[spokes]{};
    for (uint32_t i = 0; i < spokes; ++i) { // a little ragged, the same each frame (seeded)
        const float fi = static_cast<float>(i);
        const float h = std::sin((fi + 1) * 12.9898f + n.seed * 78.233f) * 43758.5453f;
        const float j = h - std::floor(h);
        r[i] = radius * (.75f + .5f * j);
        ang[i] = (fi + .35f * (j - .5f)) * 6.2831853f / static_cast<float>(spokes);
    }
    const Vec3 lift = n.normal * .02f; // just off the surface
    uint32_t c = 0;
    auto put = [&](Vec3 p, Vec3 q) {
        if (c < max) {
            a[c] = p + lift;
            b[c] = q + lift;
            ++c;
        }
    };
    auto at = [&](uint32_t i, float f) {
        return n.at + (u * std::cos(ang[i % spokes]) + v * std::sin(ang[i % spokes])) * (r[i % spokes] * f);
    };
    for (uint32_t i = 0; i < spokes; ++i)
        put(n.at, at(i, 1.f));
    for (float f : {.35f, .65f, .95f})
        for (uint32_t i = 0; i < spokes; ++i)
            put(at(i, f), at(i + 1, f * (i % 3 == 0 ? .9f : 1.f)));
    return c;
}
} // namespace spidy::wall_stick
