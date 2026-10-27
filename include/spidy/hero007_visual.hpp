#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
//
// Version 0.0.7 HERO PROFILES' looks, drawn in the overlay like every other mark (soft_shapes.hpp):
// flat, soft-edged shapes turned to the eye - the overlay has no depth test, lighting or textures,
// so each object is a shaded silhouette (a dark rim, the body colour, a light streak on the lit
// side) at real-world sizes:
//  - a pistol (a 20 cm polymer-framed service pistol: steel slide, black frame, textured grip),
//  - a sawed-off (two 30 cm steel barrels side by side, a wooden pistol grip and fore-end; it can
//    be drawn broken open at the hinge),
//  - a batarang (20 cm span, black, scalloped wings),
//  - three 30 cm steel claws on the knuckles, heat vision's twin red beams and cold breath's
//    frost cone, smoke puffs, spent cases (brass 9 mm, red 12 gauge), a gun's ammo readout,
//  - the BATON LOOK CLUB billy club (a ribbed black grip, red lacquer shaft, steel caps),
//  - the holsters (hip, back) with their item worn on the body.
// Every function draws nothing for a non-finite input or when nearer than 5 cm / past its range,
// and puts its vertices back if any came out insane (web_style::sane). The vertex counts are
// budgeted (hero007_visual::budget) and checked by core_tests.
#include "mesh_model.hpp"
#include "vertex.hpp"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace spidy::hero007 {

// What an item looks like (0: nothing).
enum Look : uint32_t {
    lookNone = 0,
    lookPistol,
    lookSawedOff,
    lookSawedOffOpen,
    lookBatarang,
    lookClaws,
    lookClub,      // BATON LOOK CLUB
    lookBaton062,  // the 0.0.62 baton (appendBaton)
    lookBatonModel, // BATON LOOK MODEL: Draw::batonModel[side] (CLUB when it is missing)
    lookShield,     // CAPTAIN AMERICA: the shield, `at` its hand strap (on the arm or the back), side = the arm
    lookShieldSpin, // the thrown shield, `at` the disc's middle, spinning `spin` about its face
    lookGrappleGun, // BATMAN's grapple gun: ONLY Draw::grappleGunModel (Models\GrappleGun); nothing without it
    looks
};
// One item: `at` its grip (world), `forward` along its barrel / blade / shaft, `up` its top.
struct Item {
    uint32_t look = lookNone;
    Vec3 at{}, forward{0, 0, -1}, up{0, 1, 0};
    float spin = 0;      // a thrown batarang's turn (radians)
    uint32_t holster{};  // 0 none, 1 a hip holster round it, 2 a back sheath, 3 a baton tube
    uint32_t side{};     // lookBatonModel: 0 the left baton, 1 the right; lookShield*: 0 the LEFT arm's, 1 the RIGHT's
    bool palm{};         // Version 0.0.7 GUN GRIP: at is the palm's grip point; the gun's own grip middle goes there
};
// CAPTAIN AMERICA's shield Items: forward = the face (the star side), up = along the forearm (elbow
// -> hand), the model frame of captain.hpp (the hand strap at the origin, -z the face).
inline constexpr uint32_t drawSmokes = 6, drawShells = 24;
// Everything 0.0.7 draws in one frame (NativeEyeFrame::hero007), world positions.
struct Draw {
    bool any{};
    Item held[2]{};
    Item worn[4]{};
    Item flying[2]{};  // thrown batarangs
    char ammo[2][10]{};      // each held gun's readout ("" none)
    Vec3 beamFrom[2]{}, beamTo{};
    float beamFade = 0;      // 0: no beam
    bool beamFreeze{};       // COLD BREATH's frost instead of heat
    Vec3 smokeAt[drawSmokes]{};
    float smokeAge[drawSmokes]{1e9f, 1e9f, 1e9f, 1e9f, 1e9f, 1e9f};
    Vec3 shellAt[drawShells]{}, shellWay[drawShells]{};
    float shellAge[drawShells]{};
    bool shellShotgun[drawShells]{};
    uint32_t shells{};       // how many of shell* are live
    // BATARANG STYLE: the dropped-in model the batarangs are drawn as (null: DEFAULT). Points
    // into mesh_model::library(), which is never freed.
    const mesh_model::Mesh* batarangModel{};
    // GUN MODELS / BATON LOOK MODEL: the converted models (null: the built-in look). Library-owned.
    const mesh_model::Mesh* pistolModel{};
    const mesh_model::Mesh* sawedFrame{};
    const mesh_model::Mesh* sawedBarrels{};
    const mesh_model::Mesh* batonModel[2]{};
    // SHIELD LOOK MODEL: Models\Shield (null: the BUILT-IN look). Library-owned.
    const mesh_model::Mesh* shieldModel{};
    // DAREDEVIL: the chain from the hand to its baton, ONLY while that baton is thrown (and on its
    // bounces); never between the batons, never while held or holstered.
    bool cable[2]{};
    Vec3 cableFrom[2]{}, cableTo[2]{};
    // Version 0.0.7 BATMAN GRAPPLE GUN: the taut line from the firing hand to the hook, ONLY while
    // the grapple pulls him (cape_glide::Grapple); never a web, never at rest.
    // BATMAN GRAPPLE GUN MODEL: Models\GrappleGun (mesh_model::slotGrappleGun). Library-owned. Null: no gun
    // is drawn at all (no stand-in) and the line starts at the hand; with it, the line starts at its muzzle
    // (extras.pivot) when the gun is in the firing hand.
    const mesh_model::Mesh* grappleGunModel{};
    bool grapple[2]{};
    Vec3 grappleFrom[2]{}, grappleTo[2]{};
};

