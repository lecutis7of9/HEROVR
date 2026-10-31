#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
// Made by Agent Mulder.
//
// Version 0.0.73 MP SOLO TEST (private test build, branch mp-solo-test): the smallest free multiplayer
// pipe - plain UDP, no server to pay for. This file is the pure part (no sockets, no game): the versioned
// packet, its checks, the receive side's interpolation, the loopback echo's delay line and the partner
// placement that puts a remote player about 3 m in front of the player, facing him. mp_socket / mp_session
// own the sockets and the thread; game_xr only publishes the local pose and draws what view() says.
//
// The packet (version 1, little-endian, 24 bytes of header and checksum around a typed payload):
//   0  magic 'HVMP'   4 version   5 type   6 role   7 reserved (0)
//   8  sequence u32   12 sender clock ms u32   16 echo hold ms u16   18 payload bytes u16
//   20 payload ...    then CRC-32 (zlib's) of everything before it
// POSE (112 bytes): hero, suit (255 none), action flags, 0 | suit folder hash (FNV-1a) | root (feet) x y z,
//   yaw (radians about +y; forward is -z turned by it) | head, left grip, right grip: position (metres, in
//   the root's yawed frame, from the feet) and orientation (x y z w) | curl L, R (0..255) | standing eye mm.
//   0.0.74 FLIGHT POSE (fix 27): + the body's leaned up x y z (the root's yawed frame; 0 1 0 upright) = 124 bytes.
//   A 112-byte pose (0.0.73 and the test buddy) is still read, upright.
//   0.0.73 MPSUIT: then (after the lean) a 12-byte LOOK tail = 136 bytes (still version 1: an older build ignores it, a pose without it -
//   the MP Test Buddy's 112, or a 124 lean-only pose - has no look): 'L', flags (1 has a look, 2 learned from the game's own render),
//   primary, secondary, emblem (sRGB, 3 bytes each), 0. The look is the worn suit's colours as SuitColours
//   (vr_mirror.hpp) learned them from the game's own rendered suit - so PLAYER 2 as SPIDER-MAN in a game suit
//   (no mod model) is drawn in that suit's colours, not the stick figure.
//   0.0.74 MP PROPS: then, only when he holds, wears or throws something or a power draws, the 'P' PROPS tail
//   (mp_props.hpp, up to 342 bytes; still version 1 - every older build ignores it).
// A packet of another version, a wrong magic, length or checksum, or a value that is not finite or out of
// range, is refused (decode says why) - the caller hides the remote player and logs it; nothing throws.
// A newer packet of the SAME version may carry more payload bytes than this build knows: they are ignored.
//
// Nothing here reads or writes the game: every value comes from the mod's own tracked poses and state
// (CONFIRMED: the mod's own data). See docs/MP-SOLO-TEST.md for the CANDIDATE list (game-side multiplayer).
#include "spidy/math.hpp"
#include "spidy/mp_props.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <string>
#include <vector>

