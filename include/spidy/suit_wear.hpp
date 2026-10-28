#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
//
// Build 9 SUITS: a roster suit's real model (Models\Suits\Suit_<hero>_<n>, tools/suits/convert_suit.py) worn by
// the player: skinned to a 16-bone skeleton (the same as tools/suits/suit_skeleton.py) that follows the headset
// and the hands, drawn textured into the model sink (hero007::modelSink), in the mirror (reflected) and in first
// person (SUIT BODY: the arms, the whole body below the neck, or off).
//
// The skeleton: the 16 joints in native_body::SuitJoint's order, then the head's top and each hand's fingertips.
// A bone b runs from boneEnds[b][0] to boneEnds[b][1]; its frame: y along it, z its reference turned square to y
// (the head: its facing; the torso, the clavicles, the upper arms and the legs: the body's facing; the forearms:
// half the body's facing, half the hand's back; the hands: the hand's back), x = y x z. The same rule on the rest
// skeleton (the model's own) and on the live one, so a vertex keeps its place in its bones' frames:
//   p' = a1 + R1 diag(1, k, 1) R0^T (p - a0) s      (s the body's scale, k the bone's stretch, .7 to 1.4)
// blended by the converter's four weights (linear blend skinning).
#include "math.hpp"
#include "mesh_model.hpp"
#include "vertex.hpp"

#include <cstdint>
#include <vector>

