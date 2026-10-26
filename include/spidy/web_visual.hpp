#pragma once
#include "slow_motion.hpp"
#include "spider_sense.hpp"
#include "training_guide.hpp"
#include "ui_style.hpp"
#include "vertex.hpp"
#include "web_strike.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace spidy {
// One web as drawn over the eye images. Positions are world metres.
struct WebLine {
    Vec3 start{}, end{}; // wrist and anchor
    float slack{};       // rope length beyond the straight distance; 0 when taut
    float extended = 1;  // 0..1 of the way from the wrist while the web is shot
    bool splat = true;   // impact web at the anchor once the shot arrives
    float seed{};        // varies splat strands between webs
    // Update 15: the shot in flight. wave: metres the rope ripples across its
    // line (0 straight), in the shape wavePhase moves along; impact: 0..1 of
    // the splat's spread since the tip arrived (1 settled).
    float wave{}, wavePhase{}, impact = 1;
    // Update 37: the rope's bend at its middle (metres, across the chord; zero straight),
    // from RopeBend: it trails the hand's motion in flight and bows when slack or turning.
    Vec3 bow{};
};
struct WebLook {
    // Update 13: the rope's half width, the same the whole way along (no taper at the hand or the
    // anchor): 1.3 cm across, under a finger's width beside the suit's hand (live.txt web_width).
    // The coil rings bulge 30% past it (web_shading.hpp tubeBulge).
    float radius = .0065f;
    static constexpr float minRadius = .002f, maxRadius = .02f; // web_width 0.4-4 cm
    // On-screen width floor, so distant webs stay continuous. It applies only where the true
    // width is under it (about 7 m away and further at the headset's pixels), never nearer.
    float minPixels = 1.6f;
    // The rope's lit tone (Update 11: the 1994 cartoon's cream #E8E2D0 in
    // linear light; web_shading.hpp steps it to its mid and shadow tones and
    // inks it per pixel) and the splat's darker edge.
    Vec3 core{.807f, .763f, .631f}, edge{.3f, .34f, .42f};
    // The sky's light, for the strand's round shading.
    Vec3 light{.24f, .95f, .2f};
    // A strand widened by the pixel floor is drawn at its true width's share
    // of opacity, never below `farOpacity`.
    float farOpacity = .4f;
    // Update 18: the rope's look (live.txt web_style; web_style below), and
    // its shade from that style's own list (web_color, kept per style:
    // web_style::shadeName). Only the picture: width, speed, the travel and
    // the far-web floor are the same for all. `seconds` is a clock for
    // MESSY's glints, SYMBIOTE's slow writhe and ENERGY's glow.
    int style = 0, color = 0;
    float seconds = 0;
    // Update 38 WEB ROPE (live.txt web_rope 0, the default in game): a solid thick rope with
    // the RopeBend bow - no loose loops or wisps (run372's video: "a flat strip of white
    // circles"), and MESSY with no glow band and opaque. false: the 37.2 fibre look.
    bool rope = false;
};
// Update 18: the web styles, in the panel's order (wrist_hud WEB STYLE).
namespace web_style {
enum : int { nineties, messy, modern, symbiote, energy };
inline constexpr int count = 5;
// What the panel shows ("90s", "MESSY", ...) and live.txt's word ("90s", "messy", ...).
const char* name(int style);
const char* key(int style);
// WEB COLOR: each style's own shades, the first its default. ENERGY's are
// real colours (BLUE, RED, GREEN, ORANGE, PURPLE, GOLD); every other
// style's are subtle shades of its own look. No two styles share a shade's
// name or a near colour (tests: swatch). Out of range: the default.
inline constexpr int maxShades = 6;
int shades(int style);
const char* shadeName(int style, int shade); // "CREAM", "WARM GREY", ...
const char* shadeKey(int style, int shade);  // live.txt's "cream", "warm_grey", ...
// A shade's colours (linear RGB): the rope's (SYMBIOTE: its sheen, the
// rope itself near black; ENERGY: its core) and the splat's edge.
struct Shade {
    Vec3 color, edge;
};
Shade shade(int style, int shade);
// ENERGY's: a saturated core, a lighter rim, a deeper glow round it; every
// one burns white-hot at its centre.
struct EnergyPalette {
    Vec3 core, rim, glow;
};
EnergyPalette energyPalette(int shade);
// How a shade looks on its rope: the shading of a lit tube's facing side,
// averaged across it (90s: its lit cel tone), linear RGB.
Vec3 swatch(int style, int shade);
// The most vertices one web may take in any style (a near, wide, slack
// web with its splat stays well under it); a styled web over it, or with
// anything not finite, is drawn as 90s instead.
inline constexpr size_t vertexBudget = 24000;
// MESSY draws its separate fibres only where the whole rope is at least
// this many pixels across; further off it is one thin strand.
inline constexpr float fibrePixels = 6;
// Whether out[from, end) is fit to draw: finite and within the budget.
bool sane(const std::vector<Vertex>& out, size_t from);
} // namespace web_style
// A round rope of one width, cel-shaded and inked per pixel with coils that crowd
// the wrist and spread along the line (web_shading.hpp), fainter where the pixel floor widens it, parabolic sag when
// slack, a travelling tip during the shot, and a splat at the anchor.
// pixelAngle is the eye's angular size of one pixel, in radians.
void appendWeb(std::vector<Vertex>& out, const WebLine& web, Vec3 viewer, float pixelAngle,
               const WebLook& look = {});
