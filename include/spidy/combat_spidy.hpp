#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
//
// Update 38: SpidyVR's combat logic, the PURE parts, folded into the SM2 combat input
// (combat_input.hpp) and the swing bridge (game_swing.cpp's SPIDYVR MOVE lunge). No new
// game address: the enemies are the WORLD LOG's registry list (CONFIRMED records, their
// CONFIRMED position read, world_trace.hpp), the sight checks the controller ray bridge's
// own rays (native_rays, CONFIRMED). What comes from SpidyVR, unchanged in its numbers:
//   - Punches (punch.cpp): the fist's speed relative to the head (the arm's own motion),
//     re-armed once it slows under 1.2 m/s or 0.45 s after a hit; an 8 cm fist swept into
//     each enemy's body capsule, at 2.2 m/s or more, into his middle or his head.
//   - Lock (web_strike.cpp): the enemy the head looks at within 40 degrees and 10 m,
//     sticky until the player looks squarely at another one for 0.3 s.
//   - WebStrike::open (web_strike.cpp): the lunge - a fist thrown outward from the neck at
//     2.6 m/s for 40 ms at an enemy 1.3-10 m away, within 37 degrees (60 at the marked one),
//     in sight.
//   - Shooter::aim (shooter.cpp): the web ball's assist - the enemy nearest the hand's ray
//     as a share of max(0.12 rad, his width's angle), within 45 m, no wall between.
//   - WebGrab's yank (web_grab.cpp): 14 cm back along the web within 0.25 s at 1.3 m/s or
//     more, from head-relative samples.
//   - Shooter's trigger edges: the ball goes on the PRESS.
// NOT ported (research/telemetry only, Remastered addresses with no SM2 evidence): the
// direct damage requests, the fling of a struck enemy, and the game's own shot hook.
#include "punch.hpp"
#include "swing.hpp"
#include "web_strike.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace spidy::combat_spidy {

// What a sight ray found for one enemy: none yet, a clear line, or a wall between.
enum Sight : uint8_t { sightUnknown = 0, sightClear = 1, sightBlocked = 2 };

// The world SpidyVR's open() and aim() ask "is he in sight?" of, answered from the
// latest sight ray to each enemy (cast from the head to his middle). A question about
// any other line (the shot's own ray past the target) finds nothing. An enemy whose line
// was never cast counts as in sight (counted: unknownAsked).
class SightWorld final : public WorldQueries {
  public:
    struct Entry {
        Vec3 middle{};
        Sight sight = sightUnknown;
    };
    explicit SightWorld(std::span<const Entry> entries) : entries_(entries) {}
    std::optional<RayHit> raycast(Vec3 origin, Vec3 direction, float distance) const override {
        if (!finite(origin) || !finite(direction) || !std::isfinite(distance))
            return std::nullopt;
        const Vec3 end = origin + direction * distance;
        for (const auto& e : entries_)
            if (length(e.middle - end) < .15f) {
                if (e.sight == sightUnknown)
                    ++unknownAsked;
                if (e.sight != sightBlocked)
                    return std::nullopt;
                // The wall between: well short of him (WebStrike's inSight wants it more
                // than a metre short, Shooter::aim more than his radius).
                RayHit hit;
                hit.point = origin + direction * std::max(0.f, distance * .5f - 1.5f);
                hit.fixed = true;
                return hit;
            }
        return std::nullopt;
    }
    bool exists(std::uint64_t) const override { return true; }
    mutable uint64_t unknownAsked{};

  private:
    std::span<const Entry> entries_;
};