namespace spidy::suit_wear {

inline constexpr uint32_t joints = 16, ends = 19, bones = 16;
enum End : uint32_t { head, neck, chest, pelvis, shoulderL, elbowL, wristL, shoulderR, elbowR, wristR,
                      hipL, kneeL, footL, hipR, kneeR, footR, headTop, tipL, tipR };
enum Bone : uint32_t { boneHead, boneNeck, boneChest, boneSpine, boneClavL, boneUpperL, boneForeL, boneHandL,
                       boneClavR, boneUpperR, boneForeR, boneHandR, boneThighL, boneShinL, boneThighR, boneShinR };
inline constexpr uint8_t boneEnds[bones][2] = {{head, headTop}, {neck, head}, {chest, neck}, {pelvis, chest},
                                               {neck, shoulderL}, {shoulderL, elbowL}, {elbowL, wristL}, {wristL, tipL},
                                               {neck, shoulderR}, {shoulderR, elbowR}, {elbowR, wristR}, {wristR, tipR},
                                               {hipL, kneeL}, {kneeL, footL}, {hipR, kneeR}, {kneeR, footR}};
// SUIT BODY (the first-person view; the mirror always shows the whole suit).
enum Part : uint32_t { partOff, partArms, partBody, parts };
inline constexpr const char* partNames[parts] = {"OFF", "ARMS", "FULL BODY"};
// The bones each part draws (bit b: bone b): ARMS the upper arms, forearms and hands; FULL BODY all but the
// head and the neck (the eyes are inside them).
inline constexpr uint32_t armBones = (1u << boneUpperL) | (1u << boneForeL) | (1u << boneHandL) | (1u << boneUpperR) |
                                     (1u << boneForeR) | (1u << boneHandR);
inline constexpr uint32_t bodyBones = 0xffffu & ~((1u << boneHead) | (1u << boneNeck));
inline constexpr uint32_t allBones = 0xffffu;
constexpr uint32_t partBones(uint32_t part) { return part == partArms ? armBones : part == partBody ? bodyBones : 0u; }

// SUIT BODY's setting (combat38::suitBody: 0 ARMS, the default, 1 FULL BODY, 2 OFF) as a Part.
constexpr uint32_t partOf(uint32_t setting) { return setting == 0 ? partArms : setting == 1 ? partBody : partOff; }

// Test build 10 ONE BODY (build 9 test: "all you see is the arms overlayed whatever your suit is, so its like two
// bodies in one"): a suit drawn in first person REPLACES the game's own hero body (BODY / GAME BODY) and the
// fallback gloves - never both. The render thread stamps the time it last drew the suit's SUIT BODY part; the
// body's command (the game thread) reads it: shown within shownFreshMs, the game's body is off (its arms with it).
// A suit that stops drawing (taken off, SUIT BODY OFF, a failed draw) gives the body back within shownFreshMs.
inline constexpr uint64_t shownFreshMs = 250;
constexpr bool shownNow(uint64_t nowMs, uint64_t shownAtMs) {
    return shownAtMs != 0 && nowMs >= shownAtMs && nowMs - shownAtMs < shownFreshMs;
}
// The game's own hero body is posed and drawn: BODY (or GAME BODY) wanted, a hero the game has a body for
// (hero_modes::gameBodyShown: not BATMAN), and no suit shown in its place.
constexpr bool gameBodyDrawn(bool bodyWanted, bool heroHasGameBody, bool suitShown) {
    return bodyWanted && heroHasGameBody && !suitShown;
}
// The fallback gloves (glove_overlay): never over a suit shown, never on the flat screen; GAME BODY's (the hero's
// body wanted but not drawn), and - Test build 10 BATMAN BODY - always for a hero with no game body (BATMAN) whose
// suit is not shown (its model missing or not tracked), so the player never loses his hands.
constexpr bool glovesShown(bool gameBodyOn, bool heroHasGameBody, bool flatScreen, bool bodyDrawn, bool suitShown) {
    return !flatScreen && !suitShown && (heroHasGameBody ? gameBodyOn && !bodyDrawn : true);
}
// SUIT BODY in first person: a hero with no game body (BATMAN) never has OFF - his suit's arms at least.
constexpr uint32_t firstPersonPart(uint32_t part, bool heroHasGameBody) {
    return !heroHasGameBody && part == partOff ? partArms : part;
}

// A skeleton: its ends, the head's facing and up, each hand's back (unit; left, right), and the body's up.
struct Skeleton {
    Vec3 at[ends]{};
    Vec3 face{0, 0, 1}, faceUp{0, 1, 0};
    Vec3 handBack[2]{{0, 1, 0}, {0, 1, 0}};
    Vec3 up{0, 1, 0};
    bool ok{};
};
// A worn suit made ready (once, at load): its unique vertices, each corner's vertex, each triangle's bone.
struct Rig {
    bool ok{};
    std::vector<Vec3> rest, restNormal;
    std::vector<uint8_t> joint, weight; // 4 a vertex
    std::vector<uint32_t> corner;       // 3 a triangle: into rest
    std::vector<uint8_t> triangleBone;  // the bone with the most of the triangle's weight
    Skeleton skeleton;                  // the rest skeleton (the model's: facing +z, its left at +x)
    float height = 1.8f;                // the head's top above the feet
};
bool makeRig(const mesh_model::Mesh& mesh, Rig& out);

// The tracked player (world): the head (the eyes' middle) and its axes, each grip (OpenXR: -z the fingers,
// x across the palm), the floor under the feet and up.
struct Tracked {
    Vec3 eye{}, eyeForward{0, 0, -1}, eyeUp{0, 1, 0};
    bool hand[2]{};
    Vec3 grip[2]{}, gripForward[2]{{0, 0, -1}, {0, 0, -1}}, gripX[2]{{1, 0, 0}, {1, 0, 0}};
    Vec3 feet{}, up{0, 1, 0};
    float standingEye = 0; // the player's standing eye height above the feet (0: this frame's, crouching shrinks him)
};
// The live skeleton for `rig` from the tracked poses: the head from the eyes, the torso the rest one turned to the
// head's level facing and scaled to the player's eye height, the arms reaching the wrists (two-bone IK, the elbows
// down and out; an untracked hand hangs at the side) and the legs standing on the floor (the knees forward).
bool estimate(const Rig& rig, const Tracked& t, Skeleton& out);

// Each bone's transform this frame (model -> world): R (row-major 3 x 3, with the stretch) and its normals' R.
struct BoneMove {
    float m[9]{}, n[9]{};
    Vec3 a0{}, a1{};
};
bool moves(const Rig& rig, const Skeleton& live, BoneMove out[bones]);
// Skins every vertex (positions and normals, world) once a frame; then draw() places triangles from it.
struct Skinned {
    std::vector<Vec3> p, n;
};
bool skin(const Rig& rig, const BoneMove m[bones], Skinned& out);
// Triangles of `mesh` whose bone is in `boneMask` into `sink` (hero007's model sink: shape = u, v, layer, 1),
// each point through `place` (a 3 x 4 affine map, row-major: the mirror's reflection; null: none). false (and
// nothing added) when it would pass `budget` vertices or came out insane.
bool draw(std::vector<Vertex>& sink, size_t budget, const mesh_model::Mesh& mesh, const Rig& rig, const Skinned& s,
          uint32_t boneMask, const float* place = nullptr);

} // namespace spidy::suit_wear
