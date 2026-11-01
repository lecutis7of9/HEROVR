#pragma once
// HERO VR 0.0.76 SM1 HERO PROPS: the heroes' own 3D weapons in the hands (the SM2 build's models and their artists:
// THIRD-PARTY-MODELS.md), drawn by the SM1 layer into both eyes as one extra OpenXR projection layer over his picture.
// This file is the pure part (no D3D, no OpenXR, no game memory), tested in tests/sm1_addon_tests.cpp:
//  - which models each hero holds (the SM2 build's mesh_model slots, loaded from HERO VR's Models folder);
//  - where each sits, from the controller's grip pose (the SM2 build's own grip helpers: thor_grip, gun_grip,
//    baton_grip, captain), as one affine placement a model (the GPU applies it; nothing is rebuilt per frame);
//  - the thrown hammer's / shield's / batarang's flight (out to the enemy or along the aim, back to the hand: the SM2
//    build's hammer numbers), in the GAME's world (the player swings and flies; the props must not move with him);
//  - the game <-> OpenXR mapping through the head (the beams' mapping, sm1_addon.hpp gameToHeadLocal);
//  - one GPU vertex a model corner, and the eye's projection (OpenXR fov, D3D depth 0..1).
// Nothing here ever moves the player (no THOR lunge).
#include "spidy/captain.hpp"
#include "spidy/controller_layout.hpp"
#include "spidy/gun_grip.hpp"
#include "spidy/math.hpp"
#include "spidy/mesh_model.hpp"

#include <cstdint>
#include <vector>

