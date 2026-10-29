// Version 0.0.73 test 3 DAREDEVIL BATON SEAT + TIP preview (dd_baton073.hpp). Wears a DAREDEVIL suit with the
// game's own wear path (suit_wear::estimate, moves, skin: Touch grips, the fists closed) and puts the baton
// model in each fist the test 3 way (baton_grip::shaft) and the new way (dd_baton073::seated), then a grapple
// with the silver tip flown on its rope. Writes each scene's triangles (x y z r g b a corner, float32) for
// tools/dd_baton073/render_dd_baton.py and prints the clipping measure: the fist's vertices inside the baton
// (closer to its axis than its radius, between its butt and tip) and how much of the fist hugs the handle.
//
//   spidy_dd_baton_preview <Models/> <out/>
#include "spidy/baton_grip.hpp"
#include "spidy/dd_baton073.hpp"
#include "spidy/hero007_visual.hpp"
#include "spidy/mesh_model.hpp"
#include "spidy/suit_wear.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

using namespace spidy;
namespace mm = spidy::mesh_model;
namespace sw = spidy::suit_wear;
namespace db = spidy::dd_baton073;

struct Tri {
    Vec3 p[3];
    Vec3 c;
};
static void put(std::ofstream& f, const Vec3& p, const Vec3& c) {
    const float v[6]{p.x, p.y, p.z, c.x, c.y, c.z};
    f.write(reinterpret_cast<const char*>(v), sizeof v);
}
static float segDist(Vec3 p, Vec3 a, Vec3 b, float& along) {
    const Vec3 d = b - a;
    const float l2 = dot(d, d);
    along = l2 > 0 ? dot(p - a, d) / l2 : 0.f;
    const float t = std::fmin(1.f, std::fmax(0.f, along));
    return length(p - (a + d * t));
}