// Update 38 BALL LOOK: our own web ball (combat38::Glob) - a glowing white glob at `ball`
// with a short fading strand back to `tail`, a soft halo round it, and a few fibres, turned
// to the eye; at least a few pixels across at any distance. `fade` 0..1.
void appendWebBall(std::vector<Vertex>& out, Vec3 ball, Vec3 tail, Vec3 viewer, float pixelAngle, float fade);
// Version 0.0.3 WEB BALL HIT: one thin white web thread a-b (the webbed-up look's strands), turned
// to the eye, at least a pixel wide; cheap (12 vertices) unlike appendWeb's full rope.
void appendWebThread(std::vector<Vertex>& out, Vec3 a, Vec3 b, Vec3 viewer, float pixelAngle, float fade);
// Version 0.0.2 BALL LOOK OURS: the web splash where a ball hit (fade 1 fresh, 0 gone).
void appendWebSplash(std::vector<Vertex>& out, Vec3 at, Vec3 viewer, float pixelAngle, float fade, float seed);
// Version 0.0.4 IRON MAN ABILITIES (REPULSOR) (repulsor.hpp): a glowing blue-white energy bolt at `bolt` with
// its streak back to `tail`; its flash (a hit, or the palm as it fires; `size` its radius in
// metres); the chest beam from `chest` to `end`. Turned to the eye, a few pixels at least; fade 0..1.
void appendRepulsorBolt(std::vector<Vertex>& out, Vec3 bolt, Vec3 tail, Vec3 viewer, float pixelAngle, float fade);
void appendRepulsorFlash(std::vector<Vertex>& out, Vec3 at, Vec3 viewer, float pixelAngle, float fade, float size);
void appendChestBeam(std::vector<Vertex>& out, Vec3 chest, Vec3 end, Vec3 viewer, float pixelAngle, float fade);
// Version 0.0.6 DAREDEVIL ABILITIES: a billy club from `grip` to `tip` (dark red, a black grip wrap on
// the grip's third, a light streak), turned to the eye, at least a pixel and a half wide.
void appendBaton(std::vector<Vertex>& out, Vec3 grip, Vec3 tip, Vec3 viewer, float pixelAngle);
// Version 0.0.5 BEAM RING: the chest beam's cooldown on the back of the right wrist (hand: the
// right grip), like a watch face: an orange arc filling clockwise from the top over 30 s (share
// 0..1), the whole ring blue-white when ready; flash 1..0 just ready: brighter, a ring swelling
// out. Nothing when share or anything else is not finite or it is nearer than 8 cm or past 3 m.
void appendBeamRing(std::vector<Vertex>& out, float share, float flash, Pose hand, Vec3 viewer, float pixelAngle,
                    float seconds);
