// 0.0.78 WRIST TWIST preview and measure (the owner: "no candy-wrapper wrist collapse ... render the hand rolled -90,
// 0, +90 and 180 degrees; the wrist cross-section must stay at least ~85% of rest"). Wears each suit with the game's
// own wear path (suit_wear::estimate, moves, skin: Touch grips, a standing player, both hands held forward), rolls
// both grips about the hand's way by each angle, with the profile's twistChain ON and OFF, and measures each forearm's
// cross-section near the wrist: its forearm+hand triangles cut by planes square to the forearm (at 75, 85, 92,
// 97 and 100 % of the way elbow -> wrist), each cut's convex hull area (within 7 cm of the forearm), against the
// same cut on the rest model (scaled by the wear's scale). Writes each pose's forearms and hands for
// tools/suits/render_wrist.py.
//
//   spidy_wrist_twist_preview <Models/> <out/> <Suit_folder>...
#include "spidy/baton_grip.hpp"
#include "spidy/mesh_model.hpp"
#include "spidy/suit_wear.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <string>
#include <sys/stat.h>
#include <vector>

using namespace spidy;
namespace mm = spidy::mesh_model;
namespace sw = spidy::suit_wear;

static Vec3 turn(Vec3 v, Vec3 k, float a) { // Rodrigues
    const float c = std::cos(a), s = std::sin(a);
    return v * c + cross(k, v) * s + k * (dot(k, v) * (1 - c));
}
static double hullArea(std::vector<std::pair<double, double>> p) {
    if (p.size() < 3)
        return 0;
    std::sort(p.begin(), p.end());
    std::vector<std::pair<double, double>> h(2 * p.size());
    size_t k = 0;
    const auto cr = [](auto o, auto a, auto b) { return (a.first - o.first) * (b.second - o.second) - (a.second - o.second) * (b.first - o.first); };
    for (size_t i = 0; i < p.size(); ++i) {
        while (k >= 2 && cr(h[k - 2], h[k - 1], p[i]) <= 0) --k;
        h[k++] = p[i];
    }
    for (size_t i = p.size() - 1, t = k + 1; i > 0; --i) {
        while (k >= t && cr(h[k - 2], h[k - 1], p[i - 1]) <= 0) --k;
        h[k++] = p[i - 1];
    }
    double a = 0;
    for (size_t i = 0; i + 1 < k; ++i)
        a += h[i].first * h[i + 1].second - h[i + 1].first * h[i].second;
    return std::abs(a) / 2;
}
static double sliceArea(const std::vector<Vec3>& pts, Vec3 e, Vec3 axis) {
    const Vec3 u = normalized(cross(axis, std::abs(axis.y) < .9f ? Vec3{0, 1, 0} : Vec3{1, 0, 0})), v = cross(axis, u);
    std::vector<std::pair<double, double>> q;
    for (const Vec3& p : pts)
        q.push_back({dot(p - e, u), dot(p - e, v)});
    return hullArea(q);
}
// The cross-section of triangles `tris` (3 points each) cut by the plane square to `axis` at `at`: the convex hull
// area of the cut's points within `radius` of the axis (the forearm, not a nearby part).
static double cutArea(const std::vector<Vec3>& tris, Vec3 e, Vec3 axis, float at, float radius) {
    std::vector<Vec3> pts;
    for (size_t i = 0; i + 2 < tris.size(); i += 3)
        for (int k = 0; k < 3; ++k) {
            const Vec3 a = tris[i + k], b = tris[i + (k + 1) % 3];
            const float da = dot(a - e, axis) - at, db = dot(b - e, axis) - at;
            if ((da < 0) == (db < 0) || da == db) continue;
            const Vec3 p = a + (b - a) * (da / (da - db));
            const Vec3 d = p - e;
            if (length(d - axis * dot(d, axis)) < radius) pts.push_back(p);
        }
    return sliceArea(pts, e, axis);
}
static void put(std::ofstream& f, Vec3 p, float u, float v, float img, Vec3 c) {
    const float a[9]{p.x, p.y, p.z, u, v, img, c.x, c.y, c.z};
    f.write(reinterpret_cast<const char*>(a), sizeof a);
}

