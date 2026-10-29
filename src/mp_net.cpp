// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
// Made by Agent Mulder. Version 0.0.73 MP SOLO TEST: the pure network layer (mp_net.hpp).
#include "spidy/mp_net.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace spidy::mp {
namespace {
constexpr float pi = 3.14159265f;

struct Writer {
    uint8_t* p;
    size_t n{}, cap;
    bool ok = true;
    void bytes(const void* d, size_t k) {
        if (n + k > cap) {
            ok = false;
            return;
        }
        std::memcpy(p + n, d, k);
        n += k;
    }
    void u8(uint8_t v) { bytes(&v, 1); }
    void u16(uint16_t v) {
        const uint8_t b[2] = {static_cast<uint8_t>(v), static_cast<uint8_t>(v >> 8)};
        bytes(b, 2);
    }
    void u32(uint32_t v) {
        const uint8_t b[4] = {static_cast<uint8_t>(v), static_cast<uint8_t>(v >> 8), static_cast<uint8_t>(v >> 16),
                              static_cast<uint8_t>(v >> 24)};
        bytes(b, 4);
    }
    void f32(float f) {
        uint32_t v;
        std::memcpy(&v, &f, 4);
        u32(v);
    }
    void vec(Vec3 v) { f32(v.x), f32(v.y), f32(v.z); }
    void quat(Quat q) { f32(q.x), f32(q.y), f32(q.z), f32(q.w); }
};
struct Reader {
    const uint8_t* p;
    size_t n{}, size;
    uint8_t u8() { return n < size ? p[n++] : 0; }
    uint16_t u16() {
        const uint16_t a = u8(), b = u8();
        return static_cast<uint16_t>(a | (b << 8));
    }
    uint32_t u32() {
        const uint32_t a = u8(), b = u8(), c = u8(), d = u8();
        return a | (b << 8) | (c << 16) | (d << 24);
    }
    float f32() {
        const uint32_t v = u32();
        float f;
        std::memcpy(&f, &v, 4);
        return f;
    }
    Vec3 vec() {
        Vec3 v;
        v.x = f32(), v.y = f32(), v.z = f32();
        return v;
    }
    Quat quat() {
        Quat q;
        q.x = f32(), q.y = f32(), q.z = f32(), q.w = f32();
        return q;
    }
};
size_t payloadFor(uint8_t type) {
    switch (type) {
    case pose: return poseBytes;
    case hello:
    case welcome: return helloBytes;
    case ping: return pingBytes;
    case pong: return pongBytes;
    case bye: return 0;
    default: return SIZE_MAX;
    }
}
bool within(Vec3 v, float most) {
    return finite(v) && std::abs(v.x) <= most && std::abs(v.y) <= most && std::abs(v.z) <= most;
}
bool unitish(Quat& q) {
    if (!std::isfinite(q.x) || !std::isfinite(q.y) || !std::isfinite(q.z) || !std::isfinite(q.w))
        return false;
    const float n2 = q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w;
    if (!(n2 > .25f && n2 < 2.25f))
        return false;
    const float k = 1.f / std::sqrt(n2);
    q = {q.x * k, q.y * k, q.z * k, q.w * k};
    return true;
}
Quat nlerp(Quat a, Quat b, float t) {
    if (a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w < 0)
        b = {-b.x, -b.y, -b.z, -b.w};
    Quat q{a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t};
    const float n = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
    return n > 1e-6f ? Quat{q.x / n, q.y / n, q.z / n, q.w / n} : a;
}
Vec3 lerp(Vec3 a, Vec3 b, float t) { return a + (b - a) * t; }
Vec3 mirrorX(Vec3 v) { return {-v.x, v.y, v.z}; }
} // namespace

const char* errorName(Error e) {
    switch (e) {
    case Error::none: return "ok";
    case Error::tooShort: return "too short";
    case Error::tooLong: return "too long";
    case Error::badMagic: return "not a HERO VR packet";
    case Error::badVersion: return "another protocol version";
    case Error::badType: return "unknown type";
    case Error::badLength: return "wrong length";
    case Error::badChecksum: return "bad checksum";
    case Error::badValue: return "bad value";
    }
    return "?";
}

