#pragma once
// Version 0.0.73 test 3 DAREDEVIL BATON SEAT + TIP (the owner after test 3: "daredevils batons still look
// weird when held ... the grapple shoots from the wrong end ... the silver part should separate while the
// baton stays in your hand with a rope connecting them"). Pure; no game memory, no address.
//
// SEAT. 0.0.7's shaft (baton_grip::localShaft) ran offForearmDeg past the forearm, from under the heel of the
// hand out over the index knuckle: the line of a hammer held by its end. The fist the suit draws (suit_wear's
// estimate and curlFinger) is not that fist: its fingers curl about a tube through the GRIP point (the
// knuckles sit R toward the back of the hand and the fingers bend about a centre R back toward the palm),
// whose axis is square to the drawn forearm in the grip's Y-Z plane - (0, sin t, -cos t) for the controller's
// handle tilt t, little finger toward the thumb. Test 3's shaft was ~70 degrees off that tube, so the red
// handle came out through the top of the hand at the thumb and index knuckle and the fingers closed on air.
// The seated shaft runs along the tube, through the grip point, the butt buttBehind below the little
// finger: the handle lies in the palm inside the curled fingers (the model is 3.6 cm across, the drawn
// fingers' inside ~4 cm; apps/dd_baton_preview.cpp measures it on the DAREDEVIL suits). Only the grip's Y-Z
// plane is used, so the left hand holds the mirror image of the right, as before.
//
// TIP. The silver end (the Yanez model's last tipShare of its length, +z; the 0.0.7 CLUB's steel end) is the
// grapple's origin (the trigger's aim runs along the shaft, butt -> tip: grapple073::batonAim). While the
// grapple pulls, it leaves the baton and flies to the anchor on a rope from the baton's front (TipFlight:
// out over flyTime, on at the anchor while it pulls, reeled back over reelTime when it ends, snapped on).
// The body and the tip are the same model's own triangles split at the seam (no flattening, nothing
// stretched); the club draws the same two parts. The throw (grip let go mid-swing) and the trigger's
// at-once grapple (c40b091) are untouched.
//
// CONFIRMED: Touch's 30 degree handle tilt (baton_grip). CANDIDATE: Index's 15; buttBehind and the palm
// clearance (measured on the suit's drawn fist, not on a headset yet); the body IK fallback hands' fist.
#include "spidy/baton_grip.hpp"
#include "spidy/math.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace spidy::dd_baton073 {

inline constexpr float batonLength = baton_grip::length; // .57 m butt to tip
inline constexpr float buttBehind = .095f;          // m: the butt this far below the tube's centre (the grip)
inline constexpr float tipShare = .26f;             // the silver end: the model's last 26% (z -0.08 .. 0.068 of .57)
inline constexpr float flyTime = .12f;              // s, as hero007::grappleFlight: the tip reaches the anchor
inline constexpr float reelTime = .25f;             // s back to the baton when the grapple ends
inline constexpr float seatedBelow = .002f;         // the flight fraction under which the tip is on the baton

inline bool fine(Vec3 v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }

// The drawn fist's tube (little finger -> thumb), grip-local, for a handle tilt in degrees.
inline Vec3 localTube(float tiltDeg) {
    const float a = tiltDeg * 3.14159265f / 180.f;
    return {0, std::sin(a), -std::cos(a)};
}
inline Vec3 localTube(ControllerKind kind) { return localTube(baton_grip::handleTiltDeg(kind)); }

