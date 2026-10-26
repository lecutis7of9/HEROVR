#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
//
// Version 0.0.2 (Update 38.6) BALL LOOK OURS: the mod's own web ball, the pre-38.3 style - one
// glowing ball per trigger press, from the pulled hand's wrist along that hand's aim. No game
// web ball goes out for the press (combat_input's Config::ballOurs: no RB at all).
//
// AIM STRENGTH (combat38 aimStrength): the cone the ball looks for an enemy in, how hard it homes
// on him (degrees a second it may turn), whether it leads a moving one, and its reach. The locked
// enemy (the head-look lock) wins when he is inside the cone; otherwise the one nearest the aim,
// near ones first. Any enemy the ball passes through counts as hit, not only the picked one.
//
// What a hit does (honest): our splash and a buzz on that hand, counted in the report. The game's
// own web-ball effect is NOT applied - no SM2 hit path is confirmed (game_shooter's SpidyVR hooks
// are MSM1's, error 9990). BALL HIT +GAME WEB (experimental, off by default) asks the game for
// its own ball at the hit (wrist_muzzle's far shot, CANDIDATE).
//
// Pure and allocation-free; all positions tracking space (metres, y up), the enemies' feet as
// combat_input::Frame has them (world_trace at 10 Hz).
#include "math.hpp"
#include "bolt_walls.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace spidy::our_ball {

// The option's order (index 0 the default; a press steps up and wraps).
enum Strength : uint32_t { strong = 0, maxStrength = 1, off = 2, low = 3, normal = 4, strengths = 5 };
struct Tune {
    float cone;      // degrees off the aim an enemy may be to be picked (0: none)
    float nearCone;  // within nearMetres (a close enemy is wide of the aim more easily)
    float turn;      // degrees a second the ball may turn toward him (0: straight)
    float range;     // metres the ball flies, and the farthest enemy picked
    float hitRadius; // metres from the body's line that counts as a hit
    bool lead;       // aim where a moving enemy will be
    float lockCone;  // Version 0.0.4: the locked enemy is taken this far off the aim (0: only in the cone)
};
inline constexpr float speed = 35.f, nearMetres = 3.f, bodyHeight = 1.8f, chest = 1.05f, magnetMetres = 3.f;
inline Tune tuneOf(uint32_t strength) {
    switch (strength) {
    case off:
        return {0.f, 0.f, 0.f, 20.f, .30f, false, 0.f};
    case low:
        return {10.f, 18.f, 120.f, 15.f, .35f, false, 0.f};
    case normal:
        return {18.f, 30.f, 300.f, 20.f, .45f, true, 0.f};
    // Version 0.0.4 (rep0030: 9 balls, 5 with no enemy in STRONG's 28 degree cone, 4 hits): STRONG
    // and MAX much wider and stickier, the locked enemy taken well off the aim, a fatter hit.
    case maxStrength:
        return {65.f, 90.f, 3600.f, 35.f, 1.f, true, 120.f};
    default: // strong
        return {45.f, 70.f, 1800.f, 30.f, .8f, true, 90.f};
    }
}
inline const char* strengthName(uint32_t s) {
    static constexpr const char* names[strengths] = {"STRONG", "MAX", "OFF", "LOW", "NORMAL"};
    return s < strengths ? names[s] : names[0];
}

namespace detail {
inline float degreesOff(Vec3 origin, Vec3 ray, Vec3 point) {
    const Vec3 to = point - origin;
    if (!finite(to) || !finite(ray) || length(to) < 1e-3f || length(ray) < .5f)
        return 180.f;
    return std::acos(std::clamp(dot(normalized(to), normalized(ray)), -1.f, 1.f)) * 57.2957795f;
}
// The closest distance between segment a-b and segment c-d.
inline float segmentDistance(Vec3 a, Vec3 b, Vec3 c, Vec3 d) {
    const Vec3 u = b - a, v = d - c, w = a - c;
    const float A = dot(u, u), B = dot(u, v), C = dot(v, v), D = dot(u, w), E = dot(v, w);
    const float den = A * C - B * B;
    float s = 0, t = 0;
    if (A < 1e-9f && C < 1e-9f)
        return length(a - c);
    if (A < 1e-9f) {
        t = std::clamp(E / C, 0.f, 1.f);
    } else if (C < 1e-9f) {
        s = std::clamp(-D / A, 0.f, 1.f);
    } else {
        s = den > 1e-9f ? std::clamp((B * E - C * D) / den, 0.f, 1.f) : 0.f;
        t = std::clamp((B * s + E) / C, 0.f, 1.f);
        s = std::clamp((B * t - D) / A, 0.f, 1.f);
    }
    return length((a + u * s) - (c + v * t));
}
// Turn unit vector `from` toward unit `to` by at most `radians`.
inline Vec3 turnToward(Vec3 from, Vec3 to, float radians) {
    const float c = std::clamp(dot(from, to), -1.f, 1.f);
    const float angle = std::acos(c);
    if (angle <= radians || angle < 1e-5f)
        return to;
    Vec3 side = to - from * c;
    if (length(side) < 1e-6f) { // opposite: any perpendicular
        side = std::abs(from.y) < .9f ? cross(from, Vec3{0, 1, 0}) : cross(from, Vec3{1, 0, 0});
    }
    side = normalized(side);
    return normalized(from * std::cos(radians) + side * std::sin(radians));
}
} // namespace detail

