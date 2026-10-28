// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
// Build 9 SUITS: suit_wear.hpp.
#include "spidy/suit_wear.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <unordered_map>
#include <vector>

namespace spidy::suit_wear {
namespace {
bool ok(Vec3 v) {
    return finite(v) && std::abs(v.x) < 1e6f && std::abs(v.y) < 1e6f && std::abs(v.z) < 1e6f;
}
Vec3 square(Vec3 v, Vec3 y) {
    return v - y * dot(v, y);
}
// The body's facing: across the shoulders (left - right) crossed with up the spine.
Vec3 facing(const Skeleton& k) {
    return normalized(cross(k.at[shoulderL] - k.at[shoulderR], k.at[neck] - k.at[pelvis]));
}
// Bone b's frame (columns x, y, z) on skeleton k; false when it has no length.
bool frame(const Skeleton& k, uint32_t b, Vec3 f, Vec3& x, Vec3& y, Vec3& z) {
    const Vec3 along = k.at[boneEnds[b][1]] - k.at[boneEnds[b][0]];
    if (!(length(along) > 1e-5f))
        return false;
    y = normalized(along);
    const Vec3 up = normalized(k.at[neck] - k.at[pelvis]);
    Vec3 ref;
    switch (b) {
    case boneHead:
        ref = square(k.face, y);
        break;
    case boneHandL:
    case boneHandR:
        ref = square(k.handBack[b == boneHandL ? 0 : 1], y);
        break;
    case boneForeL:
    case boneForeR:
        ref = normalized(square(f, y)) + normalized(square(k.handBack[b == boneForeL ? 0 : 1], y));
        break;
    case boneUpperL:
    case boneUpperR: { // the body's facing, its up taking over as the arm points ahead
        const Vec3 a = square(f, y);
        ref = a + square(up, y) * (std::max(0.f, .5f - length(a)) * 2.f);
        break;
    }
    default:
        ref = square(f, y);
    }
    if (!(length(ref) > 1e-3f))
        ref = square(up, y);
    if (!(length(ref) > 1e-3f))
        ref = square(std::abs(y.x) < .9f ? Vec3{1, 0, 0} : Vec3{0, 0, 1}, y);
    z = normalized(ref);
    x = cross(y, z);
    return ok(x) && ok(z);
}
uint64_t bits(float f) {
    uint32_t u;
    std::memcpy(&u, &f, 4);
    return u;
}
// Two-bone reach: the middle joint between `from` and `to` (bone lengths l1, l2), bent toward `pole`.
Vec3 bend(Vec3 from, Vec3 to, float l1, float l2, Vec3 pole) {
    Vec3 u = to - from;
    float d = length(u);
    if (!(d > 1e-5f))
        return from + normalized(pole) * l1;
    u = u / d;
    d = std::clamp(d, std::abs(l1 - l2) + 1e-4f, l1 + l2 - 1e-4f);
    const float x = (l1 * l1 - l2 * l2 + d * d) / (2 * d), h = std::sqrt(std::max(0.f, l1 * l1 - x * x));
    Vec3 p = square(pole, u);
    if (!(length(p) > 1e-4f))
        p = square(std::abs(u.y) < .9f ? Vec3{0, 1, 0} : Vec3{0, 0, 1}, u);
    return from + u * x + normalized(p) * h;
}
// Test build 12 HANDS: the back of each rest hand from the hand's own shape (the converter's handBack took the
// projected-out bone axis for the palm's normal: a random way on every suit, the hand turned off the controller).
// The palm's axis: the hand's thinnest way across the bone (its palm half, 5..55% along, is a slab). Its sign by
// three votes: the fingers' ends (70%+ along) sit toward the palm (a rest hand is a little curled), the thumb (the side
// reaching furthest across the width 15..60% along) is at -x of a right hand's frame and +x of a left's (x = y x back,
// y wrist -> fingers), and, weakly, the rest hands hang palms in or down (backs out, up and ahead). Both hands' votes
// summed and the hands made mirror images (x flipped), so a left and a right turn alike. Checked by eye on every suit
// (tools/suits/suit_hands SUIT_HANDS_DUMP, tools/suits/suit_bench); any the votes get wrong go in flipHands.
struct HandVote {
    Vec3 n{};
    float vote = 0;
    bool ok = false;
};
HandVote voteHand(const Rig& rig, int side) {
    HandVote out;
    const Skeleton& k = rig.skeleton;
    const uint32_t b = side ? boneHandR : boneHandL, wr = side ? wristR : wristL, tp = side ? tipR : tipL;
    Vec3 ax = k.at[tp] - k.at[wr];
    const float L = length(ax);
    if (!(L > 1e-4f))
        return out;
    ax = ax / L;
    const Vec3 u = normalized(square(std::abs(ax.y) < .9f ? Vec3{0, 1, 0} : Vec3{1, 0, 0}, ax)), v = cross(ax, u);
    std::vector<Vec3> pts;
    std::vector<float> along;
    double mu = 0, mv = 0, cu = 0, cv = 0, cuv = 0, n = 0;
    for (size_t i = 0; i < rig.rest.size(); ++i) {
        uint32_t w = 0;
        for (int c = 0; c < 4; ++c)
            if ((rig.joint[i * 4 + c] & 15) == b)
                w += rig.weight[i * 4 + c];
        if (w < 128)
            continue;
        const Vec3 d = rig.rest[i] - k.at[wr];
        const float a = dot(d, ax);
        const Vec3 q = d - ax * a;
        pts.push_back(q);
        along.push_back(a);
        if (a > .05f * L && a < .55f * L) {
            const double x = dot(q, u), y = dot(q, v);
            mu += x, mv += y, cu += x * x, cv += y * y, cuv += x * y, n += 1;
        }
    }
    if (n < 12)
        return out;
    mu /= n, mv /= n;
    cu = cu / n - mu * mu, cv = cv / n - mv * mv, cuv = cuv / n - mu * mv;
    // the 2 x 2 covariance's smaller eigenvalue's vector: the palm's normal
    const double tr = cu + cv, det = cu * cv - cuv * cuv, disc = std::sqrt(std::max(0.0, tr * tr / 4 - det));
    const double lo = tr / 2 - disc;
    Vec3 nrm = std::abs(cuv) > 1e-14 ? u * static_cast<float>(cuv) + v * static_cast<float>(lo - cu) : (cu < cv ? u : v);
    if (!(length(nrm) > 1e-9f))
        return out;
    nrm = normalized(nrm);
    double palm = 0, np = 0, tips = 0, nt = 0;
    const Vec3 wd = cross(ax, nrm);
    std::vector<float> across;
    for (size_t i = 0; i < pts.size(); ++i) {
        const float a = along[i], h = dot(pts[i], nrm);
        if (a > .05f * L && a < .55f * L)
            palm += h, np += 1;
        if (a > .7f * L)
            tips += h, nt += 1;
        if (a > .15f * L && a < .6f * L)
            across.push_back(dot(pts[i], wd));
    }
    float vote = 0;
    if (nt > 0 && np > 0)
        vote += -static_cast<float>(tips / nt - palm / np) / L; // the tips toward -nrm: nrm is the back
    if (across.size() > 20) {
        std::sort(across.begin(), across.end());
        const size_t m = across.size();
        const float mid = across[m / 2], lo1 = across[m / 100], hi1 = across[m - 1 - m / 100];
        const float th = ((hi1 - mid) - (mid - lo1)) / L; // > 0: the thumb at +wd
        vote += side ? -th : th;
    }
    out.n = nrm;
    out.vote = vote;
    out.ok = true;
    return out;
}
// Suits whose rest hands the votes turn the wrong way (by eye): their backs flipped.
bool flipHands(const char* name) {
    static const char* const flip[] = {nullptr}; // none at build 12 (Iron Man 4's repulsor checked in its palm)
    if (!name)
        return false;
    for (const char* f : flip)
        if (f && std::strcmp(name, f) == 0)
            return true;
    return false;
}
void restHands(Rig& rig, const char* name) {
    Skeleton& k = rig.skeleton;
    const HandVote l = voteHand(rig, 0), r = voteHand(rig, 1);
    const auto mirror = [](Vec3 a) { return Vec3{-a.x, a.y, a.z}; };
    if (!l.ok && !r.ok)
        return; // keep the model's own
    // the right hand's way, with the left's mirrored onto it
    Vec3 nR = r.ok ? r.n : mirror(l.n);
    float vote = r.ok ? r.vote : 0;
    if (l.ok) {
        const float s = dot(mirror(l.n), nR) < 0 ? -1.f : 1.f;
        nR = r.ok ? normalized(nR + mirror(l.n) * s) : nR;
        vote += l.vote * s;
    }
    const Vec3 prior = normalized(Vec3{-.5f, .6f, .45f}); // a right rest hand's back: out, up and ahead
    vote += .3f * dot(nR, prior);
    if (vote < 0)
        nR = nR * -1.f;
    if (flipHands(name))
        nR = nR * -1.f;
    for (int side = 0; side < 2; ++side) {
        const Vec3 ax = normalized(k.at[side ? tipR : tipL] - k.at[side ? wristR : wristL]);
        const Vec3 h = square(side ? nR : mirror(nR), ax);
        if (length(h) > .2f)
            k.handBack[side] = normalized(h);
        rig.handThumb[side] = vote;
    }
}
} // namespace

namespace {
void handReach(const Rig& rig, int side, float& knuckle, float& fingers);
void curlFinger(const BoneMove& b, Vec3& d, Vec3& n);
// Version 0.0.73 test 3 FIST: the rest fingers' line (Rig::fingerDepth, fingerSlope): the median depth (the hand
// bone's z) of the hand's own vertices in six steps past the knuckles, a straight line fitted through them.
void fingerLine(Rig& rig, int side) {
    rig.fingerDepth[side] = rig.fingerSlope[side] = 0;
    const Skeleton& k = rig.skeleton;
    const uint32_t b = side ? boneHandR : boneHandL;
    Vec3 x0, y0, z0;
    if (!frame(k, b, facing(k), x0, y0, z0))
        return;
    float kn = 0, fi = 0;
    handReach(rig, side, kn, fi);
    if (!(fi > 1e-3f))
        return;
    constexpr int steps = 6;
    std::vector<float> depth[steps];
    const Vec3 a0 = k.at[boneEnds[b][0]];
    for (size_t v = 0; v < rig.rest.size(); ++v) {
        uint32_t w = 0;
        for (int c = 0; c < 4; ++c)
            if ((rig.joint[v * 4 + c] & 15) == b)
                w += rig.weight[v * 4 + c];
        if (w < 200)
            continue;
        const Vec3 d = rig.rest[v] - a0;
        const float s = dot(d, y0) - kn;
        const int i = static_cast<int>(std::floor(s / fi * steps));
        if (s > 0 && i >= 0 && i < steps && std::isfinite(dot(d, z0)))
            depth[i].push_back(dot(d, z0));
    }
    float n = 0, sx = 0, sy = 0, sxx = 0, sxy = 0;
    for (int i = 0; i < steps; ++i) {
        if (depth[i].size() < 5)
            continue;
        auto mid = depth[i].begin() + static_cast<std::ptrdiff_t>(depth[i].size() / 2);
        std::nth_element(depth[i].begin(), mid, depth[i].end());
        const float x = (static_cast<float>(i) + .5f) * fi / steps, y = *mid;
        n += 1, sx += x, sy += y, sxx += x * x, sxy += x * y;
    }
    if (n < 2)
        return;
    const float den = n * sxx - sx * sx;
    const float slope = std::abs(den) > 1e-12f ? (n * sxy - sx * sy) / den : 0.f;
    const float h = rig.height / 1.8f;
    rig.fingerSlope[side] = std::isfinite(slope) ? std::clamp(slope, -1.f, .3f) : 0.f;
    const float depth0 = (sy - rig.fingerSlope[side] * sx) / n;
    rig.fingerDepth[side] = std::isfinite(depth0) ? std::clamp(depth0, -.06f * h, .02f * h) : 0.f;

    // the fist's hole: the hand's own vertices fully curled (as skin curls them), then the handle's axis (along x0)
    // tried on a 2.5 mm grid 4 cm round the curl's centre: the fewest outward faces inside the handle (the handle
    // showing through), never a side of it left bare (8 sectors with skin within 2 cm of its surface)
    BoneMove m;
    m.x0 = x0, m.y0 = y0, m.z0 = z0, m.knuckle = kn, m.fingers = fi, m.curl = fistRadians;
    m.depth = rig.fingerDepth[side], m.slope = rig.fingerSlope[side];
    std::vector<float> py, pz, ny, nz;
    for (size_t v = 0; v < rig.rest.size() && v < rig.restNormal.size(); ++v) {
        uint32_t w = 0;
        for (int c = 0; c < 4; ++c)
            if ((rig.joint[v * 4 + c] & 15) == b)
                w += rig.weight[v * 4 + c];
        if (w < 200)
            continue;
        Vec3 d = rig.rest[v] - a0, nn = rig.restNormal[v];
        curlFinger(m, d, nn);
        const float y = dot(d, y0), z = dot(d, z0);
        if (std::isfinite(y) && std::isfinite(z))
            py.push_back(y), pz.push_back(z), ny.push_back(dot(nn, y0)), nz.push_back(dot(nn, z0));
    }
    const float r = .018f * h, cy = kn, cz = m.depth - fi / fistRadians;
    int best = 1 << 30;
    for (int iy = -16; iy <= 16; ++iy)
        for (int iz = -16; iz <= 16; ++iz) {
            const float hy = cy + iy * .0025f * h, hz = cz + iz * .0025f * h;
            int poke = 0;
            unsigned sectors = 0;
            for (size_t i = 0; i < py.size(); ++i) {
                const float dy = py[i] - hy, dz = pz[i] - hz, dd = std::sqrt(dy * dy + dz * dz);
                if (dd < r - .001f * h && dd > 1e-6f && (ny[i] * dy + nz[i] * dz) / dd > .3f)
                    ++poke;
                if (dd < r + .02f * h) {
                    const float a = std::atan2(dz, dy) + 3.14159265f;
                    sectors |= 1u << std::min(7, static_cast<int>(a / (6.2831853f / 8)));
                }
            }
            // a step off the centre costs a face: the fit stays put unless the hole is clearly better elsewhere
            const int cost = poke + 1000 * (8 - __builtin_popcount(sectors)) + std::abs(iy) + std::abs(iz);
            if (cost < best)
                best = cost, rig.fistHole[side][0] = hy - cy, rig.fistHole[side][1] = hz - cz;
        }
}
} // namespace

bool makeRig(const mesh_model::Mesh& mesh, Rig& out, const char* name) {
    out = {};
    if (!mesh.ok() || !mesh.hasSuit || !mesh.hasSkin() || !mesh.hasNormals())
        return false;
    const uint32_t n = mesh.triangles();
    Skeleton& k = out.skeleton;
    for (uint32_t e = 0; e < ends; ++e)
        k.at[e] = {mesh.suitEnds[e * 3], mesh.suitEnds[e * 3 + 1], mesh.suitEnds[e * 3 + 2]};
    k.face = {0, 0, 1};
    k.faceUp = {0, 1, 0};
    for (int s = 0; s < 2; ++s) {
        const Vec3 hb{mesh.suitHandBack[s * 3], mesh.suitHandBack[s * 3 + 1], mesh.suitHandBack[s * 3 + 2]};
        k.handBack[s] = length(hb) > .5f ? normalized(hb) : Vec3{0, 1, 0};
    }
    k.ok = true;
    out.height = k.at[headTop].y;
    if (!(out.height > .3f) || !(length(facing(k)) > .5f))
        return false;
    // the corners welded into vertices (the same place, normal and skin): each skinned once a frame
    std::unordered_map<uint64_t, std::vector<uint32_t>> seen;
    seen.reserve(size_t{n} * 2);
    out.corner.resize(size_t{n} * 3);
    for (size_t c = 0; c < size_t{n} * 3; ++c) {
        const Vec3 p = mesh.corners[c], q = mesh.normals[c];
        uint64_t h = bits(p.x) * 0x9E3779B97F4A7C15ull ^ bits(p.y) * 0xC2B2AE3D27D4EB4Full ^ bits(p.z) * 0x165667B19E3779F9ull ^
                     bits(q.x) * 0x27D4EB2F165667C5ull ^ bits(q.y + 2.f) * 0x94D049BB133111EBull ^ bits(q.z + 4.f);
        uint32_t jw;
        std::memcpy(&jw, &mesh.skinJoints[c * 4], 4);
        h ^= uint64_t{jw} * 0xBF58476D1CE4E5B9ull;
        std::memcpy(&jw, &mesh.skinWeights[c * 4], 4);
        h ^= uint64_t{jw} * 0xD6E8FEB86659FD93ull;
        auto& list = seen[h];
        uint32_t found = UINT32_MAX;
        for (uint32_t v : list)
            if (std::memcmp(&out.rest[v], &p, sizeof p) == 0 && std::memcmp(&out.restNormal[v], &q, sizeof q) == 0 &&
                std::memcmp(&out.joint[v * 4], &mesh.skinJoints[c * 4], 4) == 0 &&
                std::memcmp(&out.weight[v * 4], &mesh.skinWeights[c * 4], 4) == 0) {
                found = v;
                break;
            }
        if (found == UINT32_MAX) {
            found = static_cast<uint32_t>(out.rest.size());
            out.rest.push_back(p);
            out.restNormal.push_back(q);
            out.joint.insert(out.joint.end(), &mesh.skinJoints[c * 4], &mesh.skinJoints[c * 4] + 4);
            out.weight.insert(out.weight.end(), &mesh.skinWeights[c * 4], &mesh.skinWeights[c * 4] + 4);
            list.push_back(found);
        }
        out.corner[c] = found;
    }
    restHands(out, name);
    fingerLine(out, 0);
    fingerLine(out, 1);
    findCape(mesh, out);
    out.triangleBone.resize(n);
    for (uint32_t t = 0; t < n; ++t) {
        uint32_t sum[bones]{};
        for (uint32_t c = t * 3; c < t * 3 + 3; ++c)
            for (uint32_t q = 0; q < 4; ++q)
                sum[mesh.skinJoints[c * 4 + q] & 15] += mesh.skinWeights[c * 4 + q];
        out.triangleBone[t] = static_cast<uint8_t>(std::max_element(sum, sum + bones) - sum);
    }
    out.ok = true;
    return true;
}

Vec3 handWay(Vec3 gripForward, Vec3 gripUp, float tiltDeg) {
    const Vec3 f = normalized(gripForward), u = normalized(square(gripUp, f));
    if (!(length(f) > .5f) || !(length(u) > .5f))
        return {};
    const float a = std::clamp(std::isfinite(tiltDeg) ? tiltDeg : 30.f, 0.f, 60.f) * 3.14159265f / 180.f;
    return normalized(u * -std::cos(a) + f * std::sin(a));
}
Vec3 handBackOf(Vec3 gripX, int side) {
    const Vec3 x = normalized(gripX);
    return side ? x : x * -1.f;
}
namespace {
// The rest hand's knuckles (from the wrist) and its fingers' reach past them (rest metres).
void handReach(const Rig& rig, int side, float& knuckle, float& fingers) {
    const Skeleton& r = rig.skeleton;
    const float L = length(r.at[side ? tipR : tipL] - r.at[side ? wristR : wristL]), h = rig.height / 1.8f;
    knuckle = std::min(L * knuckleShare, knuckleMax * h);
    fingers = std::max(.02f * h, std::min(L - knuckle, fingersMax * h));
}
} // namespace

bool estimate(const Rig& rig, const Tracked& t, Skeleton& out) {
    out = {};
    if (!rig.ok || !ok(t.eye) || !ok(t.feet) || !ok(t.eyeForward) || !ok(t.eyeUp) || !ok(t.up))
        return false;
    const Skeleton& r = rig.skeleton;
    const Vec3 up = normalized(t.up);
    if (!(length(up) > .5f))
        return false;
    // the head's axes (z its facing, y its up) and the body's (z the level facing, x its left)
    const Vec3 hz = normalized(t.eyeForward), hy = normalized(square(t.eyeUp, hz)), hx = cross(hy, hz);
    Vec3 fz = square(hz, up);
    if (!(length(fz) > .2f))
        fz = square(hy * (dot(hz, up) > 0 ? -1.f : 1.f), up); // straight up or down: the top of the head says
    if (!(length(fz) > 1e-3f) || !(length(hy) > .5f))
        return false;
    fz = normalized(fz);
    const Vec3 fx = cross(up, fz);
    const auto H = [&](Vec3 v) { return hx * v.x + hy * v.y + hz * v.z; };
    const auto B = [&](Vec3 v) { return fx * v.x + up * v.y + fz * v.z; };
    // where the model's eyes are: half way up its head, 8 cm (at 1.8 m) in front of its axis
    const float restHeight = rig.height;
    const Vec3 restEye = r.at[head] + (r.at[headTop] - r.at[head]) * .5f + Vec3{0, 0, .08f * restHeight / 1.8f};
    const float eyeHeight = dot(t.eye - t.feet, up), standing = t.standingEye > .5f ? t.standingEye : eyeHeight;
    if (!(restEye.y > .3f))
        return false;
    const float s = std::clamp(standing / restEye.y, .6f, 1.4f);
    const Vec3 headAt = t.eye - H(restEye - r.at[head]) * s;
    out.at[head] = headAt;
    out.at[headTop] = headAt + H(r.at[headTop] - r.at[head]) * s;
    for (uint32_t e : {neck, chest, pelvis, shoulderL, shoulderR, hipL, hipR})
        out.at[e] = headAt + B(r.at[e] - r.at[head]) * s;
    out.face = hz;
    out.faceUp = hy;
    out.up = up;
    // the arms
    for (int side = 0; side < 2; ++side) {
        const uint32_t sh = side ? shoulderR : shoulderL, el = side ? elbowR : elbowL, wr = side ? wristR : wristL,
                       tip = side ? tipR : tipL;
        const float l1 = length(r.at[el] - r.at[sh]) * s, l2 = length(r.at[wr] - r.at[el]) * s,
                    hand = length(r.at[tip] - r.at[wr]) * s;
        const Vec3 fwd = normalized(t.gripForward[side]), gx = normalized(t.gripX[side]);
        const Vec3 way = handWay(t.gripForward[side], t.gripUp[side], t.handleTiltDeg[side]);
        out.curl[side] = std::clamp(std::isfinite(t.curl[side]) ? t.curl[side] : 0.f, 0.f, 1.f);
        if (t.hand[side] && ok(t.grip[side]) && length(fwd) > .5f && length(gx) > .5f && length(way) > .5f) {
            // Test build 12 HANDS: the hand along the forearm through the fist (handWay), its back the grip's
            // -x (left) or +x (right); the knuckles' line just past the handle's axis (the grip point) and the
            // hand's middle a curled finger's radius behind it, so the fist closes round the handle
            const Vec3 back = handBackOf(gx, side);
            float kn = 0, fi = 0;
            handReach(rig, side, kn, fi);
            // Version 0.0.73 test 3 FIST: the curl's centre (the handle) under the fingers' own line (fingerDepth).
            // With fistFit the hole the fist leaves (Rig::fistHole) round the grip point instead.
            const bool fit = t.fistFit[side];
            out.fistFit[side] = fit;
            const float holeY = kn + (fit ? rig.fistHole[side][0] : 0.f),
                        holeZ = (fit ? rig.fingerDepth[side] + rig.fistHole[side][1] : 0.f) - fi / fistRadians;
            const Vec3 wrist = t.grip[side] - way * (holeY * s) - back * (holeZ * s);
            const Vec3 out_ = side ? fx * -1.f : fx;
            out.at[wr] = wrist;
            out.at[tip] = wrist + way * hand;
            out.at[el] = bend(out.at[sh], wrist, l1, l2, up * -.8f + out_ * .45f - fz * .3f);
            out.handBack[side] = normalized(square(back, way));
        } else { // untracked: the rest arm, turned with the body
            out.at[el] = out.at[sh] + B(r.at[el] - r.at[sh]) * s;
            out.at[wr] = out.at[sh] + B(r.at[wr] - r.at[sh]) * s;
            out.at[tip] = out.at[sh] + B(r.at[tip] - r.at[sh]) * s;
            out.handBack[side] = B(r.handBack[side]);
        }
    }
    // the legs: each foot on the floor under its rest place, the knee bent ahead
    for (int side = 0; side < 2; ++side) {
        const uint32_t hip = side ? hipR : hipL, knee = side ? kneeR : kneeL, foot = side ? footR : footL;
        const float l1 = length(r.at[knee] - r.at[hip]) * s, l2 = length(r.at[foot] - r.at[knee]) * s;
        Vec3 f = out.at[hip] + B(r.at[foot] - r.at[hip]) * s;
        f = f - up * (dot(f - t.feet, up) - r.at[foot].y * s);
        if (length(f - out.at[hip]) > l1 + l2) // in the air (or the floor guessed low): the leg straight
            f = out.at[hip] + normalized(f - out.at[hip]) * (l1 + l2);
        out.at[foot] = f;
        out.at[knee] = bend(out.at[hip], f, l1, l2, fz);
    }
    for (const Vec3& p : out.at)
        if (!ok(p))
            return false;
    out.ok = true;
    return true;
}

bool moves(const Rig& rig, const Skeleton& live, BoneMove out[bones]) {
    if (!rig.ok || !live.ok)
        return false;
    const Skeleton& r = rig.skeleton;
    const Vec3 f0 = facing(r), f1 = facing(live);
    if (!(length(f0) > .5f) || !(length(f1) > .5f))
        return false;
    float s = length(live.at[neck] - live.at[pelvis]) / std::max(1e-4f, length(r.at[neck] - r.at[pelvis]));
    s = std::clamp(s, .5f, 2.f);
    for (uint32_t b = 0; b < bones; ++b) {
        Vec3 x0, y0, z0, x1, y1, z1;
        if (!frame(r, b, f0, x0, y0, z0) || !frame(live, b, f1, x1, y1, z1))
            return false;
        const float l0 = length(r.at[boneEnds[b][1]] - r.at[boneEnds[b][0]]) * s,
                    l1 = length(live.at[boneEnds[b][1]] - live.at[boneEnds[b][0]]);
        const float k = std::clamp(l1 / std::max(1e-5f, l0), .7f, 1.4f);
        const Vec3 c0[3]{x0, y0, z0}, c1[3]{x1, y1, z1};
        const float kk[3]{1.f, k, 1.f};
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j) {
                float m = 0, n = 0;
                for (int c = 0; c < 3; ++c) {
                    const float a = (&c1[c].x)[i] * (&c0[c].x)[j];
                    m += a * kk[c];
                    n += a;
                }
                out[b].m[i * 3 + j] = m * s;
                out[b].n[i * 3 + j] = n;
            }
        out[b].a0 = r.at[boneEnds[b][0]];
        out[b].a1 = live.at[boneEnds[b][0]];
        if (b == boneHandL || b == boneHandR) { // Test build 12 FIST
            const int side = b == boneHandR;
            out[b].x0 = x0, out[b].y0 = y0, out[b].z0 = z0;
            handReach(rig, side, out[b].knuckle, out[b].fingers);
            const float c = std::isfinite(live.curl[side]) ? std::clamp(live.curl[side], 0.f, 1.f) : 0.f;
            out[b].curl = c * fistRadians;
            if (live.fistFit[side])
                out[b].depth = rig.fingerDepth[side], out[b].slope = rig.fingerSlope[side];
        }
    }
    return true;
}

