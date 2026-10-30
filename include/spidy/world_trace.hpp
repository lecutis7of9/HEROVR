#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
// Update 37: the WORLD LOG - read-only research tracers for future mods (an AI NPC
// conversation feature; the teammate bot "Spider-Man Hero Expansion" may use the
// spawn and mission data). Nothing here writes to the game.
//
//   NPC   when an NPC's dialogue component changes (a line starting or ending), the
//         speaker's kind from its actor's other components, where it is from the
//         player, whether the player looks at it, and the component's changed words
//         and any short text it points at (a candidate line id or name).
//   SPAWN a new actor in the game's component registry: its classes (the archetype),
//         where, and a spawner class on the same actor if there is one.
//   MISSION a registered component whose class is a mission, objective or activity
//         class appearing (start), going (stop) or changing its first words (state).
//
// CONFIRMED: the component registry 0xc2d3d20/0xc2d3d40 and the component header (vtable,
// actor record +0x10, own handle +0x1c; game_player.cpp, found live); MSVC RTTI is read
// at run time from each vtable (vtable[-1] -> complete object locator -> hierarchy), so no
// class address is assumed; every class name below is in the exe's RTTI (static scan,
// docs/spawn-and-mission-research.md lists their vtables).
// CANDIDATE: that dialogue components change while a line plays; which bot class is the
// police (AllyBot at a crime, as in Spider-Man Remastered); that mission objects are
// registered components at all (a session with 0 mission events says they are not).
// NOT AVAILABLE: subtitle text. Spider-Man 2's UI call wrapper is not hooked (game_menu:
// SM2TODO), and ShowSubtitle/HideSubtitle (literals 0x53ee1d8/0x53ee1c8) are a candidate
// hook for later - no new hook is added here (read-only, no invented addresses).
//
// Cost: a background thread walks the registry once every interval (default 1 s), at
// most `sliceEntries` entries a step with a sleep between, so it never holds a core; no
// work at all on the game's or the render thread. A scan slower than `slowScanMs` doubles
// the interval; any fault (an exception, a broken registry three times running) turns the
// log off for the session and says why. Events are rate-limited per kind.
#include "math.hpp"
#include "bot_probe.hpp"
#include <algorithm>
#include <cmath>
#include <array>
#include <cstdint>
#include <cstring>