// Where to aim at a target at `p` moving at `v`, from `o`, for a ball at `speed` (lead); `p`
// itself without a usable velocity. The flight time is capped at a second.
inline Vec3 intercept(Vec3 o, Vec3 p, Vec3 v, bool lead) {
    if (!lead || !finite(v) || length(v) < .05f)
        return p;
    float t = length(p - o) / speed;
    for (int i = 0; i < 4; ++i)
        t = std::clamp(length(p + v * t - o) / speed, 0.f, 1.f);
    return p + v * t;
}

// The enemies the ball may pick from this frame: feet (tracking space), registry id, velocity.
struct Enemy {
    Vec3 feet{};
    uint64_t id{};
    Vec3 velocity{};
};
constexpr int maxEnemies = 16;
struct Enemies {
    std::array<Enemy, maxEnemies> list{};
    uint32_t count{};
};

// The enemy for a shot: the locked one inside the cone wins, else the least off the aim plus a
// little per metre (a near one a little off beats a far one dead ahead); -1 none.
inline int pick(Vec3 origin, Vec3 aim, const Enemies& e, uint64_t locked, const Tune& t, float* degrees = nullptr) {
    int best = -1;
    float bestScore = 1e9f, bestOff = 180.f;
    if (t.cone > 0 && finite(origin) && finite(aim) && length(aim) > .5f)
        for (uint32_t i = 0; i < e.count && i < static_cast<uint32_t>(maxEnemies); ++i) {
            const Vec3 c = e.list[i].feet + Vec3{0, chest, 0};
            const float d = length(c - origin);
            if (!(d > .3f) || d > t.range)
                continue;
            const float off = detail::degreesOff(origin, aim, c);
            if (off > (d <= nearMetres ? t.nearCone : t.cone))
                continue;
            float score = off + .5f * d;
            if (locked && e.list[i].id == locked)
                score -= 1000.f;
            if (score < bestScore) {
                bestScore = score;
                best = static_cast<int>(i);
                bestOff = off;
            }
        }
    // Version 0.0.4: no one in the cone - the locked enemy within lockCone and range still takes it.
    if (best < 0 && locked && t.lockCone > 0 && finite(origin) && finite(aim) && length(aim) > .5f)
        for (uint32_t i = 0; i < e.count && i < static_cast<uint32_t>(maxEnemies); ++i) {
            const Vec3 c = e.list[i].feet + Vec3{0, chest, 0};
            const float d = length(c - origin);
            const float off = detail::degreesOff(origin, aim, c);
            if (e.list[i].id == locked && d > .3f && d <= t.range && off <= t.lockCone) {
                best = static_cast<int>(i);
                bestOff = off;
            }
        }
    if (degrees)
        *degrees = bestOff;
    return best;
}

// The enemies' velocities from their feet frame to frame, by id (unknown id: none).
class Motion {
  public:
    void update(float now, Enemies& e) {
        std::array<Seen, maxEnemies> next{};
        for (uint32_t i = 0; i < e.count && i < static_cast<uint32_t>(maxEnemies); ++i) {
            Enemy& en = e.list[i];
            en.velocity = {};
            next[i] = {en.id, en.feet, now, {}};
            if (!en.id || !finite(en.feet))
                continue;
            for (const auto& s : seen_)
                if (s.id == en.id) {
                    const float dt = now - s.at;
                    Vec3 v = s.velocity;
                    if (dt > .02f && dt < .5f && length(en.feet - s.feet) > 1e-4f) {
                        const Vec3 raw = (en.feet - s.feet) * (1.f / dt);
                        v = length(raw) < 12.f ? (length(s.velocity) > 0 ? s.velocity * .4f + raw * .6f : raw) : Vec3{};
                        next[i].at = now;
                    } else if (dt <= .02f || length(en.feet - s.feet) <= 1e-4f) {
                        next[i].at = s.at; // the same sample (10 Hz under a 90 Hz frame): keep its time
                        next[i].feet = s.feet;
                    }
                    en.velocity = v;
                    next[i].velocity = v;
                    break;
                }
        }
        seen_ = next;
    }