namespace {
// Test build 12 FIST: a rest point past the knuckles bent round the palm's side (-z0) about the knuckles' line
// (x0), evenly along the fingers (a curl of b.curl radians over b.fingers), so the fingers wrap a handle; past
// the fingertips it carries on straight. Its normal turned the same.
void curlFinger(const BoneMove& b, Vec3& d, Vec3& n) {
    const float lx = dot(d, b.x0), ly = dot(d, b.y0);
    const float s0 = ly - b.knuckle;
    if (!(s0 > 0))
        return;
    // Version 0.0.73 test 3 FIST: straightened off the fingers' own line (depth + slope s0) first, the arc length
    // along it; then curled round the centre under that line (with depth = slope = 0: Test build 12's curl).
    const float lz = dot(d, b.z0) - b.slope * s0 - b.depth, s = s0 * std::sqrt(1.f + b.slope * b.slope);
    const float R = b.fingers / b.curl, phi = std::min(s, b.fingers) / R, extra = std::max(0.f, s - b.fingers);
    const float cs = std::cos(phi), sn = std::sin(phi);
    // (y, z) about the curl's centre (knuckle, depth - R): (0, R + lz) turned by phi, then on along the tangent
    const float y = b.knuckle + (R + lz) * sn + extra * cs, z = b.depth - R + (R + lz) * cs - extra * sn;
    d = b.x0 * lx + b.y0 * y + b.z0 * z;
    float nx = dot(n, b.x0), ny = dot(n, b.y0), nz = dot(n, b.z0);
    if (b.slope != 0) { // straightened: turned off the slope first
        const float k = 1.f / std::sqrt(1.f + b.slope * b.slope), ty = ny * k + nz * b.slope * k;
        nz = -ny * b.slope * k + nz * k, ny = ty;
    }
    n = b.x0 * nx + b.y0 * (ny * cs + nz * sn) + b.z0 * (-ny * sn + nz * cs);
}
} // namespace

