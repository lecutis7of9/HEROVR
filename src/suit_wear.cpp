// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
// Build 9 SUITS: suit_wear.hpp.
#include "spidy/suit_wear.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <unordered_map>

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
} // namespace

bool makeRig(const mesh_model::Mesh& mesh, Rig& out) {
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
        if (t.hand[side] && ok(t.grip[side]) && length(fwd) > .5f && length(gx) > .5f) {
            // OpenXR grips: x is the palm's normal; the back of the left hand is -x, of the right +x
            const Vec3 back = side ? gx : gx * -1.f;
            const Vec3 wrist = t.grip[side] - fwd * (.07f * s) + back * (.015f * s);
            const Vec3 out_ = side ? fx * -1.f : fx;
            out.at[wr] = wrist;
            out.at[tip] = wrist + fwd * hand;
            out.at[el] = bend(out.at[sh], wrist, l1, l2, up * -.8f + out_ * .45f - fz * .3f);
            out.handBack[side] = back;
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
            const Vec3 d = r - b.a0;
            p += (b.a1 + Vec3{b.m[0] * d.x + b.m[1] * d.y + b.m[2] * d.z, b.m[3] * d.x + b.m[4] * d.y + b.m[5] * d.z,
                              b.m[6] * d.x + b.m[7] * d.y + b.m[8] * d.z}) * w;
            q += Vec3{b.n[0] * rn.x + b.n[1] * rn.y + b.n[2] * rn.z, b.n[3] * rn.x + b.n[4] * rn.y + b.n[5] * rn.z,
                      b.n[6] * rn.x + b.n[7] * rn.y + b.n[8] * rn.z} * w;
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
    sink.reserve(from + size_t{count} * 3);
    for (uint32_t t = 0; t < mesh.triangles(); ++t) {
        if (!((boneMask >> rig.triangleBone[t]) & 1u))
            continue;
        const int16_t im = uvs && t < mesh.image.size() ? mesh.image[t] : int16_t{-1};
        const int32_t at = im >= 0 && static_cast<size_t>(im) < mesh.images.size() ? mesh.images[static_cast<size_t>(im)].layer : -1;
        const float layer = at >= 0 ? static_cast<float>(at) : -1.f;
        const Vec3 tint = at >= 0 && t < mesh.tints.size() ? mesh.tints[t] : mesh.colors[t];
        for (uint32_t k = 0; k < 3; ++k) {
            const uint32_t v = rig.corner[t * 3 + k];
            const Vec3 n = normalized(N(s.n[v]));
            const float lit = .32f + .68f * std::abs(dot(n, light));
            const float u = uvs ? mesh.uvs[t * 6 + k * 2] : 0.f, w = uvs ? mesh.uvs[t * 6 + k * 2 + 1] : 0.f;
            sink.push_back(Vertex{P(s.p[v]), tint * lit, 1.f, {u, w, layer, 1.f}});
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
