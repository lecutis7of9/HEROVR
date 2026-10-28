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
//   REP           (the user, 2:13 PM) the rep meter - the played hero's level, title and step, the way to the
//                 next level - and the log (rep_log.hpp), scrolled with the stick
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

enum class Screen : uint8_t { closed, grid, hero, rep };
enum class Tab : uint8_t { controls, outfits, stats };
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
    tile = 20,   // + hero
    outfit = 40, // + suit row
};
struct State {
    Screen screen = Screen::closed;
    uint32_t group{};                   // the grid's tab: 0 MARVEL, 1 DC
    uint32_t hero{};                    // the hero page's hero
    Tab tab = Tab::controls;
    uint32_t filter = heroes;           // the log: heroes = all, else that hero's entries
    float logFirst{};                   // the log's first row shown (rows, fractional while scrolling)
    int hover = none;
    uint32_t presses{};
};
// What the roster shows, as the game thread has it (publish / latest; tests pass their own).
struct Data {
    roster_page::Snapshot suits{};
    bool suitsKnown{};
    int32_t heroPoints[heroes]{};
    uint32_t playing{};
    bool reputation = true; // REPUTATION on
    bool repLog = true;     // REP LOG on
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
