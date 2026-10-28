#pragma once
// Build 8 ROSTER: the in-game HERO ROSTER page (the user, 11:27 PM: "wheres the roster page with the
// profile pictures and suit options?"). The wrist's ROSTER: HEROES AND SUITS opens it in front of the
// player: one profile-picture tile a hero (a head-and-shoulders crop of one of his picked suits'
// own Sketchfab preview, credited in Models/Roster/license.txt; SPIDER-MAN a drawn emblem); a tile
// opens that hero's page - his bio, PLAY AS, and his suits: USE MY OWN MOD OR IN-GAME SUIT first, then
// each of the user's picks with its artist, licence, preview and state (IN USE, UNLOCKED, LOCKED: MAX
// REP for the FINAL, or MODEL MISSING - a pick whose model is not downloaded, never a stand-in).
// Laser and trigger (either hand), or a poke, picks; B / Y goes back, then closes.
//
// Threads: the page (Page::update) runs where the hands are read (the tracking thread); the game
// thread publishes the suits (publish) and takes the picks; the render thread draws a View (append),
// its text kept in the panel's own space between frames (Cache) and only moved each frame.
// The pictures are the textured slot Models/Roster (mesh_model::slotRoster): picture k is that
// model's triangles 2k and 2k + 1 (tools/roster/make_roster_glb.py), drawn unlit into the model sink.
#include "spidy/hero072.hpp"
#include "spidy/math.hpp"
#include "spidy/mesh_model.hpp"
#include "spidy/vertex.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace spidy::roster_page {
inline constexpr uint32_t heroes = hero_modes::heroes, suitsMost = 8, pictureCount = 48;
// The pictures in Models/Roster, in order (make_roster_glb.py PICTURE_FOLDERS; tests check the two):
// 11 portraits (hero_modes order), then each suit's preview by its catalog folder.
inline constexpr const char* pictureFolders[pictureCount] = {
    "portrait_spiderman", "portrait_ironman", "portrait_daredevil", "portrait_hulk", "portrait_batman",
    "portrait_wolverine", "portrait_superman", "portrait_punisher", "portrait_captainamerica", "portrait_thor",
    "portrait_greenlantern", "CapSuit", "Suit_batman_1", "Suit_batman_2", "Suit_batman_3", "Suit_batman_5",
    "Suit_ironMan_1", "Suit_ironMan_2", "Suit_ironMan_3", "Suit_ironMan_4", "Suit_captainAmerica_1",
    "Suit_captainAmerica_2", "Suit_captainAmerica_4", "Suit_daredevil_1", "Suit_daredevil_2", "Suit_daredevil_3",
    "Suit_hulk_1", "Suit_hulk_2", "Suit_hulk_4", "Suit_wolverine_1", "Suit_wolverine_2", "Suit_wolverine_3",
    "Suit_wolverine_4", "Suit_wolverine_5", "Suit_punisher_1", "Suit_punisher_2", "Suit_thor_1", "Suit_thor_2",
    "Suit_thor_3", "Suit_greenLantern_1", "Suit_greenLantern_2", "Suit_greenLantern_3", "Suit_greenLantern_4",
    "Suit_superman_1", "Suit_superman_2", "Suit_superman_3", "Suit_superman_4", "PunisherWarzone"};
int pictureOf(const char* folder); // -1: none

// ---- the suits, as the game thread sees them ----
enum class Status : uint8_t { inUse, unlocked, lockedFinal, lockedRep, missing };
struct Row {
    char name[32]{}, artist[48]{}, licence[16]{}, folder[48]{};
    Status status = Status::unlocked;
    bool final{};
    int32_t rep{};
};
struct Snapshot {
    uint32_t count[heroes]{};
    Row rows[heroes][suitsMost]{};
    bool ownSuit{}, all{}; // OWN SUIT on; UNLOCK ALL SUITS (TEST UNLOCKS)
};
Snapshot snapshotOf(const hero072::SuitCatalog&, bool all, bool ownSuit);
// The game thread's latest (a copy under a lock; the version grows when it differs).
void publish(const Snapshot&);
uint32_t version();
uint32_t latest(Snapshot& out); // its version (0: nothing published yet)
bool same(const Snapshot&, const Snapshot&); // 0.0.73: what they show (never their padding)

