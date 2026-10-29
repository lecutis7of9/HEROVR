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

#include <cmath>
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
// SUIT BODY in first person. With BODY (or GAME BODY) wanted the suit stands in for the whole game body, so ARMS
// grows to FULL BODY (the player keeps his legs, now the suit's). A hero with no game body (BATMAN) never has OFF:
// his suit's arms at least (its full body with BODY wanted).
// Test build 11 (bodies first): a hero with no game body (BATMAN) ALWAYS shows his suit's FULL BODY - BODY on or
// off, any SUIT BODY setting - so he is never arms only ("batman ... doesnt have abody at all").
constexpr uint32_t firstPersonPart(uint32_t part, bool heroHasGameBody, bool bodyWanted) {
    if (!heroHasGameBody)
        return partBody;
    if (part == partOff)
        return partOff;
    return bodyWanted ? partBody : part;
}

// A skeleton: its ends, the head's facing and up, each hand's back (unit; left, right), and the body's up.
struct Skeleton {
    Vec3 at[ends]{};
    Vec3 face{0, 0, 1}, faceUp{0, 1, 0};
    Vec3 handBack[2]{{0, 1, 0}, {0, 1, 0}};
    Vec3 up{0, 1, 0};
    float curl[2]{}; // Test build 12 FIST: each hand's fingers curled (0 open, 1 a fist round a handle)
    bool fistFit[2]{}; // Version 0.0.73 test 3 FIST: Tracked::fistFit (and the hand tracked)
    bool heroHands{};  // 0.0.73 HERO HANDS: Tracked::heroHands (moves() takes the rig's own HandProfile)
    bool open[2]{};    // 0.0.73 HERO HANDS: the profile's open hand (Tracked::open) held this frame
    float foreFollow{}; // 0.0.73 HERO HANDS: the profile's forearmFollow (0: the shared forearm), frame() reads it
    bool ok{};
};
// 0.0.73 HERO HANDS (test 4: "i feel like you are applying one fix to every heros hand but every hero works
// differently"). Until test 4 every suit's hands went down ONE path (restHands' votes, handWay / handBackOf,
// handReach, fistRadians, the curl from the trigger / grip): Iron Man's gauntlet included. A hand profile is one
// hero's (one suit's) own: its rest hand's back fixed (the votes' guess turned about the hand's own way), the worn
// hand turned on the controller (roll about the hand's way, pitch at the wrist), its knuckles, fingers and curl,
// and what the trigger does to it. The shared profile is the old path exactly (every field neutral), so every
// suit with no profile of its own (Daredevil's baton fist among them) is unchanged; HERO HANDS OFF is the shared
// profile for every suit.
struct HandProfile {
    const char* name = "shared";
    float restTurnDeg = 0;   // the rest hand's back turned about its wrist -> fingertips way (the right hand's;
                             // the left mirrored): the votes' back fixed where they read the gauntlet wrong
    float rollDeg = 0;       // the worn hand rolled about its way (+: the right hand's back toward its thumb)
    float pitchDeg = 0;      // the worn hand bent at the wrist (+: the back of the hand up toward the forearm)
    float fistRadians = 3.3f;                                       // suit_wear::fistRadians
    float knuckleShare = .5f, knuckleMax = .1f, fingersMax = .095f; // suit_wear::knuckleShare ...
    float curlMax = 1.f;       // the most the fingers curl from the trigger / grip (a held prop still closes them)
    bool triggerOpens = false; // the trigger held to fire (IRON MAN's repulsor charging) opens the hand flat
    float openPitchDeg = 0;    // and while open so, the wrist bent back this much more (the palm out ahead)
    float forearmFollow = 0;   // the forearm's share of the hand's roll past the shared half (0: the shared path,
                               // the facing and the hand's back averaged; 1: the hand's back alone) - no pinch
                               // at the wrist when the palm turns out ahead (the back toward the player)
    bool palmRaise = false;    // IRON MAN: the raised hand (or the repulsor charging / just fired) drawn palm out
                               // along the blast (raisedPalm, blendGrip; game_xr)
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
    float handThumb[2]{};               // Test build 12: each rest hand's thumb found (+-1 across it; 0 none)
    // Version 0.0.73 test 3 FIST ("the fingers dont wrap the batons"): each rest hand's fingers as a line in the
    // hand bone's frame past the knuckles - their depth toward the back (z) at the knuckles and its change along
    // them (rest metres; 0 0: straight down the bone). A suit whose rest fingers sit under the bone or droop
    // (Daredevil's: 3 cm under it, bent 25 degrees) curled about the bone threw the fingertips out through the back
    // of the fist; now each finger is straightened off its own line and curled round a handle under it.
    float fingerDepth[2]{}, fingerSlope[2]{};
    // and the handle's hole in that fist (fully curled), from the curl's centre (knuckle, fingerDepth - radius) in
    // the bone's (y, z) (rest metres): where a 36 mm handle shows least through the skin and is wrapped all round.
    float fistHole[2][2]{};
    // Test build 12 CAPE (findCape): the suit's own cape - the sheet hanging behind its back (the triangles
    // reached from the ones well behind it, below the shoulders), only when there is one (Arkham Origins' and
    // Year One's BATMAN; Beyond's and 1989's have none in their models). Per rest vertex: how much of it hangs
    // from the torso (-1 not the cape, 0 none .. 1: the cape's lower part, never dragged about by the arms it was weighted to),
    // and where it sits on the cape: across (-1 its right edge .. +1 its left), down (0 the shoulders .. 1 the
    // hem) and its fold (the rest depth off the cape's mean at that height: kept, so it is never flattened).
    bool hasCape{};
    std::vector<float> capeHang;
    std::vector<Vec3> capeAt; // x across, y down, z fold
    uint32_t capeTriangles{};
    // 0.0.73 HERO HANDS: this suit's own hand profile (handProfile(name) at makeRig; applyHandProfile) and its
    // rest hands' backs with the profile's restTurnDeg (skeleton.handBack when it has none).
    HandProfile hands;
    Vec3 profileBack[2]{{0, 1, 0}, {0, 1, 0}};
    // 0.0.73 test 5 CLAW BONE (findClawRoots, at makeRig and applyHandProfile, for a Wolverine suit: clawSuit):
    // each rest hand's three claw roots (rest model space, in the hand bone's rigid part behind the knuckles): between
    // index/middle, middle/ring and ring/pinky (in the profile's hand frame: x across, y wrist -> knuckles, z the
    // back), at least clawDepthMin under the skin with the claw's root end inside the fist; the claws' way (out over
    // the knuckles, along the punch) and back.
    bool clawSuit = false;
    bool clawOk[2]{};
    Vec3 clawRoot[2][3]{};
    Vec3 clawWay[2]{}, clawBack[2]{};
    // 0.0.73 test 6 CLAW LEVEL: the claws' way level with the hand (along the hand bone), same roots
    Vec3 clawWayLevel[2]{}, clawBackLevel[2]{};
    float clawKnuckle[2]{}; // the knuckle line (rest metres from the wrist along the hand bone)
};
// Test build 12 CAPE: the cape's thresholds (metres at a 1.8 m suit).
inline constexpr float capeSeedBehind = .18f;   // a triangle this far behind the suit's middle starts the cape
inline constexpr float capeKeepBehind = .10f;   // and the cape spreads over its neighbours this far behind
inline constexpr float capeMinArea = .25f;      // m^2: less is back armour, not a cape (Beyond's .01)
inline constexpr float capeReachDown = .25f;    // the cape reaches under this share of the height (Beyond: no)
void findCape(const mesh_model::Mesh& mesh, Rig& rig);
bool makeRig(const mesh_model::Mesh& mesh, Rig& out, const char* name = nullptr); // name: the suit's folder (Suit_...)

