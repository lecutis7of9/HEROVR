// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
// Made by Agent Mulder. Version 0.0.73 MP SOLO TEST: the network thread (mp_session.hpp).
#include "spidy/mp_session.hpp"

#include "spidy/mp_socket.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <exception>

namespace spidy::mp {
namespace {
double clockSeconds() {
    static const auto origin = std::chrono::steady_clock::now();
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - origin).count();
}
uint32_t msOf(double t) { return static_cast<uint32_t>(static_cast<uint64_t>(t * 1000.0) & 0xffffffffu); }
float smooth(float old, float v) { return old < 0 ? v : old + (v - old) * .2f; }
} // namespace

double Session::now() const { return clockSeconds(); }

bool Session::start(const SessionConfig& config) {
    if (thread_.joinable())
        return false;
    {
        std::lock_guard<std::mutex> g(lock_);
        config_ = config;
        loop_ = {}, buddy_ = {};
        loop_.delay = buddy_.delay = config.interpDelay;
        link_ = {};
        link_.timeout = config.buddyTimeout;
        running_ = buddyOn_ = false;
        loopPing_ = buddyPing_ = -1;
        sent_ = received_ = rejected_ = 0;
        status_ = "starting";
        start_ = clockSeconds();
    }
    quit_ = false;
    try {
        thread_ = std::thread([this] { run(); });
    } catch (...) {
        log("MP: could not start the network thread - remote player hidden");
        return false;
    }
    return true;
}
void Session::stop() {
    quit_ = true;
    if (thread_.joinable())
        thread_.join();
    std::lock_guard<std::mutex> g(lock_);
    running_ = buddyOn_ = false;
    status_ = "off";
}
void Session::publish(const PoseState& local) {
    std::lock_guard<std::mutex> g(lock_);
    local_ = local;
    haveLocal_ = true;
}
void Session::clearLocal() {
    std::lock_guard<std::mutex> g(lock_);
    haveLocal_ = false;
}
void Session::log(std::string line) {
    std::lock_guard<std::mutex> g(lock_);
    if (log_.size() >= 64)
        log_.pop_front();
    log_.push_back(std::move(line));
}
std::vector<std::string> Session::takeLog() {
    std::lock_guard<std::mutex> g(lock_);
    std::vector<std::string> out(log_.begin(), log_.end());
    log_.clear();
    return out;
}
SessionView Session::view() const {
    const double t = clockSeconds();
    std::lock_guard<std::mutex> g(lock_);
    SessionView v;
    v.running = running_;
    v.buddy = buddyOn_;
    v.buddyName = buddyOn_ ? link_.name() : std::string{};
    v.port = port_;
    v.sent = sent_, v.received = received_, v.rejected = rejected_;
    v.dropped = loop_.dropped() + buddy_.dropped();
    v.status = status_;
    if (!running_)
        return v;
    const Sampled s = buddyOn_ ? buddy_.sample(t) : (config_.echo ? loop_.sample(t) : Sampled{});
    v.visible = s.visible;
    v.alpha = s.alpha;
    v.state = s.state;
    const float p = buddyOn_ ? buddyPing_ : loopPing_;
    v.pingMs = p < 0 ? -1 : static_cast<int>(std::lround(p));
    return v;
}