namespace spidy::mp {
inline constexpr uint32_t magic = 0x504d5648u; // bytes 'H' 'V' 'M' 'P'
inline constexpr uint8_t version = 1;
inline constexpr size_t headerBytes = 20, crcBytes = 4, overheadBytes = headerBytes + crcBytes;
inline constexpr size_t poseBytes = 124, poseBytesV1 = 112, lookBytes = 12, poseSentBytes = poseBytes + lookBytes, helloBytes = 20, pingBytes = 4, pongBytes = 8;
inline constexpr size_t maxPacket = 512;
inline constexpr uint16_t defaultPort = 39470; // the game's UDP port (the buddy connects to it)
inline constexpr uint8_t noSuit = 255;

enum Type : uint8_t { pose = 1, hello = 2, welcome = 3, ping = 4, pong = 5, bye = 6, typeEnd };
enum Role : uint8_t { roleGame = 0, roleEcho = 1, roleBuddy = 2, roleEnd };
enum Action : uint8_t { flying = 1, swinging = 2, grapple = 4, firing = 8, handLeft = 16, handRight = 32 };
inline constexpr uint8_t actionMask = 63;

struct PoseState {
    uint8_t hero{};          // hero_modes::Hero
    uint8_t suit = noSuit;   // mesh_model::suitFolders index, noSuit: the hero's own look
    uint8_t actions{};       // Action bits
    uint32_t suitHash{};     // fnv1a(suitFolders[suit]) - a suit list of another build still finds its suit
    Vec3 root{};             // the feet (sender's world; informational in the solo test)
    float yaw{};             // the body's facing about +y (radians)
    Pose head{};             // from the feet, in the yawed root frame
    Pose hands[2]{};         // the grips, same frame
    float curl[2]{};         // 0 open .. 1 a fist
    float standingEye = 1.65f;
    Vec3 leanUp{0, 1, 0};    // 0.0.74 FLIGHT POSE: the body's up, leaned in flight (yawed root frame, unit)
    // 0.0.73 MPSUIT: the LOOK tail - the worn suit's colours (packed sRGB 0xRRGGBB); none: hasLook false
    bool hasLook{};
    bool lookLearned{};      // learned from the game's own rendered suit (else the classic default)
    uint32_t lookPrimary{}, lookSecondary{}, lookEmblem{};
    // 0.0.74 MP PROPS: the 'P' tail after LOOK (mp_props.hpp) - only sent when it has something in it.
    Props props{};
};
struct Packet {
    uint8_t type = pose;
    uint8_t role = roleGame;
    uint32_t seq{};
    uint32_t sentMs{};
    uint16_t echoHoldMs{};   // the loopback echo's hold (so the ping it shows is the network's alone)
    PoseState state{};       // pose
    char name[17]{};         // hello / welcome (16 printable characters, upper case)
    uint8_t protocol = version; // hello / welcome: the sender's version
    uint32_t token{};        // ping / pong
    uint32_t pingSentMs{};   // pong: the ping's sentMs
};
enum class Error : uint8_t { none, tooShort, tooLong, badMagic, badVersion, badType, badLength, badChecksum, badValue };
const char* errorName(Error e);

uint32_t crc32(const uint8_t* data, size_t size);
uint32_t fnv1a(const char* text);
// Encodes `p` into `out` (at least maxPacket bytes); the bytes written, 0 if `p` cannot be sent (bad type).
size_t encode(const Packet& p, uint8_t* out, size_t capacity);
std::vector<uint8_t> encode(const Packet& p);
// Decodes `size` bytes into `out`; anything wrong: the reason (out is then unspecified).
Error decode(const uint8_t* data, size_t size, Packet& out);

// 0.0.74 FLIGHT POSE: a received lean up made safe - not finite, too short or tipped past horizontal-and-a-bit
// (more than 100 degrees off +y): upright; else unit.
Vec3 safeLeanUp(Vec3 up);

// Sequence numbers wrap: a is newer than b when it is ahead by less than half the range.
inline bool newer(uint32_t a, uint32_t b) { return static_cast<int32_t>(a - b) > 0; }

// ---- the receive side ----
// The remote's poses kept by the sender's clock and drawn `delay` seconds behind the newest (100 ms: two
// to three packets at 30 Hz, so one late or lost packet never stalls). Late (older than the newest kept) and
// repeated packets are dropped; a sequence far behind (the sender restarted) starts afresh. With no packet
// for `holdAfter` the last pose is held, fading out until `hideAfter`, then hidden.
struct Sampled {
    bool visible{};
    float alpha{};           // 1 fresh .. 0 gone
    float age{};             // seconds since the newest packet arrived
    PoseState state{};
};
class Interpolator {
  public:
    float delay = .1f, holdAfter = .25f, hideAfter = 2.25f;
    static constexpr size_t keep = 32;
    // A packet's state arrived at local time `now` (seconds). False: dropped (late, repeated).
    bool push(uint32_t seq, uint32_t senderMs, double now, const PoseState& s);
    Sampled sample(double now) const;
    void reset();
    uint32_t accepted() const { return accepted_; }
    uint32_t dropped() const { return dropped_; }
    bool empty() const { return buf_.empty(); }
    double lastArrival() const { return lastArrival_; }

