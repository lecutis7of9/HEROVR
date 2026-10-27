#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
// Made by Agent Mulder.
//
// Version 0.0.72 (TEST) looks, in the overlay like 0.0.7's (soft_shapes.hpp: flat soft shapes
// turned to the eye, no depth test, no textures): THOR's hammer and lightning, GREEN LANTERN's
// constructs (ring glow, beam, bubble, chain, fist, the charge meter) and IRON MAN's helmet HUD
// (a light frame at the edges, the lock box, its lines). Every function draws nothing for a
// non-finite input and puts its vertices back if any came out insane; the counts are budgeted.
#include "spidy/math.hpp"
#include "vertex.hpp"

namespace spidy::mesh_model { struct Mesh; }

#include <cstddef>
#include <cstdint>
#include <vector>

namespace spidy::hero072 {

class Blood;
inline constexpr uint32_t boltsMost = 10, hudLines = 5;
struct Look {
    bool any{};
    // THOR: the hammer, `hammerAt` the handle's end, `hammerForward` handle -> head, `hammerUp` a face.
    bool hammer{};
    Vec3 hammerAt{}, hammerForward{0, 1, 0}, hammerUp{0, 0, -1};
    float hammerSpin{};
    // Version 0.0.72: Mjolnir is Models\Hammer (mesh_model::slotHammer) or NOTHING (no built-in stand-in;
    // the wrist says MODEL MISSING: Hammer). Library-owned; set on the render thread by game_xr.
    const mesh_model::Mesh* hammerModel{};
    // Lightning: each bolt from -> to, jagged by its seed.
    uint32_t bolts{};
    Vec3 boltFrom[boltsMost]{}, boltTo[boltsMost]{};
    float boltSeed[boltsMost]{};
    // GREEN LANTERN.
    bool ring[2]{};
    Vec3 ringAt[2]{};
    bool beam{};
    Vec3 beamFrom{}, beamTo{};
    bool bubble{};
    Vec3 bubbleAt{};
    float bubbleRadius{};
    bool chain{};
    Vec3 chainFrom{}, chainTo{};
    bool fist{};
    Vec3 fistAt{}, fistForward{0, 0, -1};
    float fistAlpha{};
    bool charge{};
    Vec3 chargeAt{}, chargeUp{0, 1, 0};
    float chargeLevel{}; // 0-1
    bool chargeEmpty{};
    // IRON MAN HUD: the eye, its forward and up (world); the lines and their alpha; the lock box.
    bool hud{};
    Vec3 hudEye{}, hudForward{0, 0, -1}, hudUp{0, 1, 0};
    char hudText[hudLines][40]{};
    float hudAlpha[hudLines]{};
    bool lock{};
    Vec3 lockAt{};
    // RAGE BLOOD: the drops and pools (not owned; drawn up to budget::blood).
    const Blood* blood{};
    // FIRE DOUSE: a spray (kind: 0 foam white, 1 frost, 2 green hose, 3 a gust of air), a shockwave
    // ring (the clap, the slam, a foam burst; age 0..1) and the PUNISHER's hook line.
    bool spray{};
    uint32_t sprayKind{};
    Vec3 sprayFrom{}, sprayDir{0, 0, -1};
    float sprayReach{}, sprayTime{};
    bool shock{};
    Vec3 shockAt{};
    float shockRadius{}, shockAge{};
    bool hook{};
    Vec3 hookFrom{}, hookTo{};
    // BATMAN's PERCH TAKEDOWN (9:31, 9:32): the light over the target (GREEN safe, RED seen) and the
    // rope of the swoop (a dark rope, never a web).
    bool perchLight{}, perchRed{};
    Vec3 perchAt{};
    bool rope{};
    Vec3 ropeFrom{}, ropeTo{};
};
// The Look moved from tracking space into the game's world by `worldFromTracking` (game_xr's
// saved frame). Every point and way turned; the blood is not copied (the game thread owns it).
// A non-finite pose leaves an empty Look (nothing drawn).
Look worldLook(const Look& l, const Pose& worldFromTracking);
void appendPerchLight(std::vector<Vertex>& out, Vec3 at, bool red, Vec3 viewer);
void appendRope(std::vector<Vertex>& out, Vec3 from, Vec3 to, Vec3 viewer, float pixelAngle);

void appendHammer(std::vector<Vertex>& out, Vec3 at, Vec3 forward, Vec3 up, Vec3 viewer, float pixelAngle);
void appendBolt(std::vector<Vertex>& out, Vec3 from, Vec3 to, float seed, Vec3 viewer, float pixelAngle);
void appendLanternBeam(std::vector<Vertex>& out, Vec3 from, Vec3 to, Vec3 viewer, float pixelAngle);
void appendBubble(std::vector<Vertex>& out, Vec3 at, float radius, Vec3 viewer);
void appendChain(std::vector<Vertex>& out, Vec3 from, Vec3 to, Vec3 viewer, float pixelAngle);
void appendFist(std::vector<Vertex>& out, Vec3 at, Vec3 forward, float alpha, Vec3 viewer, float pixelAngle);
void appendCharge(std::vector<Vertex>& out, Vec3 at, Vec3 up, float level, bool empty, Vec3 viewer);
void appendIronHud(std::vector<Vertex>& out, const Look& l, Vec3 viewer);
void appendBlood(std::vector<Vertex>& out, const Blood& b, Vec3 viewer, float pixelAngle);
void appendSpray(std::vector<Vertex>& out, uint32_t kind, Vec3 from, Vec3 dir, float reach, float time, Vec3 viewer,
                 float pixelAngle);
void appendShock(std::vector<Vertex>& out, Vec3 at, float radius, float age, Vec3 viewer);
void appendHook(std::vector<Vertex>& out, Vec3 from, Vec3 to, Vec3 viewer, float pixelAngle);
// Everything in `l`, moved by `travel`.
void appendLook(std::vector<Vertex>& out, const Look& l, Vec3 travel, Vec3 viewer, float pixelAngle);

namespace budget {
inline constexpr size_t hammer = 1200, bolt = 8 * 54 * 2, beam = 900, bubble = 1500, chain = 1700, fist = 1500,
                        charge = 2600, ring = 200, hud = 9000;
// RAGE BLOOD: drops are one soft bar each (54), pools a soft disc (10 segments) - drawn until full.
inline constexpr size_t blood = 16000, spray = 2600, shock = 1200, hook = 1200, perch = 900, rope = 400;
inline constexpr size_t frame = hammer + boltsMost * bolt + beam + bubble + chain + fist + charge + 2 * ring + hud +
                                blood + spray + shock + hook + perch + rope;
} // namespace budget
} // namespace spidy::hero072