void Session::run() {
    UdpSocket game, echo;
    const SessionConfig cfg = config_;
    uint16_t port = 0;
    for (int k = 0; k < 5 && !port; ++k)
        if (game.open(cfg.lan ? anyAddr : loopbackAddr, static_cast<uint16_t>(cfg.port + k)))
            port = game.localPort();
    if (!port) {
        log("MP: no UDP port (" + game.lastError() + ") - remote player hidden, the game goes on");
        std::lock_guard<std::mutex> g(lock_);
        status_ = "network off: no UDP port";
        return;
    }
    bool echoOk = cfg.echo && echo.open(loopbackAddr, 0);
    if (cfg.echo && !echoOk)
        log("MP: loopback echo could not open (" + echo.lastError() + ") - no ghost until a buddy connects");
    const Peer gamePeer{loopbackAddr, port};
    const Peer echoPeer{loopbackAddr, echoOk ? echo.localPort() : uint16_t{0}};
    {
        std::lock_guard<std::mutex> g(lock_);
        running_ = true;
        port_ = port;
        status_ = echoOk ? "loopback" : "waiting for a buddy";
    }
    char line[160];
    std::snprintf(line, sizeof line, "MP: UDP %s:%u up%s, protocol %u; loopback echo %s (hold %.0f ms)",
                  cfg.lan ? "0.0.0.0" : "127.0.0.1", port, cfg.lan ? " (LAN)" : "", version,
                  echoOk ? "on" : "off", cfg.echoHold * 1000);
    log(line);

    DelayLine delay;
    delay.hold = cfg.echoHold;
    uint32_t seq = 0, pingToken = 0, rejectedLogged = 0;
    double nextSend = 0, nextPing = 0;
    std::vector<uint8_t> buf(maxPacket + 64), out;
    bool failed = false;
    while (!quit_) {
        try {
            const double t = clockSeconds();
            const uint32_t ms = msOf(t);
            // The echo: hold what the game sent, then hand it back as an echo packet.
            if (echoOk) {
                for (int k = 0; k < 64; ++k) {
                    Peer from;
                    const int n = echo.receive(buf.data(), buf.size(), from);
                    if (n <= 0)
                        break;
                    if (from == gamePeer)
                        delay.push(t, buf.data(), static_cast<size_t>(n));
                }
                double held;
                while (delay.pop(t, out, held)) {
                    std::vector<uint8_t> reply;
                    const double h = std::min(held * 1000.0, 65535.0);
                    if (echoReply(out.data(), out.size(), static_cast<uint16_t>(h), reply))
                        echo.send(reply.data(), reply.size(), gamePeer);
                }
            }
            // The game's socket.
            for (int k = 0; k < 128; ++k) {
                Peer from;
                const int n = game.receive(buf.data(), buf.size(), from);
                if (n < 0) {
                    if (!failed)
                        log("MP: receive error (" + game.lastError() + ") - remote player hidden until it clears");
                    failed = true;
                    break;
                }
                if (n == 0)
                    break;
                failed = false;
                Packet p;
                const Error e = decode(buf.data(), static_cast<size_t>(n), p);
                std::lock_guard<std::mutex> g(lock_);
                if (e != Error::none) {
                    ++rejected_;
                    if (rejectedLogged < 8 || rejected_ % 500 == 0) {
                        ++rejectedLogged;
                        std::snprintf(line, sizeof line, "MP: refused a %d-byte packet from %s: %s (%u refused)", n,
                                      addrText(from).c_str(), errorName(e), rejected_);
                        if (log_.size() >= 64)
                            log_.pop_front();
                        log_.push_back(line);
                    }
                    continue;
                }
                ++received_;
                if (echoOk && from == echoPeer && p.role == roleEcho && p.type == pose) {
                    if (!buddyOn_ && loop_.push(p.seq, p.sentMs, t, p.state)) {
                        const float rtt = static_cast<float>(static_cast<int32_t>(ms - p.sentMs) - p.echoHoldMs);
                        loopPing_ = smooth(loopPing_, std::max(0.f, rtt));
                    }
                    continue;
                }
                const Peer before = link_.peer();
                const auto step = link_.onPacket(p, from, t);
                Packet reply;
                reply.role = roleGame;
                reply.sentMs = ms;
                switch (step) {
                case BuddyLink::Step::welcome: {
                    reply.type = welcome;
                    std::snprintf(reply.name, sizeof reply.name, "HERO VR");
                    const auto bytes = encode(reply);
                    game.send(bytes.data(), bytes.size(), from);
                    if (buddyOn_ && !(before == from)) { // another buddy took over: its own timeline
                        buddy_ = {};
                        buddy_.delay = cfg.interpDelay;
                        buddyPing_ = -1;
                    }
                    if (!buddyOn_) {
                        buddyOn_ = true;
                        buddy_ = {};
                        buddy_.delay = cfg.interpDelay;
                        buddyPing_ = -1;
                        status_ = "buddy " + link_.name();
                        if (log_.size() >= 64)
                            log_.pop_front();
                        log_.push_back("MP: Test Buddy '" + link_.name() + "' connected from " + addrText(from) +
                                       " - it replaces the loopback ghost");
                        nextPing = t;
                    }
                    break;
                }
                case BuddyLink::Step::pose: buddy_.push(p.seq, p.sentMs, t, p.state); break;
                case BuddyLink::Step::ping: {
                    reply.type = pong;
                    reply.token = p.token;
                    reply.pingSentMs = p.sentMs;
                    const auto bytes = encode(reply);
                    game.send(bytes.data(), bytes.size(), from);
                    break;
                }
                case BuddyLink::Step::pong:
                    buddyPing_ = smooth(buddyPing_, std::max(0.f, static_cast<float>(static_cast<int32_t>(ms - p.pingSentMs))));
                    break;
                case BuddyLink::Step::bye:
                    buddyOn_ = false;
                    buddy_ = {};
                    loop_.reset();
                    status_ = echoOk ? "loopback" : "waiting for a buddy";
                    if (log_.size() >= 64)
                        log_.pop_front();
                    log_.push_back("MP: Test Buddy said bye - back to the loopback ghost");
                    break;
                default: break;
                }
            }
            // The buddy gone quiet.
            {
                std::lock_guard<std::mutex> g(lock_);
                if (buddyOn_ && !link_.connected(t)) {
                    link_.drop();
                    buddyOn_ = false;
                    buddy_ = {};
                    loop_.reset();
                    status_ = echoOk ? "loopback" : "waiting for a buddy";
                    if (log_.size() >= 64)
                        log_.pop_front();
                    log_.push_back("MP: Test Buddy silent for 3 s - dropped, back to the loopback ghost");
                }
            }
            // Send.
            if (t >= nextSend) {
                nextSend = std::max(nextSend + 1.0 / cfg.sendHz, t - .1);
                Packet p;
                Peer to{};
                bool go = false;
                {
                    std::lock_guard<std::mutex> g(lock_);
                    if (haveLocal_) {
                        p.state = local_;
                        if (buddyOn_)
                            to = link_.peer(), go = true;
                        else if (echoOk)
                            to = echoPeer, go = true;
                    }
                }
                if (go) {
                    p.type = pose;
                    p.role = roleGame;
                    p.seq = ++seq;
                    p.sentMs = ms;
                    const auto bytes = encode(p);
                    if (!bytes.empty() && game.send(bytes.data(), bytes.size(), to)) {
                        std::lock_guard<std::mutex> g(lock_);
                        ++sent_;
                    }
                }
            }
            bool buddyNow;
            Peer buddyPeer;
            {
                std::lock_guard<std::mutex> g(lock_);
                buddyNow = buddyOn_;
                buddyPeer = link_.peer();
            }
            if (buddyNow && t >= nextPing) {
                nextPing = t + 1.0;
                Packet p;
                p.type = ping;
                p.role = roleGame;
                p.sentMs = ms;
                p.token = ++pingToken;
                const auto bytes = encode(p);
                game.send(bytes.data(), bytes.size(), buddyPeer);
            }
        } catch (const std::exception& e) {
            log(std::string("MP: network step failed (") + e.what() + ") - remote player hidden");
            std::lock_guard<std::mutex> g(lock_);
            loop_.reset(), buddy_.reset();
        } catch (...) {
            log("MP: network step failed - remote player hidden");
            std::lock_guard<std::mutex> g(lock_);
            loop_.reset(), buddy_.reset();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    // Leaving: tell a buddy.
    bool buddyNow;
    Peer buddyPeer;
    {
        std::lock_guard<std::mutex> g(lock_);
        buddyNow = buddyOn_;
        buddyPeer = link_.peer();
        running_ = false;
    }
    if (buddyNow) {
        Packet p;
        p.type = bye;
        p.role = roleGame;
        p.sentMs = msOf(clockSeconds());
        const auto bytes = encode(p);
        game.send(bytes.data(), bytes.size(), buddyPeer);
    }
    log("MP: network stopped");
}
} // namespace spidy::mp
