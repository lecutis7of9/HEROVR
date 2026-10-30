#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
// Made by Agent Mulder.
//
// 0.0.74 test 2b BATCAVE (and the SAFEHOUSE's way out): what the mod draws in a hideout over the room model.
//   * glowing markers at the room's points: RETURN TO CITY (gold, a ring on the floor, a beam and a label) at
//     the Batmobile / the van and the blast door, BATCOMPUTER (blue), the coming-soon ones dim;
//   * an arrival hint in front of the spawn for its first seconds (how to walk and how to get out);
//   * a RETURN TO CITY holo button over the left wrist (poke it with the other hand);
//   * the BATCOMPUTER: a chair on the round platform facing the console (sit: the player put in it facing the
//     screens, his eye lifted / lowered to a seated height by the SEATED DRIVING lift), and a big screen with
//     its pages: CRIMINAL FILES, ONGOING CASES, PAST CASES, POLICE SCANNER, FUTURE TECH RESEARCH, MAP,
//     BATMOBILE, MUSIC, RETURN TO CITY. Stick up / down picks, A or a trigger opens / acts, B goes back
//     (from the menu B stands up); or the dash's pointer: a finger tap on the panel, or aim + trigger
//     (rows, BACK, STAND UP).
// The crimes (ONGOING, PAST, SCANNER, MAP) are CRIME GPS's (crime_gps074.hpp: groups of the game's enemies,
// CANDIDATE); the criminal files and the scanner's chatter are FLAVOUR (original text, labelled), no invented
// addresses. Placement in the Batcave is from the model's bounds (MEASURED on the model; in game CANDIDATE).
// Pure: no game reads, no addresses. Every vertex is checked; any fault and nothing is drawn.
#include "spidy/crime_gps074.hpp"
#include "spidy/hideout074.hpp"
#include "spidy/math.hpp"
#include "spidy/vertex.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace spidy::batcave074 {

// The Batcomputer's pages (menu first: its list is the rest in order).
enum Page : uint8_t { menu, criminals, ongoing, pastCases, scanner, research, map, batmobile, music, returnCity };
inline constexpr uint32_t pages = 10;
inline constexpr const char* pageNames[pages] = {"BATCOMPUTER",  "CRIMINAL FILES", "ONGOING CASES", "PAST CASES", "POLICE SCANNER",
                                                 "FUTURE TECH RESEARCH", "MAP", "BATMOBILE", "MUSIC", "RETURN TO CITY"};

enum class Ask : uint8_t { none, nextCar, musicToggle, musicNext, returnToCity, track, stand };

struct File {
    const char* name;
    const char* threat;
    const char* lines[3]; // original, short (FLAVOUR)
};
inline constexpr uint32_t files = 8;
extern const File criminalFiles[files];
inline constexpr uint32_t researchItems = 6;
extern const char* const researchNotes[researchItems][2]; // name, status

// The Batcave's chair and screen (room / glTF space; the platform's top .37 m, the model's screens at z
// -3.9 .. -4.9: out of reach from any seat). The mod's screen is a holo panel in front of the chair, within
// arm's reach of the seated eye (reachMetres ahead of it, a little under it) so a finger can tap it; the far
// model screens stay as the backdrop. The panel is laid out in design units (designWidth x designHeight)
// and drawn at screenScale metres a unit.
inline constexpr Vec3 chairFeet{-15.7f, .32f, -1.75f}; // the walk grid's floor there
inline constexpr float chairFaceYaw = 90.f;      // degrees (hideout074's spawn yaw sense): facing -z, the screens
inline constexpr float seatedEye = 1.18f;        // metres over the chair's floor
inline constexpr float reachMetres = .58f;       // the panel's distance ahead of the seated eye (an arm's tap)
inline constexpr float designWidth = 1.7f, designHeight = 1.f, screenScale = .52f;
inline constexpr Vec3 screenCentre{chairFeet.x, chairFeet.y + seatedEye - .14f, chairFeet.z - reachMetres};
inline constexpr float screenWidth = designWidth * screenScale, screenHeight = designHeight * screenScale;
// The pointer (the dash's, van_screen DASH POINTER): a fingertip fingerReach metres out along the hand's aim
// touching the panel (pokeDepth in front .. pokeThrough behind, metres), or the hand's aim ray on it within
// rayReach metres plus a fresh trigger (past rayPress, re-armed under rayRelease). Design-unit hit areas:
// the page rows (rowTop - rowStep * i), BACK and STAND UP at the top right.
inline constexpr float fingerReach = .085f, pokeDepth = .03f, pokeThrough = .05f, rayReach = 2.5f, rayFacing = .2f,
                       rayPress = .6f, rayRelease = .3f;
inline constexpr float rowTop = designHeight / 2 - .11f, rowStep = .058f, rowText = .037f;
inline constexpr uint32_t mostRows = 14;
enum class Button : int8_t { none = -1, back = 0, stand = 1 };
// A design-unit point's row (0 = the first line under the title; -1: none) and button.
int rowAt(float x, float y);
Button buttonAt(float x, float y);
// The button's box (design units).
void buttonBox(Button b, float& x0, float& y0, float& x1, float& y1);
// The batcomputer room point (room.txt id) the chair belongs to.
inline constexpr const char* computerPoint = "batcomputer";