// Version 0.0.62 HAND COOLDOWN: a hand's 2 s repulsor cooldown on its own wrist, 4.5 cm behind the beam
// ring's place (clear of it on the right wrist): a small cyan arc filling clockwise (share 0..1), a blink
// when ready (flash 1..0); nothing when ready and not flashing, or nearer than 8 cm or past 3 m.
void appendBlastRing(std::vector<Vertex>& out, float share, float flash, Pose hand, Vec3 viewer, float pixelAngle);
// Version 0.0.5 SUPERSONIC's shockwave, `age` seconds after the boom (nothing outside 0..0.9 s):
// a ring sweeping from 3 m ahead past the eye, widening as it fades, and a faint vapour cone
// along the flight's `way` (unit), both around the eye.
inline constexpr float shockwaveSeconds = .9f;
void appendShockwave(std::vector<Vertex>& out, Vec3 eye, Vec3 way, float age, float pixelAngle);
// Version 0.0.2 TRAINING's guide: the glowing path ribbon (RED, BLUE as the hand follows,
// GREEN at SUCCESS, dim orange at FAIL), its start and end markers, the arrows along it and
// the ghost hand riding it; `travel` moves it with the player to this image. Nothing drawn
// when it is off or anything is not finite; a few thousand vertices at most.
void appendTrainingGuide(std::vector<Vertex>& out, const training_guide::Draw& guide, Vec3 travel, Vec3 viewer,
                         float pixelAngle);
