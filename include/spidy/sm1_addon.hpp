#pragma once
// HERO VR 0.0.76 SM1: Mulder's Heroes add-on for iilyn's Spider-Man VR mod. Made by Agent Mulder.
//
// iilyn's Spider-Man VR mod is SpidyVR (Spidy) by Ilya Mezerowsky (GPL-3.0, https://github.com/IlyaMez/SpidyVR). It runs
// Marvel's Spider-Man Remastered (Spider-Man.exe) in VR with its own graphics, performance and anti-aliasing work.
// This add-on is separate files: it never changes his files, his settings (%APPDATA%\Spidy) or his rendering.
//
// 0.0.76: the add-on is an OpenXR API layer (herovr_sm1_layer.dll, src/sm1/sm1_layer.cpp). The HERO VR launcher
// registers it for the current Windows user when PLAY starts Spider-Man 1 and removes it when the game closes. The
// OpenXR loader inside his VR module then puts it between his VR code and the headset runtime:
//  - it reads the controllers his own actions already read (no new actions, no new bindings);
//  - it hides only the inputs the add-on uses at that moment (the right trigger and stick while the wrist menu is
//    up; a hero's power trigger), so his webs never fire by mistake;
//  - it adds its own quad layers (the wrist menu, the holo card, a power's beam) on top of his finished frame. His
//    eye images, resolution, anti-aliasing and frame pacing are passed through untouched;
//  - a hero's power lands through his own exported test call SpidyPunchTest (spidy_ray_bridge.dll) on the enemy
//    his combat has ringed (SpidyStrikeSample's cueTarget). No game address of our own is used by any feature (the
//    read-only research scan, sm1_scan.hpp, only reads CANDIDATE addresses into its logs);
//  - flight moves the player through his own export SpidyMotionSubmit (spidy_movement_bridge.dll). His movement
//    module refuses any command whose serial is not above the last one, and his swing switches itself off on a
//    refusal; so the layer puts a small hand-over on his two movement exports (SpidyMotionSubmit and
//    SpidyMotionSample, MinHook on the exported functions, never on game code): his serials and ours go out in one
//    rising order, his commands wait while a hero flies, and his telemetry gets his own serials back (Handover).
// Any step that fails is logged to %APPDATA%\HERO VR SM1\layer.log and turns off only that feature; the layer then
// passes his calls through unchanged. It never fails his xrCreateInstance or xrEndFrame.
#include "spidy/hero_modes.hpp"
#include "spidy/math.hpp"
#include "spidy/eye_color.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace spidy::sm1 {
inline constexpr const char* addonName = "Mulder's Heroes add-on";
inline constexpr const char* addonVersion = "0.0.76";
inline constexpr uint32_t addonBuild = 76;
inline constexpr const char* hostTitle = "Marvel's Spider-Man Remastered";
// CONFIRMED (Spidy 0.7.0 tools/inspect_game.py): EXPECTED_VERSION and the Steam build's exe name.
inline constexpr const char* hostExe = "Spider-Man.exe";
inline constexpr const char* hostVersion = "4.630.0.0";
inline constexpr const char* hostSha256 = "e297d4d94f1ffe4febf289745e79e7b6fa233a788e7a00f480fc77c55db81ad1";
// CONFIRMED (Spidy 0.7.0 tools/run_game_vr.py SESSION_MODULES and the VR module loaded last).
inline constexpr const char* spidyModules[] = {"spidy_render_memory.dll", "spidy_render_probe.dll", "spidy_bridge.dll",
                                               "spidy_ray_bridge.dll", "spidy_movement_bridge.dll"};