uint32_t crc32(const uint8_t* data, size_t size) {
    static const auto table = [] {
        std::array<uint32_t, 256> t{};
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t c = i;
            for (int k = 0; k < 8; ++k)
                c = c & 1 ? 0xedb88320u ^ (c >> 1) : c >> 1;
            t[i] = c;
        }
        return t;
    }();
    uint32_t c = 0xffffffffu;
    for (size_t i = 0; i < size; ++i)
        c = table[(c ^ data[i]) & 0xff] ^ (c >> 8);
    return c ^ 0xffffffffu;
}
uint32_t fnv1a(const char* text) {
    uint32_t h = 2166136261u;
    for (; text && *text; ++text)
        h = (h ^ static_cast<uint8_t>(*text)) * 16777619u;
    return h;
}
std::string cleanName(const char* raw) {
    std::string s;
    for (size_t i = 0; raw && raw[i] && s.size() < 16; ++i) {
        char c = raw[i];
        if (c >= 'a' && c <= 'z')
            c = static_cast<char>(c - 'a' + 'A');
        s += c >= 32 && c < 127 ? c : ' ';
    }
    return s;
}

size_t encode(const Packet& p, uint8_t* out, size_t capacity) {
    const size_t body = payloadFor(p.type);
    if (body == SIZE_MAX || p.role >= roleEnd || !out || capacity < overheadBytes + body)
        return 0;
    Writer w{out, 0, capacity};
    w.u32(magic);
    w.u8(version);
    w.u8(p.type);
    w.u8(p.role);
    w.u8(0);
    w.u32(p.seq);
    w.u32(p.sentMs);
    w.u16(p.echoHoldMs);
    w.u16(static_cast<uint16_t>(body));
    switch (p.type) {
    case pose: {
        const PoseState& s = p.state;
        w.u8(s.hero), w.u8(s.suit), w.u8(static_cast<uint8_t>(s.actions & actionMask)), w.u8(0);
        w.u32(s.suitHash);
        w.vec(s.root), w.f32(s.yaw);
        w.vec(s.head.position), w.quat(s.head.orientation);
        for (const Pose& h : s.hands)
            w.vec(h.position), w.quat(h.orientation);
        for (float c : s.curl)
            w.u8(static_cast<uint8_t>(std::lround(std::clamp(std::isfinite(c) ? c : 0.f, 0.f, 1.f) * 255.f)));
        const float eye = std::isfinite(s.standingEye) ? std::clamp(s.standingEye, 0.f, 3.f) : 0.f;
        w.u16(static_cast<uint16_t>(std::lround(eye * 1000.f)));
        w.vec(safeLeanUp(s.leanUp)); // 0.0.74 FLIGHT POSE
        break;
    }
    case hello:
    case welcome: {
        char name[16]{};
        const std::string n = cleanName(p.name);
        std::memcpy(name, n.data(), std::min<size_t>(n.size(), 16));
        w.bytes(name, 16);
        w.u8(p.protocol), w.u8(0), w.u8(0), w.u8(0);
        break;
    }
    case ping: w.u32(p.token); break;
    case pong: w.u32(p.token), w.u32(p.pingSentMs); break;
    default: break;
    }
    w.u32(crc32(out, w.n));
    return w.ok ? w.n : 0;
}
std::vector<uint8_t> encode(const Packet& p) {
    std::vector<uint8_t> b(maxPacket);
    b.resize(encode(p, b.data(), b.size()));
    return b;
}

