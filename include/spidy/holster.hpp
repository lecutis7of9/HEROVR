#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
//
// Version 0.0.7 HOLSTERS: one shared, reusable state machine for things worn on the body and drawn
// by hand (PUNISHER's two hip pistols and the sawed-off on his back; written so DAREDEVIL's batons
// or BATMAN's batarangs can use the same slots later - their 0.0.62 behaviour is unchanged here).
//  - The body frame: the headset's yaw, the body turning with it only past yawDeadZone (at up to
//    yawSpeed) and drifting toward it at yawDrift a second meanwhile (as phone.hpp's PocketDraw),
//    so a look over a shoulder does not move a hip holster under a hanging hand.
//  - Zones in that frame from the head (metres): a HIP is hipBelow..hipLow under the head,
//    hipSide..hipOut to its side, hipBack behind to hipFront ahead of the eyes (not behind the hip:
//    the back pocket there is the phone's); the BACK is backAbove over to backBelow under the
//    head, within backSide of the centre line and at least backBehind behind the eyes (reach over
//    a shoulder).
//  - A grip PRESSED (past `pressed`) with the hand in a holstered slot's zone, the hand empty and
//    free (no web attached, not climbing, the grip not already someone else's) DRAWS it: a haptic
//    tick. The grip is the slot's while it is held - no web, zip or climb from it. RELEASED (under
//    `released`) back in the slot's zone it HOLSTERS: a haptic tick. Released anywhere else it goes
//    back to its slot by itself (returnedAway: nothing is ever dropped or lost).
//  - Hand mapping: a slot names the hands that may draw it. sameSideOnly (the default): the left
//    hip's pistol only to the left hand, the right hip's only to the right; the back to either.
//    crossDraw lets either hand draw either hip (HOLSTER HANDS EITHER HAND). A slot added with its own
//    hands (CAPTAIN AMERICA's shield: the SHIELD ARM's hand only) keeps them whatever crossDraw is.
//  - A hand the headset loses keeps what it holds and its zone reading from lostSeconds ago at
//    most; out of VR play nothing is drawn (what is held stays held).
// Everything stays on the body while holstered (hero007_visual draws it there: the built-in looks,
// or the converted models with GUN MODELS / BATON LOOK). Pure; no game memory, no address.
#include "spidy/math.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace spidy::holster {

// ---- the body frame ------------------------------------------------------------------------------
struct BodyYaw {
    // Version 0.0.73 HOLSTERS TURN WITH THE BODY (fix 8): the dead zone halved (34 to 17 degrees) and the
    // drift three times as quick (the body re-centres under the head in about 0.7 s), so the hip and back
    // holsters follow the player's facing instead of staying where he stood.
    static constexpr float yawDeadZone = .3f, yawSpeed = 6.f, yawDrift = 1.5f;
    float yaw = 0;
    bool valid{};
    // Test build 12 BODY SCALE: the heights below the head are for a standingEye (1.62 m) player and scale
    // with the calibrated eye height (scaleFor), so the hips sit at the player's own hips.
    float scale = 1;
    static float headYaw(Vec3 forward) { return std::atan2(forward.x, -forward.z); } // 0: -z ahead
    static float wrap(float a) {
        while (a > 3.14159265f)
            a -= 6.2831853f;
        while (a < -3.14159265f)
            a += 6.2831853f;
        return a;
    }
    void update(float dt, Vec3 headForward) {
        if (!finite(headForward) || std::hypot(headForward.x, headForward.z) < 1e-3f)
            return;
        const float h = headYaw(headForward);
        if (!valid) {
            yaw = h;
            valid = true;
            return;
        }
        const float t = std::isfinite(dt) ? std::clamp(dt, 0.f, .1f) : 0.f;
        const float d = wrap(h - yaw);
        float step = d * std::min(1.f, yawDrift * t);
        if (std::abs(d) > yawDeadZone) // past the dead zone: catch up to its edge quickly
            step = (d > 0 ? 1.f : -1.f) * std::min(std::abs(d) - yawDeadZone + std::abs(step), yawSpeed * t);
        yaw = wrap(yaw + step);
    }
    Vec3 forward() const { return {std::sin(yaw), 0, -std::cos(yaw)}; }
    Vec3 right() const { return {std::cos(yaw), 0, std::sin(yaw)}; }
};
// Test build 12: the body scale for an eye height (metres; 0 or out of range: 1, the standing average).
inline constexpr float standingEye = 1.62f;
inline float scaleFor(float eyeHeight) {
    return std::isfinite(eyeHeight) && eyeHeight >= 1.f && eyeHeight <= 2.5f ? std::clamp(eyeHeight / standingEye, .7f, 1.35f) : 1.f;
}
// Where the hand is in the body frame: below (+ under the head, in standingEye metres: divided by the
// body's scale), side (+ right), ahead (+ in front).
struct BodyPoint {
    float below, side, ahead;
};
inline BodyPoint bodyPoint(Vec3 hand, Vec3 head, const BodyYaw& body) {
    const Vec3 r = hand - head;
    const float k = std::isfinite(body.scale) && body.scale > .5f ? body.scale : 1.f;
    return {-r.y / k, dot(r, body.right()), dot(r, body.forward())};
}

