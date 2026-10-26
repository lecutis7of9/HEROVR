#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
//
// Version 0.0.3 AUTO PERFORMANCE (EXPERIMENTAL, OFF by default; the user's request pt349u): watches
// the frame time and steps a ranked ladder of LIVE graphics knobs down when frames are missed and
// back up when there is headroom. Pure and allocation-free; game_xr feeds it each frame and applies
// what effective() returns through the knobs' own live setters.
//
// The knobs (only live ones; nothing here needs a restart):
//   FAR WINDOWS reach (SpidyFarWindows, live since 38.3)   setter CONFIRMED; FPS gain CANDIDATE
//   SHARED LOD HIGHER -> GAME (SpidySharedLod, live 36)    setter CONFIRMED; FPS gain CANDIDATE
//   FAR SHADOWS -> OFF (SpidyFarShadows, live 35)          setter CONFIRMED; FPS gain CANDIDATE
// Supersampling is start-only (SpidyEyeSupersample refuses after XR start), so it is not a rung.
//
// Rules: the user's own values are the ceiling (a rung only ever lowers a knob, never raises one
// past what the user chose); OFF puts back exactly the user's values; a drop after ~0.5 s of
// misses, a raise after ~5 s of headroom; a raise that soon fails again is banned in that zone for a
// growing time (no oscillation); each zone (a 250 m cell of the city plus the activity: IDLE,
// SWING, FIGHT) remembers the rung it settled on, and entering it jumps there at once (the spike
// is anticipated rather than waited for).
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>

namespace spidy::auto_perf {

enum Target : uint32_t { targetAuto = 0, target72, target80, target90, target120, targets };
inline constexpr const char* targetNames[targets] = {"AUTO", "72", "80", "90", "120"};
inline constexpr float targetHz[targets] = {0.f, 72.f, 80.f, 90.f, 120.f};
// QUALITY FLOOR: how far down the ladder it may go.
enum Floor : uint32_t { floorMedium = 0, floorHigh, floorLow, floors };
inline constexpr const char* floorNames[floors] = {"MEDIUM", "HIGH", "LOW"};
enum Activity : uint32_t { idle = 0, swing = 1, fight = 2, activities = 3 };
inline constexpr const char* activityNames[activities] = {"IDLE", "SWING", "FIGHT"};

// The live knobs the ladder moves (the user's values, or what a rung leaves of them).
struct Knobs {
    uint32_t farWindowsPercent = 200; // 100 GAME .. 300
    uint32_t sharedLod = 2;           // shared_lod::Mode: 0 off, 1 game, 2 higher
    uint32_t farShadows = 2;          // far_shadows::Mode: 0 off, 1 eyes, 2 eyes+cache
    bool operator==(const Knobs&) const = default;
};
inline constexpr uint32_t rungs = 5; // 0 the user's values, 1-4 one more cut each
inline constexpr const char* rungNames[rungs] = {"YOUR SETTINGS", "FAR WINDOWS 1.5X", "FAR WINDOWS GAME",
                                                 "SHARED LOD GAME", "FAR SHADOWS OFF"};
// Whether each rung's knob is CONFIRMED to lower the cost (none is measured on the user's PC yet).
inline constexpr bool rungGainConfirmed[rungs] = {true, false, false, false, false};
inline uint32_t deepest(uint32_t floor) {
    return floor == floorHigh ? 1u : floor == floorLow ? 4u : 2u;
}
// The knobs at a rung: each cut is a min() with the user's own value, never a raise.
inline Knobs effective(const Knobs& user, uint32_t rung) {
    Knobs k = user;
    if (rung >= 1)
        k.farWindowsPercent = std::min(k.farWindowsPercent, 150u);
    if (rung >= 2)
        k.farWindowsPercent = std::min(k.farWindowsPercent, 100u);
    if (rung >= 3)
        k.sharedLod = std::min(k.sharedLod, 1u);
    if (rung >= 4)
        k.farShadows = 0;
    return k;
}
// The next rung down that changes something, or `rung` when none does (down to `last`).
inline uint32_t nextDown(const Knobs& user, uint32_t rung, uint32_t last) {
    for (uint32_t r = rung + 1; r <= last && r < rungs; ++r)
        if (!(effective(user, r) == effective(user, rung)))
            return r;
    return rung;
}
inline uint32_t nextUp(const Knobs& user, uint32_t rung) {
    for (uint32_t r = rung; r-- > 0;)
        if (!(effective(user, r) == effective(user, rung)))
            return r;
    return 0;
}
// AUTO: the headset's own rate rounded to a target (72/80/90/120), 90 when unknown.
inline float targetFps(uint32_t target, float headsetHz) {
    if (target > targetAuto && target < targets)
        return targetHz[target];
    if (!(headsetHz > 30.f) || !std::isfinite(headsetHz))
        return 90.f;
    float best = 72.f;
    for (uint32_t t = target72; t < targets; ++t)
        if (std::abs(targetHz[t] - headsetHz) < std::abs(best - headsetHz))
            best = targetHz[t];
    return best;
}
// Version 0.0.6 (Update 39.1): AUTO aims at half the headset rate when the full rate is out of reach.
// Reports rep0050 (0.0.5) and rep0022 (0.0.2) made 37 and 34.5 new pictures a second at 90 Hz, so AUTO's
// 90 was missed on every frame and the ladder sat at its floor for nothing; the runtime shows each
// picture for whole headset frames, so 45 (each picture exactly 2 frames) is the even rate below 90.
// Hysteresis: half below 75% of the full rate (mean over about 1 s), full again from 95%; the fixed
// targets (72/80/90/120) are kept as chosen. No game address is used.
struct PaceTarget {
    static constexpr float toHalf = .75f, toFull = .95f;
    bool half{};
    float pick(uint32_t target, float headsetHz, float meanFps) {
        const float full = targetFps(target, headsetHz);
        if (target != targetAuto || !(meanFps > 0) || !std::isfinite(meanFps)) {
            half = false;
            return full;
        }
        if (!half && meanFps < full * toHalf)
            half = true;
        else if (half && meanFps >= full * toFull)
            half = false;
        return half ? full / 2 : full;
    }
};
// A zone: a 250 m cell of the city (x, z) and the activity.
inline uint32_t zoneOf(float x, float z, uint32_t activity) {
    if (!std::isfinite(x) || !std::isfinite(z))
        return activity % activities;
    const auto cx = static_cast<int32_t>(std::floor(x / 250.f)) & 0x3ff;
    const auto cz = static_cast<int32_t>(std::floor(z / 250.f)) & 0x3ff;
    return static_cast<uint32_t>(cx) << 12 | static_cast<uint32_t>(cz) << 2 | (activity % activities);
}
inline uint32_t activityOf(float enemyMetres, float speed) {
    if (std::isfinite(enemyMetres) && enemyMetres < 15.f)
        return fight;
    return std::isfinite(speed) && speed > 6.f ? swing : idle;
}

enum Why : uint32_t { whyNone = 0, whyMiss, whyHeadroom, whyZone, whyFloor, whyOff, whyOn, whys };
inline constexpr const char* whyNames[whys] = {"", "frames missed", "headroom", "zone memory", "floor", "off: restored",
                                               "on"};
struct Event {
    float seconds{}, fps{};
    uint32_t from{}, to{}, why{}, zone{};
};
static_assert(sizeof(Event) == 24);

class Controller {
public:
    static constexpr float dropAfter = .5f, raiseAfter = 5.f, slack = 1.08f, headroom = 1.12f;
    static constexpr float missShare = .25f, banFirst = 20.f, banMost = 300.f;
    static constexpr uint32_t zones = 32, events = 32;

