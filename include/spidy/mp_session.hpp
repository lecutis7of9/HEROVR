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
};
struct SessionView {
    bool running{};               // the socket is up
    bool visible{};               // draw the remote player
    float alpha{};
    PoseState state{};
    bool buddy{};                 // the remote player is the Test Buddy (else the loopback echo)
    std::string buddyName;
    int pingMs = -1;              // -1: not measured yet
    uint16_t port{};
    uint32_t sent{}, received{}, rejected{}, dropped{};
    std::string status;           // one short line for the console / wrist
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
    bool running_{}, buddyOn_{};
    float loopPing_ = -1, buddyPing_ = -1;
    uint16_t port_{};
    uint32_t sent_{}, received_{}, rejected_{};
    std::string status_ = "starting";
    std::deque<std::string> log_;
    double start_{};
};
} // namespace spidy::mp