inline constexpr int spidyModuleCount = 5;
inline constexpr const char* spidyVrModule = "spidy_stereo_probe.dll";
inline constexpr const char* spidyRayModule = "spidy_ray_bridge.dll";
inline constexpr const char* spidyMovementModule = "spidy_movement_bridge.dll";
// His exports the layer uses, by name (CONFIRMED in his 0.7.0 sources; exports, not game addresses):
//   SpidyXrData        spidy_stereo_probe.dll  src/game_xr.cpp     XrData v43, 1312 bytes, seqlock `sequence`
//   SpidyStrikeSample  spidy_ray_bridge.dll    src/game_punch.cpp  copies game_punch::Strikes v3, 328 bytes
//   SpidyPunchTest     spidy_ray_bridge.dll    src/game_punch.cpp  one blow to the game's DamageSystem (Test v1, 80 bytes)
//   SpidyMotionSubmit  spidy_movement_bridge.dll src/native_movement.cpp  Command v1, 48 bytes (serial must rise)
//   SpidyMotionSample  spidy_movement_bridge.dll src/native_movement.cpp  copies Data v4 (grounded, position)
inline constexpr const char* layerName = "XR_APILAYER_HEROVR_sm1_heroes";
inline constexpr const char* layerDll = "herovr_sm1_layer.dll";
inline constexpr const char* layerManifest = "herovr_sm1_layer.json";
inline constexpr const char* layerDisableEnv = "HEROVR_SM1_LAYER_DISABLE";

// The heroes: HERO VR's roster (hero_modes), shared with Spider-Man 2.
inline constexpr uint32_t heroCount = hero_modes::heroes;
inline const char* heroName(uint32_t i) {
    return i < heroCount ? hero_modes::heroNames[i] : "SPIDER-MAN";
}

enum class Evidence { confirmed, candidate };
// ready: works, tested on Linux; untested: built and wired, not yet tried in the game (no game on the build box);
// stub: not done (the note says why).
enum class State { ready, untested, stub };
struct Feature {
    const char* name;
    Evidence evidence;
    State state;
    const char* note;
};
inline constexpr Feature features[] = {
    {"host check", Evidence::confirmed, State::ready, "exe, version and iilyn's modules, read only"},
    {"add-on settings", Evidence::confirmed, State::ready, "%APPDATA%\\HERO VR SM1\\addon.ini and live.txt, never his folder"},
    {"wrist hero menu", Evidence::confirmed, State::untested,
     "opens like the SM2 build: touch your left wrist/forearm with the right hand and pull the right trigger (the same "
     "click opens and closes it); right stick moves, right trigger picks (OpenXR quad layer)"},
    {"hero select", Evidence::confirmed, State::untested, "picks one of the 11 heroes, saved in addon.ini"},
    {"outfits", Evidence::confirmed, State::untested,
     "each hero's outfits listed with their artists and saved; the wrist pick does not change the model in the game yet"},
    {"holo card", Evidence::confirmed, State::untested, "the hero's card on the wrist (2D, not the 3D hologram)"},
    {"hero powers", Evidence::confirmed, State::untested,
     "trigger of an outstretched hand: the hero's blast hits the ringed enemy through SpidyPunchTest, with a beam"},
    {"flight in game", Evidence::confirmed, State::untested,
     "IRON MAN, SUPERMAN, THOR, GREEN LANTERN: hold a grip to take off, stay up after letting go; his SpidyMotionSubmit "
     "export moves the body, with the layer keeping his serial numbers in order so his swinging never shuts off"},
    {"color", Evidence::confirmed, State::untested,
     "the SM2 build's COLOR / ATMOSPHERE (its looks incl. METROPOLIS, SMALLVILLE, GOTHAM, CYBERPUNK, SYNTHWAVE and its "
     "sliders) under the heroes on the wrist's HEROES page, drawn by the layer on his eye images at release; OFF by default (no GPU work)"},
    {"hero suits", Evidence::candidate, State::stub,
     "the launcher adds each hero as a NEW SM1 suit next to Spider-Man's (purely additive, through the game's toc; CANDIDATE, "
     "not yet seen in the game); nothing of the game's own suits is replaced"},
    {"holo suit switch", Evidence::candidate, State::stub,
     "no confirmed suit-equip call in 4.630.0.0, so picking a hero on the wrist does not change the suit live: the holo "
     "tells the player to pick that hero's suit in the game's Suits menu (or to install the suits in the launcher)"},
};
inline constexpr int featureCount = static_cast<int>(sizeof(features) / sizeof(features[0]));
const char* evidenceText(Evidence e);
const char* stateText(State s);

