// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
// Made by Agent Mulder. Version 0.0.73 MP SOLO TEST: the packet, its checks, interpolation, the loopback
// delay, the partner placement and the buddy handshake - pure parts first, then the real session on
// 127.0.0.1 (a loopback round trip and a handshake from a raw socket).
#include "spidy/combat38.hpp"
#include "spidy/mesh_model.hpp"
#include "spidy/mp_body.hpp"
#include "spidy/mp_ghost.hpp"
#include "spidy/mp_net.hpp"
#include "spidy/mp_session.hpp"
#include "spidy/mp_socket.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <random>
#include <thread>

using namespace spidy;
using namespace spidy::mp;

namespace {
int failures = 0, checks = 0;
void check(bool ok, const char* what) {
    ++checks;
    if (!ok) {
        ++failures;
        std::printf("FAIL: %s\n", what);
    }
}
bool near(float a, float b, float e = 1e-4f) { return std::abs(a - b) <= e; }
bool near(Vec3 a, Vec3 b, float e = 1e-4f) { return near(a.x, b.x, e) && near(a.y, b.y, e) && near(a.z, b.z, e); }
bool sameQ(Quat a, Quat b, float e = 1e-4f) {
    const float d = std::abs(a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w);
    return d > 1 - e;
}
void sleepMs(int ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }

PoseState sample() {
    PoseState s;
    s.hero = 3;
    s.suit = 7;
    s.suitHash = fnv1a("spiderman_2099");
    s.actions = flying | firing | handLeft | handRight;
    s.root = {12.5f, -3.25f, 100.f};
    s.yaw = 1.25f;
    s.head = {{.02f, 1.62f, -.05f}, Quat::around({0, 1, 0}, .3f) * Quat::around({1, 0, 0}, -.2f)};
    s.hands[0] = {{-.3f, 1.1f, -.35f}, Quat::around({0, 0, 1}, .4f)};
    s.hands[1] = {{.28f, 1.3f, -.45f}, Quat::around({1, 1, 0}, -.7f)};
    s.curl[0] = 1.f, s.curl[1] = .5f;
    s.standingEye = 1.712f;
    return s;
}
std::vector<uint8_t> resealed(std::vector<uint8_t> b) {
    const uint32_t c = crc32(b.data(), b.size() - 4);
    for (int i = 0; i < 4; ++i)
        b[b.size() - 4 + i] = static_cast<uint8_t>(c >> (8 * i));
    return b;
}
void putF(std::vector<uint8_t>& b, size_t at, float f) { std::memcpy(b.data() + at, &f, 4); }

void testCrcAndHash() {
    const char* t = "123456789";
    check(crc32(reinterpret_cast<const uint8_t*>(t), 9) == 0xcbf43926u, "crc32 is zlib's (check value)");
    check(fnv1a("") == 2166136261u && fnv1a("a") == 0xe40c292cu, "fnv1a 32");
}

void testRoundTrip() {
    Packet p;
    p.type = pose;
    p.role = roleGame;
    p.seq = 0xfffffffeu;
    p.sentMs = 123456789;
    p.state = sample();
    const auto b = encode(p);
    check(b.size() == overheadBytes + poseSentBytes && b.size() == 160, "a pose packet is 160 bytes (0.0.74: + the flight lean, + the 0.0.73 MPSUIT LOOK tail)");
    check(b[0] == 'H' && b[1] == 'V' && b[2] == 'M' && b[3] == 'P' && b[4] == 1, "magic and version on the wire");
    Packet q;
    check(decode(b.data(), b.size(), q) == Error::none, "a pose packet decodes");
    check(q.type == pose && q.role == roleGame && q.seq == p.seq && q.sentMs == p.sentMs, "header round trip");
    const PoseState& a = p.state;
    const PoseState& s = q.state;
    check(s.hero == a.hero && s.suit == a.suit && s.suitHash == a.suitHash && s.actions == a.actions, "ids and flags");
    check(near(s.root, a.root, 0) && near(s.yaw, a.yaw, 0), "root and yaw exact");
    check(near(s.head.position, a.head.position, 0) && sameQ(s.head.orientation, a.head.orientation), "head");
    for (int i = 0; i < 2; ++i)
        check(near(s.hands[i].position, a.hands[i].position, 0) && sameQ(s.hands[i].orientation, a.hands[i].orientation), "hands");
    check(near(s.curl[0], 1.f) && near(s.curl[1], .5f, 1.f / 255) && near(s.standingEye, 1.712f, .001f), "curl and eye");
    for (uint8_t t : {hello, welcome, ping, pong, bye}) {
        Packet x;
        x.type = t;
        x.role = roleBuddy;
        std::snprintf(x.name, sizeof x.name, "buddy one");
        x.token = 77, x.pingSentMs = 99;
        const auto e = encode(x);
        Packet y;
        check(!e.empty() && decode(e.data(), e.size(), y) == Error::none && y.type == t, "every type round trips");
        if (t == hello)
            check(std::string(y.name) == "BUDDY ONE" && y.protocol == version && e.size() == 44, "hello: name upper case, 44 bytes");
        if (t == pong)
            check(y.token == 77 && y.pingSentMs == 99, "pong carries the ping's time");
        if (t == bye)
            check(e.size() == overheadBytes, "bye is header only");
    }
    Packet bad;
    bad.type = 42;
    check(encode(bad).empty(), "an unknown type is never sent");
}

void testVersioning() {
    Packet p;
    p.state = sample();
    auto b = encode(p);
    Packet q;
    auto v2 = b;
    v2[4] = 2;
    check(decode(v2.data(), v2.size(), q) == Error::badVersion, "version 2 is refused (before its checksum)");
    v2 = resealed(v2);
    check(decode(v2.data(), v2.size(), q) == Error::badVersion, "version 2 refused even when sealed");
    auto v0 = resealed([&] { auto c = b; c[4] = 0; return c; }());
    check(decode(v0.data(), v0.size(), q) == Error::badVersion, "version 0 refused");
    auto m = b;
    m[0] = 'X';
    check(decode(m.data(), m.size(), q) == Error::badMagic, "wrong magic refused");
    // The same version with a longer payload (a later v1 build): the known part is read, the rest ignored.
    auto longer = b;
    longer.resize(b.size() - 4);
    for (int i = 0; i < 8; ++i)
        longer.push_back(0xab);
    longer.resize(longer.size() + 4);
    longer[18] = static_cast<uint8_t>(poseSentBytes + 8);
    longer = resealed(longer);
    check(decode(longer.data(), longer.size(), q) == Error::none && near(q.state.yaw, p.state.yaw, 0),
          "a longer v1 payload is accepted, its tail ignored");
    // 0.0.73 MPSUIT: the LOOK tail round trips; a 0.0.73 test 6 pose (112 bytes, no tail) still decodes, lookless.
    {
        Packet lp = p;
        lp.state.hero = 0; // Spider-Man: the LOOK is his suit's colours
        setLook(lp.state, {.5f, .02f, .03f}, {.01f, .01f, .01f}, {1, 1, 1}, true);
        const auto lb = encode(lp);
        Packet lq;
        check(decode(lb.data(), lb.size(), lq) == Error::none && lq.state.hasLook && lq.state.lookLearned,
              "the LOOK tail decodes");
        const Look k = lookOf(lq.state);
        check(k.spider && k.learned && near(k.primary.x, .5f, .02f) && near(k.secondary.y, .01f, .01f),
              "the LOOK colours round trip (sRGB bytes)");
        auto old = lb;
        auto leanOnly = lb; // 0.0.74: a lean-only pose (124 bytes, no LOOK tail) decodes, without a look
        leanOnly.erase(leanOnly.begin() + headerBytes + poseBytes, leanOnly.begin() + headerBytes + poseSentBytes);
        leanOnly[18] = static_cast<uint8_t>(poseBytes);
        leanOnly = resealed(leanOnly);
        Packet loq;
        check(decode(leanOnly.data(), leanOnly.size(), loq) == Error::none && !loq.state.hasLook && near(loq.state.yaw, p.state.yaw, 0),
              "a lean-only pose (no LOOK tail) still decodes, without a look");
        old.erase(old.begin() + headerBytes + poseBytesV1, old.begin() + headerBytes + poseSentBytes);
        old[18] = static_cast<uint8_t>(poseBytesV1);
        old = resealed(old);
        Packet oq;
        check(decode(old.data(), old.size(), oq) == Error::none && !oq.state.hasLook && near(oq.state.yaw, p.state.yaw, 0),
              "a test 6 pose (no LOOK tail) still decodes, without a look");
        auto noMark = lb;
        noMark[headerBytes + poseBytes] = 'X';
        noMark = resealed(noMark);
        check(decode(noMark.data(), noMark.size(), oq) == Error::none && !oq.state.hasLook, "a tail without 'L' is ignored");
    }
    auto t = resealed([&] { auto c = b; c[5] = 99; return c; }());
    check(decode(t.data(), t.size(), q) == Error::badType, "an unknown type is refused");
}

void testFlightLean() {
    // 0.0.74 FLIGHT POSE (fix 27): the lean rides the pose; a 0.0.73 (112-byte) pose is still read, upright.
    Packet p;
    p.state = sample();
    p.state.leanUp = normalized(Vec3{0, .2f, -1});
    auto b = encode(p);
    Packet q;
    check(decode(b.data(), b.size(), q) == Error::none && near(q.state.leanUp, p.state.leanUp, 1e-5f), "the lean round trips");
    std::vector<uint8_t> old(b.begin(), b.begin() + headerBytes + poseBytesV1);
    old.resize(old.size() + crcBytes);
    old[18] = static_cast<uint8_t>(poseBytesV1);
    old = resealed(old);
    check(decode(old.data(), old.size(), q) == Error::none && near(q.state.leanUp, {0, 1, 0}, 0) &&
              near(q.state.yaw, p.state.yaw, 0),
          "a 0.0.73 pose decodes, upright");
    std::vector<uint8_t> shorter(b.begin(), b.begin() + headerBytes + poseBytesV1 - 2);
    shorter.resize(shorter.size() + crcBytes);
    shorter[18] = static_cast<uint8_t>(poseBytesV1 - 2);
    shorter = resealed(shorter);
    check(decode(shorter.data(), shorter.size(), q) == Error::badLength, "shorter than 0.0.73's: refused");
    p.state.leanUp = {std::nanf(""), 1, 0};
    b = encode(p);
    check(decode(b.data(), b.size(), q) == Error::none && near(q.state.leanUp, {0, 1, 0}, 0), "a broken lean: upright, still drawn");
    check(near(safeLeanUp({0, -1, 0}), {0, 1, 0}, 0) && near(safeLeanUp({0, 0, 0}), {0, 1, 0}, 0) &&
              near(safeLeanUp({0, 0, -2}), {0, 0, -1}, 1e-6f),
          "upside down or empty: upright; else unit");
    // The local lean (tracking frame, flying -z flat out) -> the loopback partner leans toward the player.
    LocalPose l;
    l.eye = {{0, 1.6f, 0}, {}};
    l.leanUp = {0, 0, -1};
    const PoseState s = fromLocal(l, 1, noSuit, 0, flying);
    check(near(s.leanUp, {0, 0, -1}, 1e-5f), "in the yawed frame");
    AnchorFollow f;
    f.update(0, .016f);
    const Ghost g = partner(s, f.spot({}), f.ghostYaw(), true, false);
    check(near(g.up, {0, 0, 1}, 1e-4f), "the partner faces the player: its lean comes toward him");
    const suit_wear::Tracked t = toTracked(g, {}, {});
    check(t.up.z > .99f && near(dot(t.eye - t.feet, t.eye - t.feet), 1.6f * 1.6f, 1e-3f) && t.feet.z < t.eye.z - 1.5f,
          "Player 2 lies flat, head toward the player, feet trailing");
    check(t.feet.z < t.eye.z && std::abs(t.feet.y - t.eye.y) < .05f, "feet level with the head");
    LocalPose still = l;
    still.leanUp = {0, 1, 0};
    const suit_wear::Tracked u = toTracked(partner(fromLocal(still, 1, noSuit, 0, 0), f.spot({}), f.ghostYaw(), true, false), {}, {});
    check(near(u.up, {0, 1, 0}, 1e-5f) && u.lean.w == 1 && u.lean.x == 0, "hovering: upright, unchanged");
    PoseState a = s, c = s;
    c.leanUp = {0, 1, 0};
    check(std::abs(length(blend(a, c, .5f).leanUp) - 1) < 1e-4f, "blended leans stay unit");
}

void testMalformed() {
    Packet p;
    p.state = sample();
    const auto b = encode(p);
    Packet q;
    bool allRefused = true;
    for (size_t n = 0; n < b.size(); ++n)
        if (decode(b.data(), n, q) == Error::none)
            allRefused = false;
    check(allRefused, "every short packet is refused");
    check(decode(nullptr, 0, q) == Error::tooShort && decode(b.data(), 3, q) == Error::tooShort, "too short");
    std::vector<uint8_t> big(maxPacket + 1, 0);
    check(decode(big.data(), big.size(), q) == Error::tooLong, "too long");
    bool flips = true;
    for (size_t i = 0; i < b.size(); ++i)
        for (int bit = 0; bit < 8; ++bit) {
            auto c = b;
            c[i] ^= static_cast<uint8_t>(1 << bit);
            if (decode(c.data(), c.size(), q) == Error::none)
                flips = false;
        }
    check(flips, "every single flipped bit is caught");
    auto len = resealed([&] { auto c = b; c[18] = 50; return c; }());
    check(decode(len.data(), len.size(), q) == Error::badLength, "a payload length that disagrees is refused");
    auto nanYaw = b;
    putF(nanYaw, headerBytes + 20, std::nanf(""));
    nanYaw = resealed(nanYaw);
    check(decode(nanYaw.data(), nanYaw.size(), q) == Error::badValue, "NaN yaw refused");
    auto inf = b;
    putF(inf, headerBytes + 24, INFINITY);
    inf = resealed(inf);
    check(decode(inf.data(), inf.size(), q) == Error::badValue, "infinite head refused");
    auto far = b;
    putF(far, headerBytes + 52, 400.f);
    far = resealed(far);
    check(decode(far.data(), far.size(), q) == Error::badValue, "a hand 400 m away refused");
    auto zeroQ = b;
    for (int i = 0; i < 4; ++i)
        putF(zeroQ, headerBytes + 36 + 4 * i, 0.f);
    zeroQ = resealed(zeroQ);
    check(decode(zeroQ.data(), zeroQ.size(), q) == Error::badValue, "a zero orientation refused");
    auto scaled = b;
    putF(scaled, headerBytes + 48, 1.2f * q.state.head.orientation.w + 1.f); // not unit but finite and near
    scaled = resealed(scaled);
    const Error se = decode(scaled.data(), scaled.size(), q);
    if (se == Error::none) {
        const Quat h = q.state.head.orientation;
        check(near(h.x * h.x + h.y * h.y + h.z * h.z + h.w * h.w, 1.f, 1e-4f), "a near-unit orientation is renormalised");
    }
    auto hero = resealed([&] { auto c = b; c[headerBytes] = 200; return c; }());
    check(decode(hero.data(), hero.size(), q) == Error::badValue, "an impossible hero refused");
    auto role = resealed([&] { auto c = b; c[6] = 9; return c; }());
    check(decode(role.data(), role.size(), q) == Error::badValue, "an unknown role refused");
    std::mt19937 rng(1234);
    int accepted = 0;
    for (int k = 0; k < 20000; ++k) {
        std::vector<uint8_t> g(rng() % 200);
        for (auto& x : g)
            x = static_cast<uint8_t>(rng());
        if (k % 2 && g.size() >= 8)
            g[0] = 'H', g[1] = 'V', g[2] = 'M', g[3] = 'P', g[4] = 1;
        if (decode(g.data(), g.size(), q) == Error::none)
            ++accepted;
    }
    check(accepted == 0, "20000 random datagrams: none accepted, none crashed");
}

void testInterpolation() {
    Interpolator in;
    in.delay = .1f;
    const double lat = .4;
    for (uint32_t k = 0; k < 10; ++k) {
        PoseState s;
        s.root = {static_cast<float>(k), 0, 0};
        s.yaw = 0;
        check(in.push(k + 1, 1000 + k * 100, 1.0 + k * .1 + lat, s), "in order: accepted");
    }
    // Newest sender time 1.9 s arrived at 2.3; render at now - offset - delay.
    Sampled s = in.sample(2.3);
    check(s.visible && near(s.alpha, 1.f), "visible, full");
    check(near(s.state.root.x, 8.f, 1e-3f), "drawn 0.1 s behind the newest (8 of 9)");
    s = in.sample(2.25);
    check(near(s.state.root.x, 7.5f, 1e-3f), "half way between two packets: lerped");
    PoseState late;
    check(!in.push(5, 1400, 2.31, late), "an old sequence is dropped");
    check(!in.push(10, 1900, 2.31, late), "a repeated sequence is dropped");
    check(in.dropped() == 2, "two dropped");
    // Hold, fade, hide.
    s = in.sample(2.3 + .2);
    check(s.visible && near(s.alpha, 1.f) && near(s.state.root.x, 9.f, 1e-3f), "holds the last pose");
    s = in.sample(2.3 + 1.25);
    check(s.visible && s.alpha > .4f && s.alpha < .6f, "fading after a second without packets");
    s = in.sample(2.3 + 2.3);
    check(!s.visible, "hidden after 2.25 s without packets");
    // Sender restart: far behind - a fresh timeline.
    PoseState r;
    r.root = {50, 0, 0};
    check(in.push(1, 5, 10.0, r), "a restarted sender is accepted");
    s = in.sample(10.0);
    check(s.visible && near(s.state.root.x, 50.f), "and drawn");
    // Wrapping sequence and clock.
    Interpolator w;
    PoseState a, b2;
    a.yaw = 3.0f, b2.yaw = -3.0f;
    check(w.push(0xffffffffu, 0xffffff00u, 0, a), "seq near the end");
    check(w.push(0u, 0xffffff00u + 100, .1, b2), "seq wrapped, clock wrapped");
    s = w.sample(.15);
    const float mid = std::abs(s.state.yaw);
    check(mid > 3.1f, "yaw blends the short way across pi");
    // Blend of orientations stays unit.
    PoseState q1, q2;
    q1.head.orientation = Quat::around({0, 1, 0}, .2f);
    q2.head.orientation = Quat::around({0, 1, 0}, 1.2f);
    const Quat m = blend(q1, q2, .5f).head.orientation;
    check(sameQ(m, Quat::around({0, 1, 0}, .7f), 1e-3f), "nlerp half way");
    // Jitter: arrivals spread +-20 ms, the drawn stream never steps backwards.
    Interpolator j;
    std::mt19937 rng(7);
    float last = -1;
    bool monotonic = true;
    double clock = 0;
    for (int k = 0; k < 300; ++k) {
        PoseState p;
        p.root = {k * .033f, 0, 0};
        const double arrive = k * .033 + .05 + (rng() % 40) / 1000.0;
        clock = std::max(clock, arrive); // the local clock never runs backwards (arrivals may cross)
        j.push(k + 1, static_cast<uint32_t>(k * 33), clock, p);
        const float x = j.sample(clock).state.root.x;
        if (x + 1e-4f < last)
            monotonic = false;
        last = std::max(last, x);
    }
    check(monotonic, "jittered arrivals: smooth, never backwards");
}

void testDelayAndEcho() {
    DelayLine d;
    d.hold = .4;
    const uint8_t x[3] = {1, 2, 3};
    check(d.push(0, x, 3) && d.push(.01, x, 2), "queued");
    std::vector<uint8_t> out;
    double held;
    check(!d.pop(.39, out, held), "not before its hold");
    check(d.pop(.4, out, held) && out.size() == 3 && near(static_cast<float>(held), .4f), "after 0.4 s, in order");
    check(!d.pop(.405, out, held) && d.pop(.4101, out, held) && out.size() == 2, "the second one 10 ms later");
    for (int i = 0; i < 400; ++i)
        d.push(1, x, 3);
    check(d.size() == DelayLine::most, "the echo never grows without bound");
    Packet p;
    p.state = sample();
    p.seq = 9;
    const auto b = encode(p);
    std::vector<uint8_t> r;
    check(echoReply(b.data(), b.size(), 401, r), "the echo answers a pose");
    Packet q;
    check(decode(r.data(), r.size(), q) == Error::none && q.role == roleEcho && q.echoHoldMs == 401 && q.seq == 9,
          "the echo is a valid packet marked echo, its hold stamped");
    Packet h;
    h.type = hello;
    const auto hb = encode(h);
    check(!echoReply(hb.data(), hb.size(), 1, r), "the echo answers nothing but poses");
    check(!echoReply(b.data(), b.size() - 1, 1, r), "nor a broken pose");
}

void testPartner() {
    // The player faces -z at the origin; the ghost 3 m ahead faces him.
    AnchorFollow f;
    check(near(f.update(0, .016f), 0), "first facing taken");
    const Vec3 spot = f.spot({0, 0, 0});
    check(near(spot, {0, 0, -3}), "3 m ahead");
    check(near(std::abs(f.ghostYaw()), 3.14159f, 1e-3f), "facing back at him");
    LocalPose l;
    l.eye = {{0, 1.6f, 0}, {}};
    l.hand[0] = false, l.hand[1] = true;
    l.grip[1] = {{.3f, 1.5f, -.3f}, {}};
    const PoseState s = fromLocal(l, 0, noSuit, 0, swinging);
    check((s.actions & handRight) && !(s.actions & handLeft) && (s.actions & swinging), "hand flags from tracking");
    const Ghost g = partner(s, spot, f.ghostYaw(), true, false);
    check(near(g.eye, {0, 1.6f, -3}, 1e-3f), "ghost head over its feet");
    check(near(g.eyeForward, {0, 0, 1}, 1e-3f), "ghost looks at him");
    check(g.hand[0] && !g.hand[1], "mirrored: his right hand is the ghost's left");
    check(near(g.grip[0], {.3f, 1.5f, -2.7f}, 1e-3f), "on his right, reaching towards him - a mirror");
    const Vec3 c = cross(g.gripX[0], g.gripUp[0]);
    check(dot(c, g.gripForward[0]) < -.99f, "the mirrored grip frame is right-handed again (x cross y = -forward)");
    // Turned player (yaw 90 deg left): the local pose is body relative.
    LocalPose t = l;
    const Quat turn = Quat::yaw(1.5707963f);
    t.eye.orientation = turn;
    t.grip[1].position = turn.rotate(l.grip[1].position);
    t.grip[1].orientation = turn;
    const PoseState st = fromLocal(t, 0, noSuit, 0, 0);
    check(near(st.yaw, 1.5707963f, 1e-4f) && near(st.hands[1].position, l.grip[1].position, 1e-4f), "pose kept body relative");
    // A buddy walking: its root moves it about the anchor, not mirrored.
    PoseState b;
    b.root = {.8f, 0, 0};
    b.yaw = 0;
    b.actions = handRight;
    b.hands[1] = {{.3f, 1.6f, -.2f}, {}};
    const Ghost bg = partner(b, spot, f.ghostYaw(), false, true);
    check(near(bg.feet, {-.8f, 0, -3}, 1e-3f), "buddy's +x is its own right: from his view, on his left");
    check(bg.hand[1] && near(bg.grip[1], {-.8f - .3f, 1.6f, -3 + .2f}, 1e-3f), "buddy's right hand on its right");
    // Lazy following.
    AnchorFollow lazy;
    lazy.update(0, 0);
    lazy.update(.5f, .1f);
    check(near(lazy.yaw, 0), "a small look away leaves the ghost where it is");
    for (int i = 0; i < 200; ++i)
        lazy.update(1.2f, .016f);
    check(near(lazy.yaw, 1.2f, .1f), "a big turn: the ghost comes round in front again");
    check(near(wrapAngle(7.f), 7.f - 6.2831853f, 1e-4f) && near(wrapAngle(-3.2f), -3.2f + 6.2831853f, 1e-4f), "wrap");
    check(near(yawOf(forwardOf(.7f)), .7f, 1e-5f), "yaw of forward");
}

void testHandshake() {
    BuddyLink link;
    link.timeout = 3;
    const Peer a{loopbackAddr, 5000}, other{loopbackAddr, 5001};
    Packet p;
    p.type = pose;
    p.role = roleBuddy;
    check(link.onPacket(p, a, 0) == BuddyLink::Step::ignore, "a pose before HELLO is ignored");
    Packet h;
    h.type = hello;
    h.role = roleBuddy;
    std::snprintf(h.name, sizeof h.name, "test buddy");
    check(link.onPacket(h, a, 1) == BuddyLink::Step::welcome && link.connected(1), "HELLO: WELCOME, connected");
    check(link.name() == "TEST BUDDY", "its name");
    check(link.onPacket(p, a, 1.1) == BuddyLink::Step::pose, "then its poses count");
    check(link.onPacket(p, other, 1.2) == BuddyLink::Step::ignore, "a stranger's pose does not");
    Packet game = p;
    game.role = roleGame;
    check(link.onPacket(game, a, 1.3) == BuddyLink::Step::ignore, "a pose not marked buddy does not");
    check(link.connected(4.2) && !link.connected(4.4), "silent for 3 s: gone");
    Packet old = h;
    old.protocol = 2;
    BuddyLink l2;
    check(l2.onPacket(old, a, 0) == BuddyLink::Step::ignore && !l2.connected(0), "a buddy of another protocol is not let in");
    check(link.onPacket(h, other, 5) == BuddyLink::Step::welcome && link.peer() == other, "a new buddy after the old one left");
    Packet bye;
    bye.type = mp::bye;
    bye.role = roleBuddy;
    check(link.onPacket(bye, other, 5.5) == BuddyLink::Step::bye && !link.connected(5.5), "BYE: gone at once");
}

PoseState timed(double t) {
    PoseState s = sample();
    s.root = {static_cast<float>(t), 0, 0};
    return s;
}

void testSessionLoopback() {
    Session session;
    SessionConfig cfg;
    cfg.port = 39570; // not the game's: a test never collides with a running game
    check(session.start(cfg), "session starts");
    const double t0 = session.now();
    while (session.now() - t0 < 1.6) {
        session.publish(timed(session.now()));
        sleepMs(5);
    }
    const SessionView v = session.view();
    check(v.running && v.port >= 39570 && v.port < 39575, "socket up on the test port");
    check(v.visible && !v.buddy, "the loopback ghost is visible");
    const double behind = session.now() - v.state.root.x;
    std::printf("  loopback: %.0f ms behind, ping %d ms, sent %u, received %u\n", behind * 1000, v.pingMs, v.sent, v.received);
    check(behind > .42 && behind < .66, "the ghost is about half a second behind");
    check(v.pingMs >= 0 && v.pingMs < 50, "loopback ping measured (the echo's hold left out)");
    check(v.sent >= 35 && v.received >= 20, "about 30 packets a second each way");
    check(v.state.hero == 3 && v.state.suit == 7, "his hero and suit come back");
    // A malformed packet at the game's port: refused, logged, nothing breaks.
    UdpSocket raw;
    check(raw.open(loopbackAddr, 0), "a raw socket");
    const uint8_t junk[30] = {'H', 'V', 'M', 'P', 1, 1};
    raw.send(junk, sizeof junk, {loopbackAddr, v.port});
    raw.send(junk, 5, {loopbackAddr, v.port});
    sleepMs(60);
    const SessionView v2 = session.view();
    check(v2.rejected >= 2 && v2.running, "malformed packets refused, session still up");
    bool logged = false;
    for (const auto& line : session.takeLog())
        if (line.find("refused") != std::string::npos)
            logged = true;
    check(logged, "and logged");
    // Nothing published: the ghost fades and hides.
    session.clearLocal();
    sleepMs(2900);
    check(!session.view().visible, "no pose: the ghost goes away");
    session.stop();
    check(!session.view().running && !session.view().visible, "stopped: hidden");
}

void testSessionBuddy() {
    Session session;
    SessionConfig cfg;
    cfg.port = 39580;
    cfg.buddyTimeout = 1.0;
    session.start(cfg);
    for (int i = 0; i < 40; ++i) {
        session.publish(timed(session.now()));
        sleepMs(5);
    }
    const uint16_t port = session.view().port;
    UdpSocket buddy;
    check(buddy.open(loopbackAddr, 0), "buddy socket");
    const Peer game{loopbackAddr, port};
    Packet h;
    h.type = hello;
    h.role = roleBuddy;
    std::snprintf(h.name, sizeof h.name, "C++ BUDDY");
    const auto hb = encode(h);
    bool welcomed = false;
    std::vector<uint8_t> buf(600);
    for (int i = 0; i < 100 && !welcomed; ++i) {
        if (i % 10 == 0)
            buddy.send(hb.data(), hb.size(), game);
        session.publish(timed(session.now()));
        Peer from;
        int n;
        while ((n = buddy.receive(buf.data(), buf.size(), from)) > 0) {
            Packet q;
            if (decode(buf.data(), static_cast<size_t>(n), q) == Error::none && q.type == welcome)
                welcomed = true;
        }
        sleepMs(10);
    }
    check(welcomed, "HELLO -> WELCOME");
    uint32_t seq = 0;
    int gamePoses = 0, pings = 0;
    for (int i = 0; i < 80; ++i) {
        Packet p;
        p.type = pose;
        p.role = roleBuddy;
        p.seq = ++seq;
        p.sentMs = static_cast<uint32_t>(session.now() * 1000);
        p.state = sample();
        p.state.hero = 9;
        p.state.root = {.8f, 0, 0};
        const auto pb = encode(p);
        buddy.send(pb.data(), pb.size(), game);
        session.publish(timed(session.now()));
        Peer from;
        int n;
        while ((n = buddy.receive(buf.data(), buf.size(), from)) > 0) {
            Packet q;
            if (decode(buf.data(), static_cast<size_t>(n), q) != Error::none)
                continue;
            if (q.type == pose)
                ++gamePoses;
            if (q.type == mp::ping) {
                ++pings;
                Packet r;
                r.type = pong;
                r.role = roleBuddy;
                r.token = q.token;
                r.pingSentMs = q.sentMs;
                const auto rb = encode(r);
                buddy.send(rb.data(), rb.size(), game);
            }
        }
        sleepMs(16);
    }
    SessionView v = session.view();
    std::printf("  buddy: visible %d, name '%s', ping %d ms, game poses at the buddy %d, pings %d\n", v.visible,
                v.buddyName.c_str(), v.pingMs, gamePoses, pings);
    check(v.buddy && v.visible && v.state.hero == 9, "the buddy replaces the loopback ghost");
    check(v.buddyName == "C++ BUDDY", "named");
    check(gamePoses > 20, "the game's poses go to the buddy");
    check(pings >= 1 && v.pingMs >= 0, "ping / pong measured");
    // BYE: back to the loopback.
    Packet bye;
    bye.type = mp::bye;
    bye.role = roleBuddy;
    const auto bb = encode(bye);
    buddy.send(bb.data(), bb.size(), game);
    for (int i = 0; i < 120; ++i) {
        session.publish(timed(session.now()));
        sleepMs(5);
    }
    v = session.view();
    check(!v.buddy && v.visible && v.state.hero == 3, "BYE: the loopback ghost is back");
    // A buddy that goes quiet is dropped after its timeout.
    for (int i = 0; i < 3; ++i)
        buddy.send(hb.data(), hb.size(), game), sleepMs(20);
    check(session.view().buddy, "connected again");
    for (int i = 0; i < 260; ++i) {
        session.publish(timed(session.now()));
        sleepMs(5);
    }
    check(!session.view().buddy, "a silent buddy is dropped");
    session.stop();
}

void testSocketFailure() {
    // Port taken by five sockets: the session reports it and stays down - no throw.
    UdpSocket block[5];
    bool all = true;
    for (int i = 0; i < 5; ++i)
        all = block[i].open(loopbackAddr, static_cast<uint16_t>(39590 + i)) && all;
    if (!all) {
        std::printf("  (ports 39590-39594 busy, socket failure test skipped)\n");
        return;
    }
    Session s;
    SessionConfig cfg;
    cfg.port = 39590;
    s.start(cfg);
    sleepMs(100);
    const SessionView v = s.view();
    check(!v.running && !v.visible, "no port: nothing drawn");
    bool logged = false;
    for (const auto& l : s.takeLog())
        logged = logged || l.find("no UDP port") != std::string::npos;
    check(logged, "and it says why");
    s.stop();
}
void testGhostDrawing() {
    // The wrist setting: MP SOLO TEST, 0.0.74 test 1: OFF first (the default, value 0), one sentence saying no restart.
    const auto& o = combat38::optionInfo[combat38::mpSoloTest];
    const std::string h = o.help;
    check(std::string(o.key) == "mp_solo_test" && std::string(o.label) == "MP SOLO TEST" && o.count == 2 &&
              std::string(o.values[0]) == "OFF" && std::string(o.values[1]) == "ON",
          "MP SOLO TEST: OFF first (the default, 0.0.74 test 1), ON");
    check(h.size() <= 80 && h.find('.') == h.size() - 1 && h.find("no restart") != std::string::npos, "its subtext: one sentence, no restart");
    // Suit ids: the worn slot <-> the packet's id and hash <-> the slot drawn.
    check(suitIdOfSlot(0) == noSuit && suitIdOfSlot(mesh_model::slotSuitEnd) == noSuit, "not a suit slot: the hero's own look");
    for (uint32_t k = 0; k < mesh_model::suitSlots; ++k) {
        const uint8_t id = suitIdOfSlot(mesh_model::slotSuitFirst + k);
        PoseState s;
        s.suit = id;
        s.suitHash = suitHashOf(id);
        check(id == k && s.suitHash == fnv1a(mesh_model::suitFolders[k]) && slotOfState(s) == mesh_model::slotSuitFirst + k,
              "every suit's slot comes back");
    }
    {
        PoseState s; // another build's order: the hash wins over the id
        s.suit = 0;
        s.suitHash = fnv1a(mesh_model::suitFolders[5]);
        check(slotOfState(s) == mesh_model::slotSuitFirst + 5, "the folder the hash names");
        s.suitHash = 0x1234567u;
        check(slotOfState(s) == 0, "an unknown suit: the figure");
        s.suit = noSuit;
        check(slotOfState(s) == 0, "no suit: the figure");
    }
    // Placement in the overlay's space.
    LocalPose l;
    l.eye = {{0, 1.6f, 0}, {}};
    l.hand[0] = l.hand[1] = true;
    l.grip[0] = {{-.3f, 1.2f, -.2f}, {}};
    l.grip[1] = {{.3f, 1.2f, -.2f}, {}};
    const PoseState s = fromLocal(l, 9, noSuit, 0, 0);
    AnchorFollow f;
    f.update(0, .016f);
    const Ghost g = partner(s, f.spot({}), f.ghostYaw(), true, false);
    const Vec3 origin{100, 5, -40};
    const Quat turn = Quat::yaw(1.5707963f);
    const suit_wear::Tracked t = toTracked(g, origin, turn);
    check(near(t.feet, origin + turn.rotate(Vec3{0, 0, -3}), 1e-3f), "3 m ahead of the player's feet, in his turned frame");
    check(near(t.up, {0, 1, 0}, 1e-4f) && near(dot(t.eye - t.feet, t.up), 1.6f, 1e-3f), "standing upright at his eye height");
    check(dot(t.eyeForward, turn.rotate(Vec3{0, 0, 1})) > .99f, "facing back at him");
    std::vector<Vertex> out;
    const Vec3 viewer = origin + Vec3{0, 1.6f, 0};
    const size_t n = appendFigure(out, t, viewer, heroColor(9), .8f);
    bool finiteAll = n > 0, inBox = true;
    for (const auto& v : out) {
        finiteAll = finiteAll && std::isfinite(v.position.x) && std::isfinite(v.position.y) && std::isfinite(v.position.z);
        inBox = inBox && length(v.position - t.feet) < 2.5f;
    }
    check(n > 100 && finiteAll && inBox, "the figure: finite, around the ghost");
    suit_wear::Tracked bad = t;
    bad.feet.x = std::nanf("");
    check(appendFigure(out, bad, viewer, heroColor(9), 1) == 0, "a broken pose draws nothing");
    check(nameplate(false, "", 3) == "PLAYER 2 (YOU, LOOPBACK)  PING 3 MS", "the loopback's nameplate");
    check(nameplate(true, "TEST BUDDY", 12) == "PLAYER 2 (TEST BUDDY)  PING 12 MS", "the buddy's nameplate");
    check(nameplate(true, "", -1) == "PLAYER 2 (TEST BUDDY)  PING --", "no ping yet");
    out.clear();
    check(appendNameplate(out, nameplate(false, "", 3), t.feet + Vec3{0, 2, 0}, t.up, viewer, 1) > 50, "the nameplate drawn");
    check(appendNameplate(out, "X", viewer, t.up, viewer, 1) == 0, "at the eye itself: nothing");
    check(heroColor(200).x == heroColor(10).x, "a hero past the roster: the last colour");
    // 0.0.73 MPSUIT PLAYER 2 BODY: every hero gets a shaded opaque body, never the stick figure.
    bool allBodies = true, bodiesSane = true;
    for (int h = 0; h <= 10; ++h) {
        std::vector<Vertex> b;
        PoseState hs = s;
        hs.hero = static_cast<uint8_t>(h);
        const size_t bn = appendHeroBody(b, t, lookOf(hs), viewer, 1.f);
        allBodies = allBodies && bn > 3000 && bn % 3 == 0 && bn <= bodyMaxVertices && b.size() == bn;
        for (const auto& v : b)
            bodiesSane = bodiesSane && std::isfinite(v.position.x) && std::isfinite(v.position.y) &&
                         std::isfinite(v.position.z) && length(v.position - t.feet) < 2.5f && v.alpha > .99f;
    }
    check(allBodies, "PLAYER 2 BODY: every hero 0-10 draws a body");
    check(bodiesSane, "PLAYER 2 BODY: finite, opaque, around the ghost");
    std::vector<Vertex> nb;
    check(appendHeroBody(nb, bad, lookOf(s), viewer, 1) == 0 && nb.empty(), "a broken pose: no body (the figure path)");
    PoseState sp = s;
    sp.hero = 0;
    const Look def = lookOf(sp);
    check(def.spider && !def.learned && def.primary.x > .3f && def.secondary.x < .1f,
          "Spider-Man without a LOOK: red and black");
    PoseState im = s;
    im.hero = 1;
    check(!lookOf(im).spider, "Iron Man: not the web pattern");
    check(bodyOn(bodySuitLook) && !bodyOn(bodyFigure) && bodyOn(7), "SUIT LOOK on, FIGURE off, junk the default (on)");
    const auto& rig = bodyRig();
    check(rig.skeleton.at[suit_wear::head].y > 1.5f && rig.skeleton.at[suit_wear::footL].y < .1f,
          "the body's rig: 1.8 m, standing");
    const auto& oi = combat38::optionInfo[combat38::mpPlayer2Body];
    check(std::string(oi.key) == "mp_player2_body" && oi.count == 2 && std::string(oi.values[0]) == "SUIT LOOK" &&
              std::string(oi.values[1]) == "FIGURE",
          "PLAYER 2 BODY: an option, SUIT LOOK (value 0, the default) or FIGURE");
}
} // namespace

int main() {
    testCrcAndHash();
    testRoundTrip();
    testVersioning();
    testFlightLean();
    testMalformed();
    testInterpolation();
    testDelayAndEcho();
    testPartner();
    testHandshake();
    testSessionLoopback();
    testSessionBuddy();
    testSocketFailure();
    testGhostDrawing();
    std::printf("mp tests: %d checks, %d failed\n", checks, failures);
    return failures ? 1 : 0;
}