void appendPistol(std::vector<Vertex>& out, const Item& it, Vec3 viewer, float pixelAngle);
void appendSawedOff(std::vector<Vertex>& out, const Item& it, bool open, Vec3 viewer, float pixelAngle);
void appendBatarang(std::vector<Vertex>& out, const Item& it, Vec3 viewer, float pixelAngle);
// BATARANG STYLE: a loaded model in the batarang's place (span 20 cm, lit per face, far to
// near). False (nothing added) when it cannot be drawn: the caller draws DEFAULT instead.
bool appendBatarangModel(std::vector<Vertex>& out, const Item& it, const mesh_model::Mesh& model, Vec3 viewer);
// GUN MODELS / BATON LOOK MODEL: a converted model (metres, x right, y up, -z forward, the grip at
// the origin) at the item. False (nothing added) when it cannot be drawn: the built-in look instead.
bool appendModel(std::vector<Vertex>& out, const Item& it, const mesh_model::Mesh& model, Vec3 viewer);
// The sawed-off as its frame and barrels; open: the barrels turned 35 degrees down about the pivot.
bool appendSawedOffModel(std::vector<Vertex>& out, const Item& it, const mesh_model::Mesh& frame,
                         const mesh_model::Mesh& barrels, bool open, Vec3 viewer);
// A held or thrown baton model from its butt (grip) to its tip.
bool appendBatonModel(std::vector<Vertex>& out, Vec3 grip, Vec3 tip, const mesh_model::Mesh& model, Vec3 viewer);
// The thrown baton's chain: a thin steel cable from the hand to the baton, sagging a little.
void appendBatonCable(std::vector<Vertex>& out, Vec3 from, Vec3 to, Vec3 viewer, float pixelAngle);
// BATMAN's grapple line: a taut dark line from the gun hand to the hook (three prongs on the
// surface), up to 50 m.
void appendGrappleLine(std::vector<Vertex>& out, Vec3 from, Vec3 to, Vec3 viewer, float pixelAngle);
void appendClaws(std::vector<Vertex>& out, const Item& it, Vec3 viewer, float pixelAngle);
// BATMAN GRAPPLE GUN: the muzzle (the model's extras.pivot) of a gun Item drawn with `model`. False: no
// usable muzzle (no pivot, a bad frame) - the line starts at the hand instead.
bool grappleMuzzle(const Item& gun, const mesh_model::Mesh& model, Vec3& muzzle);
// CAPTAIN AMERICA BUILT-IN: the 0.61 m disc flat in its plane (red, white, red rings, the blue centre and
// the white star) seen from the face; the steel back and the two straps from behind.
void appendShield(std::vector<Vertex>& out, const Item& it, Vec3 viewer, float pixelAngle);
// SHIELD LOOK MODEL: the converted shield (captain.hpp's frame; the RIGHT arm's turned about the
// pivot). With extras.layer the straps and the disc are drawn as two layers in the eye's order
// (the overlay has no depth test), each far to near. False: the BUILT-IN look instead.
bool appendShieldModel(std::vector<Vertex>& out, const Item& it, const mesh_model::Mesh& model, Vec3 viewer);
void appendBatonClub(std::vector<Vertex>& out, Vec3 grip, Vec3 tip, Vec3 viewer, float pixelAngle);
void appendHolster(std::vector<Vertex>& out, const Item& it, Vec3 viewer, float pixelAngle);
void appendHeatBeam(std::vector<Vertex>& out, Vec3 from, Vec3 to, Vec3 viewer, float pixelAngle, float fade);
void appendFreezeBreath(std::vector<Vertex>& out, Vec3 from, Vec3 to, Vec3 viewer, float pixelAngle, float fade);
void appendSmoke(std::vector<Vertex>& out, Vec3 at, float age, Vec3 viewer);
void appendShell(std::vector<Vertex>& out, Vec3 at, Vec3 way, bool shotgun, float age, Vec3 viewer, float pixelAngle);
void appendAmmo(std::vector<Vertex>& out, const char* text, const Item& gun, Vec3 viewer);
// One item by its look (with its holster when it has one).
void appendItem(std::vector<Vertex>& out, const Item& it, Vec3 viewer, float pixelAngle);
// Everything in `d`, moved by `travel` (the player's move to this image).
void appendHero007(std::vector<Vertex>& out, const Draw& d, Vec3 travel, Vec3 viewer, float pixelAngle);

// The vertex budget (each softBar 54, softTriangle 21, a 10-sided glob 90): the most one call may add.
namespace budget {
inline constexpr size_t pistol = 1300, sawedOff = 1500, batarang = 600, claws = 1000, club = 1200,
                        holster = 400, shield = 2100, batarangModel = 3 * mesh_model::drawTriangles, model = 3 * mesh_model::drawTriangles,
                        cable = 600, grapple = 1800, heatBeam = 1000, freeze = 1500, smoke = 300, shell = 240, ammo = 2500;
// The whole frame's worst case: 2 held + 4 worn + 2 flying + beam + smoke + shells + 2 readouts
// (+ 2 cables, 2 grapple lines; a model is at most `model`; the one shield is at most `shield` == `model`, in a held,
// worn or flying slot).
inline constexpr size_t frame = 2 * std::max(sawedOff, model) + 4 * (std::max(sawedOff, model) + holster) + 2 * cable + 2 * grapple + 2 * std::max(batarang, batarangModel) + freeze +
                                drawSmokes * smoke + drawShells * shell + 2 * ammo;
static_assert(shield <= model, "the shield fits any item slot's budget");
} // namespace budget
} // namespace spidy::hero007
