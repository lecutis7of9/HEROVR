// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
// Made by Agent Mulder. Version 0.0.73 MP SOLO TEST: the packet, its checks, interpolation, the loopback
// delay, the partner placement and the buddy handshake - pure parts first, then the real session on
// 127.0.0.1 (a loopback round trip and a handshake from a raw socket).
#include "spidy/combat38.hpp"
#include "spidy/mesh_model.hpp"
#include "spidy/mp_body.hpp"
#include "spidy/mp_ghost.hpp"
#include "spidy/mp_net.hpp"
#include "spidy/mp_prop_look.hpp"
#include "spidy/mp_props.hpp"
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

// 0.0.74 MP JOIN: another game's HELLO, the WELCOME of the friend called, his poses; MP-PEER.txt; his facing.
void testJoinLink() {
    const Peer me{loopbackAddr, 39470}, pal{0x0a000002u, 39470}, palMoved{0x0a000002u, 51111}, stranger{0x0a000009u, 39470};
    Packet h;
    h.type = hello;
    h.role = roleGame;
    std::snprintf(h.name, sizeof h.name, "peter");
    Packet w = h;
    w.type = welcome;
    Packet gp;
    gp.type = pose;
    gp.role = roleGame;
    Packet bp = gp;
    bp.role = roleBuddy;
    {
        BuddyLink l;
        l.self(me);
        check(l.onPacket(h, pal, 0) == BuddyLink::Step::welcome && l.connected(0) && l.peerIsGame(),
              "JOIN: another game's HELLO is let in (WELCOME back), marked a game");
        check(l.name() == "PETER", "JOIN: his name");
        check(l.onPacket(gp, pal, .1) == BuddyLink::Step::pose, "JOIN: his game's poses count");
        check(l.onPacket(bp, pal, .2) == BuddyLink::Step::ignore, "JOIN: a buddy-marked pose from a game does not");
        check(l.onPacket(h, me, .3) == BuddyLink::Step::ignore && l.peer() == pal, "JOIN: our own HELLO come back is not let in");
    }
    {
        BuddyLink l;
        l.self(me);
        check(l.onPacket(w, pal, 0) == BuddyLink::Step::ignore && !l.connected(0), "JOIN: a WELCOME nobody asked for: ignored");
        l.dial(pal);
        check(l.onPacket(w, stranger, 0) == BuddyLink::Step::ignore && !l.connected(0), "JOIN: a stranger's WELCOME: ignored");
        Packet wb = w;
        wb.role = roleBuddy;
        check(l.onPacket(wb, pal, 0) == BuddyLink::Step::ignore, "JOIN: a WELCOME not from a game: ignored");
        Packet wo = w;
        wo.protocol = 7;
        check(l.onPacket(wo, pal, 0) == BuddyLink::Step::ignore, "JOIN: a WELCOME of another protocol: ignored");
        check(l.onPacket(w, palMoved, 1) == BuddyLink::Step::joined && l.connected(1) && l.peerIsGame() && l.peer() == palMoved,
              "JOIN: the called friend's WELCOME (his router moved the port) joins us to that port");
        check(l.onPacket(w, palMoved, 1.5) == BuddyLink::Step::ignore && l.connected(1.5), "JOIN: a repeated WELCOME: still joined");
        check(l.onPacket(gp, palMoved, 2) == BuddyLink::Step::pose, "JOIN: then his poses count");
        check(!l.connected(5.1), "JOIN: silent 3 s: gone (calling again)");
        Packet bye;
        bye.type = mp::bye;
        bye.role = roleGame;
        l.onPacket(w, palMoved, 6);
        check(l.onPacket(bye, palMoved, 6.5) == BuddyLink::Step::bye && !l.connected(6.5), "JOIN: his BYE");
    }
    // MP-PEER.txt
    PeerSetting a = parsePeerSetting("203.0.113.7\n");
    check(a.ok && a.host == "203.0.113.7" && a.port == defaultPort && a.name.empty(), "PEER: an address alone, port 39470");
    a = parsePeerSetting("\xef\xbb\xbf# my friend\r\n\r\n  10.0.0.5:39471  \r\nname = Peter Parker\r\n");
    check(a.ok && a.host == "10.0.0.5" && a.port == 39471 && a.name == "PETER PARKER", "PEER: Notepad's file - mark, comment, port, name");
    a = parsePeerSetting("name=MJ\npeer=friend.example.net:40000 # his PC\n");
    check(a.ok && a.host == "friend.example.net" && a.port == 40000 && a.name == "MJ", "PEER: peer= and a host name");
    check(!parsePeerSetting("").ok && !parsePeerSetting("# nothing\nname=X\n").ok, "PEER: no address: not ok");
    check(!parsePeerSetting("1.2.3.4:0").ok && !parsePeerSetting("1.2.3.4:99999").ok && !parsePeerSetting("1.2.3.4:abc").ok &&
              !parsePeerSetting("bad host name").ok,
          "PEER: a bad port or host: not ok");
    a = parsePeerSetting("junk host\n10.1.1.1\n10.2.2.2\n");
    check(a.ok && a.host == "10.1.1.1", "PEER: the first good address");
    Peer r{};
    std::string why;
    check(resolvePeer("10.1.2.3", 39470, r, why) && r.addr == 0x0a010203u && r.port == 39470, "PEER: dotted address resolved");
    check(resolvePeer("localhost", 4000, r, why) && r.addr == loopbackAddr, "PEER: a host name resolved (localhost)");
    check(!resolvePeer("0.0.0.0", 4000, r, why) && !resolvePeer("", 4000, r, why) && !why.empty(), "PEER: nobody: refused, said why");
    // His facing: starts facing the player, then turns as he turns.
    PeerFacing f;
    check(near(f.bodyYaw(3.f, 1.f), 3.f), "FACING: first seen: facing the player");
    check(near(f.bodyYaw(3.f, 1.5f), wrapAngle(3.5f)), "FACING: he turns 0.5: so does his ghost");
    f.reset();
    check(near(f.bodyYaw(-1.f, -2.f), -1.f), "FACING: reset: facing the player again");
    check(nameplate(true, "PETER", 42, true) == "PETER  PING 42 MS" && nameplate(true, "", -1, true) == "PLAYER 2  PING --" &&
              nameplate(true, "X", 5, false) == nameplate(true, "X", 5),
          "JOIN: the friend's nameplate is his name");
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

// 0.0.74 MP JOIN: two real games on 127.0.0.1 - one calls (MP-PEER.txt), the other only listens; then both call.
void testSessionJoin() {
    for (int both = 0; both < 2; ++both) {
        Session a, b;
        SessionConfig ca, cb;
        ca.port = static_cast<uint16_t>(39590 + both * 10);
        cb.port = static_cast<uint16_t>(39600 + both * 10 + 5);
        ca.echo = cb.echo = false;
        ca.buddyTimeout = cb.buddyTimeout = 1.0;
        ca.name = "alice";
        cb.name = "bob";
        check(b.start(cb), "JOIN: the friend's game starts");
        for (int i = 0; i < 20 && !b.view().running; ++i)
            sleepMs(5);
        ca.peerHost = "127.0.0.1";
        ca.peerPort = b.view().port;
        if (both) {
            b.stop();
            cb.peerHost = "localhost";
            cb.peerPort = ca.port; // a takes its first free port (ca.port when it is free)
            check(b.start(cb), "JOIN: the friend's game starts calling too");
        }
        check(a.start(ca), "JOIN: the calling game starts");
        bool joined = false;
        const double t0 = a.now();
        while (a.now() - t0 < 3.0) {
            PoseState pa = sample(), pb = sample();
            pa.hero = 4, pb.hero = 6;
            a.publish(pa);
            b.publish(pb);
            const SessionView va = a.view(), vb = b.view();
            if (va.peerGame && vb.peerGame && va.visible && vb.visible && va.state.hero == 6 && vb.state.hero == 4 &&
                a.now() - t0 > .8) {
                joined = true;
                break;
            }
            sleepMs(10);
        }
        const SessionView va = a.view(), vb = b.view();
        std::printf("  join%s: a sees '%s' hero %u ping %d; b sees '%s' hero %u ping %d\n", both ? " (both calling)" : "",
                    va.buddyName.c_str(), va.state.hero, va.pingMs, vb.buddyName.c_str(), vb.state.hero, vb.pingMs);
        check(joined, both ? "JOIN: both calling: joined, each sees the other" : "JOIN: called: joined, each sees the other");
        check(va.buddyName == "BOB" && vb.buddyName == "ALICE", "JOIN: each named");
        check(va.buddy && !va.calling && va.peerText.find("127.0.0.1:") == 0, "JOIN: not calling once joined");
        bool saidJoined = false;
        for (const auto& l : a.takeLog())
            saidJoined = saidJoined || l.find("joined from") != std::string::npos;
        check(saidJoined, "JOIN: logged");
        // He quits: a drops him (BYE) and calls again.
        b.stop();
        for (int i = 0; i < 40; ++i)
            a.publish(sample()), sleepMs(5);
        const SessionView after = a.view();
        check(!after.buddy && after.calling, "JOIN: he left: dropped, calling again");
        a.stop();
    }
    // A friend nobody can find: logged, no crash, still listening.
    Session c;
    SessionConfig cc;
    cc.port = 39625;
    cc.echo = false;
    cc.peerHost = "0.0.0.0"; // (a .invalid name resolves on some networks with DNS wildcards)
    c.start(cc);
    sleepMs(300);
    bool said = false;
    for (const auto& l : c.takeLog())
        said = said || l.find("MP-PEER.txt") != std::string::npos;
    check(said && c.view().running && !c.view().calling, "JOIN: a friend address nobody has: said so, still up");
    c.stop();
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

// 0.0.74 MP PROPS: the 'P' tail - its wire, its limits, the old builds' tolerance and the props placed on the ghost.
Props sampleProps() {
    Props p;
    p.flags = propsFlagClawSteel | propsFlagBatonModel;
    PropItem h;
    h.id = idHammer, h.kind = propHammer, h.flags = propFlagFlying, h.variant = 2, h.spin = 2.5f;
    h.at = {.4f, 1.3f, -2.5f}, h.forward = normalized(Vec3{0, 1, -1}), h.up = {1, 0, 0};
    p.add(h);
    PropItem b;
    b.id = idHeld + 1, b.kind = hero007::lookBatarang, b.flags = propFlagInHand | propFlagSide, b.variant = 3, b.spin = -1.f;
    b.at = {.3f, 1.2f, -.4f}, b.forward = {0, 0, -1}, b.up = {0, 1, 0};
    p.add(b);
    return p;
}
void testPropsWire() {
    Props p = sampleProps();
    check(p.items == 2 && p.any(), "two props added");
    check(propsBytes(p) == propsHeaderBytes + 2 * propItemBytes, "props size: header + 24 a prop");
    Props none;
    check(!none.any() && propsBytes(none) == 0, "no props: no tail");
    // limits: unknown kinds, NaN and far-away props never go on the wire
    PropItem bad = p.item[0];
    bad.kind = 200;
    check(!p.add(bad), "an unknown kind is refused");
    bad = p.item[0];
    bad.at.x = NAN;
    check(!p.add(bad), "a NaN prop is refused");
    bad = p.item[0];
    bad.at = {400, 0, 0};
    check(!p.add(bad), "a prop past 327 m is refused");
    Props full;
    for (int i = 0; i < 12; ++i)
        full.add(p.item[0]);
    check(full.items == propItemsMost, "at most 8 props");
    // the pose packet carries it after LOOK, still version 1
    Packet pk;
    pk.type = pose;
    pk.state = sample();
    pk.state.props = p;
    const auto w = encode(pk);
    check(w.size() == overheadBytes + poseSentBytes + propsBytes(p), "pose + LOOK + the PROPS tail");
    check(w[4] == 1 && w[headerBytes + poseSentBytes] == 'P', "still version 1, the tail tagged 'P'");
    Packet q;
    check(decode(w.data(), w.size(), q) == Error::none, "a pose with props decodes");
    const Props& r = q.state.props;
    check(r.items == 2 && r.flags == p.flags, "props round trip");
    check(r.item[0].kind == propHammer && r.item[0].id == idHammer && r.item[0].variant == 2 && r.item[0].flags == propFlagFlying, "hammer ids");
    check(near(r.item[0].at, p.item[0].at, .006f) && near(r.item[0].forward, p.item[0].forward, .001f) && near(r.item[0].spin, 2.5f, .001f),
          "hammer place within a centimetre");
    check(near(r.item[1].spin, -1.f, .001f) && r.item[1].variant == 3, "batarang style and spin");
    Props qz = quantized(p);
    check(qz.items == 2 && near(qz.item[0].at, r.item[0].at, 1e-6f), "quantized() is what arrives");
    // the biggest tail still fits a packet
    Props most;
    for (size_t i = 0; i < propItemsMost; ++i)
        most.add(p.item[0]);
    PropLine ln;
    ln.kind = lineHeat, ln.fade = 1, ln.from = {0, 1, 0}, ln.to = {0, 1, -20};
    for (size_t i = 0; i < propLinesMost; ++i)
        most.add(ln);
    PropFx fx;
    fx.kind = fxImpact, fx.at = {0, 1, -3}, fx.way = {0, 0, 1};
    for (size_t i = 0; i < propFxMost; ++i)
        most.add(fx);
    check(propsBytes(most) == propsMostBytes, "the fullest tail is propsMostBytes");
    pk.state.props = most;
    const auto big = encode(pk);
    check(!big.empty() && big.size() <= maxPacket, "the fullest pose fits 512 bytes");
    check(decode(big.data(), big.size(), q) == Error::none && q.state.props.items == propItemsMost &&
              q.state.props.lines == propLinesMost && q.state.props.fx == propFxMost, "the fullest tail round trips");
    // without a tail (every 0.0.73 packet) or with one this build cannot read, the pose stands and has no props
    // (a 0.0.73 build itself reads its 136 bytes and ignores the rest: decode takes body >= 112)
    Packet np = pk;
    np.state.props = {};
    const auto cut = encode(np);
    check(decode(cut.data(), cut.size(), q) == Error::none && !q.state.props.any(), "no tail: no props (the 0.0.73 packet)");
    std::vector<uint8_t> junk = w;
    junk[headerBytes + poseSentBytes + 1] = 99; // a props version from the future
    junk = resealed(junk);
    check(decode(junk.data(), junk.size(), q) == Error::none && !q.state.props.any(), "a tail it cannot read: the pose, no props");
    junk = w;
    junk[headerBytes + poseSentBytes] = 'Q'; // some other tail
    junk = resealed(junk);
    check(decode(junk.data(), junk.size(), q) == Error::none && !q.state.props.any() && near(q.state.root, pk.state.root, 0),
          "an unknown tail: ignored");
    // a raw decodeProps never reads past its buffer
    uint8_t raw[propsMostBytes];
    const size_t n = encodeProps(p, raw, sizeof raw);
    Props t;
    bool anyOk = false;
    for (size_t k = 0; k < n; ++k)
        anyOk |= decodeProps(raw, k, t);
    check(!anyOk && decodeProps(raw, n, t), "a short tail is refused, the whole one read");
    check(encodeProps(p, raw, 10) == 0, "no room: nothing written");
    // the echo (the buddy's loopback) keeps the props
    std::vector<uint8_t> echoed;
    check(echoReply(w.data(), w.size(), 50, echoed) && decode(echoed.data(), echoed.size(), q) == Error::none &&
              q.state.props.items == 2, "the echo keeps the props");
}
void testPropsOnGhost() {
    // the sender: tracking frame turned 90 degrees, his root yawed 30 degrees, a batarang held a metre in front
    const Quat turn = Quat::yaw(1.5707963f);
    const Vec3 anchor{5, 0, 2};
    const float yaw = .5235988f;
    const SendFrame sf = sendFrame(anchor, turn, yaw);
    hero007::Draw d;
    d.any = true;
    // in his root frame the batarang is at (0.3, 1.2, -1) pointing ahead (-z): in tracking = anchor + turn*yaw*(local)
    const Vec3 local{.3f, 1.2f, -1.f};
    const Quat toTracking = turn * Quat::yaw(yaw);
    d.held[1].look = hero007::lookBatarang;
    d.held[1].at = anchor + toTracking.rotate(local);
    d.held[1].forward = toTracking.rotate(Vec3{0, 0, -1});
    d.held[1].up = toTracking.rotate(Vec3{0, 1, 0});
    d.held[1].side = 1;
    d.worn[0].look = hero007::lookPistol;
    d.worn[0].holster = 2;
    d.worn[0].at = anchor + toTracking.rotate(Vec3{-.2f, .9f, 0});
    d.worn[0].forward = toTracking.rotate(Vec3{0, -1, 0});
    d.worn[0].up = toTracking.rotate(Vec3{1, 0, 0});
    hero072::Look l;
    l.any = l.hammer = true;
    l.hammerSheathed = true;
    l.hammerAt = anchor + toTracking.rotate(Vec3{0, 1.4f, .2f});
    l.hammerForward = toTracking.rotate(Vec3{0, 1, 0});
    l.hammerUp = toTracking.rotate(Vec3{0, 0, 1});
    l.hammerChoice = 1;
    PropSource src;
    src.h007 = &d;
    src.h072 = &l;
    src.batarangStyle = 2;
    src.batons.on[0] = true;
    src.batons.held[0] = true;
    src.batons.grip[0] = anchor + toTracking.rotate(Vec3{-.3f, 1.f, -.3f});
    src.batons.tip[0] = anchor + toTracking.rotate(Vec3{-.3f, 1.f, -.87f});
    const Props p = captureProps(src, sf);
    check(p.items == 4, "held batarang, sheathed hammer, a baton, a worn pistol");
    check(p.item[0].kind == hero007::lookBatarang && near(p.item[0].at, local, 1e-4f) && near(p.item[0].forward, {0, 0, -1}, 1e-4f),
          "captured in his root frame (travel and turns gone)");
    check(p.item[0].variant == 2 && (p.item[0].flags & propFlagSide) && (p.item[0].flags & propFlagInHand), "batarang style, hand");
    check(p.item[1].kind == propHammer && (p.item[1].flags & propFlagSheathed) && p.item[1].variant == 1, "the hammer on his back");
    check(p.item[2].kind == propBaton && near(p.item[2].spin, .57f, 1e-3f) && (p.item[2].flags & propFlagInHand), "the baton's span");
    check(p.item[3].kind == hero007::lookPistol && (p.item[3].flags & propFlagHolster) == 2, "the worn pistol's holster");
    // nothing held: no tail at all
    PropSource empty;
    check(!captureProps(empty, sf).any(), "no props: nothing sent");
    // the receiver: the ghost 3 m ahead facing back at the viewer, overlay origin and turn
    Ghost g;
    g.feet = {0, 0, -3};
    g.bodyYaw = 3.1415927f;
    const Vec3 origin{1, 0, 1};
    const Quat vt = Quat::yaw(-.4f);
    const GhostFrame gf = ghostFrame(g, false, origin, vt);
    const RemoteProps r = rebuildProps(quantized(p), gf);
    const Vec3 want = origin + vt.rotate(g.feet + Quat::yaw(g.bodyYaw).rotate(local));
    check(r.h007.any && r.h007.held[1].look == hero007::lookBatarang && near(r.h007.held[1].at, want, .01f),
          "the batarang in PLAYER 2's right hand");
    check(near(r.h007.held[1].forward, vt.rotate(Quat::yaw(g.bodyYaw).rotate(Vec3{0, 0, -1})), .002f), "pointing his way");
    check(r.batarangStyle == 2 && r.hammerChoice == 1 && r.hammerSheathed && r.h072.hammer && !r.h072.hammerSheathed && !r.h072.hammerInHand,
          "style, hammer choice, drawn on his back");
    check(r.batons.on[0] && r.batons.held[0] && near(length(r.batons.tip[0] - r.batons.grip[0]), .57f, .01f), "his baton");
    check(r.h007.worn[0].look == hero007::lookPistol && r.h007.worn[0].holster == 2 && r.h007.modelWanted == 0,
          "the worn pistol, built-in when the model is missing");
    // the mirror: x flips and the hands swap
    const RemoteProps m = rebuildProps(quantized(p), ghostFrame(g, true, origin, vt));
    const Vec3 wantM = origin + vt.rotate(g.feet + Quat::yaw(g.bodyYaw).rotate(Vec3{-local.x, local.y, local.z}));
    check(m.h007.held[0].look == hero007::lookBatarang && m.h007.held[1].look == 0 && near(m.h007.held[0].at, wantM, .01f) &&
              m.h007.held[0].side == 0, "mirrored: in the other hand, on the other side");
    check(m.batons.on[1] && !m.batons.on[0], "mirrored: the baton in the other hand");
    // a flying hammer goes to the hero072 look
    Props fly = p;
    fly.item[1].flags = propFlagFlying;
    const RemoteProps rf = rebuildProps(fly, gf);
    check(rf.h072.any && rf.h072.hammer && !rf.h072.hammerInHand && !rf.hammerSheathed, "a flying hammer");
}

// 0.0.74 MP PROPS phase 2: thrown items by state (batarangs, the spinning shield, pellets, the hammer), blended.
void testPropsThrown() {
    const SendFrame sf = sendFrame({0, 0, 0}, Quat{}, 0.f);
    hero007::Draw d;
    d.any = true;
    d.flying[0].look = hero007::lookBatarang;
    d.flying[0].at = {1, 1.5f, -6};
    d.flying[0].spin = 3.f;
    d.flying[1].look = hero007::lookShieldSpin;
    d.flying[1].at = {-2, 1.2f, -9};
    d.flying[1].side = 1;
    d.pellet[1] = true;
    d.pelletAt[1] = {.5f, .4f, -3};
    PropSource src;
    src.h007 = &d;
    src.batarangStyle = 1;
    const Props p = captureProps(src, sf);
    check(p.items == 3, "two thrown, one pellet");
    check(p.item[0].id == idFlying && (p.item[0].flags & propFlagFlying) && p.item[0].variant == 1, "batarang in flight, styled");
    check(p.item[1].id == idFlying + 1 && p.item[1].kind == hero007::lookShieldSpin, "the shield in flight");
    check(p.item[2].id == idPellet + 1 && p.item[2].kind == propPellet && (p.item[2].flags & propFlagSide), "the pellet");
    Ghost g;
    g.feet = {0, 0, 0};
    g.bodyYaw = 0;
    const RemoteProps r = rebuildProps(quantized(p), ghostFrame(g, false, {10, 0, 0}, Quat{}));
    check(r.h007.flying[0].look == hero007::lookBatarang && near(r.h007.flying[0].at, {11, 1.5f, -6}, .01f), "flying on the ghost");
    check(r.h007.flying[1].look == hero007::lookShieldSpin && near(r.h007.flying[0].spin, 3.f, .001f), "shield, spin");
    check(r.h007.pellet[1] && !r.h007.pellet[0] && near(r.h007.pelletAt[1], {10.5f, .4f, -3}, .01f), "pellet in its hand");
    const RemoteProps m = rebuildProps(quantized(p), ghostFrame(g, true, {10, 0, 0}, Quat{}));
    check(m.h007.pellet[0] && near(m.h007.pelletAt[0], {9.5f, .4f, -3}, .01f), "mirrored pellet: other hand, x flipped");

    // blending: a hammer flying out, a batarang spinning across the +-pi seam
    Props a, b;
    PropItem h;
    h.id = idHammer;
    h.kind = propHammer;
    h.flags = propFlagFlying;
    h.at = {0, 1.5f, -2};
    h.spin = 3.f;
    a.add(h);
    h.at = {0, 1.5f, -4};
    h.spin = -3.f; // 3 -> -3 is .28 rad the short way, through pi
    b.add(h);
    PropItem bat;
    bat.id = idBaton;
    bat.kind = propBaton;
    bat.spin = .2f;
    a.add(bat);
    bat.spin = .6f;
    b.add(bat);
    PropLine ln;
    ln.kind = lineBolt;
    ln.from = {0, 1, 0};
    ln.to = {0, 1, -10};
    a.add(ln);
    ln.to = {0, 1, -12};
    b.add(ln);
    Props mid = blendProps(a, b, .5f);
    check(mid.items == 2 && near(mid.item[0].at, {0, 1.5f, -3}), "hammer half way");
    check(near(std::abs(mid.item[0].spin), 3.1415927f, .01f), "angle spin the short way (through pi)");
    check(near(mid.item[1].spin, .4f), "baton length lerped");
    check(mid.lines == 1 && near(mid.line[0].to, {0, 1, -11}), "line end blended");
    check(near(blendProps(a, b, 0.f).item[0].at, a.item[0].at) && near(blendProps(a, b, 1.f).item[0].at, b.item[0].at), "ends");
    Props caught = b;
    caught.item[0].flags = propFlagInHand;
    caught.item[0].at = {.3f, 1.2f, -.3f};
    check(near(blendProps(a, caught, .5f).item[0].at, caught.item[0].at), "caught: snaps to the hand, no slide");
    Props far = b;
    far.item[0].at = {0, 1.5f, -30};
    check(near(blendProps(a, far, .5f).item[0].at, far.item[0].at), "a new throw (far): snaps");
    check(blendProps(a, Props{}, .5f).items == 0 && blendProps(Props{}, b, .5f).items == 2, "appear / vanish at once");

    // through the Interpolator at 30 Hz: the hammer moves smoothly between packets
    Interpolator in;
    in.delay = .1f;
    for (uint32_t k = 0; k < 12; ++k) {
        PoseState s;
        PropItem it = h;
        it.at = {0, 1.5f, -static_cast<float>(k) * .5f};
        it.spin = 0;
        s.props.add(it);
        in.push(k + 1, 1000 + k * 33, 1.0 + k * .033, s);
    }
    const Sampled smp = in.sample(1.0 + 11 * .033 - .0165);
    check(smp.visible && smp.state.props.items == 1, "props through the Interpolator");
    const float z = smp.state.props.item[0].at.z;
    check(std::abs(z * 2.f - std::round(z * 2.f)) > .05f, "between two packets (not a hop)");
}

// 0.0.74 MP PROPS phase 3: beams, lines, GREEN LANTERN / IRON MAN items and the effects ring
void testPropsEffects() {
    const SendFrame sf = sendFrame({0, 0, 0}, Quat{}, 0.f);
    Ghost g;
    g.feet = {0, 0, 0};
    g.bodyYaw = 0;
    const GhostFrame gw = ghostFrame(g, false, {10, 0, 0}, Quat{}), gm = ghostFrame(g, true, {10, 0, 0}, Quat{});
    hero007::Draw d;
    d.any = true;
    d.beamFrom[0] = {-.032f, 1.7f, 0};
    d.beamFrom[1] = {.032f, 1.7f, 0};
    d.beamTo = {0, 1, -8};
    d.beamFade = .8f;
    d.grapple[1] = true;
    d.grappleFrom[1] = {.3f, 1.2f, -.3f};
    d.grappleTo[1] = {2, 9, -12};
    d.grappleOut[1] = 1;
    hero072::Look l;
    l.any = true;
    l.ring[0] = true;
    l.ringAt[0] = {-.3f, 1.1f, -.2f};
    l.bubble = true;
    l.bubbleAt = {0, 1, 0};
    l.bubbleRadius = 1.6f;
    l.bolts = 3; // the third is past propLinesMost
    l.boltFrom[0] = {0, 1.5f, 0};
    l.boltTo[0] = {0, 1, -9};
    l.boltFrom[1] = {0, 1.5f, 0};
    l.boltTo[1] = {1, 1, -9};
    l.boltFrom[2] = {0, 1.5f, 0};
    l.boltTo[2] = {-1, 1, -9};
    ShotSource sh;
    sh.repulsor = true;
    sh.palmGlow[1] = .5f;
    sh.palmAt[1] = {.3f, 1.2f, -.4f};
    sh.palmWay[1] = {0, 0, -1};
    sh.fade[2] = 1;
    sh.from[2] = {.3f, 1.2f, -.4f};
    sh.at[2] = {.3f, 1.2f, -10.4f};
    PropSource src;
    src.h007 = &d;
    src.h072 = &l;
    src.shots = &sh;
    const Props p = captureProps(src, sf);
    check(p.items == 3 && p.item[0].kind == propRing && p.item[1].kind == propBubble && p.item[2].kind == propPalm,
          "ring, bubble, palm as items");
    check(p.lines == 5 && p.line[0].kind == lineHeat && p.line[1].kind == lineGrapple && p.line[2].kind == lineRepulsor &&
              p.line[3].kind == lineBolt && p.line[4].kind == lineBolt,
          "beam, grapple, shot, then bolts (cut at propLinesMost)");
    uint8_t buf[propsMostBytes];
    Props back;
    check(decodeProps(buf, encodeProps(p, buf, sizeof buf), back) && back.lines == 5 && back.items == 3, "lines through the wire");
    const RemoteProps r = rebuildProps(back, gw);
    check(r.h007.beamFade > .7f && !r.h007.beamFreeze && near(r.h007.beamFrom[0], {9.968f, 1.7f, 0}, .02f) &&
              near(r.h007.beamTo, {10, 1, -8}, .02f),
          "heat beam: two eyes, on the ghost");
    check(r.h007.grapple[0] && near(r.h007.grappleTo[0], {12, 9, -12}, .02f), "grapple line");
    check(r.h072.ring[0] && r.h072.bubble && near(r.h072.bubbleRadius, 1.6f, .01f) && r.h072.bolts == 2, "rings, bubble, two bolts");
    check(r.shots.repulsor && r.shots.n == 1 && !r.shots.web[0] && near(r.shots.at[0], {10.3f, 1.2f, -10.4f}, .02f) &&
              near(length(r.shots.at[0] - r.shots.tail[0]), repulsorTail, .02f),
          "repulsor shot with its tail");
    check(near(r.shots.palmGlow[1], .5f, .01f) && near(r.shots.palmAt[1], {10.3f, 1.2f, -.4f}, .02f), "palm glow");
    const RemoteProps rm = rebuildProps(back, gm);
    check(rm.h072.ring[1] && !rm.h072.ring[0] && rm.shots.palmGlow[0] > .4f, "mirrored: the other hand");

    // the effects ring: a flash and a hit go out once each, newest first, for fxSendFor
    hero007::Draw fd;
    fd.any = true;
    FxRing ring;
    ring.feed({&fd, nullptr, nullptr}, 100.0); // learns the ages
    fd.flashAge[1] = 0;
    fd.flashAt[1] = {.3f, 1.2f, -.6f};
    fd.flashWay[1] = {0, 0, -1};
    fd.impactAge[2] = .01f;
    fd.impactAt[2] = {0, 1, -20};
    ring.feed({&fd, nullptr, nullptr}, 100.01);
    fd.flashAge[1] = .02f;
    fd.impactAge[2] = .03f;
    ring.feed({&fd, nullptr, nullptr}, 100.03); // still the same ones: nothing new
    check(ring.sent() == 2, "two effects noticed once each");
    Props q;
    ring.write(q, sf, 100.05);
    check(q.fx == 2 && q.effect[0].seq == 2 && q.effect[0].kind == fxImpact && q.effect[1].kind == fxFlash && q.effect[1].param == 1,
          "newest first, the hand in param");
    Props late;
    ring.write(late, sf, 100.5);
    check(late.fx == 0, "gone from the packets after fxSendFor");
    // a new shot reuses the slot (age back to 0): noticed again
    fd.flashAge[1] = 0;
    ring.feed({&fd, nullptr, nullptr}, 100.6);
    check(ring.sent() == 3, "reused slot noticed");
    hero072::Look sl;
    sl.any = sl.shock = true;
    sl.shockAt = {0, 0, -2};
    sl.shockRadius = 3.f;
    sl.shockAge = 0;
    ring.feed({&fd, &sl, nullptr}, 100.61);
    check(ring.sent() == 4, "shock ring");

    // the inbox: taken once per seq (eight packets carry it), aged on the receiver's clock, placed on the ghost
    Props w;
    ring.write(w, sf, 100.62);
    check(w.fx == 2, "flash + shock in the packet");
    Props wq;
    check(decodeProps(buf, encodeProps(w, buf, sizeof buf), wq), "fx through the wire");
    FxInbox in;
    for (int k = 0; k < 8; ++k)
        in.take(wq, gw, 50.0 + k * .033);
    RemoteProps rr;
    in.apply(rr, 50.02);
    check(rr.h007.flashAge[1] < hero007::flashLife && near(rr.h007.flashAt[1], {10.3f, 1.2f, -.6f}, .02f), "flash on the ghost's hand");
    check(rr.h072.shock && near(rr.h072.shockRadius, 3.f, .05f) && rr.h072.shockAge < .1f && near(rr.h072.shockAt, {10, 0, -2}, .02f),
          "shock ring placed");
    RemoteProps later;
    in.apply(later, 51.0);
    check(!later.h072.shock && later.h007.flashAge[1] > 1e8f, "played out on our clock (no repeat from later packets)");
    check(in.live(51.0) && !in.live(80.0), "kept for fxKeepFor");
    FxInbox im;
    im.take(wq, gm, 50.0);
    RemoteProps rmm;
    im.apply(rmm, 50.01);
    check(rmm.h007.flashAge[0] < hero007::flashLife && near(rmm.h007.flashAt[0], {9.7f, 1.2f, -.6f}, .02f), "mirrored flash: other hand");
    // the whole tail still fits in a pose packet
    Props full = p;
    for (const PropFx& x : w.effect)
        full.add(x);
    check(encodeProps(full, buf, sizeof buf) <= propsMostBytes, "tail with fx fits");
}

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
    testJoinLink();
    testSessionLoopback();
    testSessionBuddy();
    testSessionJoin();
    testSocketFailure();
    testGhostDrawing();
    testPropsWire();
    testPropsOnGhost();
    testPropsThrown();
    testPropsEffects();
    std::printf("mp tests: %d checks, %d failed\n", checks, failures);
    return failures ? 1 : 0;
}