// Test build 12 CAPE.
void findCape(const mesh_model::Mesh& mesh, Rig& rig) {
    rig.hasCape = false, rig.capeTriangles = 0;
    rig.capeHang.clear(), rig.capeAt.clear();
    const uint32_t n = mesh.triangles();
    const Skeleton& k = rig.skeleton;
    if (!n || rig.corner.size() != size_t{n} * 3 || !(rig.height > .3f))
        return;
    const float f = rig.height / 1.8f;
    const float mid = k.at[pelvis].z, top = std::max(k.at[shoulderL].y, k.at[shoulderR].y);
    // the triangles through each place (welded by position only: a seam's sides are one cloth)
    std::unordered_map<uint64_t, uint32_t> placeOf;
    placeOf.reserve(rig.rest.size() * 2);
    std::vector<uint32_t> place(rig.rest.size());
    for (size_t v = 0; v < rig.rest.size(); ++v) {
        const Vec3 p = rig.rest[v];
        const uint64_t h = bits(p.x) * 0x9E3779B97F4A7C15ull ^ bits(p.y) * 0xC2B2AE3D27D4EB4Full ^ bits(p.z) * 0x165667B19E3779F9ull;
        place[v] = placeOf.emplace(h, static_cast<uint32_t>(placeOf.size())).first->second;
    }
    std::vector<std::vector<uint32_t>> through(placeOf.size());
    std::vector<Vec3> centre(n);
    for (uint32_t t = 0; t < n; ++t) {
        Vec3 c{};
        for (uint32_t q = 0; q < 3; ++q) {
            c += rig.rest[rig.corner[t * 3 + q]];
            through[place[rig.corner[t * 3 + q]]].push_back(t);
        }
        centre[t] = c * (1.f / 3.f);
    }
    std::vector<uint8_t> in(n);
    std::vector<uint32_t> todo;
    for (uint32_t t = 0; t < n; ++t)
        if (mid - centre[t].z > capeSeedBehind * f && centre[t].y < top)
            in[t] = 1, todo.push_back(t);
    while (!todo.empty()) {
        const uint32_t t = todo.back();
        todo.pop_back();
        for (uint32_t q = 0; q < 3; ++q)
            for (uint32_t u : through[place[rig.corner[t * 3 + q]]])
                if (!in[u] && mid - centre[u].z > capeKeepBehind * f && centre[u].y < top + .05f * f)
                    in[u] = 1, todo.push_back(u);
    }
    float area = 0, low = rig.height;
    uint32_t count = 0;
    std::vector<uint8_t> vin(rig.rest.size());
    for (uint32_t t = 0; t < n; ++t)
        if (in[t]) {
            const Vec3 a = rig.rest[rig.corner[t * 3]], b = rig.rest[rig.corner[t * 3 + 1]], c = rig.rest[rig.corner[t * 3 + 2]];
            area += length(cross(b - a, c - a)) * .5f;
            ++count;
            for (uint32_t q = 0; q < 3; ++q) {
                vin[rig.corner[t * 3 + q]] = 1;
                low = std::min(low, rig.rest[rig.corner[t * 3 + q]].y);
            }
        }
    if (!(area > capeMinArea * f * f) || !(low < capeReachDown * rig.height) || !(top - low > .3f * f))
        return;
    // each height band's width and mean depth (16 bands down the cape)
    constexpr int bands = 16;
    float half[bands]{}, depth[bands]{}, weight[bands]{};
    const auto down = [&](float y) { return std::clamp((top - y) / (top - low), 0.f, 1.f); };
    const auto band = [&](float t) { return std::min(bands - 1, static_cast<int>(t * bands)); };
    for (size_t v = 0; v < rig.rest.size(); ++v)
        if (vin[v]) {
            const int b = band(down(rig.rest[v].y));
            half[b] = std::max(half[b], std::abs(rig.rest[v].x - k.at[pelvis].x));
            depth[b] += rig.rest[v].z, weight[b] += 1;
        }
    for (int b = 0; b < bands; ++b) {
        depth[b] = weight[b] > 0 ? depth[b] / weight[b] : mid - capeKeepBehind * f;
        if (b && half[b] < half[b - 1] * .5f)
            half[b] = half[b - 1];
        half[b] = std::max(half[b], .05f * f);
    }
    rig.capeHang.assign(rig.rest.size(), -1.f); // -1: not the cape
    rig.capeAt.assign(rig.rest.size(), Vec3{});
    for (size_t v = 0; v < rig.rest.size(); ++v)
        if (vin[v]) {
            const Vec3 p = rig.rest[v];
            const float t = down(p.y);
            const int b = band(t);
            const float e = std::clamp((t - .02f) / .13f, 0.f, 1.f);
            rig.capeHang[v] = e * e * (3 - 2 * e);
            rig.capeAt[v] = {std::clamp((p.x - k.at[pelvis].x) / half[b], -1.f, 1.f), t, p.z - depth[b]};
        }
    rig.hasCape = true;
    rig.capeTriangles = count;
}