// The held baton seated in the fist (tracking space in and out). Same contract as baton_grip::shaft: ok false
// on a pose with no usable rotation or position (the caller keeps its fallback). hand: 0 left, 1 right (the
// same transform; the grip frame mirrors itself).
inline baton_grip::Shaft seated(const Pose& grip, unsigned hand, ControllerKind kind, float len = batonLength) {
    (void)hand;
    baton_grip::Shaft s{};
    const Quat& q = grip.orientation;
    const float n = q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w;
    if (!fine(grip.position) || !std::isfinite(n) || std::abs(n - 1.f) > .01f || !std::isfinite(len) || len < .2f)
        return s;
    const Vec3 way = normalized(q.rotate(localTube(kind)));
    if (!fine(way))
        return s;
    s.butt = grip.position - way * buttBehind;
    s.tip = grip.position + way * (len - buttBehind);
    s.ok = fine(s.butt) && fine(s.tip);
    return s;
}
// on: the 0.0.73 seat; off: 0.0.7's hammer line (baton_grip::shaft).
inline baton_grip::Shaft held(bool on, const Pose& grip, unsigned hand, ControllerKind kind) {
    return on ? seated(grip, hand, kind) : baton_grip::shaft(grip, hand, kind);
}

// 0.0.73 test 5 GRAPPLE AIM (the owner after test 4: "when you try to grapple well the silver part is straight up so
// you have to twist your arm weird to aim"): while this hand's grapple aims (a finger on the trigger), fires or
// reels, the held baton turns about the grip point until its silver tip points along the controller's aim, eased
// in over aimIn and back to the fighting grip (seated) over aimOut. The grapple's ray leaves the turned tip along
// the aim. CANDIDATE: aimTouch (Touch's analog trigger reads 0 untouched; the trigger's first tenth = aiming).
inline constexpr float aimIn = .12f, aimOut = .25f, aimTouch = .1f;
inline bool aimWanted(bool on, bool held, float trigger, bool firing, bool tipApart) {
    return on && held && (firing || tipApart || (std::isfinite(trigger) && trigger >= aimTouch));
}
inline float aimEase(float now, bool want, float dt) {
    const float d = std::isfinite(dt) ? std::clamp(dt, 0.f, .1f) : 0.f;
    const float n = std::isfinite(now) ? std::clamp(now, 0.f, 1.f) : 0.f;
    return want ? std::min(1.f, n + d / aimIn) : std::max(0.f, n - d / aimOut);
}
// The seated shaft turned toward the aim by w (0: unchanged; 1: butt -> tip along aim). Smoothstepped; a nonsense
// input: s unchanged. 0.0.73 test 5 AIM HANDLE (the handle clipped out of the fist turned about the grip point): the
// turn's pivot slides up the seated handle to aimPivot above the grip (the fist's top, by the thumb and index) and the
// baton slides through it until only aimHold of its butt is behind that pivot - held at its end like a pointer, the
// handle out of the front of the fist rather than across it. CANDIDATE: aimPivot, aimHold (dd_baton_preview's poke,
// DAREDEVIL 1 and 3, aims level, up and across: at most 22 fist points through the handle, was up to 269).
inline constexpr float aimPivot = .05f, aimHold = .012f;
inline baton_grip::Shaft aimTurn(const baton_grip::Shaft& s, Vec3 grip, Vec3 aim, float w, float pivot = aimPivot,
                                 float hold = aimHold) {
    if (!s.ok || !fine(grip) || !fine(aim) || length(aim) < .5f || !std::isfinite(w) || !(w > 0.f))
        return s;
    const float k = std::clamp(w, 0.f, 1.f), e = k * k * (3.f - 2.f * k);
    const Vec3 was = s.tip - s.butt;
    const float len = length(was);
    if (!(len > .2f))
        return s;
    const Vec3 a = normalized(aim), h = was * (1.f / len);
    Vec3 way = h * (1.f - e) + a * e;
    if (!(length(way) > .1f)) // exactly opposed: through the grip's side rather than a flip through zero
        way = normalized(cross(h, Vec3{0, 1, 0})) * (1.f - e) + a * e;
    way = normalized(way);
    if (!fine(way) || !(length(way) > .5f))
        return s;
    const float behind = std::clamp(length(grip - s.butt), 0.f, len);
    const float up = std::isfinite(pivot) ? std::clamp(pivot, 0.f, len - behind) : 0.f;
    const float keep = std::isfinite(hold) ? std::clamp(hold, 0.f, behind) : behind;
    const Vec3 at = grip + h * (up * e);
    const float back = behind * (1.f - e) + keep * e;
    baton_grip::Shaft t = s;
    t.butt = at - way * back;
    t.tip = t.butt + way * len;
    t.ok = fine(t.butt) && fine(t.tip);
    return t.ok ? t : s;
}