// --- Host check -----------------------------------------------------------------------------------------------
struct HostInfo {
    std::string exeName;    // the game's file name ("Spider-Man.exe")
    std::string version;    // its file version ("4.630.0.0"), empty when unreadable
    bool spidy[spidyModuleCount]{};
    bool vr = false;        // spidy_stereo_probe.dll loaded
};
enum class Verdict { ok, wrongGame, otherVersion, spidyMissing, vrNotStarted };
// wrongGame and spidyMissing stop the add-on (it stays idle); otherVersion only warns (his mod decides what it runs on).
Verdict judge(const HostInfo& h);
bool runs(Verdict v);
const char* verdictText(Verdict v);
bool sameFileName(std::string_view a, std::string_view b); // ASCII case-insensitive
// The file name of a path ("C:\\x\\Spider-Man.exe" -> "Spider-Man.exe").
std::string_view fileNameOf(std::string_view path);

// --- The add-on's settings (addon.ini; the launcher writes the switches, the wrist menu the picks) --------------
struct Config {
    bool heroes = true;    // heroes=1
    bool wristMenu = true; // wrist_menu=1
    bool flight = true;    // flight=1 (the flying heroes take off on a held grip)
    uint32_t hero = 0;     // hero=0..heroCount-1 (SPIDER-MAN by default)
    uint32_t outfit[heroCount]{}; // outfit.<hero>=<index> (0: his first)
};
// key=value lines, '#' or ';' comments, unknown keys and bad values ignored (the defaults stay).
Config parseConfig(std::string_view text);
std::string configText(const Config& c);

// --- live.txt (%APPDATA%\HERO VR SM1\live.txt): the add-on's own switches, read again every 2 seconds ----------
struct Live {
    bool powers = true;    // powers=1
    bool heroWebs = true;  // hero_webs=1
    bool beams = true;     // beams=1
    int damage = 100;      // power_damage=100 (25..200 %)
    int menuSize = 100;    // menu_size=100 (60..160 %)
    bool scan = true;      // sm1scan=1 (the read-only research scan, sm1_scan.hpp)
    bool grapple = true;   // grapple=1 (CAPTAIN AMERICA and DAREDEVIL, sm1_grapple.hpp)
    // 0.0.76 COLOR (the COLOR rows under the heroes on the holo's HEROES page): the SM2 build's colour grade (eye_color.hpp, the same looks and sliders),
    // drawn by the layer on his two eye images. OFF by default: then the layer does no GPU work at all and his
    // picture, anti-aliasing and performance are exactly his.
    bool colorFx = false;     // color_fx=0
    uint32_t colorFilter = 0; // color_filter=none (none, vivid, golden, neon, raimi, noir, animated, metropolis, ...)
    uint32_t colorStrength = 100;
    int32_t colorExposure = 0; // tenths of an EV, -20..20
    uint32_t colorContrast = 100, colorSaturation = 100, colorVibrance = 0;
    int32_t colorWarmth = 0, colorTint = 0;
    uint32_t colorVignette = 0;
};
Live parseLive(std::string_view text);
// The file the add-on writes when there is none: every switch with its one-sentence subtext.
std::string liveText(const Live& l);
struct LiveSwitch {
    const char* key;
    const char* subtext; // one sentence, says it applies live (no restart)
};
inline constexpr LiveSwitch liveSwitches[] = {
    {"powers", "Hero powers: the trigger of an outstretched hand fires the hero's power at the ringed enemy; applies live, no restart."},
    {"hero_webs", "Heroes who do not fly keep iilyn's web swinging on the grips (the flying heroes use the grips to fly); applies live, no restart."},
    {"beams", "Draws each power's beam from the hand (or the eyes) to the enemy; applies live, no restart."},
    {"power_damage", "How hard the powers hit, 25 to 200 percent; applies live, no restart."},
    {"menu_size", "The wrist menu's size, 60 to 160 percent; applies live, no restart."},
    {"sm1scan", "A read-only research scan that only reads game memory and writes logs to %APPDATA%\\HERO VR SM1\\scan for the Reports zip (it pauses at 0 and stops by itself after about 15 minutes); applies live, no restart."},
    {"grapple", "CAPTAIN AMERICA and DAREDEVIL grapple instead of web swinging: hold a grip and pull its trigger to fire a line at a wall or roof, let go of the grip to coast, press A to jump out; applies live, no restart."},
    {"color_fx", "COLOR: 1 draws the colour grade below on both eyes, 0 (default) leaves iilyn's picture exactly as it is with no extra GPU work; applies live, no restart."},
    {"color_filter", "COLOR FILTER: none, vivid, golden, neon, raimi, noir, animated, metropolis, smallville, gotham, cyberpunk or synthwave; applies live, no restart."},
    {"color_strength", "COLOR STRENGTH: how much of the filter is used, 0 to 100; applies live, no restart."},
    {"color_exposure", "COLOR EXPOSURE in tenths of a stop, -20 to 20 (0 is the game's); applies live, no restart."},
    {"color_contrast", "COLOR CONTRAST 50 to 150 (100 is the game's); applies live, no restart."},
    {"color_saturation", "COLOR SATURATION 0 to 200 (100 is the game's, 0 black and white); applies live, no restart."},
    {"color_vibrance", "COLOR VIBRANCE 0 to 100: boosts dull colours most and leaves skin nearly alone; applies live, no restart."},
    {"color_warmth", "COLOR WARMTH -100 (blue) to 100 (amber); applies live, no restart."},
    {"color_tint", "COLOR TINT -100 (green) to 100 (magenta); applies live, no restart."},
    {"color_vignette", "COLOR VIGNETTE 0 to 100: darkens the edges of each eye; applies live, no restart."},
};
// The SM2 grade's settings from live.txt (90s ANIMATED's cel pass and the DAREDEVIL sonar are the SM2 build's own:
// here 90s ANIMATED is its colour grade only).
eye_color::Config colorConfig(const Live& l);
// The COLOR rows (under the heroes on the HEROES page): 0 COLOR on/off, 1 FILTER, 2.. the sliders. delta +1 / -1 (a pick or a right flick: +1;
// a left flick: -1). Steps as the SM2 holo's (eye_color.hpp limits); a slider past its end wraps round.
inline constexpr int colorRows = 10;
const char* colorRowName(int row);
// The row's subtext (its live.txt switch's, liveSwitches), shown under the rows when it is selected.
const char* colorRowSubtext(int row);
std::string colorRowValue(const Live& l, int row);
void adjustColor(Live& l, int row, int delta);
// The live.txt lines missing from an older file (a switch added in a later build), with their subtexts; empty when
// none is missing. The layer appends them so every switch is in the file the player edits.
std::string missingLive(std::string_view text);