namespace spidy::sm1::props {

// --- The models ---------------------------------------------------------------------------------------------------
enum Model : uint32_t { none = 0, hammer, shield, clawsLeft, clawsRight, batonLeft, batonRight, pistol, batarang, modelCount };
inline constexpr const char* modelNames[modelCount] = {"none",         "Thor's hammer", "Cap's shield",
                                                       "left claws",   "right claws",   "left baton",
                                                       "right baton",  "pistol",        "batarang"};
// Its mesh_model slot (Models\<slotFolders[slot]>): the SM2 build's own converted files. hammerStyle: the SM2 THOR
// HAMMER choice (0 BOOOOP, 1 DEVILSEYE, 2 LOVE+THUNDER, 3 FCCOOPER).
uint32_t slotOf(Model m, uint32_t hammerStyle = 0);
// The hero's props (hero_modes indices; empty for a hero with none): what the layer loads and draws.
std::vector<Model> modelsFor(uint32_t hero);
bool hasProps(uint32_t hero);
// The props a trigger THROWS (the right hand's): THOR's hammer and CAP's shield come back; the batarang does not.
Model thrownBy(uint32_t hero, int hand);

// --- Placement ----------------------------------------------------------------------------------------------------
// One model's place: world = at + x * m.x + y * m.y + z * m.z (m: the model's own coordinates, metres).
struct Place {
    Vec3 at{}, x{1, 0, 0}, y{0, 1, 0}, z{0, 0, 1};
    bool ok = false;
};
Vec3 apply(const Place& p, Vec3 m);
// The SM2 appendModel / appendPlacedModel frame: world = at + right * x + up * y - forward * z (up made square to
// forward, right = forward x up). ok false on a degenerate frame.
Place placed(Vec3 at, Vec3 forward, Vec3 up);

// THOR's hammer (mesh_model::slotHammer: +y handle end -> head, -z a striking face): from the handle's end `at`, the
// handle's way and a face; `spin` turns it about the axis square to both (a flying hammer tumbles end over end).
Place hammerPlace(Vec3 at, Vec3 handle, Vec3 face, float spin);
// The held hammer in the right fist (thor_grip::held: the grip 13 cm up the handle, the face along the forearm).
Place hammerInHand(const Pose& grip, ControllerKind kind);
// CAP's shield (captain.hpp: the hand strap at the origin, y along the forearm, -z the face; side 1 the RIGHT arm's,
// the left's turned half a turn about the elbow strap). spin: turned about the disc's middle in its own plane.
Place shieldPlace(const captain::ShieldPose& p, uint32_t side, Vec3 pivot, Vec3 low, Vec3 high, float spin);
// The shield on the forearm from the grip (captain::onForearm with gun_grip::held's forearm and thumb).
captain::ShieldPose shieldOnForearm(const Pose& grip, unsigned hand, ControllerKind kind);
// A flying shield: flat (its face up), its disc's middle at `middle`, spinning (captain::spinRevs).
Place shieldFlying(Vec3 middle, Vec3 way, uint32_t side, Vec3 pivot, Vec3 low, Vec3 high, float spin);
// WOLVERINE's claws (mesh_model::slotClawsLeft / Right: the base at the origin, -z along them, +y the back of the
// hand): hero007_visual clawSteelFrame from the fist (gun_grip::held), out of the back of the hand along the punch.
Place clawsPlace(const Pose& grip, unsigned hand, ControllerKind kind);
// How far the claws reach (the steel shading's root-to-tip, model metres): hero007_visual's max(.05, -low.z).
float clawReach(Vec3 low);
// DAREDEVIL's baton (mesh_model::slotBatonLeft / Right, hero007_visual appendBatonModel): its -z end the butt,
// scaled to batonDiameter across and the shaft's length along, from baton_grip::shaft.
Place batonPlace(const Pose& grip, unsigned hand, ControllerKind kind, Vec3 low, Vec3 high);
// PUNISHER's pistol (mesh_model::slotPistol, gun_grip.hpp: its palm point on the grip point, the barrel along the
// forearm, its top toward the thumb).
Place pistolPlace(const Pose& grip, unsigned hand, ControllerKind kind);
// A flying batarang (mesh_model::fit: span metres across, flat, its nose along the flight), spinning flat.
Place batarangFlying(Vec3 at, Vec3 way, float spin, Vec3 low, Vec3 high, float span = .16f);

// --- Throws -------------------------------------------------------------------------------------------------------
// The SM2 build's THOR throw (hero072_play.hpp): out at `speed` for at most `seconds` / `range` metres (or to the
// target), back at most `back` m/s slowing near the hand (`near` m/s a metre, at least `slowest`), turning at most
// `turn` rad/s until `straight` metres away, caught within `catchRange`; recalled anyway `seconds` + 4 s after the
// throw. returns false (the batarang): gone at the end of its flight.
struct Tuning {
    float speed, seconds, range, back, catchRange, near, slowest, turn, straight;
    bool returns;
};
inline constexpr Tuning hammerThrow{22.f, 1.f, 26.f, 24.f, .4f, 8.f, 6.f, 9.f, 2.5f, true};
inline constexpr Tuning shieldThrow{20.f, 1.f, 20.f, 20.f, .4f, 8.f, 6.f, 9.f, 2.5f, true};
inline constexpr Tuning batarangThrow{25.f, .9f, 20.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, false};
inline constexpr float hammerSpinPerMetre = .8f; // radians a metre flown (hero072_play spinPerMetre)

struct Flight {
    enum Phase : uint32_t { held = 0, out, back } phase = held;
    Vec3 from{}, pos{}, vel{}; // game world
    Vec3 target{};
    bool aimed = false; // flying to target (an enemy), not just along the aim
    float age = 0, travelled = 0, spin = 0;
    uint32_t throws = 0;
};
struct FlightStep {
    bool arrived = false; // reached its target this step (the hit lands now)
    bool turned = false;  // started back (or, not returning, ended)
    bool caught = false;  // back in the hand (or, not returning, gone)
};
// Out of the hand at `from` along `dir` (unit), to `target` when there is one. False (nothing changes) when it is
// not in the hand or the input is not finite.
bool launch(Flight& f, Vec3 from, Vec3 dir, const Vec3* target, const Tuning& t);
// One step of dt seconds. hand: where it is caught (game world). liveTarget: the enemy's position now (it homes on
// it while going out), null to keep the last one.
FlightStep step(Flight& f, const Tuning& t, Vec3 hand, const Vec3* liveTarget, float dt);

// --- Game <-> OpenXR (the head as the common point) -----------------------------------------------------------------
// The game head's rows (right, down, forward; game world, Y up) and position, with the headset's pose in his
// projection space: a game point is put on the game head's axes, then on the headset's. The game's world is in metres.
struct HeadMap {
    Vec3 gameHead{}, right{1, 0, 0}, down{0, -1, 0}, forward{0, 0, 1};
    Pose xrHead{};
    bool ok = false;
};
HeadMap headMap(Vec3 gameHead, Vec3 right, Vec3 down, Vec3 forward, const Pose& xrHead);
Vec3 toXr(const HeadMap& m, Vec3 gamePoint);
Vec3 toGame(const HeadMap& m, Vec3 xrPoint);
Vec3 dirToXr(const HeadMap& m, Vec3 gameDir);

// --- The GPU's side -----------------------------------------------------------------------------------------------
// One model corner, as the props' shader reads it (48 bytes).
struct GpuVertex {
    float position[3];
    float normal[3];
    float uv[2];
    float layer;     // the texture array's layer; -1 untextured; + cutoutLayer: alpha under .5 is cut
    float colour[3]; // linear: the material's tint (textured) or its colour (untextured)
};
static_assert(sizeof(GpuVertex) == 48, "the props' input layout");
inline constexpr float cutoutLayer = 1024.f;
// The model's corners in model space (normals: the model's own where it has them, else the face's). False with
// nothing added when the mesh is not usable.
bool pack(const mesh_model::Mesh& m, std::vector<GpuVertex>& out);
// The props' textures: textureSize x textureSize, textureLevels mips (VRAM: at most a few MB a layer).
inline constexpr uint32_t textureSize = 512, textureLevels = 10;

// One eye's view and projection (row-major, column vectors; D3D depth 0..1, near .02 m, far 300 m) from its OpenXR
// pose and fov angles (radians, left and down negative).
Mat4 eyeViewProjection(const Pose& eye, float angleLeft, float angleRight, float angleUp, float angleDown);
// The place as a 3x4 row-major matrix (rows: x, y, z of the world; the 4th column `at`), and its normals' 3x3
// (the inverse transpose; rows), as the shader's constants. False on a singular place.
bool modelMatrices(const Place& p, float model[12], float normal[9]);

} // namespace spidy::sm1::props