  private:
    struct Seen {
        uint64_t id{};
        Vec3 feet{};
        float at{};
        Vec3 velocity{};
    };
    std::array<Seen, maxEnemies> seen_{};
};

// One ball in flight.
struct Ball {
    bool alive{};
    int hand = -1;
    uint64_t target{}; // the picked enemy's id (0 none)
    uint32_t serial{}; // Version 0.0.62: which fire() this is (1, 2, ...), for BOLT WALLS' casts
    float age{}, life{}, travelled{};
    Vec3 from{}, at{}, direction{};
    Vec3 tail() const { return at - direction * std::min(1.2f, travelled); }
};
// What a frame did.
struct Hit {
    int hand = -1;
    uint64_t enemy{};
    int index = -1; // into this frame's Enemies
    Vec3 at{};
};
struct Step {
    static constexpr int most = 4;
    Hit hits[most]{};
    int hitCount{};
    int expired{};
    // Version 0.0.62 BOLT WALLS: balls stopped at a wall this frame (the hand, where; tracking space).
    Hit walls[most]{};
    int wallCount{};
};

// Up to `balls` balls in flight (a fast trigger never drops a ball in flight); one per fire().
class Pool {
  public:
    static constexpr int balls = 6;
    // A press: one ball from `wrist` along `aim`, toward the picked enemy (led) if any. False when
    // the wrist or aim is unusable (nothing fired - counted).
    bool fire(int hand, Vec3 wrist, Vec3 aim, const Enemies& e, uint64_t locked, uint32_t strength) {
        if (!finite(wrist) || !finite(aim) || length(aim) < .5f) {
            ++refused_;
            return false;
        }
        const Tune t = tuneOf(strength);
        tune_ = strength;
        Ball b{};
        b.alive = true;
        b.hand = hand;
        b.from = b.at = wrist;
        b.direction = normalized(aim);
        b.life = t.range / speed + .1f;
        float off = 180.f;
        const int i = pick(wrist, aim, e, locked, t, &off);
        if (i >= 0) {
            const Enemy& en = e.list[static_cast<size_t>(i)];
            b.target = en.id ? en.id : 0x0b000000ull + static_cast<uint64_t>(i);
            const Vec3 aimAt = intercept(wrist, en.feet + Vec3{0, chest, 0}, en.velocity, t.lead);
            if (length(aimAt - wrist) > .05f)
                b.direction = normalized(aimAt - wrist);
            ++assisted_;
            lastDegrees_ = off;
        } else {
            ++unassisted_;
        }
        // the oldest gives way when all are flying
        int s = slot();
        b.serial = static_cast<uint32_t>(fired_ + 1);
        ball_[static_cast<size_t>(s)] = b;
        next_ = (s + 1) % balls;
        ++fired_;
        ++firedBy_[hand & 1];
        return true;
    }
    // A frame: each ball flies (homing on its enemy by id, or his index when he has no id), and a
    // pass through any enemy's body is a hit. Version 0.0.62 BOLT WALLS: `walls` (one per slot,
    // tracking space; nullptr or an unknown wall: none) stop a ball where its step reaches the wall -
    // an enemy short of the wall is still hit first, one behind it never is.
    Step update(float seconds, const Enemies& e, const bolt_walls::Wall* walls = nullptr) {
        Step out{};
        const float dt = std::isfinite(seconds) ? std::clamp(seconds, 0.f, .1f) : 0.f;
        const Tune t = tuneOf(tune_);
        for (int s = 0; s < balls; ++s) {
            Ball& b = ball_[static_cast<size_t>(s)];
            if (!b.alive)
                continue;
            // the target this frame
            int ti = -1;
            for (uint32_t i = 0; i < e.count && i < static_cast<uint32_t>(maxEnemies); ++i)
                if (b.target && (e.list[i].id ? e.list[i].id == b.target
                                              : 0x0b000000ull + i == b.target))
                    ti = static_cast<int>(i);
            if (ti >= 0 && t.turn > 0) {
                const Enemy& en = e.list[static_cast<size_t>(ti)];
                const Vec3 c = en.feet + Vec3{0, chest, 0};
                const Vec3 aimAt = intercept(b.at, c, en.velocity, t.lead);
                const Vec3 to = aimAt - b.at;
                if (length(to) > .02f) {
                    // magnetism: inside magnetMetres it turns as fast as it needs
                    const float rate = length(c - b.at) <= magnetMetres ? 1e4f : t.turn;
                    b.direction = detail::turnToward(b.direction, normalized(to), rate * .0174533f * dt);
                }
            }
            const Vec3 before = b.at;
            const float stepMetres = speed * dt;
            b.at = b.at + b.direction * stepMetres;
            b.travelled += stepMetres;
            b.age += dt;
            Vec3 cut{};
            const bool walled = walls && bolt_walls::crossed(walls[s], before, b.at, &cut);
            if (walled)
                b.at = cut;
            // a hit: the step's segment within hitRadius of an enemy's body line
            int hitIndex = -1;
            float hitDistance = 1e9f;
            for (uint32_t i = 0; i < e.count && i < static_cast<uint32_t>(maxEnemies); ++i) {
                const Vec3 feet = e.list[i].feet;
                if (!finite(feet))
                    continue;
                const float d = detail::segmentDistance(before, b.at, feet + Vec3{0, .3f, 0},
                                                        feet + Vec3{0, bodyHeight - .2f, 0});
                if (d <= t.hitRadius && d < hitDistance) {
                    hitDistance = d;
                    hitIndex = static_cast<int>(i);
                }
            }
            if (hitIndex >= 0) {
                b.alive = false;
                ++hits_;
                ++hitsBy_[b.hand & 1];
                if (out.hitCount < Step::most)
                    out.hits[out.hitCount++] = {b.hand, e.list[static_cast<size_t>(hitIndex)].id, hitIndex, b.at};
                continue;
            }
            if (walled) {
                b.alive = false;
                ++walled_;
                if (out.wallCount < Step::most)
                    out.walls[out.wallCount++] = {b.hand, 0, -1, b.at};
                continue;
            }
            if (b.age >= b.life || b.travelled >= t.range) {
                b.alive = false;
                ++misses_;
                ++out.expired;
            }
        }
        return out;
    }
    void clear() {
        for (auto& b : ball_)
            b.alive = false;
    }
    const Ball& ball(int s) const { return ball_[static_cast<size_t>(std::clamp(s, 0, balls - 1))]; }
    int flying() const {
        int n = 0;
        for (const auto& b : ball_)
            n += b.alive;
        return n;
    }
    uint64_t fired() const { return fired_; }
    uint64_t firedBy(int hand) const { return firedBy_[hand & 1]; }
    uint64_t hits() const { return hits_; }
    uint64_t hitsBy(int hand) const { return hitsBy_[hand & 1]; }
    uint64_t misses() const { return misses_; }
    uint64_t assisted() const { return assisted_; }
    uint64_t unassisted() const { return unassisted_; }
    uint64_t refused() const { return refused_; }
    uint64_t walled() const { return walled_; } // Version 0.0.62 BOLT WALLS
    float lastDegrees() const { return lastDegrees_; }

  private:
    int slot() const {
        for (int s = 0; s < balls; ++s)
            if (!ball_[static_cast<size_t>(s)].alive)
                return s;
        return next_;
    }
    std::array<Ball, balls> ball_{};
    int next_{};
    uint32_t tune_{};
    uint64_t fired_{}, firedBy_[2]{}, hits_{}, hitsBy_[2]{}, misses_{}, assisted_{}, unassisted_{}, refused_{}, walled_{};
    float lastDegrees_ = 180.f;
};

// The splash a hit leaves (drawn a moment), one per slot.
struct Splash {
    static constexpr float life = .45f;
    Vec3 at{};
    float age = life;
    void start(Vec3 p) {
        at = p;
        age = 0;
    }
    void update(float seconds) { age = std::min(life, age + (std::isfinite(seconds) ? std::clamp(seconds, 0.f, .1f) : 0.f)); }
    float fade() const { return std::clamp(1.f - age / life, 0.f, 1.f); }
};

} // namespace spidy::our_ball
