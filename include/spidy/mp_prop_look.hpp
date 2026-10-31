#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
// Made by Agent Mulder.
//
// 0.0.74 MP PROPS: the player's own props (the hero007 / hero072 looks and Daredevil's batons, as game_xr's
// saved frame has them) turned into the PROPS tail (mp_props.hpp), and a received tail turned back into the same
// looks placed on PLAYER 2 - so game_xr draws his batarang, Thor's hammer or Cap's shield with the very functions
// that draw the player's own. Pure: no game, no files (the models are looked up by game_xr).
//
// Frames: the sender's props are in the tracking frame (game_xr's saved values, drawn + travel), sent like the
// head and grips - from the feet (rendered.anchor), turned back by trackingTurn, then into the yawed root frame
// (SendFrame). The receiver places them on the ghost as mp::partner places the eyes and grips - the ghost's feet
// and body yaw, mirrored for the loopback - then into the overlay's world by toTracked's origin and turn
// (GhostFrame). Mirrored, the left and right of every prop swap too.
//
// Everything here is mod-internal (CONFIRMED: the mod's own props); no game address.
#include "spidy/hero007_visual.hpp"
#include "spidy/hero072_visual.hpp"
#include "spidy/mp_net.hpp"

#include <cstdint>

namespace spidy::mp {
// The sender's transform: local = toLocal.rotate(p - anchor) (toLocal = yaw(-root yaw) * trackingTurn^-1).
struct SendFrame {
    Vec3 anchor{};
    Quat toLocal{};
    Vec3 at(Vec3 p) const { return toLocal.rotate(p - anchor); }
    Vec3 way(Vec3 d) const { return toLocal.rotate(d); }
};
SendFrame sendFrame(Vec3 anchor, Quat trackingTurn, float rootYaw);

// The receiver's: world = origin + turn.rotate(feet + body.rotate(mirror ? x-flipped : p)).
struct GhostFrame {
    Vec3 origin{}, feet{};
    Quat turn{}, body{};
    bool mirror{};
    Vec3 at(Vec3 p) const;
    Vec3 way(Vec3 d) const;
};
GhostFrame ghostFrame(const Ghost& g, bool mirror, Vec3 origin, Quat turn);

// Daredevil's batons as game_xr's saved frame has them (tracking frame).
struct Batons {
    bool on[2]{};
    Vec3 grip[2]{}, tip[2]{};
    bool held[2]{};
    bool club{};
};
// 0.0.74 MP PROPS phase 3: IRON MAN's beams and palms and the shots in flight (repulsor bolts or web balls), as
// game_xr's saved frame has them (tracking frame; fade / glow 0: none).
struct ShotSource {
    bool repulsor{};                 // IRON MAN's look (false: web balls)
    Vec3 chestFrom{}, chestTo{};
    float chestFade{};
    Vec3 palmAt[2]{}, palmWay[2]{};
    float palmGlow[2]{};
    static constexpr size_t most = 6;
    Vec3 from[most]{}, at[most]{}, tail[most]{};
    float fade[most]{};
    Vec3 splashAt[most]{};
    float splashFade[most]{};
};
inline constexpr float fxSendFor = .3f;   // s each effect rides in the packets (about 9 at 30 Hz: one lost is no loss)
inline constexpr float fxKeepFor = 12.f;  // s the receiver keeps one (the draws fade each in its own life)
inline constexpr float shockLife = .6f, webSplashLife = .6f, repulsorTail = .6f;

// What the sender draws and which of its models it uses (the receiver loads the same ones when it has them).
struct PropSource {
    const hero007::Draw* h007{};
    const hero072::Look* h072{};
    Batons batons{};
    uint32_t batarangStyle{}; // combat38 BATARANG STYLE (0: DEFAULT, the built-in look)
    const ShotSource* shots{}; // phase 3 (null: none)
};
// Stable ids (blend and the receiver's dedupe match by them).
enum PropId : uint8_t { idHeld = 1, idFlying = 3, idWorn = 5, idHammer = 9, idBaton = 10, idPellet = 12, idRing = 14,
                        idBubble = 16, idFist = 17, idCharge = 18, idPalm = 19 };

// Everything the player shows, in priority order (held, the hammer, the batons, then what is thrown - batarangs, the
// shield, smoke pellets - then GREEN LANTERN's constructs and IRON MAN's palms, then worn; the lines after),
// cut at propItemsMost.
Props captureProps(const PropSource& src, const SendFrame& f);

// A received tail placed on PLAYER 2 (world positions: draw with travel 0). Models are NOT set (game_xr looks them
// up by the wanted slots below); modelWanted stays 0, so a model the receiver lacks is drawn as the built-in look.
struct RemoteProps {
    hero007::Draw h007{};
    hero072::Look h072{};
    Batons batons{};          // world
    bool batonModel{};        // the sender drew his batons as the BATON LOOK MODEL
    bool hammerSheathed{};    // on his back (h072's hammerSheathed stays false so appendLook draws it where it hangs)
    uint32_t batarangStyle{}; // 0: the built-in batarang
    uint32_t hammerChoice{};
    bool gunModels{}, shieldModel{}, claws{};
    // phase 3: IRON MAN's chest beam, palms, bolts / web balls and their splashes (world)
    struct Shots {
        bool repulsor{};
        Vec3 chestFrom{}, chestTo{};
        float chestFade{};
        Vec3 palmAt[2]{}, palmWay[2]{};
        float palmGlow[2]{};
        uint32_t n{};
        Vec3 from[propLinesMost]{}, at[propLinesMost]{}, tail[propLinesMost]{};
        float fade[propLinesMost]{};
        bool web[propLinesMost]{};
        uint32_t splashes{};
        Vec3 splashAt[8]{};
        float splashAge[8]{}; // s (game_xr fades each in its own look's life)
        bool splashWeb[8]{};
        bool any() const { return chestFade > 0 || palmGlow[0] > 0 || palmGlow[1] > 0 || n || splashes; }
    } shots{};
    bool any() const { return h007.any || h072.any || batons.on[0] || batons.on[1] || shots.any(); }
};
RemoteProps rebuildProps(const Props& p, const GhostFrame& f);

// 0.0.74 MP PROPS phase 3, the sender's effects ring: one-off effects (muzzle flashes, hits, smoke, scorch marks,
// splashes, shock rings) noticed as they start (their age back to 0) and kept with a sequence number; each rides in
// the packets for fxSendFor, newest first, up to propFxMost a packet.
struct FxSource {
    const hero007::Draw* h007{};
    const hero072::Look* h072{};
    const ShotSource* shots{};
};
class FxRing {
public:
    void feed(const FxSource& s, double now);
    void write(Props& p, const SendFrame& f, double now) const;
    void clear() { *this = FxRing{}; }
    uint16_t sent() const { return seq_; }

private:
    struct Fx {
        uint16_t seq{};
        uint8_t kind{}, param{};
        double born{};
        Vec3 at{}, way{0, 0, -1};
    };
    void add(uint8_t kind, uint8_t param, Vec3 at, Vec3 way, double born);
    static constexpr size_t most = 16;
    Fx ring_[most]{};
    size_t next_{}, count_{};
    uint16_t seq_{};
    float flash_[2]{1e9f, 1e9f}, impact_[hero007::drawImpacts]{}, smoke_[hero007::drawSmokes]{}, scorch_[hero007::drawScorches]{},
        splash_[ShotSource::most]{};
    bool shock_{}, primed_{};
    float shockAge_{};
};
// The receiver's: each effect taken once (by its sequence number), placed in the world where it happened, aged by
// the receiver's own clock - then put into PLAYER 2's look for the draws (flashes per hand, the newest hits, smoke
// and scorch marks, a shock ring, splashes).
class FxInbox {
public:
    void take(const Props& p, const GhostFrame& f, double now);
    void apply(RemoteProps& r, double now) const;
    bool live(double now) const;
    uint32_t taken() const { return taken_; } // effects taken (each once), for the probe and the log
    void clear() { *this = FxInbox{}; }

private:
    struct E {
        bool on{};
        uint16_t seq{};
        uint8_t kind{}, param{};
        double born{};
        Vec3 at{}, way{0, 0, -1};
    };
    static constexpr size_t most = 24;
    E e_[most]{};
    size_t next_{};
    uint32_t taken_{};
};
} // namespace spidy::mp
