#pragma once
// 79.6 GAME WALL ANIMS: what the wrist row (VR SETTINGS > BODY AND VIEW > GAME WALL ANIMS, live.txt game_wall_anims)
// changes, and the WALL WATCH that goes with it.
//
// OFF (the default) is HEROVR's own wall handling exactly as 79.5: the swing's wall (the body held off the wall, the
// actor in the game's AIR state, the game's wall states never run). ON turns the game's own wall crawl on
// (live.txt wall_crawl 1), its hand climb on (hand_climb 1) and the level view (wall_view 0), so the game's own
// HeroStateWallCrawl / HeroStateWallRun animations take the body on walls. The game's own hero state is NOT read
// (no SM2 address for it is evidenced); the WATCH works from what the swing bridge reports: the surface that holds
// the player (none / the swing's wall / the game's crawl / a floor crawl), airborne, grounded, owned, position
// and velocity, and from the view's tilt (GameMotionFrame::viewTilt).
//
// The WATCH (pure, unit-tested; tests/wall_anims_tests.cpp):
//   contact   the player gets onto a wall (any surface, counted by kind), with the speed and the game's collision flags
//   hand-over the surface changes while on a wall (the swing's wall <-> the game's crawl)
//   leave     off a wall, how long, and why: INTENT (a jump / web / leap within 0.5 s) or not
//   drop      off the game's wall (or the swing's) with no intent and falling (vy below -3 m/s 0.4 s later)
//   far fall  a fall of 80 m with no web: logged as OUT OF WORLD?
//   flip      79.7: the head's yaw jumped over 60 degrees in one frame on a wall with no right stick: the tracking
//             space puts it back (GameTrackingRig, YAW JUMP SAFETY) and the watch logs it (79.6's tilt detector
//             never fired)
// Actions (ON only): a drop off the game's wall puts HEROVR's wall back for 15 s (the next touch is held), a third
// drop in a session turns GAME WALL ANIMS back OFF.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace spidy::wall_anims {
enum Cls : uint32_t { none, air, ground, ownWall, gameWall, floorCrawl, classes };
inline constexpr const char* clsName[classes] = {"NONE", "AIR", "GROUND", "OWN WALL", "GAME WALL", "FLOOR CRAWL"};
enum Kind : uint32_t { contact, handOver, leave, drop, farFall, flip, toggle, deferred, recover, autoOff, kinds };
inline constexpr const char* kindName[kinds] = {"CONTACT", "HAND-OVER", "LEAVE", "DROP", "OUT OF WORLD?", "VIEW FLIP",
                                                "TOGGLE", "DEFERRED", "RECOVER", "AUTO-OFF"};
inline constexpr float intentSeconds = 0.5f, dropWait = 0.4f, dropSpeed = -3.f, farFallMetres = 80.f,
                       yawJumpDegrees = 60.f, fallbackSeconds = 15.f;
inline constexpr uint32_t autoOffDrops = 3;

inline bool isWall(Cls c) { return c == ownWall || c == gameWall || c == floorCrawl; }
// The class for game_swing::Data::wall (0 none, 1 the swing's wall, 2 the game's crawl, 3 floor crawl) and flags.
inline Cls classify(uint32_t surface, bool airborne, bool grounded) {
    switch (surface) {
    case 1: return ownWall;
    case 2: return gameWall;
    case 3: return floorCrawl;
    default: return airborne ? air : grounded ? ground : none;
    }
}

struct Input {
    double session{};        // the session's seconds (summed frame seconds)
    bool enabled{};          // GAME WALL ANIMS is ON and applied (not in a recovery fallback)
    uint32_t surface{};      // game_swing::Data::wall
    bool airborne{}, grounded{}, owned{}, intent{}, webbed{};
    float y{}, vy{}, speed{}; // position.y, velocity.y, |velocity|
    uint32_t collisionFlags{};
    float yawJumpDegrees{};  // GameMotionFrame::yawJumpDegrees: the yaw the safety put back this frame (0 none)
};
struct Note {
    Kind kind{};
    Cls from{}, to{};
    double session{};
    float a{}, b{};
    uint32_t n{};
};
struct Action {
    bool recover{}, autoOff{};
};
struct EventRec { // 16 bytes, in Data
    uint32_t ms{};
    uint8_t kind{}, from{}, to{}, n{};
    float a{}, b{};
};
inline constexpr uint32_t magic = 0x4c415737; // 7WAL
inline constexpr uint32_t version = 1;
inline constexpr uint32_t ringSize = 24;
struct Data {
    uint32_t magic = wall_anims::magic, version = wall_anims::version, bytes = 512, flags{};  // flags: 1 ON applied, 2 wanted, 4 deferred, 8 fallback, 16 auto-off
    uint32_t contacts[classes]{};
    uint32_t handOvers{}, drops{}, dropsGame{}, farFalls{}, flips{}, recoveries{}, toggles{}, deferrals{};
    float seconds[classes]{};
    float maxYawJumpDegrees{}, lastContactSpeed{};
    uint32_t eventTotal{}, eventCount{}, sessionMs{}, spare{};
    EventRec events[ringSize]{};
    uint32_t pad[2]{};
};
static_assert(sizeof(Data) == 512, "SpidyWallAnimsData is 512 bytes (tools/wall_anims_record.py)");