// The tracked player (world): the head (the eyes' middle) and its axes, each grip (OpenXR: -z the fingers,
// x across the palm), the floor under the feet and up.
struct Tracked {
    Vec3 eye{}, eyeForward{0, 0, -1}, eyeUp{0, 1, 0};
    bool hand[2]{};
    Vec3 grip[2]{}, gripForward[2]{{0, 0, -1}, {0, 0, -1}}, gripX[2]{{1, 0, 0}, {1, 0, 0}};
    // Test build 12 HANDS: the grip's +Y and how far the controller's handle tilts the forearm off it (toward +Z:
    // baton_grip::handleTiltDeg, Touch 30 CONFIRMED); each hand's fingers curled (0 open .. 1 a fist: the trigger,
    // the grip, or a held prop).
    Vec3 gripUp[2]{{0, 1, 0}, {0, 1, 0}};
    float handleTiltDeg[2]{30.f, 30.f};
    float curl[2]{};
    // Version 0.0.73 test 3 FIST: the hand fits its fist round a held Daredevil baton (Rig::fingerDepth, fistHole);
    // false: Version 0.0.73 test 3's fist exactly (the dd_baton_tip switch OFF)
    bool fistFit[2]{};
    // 0.0.73 HERO HANDS: the rig's own HandProfile (HERO HANDS ON, the game's default); false: the shared path.
    // open: the profile's open hand (handOpen: IRON MAN's repulsor) this frame, per hand.
    bool heroHands{};
    bool open[2]{};
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
    // Test build 12 FIST (the hands only): the rest bone's axes, where its knuckles are and how far its fingers
    // reach (rest metres from a0), and the curl's angle (radians, 0: none).
    Vec3 x0{}, y0{}, z0{};
    float knuckle{}, fingers{}, curl{};
    float depth{}, slope{}; // Version 0.0.73 test 3 FIST: Rig::fingerDepth, fingerSlope (the hands only)
};
// Test build 12 HANDS. The hand bone's way (wrist -> knuckles) and the wrist from an OpenXR grip pose:
// the grip's -Y turned tiltDeg toward its -Z (the forearm through a fist round a controller's handle; the grip's
// -Z runs through that fist from the little finger to the thumb, NOT along the fingers - build 9 to 11 laid the
// hand along -Z: fingers up the handle, "the hand ... doesnt match ... my IRL hand"). The back of the hand: the
// grip's -X on the left hand, +X on the right (+X points right on both: OpenXR "away from the palm in the left
// hand, into the palm in the right hand").
inline constexpr float fistRadians = 3.3f;  // the fingers' whole curl round a handle (about 190 degrees)
inline constexpr float knuckleShare = .5f;  // the knuckles half way down the hand bone (at most knuckleMax)
inline constexpr float knuckleMax = .1f;    // m at 1.8 m
inline constexpr float fingersMax = .095f;  // m at 1.8 m
Vec3 handWay(Vec3 gripForward, Vec3 gripUp, float tiltDeg);
Vec3 handBackOf(Vec3 gripX, int side);
// 0.0.73 HERO HANDS: the shared profile (every field the old path's) and a suit folder's own (the shared one for
// a suit with none). applyHandProfile: the rig's profile and its turned rest backs (makeRig does it by name).
const HandProfile& handProfileShared();
const HandProfile& handProfile(const char* suitFolder);
void applyHandProfile(Rig& rig, const HandProfile& p);
// The profile in use: the rig's own with HERO HANDS on, else the shared one.
inline const HandProfile& handsOf(const Rig& rig, bool heroHands) { return heroHands ? rig.hands : handProfileShared(); }
// Whether the profile's open hand holds: triggerOpens, the repulsor charging or in a blast's pose, no prop held
// and the grip not squeezed into a fist.
bool handOpen(const HandProfile& p, float grip, bool propHeld, bool repulsorPose);
// The fingers' curl wanted (0 open .. 1 a fist) from the trigger, the grip and the wall grip (0 .. 1): the shared
// profile the most of them (test 4's exactly); a held prop (a baton, the hammer) always 1; the open hand 0;
// curlMax caps the rest.
float handCurl(const HandProfile& p, float trigger, float grip, float wallGrip, bool propHeld, bool repulsorPose);
// 0.0.73 HERO HANDS (IRON MAN): the hand raised palm out - the real palm's normal within about 55 degrees of
// the level look ahead and the grip no lower than 40 cm under the eyes (a hand held at the side or down: no).
bool raisedPalm(Vec3 palmNormal, Vec3 eyeForward, Vec3 up, Vec3 grip, Vec3 eye);
// A weight eased toward want (0 .. 1) over about `seconds` (dt the frame's; insane input: want).
float easeToward(float now, float want, float dt, float seconds);
// The grip's frame (forward, x, up, the handle tilt) turned share w (0 .. 1) toward a target frame (tf, tx, tu,
// tilt 0): x and up blended and squared, forward from them. false (the frame unchanged) on insane input.
bool blendGrip(Vec3& forward, Vec3& x, Vec3& up, float& tiltDeg, Vec3 tf, Vec3 tx, Vec3 tu, float w);
bool moves(const Rig& rig, const Skeleton& live, BoneMove out[bones]);
// 0.0.73 test 5 CLAW BONE (the owner after the CLAW LOOK previews: "none of the wolverine claw things actually looks
// like it comes out of the hand, looks like they all float around it"). The claws locked to the HAND BONE (not the
// controller): Rig::clawRoot through this frame's hand move (BoneMove, the rigid part of the hand: never curled),
// so they move with the drawn fist and stay in it as the wrist turns. CANDIDATE (at 1.8 m, scaled by the rig's
// height; measured on CLASSIC, JACKET AND JEANS and WEAPON X in apps/hero_hands_preview HANDS_CLAWS=bone):
//   clawBehind - the roots this far behind the knuckle line (the blade crosses the skin at the knuckles)
//   clawSink   - the roots this far under the back of the hand (the claw's root end, 2.7 cm tall, inside)
//   clawTiltDeg..clawTiltMaxDeg - the claws' way tilted from the hand bone toward its back so each blade leaves the
//                  skin clawExitDrop under the top of the knuckle row (out over the knuckles, not through the fingers)
//   clawDepthMin - each root at least this far under the rest skin where it can be (the skinned hand strays a few
//                  mm from its bone near the wrist)
inline constexpr float clawBehind = .02f, clawSink = .02f, clawTiltDeg = 8.f, clawTiltMaxDeg = 30.f, clawExitDrop = .005f,
                       clawDepthMin = .006f;