// Update 13: where a held web is drawn from. Webs are shot from the web shooter
// (tracking.hpp's webShooter, on the wrist behind the grip), but the
// overlay is drawn over the hand, so a rope from there lay across the palm
// and looked to start behind the hand. It is drawn from the point on its line
// beside the grip (the palm) instead, if that is ahead of the shooter (at
// most 15 cm on) and leaves 5 cm of rope; otherwise from the shooter.
// Update 20: at most webFromGripAhead (2 cm) on. The shooter is at the heel
// of the palm now, off the fist's line, and the point beside the grip was
// the middle of the fist: the October 23 player saw webs leave "through the
// closed fist between the fingers".
inline constexpr float webFromGripAhead = .02f;
inline Vec3 webFromGrip(Vec3 shooter, Vec3 grip, Vec3 toward) {
    const Vec3 line = toward - shooter;
    const float reach = length(line);
    if (!(reach > 1e-4f) || !std::isfinite(reach))
        return shooter;
    const Vec3 along = line * (1 / reach);
    const float t = std::clamp(dot(grip - shooter, along), 0.f, webFromGripAhead);
    return reach - t >= .05f ? shooter + along * t : shooter;
}
// Update 15: how the overlay shows a web being shot (live.txt web_travel,
// web_speed). Only the picture: the swing has its web from the moment it
// attaches, as always, so no player waits on it or falls for it.
struct WebTravel {
    bool travel = true;  // 0: the web drawn whole at once, as before Update 15
    float speed = 180;   // m/s the tip flies
    static constexpr float minSpeed = 20, maxSpeed = 2000;
    // Update 37: strand physics (live.txt web_physics, web_wiggle, web_bend). Off: the
    // Update 15 picture (plan B). On: a flight long enough to see (up to physicsFlight),
    // a wiggle while it flies and a sprung bend once attached, wiggle and bend in percent.
    // Update 37.2 (SM1 feel): about 0.15 s from the palm to the anchor (physicsFlight), an
    // erratic wiggle in flight, a hard snap taut on attach; bend 100 is the medium,
    // tension-driven bend (0 straight, 300 the most).
    bool physics = true;
    float wiggle = 100, bend = 100;
    static constexpr float physicsFlight = .15f, maxWiggle = 300, maxBend = 300;
};
// Update 37: an attached or flying rope's bend, one sprung point at its middle (cheap: a
// few multiplies per web per image). It trails the hand's motion across the rope (the
// player's own swing included, as the hand is drawn where the player is), sags a little
// when slack, and springs back stiffer the tauter the web is.
struct RopeBend {
    Vec3 offset{}, velocity{};
    static constexpr float lagSeconds = .06f, flyingLag = .10f, maxShare = .08f;
    void reset() { offset = velocity = {}; }
    Vec3 update(float seconds, Vec3 chord, Vec3 handVelocity, float tension, float bendPercent, bool flying) {
        const float dt = std::isfinite(seconds) ? std::clamp(seconds, 0.f, .05f) : 0.f;
        const float distance = length(chord);
        const float bend = std::isfinite(bendPercent) ? std::clamp(bendPercent, 0.f, WebTravel::maxBend) / 100 : 0.f;
        if (!(distance > .2f) || !std::isfinite(distance) || !finite(handVelocity) || bend <= 0) {
            reset();
            return {};
        }
        const Vec3 along = chord / distance;
        const float taut = std::isfinite(tension) ? std::clamp(tension, 0.f, 1.f) : 0.f;
        Vec3 across = handVelocity - along * dot(handVelocity, along);
        const float reach = std::min(1.f, distance / 8);
        Vec3 target = across * (-(flying ? flyingLag : lagSeconds) * (1 - .8f * taut) * reach * bend);
        Vec3 down{0, -1, 0};
        down = down - along * dot(down, along);
        target = target + down * (.012f * distance * (1 - taut) * bend);
        const float most = maxShare * distance * std::max(bend, .01f);
        if (const float l = length(target); l > most)
            target = target * (most / l);
        const float k = 90.f * (1 + 3 * taut), damping = 1.1f * std::sqrt(k);
        velocity = velocity + (target - offset) * (k * dt) - velocity * std::min(damping * dt, 1.f);
        offset = offset + velocity * dt;
        offset = offset - along * dot(offset, along);
        if (!finite(offset) || length(offset) > 2 * most) {
            reset();
            return {};
        }
        return offset;
    }
};
// Shot and release timing shared by the headset overlay and the lab.
struct WebAnimation {
    static constexpr float shotSeconds = .09f, releaseSeconds = .22f;
    // Update 15: the tip's flight lasts distance / speed, at most maxFlight
    // even for a long shot, and at least minFlight or minFrames images,
    // whichever is longer, so even a short shot is seen to fly. Eased out:
    // fast from the palm, slowing into the anchor. Then the rope snaps taut
    // with a small twang and the splat spreads over settleSeconds.
    static constexpr float minFlight = .03f, maxFlight = .11f, minFrames = 2.5f, settleSeconds = .08f,
                           twangSeconds = .12f;
    static constexpr float pi = 3.14159265f;
    static float flightSeconds(float distance, float speed, float frameSeconds, float longest = maxFlight) {
        const float v = std::isfinite(speed) ? std::clamp(speed, WebTravel::minSpeed, WebTravel::maxSpeed) : 180.f;
        const float frame = std::isfinite(frameSeconds) ? std::clamp(frameSeconds, 0.f, .05f) : 0.f;
        const float shortest = std::clamp(minFrames * frame, minFlight, longest);
        const float d = std::isfinite(distance) ? std::max(distance, 0.f) : 0.f;
        return std::clamp(d / v, shortest, longest);
    }
    struct Flight {
        float extended = 1, wave = 0, wavePhase = 0, impact = 1;
        bool flying = false;
    };
    // The shot `since` seconds after the web attached, `distance` metres
    // from the palm to the anchor; frameSeconds: the overlay's image interval.
    static Flight flight(float since, float distance, const WebTravel& shot, float frameSeconds) {
        Flight f;
        if (!shot.travel || !std::isfinite(since) || !std::isfinite(distance) || distance <= 0)
            return f;
        const float longest = shot.physics ? WebTravel::physicsFlight : maxFlight;
        const float t = std::max(since, 0.f), duration = flightSeconds(distance, shot.speed, frameSeconds, longest);
        const float wiggle = shot.physics && std::isfinite(shot.wiggle)
                                 ? std::clamp(shot.wiggle, 0.f, WebTravel::maxWiggle) / 100 : 1.f;
        const float ripple = std::clamp(distance * .012f, .01f, .3f) * (shot.physics ? 1.6f * wiggle : 1.f);
        if (t < duration) {
            const float u = t / duration, eased = 1 - (1 - u) * (1 - u);
            f.flying = true;
            // The image the web attached on shows its tip leaving the palm.
            f.extended = std::max(eased, std::min(.06f / distance, .5f));
            // The rope ripples by about 2.5% of its paid-out length, dying as it lands.
            f.wave = std::min(ripple, (shot.physics ? .04f * wiggle : .025f) * f.extended * distance) * (1 - eased);
            f.wavePhase = u * (shot.physics ? 5 : 3) * pi;
            if (shot.physics) {
                // Update 37.2 (SM1): erratic, not a clean sine - the size and the travel of the
                // ripple jitter (two unrelated rates, the same for both eyes: from `u` alone).
                f.wave *= .7f + .3f * std::sin(u * 23.f + 1.3f) * std::sin(u * 9.f + .4f) + .2f * std::sin(u * 41.f);
                f.wavePhase += 1.1f * std::sin(u * 13.f + .7f);
            }
            f.impact = 0;
            return f;
        }
        const float after = t - duration;
        f.impact = std::clamp(after / settleSeconds, 0.f, 1.f);
        f.impact = 1 - (1 - f.impact) * (1 - f.impact);
        // Update 37.2 (SM1): it snaps taut - a short, stiff twang that is gone in a few frames.
        if (after < twangSeconds)
            f.wave = ripple * (shot.physics ? .1f : .15f) * std::exp(-after / (shot.physics ? .018f : .03f)) *
                     std::cos(after * 2 * pi / .045f);
        return f;
    }
    // Portion of the web extended this long after attachment.
    static float extended(float sinceAttach) {
        return std::isfinite(sinceAttach) ? std::clamp(sinceAttach / shotSeconds, 0.f, 1.f) : 1.f;
    }
    // After release the free end whips back toward the anchor and sags.
    static bool released(float sinceRelease, Vec3 hand, Vec3 anchor, WebLine& out) {
        if (!std::isfinite(sinceRelease) || sinceRelease < 0 || sinceRelease >= releaseSeconds)
            return false;
        const float t = sinceRelease / releaseSeconds, pull = t * t * (3 - 2 * t);
        out.start = hand + (anchor - hand) * pull;
        out.end = anchor;
        out.slack = length(anchor - hand) * .35f * (1 - pull);
        out.extended = 1;
        return length(out.end - out.start) > .05f;
    }
};
// When each hand's web was shot or let go, in predicted display time (ns).
class WebTimeline {
  public:
    struct Hand {
        int64_t attachedAt{}, releasedAt{};
        Vec3 anchor{}, wrist{};
        bool attached{};
    };
    void update(unsigned hand, bool attached, Vec3 anchor, Vec3 wrist, int64_t now) {
        auto& h = hands_.at(hand);
        if (attached && !h.attached)
            h.attachedAt = now;
        if (!attached && h.attached)
            h.releasedAt = now;
        if (attached) {
            h.anchor = anchor;
            h.wrist = wrist;
        }
        h.attached = attached;
    }
    const Hand& operator[](unsigned hand) const {
        return hands_.at(hand);
    }
    void clear() {
        hands_ = {};
    }