Vec3 capeWing(const Skeleton& k, float a, float t, int side, float scale) {
    const uint32_t sh = side ? shoulderR : shoulderL, el = side ? elbowR : elbowL, wr = side ? wristR : wristL,
                   kn = side ? kneeR : kneeL;
    a = std::clamp(a, 0.f, 1.f), t = std::clamp(t, 0.f, 1.f);
    const auto mix = [](Vec3 p, Vec3 q, float u) { return p + (q - p) * u; };
    Vec3 top;
    if (a < .25f)
        top = mix(k.at[neck], k.at[sh], a / .25f);
    else if (a < .6f)
        top = mix(k.at[sh], k.at[el], (a - .25f) / .35f);
    else
        top = mix(k.at[el], k.at[wr], (a - .6f) / .4f);
    Vec3 out = k.at[sh] - k.at[side ? shoulderL : shoulderR];
    out = length(out) > 1e-4f ? normalized(out) : Vec3{};
    const Vec3 hem = mix((k.at[kneeL] + k.at[kneeR]) * .5f, k.at[kn] + out * (.12f * scale), a);
    const Vec3 back = facing(k) * -1.f;
    const float billow = .10f * scale * std::sin(3.14159265f * t) * std::sin(3.14159265f * std::min(1.f, a * 1.2f));
    return mix(top, hem, t) + back * (.06f * scale + billow);
}

