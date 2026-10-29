// Version 0.0.73 HERO HANDS preview (suit_wear::HandProfile). Wears one suit with the game's own wear path
// (suit_wear::estimate, moves, skin: Touch grips, a standing player, both hands forward) in one pose and writes the
// forearms and hands (textured: each corner's uv and image) for tools/hero_hands073/render_hands.py, with the REAL
// palm's way (the OpenXR grip's +X on the left, -X on the right: where PALM BLAST fires) drawn as a green rod and
// the controller's aim (grip -Y, the grapple's aim axis) as a blue one.
//
//   spidy_hero_hands_preview <Models/> <out/>
//   env HANDS_SUIT  the suit folder (Suit_ironMan_1)
//       HANDS_POSE  fist | relaxed | open | repulsor (the trigger held while IRON MAN's repulsor charges)
//       HANDS_GRIP  held (the default: both hands forward, palms in) | raised (IRON MAN's repulsor pose: the
//                   hand up, the real palm facing ahead, fingers up) | twist (the forearm rolled, palm up)
//       HANDS_PROFILE  on (the hero's own hand profile, the default) | off (0.0.73 test 4: every hero alike)
#include "spidy/baton_grip.hpp"
#include "spidy/gun_grip.hpp"
#include "spidy/hero007_visual.hpp"
#include "spidy/mesh_model.hpp"
#include "spidy/repulsor.hpp"
#include "spidy/suit_wear.hpp"

#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <cstring>
#include <fstream>
#include <string>

using namespace spidy;
namespace mm = spidy::mesh_model;
namespace sw = spidy::suit_wear;

