#pragma once
// HERO VR 0.0.76 SM1: the heroes' fight in iilyn's Spider-Man VR mod (SpidyVR by Ilya Mezerowsky, GPL-3.0,
// https://github.com/IlyaMez/SpidyVR). Made by Agent Mulder.
//
// Every enemy type: the add-on finds the game's bots itself (a port of his own target scan, game_targets.cpp in his
// 0.7.0 sources, enemy parts only) and lands each blow through his exported SpidyPunchTest, the same call his punches
// use. The part here is pure (no game memory): the class rules, the bodies, which enemy a hand, a beam, a thrown
// hammer or shield touches, and how often one enemy may be hit. sm1_targets.cpp reads the game (Windows only).
#include "spidy/math.hpp"

#include <cstdint>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace spidy::sm1::combat {
// What a component's class (or any class it derives from) says about its actor (his game_targets rules, by the
// classes' decorated RTTI names).
enum Bits : uint32_t {
    bot = 1u << 0,       // BotMoverManager: the actor is a bot
    thug = 1u << 1,      // ThugBot (and the bosses derived from it)
    civilian = 1u << 2,  // CivilianBot
    ally = 1u << 3,      // AllyBot, MissionFollowBot
    neutral = 1u << 4,   // BirdBot, Helicopter, SilverSableCraftBot
    hover = 1u << 5,     // HoverMoverManager (a drone)
    webbable = 1u << 6,  // StatusEffectTrackerWebbed
    health = 1u << 7,    // Health (and what derives from it)
    botHealth = 1u << 8, // BotHealth
    down = 1u << 9,      // a state with the bot out of the fight (death anim, ragdoll, stuck to a wall)
    held = 1u << 10,     // a state in which something else moves him (a car seat, carried, a cinematic...)
};
// The bits one class name gives by itself (0 for a class no rule knows).
uint32_t ruleBits(std::string_view decorated);
// An actor the powers may hit: a bot with a health component that is no civilian, ally or neutral (bird, helicopter).
inline bool enemy(uint32_t actorBits) {
    return (actorBits & bot) && (actorBits & (health | botHealth)) && !(actorBits & (civilian | ally | neutral));
}

// A bot's body: a vertical capsule from `low` to `high` over his feet (his game_targets::Size).
struct Size {
    float low = .4f, high = 1.6f, radius = .45f;
    // A height tuned on a street thug, stretched to this bot's capsule (a thug's come back unchanged).
    float height(float thug) const { return low + (thug - .4f) * (high - low) / 1.2f; }
};
struct Target {
    uint64_t record = 0;  // the actor record (SpidyPunchTest's victim)
    uint64_t health = 0, machine = 0, mover = 0;
    uint32_t bits = 0, handle = 0; // handle: the bot component's registry handle
    Size size;
    Vec3 feet{};          // the record's transform, game world (Y up)
    float hp = -1;        // -1: unknown
    Vec3 chest() const { return feet + Vec3{0, size.height(1.15f), 0}; }
};
// How far `p` is from the body (0 inside it).
float bodyDistance(const Target& t, Vec3 p);
// How far the segment a..b passes from the body (0 through it).
float segmentDistance(const Target& t, Vec3 a, Vec3 b);
// The enemy a shot from `from` along `dir` goes for: within `coneDegrees` of the aim (or the body within `slack`
// metres of the line) and `range`, the smallest angle first (near ones win ties). -1 for none.
int aimed(const std::vector<Target>& list, Vec3 from, Vec3 dir, float range, float coneDegrees, float slack = .35f);
// Up to `most` enemies whose bodies come within `reach` of `p`, nearest first.
std::vector<int> nearby(const std::vector<Target>& list, Vec3 p, float reach, size_t most);
// Up to `most` enemies the segment a..b passes within `reach` of, nearest to `a` first.
std::vector<int> along(const std::vector<Target>& list, Vec3 a, Vec3 b, float reach, size_t most);

// One enemy is hit at most once per `every` seconds by the same thing (a claw swipe, a thrown hammer's pass).
class HitLimiter {
  public:
    bool allow(uint64_t record, double now, double every);
    void clear() { last_.clear(); }

  private:
    std::unordered_map<uint64_t, double> last_;
};
} // namespace spidy::sm1::combat

// --- The game's bots (sm1_targets.cpp, Windows only): READ ONLY, CANDIDATE addresses ----------------------------------
// The RVAs are his game_targets.cpp's for Spider-Man.exe 4.630.0.0 (CANDIDATE: from his sources, not checked here
// against the game by hand). start() checks them before anything else: the exe must be 4.630.0.0 and the two health
// vtables must carry the Health / BotHealth RTTI names; otherwise nothing is read and the powers keep to his own
// ringed enemy. The scan runs on its own lowest-priority thread every half second, only while wanted.
namespace spidy::sm1::targets {
inline constexpr uint32_t registryTable = 0x7a44320, registryCount = 0x7a44340, syncStaticStateMachine = 0x4f843d0,
                          moverBodySize = 0x500e870, botHealthVtable = 0x3839e98, plainHealthVtable = 0x3839e08;
inline constexpr uint32_t healthPointsAt = 0x88; // float, the component's hit points
using Log = void (*)(const char*);
// The checks and the thread (idle until wanted). False: the addresses do not fit this game; nothing more is read.
bool start(Log log);
void want(bool on);
// The enemies in the fight now (up, not held), with their positions and health read now. Returns the scans so far.
uint64_t fighting(std::vector<combat::Target>& out);
} // namespace spidy::sm1::targets