// ---- zones --------------------------------------------------------------------------------------
// backLeft / backRight: the back split at the spine (DAREDEVIL's two batons over the shoulders).
enum Zone : uint32_t { leftHip = 0, rightHip, back, backLeft, backRight, zones };
inline constexpr const char* zoneNames[zones] = {"LEFT HIP", "RIGHT HIP", "BACK", "BACK LEFT", "BACK RIGHT"};
// Test build 12 HOLSTERS HIGHER (the user: the PUNISHER's holsters "too low"): the hip zone runs from the
// waist (.45 under the eyes) to mid-thigh (.95), generous to the side and ahead (the pistols' grips now sit
// at hipGrip, the hip bone, not .86 m down by the knee).
inline constexpr float hipBelow = .45f, hipLow = .95f, hipSide = .10f, hipOut = .45f, hipBack = .16f, hipFront = .30f;
inline constexpr float backAbove = .10f, backBelow = .50f, backSide = .35f, backBehind = .10f;
inline bool inZone(uint32_t zone, BodyPoint p) {
    if (!std::isfinite(p.below) || !std::isfinite(p.side) || !std::isfinite(p.ahead))
        return false;
    switch (zone) {
    case leftHip:
    case rightHip: {
        const float out = zone == leftHip ? -p.side : p.side;
        return p.below >= hipBelow && p.below <= hipLow && out >= hipSide && out <= hipOut && p.ahead >= -hipBack &&
               p.ahead <= hipFront;
    }
    case back:
    case backLeft:
    case backRight: {
        const bool backOk =
            p.below >= -backAbove && p.below <= backBelow && std::abs(p.side) <= backSide && p.ahead <= -backBehind;
        return backOk && (zone == back || (zone == backLeft ? p.side <= .02f : p.side >= -.02f));
    }
    default:
        return false;
    }
}
// The hands (bit 1 << hand: 1 left, 2 right) that may draw from a zone.
inline uint32_t handsFor(uint32_t zone, bool crossDraw) {
    if (zone == back || crossDraw)
        return 3u;
    return zone == leftHip || zone == backLeft ? 1u : 2u;
}

// ---- the rack ------------------------------------------------------------------------------------
inline constexpr float pressed = .6f, released = .3f, lostSeconds = .5f;
inline constexpr uint32_t maxSlots = 4;
struct Slot {
    uint32_t zone = leftHip;
    uint32_t item{};   // the caller's: what lives there (e.g. hero007::pistol)
    int heldBy = -1;   // -1: holstered (on the body)
    uint32_t hands{};  // Version 0.0.7 CAPTAIN AMERICA: the hands (1 << hand) that may draw it; 0: handsFor's
    // Version 0.0.73 SHIELD STAYS ON THE ARM (fix 2): a sticky slot's item stays in the hand when the grip
    // opens (and comes back to that hand after a throw); it goes back on the body only when the hand
    // reaches into its zone and presses the grip again.
    bool sticky{};
};
struct Hand {
    Vec3 position{};
    bool tracked{};
    float squeeze{};
    bool busy{}; // a web attached, climbing, the hologram's or the phone's: no new draw
    bool keep{}; // its item is out of the hand (a thrown baton): a release waits until it is back
};
// near (test build 12): an empty hand came into a slot's zone (a soft haptic pulse: "you can draw here").
enum Event : uint32_t { none = 0, drew = 1, holstered = 2, returnedAway = 4, near = 8 };
class Rack {
  public:
    Slot slots[maxSlots]{};
    uint32_t count{};
    bool crossDraw{};
    BodyYaw body{};
    uint64_t draws{}, holsters{}, returns{}, refusedSide{}, refusedBusy{};

