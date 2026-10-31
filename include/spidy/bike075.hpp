#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
// Made by Agent Mulder.
//
// Version 0.0.75 MOTORCYCLE (owner: "a summonable motorcycle for Captain America and Wolverine"): CAPTAIN AMERICA
// and WOLVERINE (every suit) tap X to summon a motorcycle and ride it on the van's own system (van072: in by the
// left handlebar grip, out by a grip beside the seat, the bars turned by the hands, bulletproof like the cars):
//   "Low-Poly Sci-Fi Motorcycle - Cyberpunk Theme" by Mert Tetik (CC BY 4.0)
// Models\Vehicle\Motorcycle\model.glb the bike without its handlebar yoke, Handlebars\model.glb the yoke (its
// steering pivot at the origin, x right, y up, -z forward, the grips toward +z), both from
// tools/bike075/convert.py. The numbers below are MEASURED by the converter (layout.json beside the model):
// CONFIRMED on the model, CANDIDATE in game (the scale is set by a 64 cm tyre; the pivot's place along the yoke,
// the seated eye, the get-off grip and the step-off point are choices). DRIVER ONLY: the mod has no passenger
// seat for any vehicle (multiplayer riders are not carried). Pure: no game reads, no addresses. Fail-safe: the
// body model missing and there is no bike (a note on the wrist); the bars missing and the bike is ridden with
// its bars not drawn - never a crash, never the flat screen.
#include "spidy/math.hpp"
#include "spidy/van072.hpp"
#include <cmath>
#include <cstdint>
namespace spidy::bike075 {
inline constexpr const char* name = "MOTORCYCLE";
inline constexpr const char* folder = "Vehicle/Motorcycle";
// The dash's credit line (upper case for its font).
inline constexpr const char* creditLine = "MODEL: LOW-POLY SCI-FI MOTORCYCLE BY MERT TETIK, CC BY 4.0";
inline constexpr const char* creditTitle = "Low-Poly Sci-Fi Motorcycle - Cyberpunk Theme";
inline constexpr const char* creditArtist = "Mert Tetik";
inline constexpr const char* creditLicence = "CC BY 4.0";
inline constexpr const char* creditUrl =
    "https://sketchfab.com/3d-models/low-poly-sci-fi-motorcycle-cyberpunk-theme-50c38758e5134a0b8ffa49780e7fe228";
// The bike in its frame (van072's: x right, y up, -z forward, the ground under the axles' middle; metres).
struct Layout {
    float length, width, height, wheelBase;
    Vec3 eye;              // the rider's eye on the seat
    Vec3 hub;              // the steering pivot (the bars turn round +y through it)
    Vec3 gripLeft, gripRight;
    Vec3 handleIn, handleOut; // out: the left grip (get on); in: beside the seat's left (get off)
    Vec3 exit;             // on the street, beside the bike's left
    Vec3 screen;           // the small dash screen's face centre, over the bars
};
const Layout& layout();
// The dash screen's metres a design unit (the Batmobile's .28 x .17 face at half size).
inline constexpr float screenScale = .5f;
// The bike's riding numbers: the van's pedals and summon with the bike's geometry and HANDLEBAR steering.
van072::Tune tune();
// Short X taps summon (a hold is the abilities' X modifier): a press released within this many seconds.
inline constexpr float tapMost = .4f;
// The summon tap: true once when X goes up after a press no longer than tapMost (dt-stepped; NaN-safe).
struct Tap {
    bool down{};
    float held{};
    bool step(bool x, float dt) {
        const float d = std::isfinite(dt) && dt > 0 && dt < 1.f ? dt : 0.f;
        bool tap = false;
        if (x)
            held = down ? held + d : 0.f;
        else if (down)
            tap = held <= tapMost;
        down = x;
        return tap;
    }
};
} // namespace spidy::bike075
