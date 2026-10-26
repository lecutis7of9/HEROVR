#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
//
// Version 0.0.4 IRON MAN ABILITIES (the player flies with FLY MODE in an Iron Man skin): what the wrist
// fires on a trigger press. WEB (default) is 0.0.3's web ball. REPULSOR fires only a glowing palm
// blast from the wrist of the hand that pulled, one per press, with the web ball's aim assist
// (our_ball's pool and AIM STRENGTH): no web ball, no web look, no splash or cocoon, no RB to the
// game, no trigger gadget (Grabber, Upshot, Ricochet, yank, Sonic: all web or the game's), so
// nothing else comes out on that trigger.
//
// CHEST BEAM (REPULSOR only): both triggers pulled within chordWindow of each other and held
// beamHold fire one large beam from the chest, straight ahead of where the chest faces, worth
// beamShare blasts. 30 s cooldown; while it cools a chord does nothing at all (no blast, no web);
// a chord is always the beam's (released early: nothing), so it never doubles as two blasts. A
// single press waits chordWindow for the other hand before its blast goes (the EVERY HIT budget).
//
// Damage: SM2's hit path for the punch is the game's own X press (CONFIRMED to be a melee blow);
// a ranged blast has no confirmed damage call or enemy write, so the blast's and the beam's
// damage are CANDIDATE: counted (Ledger, the record's would-be damage per enemy) and NOT sent.
// Knockback likewise (no confirmed enemy write: rope_grab.hpp). Pure; no game memory.
#include "spidy/math.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>