  private:
    std::array<Hand, 2> hands_{};
};
// Which of the game's two swing ropes draws which hand's web, and from which
// wrist. The game makes a rope with the sound of a web shot, so a rope is
// made for a web that was shot and never for a hand that takes hold of the
// other hand's web (Swing::shared): that web's rope goes on drawing it, from
// the wrist of the hand that holds it nearer its end, and still when the hand
// that shot it has let go. The first build with a web in both hands gave the
// second hand a rope of its own: "grabbing the web with other arm still does
// the shooting sound". And it took the nearer hand by where the wrists were,
// so the web changed hands as they moved ("they switch grab position"): the
// nearer hand is the one with the shorter rope, which only a new hold or a
// reel changes.
class WebRopes {
  public:
    struct Hand {
        bool attached{}; // a web to draw
        bool taken{};    // it took hold of the other hand's web
        bool shared{};   // both hands hold one web now
        float length{};  // its rope to what the web hangs from
        int64_t since{}; // when it attached: one web
    };
    struct Rope {
        unsigned hands{}; // a bit per hand whose web it draws; 0: none
        int64_t key{};    // one web: another value is another rope (native_webs' attachedAt)
        int from = -1;    // the hand whose wrist it leaves
        int last = -1;    // the hand whose web it drew last (a web let go of trails from it)
    };
    void update(const std::array<Hand, 2>& hands) {
        // Hands that let go, or shot anew, leave their rope.
        for (auto& rope : ropes_)
            for (unsigned h = 0; h < 2; ++h)
                if ((rope.hands >> h & 1) && (!hands[h].attached || hands[h].since != since_[h]))
                    rope.hands &= ~(1u << h);
        for (unsigned h = 0; h < 2; ++h)
            since_[h] = hands[h].attached ? hands[h].since : 0;
        // A hand with a web and no rope: one that took hold of the other
        // hand's web is drawn by that web's rope; any other gets a rope of
        // its own, the one of its side first. Shot webs first, so the web
        // taken hold of has its rope by then.
        for (const bool takers : {false, true})
            for (unsigned h = 0; h < 2; ++h) {
                const auto& hand = hands[h];
                const bool taker = hand.taken && hand.shared && hands[1 - h].attached;
                if (!hand.attached || slot(h) >= 0 || taker != takers)
                    continue;
                if (const int held = taker ? slot(1 - h) : -1; held >= 0) {
                    ropes_[static_cast<size_t>(held)].hands |= 1u << h;
                    continue;
                }
                auto& rope = !ropes_[h].hands ? ropes_[h] : ropes_[1 - h];
                if (!rope.hands)
                    rope = {1u << h, hand.since, static_cast<int>(h), static_cast<int>(h)};
            }
        // Each rope leaves the wrist of the hand nearer the web's end.
        for (auto& rope : ropes_) {
            if (!rope.hands)
                rope.from = -1;
            else if (rope.hands != 3)
                rope.from = rope.last = rope.hands == 1 ? 0 : 1;
            else if (rope.from < 0 || hands[static_cast<size_t>(1 - rope.from)].length <
                                          hands[static_cast<size_t>(rope.from)].length - 1e-3f)
                rope.from = rope.last = hands[1].length < hands[0].length ? 1 : 0;
        }
    }
    const Rope& rope(unsigned slot) const {
        return ropes_.at(slot);
    }
    // The rope that draws this hand's web; -1: none.
    int slot(unsigned hand) const {
        for (int s = 0; s < 2; ++s)
            if (ropes_[static_cast<size_t>(s)].hands >> hand & 1)
                return s;
        return -1;
    }
    // One web in both hands: the hand that holds it nearer its end, where the
    // strand comes to first; -1 otherwise.
    int upper() const {
        for (const auto& rope : ropes_)
            if (rope.hands == 3)
                return rope.from;
        return -1;
    }
    void clear() {
        ropes_ = {};
        since_ = {};
    }

