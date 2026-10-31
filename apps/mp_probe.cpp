// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
// Made by Agent Mulder. Version 0.0.73 MP SOLO TEST: the game's network side without the game.
//   spidy_mp_probe run <port> <seconds> [peer host[:port]] [name]
//                                         the real Session, publishing a fake player; prints one JSON line
//                                         (0.0.74 MP JOIN: with a peer it calls that game, like MP-PEER.txt)
//                                         a second (tests/mp_buddy_tests.py drives the Test Buddy at it)
//   spidy_mp_probe golden                 hex of fixed packets (the Python buddy must encode the same bytes)
#include "spidy/mp_net.hpp"
#include "spidy/mp_session.hpp"

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
} // namespace

int main(int argc, char** argv) {
    if (argc >= 2 && !std::strcmp(argv[1], "golden")) {
        hex("pose", encode(goldenPose()));
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
        if (argc >= 5 && argv[4][0]) { // 0.0.74 MP JOIN: call another game (two probes = two players)
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
        const double t0 = s.now();
        double nextPrint = t0 + 1;
        while (s.now() - t0 < seconds) {
            PoseState p;
            p.hero = 0;
            p.suit = noSuit;
            const float t = static_cast<float>(s.now() - t0);
            p.head = {{0, 1.6f, 0}, Quat::around({0, 1, 0}, .2f * std::sin(t))};
            p.actions = handLeft | handRight;
            s.publish(p);
            if (s.now() >= nextPrint) {
                nextPrint += 1;
                const SessionView v = s.view();
                std::printf("{\"t\": %.1f, \"running\": %d, \"port\": %u, \"visible\": %d, \"buddy\": %d, \"name\": \"%s\", "
                            "\"ping\": %d, \"hero\": %u, \"suit\": %u, \"root\": [%.3f, %.3f, %.3f], \"actions\": %u, "
                            "\"sent\": %u, \"received\": %u, \"rejected\": %u, \"peerGame\": %d, \"calling\": %d}\n",
                            t, v.running, v.port, v.visible, v.buddy, v.buddyName.c_str(), v.pingMs, v.state.hero,
                            v.state.suit, v.state.root.x, v.state.root.y, v.state.root.z, v.state.actions, v.sent,
                            v.received, v.rejected, v.peerGame, v.calling);
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
    std::fprintf(stderr, "usage: spidy_mp_probe run <port> <seconds> [peer host[:port]] [name] | golden\n");
    return 1;
}