bool cape(const Rig& rig, const BoneMove m[bones], const Skeleton& live, float spread, Skinned& s) {
    if (!rig.ok || !rig.hasCape || s.p.size() != rig.rest.size() || rig.capeHang.size() != rig.rest.size())
        return false;
    spread = std::isfinite(spread) ? std::clamp(spread, 0.f, 1.f) : 0.f;
    const BoneMove& c = m[boneChest];
    const Skeleton& r = rig.skeleton;
    const float scale = std::clamp(length(live.at[neck] - live.at[pelvis]) / std::max(1e-4f, length(r.at[neck] - r.at[pelvis])), .5f, 2.f) *
                        rig.height / 1.8f;
    const Vec3 back = facing(live) * -1.f;
    Vec3 wingNormal[2];
    for (int side = 0; side < 2; ++side) {
        const Vec3 o = capeWing(live, 0, .3f, side, scale), across = capeWing(live, 1, .3f, side, scale) - o,
                   dn = capeWing(live, .5f, .9f, side, scale) - capeWing(live, .5f, .1f, side, scale);
        const Vec3 nn = cross(across, dn);
        wingNormal[side] = length(nn) > 1e-6f ? normalized(nn) : back;
        if (dot(wingNormal[side], back) < 0)
            wingNormal[side] = wingNormal[side] * -1.f;
    }
    for (size_t v = 0; v < rig.rest.size(); ++v) {
        const float hang = rig.capeHang[v];
        if (!(hang >= 0))
            continue;
        const Vec3 d = rig.rest[v] - c.a0, rn = rig.restNormal[v];
        const Vec3 hung = c.a1 + Vec3{c.m[0] * d.x + c.m[1] * d.y + c.m[2] * d.z, c.m[3] * d.x + c.m[4] * d.y + c.m[5] * d.z,
                                      c.m[6] * d.x + c.m[7] * d.y + c.m[8] * d.z};
        const Vec3 hn{c.n[0] * rn.x + c.n[1] * rn.y + c.n[2] * rn.z, c.n[3] * rn.x + c.n[4] * rn.y + c.n[5] * rn.z,
                      c.n[6] * rn.x + c.n[7] * rn.y + c.n[8] * rn.z};
        Vec3 p = s.p[v] + (hung - s.p[v]) * hang, q = s.n[v] + (hn - s.n[v]) * hang;
        if (spread > 0) {
            const Vec3 at = rig.capeAt[v];
            const int side = at.x < 0; // +x at rest: the suit's left
            const Vec3 w = capeWing(live, std::abs(at.x), at.y, side, scale) + wingNormal[side] * (-at.z * .6f);
            const float e = std::clamp(at.y / .06f, 0.f, 1.f); // the top edge too: it runs out along the arms
            const float k = spread * e * e * (3 - 2 * e);
            p = p + (w - p) * k;
            q = q + (wingNormal[side] - q) * k;
        }
        if (ok(p) && ok(q))
            s.p[v] = p, s.n[v] = q;
    }
    return true;
}

