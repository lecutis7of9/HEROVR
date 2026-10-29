#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
// Made by Agent Mulder.
//
// Version 0.0.73 test 3 HOLO ROSTER (fix 28; the user, 2:12 PM: "please make sure the hero roster screen is
// the tiles on the hero menu in the holo not the giant pop up screen"): the wrist hologram's ROSTER row opens
// the heroes ON THE HOLOGRAM, in place of the page under it - no big panel in front of the player.
//   MARVEL / DC   a tile a hero (his colour badge and name, PLAYING on the one played); a tile opens his page
//   his page      BACK, PLAY AS, and three tabs: CONTROLS (hero_controls::guideOf), OUTFITS (the roster's
//                 suits, picked the same way: IN USE / UNLOCKED / LOCKED / MODEL MISSING) and STATS (only
//                 what the mod really keeps: his rep, level and rank, and the REP LOG's counts), whose
//                 REP LOG button opens the log with only his entries
//   SETTINGS      (0.0.73 test 4, the user: "it should just be like the tiles with subpages not all the
//                 dropdowns"): the hero's own settings (the old POWERS page's dropdown rows) a short page at a
//                 time, then EVERY HERO (INVINCIBLE and FLYING); PREV / NEXT page them. wrist_hud lays out and
//                 works the rows (its own controls) and tells the roster how many pages there are.
//   REP           (the user, 2:13 PM) the rep meter - the played hero's level, title and step, the way to the
//                 next level - and the log (rep_log.hpp), scrolled with the stick
//   SOON          (0.0.73 COMING SOON; the owner: "Let's add the profile pictures but put them in a 'coming soon'
//                 section in the roster page") the heroes coming in 0.0.74, MARVEL or DC, a picture and a name
//                 each (Models/Roster's soon_* pictures, tools/roster/soon), NOT playable: their tiles do
//                 nothing when pressed. BATMAN and SUPERMAN are playable already, so their new suits are a
//                 line under the DC tiles, not tiles. COMING SOON (combat38::comingSoon) OFF hides the tab.
// HERO MENU (combat38::heroMenu) CLASSIC brings the old big ROSTER panel (roster_page.hpp) back instead.
//
// Pure: the state, its buttons and what it draws (pieces, in the wrist panel's own measures: metres at
// scale 1, u right and v up from its middle); wrist_hud's Menu drives it and its painter draws the pieces.
#include "spidy/hero_modes.hpp"
#include "spidy/math.hpp"
#include "spidy/rep_log.hpp"
#include "spidy/roster_page.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace spidy::holo_roster {
inline constexpr uint32_t heroes = hero_modes::heroes;
// The tabs' heroes, in their order (the user's list).
inline constexpr uint32_t marvelCount = 8, dcCount = 3;
inline constexpr uint32_t marvelHeroes[marvelCount] = {
    hero_modes::spiderMan, hero_modes::ironMan,  hero_modes::daredevil,      hero_modes::hulk,
    hero_modes::wolverine, hero_modes::punisher, hero_modes::captainAmerica, hero_modes::thor};
inline constexpr uint32_t dcHeroes[dcCount] = {hero_modes::batman, hero_modes::superman, hero_modes::greenLantern};
// MARVEL (0) or DC (1).
uint32_t groupOf(uint32_t hero);
// 0.0.73 COMING SOON: the 0.0.74 heroes, in roster_page's soon_* picture order (picture soonFirst + k).
// One tile a character (CYBORG is both the JUSTICE LEAGUE and the TEEN TITANS one).
struct SoonHero {
    const char* name;
    uint32_t group; // 0 MARVEL, 1 DC
};
inline constexpr uint32_t soonCount = roster_page::soonPictures;
inline constexpr SoonHero soonHeroes[soonCount] = {
    {"DEADPOOL", 0},       {"NIGHTCRAWLER", 0},  {"MOON KNIGHT", 0},   {"MAGNETO", 0},     {"CYCLOPS", 0},
    {"JEAN GREY", 0},      {"VISION", 0},        {"NICK FURY", 0},     {"WINTER SOLDIER", 0}, {"DOCTOR STRANGE", 0},
    {"MR. FANTASTIC", 0},  {"GHOST RIDER", 0},   {"HUMAN TORCH", 0},   {"SILVER SURFER", 0}, {"THE THING", 0},
    {"CYBORG", 1},         {"THE FLASH", 1},     {"HAWKMAN", 1},       {"ROBIN", 1},       {"NIGHTWING", 1},
    {"RAVEN", 1}};
// The new suits for heroes already playable (a line on the SOON tab's DC page).
inline constexpr const char* soonSuits =
    "NEW SUITS COMING: BATMAN (BATMAN V SUPERMAN, JUSTICE LEAGUE) AND SUPERMAN (BATMAN V SUPERMAN).";
inline constexpr int soonPicture(uint32_t k) { return static_cast<int>(roster_page::soonFirst + k); }