  private:
    std::array<Rope, 2> ropes_{};
    std::array<int64_t, 2> since_{};
};
// Aim markers: where a grip press would send a hand's web now, drawn over the
// eye images facing the viewer, the same size on screen at any distance, with
// soft edges and a soft dark shadow. Each hand has its own colour (Update 16):
// the left hand's markers Spider-Man red (#E23636), the right hand's blue
// (#2F6BFF), in linear RGB. A miss is the hand's colour at missBrightness.
inline constexpr Vec3 aimColors[2] = {{.7605f, .0369f, .0369f}, {.0284f, .147f, 1.f}};
inline constexpr float missBrightness = .55f;
enum class AimMark : std::uint8_t {
    anchor,  // the web attaches here: a ring around a dot
    air,     // nothing within reach: the web attaches in the air, a faint dashed ring
    blocked, // the web misses here: a cross in the hand's colour, dimmed
    target,  // the web catches what is here: a turning ring of three arcs around it
};
struct AimMarker {
    AimMark kind = AimMark::anchor;
    Vec3 point{};      // where the web goes; a target's centre
    float radius{};    // a target's radius, metres
    float squeeze{};   // the hand's grip, 0..1: the marker tightens as it closes
    unsigned hand{};   // 0 the left hand, 1 the right: the marker's colour
    float opacity = 1; // 0..1 while the marker fades in or out
    float spin{};      // radians the target's ring has turned
    float lock = 1;    // 0..1 as the target's ring closes in on it from wider
};
// pixelAngle as for appendWeb. Each mark below draws its tech_marks.hpp
// namesake instead in UiStyle::tech.
void appendAimMarker(std::vector<Vertex>& out, const AimMarker& marker, Vec3 viewer, float pixelAngle,
                     UiStyle style = UiStyle::comic);