// Shooter::aim's assist (shooter.cpp), on a ray instead of a pose: the target nearest the
// line as a share of the angle it may be off it (max(assistAngle, atan(radius/d))), within
// assistRange and in sight. Returns the index in `targets`, -1 for none; `share` its share.
struct AimTarget {
    Vec3 centre{};
    float radius = .45f;
};
inline constexpr float assistAngle = .12f, assistRange = 45.f;
inline int spidyAim(Vec3 origin, Vec3 forward, std::span<const AimTarget> targets, const WorldQueries& world,
                    float* share = nullptr) {
    if (!finite(origin) || !finite(forward) || length(forward) < .5f)
        return -1;
    forward = normalized(forward);
    int best = -1;
    float bestShare = 2;
    for (size_t i = 0; i < targets.size(); ++i) {
        const auto& t = targets[i];
        const Vec3 to = t.centre - origin;
        const float distance = length(to);
        if (!finite(t.centre) || distance < .3f || distance > assistRange)
            continue;
        const Vec3 along = to / distance;
        const float angle = std::acos(std::clamp(dot(along, forward), -1.f, 1.f));
        const float allowed = std::max(assistAngle, std::atan2(std::max(t.radius, 0.f), distance));
        if (angle > allowed || angle / allowed >= bestShare)
            continue;
        if (const auto hit = world.raycast(origin, along, distance);
            hit && length(hit->point - origin) < distance - std::max(t.radius, 0.f))
            continue;
        best = static_cast<int>(i);
        bestShare = angle / allowed;
    }
    if (share)
        *share = best >= 0 ? bestShare : 2.f;
    return best;
}

// WebGrab's yank (web_grab.cpp): head-relative hand motion back along the web (from the
// wrist toward where the web went), summed within `window`; a yank once `distance` of it
// is reached at `speed` or faster in the latest sample. Steps and travel never count.
struct Yank {
    static constexpr float distance = .14f, window = .25f, speed = 1.3f;
    bool seen{};
    Vec3 previous{};
    float pulled{}, time{};
    void reset() { *this = Yank{}; }
    // relative: the hand less the head (tracking space); toward: unit, along the web.
    bool update(float seconds, Vec3 relative, Vec3 toward, float loose = 1.f) {
        if (!(seconds > 0) || !finite(relative) || !finite(toward) || length(toward) < .5f) {
            seen = false;
            return false;
        }
        if (!seen) {
            seen = true;
            previous = relative;
            pulled = time = 0;
            return false;
        }
        const float pull = -dot(relative - previous, normalized(toward));
        previous = relative;
        if (pull <= 0) {
            pulled = time = 0;
            return false;
        }
        pulled += pull;
        time += seconds;
        if (time > window) {
            pulled = pull;
            time = seconds;
        }
        const float l = std::isfinite(loose) ? std::clamp(loose, .5f, 2.f) : 1.f;
        return pulled >= distance && pull / seconds >= speed / l;
    }
};

// The enemies as SpidyVR's types take them (tracking space or world: one frame for all).
inline constexpr float bodyHeight = 1.8f, bodyRadius = .3f;
inline PunchTarget punchTarget(uint64_t id, Vec3 feet) { return {id, feet, bodyHeight, bodyRadius}; }
inline FightTarget fightTarget(uint64_t id, Vec3 feet) { return {id, feet, bodyHeight, bodyRadius, false}; }