Error decode(const uint8_t* data, size_t size, Packet& out) {
    if (!data || size < overheadBytes)
        return Error::tooShort;
    if (size > maxPacket)
        return Error::tooLong;
    Reader r{data, 0, size};
    if (r.u32() != magic)
        return Error::badMagic;
    if (r.u8() != version)
        return Error::badVersion;
    out = Packet{};
    out.type = r.u8();
    out.role = r.u8();
    r.u8();
    out.seq = r.u32();
    out.sentMs = r.u32();
    out.echoHoldMs = r.u16();
    const size_t body = r.u16();
    const size_t full = payloadFor(out.type);
    // 0.0.74: a 0.0.73 pose (112 bytes, no lean) is still a pose - read upright.
    const size_t want = out.type == pose ? poseBytesV1 : full;
    if (want == SIZE_MAX)
        return Error::badType;
    // The same version may grow a payload (extra bytes ignored), never shrink it.
    if (body < want || headerBytes + body + crcBytes != size)
        return Error::badLength;
    Reader c{data + headerBytes + body, 0, crcBytes};
    if (c.u32() != crc32(data, headerBytes + body))
        return Error::badChecksum;
    if (out.role >= roleEnd)
        return Error::badValue;
    switch (out.type) {
    case pose: {
        PoseState& s = out.state;
        s.hero = r.u8(), s.suit = r.u8(), s.actions = static_cast<uint8_t>(r.u8() & actionMask);
        r.u8();
        s.suitHash = r.u32();
        s.root = r.vec(), s.yaw = r.f32();
        s.head.position = r.vec(), s.head.orientation = r.quat();
        for (Pose& h : s.hands)
            h.position = r.vec(), h.orientation = r.quat();
        for (float& k : s.curl)
            k = static_cast<float>(r.u8()) / 255.f;
        s.standingEye = static_cast<float>(r.u16()) / 1000.f;
        s.leanUp = body >= poseBytes ? safeLeanUp(r.vec()) : Vec3{0, 1, 0}; // 0.0.74 FLIGHT POSE
        if (s.hero >= 32 || !within(s.root, 1e5f) || !std::isfinite(s.yaw) || std::abs(s.yaw) > 100.f ||
            !within(s.head.position, 50.f) || !unitish(s.head.orientation) || s.standingEye > 3.f)
            return Error::badValue;
        for (Pose& h : s.hands)
            if (!within(h.position, 50.f) || !unitish(h.orientation))
                return Error::badValue;
        s.yaw = wrapAngle(s.yaw);
        break;
    }
    case hello:
    case welcome: {
        for (int i = 0; i < 16; ++i)
            out.name[i] = static_cast<char>(r.u8());
        out.name[16] = 0;
        out.protocol = r.u8();
        for (int i = 0; i < 16 && out.name[i]; ++i)
            if (static_cast<uint8_t>(out.name[i]) < 32 || static_cast<uint8_t>(out.name[i]) > 126)
                return Error::badValue;
        break;
    }
    case ping: out.token = r.u32(); break;
    case pong: out.token = r.u32(), out.pingSentMs = r.u32(); break;
    default: break;
    }
    return Error::none;
}

float wrapAngle(float a) {
    if (!std::isfinite(a))
        return 0;
    if (a > -pi && a <= pi)
        return a; // already there: exact (a packet's yaw round trips bit for bit)
    a = std::fmod(a + pi, 2 * pi);
    if (a <= 0)
        a += 2 * pi;
    return a - pi;
}
float yawOf(Vec3 f) {
    if (!finite(f) || std::abs(f.x) + std::abs(f.z) < 1e-5f)
        return 0;
    return std::atan2(-f.x, -f.z);
}

Vec3 safeLeanUp(Vec3 up) {
    if (!finite(up))
        return {0, 1, 0};
    const float n = length(up);
    if (!(n > .5f) || !(n < 2.5f))
        return {0, 1, 0};
    up = up * (1.f / n);
    if (!(up.y > -.18f)) // more than ~100 degrees off upright: not a flight lean
        return {0, 1, 0};
    return up;
}

PoseState blend(const PoseState& a, const PoseState& b, float t) {
    t = std::isfinite(t) ? std::clamp(t, 0.f, 1.f) : 1.f;
    PoseState s = b;
    s.root = lerp(a.root, b.root, t);
    s.yaw = wrapAngle(a.yaw + wrapAngle(b.yaw - a.yaw) * t);
    s.head.position = lerp(a.head.position, b.head.position, t);
    s.head.orientation = nlerp(a.head.orientation, b.head.orientation, t);
    for (int i = 0; i < 2; ++i) {
        s.hands[i].position = lerp(a.hands[i].position, b.hands[i].position, t);
        s.hands[i].orientation = nlerp(a.hands[i].orientation, b.hands[i].orientation, t);
        s.curl[i] = a.curl[i] + (b.curl[i] - a.curl[i]) * t;
    }
    s.standingEye = a.standingEye + (b.standingEye - a.standingEye) * t;
    s.leanUp = safeLeanUp(lerp(safeLeanUp(a.leanUp), safeLeanUp(b.leanUp), t));
    return s;
}

