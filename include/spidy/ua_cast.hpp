#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
// Made by Agent Mulder.
//
// ULTIMATE ALLIANCE EXPANSION CHARACTERS (CANDIDATE): a UA place's people, from the player's own MUA1 PS2 disc
// (tools/ua/convert_ua.py; nothing of the game is shipped). Models\UA\<Place>\Cast\scene.glb holds every "look" (a
// skin in one of the map's idle performances) as a few whole frames of its loop, one after another (the same
// triangles in the same order each frame, so a frame between two is a straight blend); cast.txt says where each
// character stands in which look; talk.txt holds their conversations as the game has them (every line, reply and
// choice, who may start which, the mission they lead to) and Voice\v0000.wav... their recorded lines.
// Walk up to one, look or point at him, A (or a trigger) and he talks: his line as a subtitle under him (his voice
// when the clip is there), Spider-Man's replies and the game's choices as a menu (stick up / down or point, A or
// trigger picks; B ends). The missions the hub leads to (Wyatt's "Take us to the Omega Base") are not playable yet:
// they end the talk on MISSIONS: COMING SOON. Pure: no game reads, no Windows. Fail-safe: a cast that does not read
// is not drawn and nobody talks (the room still walks).
#include "spidy/hideout074.hpp"
#include "spidy/math.hpp"
#include "spidy/mesh_model.hpp"
#include "spidy/vertex.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace spidy::ua_cast {

inline constexpr const char* castHeader = "HEROVR-UA-CAST 1";
inline constexpr const char* talkHeader = "HEROVR-UA-TALK 1";
inline constexpr const char* hero = "spiderman"; // who the player is to the game's start conditions

// ---- cast.txt
struct Look {
    std::string name;
    uint32_t first{}, perFrame{}, frames{}; // triangles: frame f is [first + f * perFrame, + perFrame)
    float loopSeconds{};
    bool turns{}; // stands (may turn to face whom he talks to); a seated look never turns
};
struct Npc {
    std::string id;
    uint32_t look{};
    Vec3 at{};          // room (glTF) space, the feet
    float yawDeg{};     // his facing: +x turned about +y (the spawn's sense, hideout074::Room::spawnYaw)
    std::string group;  // "" always there; "pairNN": one pair of these a visit (the map's rand_npcs.py)
    std::string talk;   // his conversation in talk.txt, "" none
    std::string label;  // "NICK FURY"
};
struct Cast {
    std::vector<Look> looks;
    std::vector<Npc> npcs;
    std::vector<std::string> pairs; // the groups, sorted
};
inline constexpr size_t mostLooks = 64, mostNpcs = 256, mostFrames = 64;
// cast.txt; false (error said) on anything wrong. `triangles`: the Cast model's count (0: not checked) - every
// look's frames must be inside it.
bool parseCast(const std::string& text, Cast& out, std::string& error, uint32_t triangles = 0);

// ---- talk.txt
enum class Act : uint8_t { none, missions, flag };
enum class Cond : uint8_t { always, never, heroIs, heroNot };
struct Line {
    std::string speaker; // the token (NICKFURY, PLAYER, MENU)
    int voice = -1;      // Voice\v<voice>.wav, -1 none
    float seconds{};     // how long it is said (its clip's length, or a reading time)
    std::string text;    // empty: a menu's header (its responses are the choices)
};
struct Resp {
    int line{}, to = -1;
    bool end{};
    Act act{};
    int flag{};  // Act::flag's number
    int needs{}; // shown only once this flag is set (the game's disallowResponseOnVar: a follow-up question; 0 always)
    Cond cond{}; // for whom (a hero's own reply; never: story the hub cannot know)
    std::vector<std::string> heroes;
    std::string text; // %BLANK% (go on), %PLAYER%: <said>, %YES%, %NO%, or a menu choice
};
struct Start {
    int line{};
    bool once{};
    Cond cond{};
    std::vector<std::string> heroes;
};
struct Conv {
    std::string name;
    std::vector<Start> starts;
    std::vector<Line> lines; // line i is lines[i]
    std::vector<Resp> resps;
};
struct Talk {
    std::vector<std::pair<std::string, std::string>> speakers; // token, shown name
    std::vector<Conv> convs;
    int find(const std::string& name) const; // -1 none
    std::string nameOf(const std::string& token) const;
};
inline constexpr size_t mostConvs = 256, mostLines = 4096;
bool parseTalk(const std::string& text, Talk& out, std::string& error);

// A response's words as shown ("%PLAYER%: Hi" -> "Hi", "%YES%" -> "YES"); blank: "".
std::string shownText(const std::string& raw);
bool isBlank(const std::string& raw);
bool isPlayerSaid(const std::string& raw); // "%PLAYER%: ..."
// `text` broken into lines at most `width` metres wide at capital height `height` (overlay_text's font); at most
// `most` lines (the last ends "...").
std::vector<std::string> wrap(const std::string& text, float height, float width, size_t most = 4);

