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
    if (const char* claws = std::getenv("HANDS_CLAWS")) {
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