void Interpolator::reset() {
    buf_.clear();
    have_ = false;
    lastSeq_ = lastMs_ = 0;
    unwrapped_ = offset_ = lastArrival_ = 0;
}
bool Interpolator::push(uint32_t seq, uint32_t senderMs, double now, const PoseState& s) {
    if (have_ && !newer(seq, lastSeq_)) {
        // Far behind, or behind after a silence: the sender started again.
        if (static_cast<int32_t>(lastSeq_ - seq) > 1000 || now - lastArrival_ > 1.0)
            reset();
        else {
            ++dropped_;
            return false;
        }
    }
    double t;
    if (!have_)
        t = static_cast<double>(senderMs) / 1000.0;
    else
        t = unwrapped_ + static_cast<double>(static_cast<int32_t>(senderMs - lastMs_)) / 1000.0;
    if (have_ && t <= unwrapped_) { // a newer number with an older clock: not to be trusted
        ++dropped_;
        return false;
    }
    const double seen = now - t;
    offset_ = have_ ? std::min(offset_, seen) : seen;
    have_ = true;
    lastSeq_ = seq, lastMs_ = senderMs, unwrapped_ = t, lastArrival_ = now;
    buf_.push_back({t, s});
    while (buf_.size() > keep)
        buf_.pop_front();
    ++accepted_;
    return true;
}
Sampled Interpolator::sample(double now) const {
    Sampled out;
    if (buf_.empty())
        return out;
    out.age = static_cast<float>(now - lastArrival_);
    if (!(out.age < hideAfter))
        return out;
    out.visible = true;
    out.alpha = out.age <= holdAfter ? 1.f : std::clamp(1.f - (out.age - holdAfter) / (hideAfter - holdAfter), 0.f, 1.f);
    const double t = now - offset_ - delay;
    if (t <= buf_.front().senderT) {
        out.state = buf_.front().s;
        return out;
    }
    if (t >= buf_.back().senderT) {
        out.state = buf_.back().s;
        return out;
    }
    for (size_t i = 1; i < buf_.size(); ++i)
        if (buf_[i].senderT >= t) {
            const auto& a = buf_[i - 1];
            const auto& b = buf_[i];
            const double span = b.senderT - a.senderT;
            out.state = blend(a.s, b.s, span > 1e-9 ? static_cast<float>((t - a.senderT) / span) : 1.f);
            return out;
        }
    out.state = buf_.back().s;
    return out;
}

bool DelayLine::push(double now, const uint8_t* data, size_t size) {
    if (!data || !size || size > maxPacket || q_.size() >= most)
        return false;
    q_.push_back({now, std::vector<uint8_t>(data, data + size)});
    return true;
}
bool DelayLine::pop(double now, std::vector<uint8_t>& out, double& heldFor) {
    if (q_.empty() || now - q_.front().at < hold)
        return false;
    heldFor = now - q_.front().at;
    out = std::move(q_.front().bytes);
    q_.pop_front();
    return true;
}
bool echoReply(const uint8_t* data, size_t size, uint16_t heldMs, std::vector<uint8_t>& out) {
    Packet p;
    if (decode(data, size, p) != Error::none || p.type != pose)
        return false;
    p.role = roleEcho;
    p.echoHoldMs = heldMs;
    out = encode(p);
    return !out.empty();
}