class Watch {
public:
    // One frame. `dt` the frame's seconds. Notes (this frame's events) are appended to `notes`.
    Action update(float dt, const Input& in, std::vector<Note>& notes) {
        Action act;
        const Cls now = classify(in.surface, in.airborne, in.grounded);
        data_.seconds[now] += dt;
        data_.maxYawJumpDegrees = std::max(data_.maxYawJumpDegrees, std::abs(in.yawJumpDegrees));
        if (in.intent)
            lastIntent_ = in.session;
        if (isWall(now) && !isWall(prev_)) {
            ++data_.contacts[now];
            data_.lastContactSpeed = in.speed;
            contactAt_ = in.session;
            contactClass_ = now;
            pendingDrop_ = false;
            falling_ = false;
            note(notes, {contact, prev_, now, in.session, in.speed, static_cast<float>(in.collisionFlags), in.owned ? 1u : 0u});
        } else if (isWall(now) && isWall(prev_) && now != prev_) {
            ++data_.handOvers;
            contactClass_ = now;
            note(notes, {handOver, prev_, now, in.session, in.speed, 0, 0});
        } else if (!isWall(now) && isWall(prev_)) {
            const bool withIntent = in.session - lastIntent_ <= intentSeconds;
            note(notes, {leave, prev_, now, in.session, static_cast<float>(in.session - contactAt_), withIntent ? 1.f : 0.f, 0});
            leftClass_ = prev_;
            leftAt_ = in.session;
            leftY_ = in.y;
            pendingDrop_ = !withIntent;
            falling_ = true;
            farFallSaid_ = false;
        }
        if (pendingDrop_ && in.session - leftAt_ >= dropWait) {
            pendingDrop_ = false;
            if (in.session - lastIntent_ > intentSeconds && !isWall(now) && in.vy < dropSpeed && !in.webbed) {
                ++data_.drops;
                if (leftClass_ == gameWall)
                    ++data_.dropsGame;
                note(notes, {drop, leftClass_, now, in.session, in.vy, in.y - leftY_, 0});
                if (in.enabled && leftClass_ == gameWall) {
                    act.recover = true;
                    ++data_.recoveries;
                    note(notes, {recover, leftClass_, now, in.session, fallbackSeconds, 0, data_.dropsGame});
                    if (data_.dropsGame >= autoOffDrops) {
                        act.autoOff = true;
                        note(notes, {autoOff, leftClass_, now, in.session, 0, 0, data_.dropsGame});
                    }
                }
            }
        }
        if (falling_) {
            if (isWall(now) || in.grounded || in.webbed || in.intent) {
                falling_ = false;
            } else if (!farFallSaid_ && leftY_ - in.y > farFallMetres && in.vy < -20.f) {
                farFallSaid_ = true;
                ++data_.farFalls;
                note(notes, {farFall, leftClass_, now, in.session, leftY_ - in.y, in.vy, 0});
                if (in.enabled && leftClass_ == gameWall)
                    act.recover = true;
            }
        }
        // 79.7: a yaw jump the tracking space put back: counted and logged (see the console's rate limit).
        if (in.yawJumpDegrees != 0.f) {
            ++data_.flips;
            note(notes, {flip, now, now, in.session, in.yawJumpDegrees, 0, data_.flips});
        }
        prev_ = now;
        data_.sessionMs = static_cast<uint32_t>(std::min(in.session * 1000.0, 4e9));
        return act;
    }
    // Events the frame loop adds itself (a toggle, a deferral).
    void add(std::vector<Note>& notes, Kind kind, double session, float a = 0, uint32_t n = 0) {
        if (kind == toggle)
            ++data_.toggles;
        if (kind == deferred)
            ++data_.deferrals;
        note(notes, {kind, none, none, session, a, 0, n});
    }
    void flags(uint32_t f) { data_.flags = f; }
    const Data& data() const { return data_; }
    uint32_t revision() const { return revision_; }
    bool onWall() const { return isWall(prev_); }
    Cls current() const { return prev_; }

private:
    void note(std::vector<Note>& notes, Note n) {
        notes.push_back(n);
        ++revision_;
        auto& d = data_;
        if (d.eventCount == ringSize) {
            std::memmove(d.events, d.events + 1, sizeof(EventRec) * (ringSize - 1));
            --d.eventCount;
        }
        auto& e = d.events[d.eventCount++];
        e.ms = static_cast<uint32_t>(std::min(n.session * 1000.0, 4e9));
        e.kind = static_cast<uint8_t>(n.kind);
        e.from = static_cast<uint8_t>(n.from);
        e.to = static_cast<uint8_t>(n.to);
        e.n = static_cast<uint8_t>(std::min<uint32_t>(n.n, 255));
        e.a = n.a;
        e.b = n.b;
        ++d.eventTotal;
    }
    Data data_{};
    Cls prev_ = none, contactClass_ = none, leftClass_ = none;
    double lastIntent_ = -100, contactAt_{}, leftAt_{};
    float leftY_{};
    bool pendingDrop_{}, falling_{}, farFallSaid_{};
    uint32_t revision_{};
};

