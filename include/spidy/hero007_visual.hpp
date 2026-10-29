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
#include <memory>
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
    uint32_t holster{};  // 0 none, 1 a hip holster round it, 2 a back sheath, 3 a baton tube, 4 (0.0.72) a drop-leg thigh holster
    uint32_t side{};     // lookBatonModel: 0 the left baton, 1 the right; lookShield*: 0 the LEFT arm's, 1 the RIGHT's
    bool palm{};         // Version 0.0.7 GUN GRIP: at is the palm's grip point; the gun's own grip middle goes there
};
// CAPTAIN AMERICA's shield Items: forward = the face (the star side), up = along the forearm (elbow
// -> hand), the model frame of captain.hpp (the hand strap at the origin, -z the face).
inline constexpr uint32_t drawSmokes = 6, drawShells = 24;
// Version 0.0.72: SUPERMAN's scorch marks, the PUNISHER's bullet impacts (each the newest overwriting the oldest).
inline constexpr uint32_t drawScorches = 4, drawImpacts = 6;
inline constexpr float scorchLife = 2.5f, impactLife = .35f, flashLife = .06f;
// Version 0.0.72 THIN BEAMS: each eye's heat beam's half width (m), even along it (0.0.7: 1.2 cm widening to 3.2 cm).
inline constexpr float heatBeamWidth = .0045f;
// Everything 0.0.7 draws in one frame (NativeEyeFrame::hero007), world positions.
// Version 0.0.72: Draw::modelWanted bits (a look's MODEL setting on).
inline constexpr uint32_t modelBitBatarang = 1, modelBitGuns = 2, modelBitBaton = 4, modelBitShield = 8;
// A look whose model is wanted: true when the built-in stand-in must NOT be drawn for it.
constexpr bool noStandIn(uint32_t wanted, uint32_t look) {
    return ((wanted & modelBitBatarang) && look == lookBatarang) ||
           ((wanted & modelBitGuns) && (look == lookPistol || look == lookSawedOff || look == lookSawedOffOpen)) ||
           ((wanted & modelBitBaton) && look == lookBatonModel) ||
           ((wanted & modelBitShield) && (look == lookShield || look == lookShieldSpin));
}
struct Draw {
    bool any{};
    Item held[2]{};
    Item worn[4]{};
    Item flying[2]{};  // thrown batarangs
    char ammo[2][10]{};      // each held gun's readout ("" none)
    Vec3 beamFrom[2]{}, beamTo{};
    float beamFade = 0;      // 0: no beam
    bool beamFreeze{};       // COLD BREATH's frost instead of heat
    // Version 0.0.72 HEAT VISION: how far the beam reaches from between the eyes (headLockBeam redraws it
    // from the rendered head along it), whether its end is on an enemy (the hit sparks), a clock for the
    // sparks' flicker, and the scorch marks it left (age s, 1e9: none).
    float beamReach{};
    bool beamHit{};
    float beamClock{};
    Vec3 scorchAt[drawScorches]{};
    float scorchAge[drawScorches]{1e9f, 1e9f, 1e9f, 1e9f};
    // Version 0.0.72 PUNISHER: each hand's muzzle flash (age s; 1e9: none) at its muzzle along the
    // barrel, and the hit puffs on the enemies shot (age s; 1e9: none), facing back along the shot.
    Vec3 flashAt[2]{}, flashWay[2]{};
    float flashAge[2]{1e9f, 1e9f};
    bool flashShotgun[2]{};
    Vec3 impactAt[drawImpacts]{}, impactWay[drawImpacts]{};
    float impactAge[drawImpacts]{1e9f, 1e9f, 1e9f, 1e9f, 1e9f, 1e9f};
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
    // Version 0.0.73 (fix 10) RETRACTABLE CLAWS: Models\Claws\Left|Right (the JACKET AND JEANS suit's own claws) for
    // every Wolverine suit, by lookClaws' side (null: 0.0.72's built-in claws). Library-owned.
    const mesh_model::Mesh* clawModel[2]{};
    // 0.0.73 test 5 CLAW LOOK (ON, the default): those claws out between the knuckles along the fist (appendClawSteel);
    // false: test 4's (appendClawModel).
    bool clawSteel{};
    // SHIELD LOOK MODEL: Models\Shield (null: the BUILT-IN look). Library-owned.
    const mesh_model::Mesh* shieldModel{};
    // Version 0.0.72 MODEL MISSING (the user, 27 Oct: "not a fake lookalike"): the looks whose MODEL
    // setting is on (modelBit*). One of them whose model is null or will not draw is NOT drawn as the
    // built-in stand-in: nothing is drawn and the wrist says MODEL MISSING: <name> (game_xr).
    uint32_t modelWanted{};
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
    // Version 0.0.72: how far each line has flown out of the muzzle (0-1, grappleOut); 1: taut to the hook.
    float grappleOut[2]{1.f, 1.f};
    // Version 0.0.7 BATMAN SMOKE PELLET: each hand's pellet, in the hand or in the air (false: none).
    bool pellet[2]{};
    Vec3 pelletAt[2]{};
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
// Version 0.0.73 (fix 10) RETRACTABLE CLAWS: a hand's claws model (the claws' base at its origin, -z along them, +y the
// back of the hand) at the knuckles of lookClaws' frame (clawKnuckle ahead, clawRise up, as the built-in claws),
// sliding out with it.spin (the SNIKT extension 0-1.1; 0 reads as out). False: not drawn (the built-in claws then).
bool appendClawModel(std::vector<Vertex>& out, const Item& it, const mesh_model::Mesh& model, Vec3 viewer);
// 0.0.73 test 5 CLAW LOOK (the owner after test 4: "the fan out is fine but placement, color and rotation are totally
// screwed"). Test 4 laid the claw model's knuckle row (its x) across the fist's back-to-palm line and its back of
// the hand (+y) toward the thumb (lookClaws' up), so the fan rose out of the thumb side of the fist and stood up off
// the top of the hand; and it took the suit's rust-brown claw texture. Now, in lookClaws' frame (forward: along the
// forearm out past the knuckles; up: toward the thumb, gun_grip::held), the model's +y is the back of the hand
// (handBackOf: the grip's -X on the left, +X on the right), its x the knuckle row (index at the thumb side) and -z
// along the forearm: the three claws come out of the back of the fist between the knuckles (clawSteelAhead along
// the forearm, clawSteelBack toward the back of the hand), along the punch, fanned as the artist made them. Drawn
// polished steel (never the texture): lit per face with a highlight from the viewer. CANDIDATE: the two offsets
// (measured on the suits' drawn fists in apps/hero_hands_preview, not on a headset).
inline constexpr float clawSteelAhead = .055f, clawSteelBack = .024f;
// The claws' base and axes (world) for lookClaws' item: x the knuckle row, y the back of the hand, f along the claws.
bool clawSteelFrame(const Item& it, Vec3& base, Vec3& x, Vec3& y, Vec3& f);
bool appendClawSteel(std::vector<Vertex>& out, const Item& it, const mesh_model::Mesh& model, Vec3 viewer);
// 0.0.73 test 5 CLAW BONE (the owner after the CLAW LOOK previews: "none of the wolverine claw things actually looks
// like it comes out of the hand, looks like they all float around it"). The claws locked to the drawn hand's BONE:
// each claw (the model's three parts, ascending in x) set rigidly with its root end on its root (suit_wear::clawPlace:
// inside the fist, between index/middle, middle/ring, ring/pinky, under the back of the hand) - the model's x across
// the knuckles (way x back), +y the back of the hand, -z the way - so the blade leaves the skin at the knuckles,
// fanned as the artist made it. Retracting (spin, 0-1) slides each claw back along its own line into the hand,
// what passes its root kept there (inside). False: not drawn (the caller then draws appendClawSteel by the grip).
struct ClawBone {
    bool ok{};
    Vec3 root[3]{}; // ascending across (way x back)
    Vec3 way{}, back{};
};
struct ClawParts {
    bool ok{};
    std::vector<uint8_t> part; // by triangle: its claw (0-2, ascending in x)
    Vec3 base[3]{}, axis[3]{};
    float reach[3]{};
};
bool clawParts(const mesh_model::Mesh& model, ClawParts& out);
bool clawBonePoint(const ClawParts& parts, const ClawBone& at, int k, Vec3 modelPoint, float ext, Vec3& world);
bool appendClawBone(std::vector<Vertex>& out, float spin, const ClawBone& at, const mesh_model::Mesh& model, Vec3 viewer);
// Version 0.0.72 PUNISHER VAN: a model up to 8 m placed by its axes (world = at + right x + up y - forward z).
// Test build 10: `scale` the model's size (1: as made).
bool appendPlacedModel(std::vector<Vertex>& out, const mesh_model::Mesh& model, Vec3 at, Vec3 forward, Vec3 up, Vec3 viewer,
                       float scale = 1);
// The sawed-off as its frame and barrels; open: the barrels turned 35 degrees down about the pivot.
bool appendSawedOffModel(std::vector<Vertex>& out, const Item& it, const mesh_model::Mesh& frame,
                         const mesh_model::Mesh& barrels, bool open, Vec3 viewer);
// A held or thrown baton model from its butt (grip) to its tip.
// Test build 10 BATON SIZE ("daredevils batons is too large now, visually it was a good size before"): the
// Yanez model is 57 cm long but 6.7 cm thick (twice the 0.0.62 red bar's 3.2 cm, the CLUB's 3.4-3.8 cm), so
// it is drawn round its own axis at batonDiameter (an escrima stick's 1.4 inches) and along it at the
// butt-to-tip span given (the old baton's 57 cm). inHand: the model's own grip (its origin, a hand-width in
// from the ridged butt) on the hand, gripShare up the span from the butt (baton_grip.hpp); else the butt on
// `grip` (holstered, thrown).
inline constexpr float batonDiameter = .036f;
struct BatonFit {
    float across{1}, along{1}; // the model's scale round its axis and along it
    Vec3 at{};                 // where the model's origin goes
    bool ok{};
};
BatonFit batonFit(Vec3 grip, Vec3 tip, const mesh_model::Mesh& model, bool inHand);
bool appendBatonModel(std::vector<Vertex>& out, Vec3 grip, Vec3 tip, const mesh_model::Mesh& model, Vec3 viewer,
                      bool inHand = false);
// Version 0.0.73 DAREDEVIL BATON TIP (dd_baton073.hpp): the model cut at its seam - the triangles whose middle
// is in the last tipShare of its length (the silver striking end, z from -.08 m on the Yanez model) - kept
// once per model (its own corners, uvs, normals and texture layers: nothing flattened). part true: the tip.
// Null: no usable model (the club look then).
std::shared_ptr<const mesh_model::Mesh> batonPart(const mesh_model::Mesh& model, bool tip, float tipShare);
// The held baton with its tip flown: the body where appendBatonModel draws it (butt `grip`, `tip` its seated
// tip) and the cut-off tip with its back on tipBack and its point at tipFront. False (nothing drawn): no cut.
bool appendBatonModelSplit(std::vector<Vertex>& out, Vec3 grip, Vec3 tip, const mesh_model::Mesh& model, Vec3 viewer,
                           Vec3 tipBack, Vec3 tipFront, float tipShare);
// The club look's split: the club from the butt to the seam and its steel tip flown (tipBack -> tipFront).
void appendBatonClubSplit(std::vector<Vertex>& out, Vec3 grip, Vec3 seam, Vec3 viewer, float pixelAngle, Vec3 tipBack,
                          Vec3 tipFront);
// The rope between the baton's seam and its flown tip: a 6 mm grey cord, taut, no hook (up to 80 m).
void appendBatonRope(std::vector<Vertex>& out, Vec3 from, Vec3 to, Vec3 viewer, float pixelAngle);
// The thrown baton's chain: a thin steel cable from the hand to the baton, sagging a little.
void appendBatonCable(std::vector<Vertex>& out, Vec3 from, Vec3 to, Vec3 viewer, float pixelAngle);
// BATMAN's grapple line: a taut dark line from the gun hand to the hook (three prongs on the
// surface), up to 50 m.
// Version 0.0.72: an 8 mm grey-black cord; flown (0-1) how far it has shot out of the muzzle, the hook on its tip.
void appendGrappleLine(std::vector<Vertex>& out, Vec3 from, Vec3 to, Vec3 viewer, float pixelAngle, float flown = 1.f);
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
// Version 0.0.72 HEAT VISION: the sparks where a beam meets an enemy (short flickering streaks, `clock`
// s for the flicker) and a scorch mark it leaves (a dark burn, its ember fading over scorchLife).
void appendHeatSparks(std::vector<Vertex>& out, Vec3 from, Vec3 at, float clock, Vec3 viewer, float pixelAngle);
void appendScorch(std::vector<Vertex>& out, Vec3 at, float age, Vec3 viewer);
// Version 0.0.72 PUNISHER: a muzzle flash (a star of short spikes ahead of the muzzle, flashLife s;
// the sawed-off's bigger) and a hit puff on an enemy (a spray back along the shot, impactLife s).
void appendMuzzleFlash(std::vector<Vertex>& out, Vec3 at, Vec3 way, bool shotgun, float age, Vec3 viewer, float pixelAngle);
void appendImpact(std::vector<Vertex>& out, Vec3 at, Vec3 way, float age, Vec3 viewer, float pixelAngle);
void appendSmoke(std::vector<Vertex>& out, Vec3 at, float age, Vec3 viewer);
// BATMAN's smoke pellet: a small dark ball (about 4 cm) with a glint.
void appendPellet(std::vector<Vertex>& out, Vec3 at, Vec3 viewer);
void appendShell(std::vector<Vertex>& out, Vec3 at, Vec3 way, bool shotgun, float age, Vec3 viewer, float pixelAngle);
void appendAmmo(std::vector<Vertex>& out, const char* text, const Item& gun, Vec3 viewer);
// One item by its look (with its holster when it has one).
void appendItem(std::vector<Vertex>& out, const Item& it, Vec3 viewer, float pixelAngle);
// Everything in `d`, moved by `travel` (the player's move to this image).
void appendHero007(std::vector<Vertex>& out, const Draw& d, Vec3 travel, Vec3 viewer, float pixelAngle);

// Version 0.0.72 REAL MODELS: the models' own sink. Set (game_xr, each frame) when the overlay has its
// model texture array: the drop-in models then go there textured (Vertex::shape = u, v, layer, 1),
// drawn depth-tested and unsorted; null: they go untextured into `out` as before (their light copy).
void setModelSink(std::vector<Vertex>* sink);
std::vector<Vertex>* modelSink();
// Version 0.0.72: ten ordinary models, plus the van (body, cab, wheel) and a hammer in hand and one on show;
// build 9: + the worn suit twice (first person and the mirror).
inline constexpr size_t modelSinkBudget = 3 * (10 * size_t{mesh_model::drawTriangles} + size_t{mesh_model::vanTriangles} +
                                               2 * size_t{mesh_model::drawTriangles} + 2 * size_t{mesh_model::hammerTriangles} +
                                               2 * size_t{mesh_model::drawTriangles});

// The vertex budget (each softBar 54, softTriangle 21, a 10-sided glob 90): the most one call may add.
namespace budget {
inline constexpr size_t pistol = 1300, sawedOff = 1500, batarang = 600, claws = 2600 /* 0.0.72: curved, 5 segments a claw */, club = 1200,
                        holster = 600 /* 0.0.72: the drop-leg holster */, shield = 2100, batarangModel = 3 * mesh_model::liteTriangles, model = 3 * mesh_model::liteTriangles,
                        cable = 600, grapple = 2400 /* 0.0.72: the cord + the hook at the tip */, pellet = 200, heatBeam = 1000, freeze = 1500, smoke = 300, shell = 240, ammo = 2500,
                        heatSparks = 500, scorch = 200, flash = 500, impact = 500;
// The whole frame's worst case: 2 held + 4 worn + 2 flying + beam + smoke + shells + 2 readouts
// (+ 2 cables, 2 grapple lines, 2 smoke pellets; a model is at most `model`; the one shield is at most `shield` == `model`, in a held,
// worn or flying slot).
inline constexpr size_t frame = 2 * std::max(sawedOff, model) + 4 * (std::max(sawedOff, model) + holster) + 2 * cable + 2 * grapple + 2 * std::max(batarang, batarangModel) + freeze +
                                drawSmokes * smoke + drawShells * shell + 2 * ammo + 2 * pellet +
                                heatSparks + drawScorches * scorch + 2 * flash + drawImpacts * impact; // 0.0.72
static_assert(shield <= model, "the shield fits any item slot's budget");
} // namespace budget
} // namespace spidy::hero007