int main(int argc, char** argv) {
    if (argc < 4) {
        std::fprintf(stderr, "usage: spidy_wrist_twist_preview <Models/> <out/> <Suit_folder>...\n");
        return 2;
    }
    const std::string root = argv[1], out = argv[2];
    const float slices[] = {.75f, .85f, .92f, .97f, 1.f};
    const int angles[] = {-90, 0, 90, 180};
    int bad = 0;
    for (int a = 3; a < argc; ++a) {
        const char* suitName = argv[a];
        const uint32_t suit = mm::suitSlot(suitName);
        mm::Library lib;
        lib.load(suit, root);
        const mm::Mesh* m = lib.model(suit);
        sw::Rig rig;
        if (!m || !sw::makeRig(*m, rig, suitName)) {
            std::printf("%s: no model or rig\n", suitName);
            ++bad;
            continue;
        }
        const bool own = rig.hands.twistChain;
        // each side's forearm+hand triangles (Rig::triangleBone)
        std::vector<uint32_t> tri[2];
        for (size_t tr = 0; tr < rig.triangleBone.size(); ++tr)
            for (int side = 0; side < 2; ++side) {
                const uint8_t b = rig.triangleBone[tr];
                if (b == (side ? sw::boneForeR : sw::boneForeL) || b == (side ? sw::boneHandR : sw::boneHandL))
                    tri[side].push_back(static_cast<uint32_t>(tr));
            }
        // 5.5 cm round the forearm axis (a wrist is about 4.5 cm round it): at 7 cm the Punisher's holster box beside
        // his rest right wrist counted as wrist (rest R 66 vs L 57 cm2 at u .97) and its 'loss' read as a pinch
        const float radius = .055f * rig.height / 1.8f;
        const auto area = [&](int side, const std::vector<Vec3>& P, Vec3 e, Vec3 w, float u) {
            std::vector<Vec3> T;
            for (uint32_t tr : tri[side])
                for (int c = 0; c < 3; ++c) T.push_back(P[rig.corner[tr * 3 + c]]);
            return cutArea(T, e, normalized(w - e), u * length(w - e), radius);
        };
        double rest[2][5];
        for (int side = 0; side < 2; ++side) {
            std::printf("%s %c rest cross-sections (cm2):", suitName, side ? 'R' : 'L');
            for (int k = 0; k < 5; ++k) {
                rest[side][k] = area(side, rig.rest, rig.skeleton.at[side ? sw::elbowR : sw::elbowL], rig.skeleton.at[side ? sw::wristR : sw::wristL], slices[k]);
                std::printf(" %.1f", rest[side][k] * 1e4);
            }
            std::printf("  (%zu triangles)\n", tri[side].size());
        }
        for (int twist = 0; twist < 2; ++twist) {
            rig.hands.twistChain = twist != 0;
            for (int deg : angles) {
                const float tilt = baton_grip::handleTiltDeg(ControllerKind::touch);
                const float b = (78.7f - tilt) * 3.14159265f / 180.f;
                const Quat q{std::sin(b / 2), 0, 0, std::cos(b / 2)};
                sw::Tracked t;
                t.heroHands = true;
                t.eye = {0, 1.65f, 0};
                Vec3 gpos[2], palm[2], aim[2];
                for (int h = 0; h < 2; ++h) {
                    const Vec3 gx = q.rotate({1, 0, 0}), gy = q.rotate({0, 1, 0}), gz = q.rotate({0, 0, 1});
                    const Vec3 W = normalized(sw::handWay(gz * -1.f, gy, tilt));
                    const float r = deg * 3.14159265f / 180.f * (h ? 1.f : -1.f); // + : each hand's back toward its thumb
                    t.hand[h] = true;
                    t.grip[h] = gpos[h] = {h == 0 ? -.2f : .2f, 1.22f, -.36f};
                    t.gripForward[h] = turn(gz, W, r) * -1.f;
                    t.gripX[h] = turn(gx, W, r);
                    t.gripUp[h] = turn(gy, W, r);
                    t.handleTiltDeg[h] = tilt;
                    palm[h] = t.gripX[h] * (h == 0 ? 1.f : -1.f), aim[h] = t.gripUp[h] * -1.f;
                }
                sw::Skeleton live;
                sw::BoneMove mv[sw::bones];
                sw::Skinned s;
                if (!sw::estimate(rig, t, live) || !sw::moves(rig, live, mv) || !sw::skin(rig, mv, s)) {
                    std::printf("%s: wear failed\n", suitName);
                    ++bad;
                    continue;
                }
                const float sc = length(live.at[sw::neck] - live.at[sw::pelvis]) / length(rig.skeleton.at[sw::neck] - rig.skeleton.at[sw::pelvis]);
                std::printf("%-22s twist %-3s %+4d", suitName, twist ? "ON" : "OFF", deg);
                for (int side = 0; side < 2; ++side) {
                    const Vec3 e1 = live.at[side ? sw::elbowR : sw::elbowL], w1 = live.at[side ? sw::wristR : sw::wristL];
                    double worst = 1e9;
                    std::printf("  %c", side ? 'R' : 'L');
                    for (int k = 0; k < 5; ++k) {
                        const double r0 = rest[side][k] * sc * sc, r1 = area(side, s.p, e1, w1, slices[k]);
                        const double pc = r0 > 1e-9 ? 100 * r1 / r0 : 0;
                        worst = std::min(worst, pc);
                        std::printf(" %3.0f", pc);
                    }
                    std::printf(" min %3.0f%%", worst);
                }
                std::printf("%s\n", own ? "  (the suit's own profile has it)" : "");
                // the pose's forearms and hands
                char dir[512];
                std::snprintf(dir, sizeof dir, "%s%s_%s_%d/", out.c_str(), suitName, twist ? "on" : "off", deg);
                ::mkdir(dir, 0755);
                std::ofstream f(std::string(dir) + "hands.tri", std::ios::binary);
                const bool tex = m->hasUvs() && m->image.size() == m->triangles();
                for (uint32_t i = 0; i < m->triangles(); ++i) {
                    const uint8_t bn = rig.triangleBone[i];
                    if (!(bn == sw::boneHandL || bn == sw::boneHandR || bn == sw::boneForeL || bn == sw::boneForeR)) continue;
                    const Vec3 tint = m->tints.size() == m->triangles() ? m->tints[i] : Vec3{1, 1, 1};
                    const float img = tex ? static_cast<float>(m->image[i]) : -1.f;
                    const Vec3 c = img < 0 ? m->colors[i] : tint;
                    for (int k = 0; k < 3; ++k)
                        put(f, s.p[rig.corner[i * 3 + k]], tex ? m->uvs[i * 6 + k * 2] : 0.f, tex ? m->uvs[i * 6 + k * 2 + 1] : 0.f, img, c);
                }
                for (size_t k = 0; k < m->images.size(); ++k) {
                    std::ofstream g(std::string(dir) + "img_" + std::to_string(k) + ".bin", std::ios::binary);
                    g.write(reinterpret_cast<const char*>(m->images[k].bytes.data()), static_cast<std::streamsize>(m->images[k].bytes.size()));
                }
                std::ofstream info(std::string(dir) + "arm.txt");
                for (int h = 0; h < 2; ++h) {
                    const Vec3 e1 = live.at[h ? sw::elbowR : sw::elbowL], w1 = live.at[h ? sw::wristR : sw::wristL];
                    info << e1.x << ' ' << e1.y << ' ' << e1.z << ' ' << w1.x << ' ' << w1.y << ' ' << w1.z << ' ' << palm[h].x << ' ' << palm[h].y
                         << ' ' << palm[h].z << '\n';
                }
            }
        }
    }
    return bad;
}
