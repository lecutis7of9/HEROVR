#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
// Made by Agent Mulder.
//
// Version 0.0.74 test 1 BATMOBILE (owner pt1159u "add the Batmobile's to test 1"): Batman (every suit) drives
// a Batmobile on the van's own system (van072: summon on X, in and out by the driver door handle, the real
// wheel, bulletproof). Four cars, picked on the wrist (BATMOBILE CHOICE) or the dash (HOME: BATMOBILE), the
// pick applying on the next summon:
//   1989       "Bat-Mobile (1989)" by Dreamloft3D (CC BY 4.0)
//   ODAMI      "Batmobile" by Odami_3D (CC BY 4.0)
//   FOREVER    "Batmobile" by SorroW (CC BY 4.0)
//   FOREVER 2  "Batmobile - Batman Forever" by S.York-Designs (CC BY 4.0; AI-generated with Meshy)
// Each is Models\Vehicle\Batmobile\<FOLDER>\model.glb, its windshield and side glass Glass\model.glb (drawn
// blended, lightly tinted: the driver sees the road through it), and one shared cockpit (Cockpit\, original,
// Made by Agent Mulder) and wheel (SteeringWheel\) sat inside each at the layout below. The numbers are
// MEASURED by the converter (tools/batmobile074/convert.py; layout.json beside each model): CONFIRMED on
// the models, CANDIDATE in game. Pure: no game reads, no addresses. Fail-safe: a model missing or a fault
// and there is no car (a note on the wrist) - never a crash, never the flat screen.
#include "spidy/math.hpp"
#include "spidy/mesh_model.hpp"
#include "spidy/van072.hpp"
#include "spidy/vertex.hpp"

#include <cstdint>
#include <vector>

namespace spidy::batmobile074 {

// The wrist options' one-line help (combat38 BATMOBILE, BATMOBILE CHOICE; ON / 1989 is value 0, the default).
inline constexpr const char* help = "Drives a Batmobile as Batman (X summons it), turning on needs a restart.";
inline constexpr const char* choiceHelp = "Which Batmobile Batman's X summons (also the dash's BATMOBILE), no restart.";

inline constexpr uint32_t cars = 4;
inline constexpr const char* names[cars] = {"1989", "ODAMI", "FOREVER", "FOREVER 2"};
// Under Models\Vehicle\Batmobile\ (mesh_model slotBatmobileFirst + car, slotBatmobileGlassFirst + car).
inline constexpr const char* folders[cars] = {"1989", "ODAMI", "FOREVER", "FOREVER2"};

struct Credit {
    const char* title;
    const char* artist;
    const char* licence;
    const char* url;
};
// The dash's credit line (the HOME page, upper case for its font).
inline constexpr const char* creditLines[cars] = {
    "MODEL: BAT-MOBILE (1989) BY DREAMLOFT3D, CC BY 4.0", "MODEL: BATMOBILE BY ODAMI 3D, CC BY 4.0",
    "MODEL: BATMOBILE BY SORROW, CC BY 4.0", "MODEL: BATMOBILE - BATMAN FOREVER BY S.YORK-DESIGNS (AI, MESHY), CC BY 4.0"};
inline constexpr Credit credits[cars] = {
    {"Bat-Mobile (1989)", "Dreamloft3D", "CC BY 4.0", "https://dreamloft3d.itch.io/bat-mobile-1989"},
    {"Batmobile", "Odami_3D", "CC BY 4.0", "https://sketchfab.com/3d-models/batmobile-04f571c0fece4c37a01d37c70721d5a6"},
    {"Batmobile", "SorroW", "CC BY 4.0", "https://sketchfab.com/3d-models/batmobile-4a7dad5866564ab3b497cb1f427cd602"},
    {"Batmobile - Batman Forever (AI-generated with Meshy)", "S.York-Designs", "CC BY 4.0",
     "https://sketchfab.com/3d-models/batmobile-batman-forever-77795a4cef7f4ae29b649eb527c49b05"},
};

// One car in its frame (van072's: x right, y up, -z forward, the ground under the axles' middle; metres).
struct Layout {
    float length, width, height;
    Vec3 cockpit;   // where the shared Cockpit model's origin goes
    Vec3 eye;       // the driver's eye in the seat
    Vec3 hub;       // the wheel's hub
    Vec3 axis;      // the wheel's axis toward the driver
    Vec3 handleIn, handleOut;
    Vec3 exit;      // on the street, .75 m out from the driver's side
    Vec3 screen;    // the dash screen's face centre (flush, square)
    bool glass;     // a Glass model (FOREVER 2: its canopy is cut open, no glass)
};
const Layout& layout(uint32_t car);
inline uint32_t clampCar(uint32_t car) { return car < cars ? car : 0; }

// The car's driving numbers: the van's (top speed, pedals, wheel, steer) with the car's own geometry.
van072::Tune tune(uint32_t car);
// The dash screen's metres a design unit (the cockpit's bezel takes the .28 x .17 face at 1:1).
inline constexpr float screenScale = 1.f;
// The glass's tint and opacity (lightly tinted, blended: the road shows through).
inline constexpr float glassAlpha = .22f;
inline constexpr Vec3 glassTint{.10f, .13f, .17f};

// What the translucent pass draws: the car's glass (world space; library-owned mesh).
struct Glass {
    bool on{};
    const mesh_model::Mesh* mesh{};
    Vec3 at{}, forward{0, 0, -1};
};
// Appends the glass for an eye at `viewer`: every triangle blended at glassAlpha, far to near. Nothing on a
// fault (out put back); returns whether it drew.
bool appendGlass(std::vector<Vertex>& out, const Glass& g, Vec3 viewer);

// The dash picker: the next car after `car`.
inline uint32_t nextCar(uint32_t car) { return (clampCar(car) + 1) % cars; }

} // namespace spidy::batmobile074