Ghost partner(const PoseState& s, Vec3 feet, float yaw, bool mirror, bool useRoot) {
    Ghost g;
    float body = yaw;
    g.feet = feet;
    if (useRoot) {
        const Vec3 r{s.root.x, 0, s.root.z};
        if (within(r, 50.f))
            g.feet = feet + Quat::yaw(yaw).rotate(r);
        body = wrapAngle(yaw + s.yaw);
    }
    g.bodyYaw = body;
    const Quat R = Quat::yaw(body);
    const auto place = [&](Vec3 v) { return g.feet + R.rotate(mirror ? mirrorX(v) : v); };
    const auto turn = [&](Vec3 d) { return normalized(R.rotate(mirror ? mirrorX(d) : d)); };
    g.eye = place(s.head.position);
    g.eyeForward = turn(s.head.orientation.rotate({0, 0, -1}));
    g.eyeUp = turn(s.head.orientation.rotate({0, 1, 0}));
    for (int i = 0; i < 2; ++i) {
        const int from = mirror ? 1 - i : i;
        const Pose& h = s.hands[from];
        g.hand[i] = (s.actions & (from == 0 ? handLeft : handRight)) != 0;
        g.grip[i] = place(h.position);
        g.gripForward[i] = turn(h.orientation.rotate({0, 0, -1}));
        g.gripUp[i] = turn(h.orientation.rotate({0, 1, 0}));
        // A reflection turns a right-handed frame left-handed: +X flipped makes it a hand's frame again
        // (OpenXR: +X right on both hands).
        const Vec3 x = turn(h.orientation.rotate({1, 0, 0}));
        g.gripX[i] = mirror ? -x : x;
        g.curl[i] = s.curl[from];
    }
    g.standingEye = s.standingEye > .8f && s.standingEye < 2.4f ? s.standingEye : 1.65f;
    g.up = turn(safeLeanUp(s.leanUp)); // 0.0.74 FLIGHT POSE: the lean turns (and mirrors) with the body
    return g;
}

float AnchorFollow::update(float headYaw, float dt) {
    headYaw = wrapAngle(headYaw);
    if (!placed || !std::isfinite(yaw)) {
        placed = true;
        yaw = headYaw;
        turning_ = false;
        return yaw;
    }
    const float d = wrapAngle(headYaw - yaw);
    if (std::abs(d) > slack)
        turning_ = true;
    if (turning_) {
        const float k = std::clamp((std::isfinite(dt) ? dt : 0.f) * ease, 0.f, 1.f);
        yaw = wrapAngle(yaw + d * k);
        if (std::abs(wrapAngle(headYaw - yaw)) < .09f)
            turning_ = false;
    }
    return yaw;
}

PoseState fromLocal(const LocalPose& l, uint8_t hero, uint8_t suit, uint32_t suitHash, uint8_t actions) {
    PoseState s;
    s.hero = hero;
    s.suit = suit;
    s.suitHash = suitHash;
    s.root = finite(l.worldFeet) ? l.worldFeet : Vec3{};
    s.yaw = yawOf(l.eye.orientation.rotate({0, 0, -1}));
    const Quat back = Quat::yaw(-s.yaw);
    s.head = {back.rotate(l.eye.position), back * l.eye.orientation};
    uint8_t a = static_cast<uint8_t>(actions & (flying | swinging | grapple | firing));
    for (int i = 0; i < 2; ++i) {
        s.hands[i] = {back.rotate(l.grip[i].position), back * l.grip[i].orientation};
        s.curl[i] = l.curl[i];
        if (l.hand[i])
            a |= i == 0 ? handLeft : handRight;
    }
    s.actions = a;
    s.standingEye = l.standingEye;
    s.leanUp = safeLeanUp(back.rotate(safeLeanUp(l.leanUp))); // 0.0.74 FLIGHT POSE
    return s;
}

BuddyLink::Step BuddyLink::onPacket(const Packet& p, Peer from, double now) {
    if (p.type == hello) {
        if (p.role != roleBuddy || p.protocol != version)
            return Step::ignore;
        if (!connected(now) || !(from == peer_)) {
            connected_ = true;
            peer_ = from;
        }
        name_ = cleanName(p.name);
        lastHeard_ = now;
        ++hellos_;
        return Step::welcome;
    }
    if (!connected_ || !(from == peer_))
        return Step::ignore;
    lastHeard_ = now;
    switch (p.type) {
    case pose: return p.role == roleBuddy ? Step::pose : Step::ignore;
    case ping: return Step::ping;
    case pong: return Step::pong;
    case bye: connected_ = false; return Step::bye;
    default: return Step::ignore;
    }
}
} // namespace spidy::mp
