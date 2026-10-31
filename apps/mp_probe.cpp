// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
// Made by Agent Mulder. Version 0.0.73 MP SOLO TEST: the game's network side without the game.
//   spidy_mp_probe run <port> <seconds> [peer host[:port]] [name]
//                                         the real Session, publishing a fake player; prints one JSON line
//                                         (0.0.74 MP JOIN: with a peer it calls that game, like MP-PEER.txt)
//                                         a second (tests/mp_buddy_tests.py drives the Test Buddy at it)
//   spidy_mp_probe golden                 hex of fixed packets (the Python buddy must encode the same bytes)
#include "spidy/mp_lobby.hpp"
#include "spidy/mp_net.hpp"
#include "spidy/mp_prop_look.hpp"
#include "spidy/mp_props.hpp"
#include "spidy/mp_session.hpp"
#include "spidy/mp_socket.hpp"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>

using namespace spidy;
using namespace spidy::mp;

namespace {
void hex(const char* label, const std::vector<uint8_t>& b) {
    std::printf("%s ", label);
    for (uint8_t x : b)
        std::printf("%02x", x);
    std::printf("\n");
}
Packet goldenPose() {
    Packet p;
    p.type = pose;
    p.role = roleBuddy;
    p.seq = 42;
    p.sentMs = 1000;
    p.state.hero = 0;
    p.state.suit = 3;
    p.state.suitHash = fnv1a("advanced_suit_2");
    p.state.actions = swinging | handRight;
    p.state.root = {.5f, 0, -.25f};
    p.state.yaw = .5f;
    p.state.head = {{0, 1.6f, 0}, {0, 0, 0, 1}};
    p.state.hands[0] = {{-.25f, 1.f, -.25f}, {0, 0, 0, 1}};
    p.state.hands[1] = {{.25f, 1.75f, -.25f}, {0, .5f, 0, .8660254f}};
    p.state.curl[0] = 1.f, p.state.curl[1] = 0.f;
    p.state.standingEye = 1.65f;
    return p;
}
// 0.0.74 MP PROPS: the golden pose with a fixed PROPS tail (values the Python buddy rounds the same way).
Props goldenProps() {
    Props pr;
    pr.flags = propsFlagBatonClub;
    PropItem h;
    h.id = 9, h.kind = propHammer, h.flags = propFlagFlying, h.variant = 2, h.spin = 1.25f;
    h.at = {.25f, 1.5f, -2.f}, h.forward = {0, 1, 0}, h.up = {0, 0, -1};
    pr.add(h);
    PropItem b;
    b.id = 2, b.kind = 4 /* batarang */, b.flags = propFlagInHand | propFlagSide, b.variant = 3, b.spin = -.5f;
    b.at = {.25f, 1.75f, -.5f}, b.forward = {0, 0, -1}, b.up = {1, 0, 0};
    pr.add(b);
    return pr;
}
} // namespace

