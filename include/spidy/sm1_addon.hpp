#pragma once
// HERO VR 0.0.75 SM1: Mulder's Heroes add-on for iilyn's Spider-Man VR mod.
//
// iilyn's Spider-Man VR mod is Spidy by Ilya Mezerowsky (GPL-3.0, https://github.com/IlyaMez/SpidyVR). It runs
// Marvel's Spider-Man Remastered (Spider-Man.exe) in VR with its own graphics, performance and anti-aliasing work.
// This add-on is separate files: it never changes his files, his settings (%APPDATA%\Spidy) or his rendering. The
// HERO VR launcher starts his launcher unchanged, waits until his VR module (spidy_stereo_probe.dll) is in the game,
// then loads herovr_sm1_addon.dll beside it. Our files are never named spidy_* (his launcher flags those).
//
// What is real in 0.0.75 and what is not (every game address here is CANDIDATE until a SM1 research pass finds it):
//  - CONFIRMED: the host checks (exe name, version, his module names; from his 0.7.0 sources), the add-on's own
//    settings file, the wrist menu's state machine and the flight motion below (pure code, tested on Linux).
//  - CANDIDATE, a no-op in 0.0.75: drawing the wrist menu into his VR frame, moving the player for flight, the
//    heroes' powers and models. GameLink's calls do nothing and say so; no hook is installed, no game memory is
//    read or written. Nothing here can change his graphics, frame rate or anti-aliasing.
#include "spidy/fly_lean.hpp"
#include "spidy/hero_modes.hpp"
#include "spidy/math.hpp"

#include <cstdint>
#include <string>
#include <string_view>

namespace spidy::sm1 {
inline constexpr const char* addonName = "Mulder's Heroes add-on";
inline constexpr const char* addonVersion = "0.0.75";
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
// The heroes: HERO VR's roster (hero_modes), shared with Spider-Man 2.
inline constexpr uint32_t heroCount = hero_modes::heroes;
inline const char* heroName(uint32_t i) {
    return i < heroCount ? hero_modes::heroNames[i] : "SPIDER-MAN";
}

enum class Evidence { confirmed, candidate };
enum class State { ready, stub };
struct Feature {
    const char* name;
    Evidence evidence;
    State state;
    const char* note;
};
inline constexpr Feature features[] = {
    {"host check", Evidence::confirmed, State::ready, "exe, version and iilyn's modules, read only"},
    {"add-on settings", Evidence::confirmed, State::ready, "%APPDATA%\\HERO VR SM1\\addon.ini, never his folder"},
    {"wrist menu logic", Evidence::confirmed, State::ready, "pages and selection, pure code"},
    {"wrist menu drawing", Evidence::candidate, State::stub, "needs his VR frame's overlay point (SM1 research)"},
    {"flight motion", Evidence::confirmed, State::ready, "speed, steering and body lean, pure code"},
    {"flight in game", Evidence::candidate, State::stub, "needs SM1's player position and velocity (SM1 research)"},
    {"heroes' powers", Evidence::candidate, State::stub, "needs SM1's combat and damage paths (SM1 research)"},
    {"heroes' models", Evidence::candidate, State::stub, "needs SM1's character model slots (SM1 research)"},
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

// --- The add-on's settings (addon.ini; written by the launcher) ------------------------------------------------
struct Config {
    bool heroes = true;   // heroes=1
    bool wristMenu = true; // wrist_menu=1
    bool flight = true;   // flight=1
    uint32_t hero = 0;    // hero=0..heroCount-1 (SPIDER-MAN by default)
};
// key=value lines, '#' or ';' comments, unknown keys and bad values ignored (the defaults stay).
Config parseConfig(std::string_view text);
std::string configText(const Config& c);

// --- The wrist menu (pure state machine) ----------------------------------------------------------------------
// Open: the left palm faces up (palmUp >= openDot) for openHold seconds. Close: palm down for closeHold seconds.
// The stick moves the row (one step per flick: past 0.6, back under 0.3), the trigger picks (one per press).
struct WristInput {
    float palmUp = 0;    // dot(left palm normal, world up)
    float stickY = 0;    // right stick, -1..1 (up is +)
    bool trigger = false; // right trigger past half
    float dt = 0;
};
enum class Page : uint32_t { heroes, flight, addon, count };
enum class Pick { none, hero, flightToggle, closeMenu };
struct WristMenu {
    static constexpr float openDot = .7f, closeDot = .3f, openHold = .35f, closeHold = .25f;
    bool open = false;
    Page page = Page::heroes;
    int row = 0;
    float held = 0;
    bool stickArmed = true, triggerArmed = true;
    int rows() const;
    // One frame; returns what the trigger picked (the row is in row).
    Pick step(const WristInput& in);
};
const char* pageName(Page p);

// --- Flight (pure; the in-game move is GameLink::applyFlight) ------------------------------------------------
struct FlightTuning {
    float accel = 18;       // m/s^2 at full trigger
    float drag = 1.2f;      // 1/s
    float maxSpeed = 40;    // FAST
};
struct FlightInput {
    bool on = false;
    Vec3 leftAim{}, rightAim{}; // hand forward directions (world)
    float leftThrust = 0, rightThrust = 0; // triggers 0..1
    Vec3 facing{0, 0, 1};
    float dt = 0;
};
struct FlightState {
    Vec3 velocity{};
    float lean = 0; // radians, + forward (fly_lean)
};
void stepFlight(FlightState& s, const FlightInput& in, const FlightTuning& t = {});

// --- The game link: CANDIDATE, a no-op in 0.0.75 ---------------------------------------------------------------
// Each call returns false (nothing done) until SM1 research CONFIRMS the address it needs; the add-on logs that
// once per feature. Kept so the add-on's frame code is wired and tested now.
struct GameLink {
    bool drawWristMenu(const WristMenu&) { return false; }
    bool applyFlight(const FlightState&) { return false; }
    bool applyHero(uint32_t) { return false; }
};
} // namespace spidy::sm1