    // One frame. dt: real seconds since the last; frameMs: its frame time; returns true when the
    // rung changed (apply effective(user(), rung()) then).
    bool frame(bool on, float dt, float frameMs, float target, uint32_t zone, uint32_t floor) {
        if (!std::isfinite(dt) || dt <= 0 || dt > .5f)
            dt = 0;
        clock_ += dt;
        if (!on) {
            if (on_) {
                on_ = false;
                return change(0, whyOff, zone);
            }
            return false;
        }
        bool changed = false;
        if (!on_) {
            on_ = true;
            reset();
            changed = change(0, whyOn, zone);
        }
        const uint32_t last = deepest(floor);
        if (rung_ > last) // the floor was raised: back up to it at once
            return change(last, whyFloor, zone) || changed;
        if (zone != zone_) {
            zone_ = zone;
            missTime_ = goodTime_ = 0;
            if (const auto* z = find(zone); z && z->rung != rung_ && z->rung <= last)
                return change(z->rung, whyZone, zone) || changed;
        }
        if (!(target > 0) || !std::isfinite(frameMs) || frameMs <= 0 || dt == 0)
            return changed;
        const float budget = 1000.f / target;
        const bool missed = frameMs > budget * slack;
        // The share of missed frames over about 0.5 s, and the mean FPS over about 1 s.
        const float a = std::min(1.f, dt / dropAfter);
        missed_ += a * ((missed ? 1.f : 0.f) - missed_);
        const float b = std::min(1.f, dt / 1.f); // a 1 s mean: headroom is seen within about 2 s
        meanMs_ = meanMs_ > 0 ? meanMs_ + b * (frameMs - meanMs_) : frameMs;
        sinceChange_ += dt;
        missTime_ = missed_ > missShare ? missTime_ + dt : 0.f;
        goodTime_ = missed_ < .02f && 1000.f / meanMs_ >= target * headroom ? goodTime_ + dt : 0.f;
        if (missTime_ >= dropAfter && sinceChange_ >= 1.f) {
            const uint32_t down = nextDown(user_, rung_, last);
            if (down != rung_) {
                // A drop soon after a raise: that raise was too much here; ban it, longer each time.
                if (lastWhy_ == whyHeadroom && sinceChange_ < raiseAfter * 2)
                    ban(zone, down);
                remember(zone, down);
                return change(down, whyMiss, zone) || changed;
            }
        }
        if (goodTime_ >= raiseAfter && sinceChange_ >= raiseAfter && rung_ > 0) {
            const uint32_t up = nextUp(user_, rung_);
            if (!banned(zone, rung_)) {
                remember(zone, up);
                return change(up, whyHeadroom, zone) || changed;
            }
        }
        return changed;
    }
    void setUser(const Knobs& k) {
        user_ = k;
    }
    const Knobs& user() const {
        return user_;
    }
    Knobs now() const {
        return on_ ? effective(user_, rung_) : user_;
    }
    uint32_t rung() const {
        return rung_;
    }
    bool on() const {
        return on_;
    }
    float meanFps() const {
        return meanMs_ > 0 ? 1000.f / meanMs_ : 0.f;
    }
    float missedShare() const {
        return missed_;
    }
    uint64_t drops() const {
        return drops_;
    }
    uint64_t raises() const {
        return raises_;
    }
    uint64_t zoneJumps() const {
        return zoneJumps_;
    }
    uint64_t bans() const {
        return bans_;
    }
    uint32_t eventCount() const {
        return eventCount_;
    }
    const std::array<Event, events>& log() const {
        return log_;
    }

private:
    struct Zone {
        uint32_t key{}, rung{}, used{}, banMask{};
        float banUntil[rungs]{}, banFor{};
    };
    void reset() {
        rung_ = 0;
        missed_ = 0;
        meanMs_ = 0;
        missTime_ = goodTime_ = 0;
        sinceChange_ = 0;
        lastWhy_ = whyNone;
    }
    Zone* find(uint32_t key) {
        for (auto& z : zones_)
            if (z.used && z.key == key)
                return &z;
        return nullptr;
    }
    Zone& slot(uint32_t key) {
        if (auto* z = find(key))
            return *z;
        Zone* oldest = &zones_[0];
        for (auto& z : zones_)
            if (!z.used || z.used < oldest->used)
                oldest = &z;
        *oldest = {};
        oldest->key = key;
        oldest->used = ++stamp_;
        return *oldest;
    }
    void remember(uint32_t zone, uint32_t rung) {
        auto& z = slot(zone);
        z.rung = rung;
        z.used = ++stamp_;
    }
    // Ban raising out of `rung` (i.e. a raise to the rung above it failed) in this zone.
    void ban(uint32_t zone, uint32_t rung) {
        auto& z = slot(zone);
        z.banFor = z.banFor > 0 ? std::min(banMost, z.banFor * 2) : banFirst;
        if (rung < rungs)
            z.banUntil[rung] = clock_ + z.banFor;
        ++bans_;
    }
    bool banned(uint32_t zone, uint32_t rung) {
        const auto* z = find(zone);
        return z && rung < rungs && clock_ < z->banUntil[rung];
    }
    bool change(uint32_t to, uint32_t why, uint32_t zone) {
        if (to == rung_ && why != whyOn && why != whyOff)
            return false;
        Event& e = log_[eventCount_ % events];
        e = {clock_, meanFps(), rung_, to, why, zone};
        ++eventCount_;
        drops_ += to > rung_ ? 1 : 0;
        raises_ += to < rung_ && why == whyHeadroom ? 1 : 0;
        zoneJumps_ += why == whyZone ? 1 : 0;
        rung_ = to;
        sinceChange_ = 0;
        missTime_ = goodTime_ = 0;
        lastWhy_ = why;
        return true;
    }
    Knobs user_{};
    bool on_{};
    uint32_t rung_{}, zone_{~0u}, lastWhy_{}, stamp_{}, eventCount_{};
    float clock_{}, missed_{}, meanMs_{}, missTime_{}, goodTime_{}, sinceChange_{};
    uint64_t drops_{}, raises_{}, zoneJumps_{}, bans_{};
    std::array<Zone, zones> zones_{};
    std::array<Event, events> log_{};
};

// The status line (SHOW STATUS), e.g. "AUTO PERF 88/90 FPS - FAR WINDOWS GAME".
inline int status(char* out, unsigned size, const Controller& c, float target) {
    if (!out || !size)
        return 0;
    const int fps = static_cast<int>(std::lround(std::min(999.f, c.meanFps())));
    const int t = static_cast<int>(std::lround(target));
    return std::snprintf(out, size, "AUTO PERF %d/%d FPS - %s", fps, t, rungNames[std::min(c.rung(), rungs - 1)]);
}

} // namespace spidy::auto_perf