bool skin(const Rig& rig, const BoneMove m[bones], Skinned& out) {
    if (!rig.ok)
        return false;
    const size_t n = rig.rest.size();
    out.p.resize(n);
    out.n.resize(n);
    for (size_t v = 0; v < n; ++v) {
        Vec3 p{}, q{};
        const Vec3 r = rig.rest[v], rn = rig.restNormal[v];
        for (size_t c = 0; c < 4; ++c) {
            const uint8_t w8 = rig.weight[v * 4 + c];
            if (!w8)
                continue;
            const float w = w8 * (1.f / 255.f);
            const BoneMove& b = m[rig.joint[v * 4 + c] & 15];
            Vec3 d = r - b.a0;
            Vec3 rn_ = rn;
            if (b.curl > 1e-3f && b.fingers > 1e-4f)
                curlFinger(b, d, rn_);
            p += (b.a1 + Vec3{b.m[0] * d.x + b.m[1] * d.y + b.m[2] * d.z, b.m[3] * d.x + b.m[4] * d.y + b.m[5] * d.z,
                              b.m[6] * d.x + b.m[7] * d.y + b.m[8] * d.z}) * w;
            q += Vec3{b.n[0] * rn_.x + b.n[1] * rn_.y + b.n[2] * rn_.z, b.n[3] * rn_.x + b.n[4] * rn_.y + b.n[5] * rn_.z,
                      b.n[6] * rn_.x + b.n[7] * rn_.y + b.n[8] * rn_.z} * w;
        }
        out.p[v] = p;
        out.n[v] = q;
    }
    return true;
}