enum class Screen : uint8_t { closed, grid, hero, rep, soon };
enum class Tab : uint8_t { controls, outfits, stats, settings };
enum Id : int {
    none = -1,
    tabMarvel = 1,
    tabDc,
    tabRep,
    closeButton,
    backButton,
    playButton,
    subControls,
    subOutfits,
    subStats,
    seeLog,      // STATS: this hero's REP LOG
    logFilter,   // REP: ALL HEROES <-> the filtered hero
    logNewer,
    logOlder,
    subSettings,   // 0.0.73 test 4: the SETTINGS sub-tab
    settingsPrev,  // its pages
    settingsNext,
    tabSoon,     // 0.0.73 COMING SOON: the header's SOON tab
    tile = 20,   // + hero
    outfit = 40, // + suit row
    soonMarvel = 50, // the SOON tab's MARVEL / DC
    soonDc = 51,
    soonTile = 60, // + soonHeroes index (never pressable: not playable yet)
};
struct State {
    Screen screen = Screen::closed;
    uint32_t group{};                   // the grid's tab: 0 MARVEL, 1 DC
    uint32_t soonGroup{};               // 0.0.73 COMING SOON: the SOON tab's 0 MARVEL, 1 DC
    uint32_t hero{};                    // the hero page's hero
    Tab tab = Tab::controls;
    uint32_t filter = heroes;           // the log: heroes = all, else that hero's entries
    float logFirst{};                   // the log's first row shown (rows, fractional while scrolling)
    int hover = none;
    uint32_t presses{};
    // 0.0.73 test 4 SETTINGS: the page shown and how many there are (wrist_hud's, set every update), and
    // the help of the setting row pointed at (wrist_hud's static text; null: the roster's own help).
    uint32_t settingsPage{};
    uint32_t settingsPages = 1;
    const char* settingHelp{};
};
// What the roster shows, as the game thread has it (publish / latest; tests pass their own).
struct Data {
    roster_page::Snapshot suits{};
    bool suitsKnown{};
    int32_t heroPoints[heroes]{};
    uint32_t playing{};
    bool reputation = true; // REPUTATION on
    bool repLog = true;     // REP LOG on
    bool comingSoon = true; // 0.0.73 COMING SOON on: the SOON tab shown
    rep_log::Log log{};
};
void publish(const Data&);
uint32_t latest(Data& out); // its version (0: nothing published yet)
uint32_t version();

// ---- the layout (the wrist panel's measures) ----
inline constexpr float left = -.084f, right = .084f, top = .093f, bottom = -.104f;
inline constexpr float tabY0 = .080f, tabY1 = .093f;
inline constexpr float logRowPitch = .0094f;
inline constexpr uint32_t logRows = 11; // the 12th met NEWER / OLDER (seen in roster-holo-rep.png)
// 0.0.73 test 4 SETTINGS: the band wrist_hud lays the setting rows in (above PREV / NEXT).
inline constexpr float settingsTop = .047f, settingsBottom = -.068f;
struct Button {
    int id = none;
    float u0{}, v0{}, u1{}, v1{};
    bool enabled = true;
};
inline constexpr uint32_t buttonsMost = 32;
uint32_t buttons(const State&, const Data&, Button (&out)[buttonsMost]);
int buttonAt(const State&, const Data&, float u, float v);

// What to draw: fills, outlines, a bar's fill, a disc and text (left, or centred on u0..u1), in a colour
// role of the hologram's palette (or the piece's own colour: custom).
enum class Role : uint8_t { text, label, value, help, accent, frame, line, good, warn, bad, backing, custom };
struct Piece {
    enum Kind : uint8_t { fill, outline, disc, text } kind = fill;
    float u0{}, v0{}, u1{}, v1{}; // text: u0, v0 its baseline start (u1: the right edge, centred: the middle)
    Role role = Role::text;
    Vec3 color{};                 // Role::custom
    float alpha = 1;
    std::string words;
    float height{};               // text: capital height
    bool centred{};
    // 0.0.73 test 3 PORTRAITS: a hero tile's disc carries its hero (hero_modes::Hero; its portrait,
    // roster_page picture `portrait`, is drawn in its place when there is one); its initials are
    // `fallback`, drawn only when that portrait is not.
    int portrait = -1;
    bool fallback{};
};
// Every piece of `s`'s screen. Text is fitted to its room (overlay_text's stroke widths).
void pieces(const State&, const Data&, std::vector<Piece>& out);
// The hovered button's one-sentence help (with its restart note).
std::string helpFor(const State&, const Data&, int id);

// ---- the input (wrist_hud::Menu): the pointer on the panel and a press, the stick to scroll ----
struct Actions {
    int playAs = -1;                    // switch to this hero (hero_modes)
    int suitHero = -1, suitIndex = -1;  // pick this suit
    bool closed{};
};
class Holo {
  public:
    void open(uint32_t playing);
    void close();
    bool isOpen() const { return s_.screen != Screen::closed; }
    // `hit`: the pointer on the panel at (u, v); `press`: a fresh trigger pull this update; `stick` up
    // positive (scrolls the log while pointed at the panel); `seconds` since the last update.
    Actions update(bool hit, float u, float v, bool press, float stick, float seconds, const Data&);
    const State& state() const { return s_; }
    State& state() { return s_; }

  private:
    State s_{};
};
} // namespace spidy::holo_roster