// The clipping that shows: the fist's outward skin (its normal away from the handle's axis) inside the baton -
// the baton's surface outside the hand there (through the back of the hand, a knuckle or a finger's back).
static int poke(const std::vector<Vec3>& p, const std::vector<Vec3>& n, Vec3 butt, Vec3 seam) {
    int k = 0;
    for (size_t i = 0; i < p.size(); ++i) {
        float along = 0;
        const float d = segDist(p[i], butt, seam, along);
        if (!(along > 0 && along < 1) || d > hero007::batonDiameter * .5f - .001f)
            continue;
        const Vec3 foot = butt + (seam - butt) * along, radial = normalized(p[i] - foot);
        if (dot(n[i], radial) > .3f)
            ++k;
    }
    return k;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: spidy_dd_baton_preview <Models/> <out/>\n");
        return 2;
    }
    const std::string root = argv[1], out = argv[2];
    mm::Library lib;
    const char* suitName = std::getenv("DD_SUIT") ? std::getenv("DD_SUIT") : "Suit_daredevil_1";
    const uint32_t suit = mm::suitSlot(suitName);
    lib.load(suit, root);
    lib.load(mm::slotBatonLeft, root);
    lib.load(mm::slotBatonRight, root);
    const mm::Mesh* m = lib.model(suit);
    const mm::Mesh* baton[2]{lib.model(mm::slotBatonLeft), lib.model(mm::slotBatonRight)};
    if (!m || !baton[0] || !baton[1]) {
        std::fprintf(stderr, "models missing under %s\n", root.c_str());
        return 1;
    }
    sw::Rig rig;
    if (!sw::makeRig(*m, rig, suitName)) {
        std::fprintf(stderr, "no rig\n");
        return 1;
    }
    // A standing player, both fists forward (the forearms 11 degrees under level), the backs of the hands out.
    const float tilt = baton_grip::handleTiltDeg(ControllerKind::touch);
    const float b = (78.7f - tilt) * 3.14159265f / 180.f;
    const Quat q{std::sin(b / 2), 0, 0, std::cos(b / 2)}; // about +x: the grip's +y up and toward the player
    sw::Tracked t;
    t.eye = {0, 1.65f, 0};
    t.feet = {0, 0, 0};
    Pose grip[2];
    for (int h = 0; h < 2; ++h) {
        grip[h].position = {h == 0 ? -.2f : .2f, 1.22f, -.36f};
        grip[h].orientation = q;
        t.hand[h] = true;
        t.grip[h] = grip[h].position;
        t.gripForward[h] = q.rotate({0, 0, -1});
        t.gripX[h] = q.rotate({1, 0, 0});
        t.gripUp[h] = q.rotate({0, 1, 0});
        t.handleTiltDeg[h] = tilt;
        t.curl[h] = std::getenv("DD_CURL") ? static_cast<float>(std::atof(std::getenv("DD_CURL"))) : 1.f;
    }
    // The suit worn: with fistFit (dd_baton_tip ON, the new fist) or without (test 3's).
    struct Worn {
        std::vector<Tri> arm;
        std::vector<Vec3> fist[2], fistN[2];
    };
    Worn worn[2];
    for (int fit = 0; fit < 2; ++fit) {
    t.fistFit[0] = t.fistFit[1] = fit == 1;
    sw::Skeleton live;
    sw::BoneMove mv[sw::bones];
    sw::Skinned s;
    std::vector<Tri>& arm = worn[fit].arm;
    std::vector<Vec3>* fist = worn[fit].fist;
    std::vector<Vec3>* fistN = worn[fit].fistN;
    if (!sw::estimate(rig, t, live) || !sw::moves(rig, live, mv) || !sw::skin(rig, mv, s)) {
        std::fprintf(stderr, "wear failed\n");
        return 1;
    }
    if (std::getenv("DD_DEBUG"))
        for (int h = 0; h < 2; ++h) {
            const sw::BoneMove& b = mv[h ? sw::boneHandR : sw::boneHandL];
            float lo = 1e9f, hi = -1e9f, zlo = 1e9f, zhi = -1e9f;
            int n = 0;
            for (size_t v = 0; v < rig.rest.size(); ++v)
                for (int c = 0; c < 4; ++c)
                    if (rig.weight[v * 4 + c] > 128 && (rig.joint[v * 4 + c] & 15) == (h ? sw::boneHandR : sw::boneHandL)) {
                        const Vec3 d = rig.rest[v] - b.a0;
                        lo = std::fmin(lo, dot(d, b.y0)), hi = std::fmax(hi, dot(d, b.y0));
                        zlo = std::fmin(zlo, dot(d, b.z0)), zhi = std::fmax(zhi, dot(d, b.z0));
                        ++n;
                    }
            const Vec3 w = rig.skeleton.at[h ? sw::wristR : sw::wristL], tp = rig.skeleton.at[h ? sw::tipR : sw::tipL];
            Vec3 mean{};
            int cnt = 0;
            for (size_t v = 0; v < rig.rest.size(); ++v)
                if (rig.weight[v * 4] > 128 && (rig.joint[v * 4] & 15) == (h ? sw::boneHandR : sw::boneHandL))
                    mean = mean + rig.rest[v], ++cnt;
            mean = mean * (1.f / std::max(1, cnt));
            std::fprintf(stderr, "height %.2f wrist %.3f %.3f %.3f tip %.3f %.3f %.3f mean %.3f %.3f %.3f y0 %.2f %.2f %.2f z0 %.2f %.2f %.2f\n",
                         rig.height, w.x, w.y, w.z, tp.x, tp.y, tp.z, mean.x, mean.y, mean.z, b.y0.x, b.y0.y, b.y0.z, b.z0.x, b.z0.y, b.z0.z);
            std::fprintf(stderr, "hand %d knuckle %.3f fingers %.3f curl %.2f verts %d y %.3f..%.3f z %.3f..%.3f thumb %.0f\n", h,
                         b.knuckle, b.fingers, b.curl, n, lo, hi, zlo, zhi, rig.handThumb[h]);
        }
    for (int h = 0; h < 2; ++h) { // the curled fingers' skinned vertices (rest past the knuckles), grip frame
        const sw::BoneMove& b = mv[h ? sw::boneHandR : sw::boneHandL];
        std::ofstream f(out + "fingers_" + std::to_string(fit) + "_" + std::to_string(h) + ".xyz", std::ios::binary);
        for (size_t v = 0; v < rig.rest.size() && v < s.p.size(); ++v)
            if (rig.weight[v * 4] > 200 && (rig.joint[v * 4] & 15) == (h ? sw::boneHandR : sw::boneHandL)) {
                const Vec3 d = rig.rest[v] - b.a0;
                put(f, q.conjugate().rotate(s.p[v] - grip[h].position), {dot(d, b.y0) - b.knuckle, dot(d, b.z0), dot(d, b.x0)});
            }
    }
    for (uint32_t i = 0; i < m->triangles(); ++i) {
        const uint8_t bone = rig.triangleBone[i];
        const bool hand = bone == sw::boneHandL || bone == sw::boneHandR;
        if (!(hand || bone == sw::boneForeL || bone == sw::boneForeR))
            continue;
        Tri tr{};
        for (int k = 0; k < 3; ++k)
            tr.p[k] = s.p[rig.corner[i * 3 + k]];
        tr.c = hand ? Vec3{.42f, .07f, .07f} : Vec3{.30f, .05f, .05f}; // the suit's red (its textures not read here)
        arm.push_back(tr);
        if (hand)
            for (int k = 0; k < 3; ++k) {
                const int side = length(tr.p[k] - grip[0].position) < length(tr.p[k] - grip[1].position) ? 0 : 1;
                fist[side].push_back(tr.p[k]);
                fistN[side].push_back(normalized(s.n[rig.corner[i * 3 + k]]));
            }
    }
    for (int h = 0; h < 2; ++h) { // the fists in their grip frames (for tools/dd_baton073: the handle's hole)
        std::ofstream f(out + "fist_" + std::to_string(fit) + "_" + std::to_string(h) + ".xyz", std::ios::binary);
        for (const Vec3& p : fist[h])
            put(f, q.conjugate().rotate(p - grip[h].position), {});
    }
    }
    if (std::getenv("DD_SWEEP")) // the seat's offset (grip frame: back of the hand, along the forearm), poke per hand
        for (float bx = -.02f; bx <= .0201f; bx += .005f)
            for (float fo = -.02f; fo <= .0301f; fo += .005f) {
                int sum = 0;
                for (unsigned h = 0; h < 2; ++h) {
                    const baton_grip::Shaft sh = db::seated(grip[h], h, ControllerKind::touch);
                    const Vec3 back = q.rotate({h ? 1.f : -1.f, 0, 0}), fore = q.rotate(baton_grip::localForearm(ControllerKind::touch));
                    const Vec3 o = back * bx + fore * fo;
                    sum += poke(worn[1].fist[h], worn[1].fistN[h], sh.butt + o, db::seam(sh.butt, sh.tip) + o);
                }
                std::printf("back %+.3f fore %+.3f poke %d\n", bx, fo, sum);
            }
    const Vec3 viewer = t.eye;
    for (int h = 0; h < 2; ++h)
        std::printf("hole %d: depth %.4f slope %.3f hole %+.4f %+.4f\n", h, rig.fingerDepth[h], rig.fingerSlope[h], rig.fistHole[h][0], rig.fistHole[h][1]);
    std::printf("{\n");
    for (int scene = 0; scene < 3; ++scene) {
        const char* name = scene == 0 ? "old" : scene == 1 ? "new" : "grapple";
        std::ofstream f(out + "dd_" + name + ".tri", std::ios::binary);
        const Worn& w = worn[scene == 0 ? 0 : 1];
        const std::vector<Vec3>* fist = w.fist;
        const std::vector<Vec3>* fistN = w.fistN;
        for (const Tri& tr : w.arm)
            for (const Vec3& p : tr.p)
                put(f, p, tr.c);
        std::printf("  \"%s\": {", name);
        for (unsigned h = 0; h < 2; ++h) {
            const baton_grip::Shaft sh = scene == 0 ? baton_grip::shaft(grip[h], h, ControllerKind::touch)
                                                    : db::seated(grip[h], h, ControllerKind::touch);
            std::vector<Vertex> v;
            if (scene == 2 && h == 1) {
                // The right hand grapples to a point 6 m ahead and up, the tip all the way out (fraction .6 shown).
                const Vec3 anchor = sh.tip + normalized(sh.tip - sh.butt) * 1.2f;
                const db::TipPlace tp = db::place(sh.butt, sh.tip, anchor, .7f);
                hero007::appendBatonModelSplit(v, sh.butt, sh.tip, *baton[h], viewer, tp.back, tp.front, db::tipShare);
                hero007::appendBatonRope(v, tp.rope, tp.back, viewer, .0015f);
            } else {
                hero007::appendBatonModel(v, sh.butt, sh.tip, *baton[h], viewer, true);
            }
            // The model's colours (untextured here): the Yanez baton's silver ends and red body by place.
            const Vec3 seamAt = db::seam(sh.butt, sh.tip), way = normalized(sh.tip - sh.butt);
            const float len = length(sh.tip - sh.butt);
            for (size_t k = 0; k + 2 < v.size(); k += 3) {
                const Vec3 mid = (v[k].position + v[k + 1].position + v[k + 2].position) * (1.f / 3);
                const float a = dot(mid - sh.butt, way);
                float al = 0;
                const bool onShaft = segDist(mid, sh.butt, sh.tip, al) < .03f;
                const bool silver = !onShaft || a > dot(seamAt - sh.butt, way) - .002f || a < len * .055f;
                const Vec3 c = v[k].color.x + v[k].color.y + v[k].color.z < .3f ? v[k].color
                             : silver ? Vec3{.78f, .80f, .84f} : Vec3{.80f, .05f, .05f};
                for (int j = 0; j < 3; ++j)
                    put(f, v[k + j].position, c);
            }
            // The clipping measure on the handle (butt to the seam).
            const Vec3 seam = db::seam(sh.butt, sh.tip);
            const int poked = poke(fist[h], fistN[h], sh.butt, seam);
            int inside = 0, hug = 0;
            for (const Vec3& p : fist[h]) {
                float along = 0;
                const float d = segDist(p, sh.butt, seam, along);
                if (along > 0 && along < 1 && d < hero007::batonDiameter * .5f - .002f)
                    ++inside;
                if (d < .045f)
                    ++hug;
            }
            const Vec3 tube = normalized(q.rotate(db::localTube(tilt)));
            const float off = std::acos(std::fmin(1.f, std::fabs(dot(normalized(sh.tip - sh.butt), tube)))) * 57.2958f;
            std::printf("%s\"%s\": {\"poke\": %d, \"inside\": %d, \"hug\": %d, \"fist\": %zu, \"offTubeDeg\": %.1f}", h ? ", " : "",
                        h ? "right" : "left", poked, inside, hug, fist[h].size(), off);
        }
        std::printf("}%s\n", scene < 2 ? "," : "");
    }
    std::printf("}\n");
    return 0;
}