// --- Outfits (each hero's suits from HERO VR's roster, with their artists) --------------------------------------
uint32_t outfitCount(uint32_t hero);
// The hero's i-th outfit (name, artist, licence); empty strings past the end.
struct OutfitInfo {
    std::string name, artist, licence;
};
OutfitInfo outfitOf(uint32_t hero, uint32_t i);

// --- The heroes' powers in Spider-Man 1 ------------------------------------------------------------------------
enum class Source { none, hand, eyes };
struct Power {
    const char* name;
    Source source;
    float damage;    // at power_damage=100 (his punches do 7..28)
    int32_t knockback; // his punch.hpp Knockback: 2 Stagger, 3 Knockback, 4 Knockdown, 5 FlyBack, 9 SuperFlyBack
    float range;     // metres
    float cooldown;  // seconds between shots
    uint32_t color;  // 0xRRGGBB, the beam
};
const Power& powerOf(uint32_t hero);
// The aim cone: within maxDegrees of `dir` from `from`, and no further than range.
bool aimedAt(Vec3 from, Vec3 dir, Vec3 target, float maxDegrees, float range);
inline constexpr float aimDegrees = 15, outstretched = .42f; // a hand this far from the head fires (nearer: his scan/suit power)

// --- Input: the bindings his actions suggested, and what the layer hides from him ------------------------------
enum class Role { none, trigger, grip, stick, gripPose, aimPose, jump }; // jump: his "jump" action (A), read only
struct Binding {
    Role role = Role::none;
    int side = -1; // 0 left, 1 right
};
// "/user/hand/left/input/trigger/value" -> trigger, left. Clicks, values and touches: only value/click count.
Binding classify(std::string_view path);
struct MaskInput {
    bool menuOpen = false;
    uint32_t holoHands = 0; // bit per hand: the SM2 wrist click holds that hand's trigger and grip (wrist_hud::Toggle)
    bool spiderMan = true, powers = true, heroWebs = true;
    bool flightArmed = false; // a flying hero with flight on: the grips are his take-off and boost
    bool flying = false;      // in the air on our flight: the left stick and the right stick's up/down are ours
    bool grappleArmed = false; // CAPTAIN AMERICA / DAREDEVIL with grapple on: the grips are ours, and a held grip's trigger
    bool grip[2]{}, handOut[2]{};
};
struct Mask {
    bool trigger[2]{}, grip[2]{}, stick[2]{}, stickY[2]{};
};
Mask maskFor(const MaskInput& in);