    uint32_t add(uint32_t zone, uint32_t item, uint32_t hands = 0, bool sticky = false) {
        if (count >= maxSlots)
            return maxSlots;
        slots[count] = Slot{zone, item, -1, hands & 3u, sticky};
        return count++;
    }
    // The slot hand i holds (-1 none).
    int heldIn(unsigned hand) const {
        for (uint32_t s = 0; s < count; ++s)
            if (slots[s].heldBy == static_cast<int>(hand))
                return static_cast<int>(s);
        return -1;
    }
    // The hands whose grip is the rack's this frame (bit 1 << hand): zero those grips for web/climb.
    uint32_t claimed() const { return claimed_; }
    // This frame's events for hand i (Event bits) and the slot they were about.
    uint32_t events(unsigned hand) const { return hand < 2 ? events_[hand] : 0; }
    int eventSlot(unsigned hand) const { return hand < 2 ? eventSlot_[hand] : -1; }
    // Holster everything at once (a hero change, VR closing): no events.
    void stowAll() {
        for (uint32_t s = 0; s < count; ++s)
            slots[s].heldBy = -1;
        claimed_ = 0;
        down_[0] = down_[1] = false;
    }
    void update(float dt, bool play, const Pose& head, const Hand hands[2]) {
        const float t = std::isfinite(dt) ? std::clamp(dt, 0.f, .1f) : 0.f;
        body.update(t, head.orientation.rotate(Vec3{0, 0, -1}));
        claimed_ = 0;
        for (unsigned i = 0; i < 2; ++i) {
            events_[i] = none;
            eventSlot_[i] = -1;
            const Hand& h = hands[i];
            if (h.tracked && finite(h.position) && body.valid) {
                point_[i] = bodyPoint(h.position, head.position, body);
                pointAge_[i] = 0;
            } else {
                pointAge_[i] += t;
            }
            const bool pointOk = body.valid && pointAge_[i] <= lostSeconds;
            const float sq = std::isfinite(h.squeeze) ? h.squeeze : 0.f;
            const bool press = !down_[i] && sq > pressed;
            const bool release = down_[i] && sq < released;
            if (press)
                down_[i] = true;
            if (release)
                down_[i] = false;
            const int held = heldIn(i);
            // Test build 12: a soft pulse as an empty hand comes into a holstered slot's zone.
            bool nearNow = false;
            for (uint32_t s = 0; held < 0 && pointOk && s < count; ++s)
                nearNow |= slots[s].heldBy < 0 && inZone(slots[s].zone, point_[i]) &&
                           ((slots[s].hands ? slots[s].hands : handsFor(slots[s].zone, crossDraw)) & (1u << i));
            if (nearNow && !near_[i] && play && !h.busy)
                events_[i] |= near;
            near_[i] = nearNow;
            if (held >= 0) {
                // A released grip lets go now, or - while the item is out of the hand (thrown) - as
                // soon as it is back: never a second copy on the body while one is in the air.
                Slot& sh = slots[held];
                if (sh.sticky) {
                    if (press && !h.keep && pointOk && inZone(sh.zone, point_[i])) {
                        sh.heldBy = -1;
                        eventSlot_[i] = held;
                        events_[i] = holstered;
                        ++holsters;
                        claimed_ |= 1u << i;
                    } else if (down_[i]) {
                        claimed_ |= 1u << i;
                    }
                    continue;
                }
                if (!down_[i] && !h.keep) {
                    Slot& s = slots[held];
                    s.heldBy = -1;
                    eventSlot_[i] = held;
                    if (pointOk && inZone(s.zone, point_[i])) {
                        events_[i] = holstered;
                        ++holsters;
                    } else {
                        events_[i] = returnedAway;
                        ++returns;
                    }
                } else if (down_[i]) {
                    claimed_ |= 1u << i;
                }
                continue;
            }
            if (!press || !play || !pointOk)
                continue;
            int pick = -1;
            bool sideRefused = false;
            for (uint32_t s = 0; s < count; ++s) {
                if (slots[s].heldBy >= 0 || !inZone(slots[s].zone, point_[i]))
                    continue;
                if (!((slots[s].hands ? slots[s].hands : handsFor(slots[s].zone, crossDraw)) & (1u << i))) {
                    sideRefused = true;
                    continue;
                }
                pick = static_cast<int>(s);
                break;
            }
            if (pick < 0) {
                refusedSide += sideRefused;
                continue; // not the rack's: the grip is the web's or the climb's as before
            }
            if (h.busy) {
                ++refusedBusy;
                continue;
            }
            slots[pick].heldBy = static_cast<int>(i);
            events_[i] = drew;
            near_[i] = false;
            eventSlot_[i] = pick;
            claimed_ |= 1u << i;
            ++draws;
        }
    }

  private:
    BodyPoint point_[2]{};
    float pointAge_[2]{1e9f, 1e9f};
    bool down_[2]{}, near_[2]{};
    uint32_t claimed_{}, events_[2]{};
    int eventSlot_[2]{-1, -1};
};
// The haptic tick for an event (strength 0-1; 0: none): a firm one on a draw, a lighter one on a
// holster, none for a return from away.
inline float tickFor(uint32_t event) { return event & drew ? .45f : event & holstered ? .3f : event & near ? .15f : 0.f; }

} // namespace spidy::holster
