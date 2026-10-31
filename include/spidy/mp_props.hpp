#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
// Made by Agent Mulder.
//
// 0.0.74 MP PROPS (owner pt1360u: "we cant have multiplayer and not see your friend throw a batarand or thors
// hammer or whatever"): the 'P' PROPS tail of a POSE packet - what the player holds, wears and throws, and the
// lines and flashes of his powers - so PLAYER 2 carries them too. Pure (no game, no sockets): mp_net writes it
// after the LOOK tail, mp_prop_look.hpp turns the mod's own hero007 / hero072 state into it and back.
//
// Still protocol version 1: the tail is written only when there is something in it (so every pose without props
// - and the golden packets - are byte-for-byte unchanged), and every older build reads a longer pose and ignores
// the bytes it does not know. A tail that does not parse is dropped (the pose still counts).
//
// The tail (at payload offset 136, after LOOK; little-endian):
//   8-byte header: 'P', props version 1, items, lines, effects, flags (1 claw steel, 2 baton club, 4 gun models,
//   8 shield model, 16 baton model - what the sender draws, so the ghost draws the same), 0, 0
//   items   (<= 8, 24 bytes each): id, kind (PropKind), flags (holster bits 0-2, side 3, palm 4, in hand 5,
//           sheathed 6, flying 7), variant (batarang style / hammer choice / a fist's or charge's 0-255),
//           spin i16 (1/4096: a batarang's turn, the claws' extension, a bubble's radius m), at i16 x3 (cm),
//           forward i16 x3, up i16 x3 (unit x 32767)
//   lines   (<= 5, 14 bytes each): kind (PropLineKind), fade (0-255), from i16 x3, to i16 x3 (cm)
//   effects (<= 4, 18 bytes each): seq u16 (the receiver shows each once), kind (PropFxKind), age (4 ms units),
//           at i16 x3 (cm), way i16 x3 (unit x 32767), param (a shock's radius in 10 cm), 0
// Every position is in the sender's yawed root frame from his feet, like the head and grips (so +-327 m; a
// prop farther than that is not sent). Largest: 8 + 8*24 + 5*14 + 4*18 = 342 bytes: a pose with every prop is
// 24 + 136 + 342 = 502 bytes, under maxPacket (512).
//
// Everything here is mod-internal (CONFIRMED: the mod's own props and effects); no game address is read.
#include "spidy/math.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace spidy::mp {
inline constexpr uint8_t propsTag = 'P', propsVersion = 1;
inline constexpr size_t propItemsMost = 8, propLinesMost = 5, propFxMost = 4;
inline constexpr size_t propsHeaderBytes = 8, propItemBytes = 24, propLineBytes = 14, propFxBytes = 18;
inline constexpr size_t propsMostBytes =
    propsHeaderBytes + propItemsMost * propItemBytes + propLinesMost * propLineBytes + propFxMost * propFxBytes;
inline constexpr float propReach = 327.f; // metres from the feet a prop may be (i16 cm)

// Items: 1-11 are hero007::Look's values (pistol .. grapple gun); then the 0.0.72 props and Daredevil's batons.
enum PropKind : uint8_t {
    propNone = 0,
    propLookLast = 11, // hero007::lookGrappleGun
    propHammer = 20,   // THOR: at the handle's end, forward handle -> head, up a face
    propBaton = 21,    // DAREDEVIL: at the grip, forward to the tip (spin: the length m), side = which baton
    propPellet = 22,   // BATMAN's smoke pellet (side = which hand)
    propRing = 23,     // GREEN LANTERN's ring glow (side = which hand)
    propBubble = 24,   // GREEN LANTERN's bubble (spin: radius m)
    propFist = 25,     // GREEN LANTERN's fist (forward; variant: alpha)
    propCharge = 26,   // GREEN LANTERN's charge (up; variant: level, flag palm: empty)
    propPalm = 27,     // IRON MAN's palm glow (forward the palm's way; variant: glow)
    propKindEnd
};
inline constexpr uint8_t propFlagHolster = 7, propFlagSide = 8, propFlagPalm = 16, propFlagInHand = 32, propFlagSheathed = 64,
                         propFlagFlying = 128;
enum PropLineKind : uint8_t {
    lineHeat = 1,  // SUPERMAN's heat beam (from = between the eyes)
    lineFreeze,    // his cold breath
    lineGrapple,   // BATMAN's grapple line
    lineCable,     // DAREDEVIL's chain to a thrown baton
    lineBolt,      // THOR's lightning
    lineLantern,   // GREEN LANTERN's beam
    lineChain,     // GREEN LANTERN's chain
    lineRepulsor,  // IRON MAN's repulsor beam / bolt
    lineChest,     // IRON MAN's chest beam
    lineHook,      // PUNISHER's hook line
    lineRope,      // BATMAN's perch rope
    lineKindEnd
};
enum PropFxKind : uint8_t {
    fxFlash = 1,   // a muzzle flash (way: along the barrel)
    fxFlashShotgun,
    fxImpact,      // a bullet's hit puff (way: back along the shot)
    fxSmoke,       // a smoke puff
    fxScorch,      // a heat-vision scorch mark
    fxSplash,      // a repulsor's splash
    fxShock,       // a shockwave ring (param: radius / 10 cm)
    fxKindEnd
};
inline constexpr uint8_t propsFlagClawSteel = 1, propsFlagBatonClub = 2, propsFlagGunModels = 4, propsFlagShieldModel = 8,
                          propsFlagBatonModel = 16; // the sender draws those as the models (the receiver too, if it has them)

struct PropItem {
    uint8_t id{}, kind{}, flags{}, variant{};
    float spin{};
    Vec3 at{}, forward{0, 0, -1}, up{0, 1, 0};
};
struct PropLine {
    uint8_t kind{};
    float fade = 1;
    Vec3 from{}, to{};
};
struct PropFx {
    uint16_t seq{};
    uint8_t kind{}, param{};
    float age{};
    Vec3 at{}, way{0, 0, -1};
};
struct Props {
    uint8_t flags{};
    uint8_t items{}, lines{}, fx{};
    std::array<PropItem, propItemsMost> item{};
    std::array<PropLine, propLinesMost> line{};
    std::array<PropFx, propFxMost> effect{};
    bool any() const { return items || lines || fx; }
    // Adds when there is room and every point is finite and within propReach (false: not added).
    bool add(const PropItem& it);
    bool add(const PropLine& l);
    bool add(const PropFx& f);
};

// The tail's size (0 when !any()).
size_t propsBytes(const Props& p);
// Writes the tail; the bytes written (0: nothing to write or no room).
size_t encodeProps(const Props& p, uint8_t* out, size_t capacity);
// Reads a tail (`size` bytes from its 'P'); false (out emptied) when it is not one this build reads.
bool decodeProps(const uint8_t* data, size_t size, Props& out);
// 0.0.74 MP PROPS phase 2 (thrown items by state, 30 Hz): the props between two received poses. b's set, each
// item that a has too (same id and kind, both in or both out of the hand, under propSnap apart) moved part way -
// `at` lerped, its ways nlerped, an angle spin the short way round (a length spin - batons, claws, the bubble -
// lerped); lines matched by kind likewise; effects are b's (each is a one-off with its own age).
inline constexpr float propSnap = 4.f; // m: farther in one step is a new throw or a catch (no slide)
Props blendProps(const Props& a, const Props& b, float t);
// The props as the receiver gets them back (each value through its wire resolution) - for the tests.
Props quantized(const Props& p);
} // namespace spidy::mp