// The seam between the body and the silver tip (on the shaft, tipShare from the tip end).
inline Vec3 seam(Vec3 butt, Vec3 tip) { return tip + (butt - tip) * tipShare; }

// The tip's flight, per hand: 0 on the baton, 1 at the anchor.
class TipFlight {
  public:
    enum Phase : uint32_t { seatedOn = 0, out, on, reel };
    // grappling: this hand's grapple pulls toward anchor (world) this frame. dt in seconds.
    void step(bool grappling, Vec3 anchor, float dt) {
        const float d = std::isfinite(dt) ? std::clamp(dt, 0.f, .1f) : 0.f;
        if (grappling && fine(anchor)) {
            anchor_ = anchor;
            if (phase_ == seatedOn || phase_ == reel)
                phase_ = out;
            if (phase_ == out) {
                f_ = std::min(1.f, f_ + d / flyTime);
                if (f_ >= 1.f)
                    phase_ = on;
            }
            return;
        }
        if (phase_ == out || phase_ == on) {
            phase_ = reel;
            ++reels_;
        }
        if (phase_ == reel) {
            f_ = std::max(0.f, f_ - d / reelTime);
            if (f_ <= 0.f) {
                phase_ = seatedOn;
                ++snaps_;
            }
        }
    }
    Phase phase() const { return phase_; }
    float fraction() const { return f_; }
    Vec3 anchor() const { return anchor_; }
    bool apart() const { return phase_ != seatedOn && f_ > seatedBelow && fine(anchor_); }
    uint64_t snaps() const { return snaps_; }
    uint64_t reels() const { return reels_; }
    void reset() { *this = TipFlight{}; }

  private:
    Phase phase_ = seatedOn;
    float f_{};
    Vec3 anchor_{};
    uint64_t snaps_{}, reels_{};
};

// Where the flying tip is drawn: its seam end (back) and its point (front), the rope from the baton's seam to
// back. f 0: on the baton (back = the seam, front = the baton's tip); 1: its point on the anchor.
struct TipPlace {
    Vec3 rope{}, back{}, front{};
    bool apart{}, ok{};
};
inline TipPlace place(Vec3 butt, Vec3 tip, Vec3 anchor, float f) {
    TipPlace p{};
    if (!fine(butt) || !fine(tip) || length(tip - butt) < .05f)
        return p;
    const Vec3 s = seam(butt, tip);
    const float tipLen = length(tip - s);
    p.rope = s;
    p.back = s;
    p.front = tip;
    p.ok = true;
    const float k = std::isfinite(f) ? std::clamp(f, 0.f, 1.f) : 0.f;
    if (!(k > seatedBelow) || !fine(anchor))
        return p;
    const Vec3 toAnchor = anchor - s;
    const float dist = length(toAnchor);
    if (!(dist > tipLen + .05f) || dist > 80.f)
        return p; // at (or past) the anchor already, or nonsense: on the baton
    const Vec3 way = toAnchor * (1.f / dist);
    // Eased out (fast then settling) so the rope reads as thrown; linear back.
    const Vec3 restFront = tip;
    const Vec3 hitFront = anchor;
    const Vec3 front = restFront + (hitFront - restFront) * k;
    // The tip turns from the baton's line to the rope's as it leaves.
    const Vec3 bodyWay = normalized(tip - s);
    Vec3 lineWay = normalized(bodyWay * (1.f - k) + way * k);
    if (!fine(lineWay) || length(lineWay) < .5f)
        lineWay = way;
    p.front = front;
    p.back = front - lineWay * tipLen;
    p.apart = length(p.back - s) > .01f;
    p.ok = fine(p.front) && fine(p.back);
    return p;
}

} // namespace spidy::dd_baton073
