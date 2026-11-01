#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
// Made by Agent Mulder.
//
// 0.0.73 MP SOLO TEST: drawing the remote player (mp_session.hpp's SessionView) - pure, so the tests and
// apps/mp_preview draw exactly what the game draws.
//   * where: mp::AnchorFollow's spot ~3 m ahead, facing the player (loopback: mirrored like a partner;
//     the Test Buddy: its own root offset and yaw from that spot, not mirrored)
//   * what: ONLY the mod's own models - the remote suit's suit_wear rig and mesh_model mesh (Models\Suits)
//     when that suit is installed, else a soft stick figure in the hero's colour (soft_shapes). Never a
//     game body, never a game address.
//   * the nameplate above it: "PLAYER 2 (YOU, LOOPBACK)  PING 3 MS" / "PLAYER 2 (TEST BUDDY)  PING 4 MS".
// Everything here is mod-internal state (CONFIRMED: the mod's own tracked poses, hero, worn suit slot and
// action flags); nothing is read from the game.
#include "spidy/math.hpp"
#include "spidy/mp_net.hpp"
#include "spidy/suit_wear.hpp"
#include "spidy/vertex.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace spidy::mp {
// The packet's suit id for mesh_model's worn slot (noSuit: none / not a suit); suitHash names the
// folder (fnv1a of mesh_model::suitFolders[k]).
uint8_t suitIdOfSlot(uint32_t slot);
uint32_t suitHashOf(uint8_t suit);
// The mesh_model slot to draw for a received state (0: none - the figure): the folder its hash names
// (a build with another suit order still finds it), else the id when its hash is 0 or matches.
uint32_t slotOfState(const PoseState& s);

// A ghost (mp::partner's, in the player's level frame from his feet) placed in the overlay's space:
// p -> origin + turn.rotate(p), its up turn.rotate(+Y) (game_xr: the feet + travel and trackingTurn).
suit_wear::Tracked toTracked(const Ghost& g, Vec3 origin, Quat turn);

// The soft stick figure (no suit model): head ring, torso, arms to the grips, legs, in `color` (linear),
// facing `viewer`. Returns the vertices added.
size_t appendFigure(std::vector<Vertex>& out, const suit_wear::Tracked& t, Vec3 viewer, Vec3 color, float alpha);
// The hero's colour (hero072::bio, linear).
Vec3 heroColor(uint8_t hero);

// "PLAYER 2 (YOU, LOOPBACK)  PING 3 MS"; the buddy: "PLAYER 2 (TEST BUDDY)  PING 4 MS"; ping unknown: "PING --".
std::string nameplate(bool buddy, const std::string& buddyName, int pingMs);
// 0.0.74 MP JOIN: a friend's game (peerGame) shows his own name: "PETER  PING 42 MS" ("PLAYER 2" unnamed).
std::string nameplate(bool buddy, const std::string& buddyName, int pingMs, bool peerGame);
// The nameplate centred `above` (world), turned to `viewer`, on a dark backing, at `alpha`.
size_t appendNameplate(std::vector<Vertex>& out, const std::string& text, Vec3 above, Vec3 up, Vec3 viewer, float alpha);
} // namespace spidy::mp