// --- The wrist menu (pure state machine) ----------------------------------------------------------------------
// Open and close exactly as the SM2 build's hologram (wrist_hud::Toggle, its FOREARM mode, mode 4): the right
// hand touches the left wrist or forearm and pulls the right trigger; the same click closes it. The layer runs
// the SM2 detector and passes each toggle in `toggled`. The right stick moves the row up/down (one step per
// flick); the last row of a page goes to the next page; the right trigger picks.
struct WristInput {
    bool toggled = false; // the SM2 wrist click toggled the hologram this frame
    float stickX = 0, stickY = 0; // right stick, -1..1 (up and right are +)
    bool trigger = false; // right trigger past half
    float dt = 0;
};
// 0.0.76 (the user: the color profiles on the SAME page as the heroes, not a separate COLOR page): the HEROES page
// lists the heroes, then the COLOR rows (colorRows of them, with the selected row's subtext under them).
enum class Page : uint32_t { heroes, outfits, powers, addon, count };
// POWERS page rows: what a pick there toggles.
enum class Toggle : int { powers = 0, flight, heroWebs, beams, count };
enum class Pick { none, hero, outfit, toggle, closeMenu, color }; // color: adjust row by delta
struct WristMenu {
    bool open = false;
    Page page = Page::heroes;
    int row = 0;
    bool stickArmed = true, triggerArmed = true, stickXArmed = true;
    int delta = 0;        // Pick::color: +1 or -1 (the row: colorRow())
    uint32_t outfits = 1; // rows on the OUTFITS page (the caller sets it for the hero)
    int rows() const;
    // The HEROES page's COLOR row selected (0..colorRows-1), else -1.
    int colorRow() const;
    // One frame; returns what the trigger picked (the row is in row).
    Pick step(const WristInput& in);
};
const char* pageName(Page p);
const char* toggleName(Toggle t);

// --- Drawing: an RGBA8 picture (row 0 the top) the layer uploads to its quad --------------------------------------
struct Canvas {
    int width = 0, height = 0;
    std::vector<uint32_t> pixels; // 0xAABBGGRR (R first in memory)
    Canvas() = default;
    Canvas(int w, int h);
    void clear(uint32_t rgba);
    void rect(int x, int y, int w, int h, uint32_t rgba);       // filled, alpha blended
    void frame(int x, int y, int w, int h, int t, uint32_t rgba); // outline t pixels thick
    // Text in the pixel font (capitals, digits, punctuation), each font pixel `scale` square; returns its width.
    int text(int x, int y, std::string_view s, int scale, uint32_t rgba);
    static int textWidth(std::string_view s, int scale);
};
uint32_t rgba(uint32_t rgb, uint32_t alpha = 255);
// What the menu shows (the layer fills it each time something changes).
// --- Hero suits (the launcher's SM1 suits, tools/sm1_suits) --------------------------------------------------------
// The suit id the launcher installs for a hero ("herovr_thor"); nullptr for SPIDER-MAN (the game's own suits).
// The same table as tools/sm1_suits/heroes.py (a test keeps them equal).
const char* suitIdOf(uint32_t hero);
// The display name the game's Suits menu shows for it ("Thor"); "" for SPIDER-MAN.
const char* suitNameOf(uint32_t hero);
// The launcher's asset_archive\HeroVR\state.json (read only, our own file): true when a finished install lists the
// hero's suit.
bool suitInstalled(std::string_view stateJson, uint32_t hero);
// The holo's line after a hero pick. A live suit switch needs a confirmed suit-equip call, which 4.630.0.0 does not
// have yet, so the line tells the player where to pick the suit.
std::string suitHint(uint32_t hero, bool installed);
// The game folder of an exe path ("C:\g\Spider-Man.exe" -> "C:\g\").
std::string_view folderOf(std::string_view path);