// SPIDYVR MOVE: the lunge the swing bridge flies (game_swing.cpp), SpidyVR's own open()
// and flight (WebStrike::step) with the bridge's world rays for sight, and the enemies the
// XR side sends with each command. No blow of its own: on arrival the XR side presses the
// game's X (combat_input), so the hit is the game's. Positions are the world's.
class Lunger {
  public:
    struct Fist {
        bool tracked{}, busy{};
        Vec3 fist{}, relative{}; // world; relative: the grip less the head, turned to the world
        std::uint64_t holds{};
        float web{};
    };
    // One input sample. Returns the strike opened, if one did (the flight starts at fly()).
    std::optional<WebStrike::Opening> open(float seconds, const std::array<Fist, 2>& in, Vec3 look, Vec3 feet,
                                           std::span<const FightTarget> enemies, const WorldQueries& world) {
        if (!(seconds > 0))
            return std::nullopt;
        enemies_.assign(enemies.begin(), enemies.end());
        std::array<PunchHand, 2> hands{};
        Vec3 head{};
        int tracked{};
        for (int i = 0; i < 2; ++i) {
            const auto& f = in[static_cast<size_t>(i)];
            hands[static_cast<size_t>(i)] = {f.tracked, f.fist, f.relative, f.busy};
            if (f.tracked && finite(f.fist) && finite(f.relative)) {
                head += f.fist - f.relative;
                ++tracked;
            }
        }
        head = tracked ? head / static_cast<float>(tracked) : feet + Vec3{0, 1.6f, 0};
        events_.clear();
        punches_.update(seconds, hands, {}, events_); // the arm's speed alone: no blow
        lock_.update(seconds, head, look, enemies_);
        std::array<FightFist, 2> fists{};
        for (int i = 0; i < 2; ++i) {
            const auto& f = in[static_cast<size_t>(i)];
            fists[static_cast<size_t>(i)] = {f.tracked, f.fist, punches_.velocity(i), !f.busy, f.holds, f.web};
        }
        StrikeWish wish;
        wish.target = lock_.target();
        // Version 0.0.62 AIM PICK (FLY MODE's lunge too): the fist and the look pick the enemy.
        wish.gaze = look;
        wish.gazePick = true;
        const bool looking = finite(look) && length(look) > .5f;
        wish.freeAim = !looking; // no look known: any enemy the fist points at, as before
        const std::span<const FightTarget> strikeable{enemies_};
        auto opened = strike_.open(seconds, fists, head, feet, strikeable, world, wish);
        if (opened) {
            punches_.spend(opened->hand);
            lock_.take(opened->target);
            ++opened_;
        }
        return opened;
    }
    struct Flight {
        bool drive{};
        Vec3 velocity{};
    };
    Flight fly(float seconds, Vec3 feet, bool grounded, bool waiting = false) {
        Flight out;
        if (strike_.phase() == WebStrike::Phase::Idle)
            return out;
        if (waiting && strike_.phase() == WebStrike::Phase::Flying) {
            strike_.wait(seconds);
            missed_ += strike_.phase() == WebStrike::Phase::Idle;
            return out;
        }
        const FightTarget* target{};
        for (const auto& e : enemies_)
            if (e.id == strike_.target())
                target = &e;
        const bool flying = strike_.phase() == WebStrike::Phase::Flying;
        const auto step = strike_.step(seconds, feet, grounded, flying ? target : nullptr);
        out.drive = step.drive;
        out.velocity = step.velocity;
        landed_ += step.landed;
        missed_ += step.missed;
        // A lunge that lands in the air hangs a moment there (game_punch's rule) for the
        // game's own blow (the X the XR side presses on arrival).
        if (step.landed && !grounded)
            strike_.hangOn(false);
        return out;
    }
    // Update 38.5 FLY MODE: the lunge's reach and aim (combat_input::farStrike, the zip strike's),
    // taken while no lunge flies; the default WebStrikeConfig{} is 38's SPIDYVR MOVE as it was.
    bool configure(const WebStrikeConfig& sc) {
        if (strike_.phase() != WebStrike::Phase::Idle)
            return false;
        strike_ = WebStrike(sc);
        LockConfig lc = lock_.config(); // the lock as far as the lunge (combat_input's farLunge rule)
        lc.reach = sc.maxDistance;
        lock_.configure(lc);
        return true;
    }
    // A snap or smooth turn of the tracking space (the hands turned with the player).
    void turn(Quat change) { punches_.turn(change); }
    bool lifting(Vec3 feet) const { return strike_.lifting(feet); }
    WebStrike::Phase phase() const { return strike_.phase(); }
    std::uint64_t target() const { return strike_.target(); }
    int hand() const { return strike_.hand(); }
    bool active() const { return strike_.phase() != WebStrike::Phase::Idle; }
    void cancel() {
        if (strike_.phase() != WebStrike::Phase::Idle) {
            missed_ += strike_.phase() == WebStrike::Phase::Flying;
            strike_.cancel();
        }
    }
    void reset() {
        strike_.cancel();
        punches_.reset();
        lock_.clear();
        enemies_.clear();
    }
    uint64_t opened() const { return opened_; }
    uint64_t landed() const { return landed_; }
    uint64_t missed() const { return missed_; }

  private:
    Punches punches_{};
    Lock lock_{};
    WebStrike strike_{};
    std::vector<FightTarget> enemies_;
    std::vector<PunchEvent> events_;
    uint64_t opened_{}, landed_{}, missed_{};
};
// SpidyLungeSample (game_swing.cpp, the ray module): the bridge's SPIDYVR MOVE lunges, so
// the XR side presses the game's X when one lands. phase: 0 idle, 1 flying, 2 hanging.
struct LungeData {
    uint32_t magic = 0x4c4e4753, version = 1, bytes = 64, phase{}; // SGNL
    uint64_t opened{}, landed{}, missed{}, target{};
    int32_t hand = -1, mode{};
    uint64_t sequence{};
};
static_assert(sizeof(LungeData) == 64);
} // namespace spidy::combat_spidy
