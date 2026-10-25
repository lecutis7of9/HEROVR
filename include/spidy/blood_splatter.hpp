#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
// Update 37: BLOOD SPLATTER (optional, OFF by default): a quick splash of our own
// overlay droplets where a blow meets an enemy. Nothing is drawn in the game and
// nothing in it changes.
//
// When it fires - honestly:
//   CONFIRMED HIT path: a landed blow's point from game_punch (the strike tally the
//     impact bursts use). In Spider-Man 2 that path is not ported (game_punch: SM2TODO,
//     native_bodies), so it never fires there today.
//   PLAN B (what fires in Spider-Man 2 now): a HAND HIT (combat_input's punch, the
//     game's X) thrown while the WORLD LOG's last scan saw an enemy (ThugBot) within
//     `nearMetres` of the player; the splash goes off a forearm ahead of the fist. That
//     is "a punch near a target", not a confirmed hit, and the holo/PDF say so.
//   Web-ball and gadget impacts have no confirmed hit point in SM2 (CANDIDATE only), so
//   they do not splash.
// Stains that stay on the enemy: not shipped - no confirmed way to drive the game's own
// character decals or damage masks (docs/spawn-and-mission-research.md, "Blood stains").
//
// Cost: OFF makes nothing at all (no splash is kept, no vertex). ON: at most `most`
// splashes alive, each `drops` droplets (two triangles each), gone within `life` s.
#include "soft_shapes.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace spidy::blood_splatter {
inline constexpr const char* help = "Blood where your punch meets a near enemy: ON, BRUTAL or EXTREME; OFF is free.";
inline constexpr float life = .45f, nearMetres = 3.f, gravity = 6.f;
// Update 38.5: `most` 6 -> 10 so EXTREME's second burst does not push out the first.
inline constexpr unsigned most = 10, drops = 14;
// Update 38.1 BLOOD LOOK BIG (combat38.hpp's numbers): a 1.1 s splash of 20 bright 2-5 cm
// drops at the enemy (not off the fist), and a flat spatter on his floor for 2.5 s. Still
// cheap: at most most * (bigDrops + bigSpots) quads.
inline constexpr float bigLife = 1.1f, bigFloorLife = 2.5f, bigDropMin = .02f, bigDropMax = .05f;
inline constexpr unsigned bigDrops = 20, bigSpots = 7;
// Update 38.2 BLOOD LOOK BRUTAL (combat38.hpp's numbers, the default): 32 drops of 3-8 cm,
// opaque, 2.2 s in the air, 14 floor spots lasting 8 s, and a coat on the punching glove
// (appendCoat). Worst case most * (32 + 14) + 2 * 8 quads (~290): still cheap.
inline constexpr float brutalLife = 2.2f, brutalFloorLife = 8.f, brutalDropMin = .03f, brutalDropMax = .08f;
inline constexpr unsigned brutalDrops = 32, brutalSpots = 14, coatSpots = 8;
// Update 38.5 EXTREME (intensity 200, the step past BRUTAL): every punch near an enemy throws
// two bursts (the second 0.12 s later, to the side), 56 drops of 4-11 cm flying 1.6x as far,
// 3 s in the air, 24 floor spots and a dark pool that spreads for 1.5 s and lasts 14 s (our own
// overlay on the floor under him - no game decal or enemy stain: no confirmed address for
// either), a heavier glove coat that drips, and (sub-toggle VISOR DROPS, default OFF) a few
// drops on your visor after a burst close to your face. Budget: worst case
// most * (56 + 24 + 3) + 2 * (16 + 4) + visorDrops quads (~880, ~1760 triangles, one draw).
inline constexpr float extremeLife = 3.f, extremeFloorLife = 14.f, extremeDropMin = .04f, extremeDropMax = .11f,
                       extremeReach = 1.6f, extremeSecondBurst = .12f, extremePoolGrow = 1.5f, extremePoolMax = .6f;