struct MenuView {
    Page page = Page::heroes;
    int row = 0;
    uint32_t hero = 0;
    uint32_t outfit = 0;
    bool toggles[static_cast<int>(Toggle::count)]{true, true, true, true};
    bool flying = false;
    std::string powerState; // "READY", "NO TARGET", "HIT"...
    std::string status[6];  // the ADD-ON page's lines
    std::string notice;     // a short line on the holo card after a pick (the suit hint), "" when none
    std::string color[colorRows]; // the COLOR rows' values on the HEROES page ("ON", "GOTHAM", "100"...)
};
inline constexpr int menuWidth = 512, menuHeight = 640;
void paintMenu(Canvas& c, const MenuView& v);
// The beam's texture: a soft core along its length (height), the hero's colour.
void paintBeam(Canvas& c, uint32_t rgb);
bool sameView(const MenuView& a, const MenuView& b);

// --- Where the layer puts its quads (pure; the space is his projection layer's, Y up) -----------------------------
Quat fromBasis(Vec3 x, Vec3 y, Vec3 z); // unit, orthogonal axes -> the turn that takes +X,+Y,+Z to them
// The menu: centred `lift` metres over the left grip, turned to face the head, upright.
Pose menuPose(Pose leftGrip, Vec3 head, float lift);
// A beam `length` long from `from` along unit `dir`, its face turned to the head (the quad's +Y along the beam).
Pose beamPose(Vec3 from, Vec3 dir, float length, Vec3 head);

// --- Flight (the flying heroes; moved through his SpidyMotionSubmit by the layer) --------------------------------
bool flies(uint32_t hero); // IRON MAN, SUPERMAN, THOR, GREEN LANTERN
struct FlightTuning {
    float enterHold = .4f;  // seconds a grip is held to take off
    float liftOff = 4;      // m/s up on take-off
    float cruise = 12;      // m/s at full left stick
    float climb = 8;        // m/s at full right stick up/down
    float boost = 14;       // m/s along each held grip's hand aim
    float response = 4;     // 1/s: how fast the velocity follows the sticks
    float maxSpeed = 28;    // m/s (his movement module allows up to its own cap)
    float landAfter = 1;    // seconds in the air before touching the ground lands
};
struct FlightInput {
    bool allowed = false;     // a flying hero, heroes and flight on, the menu closed
    bool stop = false;        // the menu's FLIGHT toggle turned off
    bool grip[2]{};           // grips past half
    Vec3 aim[2]{};            // hand forward, game world (Y up)
    Vec3 headForward{0, 0, 1}, headRight{1, 0, 0}; // the head's axes, game world
    float moveX = 0, moveY = 0; // left stick (body-relative: right, forward)
    float climb = 0;          // right stick up/down
    bool grounded = false;    // his movement telemetry: the body stands on something
    float dt = 0;
};
enum class FlightEvent { none, tookOff, landed, stopped };
struct FlightState {
    bool on = false;
    float gripHeld = 0, airTime = 0;
    bool gripArmed[2]{}; // a grip counts as boost only after it was let go once in the air
    Vec3 velocity{};
};
// One frame. While on, `s.velocity` is what the layer submits (enabled, a short lease); off, nothing is submitted.
FlightEvent stepFlight(FlightState& s, const FlightInput& in, const FlightTuning& t = {});

// --- The hand-over on his movement exports (pure; the layer's detours call it under one lock) -------------------
// His movement module keeps the last accepted serial and refuses anything not above it (2003); his swing then shuts
// itself off for the session. Both movers' commands go out through here in one rising order. While ours is on, his
// are held back (he is told they went through: the body is ours for the moment). His telemetry gets his own serial
// back (sampleSerial), or 0 for a step one of ours drove.
class Handover {
  public:
    static constexpr uint64_t base = 1ull << 40; // our first serial: above anything his counter reaches in a session
    // His command with his serial: the serial to pass on, or 0 to hold it back (return success to him).
    uint64_t his(uint64_t serial);
    // Ours: the serial to submit.
    uint64_t ours();
    // A refusal of ours (2003): jump further up and try again once.
    void refused();
    void take() { ours_ = true; }
    void release() { ours_ = false; }
    bool ourTurn() const { return ours_; }
    // The serial his SpidyMotionSample shows -> the one his swing knows.
    uint64_t sampleSerial(uint64_t shown) const;

  private:
    struct Pair {
        uint64_t out, in;
    };
    Pair ring_[256]{};
    uint32_t next_ = 0;
    uint64_t last_ = 0, offset_ = 0;
    bool ours_ = false, used_ = false;
};
} // namespace spidy::sm1
