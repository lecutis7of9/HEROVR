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
// What the sender draws and which of its models it uses (the receiver loads the same ones when it has them).
struct PropSource {
    const hero007::Draw* h007{};
    const hero072::Look* h072{};
    Batons batons{};
    uint32_t batarangStyle{}; // combat38 BATARANG STYLE (0: DEFAULT, the built-in look)
};
// Stable ids (blend and the receiver's dedupe match by them).
enum PropId : uint8_t { idHeld = 1, idFlying = 3, idWorn = 5, idHammer = 9, idBaton = 10, idPellet = 12, idRing = 14,
                        idBubble = 16, idFist = 17, idCharge = 18, idPalm = 19 };

// Everything the player shows, in priority order (held, the hammer, the batons, then what is thrown - batarangs, the
// shield, smoke pellets - then worn),
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
    bool any() const { return h007.any || h072.any || batons.on[0] || batons.on[1]; }
};
RemoteProps rebuildProps(const Props& p, const GhostFrame& f);
} // namespace spidy::mp
