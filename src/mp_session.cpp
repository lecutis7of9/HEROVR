// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
// Made by Agent Mulder. Version 0.0.73 MP SOLO TEST: the network thread (mp_session.hpp).
#include "spidy/mp_session.hpp"

#include "spidy/hero_modes.hpp"
#include "spidy/mp_socket.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
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
        running_ = buddyOn_ = calling_ = false;
        peerText_.clear();
        loopPing_ = buddyPing_ = -1;
        sent_ = received_ = rejected_ = 0;
        status_ = "starting";
        start_ = clockSeconds();
        punchWant_ = config.punch, relayHostWant_ = config.relayHost, relayPortWant_ = config.relayPort, tokenWant_ = config.token;
        ++targetsGen_;
        publicText_.clear();
        relayed_ = false;
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
void Session::setTargets(const std::vector<std::string>& punch, const std::string& relayHost, uint16_t relayPort,
                         const std::string& token) {
    std::lock_guard<std::mutex> g(lock_);
    if (punch == punchWant_ && relayHost == relayHostWant_ && relayPort == relayPortWant_ && token == tokenWant_)
        return;
    punchWant_ = punch, relayHostWant_ = relayHost, relayPortWant_ = relayPort, tokenWant_ = token;
    ++targetsGen_;
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
    v.peerGame = buddyOn_ && link_.peerIsGame();
    v.calling = calling_ && !buddyOn_;
    v.peerText = peerText_;
    v.port = port_;
    v.sent = sent_, v.received = received_, v.rejected = rejected_;
    v.dropped = loop_.dropped() + buddy_.dropped();
    v.status = status_;
    v.hosting = hosting_ && running_;
    v.beacons = beacons_;
    v.lanText = lanText_;
    v.publicText = publicText_;
    v.relayed = relayed_ && buddyOn_;
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
    // 0.0.74 MP JOIN: our name, and the friend to call (resolved here, on the network thread; retried).
    const std::string myName = cfg.name.empty() ? std::string("HERO VR") : cleanName(cfg.name.c_str());
    const bool wantCall = !cfg.peerHost.empty();
    Peer callPeer{};
    bool callOk = false;
    double nextResolve = 0, nextCall = 0, nextCallLog = 0;
    const std::string idle = echoOk ? "loopback" : (wantCall ? "calling" : cfg.host ? "hosting" : "waiting for a buddy");
    // 0.0.74 MP LOBBY: hosting - the beacon's targets (each LAN's broadcast) and a random id for this session.
    std::vector<uint32_t> targets = cfg.announceTo;
    std::string lan;
    if (cfg.host) {
        const auto mine = localAddresses();
        for (uint32_t a : mine)
            lan += (lan.empty() ? "" : ", ") + addrText({a, 0}).substr(0, addrText({a, 0}).rfind(':'));
        if (targets.empty())
            targets = broadcastTargets(mine);
        if (!game.allowBroadcast())
            log("MP LOBBY: broadcast refused (" + game.lastError() + ") - friends will not see you in their list");
    }
    const uint32_t hostId = static_cast<uint32_t>(std::chrono::steady_clock::now().time_since_epoch().count() * 2654435761u) ^
                            (static_cast<uint32_t>(port) << 16) ^ 0x5eedu;
    double nextAnnounce = 0;
    {
        std::lock_guard<std::mutex> g(lock_);
        running_ = true;
        port_ = port;
        link_.self(gamePeer);
        link_.keepPeer = cfg.host;
        hosting_ = cfg.host;
        lanText_ = lan;
        status_ = idle;
        peerText_ = wantCall ? cfg.peerHost + ":" + std::to_string(cfg.peerPort) : std::string{};
    }
    char line[160];
    std::snprintf(line, sizeof line, "MP: UDP %s:%u up%s, protocol %u; loopback echo %s (hold %.0f ms)",
                  cfg.lan ? "0.0.0.0" : "127.0.0.1", port, cfg.lan ? " (LAN)" : "", version,
                  echoOk ? "on" : "off", cfg.echoHold * 1000);
    log(line);

    // 0.0.74 MP INTERNET: STUN (our public address), the punch list and the relay (mp_inet.hpp).
    // The STUN names are looked up on a helper thread: a PC without internet (or a slow resolver) never holds
    // up the LAN game.
    std::vector<Peer> stunPeers;
    std::mutex stunLock;
    std::vector<Peer> stunFound;
    std::atomic<bool> stunDone{cfg.stun.empty() || !cfg.lan};
    std::thread stunResolver;
    if (!stunDone)
        stunResolver = std::thread([&] {
            std::vector<Peer> found;
            for (const auto& srv : cfg.stun) {
                const PeerSetting ps = parsePeerSetting(srv);
                Peer sp;
                std::string why;
                const uint16_t sport = srv.find(':') == std::string::npos ? stunDefaultPort : ps.port;
                if (ps.ok && resolvePeer(ps.host, sport, sp, why))
                    found.push_back(sp);
                else
                    log("MP INTERNET: STUN server " + srv + " not found" + (why.empty() ? "" : " (" + why + ")") +
                        " - no internet list for this game (the LAN still works)");
            }
            std::lock_guard<std::mutex> g(stunLock);
            stunFound = found;
            stunDone = true;
        });
    StunTxn stunTxn{};
    for (size_t i = 0; i < stunTxn.size(); ++i)
        stunTxn[i] = static_cast<uint8_t>((hostId >> ((i % 4) * 8)) ^ (i * 37u) ^ static_cast<uint32_t>(clockSeconds() * 1e6));
    size_t stunNext = 0;
    double nextStun = 0;
    bool stunGot = false;
    Peer publicPeer{};
    uint32_t targetsSeen = ~0u;
    std::vector<Peer> punchPeers;
    Peer relayPeer{};
    bool relayOk = false, onRelay = false;
    RelayToken token{};
    double nextRelayKeep = 0, nextPunch = 0;
    const double callStart = clockSeconds();
    // Every send goes through here: to the relay, the packet gets its relay header.
    auto sendTo = [&](const std::vector<uint8_t>& bytes, Peer to) {
        if (relayOk && to == relayPeer) {
            const auto framed = relayWrap(token, bytes.data(), bytes.size());
            return game.send(framed.data(), framed.size(), to);
        }
        return game.send(bytes.data(), bytes.size(), to);
    };

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
            // 0.0.74 MP INTERNET: new punch / relay / token from MP-LOBBY.txt.
            {
                std::vector<std::string> want;
                std::string rHost, tok;
                uint16_t rPort = 0;
                bool changed = false;
                {
                    std::lock_guard<std::mutex> g(lock_);
                    if (targetsGen_ != targetsSeen) {
                        targetsSeen = targetsGen_, changed = true;
                        want = punchWant_, rHost = relayHostWant_, rPort = relayPortWant_, tok = tokenWant_;
                    }
                }
                if (changed) {
                    punchPeers.clear();
                    for (const auto& w : want) {
                        const size_t colon = w.rfind(':');
                        uint32_t a = 0;
                        if (colon != std::string::npos && parseAddr(w.substr(0, colon), a))
                            punchPeers.push_back({a, static_cast<uint16_t>(std::strtoul(w.c_str() + colon + 1, nullptr, 10))});
                    }
                    {
                        std::vector<uint32_t> addrs;
                        for (const Peer& pp : punchPeers)
                            addrs.push_back(pp.addr);
                        std::lock_guard<std::mutex> g(lock_);
                        link_.punch(addrs);
                    }
                    RelayToken nt{};
                    const bool tokOk = parseToken(tok, nt) && tokenSet(nt);
                    Peer rp{};
                    std::string why;
                    const bool rOk = cfg.lan && tokOk && !rHost.empty() && resolvePeer(rHost, rPort, rp, why);
                    if (!rHost.empty() && !rOk)
                        log("MP INTERNET: relay " + rHost + " not usable" + (why.empty() ? " (no token)" : " (" + why + ")"));
                    if (rOk && (!relayOk || !(rp == relayPeer) || nt != token))
                        log("MP INTERNET: relay " + addrText(rp) + " ready (used only if the direct way fails)");
                    relayOk = rOk, relayPeer = rp, token = nt;
                    nextRelayKeep = t, nextPunch = t;
                    if (!punchPeers.empty())
                        log("MP INTERNET: opening the way to " + std::to_string(punchPeers.size()) + " joining friend(s)");
                }
            }
            if (stunPeers.empty() && stunDone && stunResolver.joinable()) {
                stunResolver.join();
                stunPeers = stunFound;
            }
            if (!stunPeers.empty() && t >= nextStun) {
                nextStun = t + (stunGot ? cfg.stunEvery : 2.0);
                const auto req = stunRequest(stunTxn);
                game.send(req.data(), req.size(), stunPeers[stunNext++ % stunPeers.size()]);
            }
            if (relayOk && t >= nextRelayKeep) { // the relay learns (and keeps) our address; also our router's door
                nextRelayKeep = t + cfg.relayKeepEvery;
                const auto keep = relayWrap(token, nullptr, 0);
                game.send(keep.data(), keep.size(), relayPeer);
            }
            // 0.0.74 MP JOIN: find the friend (a host name may need the resolver; retried every 10 s).
            if (wantCall && !callOk && t >= nextResolve) {
                nextResolve = t + 10;
                std::string why;
                callOk = resolvePeer(cfg.peerHost, cfg.peerPort, callPeer, why);
                std::lock_guard<std::mutex> g(lock_);
                if (callOk) {
                    link_.dial(callPeer);
                    peerText_ = addrText(callPeer);
                    calling_ = true;
                } else if (log_.size() < 64) {
                    log_.push_back("MP: the friend in MP-PEER.txt: " + why + " - trying again in 10 s");
                }
            }
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
                const uint8_t* data = buf.data();
                size_t size = static_cast<size_t>(n);
                if (isStun(data, size)) { // 0.0.74 MP INTERNET: our public address
                    Peer mapped;
                    if (stunMapped(data, size, stunTxn, mapped)) {
                        const bool news = !stunGot || !(mapped == publicPeer);
                        stunGot = true, publicPeer = mapped;
                        nextStun = t + cfg.stunEvery;
                        std::lock_guard<std::mutex> g(lock_);
                        publicText_ = addrText(mapped);
                        if (news && log_.size() < 64)
                            log_.push_back("MP INTERNET: the internet sees this game at " + publicText_);
                    }
                    continue;
                }
                if (relayOk && from == relayPeer) {
                    if (!relayFrame(data, size, token))
                        continue;
                    data += relayHeaderBytes, size -= relayHeaderBytes;
                    if (size == 0)
                        continue; // the relay's keepalive answer
                }
                Packet p;
                const Error e = decode(data, size, p);
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
                case BuddyLink::Step::welcome:
                case BuddyLink::Step::joined: {
                    if (step == BuddyLink::Step::welcome) {
                        reply.type = welcome;
                        std::snprintf(reply.name, sizeof reply.name, "%s", myName.c_str());
                        const auto bytes = encode(reply);
                        sendTo(bytes, from);
                    }
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
                        const bool friendGame = link_.peerIsGame();
                        status_ = (friendGame ? "player " : "buddy ") + link_.name();
                        if (log_.size() >= 64)
                            log_.pop_front();
                        if (friendGame)
                            log_.push_back("MP: player '" + link_.name() + "' joined from " + addrText(from) +
                                           (step == BuddyLink::Step::joined ? " (answered our call)" : " (called us)") +
                                           " - PLAYER 2 is your friend now");
                        else
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
                    sendTo(bytes, from);
                    break;
                }
                case BuddyLink::Step::pong:
                    buddyPing_ = smooth(buddyPing_, std::max(0.f, static_cast<float>(static_cast<int32_t>(ms - p.pingSentMs))));
                    break;
                case BuddyLink::Step::bye:
                    buddyOn_ = false;
                    buddy_ = {};
                    loop_.reset();
                    status_ = idle;
                    nextCall = t + 2; // a friend who left: called again after a moment (he may come back)
                    if (log_.size() >= 64)
                        log_.pop_front();
                    log_.push_back(link_.peerIsGame() ? "MP: player '" + link_.name() + "' left" +
                                                            (callOk ? " - calling again" : "")
                                                      : std::string("MP: Test Buddy said bye - back to the loopback ghost"));
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
                    status_ = idle;
                    if (log_.size() >= 64)
                        log_.pop_front();
                    log_.push_back(link_.peerIsGame()
                                       ? "MP: player '" + link_.name() + "' silent for 3 s - dropped" + (callOk ? ", calling again" : "")
                                       : std::string("MP: Test Buddy silent for 3 s - dropped, back to the loopback ghost"));
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
                    if (!bytes.empty() && sendTo(bytes, to)) {
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
                calling_ = callOk && !buddyOn_;
            }
            // 0.0.74 MP LOBBY: the host's beacon, once a second, to every LAN's broadcast (and announceTo).
            if (cfg.host && t >= nextAnnounce) {
                nextAnnounce = t + cfg.announceEvery;
                Beacon b;
                b.protocol = version;
                b.gamePort = port;
                b.hostId = hostId;
                b.name = myName;
                {
                    std::lock_guard<std::mutex> g(lock_);
                    b.players = static_cast<uint8_t>(1 + (buddyOn_ && link_.peerIsGame() ? 1 : 0));
                    if (haveLocal_ && local_.hero < hero_modes::heroes)
                        b.hero = local_.hero, b.heroName = hero_modes::heroNames[local_.hero];
                }
                const auto bytes = encodeBeacon(b);
                uint32_t ok = 0;
                for (uint32_t a : targets)
                    ok += game.send(bytes.data(), bytes.size(), {a, cfg.lobbyPort}) ? 1u : 0u;
                std::lock_guard<std::mutex> g(lock_);
                beacons_ += ok ? 1 : 0;
            }
            // 0.0.74 MP JOIN: calling the friend - a HELLO a second until he answers (both calling at once
            // also opens most home routers' UDP doors for each other).
            if (callOk && !buddyNow && t >= nextCall) {
                nextCall = t + cfg.callEvery;
                Packet h;
                h.type = hello;
                h.role = roleGame;
                h.seq = ++seq;
                h.sentMs = ms;
                std::snprintf(h.name, sizeof h.name, "%s", myName.c_str());
                const auto bytes = encode(h);
                sendTo(bytes, callPeer);
                if (t >= nextCallLog) {
                    const bool first = nextCallLog == 0;
                    nextCallLog = t + 15;
                    log(std::string(first ? "MP: calling " : "MP: still calling ") + addrText(callPeer) + " as '" + myName +
                        "'" + (first ? "" : " - no answer yet: is his game on with MP-PEER.txt, UDP " +
                                                std::to_string(callPeer.port) + " open to you?"));
                }
            }
            if (buddyNow && t >= nextPing) {
                nextPing = t + 1.0;
                Packet p;
                p.type = ping;
                p.role = roleGame;
                p.sentMs = ms;
                p.token = ++pingToken;
                const auto bytes = encode(p);
                sendTo(bytes, buddyPeer);
            }
            // 0.0.74 MP INTERNET: host - a HELLO a second to each joining friend the lobby named (our router then
            // lets his packets in); joiner - no answer from the host's public address: call through the relay.
            if (!buddyNow && !punchPeers.empty() && t >= nextPunch) {
                nextPunch = t + cfg.callEvery;
                Packet h;
                h.type = hello;
                h.role = roleGame;
                h.seq = ++seq;
                h.sentMs = ms;
                std::snprintf(h.name, sizeof h.name, "%s", myName.c_str());
                const auto bytes = encode(h);
                for (const Peer& pp : punchPeers)
                    game.send(bytes.data(), bytes.size(), pp);
            }
            if (wantCall && callOk && relayOk && !onRelay && !buddyNow && t - callStart >= cfg.relayAfter) {
                onRelay = true;
                log("MP INTERNET: no direct answer from " + addrText(callPeer) + " in " +
                    std::to_string(static_cast<int>(cfg.relayAfter)) + " s - calling through the relay " + addrText(relayPeer));
                callPeer = relayPeer;
                nextCall = t;
                std::lock_guard<std::mutex> g(lock_);
                link_.dial(relayPeer);
            }
            {
                std::lock_guard<std::mutex> g(lock_);
                relayed_ = relayOk && buddyOn_ && link_.peer() == relayPeer;
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
    if (stunResolver.joinable())
        stunResolver.join();
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
        sendTo(bytes, buddyPeer);
    }
    log("MP: network stopped");
}
} // namespace spidy::mp