// Steadies one hand's aim marker from eye image to eye image. Hand tremor and
// tracking noise swing the aim ray by a few tenths of a degree, which the
// marker shows as jitter at any distance; the aim previews arrive at the
// game's own rate. The ray's direction goes through a speed-adaptive low-pass
// (the "1 euro filter": steady while the hand holds still, a few tenths of a
// degree behind while it sweeps), in the tracking space so a snap turn is no
// motion; a marker eases along the ray to a new distance, a target's ring
// slides to a new target, and each kind of marker fades in and out instead of
// switching at once.
class AimMarkerMotion {
  public:
    static constexpr float minCutoffHz = 1, speedCutoff = 60, speedDeadband = .06f, speedCutoffHz = 4;
    static constexpr float fadeInSeconds = .04f, lockSeconds = .12f, fadeOutSeconds = .08f;
    static constexpr float distanceSeconds = .03f, targetSeconds = .035f, gapSeconds = .25f;
    // Starts the eye image shown at `timeNs` (predicted display time): the
    // same image again changes nothing, and after a gap of over gapSeconds
    // the markers start afresh.
    void begin(int64_t timeNs);
    // The steadied direction of the hand's aim ray for this image, from its
    // `direction` (world, unit) and the tracking space's yaw in the world.
    Vec3 aim(Vec3 direction, float yaw);
    // The same with the tracking space's whole orientation (trackingTurn): a
    // flip's tilt is no motion of the aim either.
    Vec3 aim(Vec3 direction, Quat turn);
    // Appends what to draw for this image: `wanted` (nullptr: no marker) on
    // the steadied ray from `origin` along `direction` (from aim()), easing
    // in, and the markers before it fading out where they were last wanted.
    void markers(const AimMarker* wanted, Vec3 origin, Vec3 direction, std::vector<AimMarker>& out);
    void reset();

  private:
    struct Shown {
        AimMarker marker; // the latest wanted marker of this kind
        float weight{};   // 0..1: its fade
        float lock{};     // 0..1: a target's ring closing in
        float distance{}; // along the ray (anchor, air, blocked)
    };
    std::array<Shown, 4> kinds_{};
    Vec3 steady_{}, speed_{}; // the filtered direction in the tracking space, and its rate
    bool filtering_{};
    int64_t time_{};
    float seconds_{}; // since the image before
    float spin_{};
};
// Where the marker for a surface the aim ray met at `point` (facing `normal`)
// goes on an aim ray from `origin` along unit `direction`: where that ray
// crosses the surface's plane, so the marker stays on the line the hand
// points along until the next preview arrives. `point` itself when the ray
// runs nearly along the plane or crosses it far from `point`.
Vec3 onAimLine(Vec3 point, Vec3 normal, Vec3 origin, Vec3 direction);
// Slow motion's focus meter (slow_motion.hpp) over the back of the left
// wrist, facing the viewer: a cyan ring around an hourglass that empties
// clockwise from the top as focus is spent and fills again as it refills. In
// slow motion it brightens and pulses; a refused press or an empty meter
// flashes it red. `hand` is the left hand's pose as the glove's (fingers
// along -z, the back of the hand +y); `seconds` any running clock, for the
// pulse. pixelAngle as for appendWeb.
void appendSlowMotionMeter(std::vector<Vertex>& out, const SlowMotionView& view, Pose hand, Vec3 viewer,
                           float pixelAngle, float seconds, UiStyle style = UiStyle::comic);
// The combat marks below are printed, as the graphics of Spidy's videos are
// (their style system: flat plates of colour, every shape keyed in ink, a
// hard drop where a shadow would be, lettering out of register as it lands),
// and they move on twos: a pose held a twelfth of a second, then the next,
// with nothing eased or faded.
//
// A landed blow where the fist met him (web_strike.hpp), facing the viewer:
// a comic's burst. A star, small and then whole, yellow around paper for a
// jab and red around yellow for a harder blow; then its lines thrown clear,
// a pose for a jab and two for a harder blow. A finisher's has a third,
// widest star and three poses of lines. Above it the blow's count in the
// flow, from the second on ("X3"): yellow lettering keyed in ink that pops
// in off level, holds, and is gone in two poses.
struct ImpactView {
    static constexpr float seconds = .75f; // until the count has gone too
    Vec3 point{};
    float strength{}; // 0..1
    BlowKind kind{};
    int count{};
    float age = -1; // seconds since it landed; below 0, or past `seconds`: nothing
    // A called blow (Cues): its star is out of register, a cyan ghost to its
    // left and a magenta one to its right.
    bool called{};
};
// pixelAngle as for appendWeb.
void appendImpact(std::vector<Vertex>& out, const ImpactView& impact, Vec3 viewer, float pixelAngle,
                  UiStyle style = UiStyle::comic);
