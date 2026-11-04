#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
// Made by Agent Mulder.
//
// Version 0.0.75 MOTORCYCLE (owner: "a summonable motorcycle for Captain America and Wolverine"): CAPTAIN AMERICA
// and WOLVERINE (every suit) tap X to summon a motorcycle and ride it on the van's own system (van072: in by the
// left handlebar grip, out by a grip beside the seat, the bars turned by the hands, bulletproof like the cars).
// 0.0.76 HARLEYS (owner pt1716u "1 for cap 3 for wolverine"; "like a Harley and not low poly"): each hero his own
// real bike, both CC BY 4.0:
//   CAPTAIN AMERICA  Models\Vehicle\Motorcycle\Cap        "EL OHV V-TWIN" by matias_romero
//   WOLVERINE        Models\Vehicle\Motorcycle\Wolverine  "2016 Harley-Davidson FXDLS Dyna Low Rider S" by everhard
//   either hero      Models\Vehicle\Motorcycle            the PLAYER'S OWN BIKE (read only with its override.txt)
// Each folder: model.glb the bike without its steer part, Handlebars\model.glb the steer part (the bars, risers and
// fork; its pivot at the origin, x right, y up, -z forward, the grips toward +z) and layout.json, all three from
// tools/bike076/convert.py (MEASURED on the model: CONFIRMED on the model, CANDIDATE in game). The layout is read
// at the start (parseLayout: every number checked); DRIVER ONLY (the mod has no passenger seat for any vehicle).
// Pure: no game reads, no addresses. Fail-safe: the body or its layout missing or refused and that hero has no
// bike (MODEL MISSING on the wrist, never another bike in its place); the bars missing and the bike is ridden with
// its bars not drawn - never a crash, never the flat screen.
#include "spidy/math.hpp"
#include "spidy/van072.hpp"
#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>
namespace spidy::bike075 {
inline constexpr const char* name = "MOTORCYCLE";
// The bikes: each hero's own, then the player's override for both.
enum Bike : uint32_t { cap, wolverine, own, bikes };
inline constexpr const char* folders[bikes] = {"Vehicle/Motorcycle/Cap", "Vehicle/Motorcycle/Wolverine", "Vehicle/Motorcycle"};
// The dash's name for each (upper case for its font).
inline constexpr const char* names[bikes] = {"EL OHV V-TWIN", "DYNA LOW RIDER S", "MOTORCYCLE"};
// The override is read only when this file is in Models\Vehicle\Motorcycle (the 0.0.75 bike's files left there by an
// older install are never picked up).
inline constexpr const char* overrideMarker = "override.txt";
inline constexpr const char* layoutFile = "layout.json";
// Which bike a summon brings: the player's own when it is ready, else the hero's (wolverine: WOLVERINE, else CAP).
inline constexpr Bike bikeFor(bool wolverineHero, bool ownReady) { return ownReady ? own : wolverineHero ? wolverine : cap; }

// The CC BY credits of the shipped bikes (the player's own bike: ownCreditLine; its credit is the player's).
struct Credit {
    const char* title;
    const char* artist;
    const char* artistUrl;
    const char* licence;
    const char* licenceUrl;
    const char* url;
    const char* line; // the dash's credit line (upper case for its font)
};
inline constexpr Credit credits[2] = {
    {"Harley Davidson Motorcycle \"EL OHV V-TWIN\"", "matias_romero", "https://sketchfab.com/matias_romero", "CC BY 4.0",
     "http://creativecommons.org/licenses/by/4.0/",
     "https://sketchfab.com/3d-models/harley-davidson-motorcycle-el-ohv-v-twin-9923aaa519334f48b8dd44fd792475ed",
     "MODEL: EL OHV V-TWIN BY MATIAS_ROMERO, CC BY 4.0"},
    {"2016 Harley-Davidson FXDLS Dyna Low Rider S", "everhard", "https://sketchfab.com/everhard", "CC BY 4.0",
     "http://creativecommons.org/licenses/by/4.0/",
     "https://sketchfab.com/3d-models/2016-harley-davidson-fxdls-dyna-low-rider-s-ad65bcb9282a46459a9a42f787bb5cbd",
     "MODEL: 2016 HARLEY-DAVIDSON FXDLS DYNA LOW RIDER S BY EVERHARD, CC BY 4.0"},
};
inline constexpr const char* ownCreditLine = "MODEL: THE PLAYER'S OWN BIKE (VEHICLE/MOTORCYCLE)";

// The bike in its frame (van072's: x right, y up, -z forward, the ground under the axles' middle; metres).
struct Layout {
    float length{}, width{}, height{}, wheelBase{};
    Vec3 eye{};                // the rider's eye on the seat
    Vec3 hub{};                // the steering pivot: on the steer axis at the grips' height (the steer part's origin)
    Vec3 steerAxis{0, 1, 0};   // the head tube's axis, unit, up (and back by the rake): the steer part turns round it
    float gripRadius{};        // the grips' distance from the hub seen from above
    Vec3 gripLeft{}, gripRight{};
    Vec3 handleIn{}, handleOut{}; // out: the left grip (get on); in: beside the seat's left (get off)
    Vec3 exit{};               // on the street, beside the bike's left
    Vec3 screen{};             // the dash screen's face centre: on the frame, centred between the bars, upright
    Vec3 tracker{};            // the CRIME GPS tracker's centre: level, centred between the grips, over the screen
};
// Limits parseLayout holds a layout to (a bike, not a bus or a toy).
inline constexpr float leastLength = 1.6f, mostLength = 3.2f, mostWidth = 1.4f, mostHeight = 2.f;
inline constexpr float mostRake = .8f; // radians from vertical (46 deg)
inline constexpr size_t mostLayoutBytes = 64u << 10;
// layout.json's text -> `out` (true), or false with `why` (and `out` untouched). Required: length, width, height,
// wheelBase, eye, hub, gripLeft, gripRight, gripRadius, handleIn, handleOut, exit, screen; optional: steerAxis
// (vertical), tracker (over the screen's top edge). Every number finite and in the bike's box; the grips on the
// hub's height and its rear half; the screen and tracker between the grips. NaN-safe, never throws.
bool parseLayout(std::string_view text, Layout& out, std::string& why);
// The dash screen's metres a design unit (the Batmobile's .28 x .17 face at half size: .14 x .085 m).
inline constexpr float screenScale = .5f;
// The default tracker: this far over the screen's centre, in its plane's metres (the screen's top edge + 2 cm).
inline constexpr float trackerOverScreen = .085f * .5f + .02f;
// The bike's riding numbers: the van's pedals and summon with the bike's geometry and HANDLEBAR steering.
van072::Tune tune(const Layout& l);
// 0.0.78 SEAT REACH (owner t1891u: "far too far back from the motorcycle handlebars"): ROOT CAUSE - convert.py put
// the eye straight over the seat (an upright 0.72 m torso), 0.64 m behind the grips (Cap 0.81 m, Wolverine 0.79 m
// eye -> grip: past an arm's reach). The rider leans forward about his hip (the eye's 0.72 m under it) until the
// grips are van072::driverReach away (Cap 25, Wolverine 28 degrees; at most mostLean). Bad input: the eye kept.
inline constexpr float riderTorso = .72f, mostLean = .7f;
Vec3 riderEye(const Layout& l);
// The steer part's model axes in the bike's frame, turned `angle` (van072's handlebar sign: + a right turn) round
// the layout's steer axis: `up` the model's +y, `toward` its +z. A vertical axis gives van072::wheelAxes' own.
void steerAxes(float angle, const Layout& l, Vec3& up, Vec3& toward);
// Short X taps summon (a hold is the abilities' X modifier): a press released within this many seconds.
// 0.0.76 fix 2 (owner, e808401 test: "no cap bike no wolverine bike"): 1 s, was .4 s - a deliberate press of X
// that lasted past .4 s summoned nothing and said nothing. A longer hold now says so (lastHeld, game_xr).
inline constexpr float tapMost = 1.f;
// The summon tap: true once when X goes up after a press no longer than tapMost (dt-stepped; NaN-safe).
struct Tap {
    bool down{};
    float held{}, lastHeld{}; // lastHeld: the last press's length, set as it goes up
    bool released{};          // this step: X went up (a tap or a too-long hold)
    bool step(bool x, float dt) {
        const float d = std::isfinite(dt) && dt > 0 && dt < 1.f ? dt : 0.f;
        bool tap = false;
        released = false;
        if (x)
            held = down ? held + d : 0.f;
        else if (down) {
            tap = held <= tapMost;
            lastHeld = held;
            released = true;
        }
        down = x;
        return tap;
    }
};
} // namespace spidy::bike075