namespace spidy::repulsor {

enum Type : uint32_t { web = 0, repulsor = 1, types };
inline constexpr const char* typeNames[types] = {"WEB", "REPULSOR"};

// What a trigger press may make, per IRON MAN ABILITIES. REPULSOR: only our shot, drawn as the blast.
struct Plan {
    bool ourShot{};      // the mod's own shot from the pool (our_ball)
    bool gameWebBall{};  // the game's web ball (RB, BALL LOOK WRIST / GLOB / GAME ONLY)
    bool webLook{};      // our shot drawn as a web ball
    bool webSplash{};    // the web splash at a hit
    bool cocoon{};       // the webbed-up look (web_cocoon)
    bool gameWebOnHit{}; // BALL HIT +GAME WEB
    bool triggerGadgets{}; // Grabber, Upshot, Ricochet, yank and Sonic on the trigger
    bool repulsorLook{}; // the blast's look
    bool chestBeam{};    // both triggers held: the beam
};
// ballOurs: BALL LOOK is OURS; gameWebOnHit: BALL HIT is +GAME WEB.
inline Plan planFor(uint32_t type, bool ballOurs, bool gameWebOnHit) {
    Plan p;
    if (type == repulsor) {
        p.ourShot = p.repulsorLook = p.chestBeam = true;
        return p; // everything web stays false
    }
    p.ourShot = ballOurs;
    p.gameWebBall = !ballOurs;
    p.webLook = p.webSplash = p.cocoon = ballOurs;
    p.gameWebOnHit = ballOurs && gameWebOnHit;
    p.triggerGadgets = true;
    return p;
}
// Web shots a press would make under a plan (the test's "zero web shots in REPULSOR").
inline uint32_t webShotsPerPress(const Plan& p) {
    return (p.gameWebBall ? 1u : 0u) + (p.ourShot && p.webLook ? 1u : 0u) + (p.gameWebOnHit ? 1u : 0u);
}

inline constexpr float chordWindow = .12f; // both presses this close: a chord (= combat_input's Sonic grace)
inline constexpr float beamHold = .35f;    // the chord held this long fires the beam
inline constexpr float beamCooldown = 30.f;
inline constexpr float beamRange = 40.f, beamRadius = .9f, beamSeconds = .7f, chestBelowHead = .35f;
inline constexpr float blastDamage = 1.f, beamShare = 4.f, beamDamage = beamShare * blastDamage;
inline constexpr float blastLife = .25f; // the impact flash

struct Out {
    bool blast[2]{};     // fire this hand's blast now (direction: blastAim)
    Vec3 blastAim[2]{};
    bool beam{};         // fire the chest beam now
    bool cooledChord{};  // a chord came while cooling: nothing fired
    bool ready{};        // the cooldown just ended (the cue)
};

// The trigger's chord and the beam's cooldown. press: this hand's shot press this frame (one per
// press, combat_input's Out::ballOurs); aim: its flight; held: the trigger is still down.
class Trigger {
public:
    Out update(float now, const bool press[2], const Vec3 aim[2], const bool held[2]) {
        Out o;
        if (!std::isfinite(now))
            return o;
        now_ = now;
        if (cooling_ && now >= readyAt_) {
            cooling_ = false;
            o.ready = true;
            ++readyCues_;
        }
        for (int i = 0; i < 2; ++i) {
            if (!press[i])
                continue;
            if (chord_ != idle) // a third press during a chord is the chord's
                continue;
            const int other = 1 - i;
            if (pending_[other] && now - pendingAt_[other] <= chordWindow) {
                pending_[other] = false;
                chord_ = waiting;
                chordAt_ = now;
                ++chords_;
                continue;
            }
            pending_[i] = true;
            pendingAt_[i] = now;
            pendingAim_[i] = aim[i];
        }
        // A single press with no partner within the window: its blast.
        for (int i = 0; i < 2; ++i)
            if (pending_[i] && now - pendingAt_[i] > chordWindow) {
                pending_[i] = false;
                blastAt_[i] = now;
                o.blast[i] = true;
                o.blastAim[i] = pendingAim_[i];
                ++blasts_;
            }
        if (chord_ == waiting) {
            if (!held[0] || !held[1]) {
                chord_ = spent; // released before the hold: nothing
                ++shortChords_;
            } else if (now - chordAt_ >= beamHold) {
                chord_ = spent;
                if (cooling_) {
                    o.cooledChord = true;
                    ++cooledChords_;
                } else {
                    o.beam = true;
                    ++beams_;
                    cooling_ = true;
                    readyAt_ = now + beamCooldown;
                }
            }
        }
        if (chord_ == spent && !held[0] && !held[1])
            chord_ = idle;
        return o;
    }
    void clear() {
        pending_[0] = pending_[1] = false;
        blastAt_[0] = blastAt_[1] = -1.f;
        chord_ = idle;
    }
    bool cooling() const { return cooling_; }
    float cooldownLeft() const { return cooling_ ? std::max(0.f, readyAt_ - now_) : 0.f; }
    bool chordWaiting() const { return chord_ == waiting; }
    // Version 0.0.6 (Update 39.0.2) REPULSOR LOOK (repulsor_mcu.hpp): how long this hand's palm has been
    // charging (its press waiting out the chord window; a chord held for the beam: both palms, full),
    // -1 when it is not; and seconds since its last blast (-1 none yet).
    float chargingFor(int i) const {
        if (i < 0 || i > 1)
            return -1.f;
        if (pending_[i])
            return std::max(0.f, now_ - pendingAt_[i]);
        return chord_ == waiting ? chordWindow + std::max(0.f, now_ - chordAt_) : -1.f;
    }
    float sinceBlast(int i) const { return i >= 0 && i <= 1 && blastAt_[i] >= 0 ? std::max(0.f, now_ - blastAt_[i]) : -1.f; }
    uint64_t blasts() const { return blasts_; }
    uint64_t beams() const { return beams_; }
    uint64_t chords() const { return chords_; }
    uint64_t shortChords() const { return shortChords_; }
    uint64_t cooledChords() const { return cooledChords_; }
    uint64_t readyCues() const { return readyCues_; }

private:
    enum Chord { idle, waiting, spent };
    bool pending_[2]{};
    float pendingAt_[2]{};
    Vec3 pendingAim_[2]{};
    Chord chord_ = idle;
    float chordAt_{}, readyAt_{}, now_{};
    float blastAt_[2]{-1.f, -1.f};
    bool cooling_{};
    uint64_t blasts_{}, beams_{}, chords_{}, shortChords_{}, cooledChords_{}, readyCues_{};
};

// The chest and where it faces: the head's level forward (the torso is not tracked), the chest
// chestBelowHead under the head and 0.1 m ahead of it.
inline bool chestRay(Vec3 head, Vec3 forward, Vec3& from, Vec3& dir) {
    Vec3 f{forward.x, 0, forward.z};
    if (!finite(head) || !finite(f) || length(f) < 1e-3f)
        return false;
    dir = normalized(f);
    from = head - Vec3{0, chestBelowHead, 0} + dir * .1f;
    return true;
}
// The enemies the beam strikes: a chest-high point (feet + 1 m) within beamRadius of the ray, 0.3 to
// beamRange ahead. Returns how many (indices in `hit`, at most `most`).
inline int beamHits(Vec3 from, Vec3 dir, const Vec3* feet, int count, int* hit, int most) {
    int n = 0;
    if (!finite(from) || !finite(dir) || !feet || !hit)
        return 0;
    for (int i = 0; i < count && n < most; ++i) {
        const Vec3 c = feet[i] + Vec3{0, 1.f, 0};
        if (!finite(c))
            continue;
        const float along = dot(c - from, dir);
        if (along < .3f || along > beamRange)
            continue;
        if (length(c - (from + dir * along)) <= beamRadius)
            hit[n++] = i;
    }
    return n;
}

// Version 0.0.6 BEAM AIM ASSIST (rep0050: a beam at 460.6 s caught nobody, aimed along the head's
// level forward with no enemy near): the beam turns to the chest of the enemy nearest the head's
// forward within beamAssistDeg and beamRange (pitch too); none: the level forward, as 0.0.5.
// Returns the enemy's index (-1 none) and sets dir.
inline constexpr float beamAssistDeg = 30.f;
inline int beamAim(Vec3 from, Vec3 forward, const Vec3* feet, int count, Vec3& dir) {
    Vec3 f{forward.x, 0, forward.z};
    if (!finite(from) || !finite(f) || length(f) < 1e-3f)
        return -1;
    f = normalized(f);
    dir = f;
    if (!feet)
        return -1;
    const float cosMax = std::cos(beamAssistDeg * 3.14159265f / 180.f);
    int best = -1;
    float bestCos = cosMax;
    for (int i = 0; i < count; ++i) {
        const Vec3 c = feet[i] + Vec3{0, 1.f, 0};
        if (!finite(c))
            continue;
        const Vec3 to = c - from;
        const float d = length(to);
        if (!(d >= .3f) || d > beamRange)
            continue;
        const Vec3 u = to / d;
        const float flat = length(Vec3{u.x, 0, u.z});
        const float cosFlat = flat > 1e-4f ? dot(Vec3{u.x, 0, u.z} / flat, f) : 0.f;
        if (cosFlat > bestCos) {
            bestCos = cosFlat;
            best = i;
            dir = u;
        }
    }
    return best;
}
// The nearest enemy's distance from `from` (-1 none): the log says why a beam caught nobody.
inline float nearestMetres(Vec3 from, const Vec3* feet, int count) {
    float best = -1;
    for (int i = 0; feet && i < count; ++i) {
        const float d = length(feet[i] + Vec3{0, 1.f, 0} - from);
        if (std::isfinite(d) && (best < 0 || d < best))
            best = d;
    }
    return best;
}

// The damage counted per kind (Version 0.0.6: each hit also sends its blow, sm2_damage's CONFIRMED path).
struct Ledger {
    float blastTotal{}, beamTotal{};
    uint64_t blastHits{}, beamHits{}, sent{}; // sent stays 0 until a damage call is confirmed
    void blast() {
        blastTotal += blastDamage;
        ++blastHits;
    }
    void beam() {
        beamTotal += beamDamage;
        ++beamHits;
    }
};

// The beam's look over its life (0..1 at its start, fading to 0 at beamSeconds).
inline float beamFade(float age) {
    if (!std::isfinite(age) || age < 0 || age >= beamSeconds)
        return 0;
    const float rise = std::min(1.f, age / .06f);
    return rise * (1 - age / beamSeconds);
}
// Version 0.0.5 BEAM RING: the right wrist's ring fills over the 30 s cooldown (0 just fired, 1
// ready) and flashes for readyFlashSeconds once ready (with the beam_ready chime and a buzz).
inline constexpr float readyFlashSeconds = 1.2f;
inline float chargeShare(const Trigger& t) {
    if (!t.cooling())
        return 1.f;
    const float left = t.cooldownLeft();
    return std::isfinite(left) ? std::clamp(1.f - left / beamCooldown, 0.f, 1.f) : 1.f;
}
// 1 at the ready moment, falling to 0 over readyFlashSeconds; 0 before (age < 0) or after.
inline float readyFlash(float age) {
    if (!std::isfinite(age) || age < 0 || age >= readyFlashSeconds)
        return 0.f;
    return 1.f - age / readyFlashSeconds;
}
// The log's cooldown line: "BEAM READY" or "BEAM 23 S" (0.0.4's floating line; 0.0.5 shows the ring).
inline void status(char* out, size_t size, const Trigger& t) {
    if (!out || !size)
        return;
    const int left = static_cast<int>(std::ceil(t.cooldownLeft()));
    if (t.cooling() && left > 0)
        std::snprintf(out, size, "BEAM %d S", left);
    else
        std::snprintf(out, size, "BEAM READY");
}
} // namespace spidy::repulsor