  private:
    struct Entry {
        double senderT{};     // sender seconds (unwrapped)
        PoseState s{};
    };
    std::deque<Entry> buf_;
    bool have_{};
    uint32_t lastSeq_{}, lastMs_{};
    double unwrapped_{};     // the newest sender time, seconds
    double offset_{};        // local - sender (the smallest seen: the least delayed packet)
    double lastArrival_{};
    uint32_t accepted_{}, dropped_{};
};
// Linear between two states (positions, curls, yaw the short way; orientations normalised-lerp);
// the discrete fields from b (the newer).
PoseState blend(const PoseState& a, const PoseState& b, float t);

// ---- the loopback echo ----
// Holds each datagram `hold` seconds, then gives it back (in order) - the "round trip" the player sees.
class DelayLine {
  public:
    double hold = .4;
    static constexpr size_t most = 256; // about 8 s at 30 Hz: more is dropped (never grows without bound)
    bool push(double now, const uint8_t* data, size_t size);
    // The next datagram due by `now` (its bytes and how long it was held), false if none is due.
    bool pop(double now, std::vector<uint8_t>& out, double& heldFor);
    size_t size() const { return q_.size(); }

  private:
    struct Item {
        double at{};
        std::vector<uint8_t> bytes;
    };
    std::deque<Item> q_;
};
// The echo's reply: the same pose with role echo and the hold stamped in (a new checksum). False: not a
// pose packet of this version (the echo never answers anything else).
bool echoReply(const uint8_t* data, size_t size, uint16_t heldMs, std::vector<uint8_t>& out);

// ---- the partner: where the remote player stands for the player ----
// Everything in the player's tracking frame (metres from his feet, +y up). The ghost stands at `feet`,
// its body facing `yaw`. mirror (the loopback): the player's own pose reflected left / right, as a partner
// facing him (his right hand comes up on his right). useRoot (a buddy): the sender's root moves the ghost
// about `feet` (a walked circle), its yaw added.
struct Ghost {
    Vec3 feet{}, eye{}, eyeForward{0, 0, -1}, eyeUp{0, 1, 0};
    bool hand[2]{};
    Vec3 grip[2]{}, gripForward[2]{{0, 0, -1}, {0, 0, -1}}, gripX[2]{{1, 0, 0}, {1, 0, 0}}, gripUp[2]{{0, 1, 0}, {0, 1, 0}};
    float curl[2]{};
    float standingEye = 1.65f;
    float bodyYaw{};
    Vec3 up{0, 1, 0}; // 0.0.74 FLIGHT POSE: the body's leaned up (world, placed like the eye; 0 1 0 upright)
};
Ghost partner(const PoseState& s, Vec3 feet, float yaw, bool mirror, bool useRoot);
inline Vec3 forwardOf(float yaw) { return Quat::yaw(yaw).rotate(Vec3{0, 0, -1}); }
// The yaw of a direction's level part (0 when it has none).
float yawOf(Vec3 forward);
float wrapAngle(float a); // into (-pi, pi]
// The spot the ghost stands on: `distance` ahead of the player along a lazily followed facing (it only
// turns once the head has turned more than `slack` away, then eases round), facing him.
struct AnchorFollow {
    float distance = 3.f, slack = .785f, ease = 2.5f; // metres, radians, 1/s
    bool placed{};
    float yaw{};
    // `headYaw` the player's facing now; `dt` seconds since the last call. Returns the facing used.
    float update(float headYaw, float dt);
    Vec3 spot(Vec3 playerFeet) const { return playerFeet + forwardOf(yaw) * distance; }
    float ghostYaw() const { return wrapAngle(yaw + 3.14159265f); }