// ---- who is there this visit
// Every character always there, and the pair `pair` (an index into Cast::pairs; out of range: none).
std::vector<uint32_t> shownFor(const Cast& cast, uint32_t pair);

// ---- whom he is talking to
inline constexpr float talkReach = 3.f;    // metres, feet to feet (room space, flat)
inline constexpr float aimDegrees = 20.f;  // the look or the pointing hand this near his chest
inline constexpr float chestHeight = 1.3f; // above his feet (seated: .9)
// The character (index into cast.npcs, among `shown`, with a conversation) the eye's look or a hand's ray picks:
// within talkReach of `feet` and aimDegrees of either ray (both in room space); the nearest in angle. -1 none. A zero
// `look` (game_xr: standing on the gold ring, where Wyatt stands) leaves the hand's ray alone.
int pick(const Cast& cast, const std::vector<uint32_t>& shown, Vec3 feet, Vec3 eye, Vec3 look, bool handOn, Vec3 hand,
         Vec3 handDir);

// ---- a conversation
struct ChatIn {
    float dt{};
    bool next{};       // a fresh A or trigger: on (skip the line), or pick the lit choice
    bool quit{};       // a fresh B: end the talk
    int move{};        // -1 up, +1 down (a fresh stick flick)
    int point = -1;    // the choice a hand points at (-1 none): lights it
};
struct ChatOut {
    int voice = -1;    // start this clip now
    bool stopVoice{};  // the line was cut short (or ended)
    bool ended{};      // the talk is over this frame
    bool missions{};   // ...because the player chose a mission (MISSIONS: COMING SOON)
};
class Chat {
public:
    // Opens `conv` of `talk` (its first start for `hero` that passes, a spent run-once skipped); false: none.
    bool begin(const Talk& talk, int conv, const std::string& heroName = hero);
    ChatOut step(const ChatIn& in);
    void end() { conv_ = -1; }
    bool active() const { return conv_ >= 0; }
    int conv() const { return conv_; }
    // What is on the panel now.
    std::string speaker() const; // the shown name ("NICK FURY", "SPIDER-MAN"), "" for a menu
    std::string text() const;    // the line, or Spider-Man's reply
    std::vector<std::string> choices() const; // the menu (empty: none)
    int hot() const { return hot_; }
    bool choosing() const { return phase_ == Phase::choosing; }
    int voiceNow() const; // the clip of the line being said (just opened: play it), -1 none
    float lineSeconds() const { return seconds_; }
    // Run-once starts and flags kept for the session (each conversation its own, as the game's conv_vars).
    void resetMemory() { used_.clear(); flags_.clear(); }
    const std::string& heroName() const { return hero_; }

private:
    enum class Phase : uint8_t { saying, choosing, replying };
    const Talk* talk_{};
    int conv_ = -1, line_ = -1, hot_ = 0, after_ = -1;
    Phase phase_{};
    float clock_{}, seconds_{};
    std::string reply_, hero_ = hero;
    std::vector<int> visible_; // resps of this line shown (index into Conv::resps)
    std::vector<std::pair<int, int>> used_;  // (conv, start) spent run-once
    std::vector<std::pair<int, int>> flags_; // (conv, flag)
    bool flagSet(int flag) const;
    void enter(int line, ChatOut& out);
    void follow(int resp, ChatOut& out);
};

// ---- turning to face him (a standing look only)
inline constexpr float turnDegreesPerSecond = 160.f;
// Yaw (degrees) from `yawDeg` toward facing `target` from `at` (room space), at most turnDegreesPerSecond * dt.
float turnToward(float yawDeg, Vec3 at, Vec3 target, float dt);

// ---- the draw
struct Pose {
    uint32_t npc{};
    float yawDeg{};
    float clock{}; // seconds into his loop
};
// The posed characters (each look's frame at `clock`, blended with the next) placed in the room by `place`, as
// hideout074::appendRoom draws the room (textured: shape = u, v, layer, 1), each triangle lit a little by its face.
// Into the model sink; false (nothing added) on any fault or past `budget` vertices.
inline constexpr uint32_t drawBudget = mesh_model::uaCastDrawnTriangles;
bool append(std::vector<Vertex>& sink, const mesh_model::Mesh& model, const Cast& cast, const std::vector<Pose>& poses,
            const hideout074::Place& place, size_t budget);
// The frame pair and blend for `clock` in a look's loop (frames sampled evenly, the last blending to the first).
void frameAt(const Look& look, float clock, uint32_t& a, uint32_t& b, float& t);

} // namespace spidy::ua_cast
