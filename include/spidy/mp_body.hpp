#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
// Made by Agent Mulder.
//
// 0.0.73 MPSUIT PLAYER 2 BODY (the owner after test 6: "when im spiderman with ingame suits the "other player"
// was that stick figure thing again it only shows the characters we made normally").
//
// Why the stick figure (CONFIRMED, mod code): PLAYER 2 drew a suit only through the mod's own Models\Suits
// glb (mp_ghost slotOfState). A game suit - SPIDER-MAN's own, any of them, and every hero in his own look -
// has no mod suit slot (mp::suitIdOfSlot -> noSuit), so slotOfState is 0 and appendFigure drew the soft stick
// figure.
// What is not possible: the game's own suit mesh or a second game character (no evidenced address for the
// equipped suit's mesh, nor for spawning one - vr_mirror.hpp, hero_switch.hpp; never guessed), and no
// Spider-Man model ships with the mod or is on the box under a licence (THIRD-PARTY-MODELS.md).
// So, PLAYER 2 BODY: a solid, shaded, procedural hero body built here (pure: the tests and apps/mp_preview
// draw exactly what the game draws), posed by suit_wear::estimate on a 1.8 m rest skeleton of its own
// (bodyRig), opaque triangles with the light baked in (like hero007's claw steel):
//   * SPIDER-MAN (hero 0): the suit pattern - the primary over the head, the chest and back, the arms and the
//     boots, the secondary on the flanks, the underarms and the legs - with web lines, white lenses in a dark
//     rim and the spider on the chest and the back, in the worn suit's colours (the LOOK tail of the pose
//     packet: vr_mirror::SuitColours learned them from the game's own rendered suit, so a game suit's
//     colours; none learned yet / the Test Buddy: the classic red and black with a white spider).
//   * every other hero: the same body in the hero's colour (hero072::bio) with a darker secondary, a dark
//     visor band and a light chest badge.
// Only mod-internal state (CONFIRMED) - nothing is read from the game. combat38 mp_player2_body (PLAYER 2
// BODY): SUIT LOOK (0, the default) this body, FIGURE (1) the old stick figure. A mod suit model, when the
// remote suit is one, still wins; the stick figure only when this body cannot be built.
#include "spidy/math.hpp"
#include "spidy/mp_net.hpp"
#include "spidy/suit_wear.hpp"
#include "spidy/vertex.hpp"

#include <cstdint>
#include <vector>

namespace spidy::mp {
// The suit's look, linear RGB.
struct Look {
    Vec3 primary{.62f, .03f, .04f}, secondary{.02f, .02f, .03f}, emblem{.85f, .85f, .88f};
    bool spider = true;  // SPIDER-MAN's pattern (web lines, lenses, spider); else the plain hero body
    bool learned{};      // the colours came from the game's own rendered suit
};
// The look to draw a received state in: SPIDER-MAN its LOOK tail (else the classic red and black), any other
// hero its colour (heroColor).
Look lookOf(const PoseState& s);
// The LOOK tail for the player's own palette (vr_mirror::SuitPalette's three colours, linear) - fromLocal's
// state gets these before publish.
void setLook(PoseState& s, Vec3 primary, Vec3 secondary, Vec3 emblem, bool learned);

// combat38 mp_player2_body: 0 SUIT LOOK (the default), 1 FIGURE (the stick figure, test 6).
inline constexpr uint32_t bodySuitLook = 0, bodyFigure = 1;
inline bool bodyOn(uint32_t choice) { return choice != bodyFigure; }

// The body's own rest rig (no mesh): a 1.8 m skeleton facing +z, its left at +x, the feet at y 0 - all
// suit_wear::estimate needs.
const suit_wear::Rig& bodyRig();
// The body for tracked `t` (toTracked's) in `look`, lit toward `viewer`, appended to `out` as opaque
// triangles (Vertex: the colour linear, `alpha`). Returns the vertices added - 0 (and nothing added) when it
// cannot be posed; then the caller falls back to appendFigure.
size_t appendHeroBody(std::vector<Vertex>& out, const suit_wear::Tracked& t, const Look& look, Vec3 viewer, float alpha);
// The same from an already posed skeleton (suit_wear::estimate on bodyRig).
size_t appendHeroBody(std::vector<Vertex>& out, const suit_wear::Skeleton& k, const Look& look, Vec3 viewer, float alpha);
// No more than this many vertices are ever added (a frame's opaque list stays small).
inline constexpr size_t bodyMaxVertices = 60000;
} // namespace spidy::mp
