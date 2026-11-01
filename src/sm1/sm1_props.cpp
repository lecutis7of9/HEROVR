// HERO VR 0.0.76 SM1 HERO PROPS: the pure part (sm1_props.hpp). No game address, no D3D, no OpenXR.
#include "spidy/sm1_props.hpp"

#include "spidy/baton_grip.hpp"
#include "spidy/hero_modes.hpp"
#include "spidy/thor_grip.hpp"

#include <algorithm>
#include <cmath>

namespace spidy::sm1::props {
namespace {
bool usable(const Pose& p) {
    const Quat& q = p.orientation;
    const float n = q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w;
    return finite(p.position) && std::isfinite(n) && std::abs(n - 1.f) <= .01f;
}
} // namespace

uint32_t slotOf(Model m, uint32_t hammerStyle) {
    switch (m) {
    case hammer: return mesh_model::hammerSlot(hammerStyle < mesh_model::hammers ? hammerStyle : 0);
    case shield: return mesh_model::slotShield;
    case clawsLeft: return mesh_model::slotClawsLeft;
    case clawsRight: return mesh_model::slotClawsRight;
    case batonLeft: return mesh_model::slotBatonLeft;
    case batonRight: return mesh_model::slotBatonRight;
    case pistol: return mesh_model::slotPistol;
    case batarang: return 1; // mesh_model style 1: EIRA's batarang (the SM2 build's default)
    default: return mesh_model::slots;
    }
}

std::vector<Model> modelsFor(uint32_t hero) {
    switch (hero) {
    case hero_modes::thor: return {hammer};
    case hero_modes::captainAmerica: return {shield};
    case hero_modes::wolverine: return {clawsLeft, clawsRight};
    case hero_modes::daredevil: return {batonLeft, batonRight};
    case hero_modes::punisher: return {pistol};
    case hero_modes::batman: return {batarang};
    default: return {};
    }
}
bool hasProps(uint32_t hero) { return !modelsFor(hero).empty(); }
Model thrownBy(uint32_t hero, int hand) {
    if (hand != 1)
        return none;
    return hero == hero_modes::thor ? hammer : hero == hero_modes::captainAmerica ? shield : hero == hero_modes::batman ? batarang : none;
}

Vec3 apply(const Place& p, Vec3 m) { return p.at + p.x * m.x + p.y * m.y + p.z * m.z; }

Place placed(Vec3 at, Vec3 forward, Vec3 up) {
    Place p;
    if (!finite(at) || !finite(forward) || !finite(up) || !(length(forward) > 1e-4f))
        return p;
    const Vec3 f = normalized(forward);
    Vec3 u = up - f * dot(up, f);
    if (!(length(u) > 1e-4f))
        return p;
    u = normalized(u);
    p.at = at;
    p.x = cross(f, u);
    p.y = u;
    p.z = f * -1.f;
    p.ok = true;
    return p;
}

Place hammerPlace(Vec3 at, Vec3 handle, Vec3 face, float spin) {
    // hero072_play's visual: along = the handle, face square to it; the spin turns both about their cross.
    if (!finite(handle) || !finite(face) || !(length(handle) > 1e-4f) || !std::isfinite(spin))
        return {};
    const Vec3 along = normalized(handle);
    Vec3 f = face - along * dot(face, along);
    if (!(length(f) > 1e-4f))
        return {};
    f = normalized(f);
    const float c = std::cos(spin), s = std::sin(spin);
    const Vec3 h = along * c + f * s, fc = f * c - along * s;
    return placed(at, fc, h); // up (+y) the handle's way, forward (-z) the face
}
Place hammerInHand(const Pose& grip, ControllerKind kind) {
    const thor_grip::Held h = thor_grip::held(grip, kind);
    return h.ok ? hammerPlace(h.at, h.handle, h.face, 0) : Place{};
}

Place shieldPlace(const captain::ShieldPose& sp, uint32_t side, Vec3 pivot, Vec3 low, Vec3 high, float spin) {
    if (!sp.ok || !finite(pivot) || !std::isfinite(spin))
        return {};
    const Place frame = placed(sp.at, sp.face, sp.up); // r = face x up, world = at + r x + up y - face z (captain::place)
    if (!frame.ok)
        return {};
    // Model point m -> c = forSide(m) (the right arm's: half a turn about the pivot); spinning: about the disc's middle.
    // forSide: (px - x, py - y, z) on side 1, identity on side 0 - affine: c = A m + b.
    Vec3 ax{1, 0, 0}, ay{0, 1, 0}, b{};
    if (side == 1)
        ax = {-1, 0, 0}, ay = {0, -1, 0}, b = {pivot.x, pivot.y, 0};
    if (spin != 0) {
        const Vec3 mid = captain::forSide((low + high) * .5f, side, pivot);
        const float c = std::cos(spin), s = std::sin(spin);
        // c' = mid + R (c - mid) in xy (z kept): R = [[c,-s],[s,c]]
        const auto rot = [&](Vec3 v) { return Vec3{v.x * c - v.y * s, v.x * s + v.y * c, v.z}; };
        ax = rot(ax), ay = rot(ay);
        b = mid + rot(b - mid);
    }
    Place p;
    p.at = apply(frame, b);
    p.x = frame.x * ax.x + frame.y * ax.y + frame.z * ax.z;
    p.y = frame.x * ay.x + frame.y * ay.y + frame.z * ay.z;
    p.z = frame.z;
    p.ok = finite(p.at);
    return p;
}
captain::ShieldPose shieldOnForearm(const Pose& grip, unsigned hand, ControllerKind kind) {
    const gun_grip::Held g = gun_grip::held(grip, hand, kind);
    if (!g.ok)
        return {};
    return captain::onForearm(g.at, g.forward, g.up, hand == 1 ? 1u : 0u);
}
Place shieldFlying(Vec3 middle, Vec3 way, uint32_t side, Vec3 pivot, Vec3 low, Vec3 high, float spin) {
    // hero007_play: the flying shield's face up, its up along the flight; placed so its disc's middle is `middle`.
    captain::ShieldPose sp;
    sp.face = {0, 1, 0};
    Vec3 u = finite(way) ? way - sp.face * dot(way, sp.face) : Vec3{};
    sp.up = length(u) > 1e-3f ? normalized(u) : Vec3{0, 0, -1};
    sp.at = {};
    sp.ok = true;
    Place p = shieldPlace(sp, side, pivot, low, high, spin);
    if (!p.ok || !finite(middle))
        return {};
    const Vec3 mid = apply(p, (low + high) * .5f); // the middle with the strap at the origin
    p.at = p.at + (middle - mid);
    return p;
}

Place clawsPlace(const Pose& grip, unsigned hand, ControllerKind kind) {
    const gun_grip::Held g = gun_grip::held(grip, hand, kind);
    if (!g.ok || hand > 1)
        return {};
    const Place k = placed(g.at, g.forward, g.up); // k.x = forward x up: the grip's +X
    if (!k.ok)
        return {};
    // hero007_visual clawSteelFrame: y the back of the hand (+X right, -X left), x = f x y, base ahead and back.
    const Vec3 f = g.forward, y = hand == 1 ? k.x : k.x * -1.f, x = cross(f, y);
    constexpr float ahead = .055f, back = .024f; // hero007_visual clawSteelAhead / clawSteelBack (CANDIDATE fit)
    Place p;
    p.at = g.at + f * ahead + y * back;
    p.x = x;
    p.y = y;
    p.z = f * -1.f;
    p.ok = finite(p.at);
    return p;
}
float clawReach(Vec3 low) { return std::isfinite(low.z) ? std::max(.05f, -low.z) : .05f; }

Place batonPlace(const Pose& grip, unsigned hand, ControllerKind kind, Vec3 low, Vec3 high) {
    const baton_grip::Shaft s = baton_grip::shaft(grip, hand, kind);
    const Vec3 size = high - low;
    if (!s.ok || !finite(size))
        return {};
    const float span = length(s.tip - s.butt), thick = std::min(size.x, size.y);
    if (span < .05f || !(size.z > .05f) || !(thick > .005f))
        return {};
    constexpr float diameter = .036f; // hero007_visual batonDiameter
    const float across = std::clamp(diameter / thick, .1f, 4.f), along = std::clamp(span / size.z, .1f, 4.f);
    const Vec3 f = (s.tip - s.butt) * (1.f / span);
    Vec3 up = Vec3{0, 1, 0} - f * f.y;
    up = length(up) > 1e-3f ? normalized(up) : Vec3{0, 0, 1};
    // hero007_visual appendBatonModel: at - r x + u y + f z (its -z end the butt on the butt).
    Place p;
    p.at = s.butt + f * (std::max(0.f, -low.z) * along);
    p.x = cross(f, up) * -across;
    p.y = up * across;
    p.z = f * along;
    p.ok = finite(p.at);
    return p;
}

Place pistolPlace(const Pose& grip, unsigned hand, ControllerKind kind) {
    const gun_grip::Held g = gun_grip::held(grip, hand, kind);
    if (!g.ok)
        return {};
    const Vec3 o = gun_grip::originFor(g.at, g.forward, g.up, gun_grip::palmPoint(gun_grip::pistol, true));
    return placed(o, g.forward, g.up);
}

Place batarangFlying(Vec3 at, Vec3 way, float spin, Vec3 low, Vec3 high, float span) {
    mesh_model::Mesh box;
    box.low = low, box.high = high;
    const mesh_model::Fit fit = mesh_model::fit(box, span);
    if (!finite(at) || !finite(way) || !(length(way) > 1e-4f) || !(fit.scale > 0) || !std::isfinite(spin))
        return {};
    const Vec3 up{0, 1, 0};
    Vec3 f = way - up * dot(way, up);
    f = length(f) > 1e-3f ? normalized(f) : Vec3{0, 0, -1};
    const Vec3 r = cross(f, up);
    const float c = std::cos(spin), s = std::sin(spin);
    const Vec3 across = r * c + f * s, nose = f * c - r * s;
    // model axis k -> world: across -> across, thin -> up, along -> nose; scaled, centred on the bounds' middle.
    Vec3 col[3];
    col[fit.across] = across * fit.scale;
    col[fit.thin] = up * fit.scale;
    col[fit.along] = nose * fit.scale;
    Place p;
    p.x = col[0], p.y = col[1], p.z = col[2];
    p.at = at - (p.x * fit.centre.x + p.y * fit.centre.y + p.z * fit.centre.z);
    p.ok = finite(p.at);
    return p;
}

bool launch(Flight& f, Vec3 from, Vec3 dir, const Vec3* target, const Tuning& t) {
    if (f.phase != Flight::held || !finite(from) || !finite(dir) || !(length(dir) > 1e-4f))
        return false;
    if (target && !finite(*target))
        target = nullptr;
    const Vec3 way = normalized(dir);
    f.phase = Flight::out;
    f.from = f.pos = from;
    f.vel = way * t.speed;
    f.aimed = target != nullptr;
    f.target = target ? *target : from + way * t.range;
    f.age = f.travelled = f.spin = 0;
    ++f.throws;
    return true;
}

FlightStep step(Flight& f, const Tuning& t, Vec3 hand, const Vec3* liveTarget, float dt) {
    FlightStep r;
    if (f.phase == Flight::held || !(dt > 0) || !std::isfinite(dt))
        return r;
    dt = std::min(dt, .1f);
    f.age += dt;
    const auto finish = [&]() {
        f.phase = Flight::held;
        f.vel = {};
        r.caught = true;
    };
    if (f.phase == Flight::out) {
        if (f.aimed && liveTarget && finite(*liveTarget))
            f.target = *liveTarget;
        const Vec3 to = f.target - f.pos;
        const float left = length(to), move = t.speed * dt;
        if (f.aimed && left <= move + .05f) { // reaches the enemy this step: the hit lands, it turns back
            f.pos = f.target;
            f.travelled += left;
            r.arrived = true;
            r.turned = true;
        } else {
            const Vec3 way = f.aimed && left > 1e-4f ? to * (1.f / left) : normalized(f.vel);
            f.vel = way * t.speed;
            f.pos = f.pos + f.vel * dt;
            f.travelled += move;
            if (f.age >= t.seconds || f.travelled >= t.range)
                r.turned = true;
        }
        if (r.turned) {
            if (!t.returns) {
                finish();
                return r;
            }
            f.phase = Flight::back;
        }
        return r;
    }
    // Back to the hand (hero072_play's return: steered, slowing near the hand, caught within catchRange).
    if (!finite(hand)) {
        finish();
        return r;
    }
    const Vec3 to = hand - f.pos;
    const float dist = length(to);
    if (dist <= t.catchRange || f.age >= t.seconds + 4.f) {
        f.pos = hand;
        finish();
        return r;
    }
    const float speed = std::min(t.back, std::max(t.slowest, dist * t.near));
    const Vec3 want = to * (1.f / dist);
    Vec3 way = length(f.vel) > 1e-4f ? normalized(f.vel) : want;
    if (dist <= t.straight) {
        way = want;
    } else {
        const float most = t.turn * dt, angle = std::acos(std::clamp(dot(way, want), -1.f, 1.f));
        if (angle <= most || angle < 1e-4f) {
            way = want;
        } else {
            const Quat q = Quat::around(cross(way, want), most);
            way = length(cross(way, want)) > 1e-5f ? normalized(q.rotate(way)) : want;
        }
    }
    const float move = speed * dt;
    if (move >= dist - t.catchRange) { // it would pass the hand: caught
        f.pos = hand;
        finish();
        return r;
    }
    f.vel = way * speed;
    f.pos = f.pos + f.vel * dt;
    f.travelled += move;
    return r;
}

HeadMap headMap(Vec3 gameHead, Vec3 right, Vec3 down, Vec3 forward, const Pose& xrHead) {
    HeadMap m;
    if (!finite(gameHead) || !finite(right) || !finite(down) || !finite(forward) || !usable(xrHead) || !(length(right) > .5f) ||
        !(length(down) > .5f) || !(length(forward) > .5f))
        return m;
    m.gameHead = gameHead;
    m.right = normalized(right), m.down = normalized(down), m.forward = normalized(forward);
    m.xrHead = xrHead;
    m.ok = true;
    return m;
}
Vec3 dirToXr(const HeadMap& m, Vec3 d) {
    // sm1_addon.hpp gameToHeadLocal: the head's own axes (+x right, +y up, -z forward), then the headset's turn.
    const Vec3 local{dot(d, m.right), -dot(d, m.down), -dot(d, m.forward)};
    return m.xrHead.orientation.rotate(local);
}
Vec3 toXr(const HeadMap& m, Vec3 g) { return m.xrHead.position + dirToXr(m, g - m.gameHead); }
Vec3 toGame(const HeadMap& m, Vec3 x) {
    const Vec3 l = m.xrHead.orientation.conjugate().rotate(x - m.xrHead.position);
    return m.gameHead + m.right * l.x - m.down * l.y - m.forward * l.z;
}

bool pack(const mesh_model::Mesh& m, std::vector<GpuVertex>& out) {
    const uint32_t n = m.triangles();
    if (!m.ok() || !n || m.corners.size() != size_t{n} * 3 || m.colors.size() != n)
        return false;
    const bool uvs = m.uvs.size() == size_t{n} * 6, normals = m.normals.size() == size_t{n} * 3;
    const bool textured = uvs && m.image.size() == n;
    const size_t mark = out.size();
    out.reserve(mark + size_t{n} * 3);
    for (uint32_t t = 0; t < n; ++t) {
        const Vec3 a = m.corners[t * 3], b = m.corners[t * 3 + 1], c = m.corners[t * 3 + 2];
        Vec3 face = cross(b - a, c - a);
        face = length(face) > 1e-12f ? normalized(face) : Vec3{0, 1, 0};
        float layer = -1;
        Vec3 colour = m.colors[t];
        if (textured && m.image[t] >= 0 && static_cast<size_t>(m.image[t]) < m.images.size()) {
            const auto& im = m.images[static_cast<size_t>(m.image[t])];
            if (im.layer >= 0) {
                layer = static_cast<float>(im.layer) + (im.cutout ? cutoutLayer : 0.f);
                colour = m.tints.size() == n ? m.tints[t] : Vec3{1, 1, 1};
            }
        }
        if (!finite(a) || !finite(b) || !finite(c) || !finite(colour)) {
            out.resize(mark);
            return false;
        }
        for (uint32_t k = 0; k < 3; ++k) {
            const Vec3 p = m.corners[t * 3 + k];
            Vec3 nn = normals ? m.normals[t * 3 + k] : face;
            nn = finite(nn) && length(nn) > 1e-6f ? normalized(nn) : face;
            GpuVertex v{};
            v.position[0] = p.x, v.position[1] = p.y, v.position[2] = p.z;
            v.normal[0] = nn.x, v.normal[1] = nn.y, v.normal[2] = nn.z;
            if (uvs)
                v.uv[0] = m.uvs[t * 6 + k * 2], v.uv[1] = m.uvs[t * 6 + k * 2 + 1];
            v.layer = layer;
            v.colour[0] = colour.x, v.colour[1] = colour.y, v.colour[2] = colour.z;
            out.push_back(v);
        }
    }
    return true;
}

Mat4 eyeViewProjection(const Pose& eye, float angleLeft, float angleRight, float angleUp, float angleDown) {
    return multiply(projection(angleLeft, angleRight, angleDown, angleUp, .02f, 300.f), viewMatrix(eye));
}

bool modelMatrices(const Place& p, float model[12], float normal[9]) {
    if (!p.ok || !finite(p.at) || !finite(p.x) || !finite(p.y) || !finite(p.z))
        return false;
    // columns x y z, the 4th `at`; rows the world's x y z.
    const float a[3][3] = {{p.x.x, p.y.x, p.z.x}, {p.x.y, p.y.y, p.z.y}, {p.x.z, p.y.z, p.z.z}};
    const float at[3] = {p.at.x, p.at.y, p.at.z};
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 3; ++c)
            model[r * 4 + c] = a[r][c];
        model[r * 4 + 3] = at[r];
    }
    const float det = a[0][0] * (a[1][1] * a[2][2] - a[1][2] * a[2][1]) - a[0][1] * (a[1][0] * a[2][2] - a[1][2] * a[2][0]) +
                      a[0][2] * (a[1][0] * a[2][1] - a[1][1] * a[2][0]);
    if (!(std::abs(det) > 1e-12f) || !std::isfinite(det))
        return false;
    // inverse transpose = cofactor matrix / det
    const float k = 1.f / det;
    normal[0] = (a[1][1] * a[2][2] - a[1][2] * a[2][1]) * k;
    normal[1] = -(a[1][0] * a[2][2] - a[1][2] * a[2][0]) * k;
    normal[2] = (a[1][0] * a[2][1] - a[1][1] * a[2][0]) * k;
    normal[3] = -(a[0][1] * a[2][2] - a[0][2] * a[2][1]) * k;
    normal[4] = (a[0][0] * a[2][2] - a[0][2] * a[2][0]) * k;
    normal[5] = -(a[0][0] * a[2][1] - a[0][1] * a[2][0]) * k;
    normal[6] = (a[0][1] * a[1][2] - a[0][2] * a[1][1]) * k;
    normal[7] = -(a[0][0] * a[1][2] - a[0][2] * a[1][0]) * k;
    normal[8] = (a[0][0] * a[1][1] - a[0][1] * a[1][0]) * k;
    return true;
}

} // namespace spidy::sm1::props
