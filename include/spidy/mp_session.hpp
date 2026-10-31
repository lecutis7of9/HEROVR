#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
// Made by Agent Mulder.
//
// Version 0.0.73 MP SOLO TEST: the network thread. The game publishes its pose every frame; at 30 Hz the
// thread sends it - to its own loopback echo on 127.0.0.1 (held ~0.4 s, sent back marked as an echo, so the
// player sees himself as a REMOTE player through the real packets), or, while the HERO VR MP Test Buddy is
// connected (HELLO / WELCOME on UDP 39470), to the buddy instead; then the buddy's puppet is the remote
// player. view() is the remote player as the render thread should draw it (interpolated 0.1 s behind).
// Any socket error or bad packet: the remote player is hidden and a line is logged - never a throw, never
// a stall of the caller (publish / view only take a short lock).
#include "spidy/mp_inet.hpp"
#include "spidy/mp_lobby.hpp"
#include "spidy/mp_net.hpp"

#include <atomic>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace spidy::mp {
struct SessionConfig {
    uint16_t port = defaultPort;  // the game's port (tries the next 4 if it is taken)
    bool lan = false;             // listen on every interface (a buddy on another PC) - else 127.0.0.1 only
    bool echo = true;             // the loopback ghost
    double echoHold = .4;         // seconds (+ the 0.1 s interpolation: about half a second behind)
    float interpDelay = .1f;
    double sendHz = 30;
    double buddyTimeout = 3;
    // 0.0.74 MP JOIN: the friend's game to call (MP-PEER.txt; empty: nobody - wait for a HELLO) - HELLO every
    // second until his WELCOME (or his own HELLO) comes, again after he goes quiet - and the name he sees.
    std::string peerHost;
    uint16_t peerPort = defaultPort;
    std::string name;             // cleanName'd; empty: "HERO VR"
    double callEvery = 1.0;       // seconds between HELLOs while calling
    // 0.0.74 MP LOBBY: HOST - announce this game on the LAN (mp_lobby.hpp beacon to the lobby port, every
    // announceEvery s) and keep the first friend who joins. announceTo empty: broadcastTargets(localAddresses()).
    bool host = false;
    uint16_t lobbyPort = spidy::mp::lobbyPort;
    std::vector<uint32_t> announceTo;
    double announceEvery = 1.0;
    // 0.0.74 MP INTERNET: STUN servers ("host:port") asked from the game socket for this game's public address
    // (empty: none - LAN only); the friends to punch toward (host: the joiners the lobby named, numeric
    // "a.b.c.d:port"); the relay and its token (both players). A joiner calls the relay once the direct call
    // got no answer for relayAfter s. setTargets() changes punch / relay / token while running.
    std::vector<std::string> stun;
    double stunEvery = 25;        // after the first answer (before it: every 2 s, the next server each time)
    std::vector<std::string> punch;
    std::string relayHost;
    uint16_t relayPort = 3479;
    std::string token;            // 16 hex digits
    double relayAfter = 6;
    double relayKeepEvery = 5;
};
struct SessionView {
    bool running{};               // the socket is up
    bool visible{};               // draw the remote player
    float alpha{};
    PoseState state{};
    bool buddy{};                 // a remote is connected: the Test Buddy or another game (else the loopback echo)
    bool peerGame{};              // 0.0.74 MP JOIN: that remote is another game (a friend), not the Test Buddy
    bool calling{};               // 0.0.74 MP JOIN: calling the MP-PEER.txt friend (no answer yet)
    std::string peerText;         // the friend called ("1.2.3.4:39470"), empty: nobody
    std::string buddyName;
    int pingMs = -1;              // -1: not measured yet
    uint16_t port{};
    uint32_t sent{}, received{}, rejected{}, dropped{};
    std::string status;           // one short line for the console / wrist
    bool hosting{};               // 0.0.74 MP LOBBY: announcing on the LAN
    uint32_t beacons{};           // beacons sent
    std::string lanText;          // this PC's LAN addresses ("192.168.1.5"), for the log and the friend
    std::string publicText;       // 0.0.74 MP INTERNET: what STUN saw ("203.0.113.7:51234"), empty: not known
    bool relayed{};               // the friend is reached through the relay
};
class Session {
  public:
    Session() = default;
    ~Session() { stop(); }
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;
    // Starts the thread (false: it is already running or could not start; a socket failure is reported
    // by view().status and the log, with running false).
    bool start(const SessionConfig& config = {});
    void stop();
    bool started() const { return thread_.joinable(); }
    void publish(const PoseState& local);
    void clearLocal();            // nothing to send (e.g. a loading screen)
    // 0.0.74 MP INTERNET: new punch list / relay / token (MP-LOBBY.txt changed) - no restart, the socket and
    // its public port stay.
    void setTargets(const std::vector<std::string>& punch, const std::string& relayHost, uint16_t relayPort,
                    const std::string& token);
    SessionView view() const;
    std::vector<std::string> takeLog();
    double now() const;           // the session clock (seconds)

  private:
    void run();
    void log(std::string line);
    SessionConfig config_{};
    std::thread thread_;
    std::atomic<bool> quit_{false};
    mutable std::mutex lock_;
    // under lock_:
    bool haveLocal_{};
    PoseState local_{};
    Interpolator loop_, buddy_;
    BuddyLink link_;
    bool running_{}, buddyOn_{}, calling_{}, hosting_{};
    uint32_t beacons_{};
    std::string lanText_;
    std::vector<std::string> punchWant_;
    std::string relayHostWant_, tokenWant_;
    uint16_t relayPortWant_{};
    uint32_t targetsGen_{};
    std::string publicText_;
    bool relayed_{};
    std::string peerText_;
    float loopPing_ = -1, buddyPing_ = -1;
    uint16_t port_{};
    uint32_t sent_{}, received_{}, rejected_{};
    std::string status_ = "starting";
    std::deque<std::string> log_;
    double start_{};
};
} // namespace spidy::mp