// The console / log line for one note.
inline std::string text(const Note& n) {
    char line[260];
    const char* from = clsName[std::min<uint32_t>(n.from, classes - 1)];
    const char* to = clsName[std::min<uint32_t>(n.to, classes - 1)];
    switch (n.kind) {
    case contact:
        std::snprintf(line, sizeof line, "79.7 WALL %.1fs: CONTACT %s (from %s) speed %.1f m/s, game collision flags 0x%X",
                      n.session, to, from, static_cast<double>(n.a), static_cast<unsigned>(n.b));
        break;
    case handOver:
        std::snprintf(line, sizeof line, "79.7 WALL %.1fs: HAND-OVER %s -> %s at %.1f m/s", n.session, from, to,
                      static_cast<double>(n.a));
        break;
    case leave:
        std::snprintf(line, sizeof line, "79.7 WALL %.1fs: LEFT %s after %.1fs -> %s (%s)", n.session, from,
                      static_cast<double>(n.a), to, n.b > 0 ? "by intent: jump, web or leap" : "no jump or web");
        break;
    case drop:
        std::snprintf(line, sizeof line, "79.7 WALL %.1fs: DROP off %s, falling %.1f m/s, %.1f m below the wall's last point",
                      n.session, from, static_cast<double>(n.a), static_cast<double>(-n.b));
        break;
    case farFall:
        std::snprintf(line, sizeof line, "79.7 WALL %.1fs: OUT OF WORLD? fallen %.0f m (%.0f m/s) with no web since %s",
                      n.session, static_cast<double>(n.a), static_cast<double>(n.b), from);
        break;
    case flip:
        std::snprintf(line, sizeof line, "79.7 WALL %.1fs: VIEW YAW JUMP of %.0f deg put back, #%u", n.session,
                      static_cast<double>(n.a), n.n);
        break;
    case toggle:
        std::snprintf(line, sizeof line, "79.7 WALL %.1fs: GAME WALL ANIMS %s", n.session, n.a > 0 ? "ON" : "OFF");
        break;
    case deferred:
        std::snprintf(line, sizeof line, "79.7 WALL %.1fs: GAME WALL ANIMS ON waits: on the swing's wall now, applies when you leave it",
                      n.session);
        break;
    case recover:
        std::snprintf(line, sizeof line, "79.7 WALL %.1fs: RECOVER: HEROVR's wall holds you for %.0fs (drop #%u off the game's wall)",
                      n.session, static_cast<double>(n.a), n.n);
        break;
    case autoOff:
        std::snprintf(line, sizeof line, "79.7 WALL %.1fs: AUTO-OFF: %u drops off the game's wall this session, GAME WALL ANIMS back OFF",
                      n.session, n.n);
        break;
    default:
        line[0] = 0;
    }
    return line;
}
// The summary: '79.7 WALL ...' (console every 20 s while something changed, and the report's line).
inline std::string summary(const Data& d) {
    char line[420];
    std::snprintf(line, sizeof line,
                  "79.7 WALL: GAME WALL ANIMS %s; contacts own %u / game %u / floor %u; hand-overs %u; drops %u (game %u); "
                  "out-of-world? %u; view yaw jumps %u; recoveries %u; on own wall %.0fs, game wall %.0fs, floor crawl %.0fs, air %.0fs",
                  (d.flags & 1) ? "ON" : "OFF", d.contacts[ownWall], d.contacts[gameWall], d.contacts[floorCrawl], d.handOvers,
                  d.drops, d.dropsGame, d.farFalls, d.flips, d.recoveries, static_cast<double>(d.seconds[ownWall]),
                  static_cast<double>(d.seconds[gameWall]), static_cast<double>(d.seconds[floorCrawl]),
                  static_cast<double>(d.seconds[air]));
    return line;
}
} // namespace spidy::wall_anims