namespace spidy::world_trace {
enum Kind : uint32_t { npc = 0, spawn = 1, mission = 2, kinds = 3 };
inline constexpr const char* kindNames[kinds] = {"NPC", "SPAWN", "MISSION"};
enum Event : uint32_t { dialogue = 1, spawned = 2, despawned = 3, missionStart = 4, missionStop = 5, missionState = 6 };
inline constexpr const char* eventNames[] = {"none", "dialogue", "spawned", "despawned",
                                             "mission start", "mission stop", "mission state"};
// What a class (or a class it derives from) says, by its decorated RTTI name.
enum Bits : uint32_t {
    dialogueBit = 1, civilianBit = 2, enemyBit = 4, allyBit = 8, companionBit = 16, pedestrianBit = 32,
    botBit = 64, spawnerBit = 128, missionBit = 256, objectiveBit = 512, activityBit = 1024, heroBit = 2048,
    // Update 38.5 (SKIP MISSION research, read-only): a class named for a checkpoint (CANDIDATE).
    checkpointBit = 4096
};
inline bool has(const char* name, const char* part) { return name && std::strstr(name, part) != nullptr; }
inline bool exact(const char* name, const char* full) { return name && !std::strcmp(name, full); }
inline uint32_t bitsOf(const char* name) {
    if (!name || std::strncmp(name, ".?A", 3))
        return 0;
    uint32_t b{};
    if (exact(name, ".?AVPlayDialogComponent@@") || exact(name, ".?AVSharedPlayDialogComponent@@") ||
        exact(name, ".?AVConversationActiveSpeakerComponent@Conversations@@"))
        b |= dialogueBit;
    if (exact(name, ".?AVCivilianBot@@"))
        b |= civilianBit;
    if (exact(name, ".?AVThugBot@@"))
        b |= enemyBit;
    if (exact(name, ".?AVAllyBot@@"))
        b |= allyBit;
    if (exact(name, ".?AVMissionFollowBot@@"))
        b |= companionBit;
    if (exact(name, ".?AVPedestrianMover@@"))
        b |= pedestrianBit;
    if (exact(name, ".?AVBotMoverManager@@"))
        b |= botBit;
    if (exact(name, ".?AVHeroLocal@Hero@@"))
        b |= heroBit;
    if (!has(name, "Event@") && !has(name, "Data@") && !has(name, "Config@") && !has(name, "Prius")) {
        if (has(name, "Spawner") || has(name, "SpawnSystem") || has(name, "SpawnManager"))
            b |= spawnerBit;
        if (!std::strncmp(name, ".?AVMission", 11) || !std::strncmp(name, ".?AVMLC_Mission", 15))
            b |= missionBit;
        if (!std::strncmp(name, ".?AVObjective", 13) || !std::strncmp(name, ".?AVMLC_Objective", 17))
            b |= objectiveBit;
        if (!std::strncmp(name, ".?AVActivity", 12) || !std::strncmp(name, ".?AVOpenWorldActivity", 21))
            b |= activityBit;
        if (has(name, "Checkpoint"))
            b |= checkpointBit;
    }
    return b;
}
// The speaker's kind for the log, from its actor's bits (most specific first).
inline const char* speakerOf(uint32_t b) {
    return b & heroBit ? "hero" : b & companionBit ? "named (mission companion)" : b & allyBit ? "ally/police (candidate)"
         : b & enemyBit ? "enemy" : b & civilianBit ? "civilian" : b & pedestrianBit ? "pedestrian"
         : b & botBit ? "bot" : "unknown";
}
// Degrees between the look and the way to `at`; 180 when unknown.
inline float gazeDegrees(Vec3 eye, Vec3 forward, Vec3 at) {
    const Vec3 to = at - eye;
    const float lt = length(to), lf = length(forward);
    if (!(lt > 1e-3f) || !(lf > 1e-3f) || !finite(to) || !finite(forward))
        return 180.f;
    return std::acos(std::clamp(dot(to, forward) / (lt * lf), -1.f, 1.f)) * 57.2957795f;
}
inline constexpr float gazeCone = 12.f; // looked at: within this of the head's forward
// Per kind, at most `burst` events then one per `every` seconds (a crowd spawning is one
// line with a count, not hundreds).
struct RateLimit {
    float tokens = 8, burst = 8, every = .5f;
    uint64_t dropped{};
    bool take(float dt) {
        if (std::isfinite(dt) && dt > 0)
            tokens = std::min(burst, tokens + dt / every);
        if (tokens >= 1) {
            tokens -= 1;
            return true;
        }
        ++dropped;
        return false;
    }
    // Tokens back for the time that passed (take(0) then only spends).
    void refill(float dt) {
        if (std::isfinite(dt) && dt > 0)
            tokens = std::min(burst, tokens + dt / every);
    }
};
// The words of a block that changed: a cheap hash over them.
inline uint64_t hashWords(const uint64_t* w, unsigned n) {
    uint64_t h = 1469598103934665603ull;
    for (unsigned i = 0; i < n; ++i)
        h = (h ^ w[i]) * 1099511628211ull;
    return h;
}
// A short printable text (a candidate line id or name): at least 4 characters of
// [A-Za-z0-9_ ./:-], ending at a zero.
inline unsigned printable(const char* s, unsigned most) {
    unsigned n = 0;
    for (; n < most && s[n]; ++n) {
        const char c = s[n];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == ' ' ||
              c == '.' || c == '/' || c == ':' || c == '-'))
            return 0;
    }
    return n >= 4 && n < most ? n : 0;
}

