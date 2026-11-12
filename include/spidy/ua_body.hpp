#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
//
// ULTIMATE ALLIANCE EXPANSION fix 2 - THE PLAYER'S BODY IN A UA PLACE (owner: no body or hands in Stark Tower).
// In a UA place the game's picture is hidden (its black box) and the hero frozen in the street, so the game's own
// body (Spider-Man's: no suit model is worn - wornSuitSlot 0) is never seen: the player had no body at all. Now the
// player wears an AVATAR there: a skinned body following the headset and both controllers (suit_wear: the head, the
// two-bone arms to the grips, the fingers curling with the trigger and grip, the legs stepping with the walk), standing
// on the room's floor under the eyes.
//
// Which body (pick): the hero's own worn suit when one is (the roster's - IRON MAN, CAP...), else (v6) a stand-in roster suit
// (standInSlot), else the player's own SM2 Spider-Man, the game's, shown through the room (ua_body_cut.hpp), else nothing. The MUA1 Spider-Man avatar of v2 / v3
// (Models\UA\Body) and the fallback gloves are gone: the owner wants only the real SM2 body.
//
// LATER (combat and powers): the avatar is one function of a Drive - the tracked poses plus what the hero is doing.
// Today only the tracked poses and the fingers; a power (a web shot, a punch, a repulsor) would set the hand's pose or
// curl in its Drive (suit_wear::Tracked's palm / curl / open fields, as IRON MAN's repulsor already does outside),
// with no change to how the body is chosen or placed. Pure (no Windows, no D3D): the tests check it.
#include "math.hpp"
#include "mesh_model.hpp"
#include "suit_wear.hpp"

#include <cmath>
#include <cstdint>

namespace spidy::ua_body {

// Models\UA\Body (mesh_model::slotUaBody's folder). RETIRED in v4: MUA1's own Spider-Man is no longer a body anywhere
// (owner: only the real SM2 Spider-Man). The slot stays in the list (its numbers are the saved layout) but is never loaded
// - not even from a v3 install's leftover folder - and the converter no longer builds it.
inline constexpr const char* folder = "UA/Body";

enum class Source : uint8_t { none, wornSuit, gameHero, standInSuit };
inline constexpr const char* sourceNames[4] = {"none", "worn suit", "game hero", "stand-in suit"};

// UA EXPANSION v6 (owner: "no body in the tower now"): his own SM2 Spider-Man shown through the room by the cut (ua_body_cut.hpp)
// has failed in every test, and the game ships no SM2 suit model to wear instead. Until the SM2 suit is extracted from the game
// files (tools/ua/extract_sm2_suit.py), a roster suit stands in as the body: the first of these (indexes into
// mesh_model::suitFolders) that is loaded and rigged. DAREDEVIL's red full-face suit first (the nearest to Spider-Man's
// red and blue), then CAPTAIN AMERICA's (the Avengers'), then the others.
inline constexpr uint32_t standInSuits[6] = {10, 8, 11, 9, 0, 4};
constexpr bool sameName(const char* a, const char* b) {
    while (*a && *a == *b)
        ++a, ++b;
    return *a == *b;
}
static_assert(sameName(mesh_model::suitFolders[standInSuits[0]], "Suit_daredevil_1") &&
                  sameName(mesh_model::suitFolders[standInSuits[1]], "Suit_captainAmerica_1") &&
                  sameName(mesh_model::suitFolders[standInSuits[2]], "Suit_daredevil_3") &&
                  sameName(mesh_model::suitFolders[standInSuits[3]], "Suit_captainAmerica_2") &&
                  sameName(mesh_model::suitFolders[standInSuits[4]], "Suit_batman_1") &&
                  sameName(mesh_model::suitFolders[standInSuits[5]], "Suit_ironMan_1"),
              "the stand-in suits' indexes follow mesh_model::suitFolders");
// The slot of the stand-in suit (0: none is ready). `ready(k)`: suit k (an index into suitFolders) is loaded and rigged.
template <class Ready>
inline uint32_t standInSlot(Ready ready) {
    for (const uint32_t k : standInSuits)
        if (ready(k))
            return mesh_model::slotSuitFirst + k;
    return 0;
}

// The body shown in a UA place (v6): the roster hero's worn suit (ready to wear) first; else a STAND-IN roster suit (standInSlot,
// ready: loaded and rigged) - it always shows, no cut needed; else, only when none exists, the player's OWN SM2 hero (the game's,
// posed by native_body, shown through the room by the cut: it has never come through in a test); else nothing. Never MUA's body
// or gloves. Outside a UA place: none from here.
constexpr Source pick(bool inUa, bool flatScreen, bool wornSuitReady, bool standInReady, bool nativeBodyDrawn) {
    if (!inUa || flatScreen)
        return Source::none;
    if (wornSuitReady)
        return Source::wornSuit;
    if (standInReady)
        return Source::standInSuit;
    return nativeBodyDrawn ? Source::gameHero : Source::none;
}

// The bones drawn in first person in a UA place: the FULL BODY (no game body stands in there: the player looks down at
// his own legs); SUIT BODY OFF still keeps the arms and hands (the player never loses his hands in a room).
constexpr uint32_t mask(uint32_t suitBodySetting) {
    return suit_wear::partOf(suitBodySetting) == suit_wear::partOff ? suit_wear::armBones : suit_wear::bodyBones;
}

// The feet: on the room's floor straight under the eyes (the frozen hero's feet are out in the street). `floorY` the
// room's floor (world, travel already added); a bad floor, or one not under the eyes (more than 2.6 m below or above
// them): `eyeHeight` under the eyes.
inline Vec3 feet(Vec3 eye, float floorY, float eyeHeight = 1.65f) {
    const float drop = eye.y - floorY;
    const float y = std::isfinite(floorY) && drop > .3f && drop < 2.6f ? floorY : eye.y - eyeHeight;
    return Vec3{eye.x, y, eye.z};
}

// The drive the avatar takes each frame beyond the tracked poses (today: nothing - the hands' own curl from the
// controllers). Combat and powers fill it later; kept here so they plug in without touching the body's draw.
struct Drive {
    bool handPose[2]{};      // a power holds this hand's pose (the palm's frame below) instead of the controller's
    Vec3 palmForward[2]{};   // that pose's -z (the fingers) and x (across the palm), world
    Vec3 palmX[2]{};
    float curl[2]{-1.f, -1.f}; // a power's finger curl (0 open .. 1 a fist); below 0: the controller's
};
// The drive applied to a tracked frame (a hand held by a power takes its palm frame, its tilt off; a curl given
// replaces the controller's). With an empty Drive: t unchanged.
inline void apply(const Drive& d, suit_wear::Tracked& t) {
    for (unsigned i = 0; i < 2; ++i) {
        if (d.handPose[i] && std::isfinite(d.palmForward[i].x + d.palmForward[i].y + d.palmForward[i].z) &&
            length(d.palmForward[i]) > .5f && length(d.palmX[i]) > .5f) {
            t.gripForward[i] = normalized(d.palmForward[i]);
            t.gripX[i] = normalized(d.palmX[i]);
            t.gripUp[i] = normalized(cross(t.gripForward[i], t.gripX[i]) * -1.f);
            t.handleTiltDeg[i] = 0.f;
        }
        if (d.curl[i] >= 0.f && std::isfinite(d.curl[i]))
            t.curl[i] = d.curl[i] > 1.f ? 1.f : d.curl[i];
    }
}

} // namespace spidy::ua_body
