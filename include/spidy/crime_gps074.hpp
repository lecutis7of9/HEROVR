#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
// Made by Agent Mulder.
//
// 0.0.74 test 2b CRIME GPS (combat38 CRIME GPS; ON is value 0, the default): in the Batmobile or the Punisher
// van, crimes near the car. A "crime" is a group of the game's enemies (ThugBot records, world_trace
// enemiesFar) standing together - CANDIDATE: the mod reads no crime / activity list of the game's, so a group
// of thugs is our best sign of one; no street names or addresses are invented. A new one pops "CRIME IN
// PROGRESS - press OK to track" on the dash (its MAP page); OK tracks it: a red dot on the MAP and an arrow
// over the dash with its bearing and distance. The game's own waypoint is NOT set (no CONFIRMED hook).
// When a crime's group is gone it moves to the past cases: CLEARED when the player was near it at the end,
// LOST otherwise. The Batcomputer's ONGOING / PAST CASES, MAP and POLICE SCANNER read the same list.
// Pure: no game reads, no addresses, no allocation in step (fixed arrays). Every number is checked.
#include "spidy/math.hpp"

#include <array>
#include <cstdint>

namespace spidy::crime_gps074 {

inline constexpr const char* help = "ON shows crimes near your car: popup, MAP, HUD arrow; no restart.";

inline constexpr uint32_t mostThugs = 32;     // the enemies taken per scan (nearest first)
inline constexpr uint32_t mostCrimes = 8;     // live crimes kept
inline constexpr uint32_t mostPast = 8;       // past cases kept (the oldest dropped)
inline constexpr uint32_t mostLog = 12;       // scanner lines kept
inline constexpr float groupMetres = 20.f;    // two thugs this near are one crime
inline constexpr uint32_t groupLeast = 2;     // the fewest thugs that make a crime (CANDIDATE)
inline constexpr float matchMetres = 35.f;    // a group this near last scan's crime is the same crime
inline constexpr float goneSeconds = 4.f;     // unseen this long: over
inline constexpr float clearedMetres = 45.f;  // the player this near at the end: CLEARED
inline constexpr float popupSeconds = 12.f;   // the popup waits this long for OK
inline constexpr float reachMetres = 600.f;   // world_trace enemiesFar's reach

enum class Outcome : uint8_t { none, cleared, lost };

struct Crime {
    uint32_t id{};       // 1, 2, ... (0: empty slot)
    Vec3 at{};           // the group's centre (world)
    uint32_t thugs{};    // its size at the last scan
    uint32_t most{};     // the most seen at once
    float first{}, last{}; // seconds (the tracker's clock) first / last seen
    bool nearSeen{};     // the player was within clearedMetres at its last sight
};
struct Past {
    uint32_t id{};
    Vec3 at{};
    uint32_t most{};
    float ended{};       // seconds
    float lasted{};      // seconds
    Outcome outcome{};
};
// A scanner line: what happened, when (seconds), the crime's id and where it was from the player then.
enum class LogKind : uint8_t { none, spotted, tracked, cleared, lost };
struct LogLine {
    LogKind kind{};
    uint32_t id{};
    float at{};          // seconds
    float metres{};      // from the player
    float bearing{};     // degrees clockwise from world -z (the map's N, CANDIDATE)
    uint32_t thugs{};
};

struct Event {
    uint32_t appeared{}; // a new crime's id (0: none)
    uint32_t ended{};    // a crime that ended this step (0: none)
    Outcome outcome{};
};

// Compass bearing (degrees 0..360, clockwise from world -z) and flat distance from `from` to `to`.
bool bearingTo(Vec3 from, Vec3 to, float& degrees, float& metres);
// Bearing relative to a heading (radians clockwise from world -z), -180..180 degrees (+: to the right).
float relative(float bearingDegrees, float headingRadians);
// The compass point of a bearing ("N", "NE", ...).
const char* compass(float degrees);

class Tracker {
public:
    // One scan: the clock (seconds), the enemies' feet (world), the player (world). `on` false clears the
    // live list and the popup (the past cases and the log are kept).
    Event step(float now, const Vec3* thugs, uint32_t count, Vec3 player, bool on);
    // The popup's crime (0: none) and the seconds it has left.
    uint32_t popup() const { return popup_; }
    float popupLeft(float now) const;
    // OK on the popup: that crime tracked (false: no popup or it is gone).
    bool accept(float now);
    void dismiss() { popup_ = 0; }
    // Track (or untrack with 0) a live crime by id; false: no such crime.
    bool track(uint32_t id, float now);
    uint32_t tracked() const { return tracked_; }
    const Crime* find(uint32_t id) const;
    const std::array<Crime, mostCrimes>& live() const { return live_; }
    uint32_t liveCount() const;
    // Past cases, newest first (k < pastCount()).
    const Past& past(uint32_t k) const;
    uint32_t pastCount() const { return pastN_; }
    // Scanner lines, newest first (k < logCount()).
    const LogLine& log(uint32_t k) const;
    uint32_t logCount() const { return logN_; }
    void reset();

private:
    std::array<Crime, mostCrimes> live_{};
    std::array<Past, mostPast> past_{};
    std::array<LogLine, mostLog> log_{};
    uint32_t pastHead_{}, pastN_{}, logHead_{}, logN_{};
    uint32_t nextId_{1}, popup_{}, tracked_{};
    float popupAt_{};
    Vec3 player_{};
    void addPast(const Crime& c, float now, Outcome o);
    void addLog(LogKind k, const Crime& c, float now);
};

} // namespace spidy::crime_gps074