inline constexpr unsigned events = 64, nameChars = 48, textChars = 64, census = 24;
struct Entry {
    float seconds{}, x{}, y{}, z{};    // since the log began; where, relative to the player (world axes, m)
    float distance{}, gaze{};          // metres; degrees off the look (180 unknown)
    uint32_t event{}, bits{};          // Event; the actor's Bits
    uint32_t changed{}, count{};       // dialogue/state: a mask of the 16 words that changed; spawn: actors in the batch
    char name[nameChars]{};            // the class (archetype) or the dialogue component's class
    char text[textChars]{};            // speaker kind / spawner class / a text the component points at
};
inline constexpr uint32_t dataMagic = 0x43525457; // WTRC
struct Data {
    uint32_t magic = dataMagic, version = 1, bytes = 11248, status{}; // status 0 off, 1 running, 2 stopped (fault), 3 no registry yet
    uint32_t on[kinds]{}, fault{};                                // fault: why it stopped (1 exception, 2 registry, 3 slow)
    uint64_t scans{}, entries{}, classes{}, actors{};
    uint64_t counts[7]{}, dropped[kinds]{};
    float lastScanMs{}, meanScanMs{}, peakScanMs{}, intervalS{};
    uint32_t next{}, censusCount{};
    Entry ring[events]{};
    struct Census {
        char name[nameChars]{};
        uint32_t bits{}, live{};
    } census_[census]{};
    uint64_t sequence{};
};
} // namespace spidy::world_trace
namespace spidy::world_trace {
static_assert(sizeof(Data) == 11248);
// The thread (src/world_trace.cpp, Windows only): started with the VR session, stopped with it.
void start(uintptr_t imageBase);
void stop();
// The head's forward in the world, each image (three relaxed stores).
void look(Vec3 forward);
// The hologram's WORLD LOG toggle (all three tracers).
void setOn(bool);
bool isOn();
// The nearest enemy (ThugBot) at the last scan, metres; 1e9 when none or the log is off.
float nearestEnemyMetres();
// Update 37.2: the nearest civilian or pedestrian, metres; 1e9 when none or the log is off.
float nearestPersonMetres();
// Update 38: the enemies (ThugBot) within enemyReach of the hero, nearest first, their feet
// relative to the hero's root in the world's axes (y up), re-read at 10 Hz from the last
// full scan's records; 0 when the log is off or the list is older than 0.5 s.
inline constexpr uint32_t enemySlots = 16;
inline constexpr float enemyReach = 30.f;
// ids (optional): each one's id, the same enemy the same id from scan to scan (SpidyVR's
// lock and lunge keep to one man by it).
uint32_t enemies(std::array<Vec3, enemySlots>& out, std::array<uint64_t, enemySlots>* ids = nullptr);
// How many times a second the list was refreshed (about 10).
float enemyScanHz();
// Version 0.0.62 ENEMY TRACKING: LIVE (true, the default) refreshes the list 30 times a second,
// 0.0.6 (false) 10 times.
void setFastRefresh(bool on);
// The hero's world position the last enemy list was measured from (false: none).
bool heroAtRefresh(Vec3& out);
// Version 0.0.3: the same list in world positions (feet, y up) with the list's age in seconds
// (WALL STICK measures speeds with it; the hero-relative list carries the player's own motion).
uint32_t enemiesWorld(std::array<Vec3, enemySlots>& out, std::array<uint64_t, enemySlots>& ids, float& ageSeconds);
// 0.0.74 test 2b CRIME GPS (crime_gps074.hpp): every enemy record the last scan holds within farReach of
// the hero (nearest first, world feet), not only the near ones; its age in seconds. Which records the game
// keeps (how far out) is the game's streaming - CANDIDATE. Read-only, the same scan (no new reads of the game
// beyond each listed record's position).
inline constexpr uint32_t farSlots = 32;
inline constexpr float farReach = 600.f;
uint32_t enemiesFar(std::array<Vec3, farSlots>& out, float& ageSeconds);
// Version 0.0.5 DAMAGE PROBE (damage_probe.hpp), read-only: the same enemies' hit points, each one's
// BotHealth component (vtable 0x531b0a0, owner record +0x10, as the CONFIRMED HeroHealth; hit
// points +0xd0, most +0xa0) found in the full scan and re-read with the 10 Hz refresh.
struct Vital {
    uint64_t id{};
    float hp{}, most{};
    uint32_t have{};
};
uint32_t enemyVitals(std::array<Vital, enemySlots>& out);
uint64_t healthComponents(); // BotHealth components the last scan found
uint64_t heroRecordNow();    // the hero's record the last scan found (0: none)
// Version 0.0.3 BOT STATE PROBE (include/spidy/bot_probe.hpp): on or off, and the enemy to watch
// (an id from enemies(); 0 = the nearest). Read-only.
void watchBot(bool on, uint64_t id);
// Its findings so far (false when it never ran).
bool botProbe(bot_probe::State& out);
} // namespace spidy::world_trace