static void put(std::ofstream& f, Vec3 p, float u, float v, float img, Vec3 c) {
    const float a[9]{p.x, p.y, p.z, u, v, img, c.x, c.y, c.z};
    f.write(reinterpret_cast<const char*>(a), sizeof a);
}
// A thin square rod from a to b (untextured, colour c).
static void rod(std::ofstream& f, Vec3 a, Vec3 b, Vec3 c, float r) {
    const Vec3 d = normalized(b - a);
    const Vec3 u = normalized(cross(d, std::fabs(d.y) < .9f ? Vec3{0, 1, 0} : Vec3{1, 0, 0})), v = cross(d, u);
    const Vec3 o[4]{u * r, v * r, u * -r, v * -r};
    for (int k = 0; k < 4; ++k) {
        const Vec3 p0 = a + o[k], p1 = a + o[(k + 1) % 4], p2 = b + o[k], p3 = b + o[(k + 1) % 4];
        put(f, p0, 0, 0, -1, c), put(f, p1, 0, 0, -1, c), put(f, p2, 0, 0, -1, c);
        put(f, p1, 0, 0, -1, c), put(f, p3, 0, 0, -1, c), put(f, p2, 0, 0, -1, c);
    }
}

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: spidy_hero_hands_preview <Models/> <out/>\n");
        return 2;
    }
    const std::string root = argv[1], out = argv[2];
    const char* suitName = std::getenv("HANDS_SUIT") ? std::getenv("HANDS_SUIT") : "Suit_ironMan_1";
    const std::string pose = std::getenv("HANDS_POSE") ? std::getenv("HANDS_POSE") : "fist";
    const bool profileOn = !(std::getenv("HANDS_PROFILE") && std::strcmp(std::getenv("HANDS_PROFILE"), "off") == 0);
    const uint32_t suit = mm::suitSlot(suitName);
    mm::Library lib;
    lib.load(suit, root);
    const mm::Mesh* m = lib.model(suit);
    if (!m) {
        std::fprintf(stderr, "no %s under %s\n", suitName, root.c_str());
        return 1;
    }
    sw::Rig rig;
    if (!sw::makeRig(*m, rig, suitName)) {
        std::fprintf(stderr, "no rig\n");
        return 1;
    }
    if (std::getenv("HANDS_TURN") || std::getenv("HANDS_ROLL") || std::getenv("HANDS_PITCH")) { // a look at a change
        sw::HandProfile p = rig.hands;
        p.restTurnDeg = std::getenv("HANDS_TURN") ? static_cast<float>(std::atof(std::getenv("HANDS_TURN"))) : p.restTurnDeg;
        p.rollDeg = std::getenv("HANDS_ROLL") ? static_cast<float>(std::atof(std::getenv("HANDS_ROLL"))) : p.rollDeg;
        p.pitchDeg = std::getenv("HANDS_PITCH") ? static_cast<float>(std::atof(std::getenv("HANDS_PITCH"))) : p.pitchDeg;
        sw::applyHandProfile(rig, p);
    }
    const sw::HandProfile& prof = sw::handsOf(rig, profileOn);
    // A standing player, both fists forward (the forearms 11 degrees under level), the backs of the hands out.
    const float tilt = baton_grip::handleTiltDeg(ControllerKind::touch);
    const float b = (78.7f - tilt) * 3.14159265f / 180.f;
    const Quat q{std::sin(b / 2), 0, 0, std::cos(b / 2)};
    sw::Tracked t;
    t.heroHands = profileOn;
    t.eye = {0, 1.65f, 0};
    t.feet = {0, 0, 0};
    // the trigger, the grip (0 .. 1) and IRON MAN's repulsor charging, per pose
    const float trigger = pose == "fist" || pose == "repulsor" ? 1.f : pose == "relaxed" ? .3f : 0.f;
    const float squeeze = pose == "fist" ? 1.f : 0.f;
    const bool charging = pose == "repulsor";
    const std::string gripPose = std::getenv("HANDS_GRIP") ? std::getenv("HANDS_GRIP") : "held";
    Pose grip[2];
    Vec3 gx[2], gy[2], gz[2]; // the grip's +X, +Y, +Z in the world
    for (int h = 0; h < 2; ++h) {
        grip[h].position = {h == 0 ? -.2f : .2f, 1.22f, -.36f};
        grip[h].orientation = q;
        gx[h] = q.rotate({1, 0, 0}), gy[h] = q.rotate({0, 1, 0}), gz[h] = q.rotate({0, 0, 1});
        if (gripPose == "raised" || gripPose == "twist") {
            // the local palm (grip +X left, -X right) and the hand's way (grip -Y tilted toward -Z) sent to P and W
            const Vec3 pl{h == 0 ? 1.f : -1.f, 0, 0};
            const float a = tilt * 3.14159265f / 180.f;
            const Vec3 wl{0, -std::cos(a), -std::sin(a)};
            const Vec3 nl = cross(pl, wl);
            Vec3 P, W;
            if (gripPose == "raised") {
                P = {0, 0, -1}, W = {h == 0 ? .15f : -.15f, 1, -.1f};
                grip[h].position = {h == 0 ? -.22f : .22f, 1.42f, -.45f};
            } else {
                P = {0, 1, 0}, W = {0, -.2f, -1}; // palm up, the hand ahead
            }
            W = normalized(W - P * dot(W, P));
            const Vec3 N = cross(P, W);
            const auto map = [&](Vec3 v) { return P * dot(v, pl) + W * dot(v, wl) + N * dot(v, nl); };
            gx[h] = map({1, 0, 0}), gy[h] = map({0, 1, 0}), gz[h] = map({0, 0, 1});
        }
        t.hand[h] = true;
        t.grip[h] = grip[h].position;
        t.gripForward[h] = gz[h] * -1.f;
        t.gripX[h] = gx[h];
        t.gripUp[h] = gy[h];
        t.handleTiltDeg[h] = tilt;
        // IRON MAN (palmRaise): the raised hand drawn palm out along the real palm (game_xr does the same, eased)
        if (prof.palmRaise) {
            const Vec3 palm = sw::handBackOf(gx[h], h) * -1.f;
            repulsor::PalmFrame pf;
            if (sw::raisedPalm(palm, {0, 0, -1}, {0, 1, 0}, t.grip[h], t.eye) &&
                repulsor::palmFrame(palm, {0, 1, 0}, static_cast<unsigned>(h), pf))
                sw::blendGrip(t.gripForward[h], t.gripX[h], t.gripUp[h], t.handleTiltDeg[h], pf.forward, pf.x, pf.up, 1.f);
        }
        t.curl[h] = sw::handCurl(prof, trigger, squeeze, 0.f, false, charging);
        t.open[h] = sw::handOpen(prof, squeeze, false, charging);
    }
    sw::Skeleton live;
    sw::BoneMove mv[sw::bones];
    sw::Skinned s;
    if (!sw::estimate(rig, t, live) || !sw::moves(rig, live, mv) || !sw::skin(rig, mv, s)) {
        std::fprintf(stderr, "wear failed\n");
        return 1;
    }
    std::ofstream f(out + "hands.tri", std::ios::binary);
    const bool tex = m->hasUvs() && m->image.size() == m->triangles();
    for (uint32_t i = 0; i < m->triangles(); ++i) {
        const uint8_t bone = rig.triangleBone[i];
        if (!(bone == sw::boneHandL || bone == sw::boneHandR || bone == sw::boneForeL || bone == sw::boneForeR))
            continue;
        const Vec3 tint = m->tints.size() == m->triangles() ? m->tints[i] : Vec3{1, 1, 1};
        const float img = tex ? static_cast<float>(m->image[i]) : -1.f;
        const Vec3 c = img < 0 ? m->colors[i] : tint;
        for (int k = 0; k < 3; ++k)
            put(f, s.p[rig.corner[i * 3 + k]], tex ? m->uvs[i * 6 + k * 2] : 0.f, tex ? m->uvs[i * 6 + k * 2 + 1] : 0.f, img, c);
    }
    for (int h = 0; h < 2; ++h) {
        const Vec3 palm = gx[h] * (h == 0 ? 1.f : -1.f), aim = gy[h] * -1.f;
        rod(f, grip[h].position, grip[h].position + palm * .16f, {.1f, .85f, .1f}, .003f);
        rod(f, grip[h].position, grip[h].position + aim * .22f, {.15f, .3f, .95f}, .0025f);
    }
    // 0.0.73 test 5 CLAW LOOK: HANDS_CLAWS=steel (ON: hero007::appendClawSteel) | old (OFF: test 4's appendClawModel)
    // draws Models/Claws/{Left,Right} on each fist as the game's lookClaws item (gun_grip::held's frame, fully out).
    // HANDS_CLAWS=bone (0.0.73 test 5 CLAW BONE: hero007::appendClawBone at suit_wear::clawPlace, the claws locked to
    // the drawn hand's bone; HANDS_EXT the extension, 1 out): drawn, and measured against the skinned hand (its hand
    // and forearm triangles): each root's generalized winding number (> .5 inside) and depth under the skin (mm), how
    // many of each claw's root-end corners (within 1 cm of its root) are inside, where the blade leaves the skin
    // (mm along the claw from its root) and that exit's place against the knuckle line (mm, + past it).
    if (const char* claws = std::getenv("HANDS_CLAWS"); claws && std::strcmp(claws, "bone") == 0) {
        std::vector<std::array<Vec3, 3>> skinTris;
        for (uint32_t i = 0; i < m->triangles(); ++i) {
            const uint8_t bone = rig.triangleBone[i];
            if (bone == sw::boneHandL || bone == sw::boneHandR || bone == sw::boneForeL || bone == sw::boneForeR)
                skinTris.push_back({s.p[rig.corner[i * 3]], s.p[rig.corner[i * 3 + 1]], s.p[rig.corner[i * 3 + 2]]});
        }
        const auto winding = [&](Vec3 p) {
            double w = 0;
            for (const auto& tr : skinTris) {
                const Vec3 a = tr[0] - p, b = tr[1] - p, c = tr[2] - p;
                const double la = length(a), lb = length(b), lc = length(c);
                const double num = dot(a, cross(b, c));
                const double den = la * lb * lc + dot(a, b) * lc + dot(b, c) * la + dot(c, a) * lb;
                w += 2.0 * std::atan2(num, den);
            }
            return std::abs(w / (4.0 * 3.14159265358979));
        };
        const auto closest = [](Vec3 p, Vec3 a, Vec3 b, Vec3 c) { // Ericson's closest point on a triangle
            const Vec3 ab = b - a, ac = c - a, ap = p - a;
            const float d1 = dot(ab, ap), d2 = dot(ac, ap);
            if (d1 <= 0 && d2 <= 0) return a;
            const Vec3 bp = p - b;
            const float d3 = dot(ab, bp), d4 = dot(ac, bp);
            if (d3 >= 0 && d4 <= d3) return b;
            const float vc = d1 * d4 - d3 * d2;
            if (vc <= 0 && d1 >= 0 && d3 <= 0) return a + ab * (d1 / (d1 - d3));
            const Vec3 cp = p - c;
            const float d5 = dot(ab, cp), d6 = dot(ac, cp);
            if (d6 >= 0 && d5 <= d6) return c;
            const float vb = d5 * d2 - d1 * d6;
            if (vb <= 0 && d2 >= 0 && d6 <= 0) return a + ac * (d2 / (d2 - d6));
            const float va = d3 * d6 - d5 * d4;
            if (va <= 0 && (d4 - d3) >= 0 && (d5 - d6) >= 0) return b + (c - b) * ((d4 - d3) / ((d4 - d3) + (d5 - d6)));
            const float den = 1.f / (va + vb + vc);
            return a + ab * (vb * den) + ac * (vc * den);
        };
        const auto skinGap = [&](Vec3 p) {
            float best = 1e9f;
            for (const auto& tr : skinTris)
                best = std::min(best, length(p - closest(p, tr[0], tr[1], tr[2])));
            return best;
        };
        // enclosed: at least 90% of 64 evenly spread rays from p meet the skin within 25 cm, so the point is hidden in the
        // hand from (nearly) every side, even where a fist's curled fingertips pass into its palm (where the winding
        // number is not 1); a few rays may slip out through seams of the suits' separate mesh pieces
        std::vector<Vec3> rays;
        for (int i = 0; i < 64; ++i) { // a Fibonacci sphere
            const float z = 1.f - (i + .5f) * 2.f / 64.f, r = std::sqrt(std::max(0.f, 1.f - z * z)), a = 2.39996323f * static_cast<float>(i);
            rays.push_back({r * std::cos(a), r * std::sin(a), z});
        }
        const auto enclosed = [&](Vec3 p) {
            int hits = 0;
            for (const Vec3 d : rays) {
                for (const auto& tr : skinTris) { // Moller-Trumbore
                    const Vec3 e1 = tr[1] - tr[0], e2 = tr[2] - tr[0], q = cross(d, e2);
                    const float det = dot(e1, q);
                    if (std::abs(det) < 1e-12f)
                        continue;
                    const float inv = 1.f / det;
                    const Vec3 o = p - tr[0];
                    const float u = dot(o, q) * inv;
                    if (u < 0 || u > 1)
                        continue;
                    const Vec3 r = cross(o, e1);
                    const float vv = dot(d, r) * inv;
                    if (vv < 0 || u + vv > 1)
                        continue;
                    const float tt = dot(e2, r) * inv;
                    if (tt > 0 && tt < .25f) {
                        ++hits;
                        break;
                    }
                }
            }
            return hits >= 58;
        };
        const float ext = std::getenv("HANDS_EXT") ? static_cast<float>(std::atof(std::getenv("HANDS_EXT"))) : 1.f;
        std::ofstream mfile(out + "claws.txt");
        for (int h = 0; h < 2; ++h) {
            const uint32_t slot = h ? mm::slotClawsRight : mm::slotClawsLeft;
            lib.load(slot, root);
            const mm::Mesh* cm = lib.model(slot);
            hero007::ClawBone cb;
            // 0.0.73 test 6 CLAW LEVEL: HANDS_CLAW_LEVEL=on the level way (the game's default), else test 5's tilt
            const bool level = std::getenv("HANDS_CLAW_LEVEL") && std::strcmp(std::getenv("HANDS_CLAW_LEVEL"), "on") == 0;
            cb.ok = cm && sw::clawPlace(rig, mv, h, cb.root, cb.way, cb.back, level);
            hero007::ClawParts parts;
            if (!cb.ok || !hero007::clawParts(*cm, parts)) {
                std::fprintf(stderr, "no bone claws %d\n", h);
                continue;
            }
            std::vector<Vertex> v;
            const bool drawn = hero007::appendClawBone(v, ext, cb, *cm, t.eye);
            std::printf("bone claws %s %s: %zu triangles (ext %.2f)\n", h ? "R" : "L", drawn ? "drawn" : "NOT drawn", v.size() / 3, ext);
            for (const Vertex& x : v)
                put(f, x.position, 0, 0, -1, x.color);
            const sw::BoneMove& b = mv[h ? sw::boneHandR : sw::boneHandL];
            const auto apply = [&](Vec3 d) {
                return Vec3{b.m[0] * d.x + b.m[1] * d.y + b.m[2] * d.z, b.m[3] * d.x + b.m[4] * d.y + b.m[5] * d.z,
                            b.m[6] * d.x + b.m[7] * d.y + b.m[8] * d.z};
            };
            const Vec3 handWay = normalized(apply(b.y0)), knuckle = b.a1 + apply(b.y0 * rig.clawKnuckle[h]);
            std::printf("%s %s knuckle line %.1f mm from the wrist (handReach %.1f)\n", suitName, h ? "R" : "L", rig.clawKnuckle[h] * 1000.f, b.knuckle * 1000.f);
            if (std::getenv("CLAW_DEBUG3"))
                for (int k = 0; k < 3; ++k) { // the posed triangles nearest each root, and their rest distance
                    std::vector<std::pair<float, uint32_t>> near;
                    for (uint32_t i = 0; i < m->triangles(); ++i) {
                        const uint8_t bone = rig.triangleBone[i];
                        if (!(bone == sw::boneHandL || bone == sw::boneHandR || bone == sw::boneForeL || bone == sw::boneForeR))
                            continue;
                        const Vec3 a = s.p[rig.corner[i * 3]], bb = s.p[rig.corner[i * 3 + 1]], c = s.p[rig.corner[i * 3 + 2]];
                        near.push_back({length(cb.root[k] - closest(cb.root[k], a, bb, c)), i});
                    }
                    std::sort(near.begin(), near.end());
                    for (int q = 0; q < 4 && q < static_cast<int>(near.size()); ++q) {
                        const uint32_t i = near[q].second;
                        const Vec3 a = rig.rest[rig.corner[i * 3]], bb = rig.rest[rig.corner[i * 3 + 1]], c = rig.rest[rig.corner[i * 3 + 2]];
                        std::printf("  claw %d tri %u posed %.1f mm rest %.1f mm; corner moves %.1f %.1f %.1f mm off rigid\n", k, i, near[q].first * 1000.f,
                                    length(rig.clawRoot[h][k] - closest(rig.clawRoot[h][k], a, bb, c)) * 1000.f,
                                    length(s.p[rig.corner[i * 3]] - (b.a1 + apply(a - b.a0))) * 1000.f,
                                    length(s.p[rig.corner[i * 3 + 1]] - (b.a1 + apply(bb - b.a0))) * 1000.f,
                                    length(s.p[rig.corner[i * 3 + 2]] - (b.a1 + apply(c - b.a0))) * 1000.f);
                    }
                }
            if (std::getenv("CLAW_DEBUG"))
                for (int k = 0; k < 3; ++k) { // rest winding (same triangles) and posed
                    double wr = 0;
                    const Vec3 p = rig.clawRoot[h][k];
                    for (uint32_t i = 0; i < m->triangles(); ++i) {
                        const uint8_t bone = rig.triangleBone[i];
                        if (!(bone == sw::boneHandL || bone == sw::boneHandR || bone == sw::boneForeL || bone == sw::boneForeR))
                            continue;
                        const Vec3 a = rig.rest[rig.corner[i * 3]] - p, bb = rig.rest[rig.corner[i * 3 + 1]] - p, c = rig.rest[rig.corner[i * 3 + 2]] - p;
                        const double la = length(a), lb = length(bb), lc = length(c);
                        wr += 2.0 * std::atan2(static_cast<double>(dot(a, cross(bb, c))), la * lb * lc + dot(a, bb) * lc + dot(bb, c) * la + dot(c, a) * lb);
                    }
                    std::printf(" claw %d rest winding %.3f posed %.3f\n", k, wr / (4 * 3.14159265), winding(cb.root[k]));
                }
            if (std::getenv("CLAW_DEBUG2"))
                for (int k = 0; k < 3; ++k) { // the skin near each root against the hand bone's rigid move
                    const Vec3 r0 = rig.clawRoot[h][k];
                    for (size_t v = 0; v < rig.rest.size(); ++v) {
                        if (length(rig.rest[v] - r0) > .025f)
                            continue;
                        uint32_t wh = 0;
                        for (int c = 0; c < 4; ++c)
                            if ((rig.joint[v * 4 + c] & 15) == (h ? sw::boneHandR : sw::boneHandL))
                                wh += rig.weight[v * 4 + c];
                        const Vec3 rigid = b.a1 + apply(rig.rest[v] - b.a0);
                        std::printf("  claw %d vert %zu at %.1f mm: hand weight %u, off rigid %.1f mm\n", k, v, length(rig.rest[v] - r0) * 1000.f, wh,
                                    length(s.p[v] - rigid) * 1000.f);
                    }
                }
            for (int k = 0; k < 3; ++k) {
                const double w = winding(cb.root[k]);
                const float depth = skinGap(cb.root[k]);
                int capIn = 0, capN = 0, capHid = 0;
                Vec3 tipW{};
                float far = -1;
                for (uint32_t c = 0; c < cm->triangles() * 3; ++c) {
                    if (parts.part[c / 3] != k)
                        continue;
                    Vec3 wp;
                    hero007::clawBonePoint(parts, cb, k, cm->corners[c], ext, wp);
                    const float along = dot(cm->corners[c] - parts.base[k], parts.axis[k]);
                    if (along < .01f) {
                        ++capN;
                        capIn += winding(wp) > .5;
                        capHid += enclosed(wp);
                    }
                    if (along > far)
                        far = along, tipW = wp;
                }
                const Vec3 axisW = normalized(tipW - cb.root[k]);
                float exitAt = -1;
                for (float d = 0; d < .2f; d += .0005f)
                    if (winding(cb.root[k] + axisW * d) < .5) {
                        exitAt = d;
                        break;
                    }
                float hideTo = -1; // the blade hidden from every side up to here
                for (float d = 0; d < .2f; d += .0005f)
                    if (!enclosed(cb.root[k] + axisW * d)) {
                        hideTo = d;
                        break;
                    }
                const Vec3 ex = cb.root[k] + axisW * exitAt;
                const float hideVsKnuckle = dot(cb.root[k] + axisW * hideTo - knuckle, handWay);
                const float vsKnuckle = dot(ex - knuckle, handWay);
                char line[512];
                std::snprintf(line, sizeof line,
                              "%s %s claw %d: root winding %.3f %s, depth %.1f mm; root-end corners inside %d/%d; "
                              "blade exits %.1f mm out (skin gap there %.2f mm), %+.1f mm vs knuckle line; root %s, root-end corners "
                              "hidden %d/%d, blade hidden to %.1f mm (%+.1f mm vs knuckle line)\n",
                              suitName, h ? "R" : "L", k, w, w > .5 ? "INSIDE" : "OUTSIDE", depth * 1000.f, capIn, capN, exitAt * 1000.f,
                              skinGap(ex) * 1000.f, vsKnuckle * 1000.f, enclosed(cb.root[k]) ? "HIDDEN" : "SEEN", capHid, capN,
                              hideTo * 1000.f, hideVsKnuckle * 1000.f);
                std::fputs(line, stdout);
                mfile << line;
            }
        }
    } else if (const char* claws = std::getenv("HANDS_CLAWS")) {
        const bool steel = std::strcmp(claws, "steel") == 0;
        for (int h = 0; h < 2; ++h) {
            const uint32_t slot = h ? mm::slotClawsRight : mm::slotClawsLeft;
            lib.load(slot, root);
            const mm::Mesh* cm = lib.model(slot);
            Pose g = grip[h];
            const gun_grip::Held held = gun_grip::held(g, static_cast<unsigned>(h), ControllerKind::touch);
            if (!cm || !held.ok) {
                std::fprintf(stderr, "no claws %d\n", h);
                continue;
            }
            hero007::Item it{hero007::lookClaws, held.at, held.forward, held.up};
            it.spin = 1.f;
            it.side = static_cast<uint32_t>(h);
            std::vector<Vertex> v;
            const bool drawn = steel ? hero007::appendClawSteel(v, it, *cm, t.eye) : hero007::appendClawModel(v, it, *cm, t.eye);
            std::printf("claws %s %s: %zu triangles\n", h ? "R" : "L", drawn ? "drawn" : "NOT drawn", v.size() / 3);
            for (const Vertex& x : v)
                put(f, x.position, 0, 0, -1, x.color);
            // the "before" measure (the grip's claws): the base's winding in the drawn hand and its gap to the skin,
            // and how many of the claws' corners within 1 cm of the base are inside the hand
            Vec3 base{}, bx{}, by{}, bf{};
            if (steel && drawn && hero007::clawSteelFrame(it, base, bx, by, bf)) {
                const uint8_t hb = h ? sw::boneHandR : sw::boneHandL;
                std::vector<std::array<Vec3, 3>> hand;
                for (uint32_t i = 0; i < m->triangles(); ++i)
                    if (rig.triangleBone[i] == hb || rig.triangleBone[i] == (h ? sw::boneForeR : sw::boneForeL))
                        hand.push_back({s.p[rig.corner[i * 3]], s.p[rig.corner[i * 3 + 1]], s.p[rig.corner[i * 3 + 2]]});
                const auto wind = [&](Vec3 p) {
                    double w = 0;
                    for (const auto& tr : hand) {
                        const Vec3 a = tr[0] - p, b = tr[1] - p, c = tr[2] - p;
                        const double la = length(a), lb = length(b), lc = length(c);
                        w += 2.0 * std::atan2(dot(a, cross(b, c)), la * lb * lc + dot(a, b) * lc + dot(b, c) * la + dot(c, a) * lb);
                    }
                    return std::abs(w / (4.0 * 3.14159265358979));
                };
                float gap = 1e9f;
                for (const auto& tr : hand)
                    for (const Vec3& q : tr)
                        gap = std::min(gap, length(q - base));
                int nearBase = 0, inside = 0;
                for (const Vertex& x : v)
                    if (length(x.position - base) < .01f + .03f) {
                        ++nearBase;
                        inside += wind(x.position) > .5 ? 1 : 0;
                    }
                char line[256];
                std::snprintf(line, sizeof line, "%s %s grip claws: base winding %.3f %s, nearest hand vertex %.1f mm; corners within 4 cm of the base inside the hand %d/%d\n",
                              suitName, h ? "R" : "L", wind(base), wind(base) > .5 ? "INSIDE" : "OUTSIDE", gap * 1000, inside, nearBase);
                std::fputs(line, stdout);
                std::ofstream(out + "claws.txt", std::ios::app) << line;
            }
        }
    }
    for (size_t k = 0; k < m->images.size(); ++k) {
        std::ofstream g(out + "img_" + std::to_string(k) + ".bin", std::ios::binary);
        g.write(reinterpret_cast<const char*>(m->images[k].bytes.data()), static_cast<std::streamsize>(m->images[k].bytes.size()));
    }
    // The real palm (written for render_hands.py / the repulsor-disc measure) and the curl used.
    std::ofstream info(out + "grip.txt");
    for (int h = 0; h < 2; ++h) {
        const Vec3 palm = gx[h] * (h == 0 ? 1.f : -1.f), aim = gy[h] * -1.f;
        info << grip[h].position.x << ' ' << grip[h].position.y << ' ' << grip[h].position.z << ' ' << palm.x << ' ' << palm.y << ' '
             << palm.z << ' ' << aim.x << ' ' << aim.y << ' ' << aim.z << '\n';
        std::printf("%s %s %s %s %s: curl %.2f, images %zu\n", suitName, pose.c_str(), gripPose.c_str(), profileOn ? "profile" : "shared",
                    h ? "R" : "L", t.curl[h], m->images.size());
    }
    return 0;
}