// One frame's Batcomputer input.
struct In {
    float stickY{};  // either stick's y (up +)
    bool press{};    // fresh A (a fresh trigger of a hand not aimed at the panel presses too)
    bool back{};     // fresh B
    uint32_t liveCrimes{};
    // 0.0.74 test 2b BATCOMPUTER POINTER (room space): the hands, their aim (unit), trigger and grip.
    bool pointer{true}; // false: the stick and buttons only
    bool tracked[2]{};
    Vec3 hand[2]{}, aim[2]{};
    float trigger[2]{}, grip[2]{};
};
// Where a room point lies on the panel: design units x right, y up; z metres out of it (toward the chair).
Vec3 onPanel(Vec3 room);

class Computer {
public:
    void open();
    void close();
    bool isOpen() const { return open_; }
    uint32_t page() const { return page_; }
    uint32_t cursor() const { return cursor_; }
    // One frame; what it asked for (track: trackIndex() is the ONGOING row picked).
    Ask step(const In& in);
    uint32_t trackIndex() const { return trackIndex_; }
    // A tap at design point (x, y) (a finger or the ray's trigger): a row picks and opens it, BACK / STAND UP.
    Ask tap(float x, float y, uint32_t liveCrimes);
    // The pointer (last step): the cursor on the panel (design units), the beam's start (room), what is lit.
    bool cursorOn() const { return cursorOn_; }
    Vec3 cursorAt() const { return cursorAt_; }
    bool beamOn() const { return beamOn_; }
    Vec3 beamFrom() const { return beamFrom_; }
    int hotRow() const { return hotRow_; }
    Button hotButton() const { return hotButton_; }
    bool touching() const { return near_[0] || near_[1]; }

private:
    bool open_{}, held_{};
    uint32_t page_{}, cursor_{}, trackIndex_{};
    bool near_[2]{}, trigWas_[2]{};
    bool cursorOn_{}, beamOn_{};
    Vec3 cursorAt_{}, beamFrom_{};
    int hotRow_{-1};
    Button hotButton_{Button::none};
    uint32_t rows(uint32_t liveCrimes) const;
    Ask activate(uint32_t liveCrimes); // the cursor's row on this page
};

// What the computer pages read (this frame's).
struct Feed {
    const crime_gps074::Tracker* crimes{};
    Vec3 city{};          // where the car stands in the city (world): the MAP's centre
    bool cityOk{};
    float now{};          // the tracker's clock
    const char* car{};    // the Batmobile picked
    std::string song;     // the music
    bool musicOn{}, musicPlaying{};
    bool batman{true};
};

// What the renderer draws in the hideout (room space + the room's place; the wrist button world).
struct View {
    bool on{};
    hideout074::Place place{};
    // markers: room-space point, kind (0 exit, 1 computer, 2 coming soon), radius
    static constexpr uint32_t mostMarks = 12;
    Vec3 markAt[mostMarks]{};
    uint8_t markKind[mostMarks]{};
    uint32_t marks{};
    int markLit{-1};
    float seconds{};      // the pulse clock
    bool batcave{};       // the chair and the screen are the Batcave's
    // the arrival hint (room space), its fade (0 none)
    Vec3 hintAt{}, hintFacing{};
    float hintFade{};
    // the wrist button (world), lit when the poking hand is near
    bool wristOn{}, wristLit{};
    Vec3 wristAt{}, wristRight{1, 0, 0}, wristUp{0, 1, 0};
    // the Batcomputer screen
    bool computer{};
    uint32_t page{}, cursor{};
    // the pointer: the cursor (design units), the beam from (room), the lit row / button
    bool cursorOn{}, beamOn{}, touching{};
    Vec3 cursorAt{}, beamFrom{};
    int hotRow{-1};
    int hotButton{-1};
    std::vector<std::string> lines; // the page's text (title first), built by pageLines
    std::vector<Vec3> mapDots;      // MAP: x, z on the map (-1..1), kind in y (0 crime, 1 tracked, 2 the car)
};

// The page's lines (title first) for `feed`.
std::vector<std::string> pageLines(uint32_t page, uint32_t cursor, const Feed& feed);
// MAP's dots (see View::mapDots); `range` metres to the map's edge.
std::vector<Vec3> mapDots(const Feed& feed, float range = 400.f);

// The wrist button's press: the other hand's tip `tip` within pokeMetres of `at` after being out past releaseMetres.
inline constexpr float pokeMetres = .06f, releaseMetres = .11f;
class WristButton {
public:
    bool step(bool on, Vec3 at, Vec3 tip, bool tracked);
    bool lit() const { return lit_; }

private:
    bool armed_{}, lit_{};
};

// Appends the markers, the hint, the wrist button and (Batcave) the chair and the computer's screen for an
// eye at `viewer` (world). Opaque parts (the chair) into `solid`, the rest into `glow`. On any fault both are
// put back as they were; returns whether it drew.
bool append(std::vector<Vertex>& solid, std::vector<Vertex>& glow, const View& v, Vec3 viewer);

} // namespace spidy::batcave074