void findClawRoots(Rig& rig);
// 0.0.73 test 6 CLAW LEVEL (the owner after test 5: "the angle on the attack end was way too high, it should be
// more level with the hand"): the claws' way tilted clawLevelDeg from the hand bone (0: along it, level with the
// back of the hand) instead of clawTiltDeg..clawTiltMaxDeg; the roots stay where findClawRoots put them.
inline constexpr float clawLevelDeg = 0.f;
// The claws' way tilted `tiltRad` from the hand bone `y0` toward the back of the hand `z0` (both unit, square).
inline Vec3 clawTiltWay(Vec3 y0, Vec3 z0, float tiltRad) {
    return normalized(y0 * std::cos(tiltRad) + z0 * std::sin(tiltRad));
}
// The claw roots (world, ascending across the hand), the claws' way and the back of the hand this frame; false
// (nothing) without them or on insane moves.
// level (CLAW LEVEL ON): the level way (Rig::clawWayLevel) instead of the tilted one.
bool clawPlace(const Rig& rig, const BoneMove m[bones], int side, Vec3 root[3], Vec3& way, Vec3& back,
               bool level = false);
// Skins every vertex (positions and normals, world) once a frame; then draw() places triangles from it.
struct Skinned {
    std::vector<Vec3> p, n;
};
bool skin(const Rig& rig, const BoneMove m[bones], Skinned& out);
// Test build 12 CAPE: the cape on the skinned suit (after skin, nothing without one). Its lower part hangs
// from the torso (the chest bone), not from the arms its weights follow; `spread` (0 .. 1, BATMAN's cape glide
// eased in and out) fans it out into a wing on each side - across from the neck along the arm to the wrist
// (the top edge), down to the knee (the outer edge), a hand's width behind the arms and billowing - its own
// mesh and folds kept, so it is seen in first person from the arms out to the sides.
bool cape(const Rig& rig, const BoneMove m[bones], const Skeleton& live, float spread, Skinned& s);
// A point on the glide's wing: across a (0 the spine .. 1 the outer edge), down t (0 top .. 1 hem), side 0 the
// suit's left (+x at rest), 1 its right; and the wing's normal there.
Vec3 capeWing(const Skeleton& live, float a, float t, int side, float scale);
// Triangles of `mesh` whose bone is in `boneMask` into `sink` (hero007's model sink: shape = u, v, layer, 1),
// each point through `place` (a 3 x 4 affine map, row-major: the mirror's reflection; null: none). false (and
// nothing added) when it would pass `budget` vertices or came out insane.
bool draw(std::vector<Vertex>& sink, size_t budget, const mesh_model::Mesh& mesh, const Rig& rig, const Skinned& s,
          uint32_t boneMask, const float* place = nullptr);

} // namespace spidy::suit_wear