// ---- the page (tracking thread) ----
inline constexpr float width = 1.24f, height = .92f, distance = 1.2f, drop = .12f;
inline constexpr float pokeDepth = .05f, pokeSlack = .015f, aimSlack = .01f;
inline constexpr float triggerOn = .75f, triggerOff = .35f, armSeconds = .4f, fadeSeconds = .2f;
// 0.0.73 fix 1 (ROSTER FLICKER): a lit button stays lit within stickySlack of it, or for hoverHoldSeconds
// after the laser leaves it (across a gap between tiles).
inline constexpr float stickySlack = .02f, hoverHoldSeconds = .15f;
// 0.0.73 test 2 BUTTON GUIDE (fix 23): controls is the hero's page with its CONTROLS tab open
// (hero_controls.hpp) in place of his suits; B goes back to SUITS, BACK to the heroes.
enum class Screen : uint8_t { closed, grid, hero, controls };
// What can be pressed: the tiles, CLOSE, BACK, PLAY AS, and suit row i (row + i).
enum Id : int { none = -1, tile = 0, closeButton = 20, backButton = 21, playButton = 22, suitsTab = 23, controlsTab = 24, row = 30 };
struct Button {
    int id = none;
    float x0{}, y0{}, x1{}, y1{}; // panel space, metres (x right, y up)
    bool enabled = true;
};
inline constexpr uint32_t buttonsMost = 24;
// The screen's buttons (`s` may be null: no rows).
uint32_t buttons(Screen, uint32_t hero, const Snapshot* s, Button (&out)[buttonsMost]);

struct Hand {
    bool tracked{};
    Pose grip{}, aim{}; // tracking space; aim -z forward
    float trigger{};
    bool a{}, b{}; // A / X presses the aimed button; B / Y goes back (closes from the grid)
};
struct Input {
    double seconds{};
    Pose head{}; // tracking space
    Hand hands[2]{};
    uint32_t playing{}; // the hero now (hero_modes)
};
struct Actions {
    int playAs = -1;               // switch to this hero (hero_modes)
    int suitHero = -1, suitIndex = -1; // pick this suit
    bool closed{};
};
class Page {
  public:
    // Opens in front of `head` (tracking space) on the grid.
    bool open(Pose head, double seconds);
    void close();
    Actions update(const Input&);
    bool isOpen() const { return screen_ != Screen::closed; }
    // While open, and after it closes until every trigger and button it saw is let go.
    bool holdsInput() const { return isOpen() || latch_; }
    Screen screen() const { return screen_; }
    uint32_t hero() const { return hero_; }
    int hover() const { return hover_; }
    Pose panel() const { return panel_; }
    float opacity() const { return opacity_; }
    bool laser(unsigned hand, Vec3& from, Vec3& to) const;
    uint32_t presses() const { return presses_; }

  private:
    Screen screen_ = Screen::closed;
    uint32_t hero_{}, presses_{};
    int hover_ = none;
    Pose panel_{};
    double openedAt_{}, last_{};
    float opacity_{};
    bool released_[2]{}, aReleased_[2]{}, bReleased_[2]{}, wasNear_[2]{}, latch_{};
    bool laserOn_[2]{}, laserWas_[2]{};
    double hoverSeen_{};
    Vec3 laserFrom_[2]{}, laserTo_[2]{};
    Snapshot snap_{};
    uint32_t snapVersion_{};
};

// ---- the render thread ----
struct View {
    float opacity{};
    Pose panel{};
    Screen screen = Screen::closed;
    uint32_t hero{}, playing{}, version{};
    int hover = none;
    bool laser[2]{};
    Vec3 laserFrom[2]{}, laserTo[2]{};
};
View view(const Page&, uint32_t playing, Pose worldFromTracking);
// One picture's place in the texture array (layer -1: not there - PICTURE MISSING is drawn).
struct Picture {
    int32_t layer = -1;
    float u0{}, v0{}, u1{}, v1{};
};
// Picture k of the Roster model: its triangles 2k, 2k + 1 (false: not that model).
bool pictures(const mesh_model::Mesh* roster, Picture (&out)[pictureCount]);
// The page's own vertices in panel space, rebuilt only when what it shows changes.
struct Cache {
    uint64_t key = ~0ull;
    std::vector<Vertex> plate;  // blended (the translucent pass)
    std::vector<Vertex> images; // textured (shape = u, v, layer, 1), panel space
    Snapshot snap{};
    uint32_t rebuilds{};
};
// Appends the page: the plate, frames and text into `out` (blended, no depth), the pictures into
// `models` (the depth-tested textured pass, drawn after; null: PICTURE MISSING boxes instead).
// `roster` is the Roster model (null: not loaded). `s` (tests) stands in for the published suits.
void append(std::vector<Vertex>& out, std::vector<Vertex>* models, const View&, Cache&, const mesh_model::Mesh* roster,
            const Snapshot* s = nullptr);
// The hovered button's one sentence and its restart note (the subtext line).
std::string helpFor(Screen, uint32_t hero, int id, const Snapshot*);
} // namespace spidy::roster_page
