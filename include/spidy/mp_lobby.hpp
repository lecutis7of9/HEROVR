#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
// Made by Agent Mulder.
//
// 0.0.74 MP LOBBY (owner pt1384u: "there has to be a way for us to have a 'start host' ... and then someone can just
// join the host on a server list"): no IP typing on a LAN. A HOSTING game announces itself once a second by UDP
// broadcast to the lobby port (39469; the game's own port stays 39470): its name, hero, players and game port. The
// launcher's Multiplayer sheet listens there and lists the hosts; JOIN writes %APPDATA%\Spidy\MP-LOBBY.txt, which
// the game reads like MP-PEER.txt (MP-PEER.txt stays as the advanced fallback; MP-LOBBY.txt wins when both exist).
//
// The beacon (52 bytes, little-endian):
//   0 magic 'HVLB'  4 lobby version (1)  5 MP protocol (mp::version)  6 game port u16  8 players  9 max players
//   10 hero (255 unknown)  11 flags (0)  12 host id u32 (random per session: one host seen through two networks
//   is listed once)  16 name[16] (printable, zero padded)  32 hero name[16]  48 CRC-32 of bytes 0..47.
// Pure: no sockets (mp_session sends it, the launcher's Lobby.cs reads the same bytes). Nothing touches the game
// (CONFIRMED: the mod's own data).
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace spidy::mp {
inline constexpr uint32_t beaconMagic = 0x424c5648u; // bytes 'H' 'V' 'L' 'B'
inline constexpr uint8_t lobbyVersion = 1;
inline constexpr uint16_t lobbyPort = 39469;
inline constexpr size_t beaconBytes = 52;
inline constexpr uint8_t noHero = 255;

struct Beacon {
    uint8_t protocol{};       // the host's mp::version (a different one: listed, but JOIN says "other version")
    uint16_t gamePort{};
    uint8_t players = 1, maxPlayers = 2;
    uint8_t hero = noHero;
    uint32_t hostId{};
    std::string name;         // cleanName'd
    std::string heroName;     // cleanName'd
};
std::vector<uint8_t> encodeBeacon(const Beacon& b);
// False: not a beacon of this lobby version (wrong size, magic, checksum or values).
bool decodeBeacon(const uint8_t* data, size_t size, Beacon& out);

// Where the beacon goes: 255.255.255.255 and each local address's /24 broadcast (Windows sends the all-ones
// broadcast out of one network card only; the /24 one reaches the others - most home LANs are /24). Loopback
// and 0.0.0.0 are skipped; no duplicates. Host-order IPv4.
std::vector<uint32_t> broadcastTargets(const std::vector<uint32_t>& localAddrs);

// The hosts heard, forgotten `forget` seconds after their last beacon.
struct HostSeen {
    uint32_t addr{};          // where the beacon came from (host order) - the address JOIN calls
    Beacon beacon{};
    double lastSeen{};
};
class HostList {
  public:
    double forget = 4.0;
    void heard(uint32_t fromAddr, const Beacon& b, double now);
    std::vector<HostSeen> list(double now) const; // by name, then address
    void clear() { hosts_.clear(); }

  private:
    std::vector<HostSeen> hosts_;
};

// %APPDATA%\Spidy\MP-LOBBY.txt - written by the launcher's HOST / JOIN (or by hand), deleted by LEAVE:
//   mode=host | mode=join      join=192.168.1.20[:39470] (join only)      name=PETER (shown to the friend)
//   host=MARY (join: the host's name, for the log)
// 0.0.74 MP INTERNET (the launcher's internet list; these change while the game runs - no network restart):
//   punch=203.0.113.7:51234 (host: a joining friend's public address to open the router to; one line each)
//   relay=relay.example.org:3479   token=0123456789abcdef (the relay both use when punching fails)
// '#' comments, blank lines and unknown keys are skipped. mode none: nothing usable (join without an address).
struct LobbySetting {
    enum Mode : uint8_t { none, host, join } mode = none;
    std::string joinHost;
    uint16_t joinPort = 39470;
    std::string name, hostName;
    std::vector<std::string> punch;   // "a.b.c.d:port", numeric only (the lobby's), at most 8
    std::string relayHost;
    uint16_t relayPort = 3479;
    std::string token;                // 16 hex digits, else empty
};
// The lines that need a network restart when they change (mode, join, name, host) - not punch / relay / token.
std::string lobbyRestartKey(const LobbySetting& s);
LobbySetting parseLobbySetting(const std::string& text);
std::string lobbyText(const LobbySetting& s); // what the game writes (round-trips through parseLobbySetting)
} // namespace spidy::mp