int main(int argc, char** argv) {
    if (argc >= 2 && !std::strcmp(argv[1], "golden")) {
        hex("pose", encode(goldenPose()));
        Packet pp = goldenPose();
        pp.state.props = goldenProps();
        hex("pose_props", encode(pp));
        Packet h;
        h.type = hello;
        h.role = roleBuddy;
        h.seq = 1;
        h.sentMs = 7;
        std::snprintf(h.name, sizeof h.name, "TEST BUDDY");
        hex("hello", encode(h));
        Packet b;
        b.type = bye;
        b.role = roleBuddy;
        b.seq = 3;
        hex("bye", encode(b));
        std::printf("fnv advanced_suit_2 %08x\n", fnv1a("advanced_suit_2"));
        return 0;
    }
    if (argc >= 4 && !std::strcmp(argv[1], "run")) {
        SessionConfig cfg;
        cfg.port = static_cast<uint16_t>(std::atoi(argv[2]));
        const double seconds = std::atof(argv[3]);
        if (argc >= 5)
            cfg.echo = false; // a game in a join: PLAYER 2 is only the friend ("" = listen, the friend calls)
        if (argc >= 5 && !std::strncmp(argv[4], "host", 4)) { // 0.0.74 MP LOBBY: host[:lobby port] (that port: to 127.0.0.1 only)
            cfg.host = true;
            if (argv[4][4] == ':') {
                cfg.lobbyPort = static_cast<uint16_t>(std::atoi(argv[4] + 5));
                cfg.announceTo = {0x7f000001u};
            }
        } else if (argc >= 5 && argv[4][0]) { // 0.0.74 MP JOIN: call another game (two probes = two players)
            const PeerSetting peer = parsePeerSetting(argv[4]);
            if (!peer.ok) {
                std::fprintf(stderr, "bad peer '%s'\n", argv[4]);
                return 1;
            }
            cfg.peerHost = peer.host;
            cfg.peerPort = peer.port;
        }
        if (argc >= 6)
            cfg.name = argv[5];
        Session s;
        if (!s.start(cfg))
            return 2;
        hero007::Draw fakeShot;
        FxRing fx;
        FxInbox inbox;
        const double t0 = s.now();
        double nextPrint = t0 + 1;
        while (s.now() - t0 < seconds) {
            PoseState p;
            p.hero = 0;
            p.suit = noSuit;
            const float t = static_cast<float>(s.now() - t0);
            p.head = {{0, 1.6f, 0}, Quat::around({0, 1, 0}, .2f * std::sin(t))};
            p.actions = handLeft | handRight;
            p.props = goldenProps(); // 0.0.74 MP PROPS: a thrown hammer and a held batarang ride along
            // phase 3: a lightning bolt from the right hand, and a muzzle flash every half second through the effects ring
            PropLine bolt;
            bolt.kind = lineBolt;
            bolt.from = {.25f, 1.4f, -.4f};
            bolt.to = {.25f, 1.f, -8.f};
            p.props.add(bolt);
            fakeShot.any = true;
            fakeShot.flashAge[1] = std::fmod(t, .5f);
            fakeShot.flashAt[1] = {.25f, 1.4f, -.5f};
            fakeShot.flashWay[1] = {0, 0, -1};
            fx.feed({&fakeShot, nullptr, nullptr}, s.now());
            fx.write(p.props, sendFrame({}, Quat{}, 0.f), s.now());
            s.publish(p);
            {
                const SessionView w = s.view();
                inbox.take(w.state.props, GhostFrame{}, s.now());
            }
            if (s.now() >= nextPrint) {
                nextPrint += 1;
                const SessionView v = s.view();
                std::printf("{\"t\": %.1f, \"running\": %d, \"port\": %u, \"visible\": %d, \"buddy\": %d, \"name\": \"%s\", "
                            "\"ping\": %d, \"hero\": %u, \"suit\": %u, \"root\": [%.3f, %.3f, %.3f], \"actions\": %u, "
                            "\"sent\": %u, \"received\": %u, \"rejected\": %u, \"peerGame\": %d, \"calling\": %d, "
                            "\"props\": %u, \"propKinds\": [%u, %u, %u, %u], \"lines\": %u, \"fxTaken\": %u, "
                            "\"hosting\": %d, \"beacons\": %u}\n",
                            t, v.running, v.port, v.visible, v.buddy, v.buddyName.c_str(), v.pingMs, v.state.hero,
                            v.state.suit, v.state.root.x, v.state.root.y, v.state.root.z, v.state.actions, v.sent,
                            v.received, v.rejected, v.peerGame, v.calling, static_cast<unsigned>(v.state.props.items),
                            v.state.props.item[0].kind, v.state.props.item[1].kind, v.state.props.item[2].kind,
                            v.state.props.item[3].kind, static_cast<unsigned>(v.state.props.lines), inbox.taken(), v.hosting, v.beacons);
                for (const auto& l : s.takeLog())
                    std::printf("# %s\n", l.c_str());
                std::fflush(stdout);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        s.stop();
        for (const auto& l : s.takeLog())
            std::printf("# %s\n", l.c_str());
        return 0;
    }
    if (argc >= 3 && !std::strcmp(argv[1], "lobbyfile")) { // 0.0.74 MP LOBBY: how the game reads an MP-LOBBY.txt
        std::FILE* f = std::fopen(argv[2], "rb");
        if (!f)
            return 2;
        std::string text;
        char buf[4096];
        size_t n;
        while ((n = std::fread(buf, 1, sizeof buf, f)) > 0)
            text.append(buf, n);
        std::fclose(f);
        const LobbySetting l = parseLobbySetting(text);
        std::printf("{\"mode\": \"%s\", \"join\": \"%s\", \"port\": %u, \"name\": \"%s\", \"host\": \"%s\"}\n",
                    l.mode == LobbySetting::host ? "host" : l.mode == LobbySetting::join ? "join" : "none", l.joinHost.c_str(),
                    l.joinPort, l.name.c_str(), l.hostName.c_str());
        return 0;
    }
    if (argc >= 3 && !std::strcmp(argv[1], "lobby")) { // 0.0.74 MP LOBBY: the launcher's list, as JSON once a second
        const double seconds = std::atof(argv[2]);
        const uint16_t port = argc >= 4 ? static_cast<uint16_t>(std::atoi(argv[3])) : lobbyPort;
        UdpSocket sock;
        if (!sock.openShared(0, port)) {
            std::fprintf(stderr, "lobby: %s\n", sock.lastError().c_str());
            return 2;
        }
        HostList hosts;
        const auto clock = [] {
            return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
        };
        const double t0 = clock();
        double nextPrint = t0 + 1;
        uint8_t buf[2048];
        while (clock() - t0 < seconds) {
            Peer from;
            int n;
            while ((n = sock.receive(buf, sizeof buf, from)) > 0) {
                Beacon b;
                if (decodeBeacon(buf, static_cast<size_t>(n), b))
                    hosts.heard(from.addr, b, clock());
            }
            if (clock() >= nextPrint) {
                nextPrint += 1;
                std::printf("{\"t\": %.1f, \"hosts\": [", clock() - t0);
                bool first = true;
                for (const auto& h : hosts.list(clock())) {
                    std::printf("%s{\"addr\": \"%s\", \"name\": \"%s\", \"hero\": %u, \"heroName\": \"%s\", "
                                "\"players\": %u, \"max\": %u, \"protocol\": %u}",
                                first ? "" : ", ", addrText({h.addr, h.beacon.gamePort}).c_str(), h.beacon.name.c_str(),
                                h.beacon.hero, h.beacon.heroName.c_str(), h.beacon.players, h.beacon.maxPlayers,
                                h.beacon.protocol);
                    first = false;
                }
                std::printf("]}\n");
                std::fflush(stdout);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        return 0;
    }
    std::fprintf(stderr, "usage: spidy_mp_probe run <port> <seconds> [peer host[:port] | host[:lobby port]] [name] | "
                         "lobby <seconds> [port] | lobbyfile <path> | golden\n");
    return 1;
}
