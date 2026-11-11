#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
// Made by Agent Mulder.
//
// ULTIMATE ALLIANCE EXPANSION (M1, CANDIDATE): Marvel: Ultimate Alliance places walked in VR, on foot, from the
// wrist's own page (ULTIMATE ALLIANCE EXPANSION). Each place is a hideout074 room - the mod draws it, the game's
// picture hidden behind its black box, the left stick walks, the right snap-turns - built by the player from his
// OWN MUA1 PS2 disc (tools/ua/convert_ua.py; nothing of the game is shipped) into Models\UA\<Place>. While he is
// there the game is sent a pad at rest and no hero tool answers a hand, so the hero stands where he was left;
// RETURN TO CITY (the gold ring, the wrist button, B held or the page's button again) puts him back exactly there.
// npcs.txt beside the room lists where the place's characters stand (read now, drawn by the next update).
// Pure: no game reads, no addresses. Fail-safe: anything wrong and he never leaves the city (or is put back).
#include "spidy/hideout074.hpp"
#include "spidy/math.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace spidy::ua_expansion {

inline constexpr const char* pageTitle = "ULTIMATE ALLIANCE EXPANSION";
// The wrist option's one-sentence help (combat38 UA LOCATIONS; ON is value 0, the default).
inline constexpr const char* help = "UA places from your own disc on the wrist's UA page, needs a restart.";

enum Location : uint32_t { starkTower, locations };
inline constexpr const char* names[locations] = {"STARK TOWER"};
// Under Models\ (mesh_model::slotUaStarkTower's folder, hideout074::folders' too).
inline constexpr const char* folders[locations] = {"UA/StarkTower"};
inline constexpr hideout074::Which which[locations] = {hideout074::Which::uaStarkTower};

// On foot the real floor is not read from the game: the room's floor goes this far under the eye as he leaves.
inline constexpr float eyeHeight = 1.65f;
// B held this long goes back to the city (the hideouts' hold).
inline constexpr float exitHold = .8f;

// Why a trip cannot start now (nullptr: it can). The first reason that holds, in this order.
struct Gate {
    bool optionOn{};      // UA LOCATIONS
    bool modelOk{};       // the place's scene.glb loaded
    bool roomOk{};        // its room.txt read
    bool texturesReady{}; // the models' textures on the GPU
    bool flatScreen{};    // played on the monitor (nothing to walk)
    bool headTracked{};   // a head pose this frame
    bool inVehicle{};     // in, getting in or out of the van / Batmobile / bike
    bool hideoutActive{}; // already in the safehouse or the Batcave
    bool airborne{};      // swinging, falling, flying
};
const char* refuse(const Gate& gate);

// The page's row for a place: what pressing it does now.
enum class Row : uint8_t { ready, inside, off, missing };
Row rowOf(bool optionOn, bool modelOk, bool roomOk, bool inside);
const char* rowValue(Row row); // ENTER, RETURN TO CITY, OFF, NOT CONVERTED
inline constexpr const char* missingHelp =
    "Not converted yet: run tools/ua/convert_ua.py on your own MUA disc (docs/UA-EXPANSION.md), then restart.";

// The next update's hooks: where the place's characters stand (npcs.txt, written by the converter from the
// map's own spawners). Room (glTF) space: metres, the room's floor at y 0 under the spawn; yaw in degrees the
// spawn's sense (about +y from +x counter-clockwise).
struct Npc {
    std::string id; // the map's character id (e.g. "nickfurynpc01")
    Vec3 at{};
    float yawDeg{};
};
inline constexpr size_t mostNpcs = 256;
// npcs.txt's text ("HEROVR-UA-NPCS 1" then "npc <id> x y z yaw" lines; # comments). False (error said, out
// empty) on anything wrong.
bool parseNpcs(const std::string& text, std::vector<Npc>& out, std::string& error);
// A readable name for an id ("nickfurynpc01" -> "NICK FURY") for labels and logs.
std::string npcLabel(const std::string& id);

} // namespace spidy::ua_expansion
