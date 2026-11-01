#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
// Made by Agent Mulder.
//
// 0.0.76 MP WORLD (owner pt1556u: "well find the address this needs to happen" - the friend seen where he
// really is in the city, not 3 m ahead): pure placement of PLAYER 2 at his real world spot.
//
// The source (CONFIRMED by working code and a live read): the local player actor's transform - the
// component registry's HeroLocal record -> its 4x4 transform, translation at +0x30 (game_player.cpp,
// game_bridge.cpp, world_trace.cpp). The VR rig already stands on it every frame (game_xr: body[12..14] ->
// rig.update -> rendered.anchor), webs and enemy tracking line up with the city through it, and the
// 0.0.75 world trace report (31 Oct) read it live (pedestrians 93 m away, a mission 2.6 km away). The overlay
// draws in that same world space (rendered.anchor + travel), so the sender's feet go on the wire as they are
// (PoseState::root, sent since 0.0.73) and the 'W' tail (mp_net.hpp) adds his world facing and an "in the
// city" flag. CANDIDATE (needs the two-player test): both games use the same absolute city coordinates (no
// per-session origin shift was ever seen; the trace's positions are absolute map values like 2250, -14, -1354).
//
// Facing: the eye's level facing in the overlay's world frame (mod-internal, the rendered head pose) - the
// actor matrix's forward row is not used (its axis and sign are CANDIDATE).
//
// live.txt mp_place (combat38::mpPlace): WORLD (0, the default) - a friend's game whose pose carries the
// WORLD tail stands at his real spot; BUDDY (1) - the 0.0.74 spot 3 m ahead. A friend on an older build, the
// Test Buddy, the loopback, or a friend not in the city (menu, loading): always the buddy spot.
#include "spidy/math.hpp"
#include "spidy/mp_net.hpp"
#include "spidy/vertex.hpp"

#include <string>
#include <vector>

namespace spidy::mp {
enum Place : uint32_t { placeWorld = 0, placeBuddy = 1 };

// Draw him at his real spot: WORLD chosen, a friend's game (not the loopback or Test Buddy) and his pose says
// where he is in the city. Anything else: the buddy spot (0.0.74).
bool useWorld(uint32_t place, bool peerGame, bool buddy, const PoseState& s);

// Distances for the far look (metres).
struct WorldTune {
    float bodyFull = 150.f;  // the body drawn fully out to here,
    float bodyGone = 250.f;  // fading to nothing here (beyond: only his tag)
    float tagNear = 6.f;     // farther than this the tag is drawn at tagRange along the line to him,
    float tagRange = 6.f;    // so it reads at any distance (the nameplate is its biggest there)
    float arrowCone = .6f;   // radians off the view's centre before the off-screen arrow shows
    float arrowAhead = 1.4f; // metres in front of the eyes
    float arrowOut = .32f;   // metres from the view's centre toward him
};

// The body's alpha for a friend `distance` away (1 near, 0 past bodyGone; NaN: 0).
float bodyAlpha(float distance, const WorldTune& t = {});
// "230 M", "1.2 KM" (NaN or negative: "--").
std::string distanceText(float metres);
// The nameplate with his distance: "PETER  PING 42 MS  230 M".
std::string worldNameplate(const std::string& base, float metres);

// Where his tag goes: above his head when near, else tagRange along the line from the viewer to it.
Vec3 tagSpot(Vec3 viewer, Vec3 above, const WorldTune& t = {});

// The off-screen arrow: shown when he is more than arrowCone off where the viewer looks.
struct Arrow {
    bool show{};
    Vec3 at{};  // its centre (world)
    Vec3 way{}; // the way it points (unit, across the view)
    Vec3 face{}; // its normal (toward the eyes)
};
Arrow offscreenArrow(Vec3 viewer, Vec3 forward, Vec3 up, Vec3 target, const WorldTune& t = {});
// A soft chevron for `a` (size metres across) in `color`; the vertices added.
size_t appendArrow(std::vector<Vertex>& out, const Arrow& a, float size, Vec3 color, float alpha);
} // namespace spidy::mp