bool draw(std::vector<Vertex>& sink, size_t budget, const mesh_model::Mesh& mesh, const Rig& rig, const Skinned& s,
          uint32_t boneMask, const float* place) {
    if (!rig.ok || !boneMask || s.p.size() != rig.rest.size() || rig.corner.size() != size_t{mesh.triangles()} * 3)
        return false;
    uint32_t count = 0;
    for (uint32_t t = 0; t < mesh.triangles(); ++t)
        count += (boneMask >> rig.triangleBone[t]) & 1u;
    const size_t from = sink.size();
    if (from + size_t{count} * 3 > budget)
        return false;
    const Vec3 light = normalized(Vec3{.3f, 1.f, .25f});
    const bool uvs = mesh.hasUvs();
    const auto P = [&](Vec3 p) {
        return place ? Vec3{place[0] * p.x + place[1] * p.y + place[2] * p.z + place[3], place[4] * p.x + place[5] * p.y + place[6] * p.z + place[7],
                            place[8] * p.x + place[9] * p.y + place[10] * p.z + place[11]}
                     : p;
    };
    const auto N = [&](Vec3 n) { // a reflection (or rotation and scale): its own inverse transpose, up to scale
        return place ? Vec3{place[0] * n.x + place[1] * n.y + place[2] * n.z, place[4] * n.x + place[5] * n.y + place[6] * n.z,
                            place[8] * n.x + place[9] * n.y + place[10] * n.z}
                     : n;
    };
    // Each welded vertex placed and lit once (not once per corner: about 4x fewer), then the corners copied.
    thread_local std::vector<Vec3> placed;
    thread_local std::vector<float> lights;
    const size_t nv = s.p.size();
    const bool once = size_t{count} * 3 >= nv * 2; // most of the suit drawn (the mirror): else per corner (the arms)
    if (once) {
        placed.resize(nv);
        lights.resize(nv);
        for (size_t v = 0; v < nv; ++v) {
            placed[v] = P(s.p[v]);
            lights[v] = .32f + .68f * std::abs(dot(normalized(N(s.n[v])), light));
        }
    }
    sink.resize(from + size_t{count} * 3);
    Vertex* o = sink.data() + from;
    for (uint32_t t = 0; t < mesh.triangles(); ++t) {
        if (!((boneMask >> rig.triangleBone[t]) & 1u))
            continue;
        const int16_t im = uvs && t < mesh.image.size() ? mesh.image[t] : int16_t{-1};
        const int32_t at = im >= 0 && static_cast<size_t>(im) < mesh.images.size() ? mesh.images[static_cast<size_t>(im)].layer : -1;
        const float layer = at >= 0 ? static_cast<float>(at) : -1.f;
        const Vec3 tint = at >= 0 && t < mesh.tints.size() ? mesh.tints[t] : mesh.colors[t];
        for (uint32_t k = 0; k < 3; ++k) {
            const uint32_t v = rig.corner[t * 3 + k];
            const float u = uvs ? mesh.uvs[t * 6 + k * 2] : 0.f, w = uvs ? mesh.uvs[t * 6 + k * 2 + 1] : 0.f;
            const float lit = once ? lights[v] : .32f + .68f * std::abs(dot(normalized(N(s.n[v])), light));
            *o++ = Vertex{once ? placed[v] : P(s.p[v]), tint * lit, 1.f, {u, w, layer, 1.f}};
        }
    }
    for (size_t i = from; i < sink.size(); ++i)
        if (!ok(sink[i].position) || !ok(sink[i].color)) {
            sink.resize(from);
            return false;
        }
    return true;
}

} // namespace spidy::suit_wear