inline constexpr unsigned extremeDrops = 56, extremeSpots = 24, extremePoolQuads = 3, extremeCoatSpots = 16,
                          drips = 4, visorDrops = 10;
inline constexpr uint32_t minIntensity = 0, maxIntensity = 200, defaultIntensity = 100, brutal = 150, extreme = 200;
inline constexpr unsigned budgetQuads = most * (extremeDrops + extremeSpots + extremePoolQuads) +
                                        2 * (extremeCoatSpots + drips) + visorDrops;
// The holo's four steps for the press (intensity percent): OFF, ON, BRUTAL, EXTREME.
inline uint32_t nextStep(uint32_t intensity) {
    return intensity == 0 ? defaultIntensity : intensity < brutal ? 175 : intensity < extreme ? extreme : 0;
}
inline const char* stepName(uint32_t intensity) {
    return intensity == 0 ? "OFF" : intensity >= extreme ? "EXTREME" : intensity >= brutal ? "BRUTAL" : "ON";
}
inline bool isExtreme(uint32_t intensity) { return intensity >= extreme; }
inline constexpr const char* visorHelp = "EXTREME only: a few drops land on your visor after a close hit, then run off.";

struct Splash {
    Vec3 point{}, toward{}; // where; the blow's way (unit)
    float age = -1;         // seconds; < 0 a free slot
    float strength = 1;     // 0..2 (intensity / 100)
    uint32_t seed{};
    bool big{};             // Update 38.1 BLOOD LOOK BIG
    bool brutal{};          // Update 38.2 BLOOD LOOK BRUTAL (big too)
    bool extreme{};         // Update 38.5 EXTREME (brutal and big too)
    float wait{};           // Update 38.5: seconds before it starts (EXTREME's second burst)
    float floorY = NAN;     // BIG: the floor's height under him (NaN: no spatter)
    float airLife() const { return extreme ? extremeLife : brutal ? brutalLife : bigLife; }
    float floorLife() const { return extreme ? extremeFloorLife : brutal ? brutalFloorLife : bigFloorLife; }
    float lifetime() const { return big ? std::max(airLife(), std::isfinite(floorY) ? floorLife() : 0.f) : life; }
};
inline float hash01(uint32_t x) {
    x ^= x >> 16; x *= 0x7feb352d; x ^= x >> 15; x *= 0x846ca68b; x ^= x >> 16;
    return static_cast<float>(x & 0xffffff) / 16777216.f;
}
struct Pool {
    std::array<Splash, most> splashes{};
    unsigned next{};
    uint64_t fired{}, planB{};
    // A splash; refused for a bad point or intensity 0.
    bool fire(Vec3 point, Vec3 toward, uint32_t intensity, bool confirmed, bool big = false, float floorY = NAN,
              bool brutalSplash = false) {
        if (!intensity || !finite(point) || !finite(toward) || length(toward) < 1e-3f)
            return false;
        Splash& s = splashes[next++ % most];
        s.point = point;
        s.toward = normalized(toward);
        s.age = 0;
        s.strength = std::clamp(static_cast<float>(intensity) / 100.f, 0.f, 2.f);
        s.seed = static_cast<uint32_t>(fired * 2654435761u + 977u);
        s.big = big || brutalSplash;
        s.brutal = brutalSplash;
        s.floorY = s.big && std::isfinite(floorY) && floorY < point.y ? floorY : NAN;
        s.extreme = false;
        s.wait = 0;
        ++fired;
        planB += !confirmed;
        // Update 38.5 EXTREME: brutal's look, bigger, and a second burst off to the side.
        if (isExtreme(intensity)) {
            s.big = s.brutal = s.extreme = true;
            Splash& second = splashes[next++ % most];
            second = s;
            const Vec3 side = soft::perpendicular(s.toward);
            second.toward = normalized(s.toward + side * .6f + Vec3{0, .25f, 0});
            second.seed = s.seed ^ 0x9e3779b9u;
            second.wait = extremeSecondBurst;
            second.floorY = NAN; // one pool a hit
            ++extremeBursts;
        }
        return true;
    }
    uint64_t extremeBursts{};
    void step(float dt) {
        if (!std::isfinite(dt) || dt < 0)
            dt = 0;
        for (auto& s : splashes) {
            if (s.age < 0)
                continue;
            if (s.wait > 0) { // Update 38.5: EXTREME's second burst waits its turn
                s.wait = std::max(0.f, s.wait - dt);
                continue;
            }
            if ((s.age += dt) > s.lifetime())
                s.age = -1;
        }
    }
    bool any() const {
        for (const auto& s : splashes)
            if (s.age >= 0)
                return true;
        return false;
    }
    void clear() {
        for (auto& s : splashes)
            s.age = -1;
    }
};
// Update 38.1 BIG: bright drops thrown out of him and falling to the floor, then a spatter.
inline void appendBig(std::vector<Vertex>& out, const Splash& s, Vec3 viewer) {
    const Vec3 side = soft::perpendicular(s.toward), up2 = cross(s.toward, side);
    const float air = s.airLife(), floorLife = s.floorLife();
    const unsigned dropCount = s.extreme ? extremeDrops : s.brutal ? brutalDrops : bigDrops,
                   spotCount = s.extreme ? extremeSpots : s.brutal ? brutalSpots : bigSpots;
    const float dropMin = s.extreme ? extremeDropMin : s.brutal ? brutalDropMin : bigDropMin,
                dropMax = s.extreme ? extremeDropMax : s.brutal ? brutalDropMax : bigDropMax,
                reach = s.extreme ? extremeReach : 1.f;
    if (s.age <= air) {
        const float t = s.age / air, fade = s.brutal ? 1 - t * t * t : 1 - t * t;
        for (unsigned i = 0; i < dropCount; ++i) {
            const float a = hash01(s.seed + i * 3) * 6.2831853f, r = hash01(s.seed + i * 3 + 1),
                        v = (1.4f + 2.4f * hash01(s.seed + i * 3 + 2)) * (.7f + .3f * s.strength) * reach;
            const Vec3 dir = normalized(s.toward * .6f + Vec3{0, .5f, 0} + (side * std::cos(a) + up2 * std::sin(a)) * (.5f + .7f * r));
            Vec3 at = s.point + dir * (v * s.age) + Vec3{0, -.5f * gravity * s.age * s.age, 0};
            if (std::isfinite(s.floorY) && at.y < s.floorY + .01f)
                continue; // landed: the spatter has it
            const float size = (dropMin + (dropMax - dropMin) * hash01(s.seed + i * 7)) * .5f * (1 - .4f * t);
            const Vec3 view = viewer - at;
            const float d = length(view);
            if (!(d > .1f && d < 60))
                continue;
            const Vec3 f = view / d;
            Vec3 right = cross(Vec3{0, 1, 0}, f);
            right = length(right) > .1f ? normalized(right) : soft::perpendicular(f);
            const Vec3 up = cross(f, right);
            const Vec3 color = (s.brutal ? Vec3{.6f, .02f, .02f} : Vec3{.78f, .04f, .03f}) * (.85f + .3f * hash01(s.seed + i * 11));
            const float alpha = std::clamp(fade, 0.f, 1.f);
            const Vec3 along = up * (size * 1.4f), across = right * size;
            soft::quad(out, {at - across - along, color, alpha}, {at + across - along, color, alpha},
                       {at + across + along, color, alpha}, {at - across + along, color, alpha});
        }
    }
    if (std::isfinite(s.floorY) && s.age > .25f) {
        const float t = std::clamp((s.age - .25f) / (floorLife - .25f), 0.f, 1.f), fade = 1 - t * t * t;
        const Vec3 flat = Vec3{s.toward.x, 0, s.toward.z};
        const Vec3 ahead = length(flat) > .1f ? normalized(flat) : Vec3{0, 0, -1}, aside{-ahead.z, 0, ahead.x};
        for (unsigned i = 0; i < spotCount; ++i) {
            const float a = hash01(s.seed + i * 5 + 101) * 6.2831853f,
                        r = (.05f + .35f * hash01(s.seed + i * 5 + 102)) * (s.extreme ? 2.f : s.brutal ? 1.4f : 1.f);
            const Vec3 at = Vec3{s.point.x, s.floorY + .01f, s.point.z} + (ahead * (.15f + std::cos(a) * r)) + aside * (std::sin(a) * r);
            const float size = (.03f + .06f * hash01(s.seed + i * 5 + 103)) * (s.extreme ? 1.8f : s.brutal ? 1.5f : 1.f);
            const Vec3 color = Vec3{.55f, .02f, .02f} * (.85f + .3f * hash01(s.seed + i * 5 + 104));
            const float alpha = std::clamp((s.brutal ? .95f : .85f) * fade, 0.f, 1.f);
            const Vec3 x{size, 0, 0}, z{0, 0, size * (.6f + .8f * hash01(s.seed + i * 5 + 105))};
            soft::quad(out, {at - x - z, color, alpha}, {at + x - z, color, alpha}, {at + x + z, color, alpha},
                       {at - x + z, color, alpha});
        }
        // Update 38.5 EXTREME: the pool - three overlapping turned squares (a rough round puddle)
        // spreading to extremePoolMax over extremePoolGrow s, darker than the spots, just above them.
        if (s.extreme) {
            const float grow = std::clamp((s.age - .25f) / extremePoolGrow, 0.f, 1.f);
            const float radius = extremePoolMax * (.25f + .75f * (1 - (1 - grow) * (1 - grow)));
            const Vec3 centre = Vec3{s.point.x, s.floorY + .012f, s.point.z} + ahead * .2f;
            const Vec3 color{.32f, .01f, .01f};
            const float alpha = std::clamp(.9f * fade, 0.f, 1.f);
            for (unsigned q = 0; q < extremePoolQuads; ++q) {
                const float a = static_cast<float>(q) * 1.0471976f + hash01(s.seed + q) * .5f;
                const float k = radius * (.75f + .3f * hash01(s.seed + q * 7 + 3));
                const Vec3 x = (ahead * std::cos(a) + aside * std::sin(a)) * k,
                           z = (aside * std::cos(a) - ahead * std::sin(a)) * (k * .8f);
                soft::quad(out, {centre - x - z, color, alpha}, {centre + x - z, color, alpha},
                           {centre + x + z, color, alpha}, {centre - x + z, color, alpha});
            }
        }
    }
}
// The droplets: thrown out along the blow and around it, falling, shrinking and fading
// to nothing by `life`. Dark red; BRUTAL throws more and bigger. Vertices made.
inline size_t append(std::vector<Vertex>& out, const Pool& pool, Vec3 viewer) {
    const size_t before = out.size();
    if (!finite(viewer))
        return 0;
    for (const auto& s : pool.splashes) {
        if (s.age < 0 || s.age > s.lifetime() || s.wait > 0)
            continue;
        if (s.big) {
            appendBig(out, s, viewer);
            continue;
        }
        const float t = s.age / life, fade = (1 - t) * (1 - t);
        const unsigned n = std::min(drops, static_cast<unsigned>(std::lround(drops * std::min(s.strength, 1.5f) / 1.5f)) + 4);
        const Vec3 side = soft::perpendicular(s.toward), up2 = cross(s.toward, side);
        for (unsigned i = 0; i < n; ++i) {
            const float a = hash01(s.seed + i * 3) * 6.2831853f, r = hash01(s.seed + i * 3 + 1),
                        v = (1.2f + 2.2f * hash01(s.seed + i * 3 + 2)) * (.6f + .4f * s.strength);
            const Vec3 dir = normalized(s.toward * .8f + (side * std::cos(a) + up2 * std::sin(a)) * (.4f + .8f * r));
            const Vec3 at = s.point + dir * (v * s.age) + Vec3{0, -.5f * gravity * s.age * s.age, 0};
            const float size = (.008f + .016f * hash01(s.seed + i * 7)) * (.7f + .3f * s.strength) * (1 - .5f * t);
            const Vec3 view = viewer - at;
            const float d = length(view);
            if (!(d > .1f && d < 60))
                continue;
            const Vec3 f = view / d;
            Vec3 right = cross(Vec3{0, 1, 0}, f);
            right = length(right) > .1f ? normalized(right) : soft::perpendicular(f);
            const Vec3 up = cross(f, right);
            const Vec3 color = Vec3{.42f, .02f, .02f} * (.8f + .4f * hash01(s.seed + i * 11));
            const float alpha = std::clamp(.9f * fade, 0.f, 1.f);
            // A droplet streaked along its fall: longer than wide.
            const Vec3 along = up * (size * 1.6f), across = right * size;
            soft::quad(out, {at - across - along, color, alpha}, {at + across - along, color, alpha},
                       {at + across + along, color, alpha}, {at - across + along, color, alpha});
        }
    }
    return out.size() - before;
}
// Update 38.2 BRUTAL's coat: dark-red blotches stuck on the glove around the fist (`fist`, the
// grip's knuckles; `aim` the fist's way), `amount` 0..1 their opacity (combat38::BloodCoat).
// Our overlay over the glove, not the game's own suit material. coatSpots quads at most.
// Update 38.5 EXTREME (`extreme`): twice the blotches, a bit bigger, and `drips` drops that
// gather under the fist and fall off it in turn (`seconds`: a clock for their cycle).
inline size_t appendCoat(std::vector<Vertex>& out, Vec3 fist, Vec3 aim, float amount, uint32_t seed, Vec3 viewer,
                         bool extremeCoat = false, float seconds = 0) {
    const size_t before = out.size();
    if (!(amount > .01f) || !finite(fist) || !finite(viewer) || !finite(aim) || length(aim) < .5f)
        return 0;
    const Vec3 forward = normalized(aim), side = soft::perpendicular(forward), up2 = cross(forward, side);
    const unsigned spots = extremeCoat ? extremeCoatSpots : coatSpots;
    if (extremeCoat && std::isfinite(seconds))
        for (unsigned i = 0; i < drips; ++i) {
            // Each drip: 0.6 s swelling under the knuckles, then 0.5 s falling (its own phase).
            const float cycle = 1.1f, phase = std::fmod(seconds + cycle * static_cast<float>(i) / drips + 10 * cycle, cycle);
            const float a = hash01(seed + i * 29) * 6.2831853f;
            const Vec3 hang = fist + (side * std::cos(a) + forward * std::sin(a)) * .025f + Vec3{0, -.02f, 0};
            const float fall = std::max(0.f, phase - .6f);
            const Vec3 at = hang + Vec3{0, -.5f * gravity * fall * fall, 0};
            const float size = .004f + .005f * std::min(phase / .6f, 1.f);
            const Vec3 view = viewer - at;
            const float d = length(view);
            if (!(d > .05f && d < 5))
                continue;
            const Vec3 f = view / d;
            Vec3 right = cross(Vec3{0, 1, 0}, f);
            right = length(right) > .1f ? normalized(right) : soft::perpendicular(f);
            const Vec3 up = cross(f, right);
            const Vec3 color{.38f, .01f, .01f};
            const float alpha = std::clamp(amount * (fall > 0 ? 1 - fall / .5f : 1.f), 0.f, 1.f);
            const Vec3 across = right * size, along = up * (size * (fall > 0 ? 2.2f : 1.3f));
            const Vec3 c = at + f * .012f;
            soft::quad(out, {c - across - along, color, alpha}, {c + across - along, color, alpha},
                       {c + across + along, color, alpha}, {c - across + along, color, alpha});
        }
    for (unsigned i = 0; i < spots; ++i) {
        const float a = hash01(seed + i * 13) * 6.2831853f, r = .015f + .03f * hash01(seed + i * 13 + 1);
        const Vec3 at = fist + forward * (.02f - .06f * hash01(seed + i * 13 + 2)) + (side * std::cos(a) + up2 * std::sin(a)) * r;
        const Vec3 view = viewer - at;
        const float d = length(view);
        if (!(d > .05f && d < 5))
            continue;
        const Vec3 f = view / d;
        Vec3 right = cross(Vec3{0, 1, 0}, f);
        right = length(right) > .1f ? normalized(right) : soft::perpendicular(f);
        const Vec3 up = cross(f, right);
        // Just off the glove toward the eye, so the glove does not hide it.
        const Vec3 c = at + f * .012f;
        const float size = (.008f + .014f * hash01(seed + i * 13 + 3)) * (extremeCoat ? 1.3f : 1.f);
        const Vec3 color = Vec3{.36f, .01f, .01f} * (.8f + .4f * hash01(seed + i * 13 + 4));
        const float alpha = std::clamp(amount, 0.f, 1.f);
        const Vec3 across = right * size, along = up * (size * (.7f + .8f * hash01(seed + i * 13 + 5)));
        soft::quad(out, {c - across - along, color, alpha}, {c + across - along, color, alpha},
                   {c + across + along, color, alpha}, {c - across + along, color, alpha});
    }
    return out.size() - before;
}
// Update 38.5 EXTREME, VISOR DROPS (sub-toggle, default OFF): a burst within visorMetres of
// your eyes splats a few drops on the "visor" - head-locked at 0.2 m, kept off the middle
// third of the view - which run down and fade over visorLife s. Our overlay only.
inline constexpr float visorMetres = 1.2f, visorLife = 5.f, visorDistance = .2f;
struct Visor {
    float age = -1; // < 0 clean
    uint32_t seed{};
    uint64_t splats{};
    void splat(uint32_t s) {
        age = 0;
        seed = s;
        ++splats;
    }
    void step(float dt) {
        if (age >= 0 && std::isfinite(dt) && (age += std::clamp(dt, 0.f, .1f)) > visorLife)
            age = -1;
    }
};
// eye: between the eyes; forward/up/right: the head's unit axes.
inline size_t appendVisor(std::vector<Vertex>& out, const Visor& v, Vec3 eye, Vec3 forward, Vec3 up, Vec3 right) {
    const size_t before = out.size();
    if (v.age < 0 || !finite(eye) || !finite(forward) || !finite(up) || !finite(right))
        return 0;
    const float t = v.age / visorLife, fade = 1 - t * t;
    for (unsigned i = 0; i < visorDrops; ++i) {
        // Spread over the edges: x in +-(0.2..0.55) of the half-view, y anywhere; runs down.
        const float side = hash01(v.seed + i * 17) < .5f ? -1.f : 1.f;
        const float x = side * (.05f + .08f * hash01(v.seed + i * 17 + 1));
        const float y = -.06f + .12f * hash01(v.seed + i * 17 + 2) - .03f * v.age * hash01(v.seed + i * 17 + 3);
        const float size = .003f + .005f * hash01(v.seed + i * 17 + 4);
        const Vec3 c = eye + forward * visorDistance + right * x + up * y;
        const Vec3 color = Vec3{.45f, .02f, .02f} * (.8f + .3f * hash01(v.seed + i * 17 + 5));
        const float alpha = std::clamp(.75f * fade, 0.f, 1.f);
        const Vec3 across = right * size, along = up * (size * (1.2f + .6f * std::min(v.age, 1.f)));
        soft::quad(out, {c - across - along, color, alpha}, {c + across - along, color, alpha},
                   {c + across + along, color, alpha}, {c - across + along, color, alpha});
    }
    return out.size() - before;
}
// Update 37 telemetry (BLDS), read by tools/update37_records.py.
inline constexpr uint32_t dataMagic = 0x53444c42; // BLDS
struct Data {
    uint32_t magic = dataMagic, version = 1, bytes = 56, intensity{};
    uint64_t fired{}, planB{}, confirmed{};
    float meanMs{}, peakMs{};
    uint64_t sequence{};
};
static_assert(sizeof(Data) == 56);
} // namespace spidy::blood_splatter