// The mark over the chest of the enemy the player's next punch would go to
// (game_punch), facing the viewer, with what the flow calls (web_strike.hpp's
// Cues). Nothing called: a thin paper ring, a target and no more; once a
// blow has locked the player onto him (Lock) the ring is bolder, with four
// ticks in from it. A call: an arrow in a marker's stroke pointing the way
// the fist must travel, as the viewer sees him (left, right, up or down),
// coming in from the side the fist comes from and ending at his middle, so
// that "from there" and "that way" are one reading; paper on a cyan drop,
// over a cyan ring that runs down clockwise, a notch at a time, as the
// flow's time does. The call for the flow's finisher is
// larger: a yellow arrow on a red drop inside a burst's edge, a red zigzag
// that runs down the same way. A call pops in (three poses, the first two
// out of register) and its arrow then boils as a held drawing does; the mark
// keeps a readable size at a distance.
struct CueView {
    Vec3 point{};
    CueWay way{}; // None: the plain ring
    bool finisher{};
    float left{}; // 1..0 of the flow's time; 0: nothing at all
    float age{};  // seconds since it showed
    bool locked{}; // a blow holds the mark on him: it does not follow the look
};
// pixelAngle as for appendWeb.
void appendCue(std::vector<Vertex>& out, const CueView& cue, Vec3 viewer, float pixelAngle,
               UiStyle style = UiStyle::comic);
// A knockout of the player's, lettered where the enemy stood: "KO!" in yellow
// on a red burst with a paper heart, as a sound word is. It pops in, holds,
// and is gone in two poses: this one is done, look for the next.
struct KnockoutView {
    static constexpr float seconds = 8.f / 12; // eight poses
    Vec3 point{};
    float age = -1; // seconds since he went down; below 0, or past `seconds`: nothing
};
// pixelAngle as for appendWeb.
void appendKnockout(std::vector<Vertex>& out, const KnockoutView& knockout, Vec3 viewer, float pixelAngle,
                    UiStyle style = UiStyle::comic);
// Spider-Sense (spider_sense.hpp): the warning of an attack about to land,
// printed over the attacker's head, facing the viewer. The comic's lines:
// five jagged strokes fanned up from his head, in what the warning asks for
// (cyan: a fist at him counters; red: dodge; yellow: jump, under a paper
// chevron), keyed in ink on a hard drop. Around them a paper ring closes in a
// notch at a time as the blow comes; in its last notch the ring is a yellow
// burst's edge: now. It pops in out of register, then boils as a held
// drawing does. A counter breaks it: a pose of paper flash, a pose of its
// strokes thrown wide and thin, a pose small, and it is gone.
struct SenseView {
    static constexpr float brokenSeconds = 3.f / 12; // three poses
    Vec3 point{};                                    // the top of his head: the mark stands over it
    SenseAnswer answer = SenseAnswer::Counter;
    float left = 1;    // 1..0 of the warning's time left (senseLeft)
    bool timed{};      // false: the game set no time, no ring
    float age{};       // seconds since it showed
    float broken = -1; // seconds since a counter broke it; below 0: not
};
// pixelAngle as for appendWeb.
void appendSense(std::vector<Vertex>& out, const SenseView& sense, Vec3 viewer, float pixelAngle,
                 UiStyle style = UiStyle::comic);
// The warning of an attacker out of view, in the player's sightline (Arkham
// Shadow's icon there): the same mark, small, on the circle
// SenseSightConfig::angle round the middle of the view on his side, an
// arrowhead beyond it pointing to him, and three of the comic's wavy lines at
// the rim of the view on that side (SenseSightConfig::rim). `head`: the
// middle of the eyes and their orientation, in the world.
struct SenseSightView {
    SenseAnswer answer = SenseAnswer::Counter;
    SenseSight sight{}; // its `shown`: drawn at all
    float left = 1;
    bool timed{};
    float age{}, broken = -1;
};
void appendSenseSight(std::vector<Vertex>& out, const SenseSightView& view, Pose head, float pixelAngle,
                      const SenseSightConfig& config = {}, UiStyle style = UiStyle::comic);
} // namespace spidy
