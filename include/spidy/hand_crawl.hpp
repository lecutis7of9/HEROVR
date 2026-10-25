#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
//
// Update 37: hand-over-hand climbing on the GAME'S OWN wall crawl.
//
// Why this way. On the swing's own wall (Swing::wallMotion, SpidyVR Remastered's HAND WALL
// CLIMBING, carried over unchanged) the game keeps the hero in its AIR state, so the drawn
// body stands straight out from the wall with no leg movement - the October 24 report.
// SpidyVR Remastered laid the hero onto the wall with the game's own crawl
// (game_warp.hpp: Hero::HeroTransitionManager +0x360 -> HeroStateWallCrawlEnter); that
// needs the transition manager and the component registry, whose Spider-Man 2 addresses are
// NOT evidenced (game_warp stays off, SM2TODO). What IS there in Spider-Man 2:
// - CONFIRMED (exe RTTI): the crawl's states, HeroStateWallCrawlEnter/Idle/Move/Corner...
// - CONFIRMED (the port, game_swing.cpp): Spidy already sees the game's crawl (the actor's
//   up turned onto a surface a ray finds, Surface::game), and the rig already turns the view
//   onto it and lays the stick's axes into the surface (GameTrackingRig, crawl_stick.hpp).
// - CANDIDATE: the crawl's own locomotion moves the hero by the left stick, flat on the wall
//   with its own leg animation at the stick's speed. So a hand pulling on the wall becomes
//   that stick: the game crawls, the body lies on the wall and the legs move, all its own.
//
// The pull: while a hand grips near the wall (grip, or grip or trigger: GRIP TYPE), its
// motion against the head, turned into the world, is how the body should move - opposite to
// the hand (pull down, climb up). Only the part along the wall counts; two hands share it.
// The stick is that speed over the crawl's top speed (crawlSpeed), times PULL. A hand let
// go keeps the last speed for coast seconds, so hand-over-hand is one motion, not stutter.
#include "math.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace spidy::hand_crawl {
enum GripType : uint32_t { gripOnly = 0, gripOrTrigger = 1, triggerOnly = 2, gripTypes = 3 };
inline constexpr const char* gripNames[gripTypes] = {"GRIP", "GRIP OR TRIGGER", "TRIGGER"};
inline const char* gripName(uint32_t g) { return g < gripTypes ? gripNames[g] : "?"; }
struct Config {
    bool on = true;                     // live.txt hand_climb
    uint32_t pull = 100;                // climb_pull, percent (25-300)
    uint32_t grip = gripOrTrigger;      // climb_grip
    static constexpr uint32_t minPull = 25, maxPull = 300;
    static constexpr float crawlSpeed = 2.5f;  // m/s the game's crawl reaches at a full stick (CANDIDATE)
    static constexpr float reach = .45f;       // a hand this near the wall's plane can take hold
    static constexpr float press = .55f, keep = .30f; // the hold's button (forgiving), and let go below
    static constexpr float coast = .18f;       // seconds a let-go keeps the last speed
    static constexpr float smoothing = .06f;   // seconds, the speed's ease
};
struct Hand {
    bool tracked{}, free{};   // free: no web attached, nothing else claims the buttons
    float grip{}, trigger{};
    Vec3 relative{};          // the grip relative to the head, tracking space
    Vec3 world{};             // the grip in the world (for the wall distance)
};
struct Frame {
    float seconds{};
    bool play{};              // VR play, nothing else holding the controls
    bool crawl{};             // the game's crawl holds the player (SurfaceHold::crawl)
    Vec3 normal{}, anchor{};  // out of the wall, unit; a point on it
    Vec3 forwardAxis{};       // the stick's forward along the surface (GameTrackingRig)
    Quat turn{};              // tracking space -> world (Swing's trackingTurn)
    std::array<Hand, 2> hands{};
};
struct Out {
    bool active{};            // the stick is the climb's this frame
    float stickX{}, stickY{}; // -1..1 (x right, y forward along the surface)
    bool grabbed[2]{};        // a hand took hold this frame (haptic)
    bool holding[2]{};
    Vec3 velocity{};          // the asked speed, world
};
inline float buttonOf(const Hand& h, uint32_t type) {
    const float g = std::isfinite(h.grip) ? h.grip : 0.f, t = std::isfinite(h.trigger) ? h.trigger : 0.f;
    return type == gripOnly ? g : type == triggerOnly ? t : std::max(g, t);
}
class Climber {
  public:
    Out update(const Frame& f, const Config& c) {
        Out out;
        const float dt = std::isfinite(f.seconds) ? std::clamp(f.seconds, 0.f, .1f) : 0.f;
        const Vec3 n = normalized(f.normal);
        const bool surface = f.crawl && finite(n) && length(n) > .5f && finite(f.anchor);
        const bool live = c.on && f.play && surface && dt > 0;
        Vec3 pull{};
        unsigned count{};
        for (unsigned i = 0; i < 2; ++i) {
            auto& s = hands_[i];
            const auto& h = f.hands[i];
            const float b = buttonOf(h, c.grip);
            const bool usable = live && h.tracked && h.free && finite(h.relative) && finite(h.world);
            if (!usable || b < Config::keep) {
                s.held = false;
                continue;
            }
            if (!s.held) {
                if (b < Config::press || std::abs(dot(h.world - f.anchor, n)) > Config::reach)
                    continue;
                s.held = true;
                s.previous = h.relative;
                out.grabbed[i] = true;
                ++grabs_[i];
                continue;
            }
            Vec3 moved = f.turn.rotate(s.previous - h.relative) / dt; // the body goes against the hand
            s.previous = h.relative;
            moved -= n * dot(moved, n);
            if (finite(moved)) {
                pull += moved;
                ++count;
            }
            out.holding[i] = true;
        }
        if (count) {
            target_ = pull / static_cast<float>(count) * (static_cast<float>(std::clamp(c.pull, Config::minPull,
                                                                                        Config::maxPull)) / 100.f);
            coastLeft_ = Config::coast;
        } else if (coastLeft_ > 0 && live) {
            coastLeft_ -= dt;
        } else {
            target_ = {};
            coastLeft_ = 0;
        }
        const float k = dt > 0 ? 1 - std::exp(-dt / Config::smoothing) : 1.f;
        speed_ += (target_ - speed_) * k;
        if (!live) {
            speed_ = {};
            return out;
        }
        out.active = count > 0 || coastLeft_ > 0;
        if (!out.active)
            return out;
        // The stick in the surface's axes as the rig lays them (crawl_stick.hpp: right = ahead x up).
        Vec3 ahead = f.forwardAxis - n * dot(f.forwardAxis, n);
        if (!(length(ahead) > .3f)) {
            const Vec3 up = Vec3{0, 1, 0} - n * n.y;
            ahead = length(up) > .3f ? up : cross(n, Vec3{1, 0, 0});
        }
        ahead = normalized(ahead);
        const Vec3 right = cross(ahead, n);
        float x = dot(speed_, right) / Config::crawlSpeed, y = dot(speed_, ahead) / Config::crawlSpeed;
        const float l = std::hypot(x, y);
        if (l > 1) {
            x /= l;
            y /= l;
        }
        out.stickX = std::isfinite(x) ? x : 0.f;
        out.stickY = std::isfinite(y) ? y : 0.f;
        out.velocity = speed_;
        ++frames_;
        return out;
    }
    uint64_t grabs(unsigned i) const { return i < 2 ? grabs_[i] : 0; }
    uint64_t frames() const { return frames_; }

  private:
    struct HandState {
        bool held{};
        Vec3 previous{};
    };
    std::array<HandState, 2> hands_{};
    Vec3 target_{}, speed_{};
    float coastLeft_{};
    uint64_t grabs_[2]{}, frames_{};
};
// Telemetry, SpidyHandCrawlData (tools/update37_records.py).
struct Data {
    uint32_t magic = 0x4c575243, version = 1, bytes = 96, on = 1; // CRWL
    uint32_t pull = 100, grip = gripOrTrigger, wallCrawl = 1, crawlNow{};
    uint64_t grabs[2]{}, climbFrames{}, crawlFrames{}, swingWallFrames{};
    float lastStickX{}, lastStickY{}, peakSpeed{}, pad{};
    uint64_t sequence{};
};
static_assert(sizeof(Data) == 96);
} // namespace spidy::hand_crawl