  private:
    bool turning_{};
};

// ---- local pose -> packet state (game_xr fills these from the frame's tracked poses) ----
// eye / grips are in the tracking frame from the feet (+y up); yaw from the eye's level facing.
struct LocalPose {
    Pose eye{};
    bool hand[2]{};
    Pose grip[2]{};
    float curl[2]{};
    float standingEye = 1.65f;
    Vec3 worldFeet{};
    Vec3 leanUp{0, 1, 0}; // 0.0.74 FLIGHT POSE: the body's up in flight, the tracking frame (0 1 0 upright)
};
PoseState fromLocal(const LocalPose& l, uint8_t hero, uint8_t suit, uint32_t suitHash, uint8_t actions);

// ---- the buddy's handshake (the game's side, pure) ----
// HELLO from a new address: WELCOME back and that address is the buddy; its POSE / PING / BYE then count.
// A buddy silent for `timeout` seconds is dropped (the loopback comes back).
struct Peer {
    uint32_t addr{};
    uint16_t port{};
    bool operator==(const Peer&) const = default;
};
// 0.0.74 MP JOIN (game to game): another game's HELLO (role game) is let in like the buddy's, and a game that
// CALLS a friend (dial(): HELLO to his address until he answers) is joined by his WELCOME - from that address,
// any port (a home router may move it). Either way the peer's POSE counts when it carries the peer's own role.
// A HELLO from ourselves (the same address and port) is never let in.
class BuddyLink {
  public:
    double timeout = 3.0;
    enum class Step : uint8_t { ignore, welcome, joined, pose, ping, pong, bye };
    // What to do with a decoded packet from `from` at `now`.
    Step onPacket(const Packet& p, Peer from, double now);
    bool connected(double now) const { return connected_ && now - lastHeard_ < timeout; }
    bool connectedFlag() const { return connected_; }
    void drop() { connected_ = false; }
    Peer peer() const { return peer_; }
    const std::string& name() const { return name_; }
    uint32_t hellos() const { return hellos_; }
    // The connected peer is another game (else the Test Buddy).
    bool peerIsGame() const { return peerRole_ == roleGame; }
    // 0.0.74 MP JOIN: the friend this game calls (addr 0: nobody) and our own socket (never let in).
    void dial(Peer friendAddr) { dial_ = friendAddr; }
    Peer dialing() const { return dial_; }
    // 0.0.74 MP INTERNET: a host's punch list (the joiners the internet lobby named): their WELCOME is let in too.
    void punch(std::vector<uint32_t> addrs) { punch_ = std::move(addrs); }
    void self(Peer own) { self_ = own; }
    // 0.0.74 MP LOBBY: a host keeps the friend it has - another game's HELLO is ignored while he is still here
    // (a 2-player game; the beacon says FULL). Off: a new HELLO takes over (the Test Buddy's old behaviour).
    bool keepPeer = false;

  private:
    void take(Peer from, uint8_t role, const char* name, double now);
    bool connected_{};
    Peer peer_{}, dial_{}, self_{};
    std::vector<uint32_t> punch_;
    uint8_t peerRole_ = roleBuddy;
    double lastHeard_{};
    std::string name_;
    uint32_t hellos_{};
};
// 0.0.74 MP JOIN: %APPDATA%\Spidy\MP-PEER.txt - the friend to call. Lines: his address ("1.2.3.4", "1.2.3.4:39471"
// or a host name, the port 39470 when not given) and optionally "name=PETER" (the name he sees over you). '#' starts
// a comment; blank lines and unknown keys are skipped. ok false: no address in it.
struct PeerSetting {
    bool ok{};
    std::string host;          // as written (resolved by mp_socket's resolvePeer)
    uint16_t port = defaultPort;
    std::string name;          // cleanName'd; empty: "PLAYER"
};
PeerSetting parsePeerSetting(const std::string& text);
// 0.0.74 MP JOIN: where a remote GAME's ghost faces. Its body turns as the friend turns (his yaw against the
// yaw he had when first seen), starting out facing the player from the anchor spot.
struct PeerFacing {
    bool set{};
    float ref{};
    float bodyYaw(float ghostYaw, float senderYaw);
    void reset() { set = false; }
};
std::string cleanName(const char* raw); // printable, upper case, at most 16
} // namespace spidy::mp
