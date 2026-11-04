// HERO VR 0.0.78 PUNISHER ARSENAL: the guns in hero007::Play - drawn from the model's own right-leg holster
// and the back, fired, the magazine ejected, a new one from the thigh pouch seated and the slide racked; the
// pump's dry pull, rack and a shell from the belt into the port; the sniper's scope for game_xr.
#include "spidy/hero007_play.hpp"

#include <cmath>
#include <cstdio>
#include <string>

using namespace spidy;
namespace p78 = spidy::punisher078;

static int failures = 0;
static void check(bool ok, const std::string& what) {
    if (!ok) {
        ++failures;
        std::printf("FAIL: %s\n", what.c_str());
    }
}

// HERO VR 0.0.78 GUN BLOOD: recording the pellets never changes a shot's damage (the amounts, the hit count),
// and each pellet that hit is there once, on the enemy it hit, along its way.
static void gunBloodShots() {
    const hero007::GunSpec specs[] = {hero007::pistolSpec, hero007::sawedOffSpec, p78::pumpShot, p78::pistolShot,
                                      p78::sniperShot,     p78::rifleShot};
    uint32_t layouts = 0, pelletsSeen = 0;
    for (const auto& g : specs)
        for (int lay = 0; lay < 40; ++lay) {
            hero_modes::BatonEnemy foes[3] = {{11, {-.6f + .03f * lay, 1.4f, -2.f - .1f * lay}},
                                              {12, {.25f, 1.3f + .01f * lay, -4.5f}},
                                              {13, {.05f * lay - 1.f, 1.4f, -5.5f}, lay % 7 == 0}};
            const Vec3 from{0, 1.45f, 0}, dir = normalized(Vec3{.004f * (lay % 9) - .016f, -.01f, -1});
            float plain[3]{}, logged[3]{};
            hero007::PelletHit pel[hero007::pelletHitsMost]{};
            int pelN = -1;
            const int a = hero007::shoot(g, from, dir, foes, 3, plain);
            const int b = hero007::shoot(g, from, dir, foes, 3, logged, pel, &pelN);
            bool same = a == b;
            for (int e = 0; e < 3; ++e)
                same = same && plain[e] == logged[e];
            check(same, "gun blood: the shot's damage is the same with the pellets recorded");
            int perFoe[3]{};
            for (int k = 0; k < pelN; ++k) {
                check(pel[k].enemy < 3 && !foes[pel[k].enemy].down, "gun blood: a pellet on a standing enemy");
                if (pel[k].enemy >= 3)
                    continue;
                ++perFoe[pel[k].enemy];
                const Vec3 c = foes[pel[k].enemy].chest;
                check(length(pel[k].at - c) < 1.2f && std::abs(length(pel[k].way) - 1.f) < 1e-3f && dot(pel[k].way, c - from) > 0,
                      "gun blood: a pellet's blood starts on him, along the shot");
            }
            for (int e = 0; e < 3; ++e) // all within 6 m: full strength a pellet
                check(std::abs(perFoe[e] * g.amount - plain[e]) < 1e-3f, "gun blood: one blood a pellet that hit");
            layouts += a > 0;
            pelletsSeen += static_cast<uint32_t>(pelN);
        }
    check(layouts > 100 && pelletsSeen > layouts, "gun blood: the layouts hit (" + std::to_string(layouts) + ")");
    float none[1]{};
    int n = 5;
    check(hero007::shoot(p78::pumpShot, {NAN, 0, 0}, {0, 0, -1}, nullptr, 0, none, nullptr, &n) == 0 && n == 0,
          "gun blood: nothing for a bad shot");
}

int main() {
    gunBloodShots();
    const float dt = 1.f / 90.f, eye = 1.7f, k = holster::scaleFor(eye);
    hero007::Play p;
    hero007::PlayInput in;
    in.dt = dt;
    in.play = true;
    in.hero = hero_modes::punisher;
    in.head.position = {0, eye, 0};
    in.eyeHeight = eye;
    in.arsenal078 = true;
    const Vec3 head = in.head.position;
    const Vec3 leg = head + Vec3{.17f * k, -.70f * k, 0};          // the right leg's holster (body facing -z)
    const Vec3 backMid = head + Vec3{0, -.12f * k, .18f};            // between the shoulder blades
    const Vec3 backL = head + Vec3{-.16f, -.12f * k, .18f};          // over the left shoulder
    const Vec3 thighL = head + Vec3{-.17f * k, -.76f * k, 0};        // the left thigh's magazine pouch
    const Vec3 belt = head + Vec3{0, -.55f * k, -.15f};              // the belt's front shell pouches
    const Vec3 aimAt = head + Vec3{.2f, -.4f, -.4f};                 // the gun out in front
    const Vec3 rest = head + Vec3{-.35f, -.6f, -.1f};
    for (unsigned i = 0; i < 2; ++i) {
        in.hands[i].tracked = true;
        in.hands[i].aim = {0, 0, -1};
        in.hands[i].position = i ? aimAt : rest;
    }
    hero007::PlayOutput out;
    uint32_t heard = 0;
    float blowHp = 0;
    const auto frame = [&](int n = 1) {
        for (int f = 0; f < n; ++f) {
            out = {};
            p.holsterStep(in, out);
            p.step(in, out);
            heard |= out.sounds;
            for (uint32_t b = 0; b < out.blowCount; ++b)
                blowHp += out.blows[b].blow.amount;
        }
    };
    hero007::PlayHand& R = in.hands[1];
    hero007::PlayHand& L = in.hands[0];
    frame(5);
    check(p.arsenalOn078 && p.guns.count == 4, "arsenal: four guns on the body");
    check(p.guns.slots[0].zone == holster::rightLeg && p.guns.slots[0].item == hero007::item078 + p78::suppressedPistol,
          "arsenal: the suppressed pistol in the right-leg holster");
    // ---- the pistol: draw, fire, eject, a new magazine, rack ----
    R.position = leg;
    frame(3);
    R.squeeze = 1;
    frame(2);
    check(p.guns.heldIn(1) == 0, "pistol: drawn from the leg holster by the right hand");
    R.position = aimAt;
    frame(3);
    R.trigger = 1;
    frame(1);
    check(p.arsenalShots078 == 1 && (heard >> sfx::suppressedShot & 1u), "pistol: fired (the suppressed shot)");
    R.trigger = 0;
    frame(30);
    const uint32_t before = p.mag078[0].rounds();
    R.eject = true;
    frame(1);
    R.eject = false;
    check(!p.mag078[0].magIn && p.mag078[0].chambered && (heard >> sfx::magDrop & 1u), "pistol: the eject button dropped the magazine");
    check(p.mag078[0].rounds() == 1 && before > 1, "pistol: one left in the chamber");
    R.trigger = 1;
    frame(1);
    R.trigger = 0;
    frame(30);
    R.trigger = 1;
    frame(1);
    R.trigger = 0;
    frame(2);
    check(p.arsenalShots078 == 2 && p.mag078[0].locked && p.mag078[0].dryPulls == 1, "pistol: the last round, then a dry click, no auto reload");
    // the left hand: the thigh pouch, the well, then the slide
    const p78::GunPoints g = p78::pointsOf(p78::suppressedPistol);
    const auto pose = p.pose078(in, 1);
    check(pose.ok, "pistol: a pose");
    L.position = thighL;
    frame(2);
    L.squeeze = 1;
    frame(2);
    check(p.mag078[0].mag.carrying && p.guns.heldIn(0) < 0, "pistol: the left hand took a magazine from the thigh pouch");
    hero007::Draw d;
    p.draw(d, Pose{}, in, 0);
    check(d.held[0].look == hero007::lookMag078 && d.held[0].side == p78::suppressedPistol, "pistol: the magazine drawn in the hand");
    L.position = p78::toWorld(pose.at, pose.forward, pose.up, g.magWell);
    frame(2);
    check(p.mag078[0].magIn && p.mag078[0].magRounds == p78::pistolShot.rounds, "pistol: the magazine seated");
    L.squeeze = 0;
    frame(3);
    const Vec3 rackAt = p78::toWorld(pose.at, pose.forward, pose.up, g.rack);
    L.position = rackAt;
    frame(1);
    L.squeeze = 1;
    frame(2);
    for (int f = 1; f <= 6; ++f) {
        L.position = rackAt - pose.forward * (g.rackTravel * f / 6.f);
        frame(1);
    }
    for (int f = 5; f >= 0; --f) {
        L.position = rackAt - pose.forward * (g.rackTravel * f / 6.f);
        frame(1);
    }
    check(p.mag078[0].chambered && !p.mag078[0].locked && p.mag078[0].racks == 1, "pistol: racked, a round chambered");
    check(p.guns.heldIn(0) < 0, "pistol: the racking hand drew nothing");
    L.squeeze = 0;
    L.position = rest;
    frame(3);
    R.trigger = 1;
    frame(1);
    R.trigger = 0;
    check(p.arsenalShots078 == 3, "pistol: fires again after the manual reload");
    // holster it: the grip let go at the leg
    R.position = leg;
    frame(2);
    R.squeeze = 0;
    frame(2);
    check(p.guns.heldIn(1) < 0 && p.guns.slots[0].heldBy < 0, "pistol: back in the leg holster");
    p.draw(d = {}, Pose{}, in, 0);
    check(d.worn[0].look == hero007::lookPistol078 && d.worn[0].holster == 0, "pistol: drawn in the holster, no holster mesh");
    // ---- the pump: draw from the back, fire, dry, rack, a shell in ----
    R.position = backMid;
    frame(3);
    R.squeeze = 1;
    frame(2);
    check(p.guns.heldIn(1) == 1, "pump: drawn from the middle of the back");
    R.position = aimAt;
    frame(3);
    // HERO VR 0.0.78 GUN BLOOD: the pump into an enemy 4 m off - a blood a pellet, his damage as before.
    const auto pumpPose = p.pose078(in, 1);
    const hero_modes::BatonEnemy pumpFoe[1] = {{91, pumpPose.at + pumpPose.forward * 4.f}};
    in.list = pumpFoe;
    in.count = 1;
    const uint64_t bleedsWas = p.bleeds;
    blowHp = 0;
    R.trigger = 1;
    frame(1);
    R.trigger = 0;
    {
        const uint64_t pellets = p.bleeds - bleedsWas;
        check(pellets > 1 && pellets <= p78::pumpShot.pellets, "gun blood: the pump's pellets each bleed (" + std::to_string(pellets) + ")");
        check(std::abs(blowHp - hero007::shotReaction(p78::pumpShot, pellets * p78::pumpShot.amount).amount) < 1e-3f,
              "gun blood: the pump's blow is the pellets' damage, as before");
        hero007::Draw bd;
        p.draw(bd, Pose{}, in, 0);
        uint32_t wounds = 0;
        for (uint32_t k = 0; k < hero007::drawImpacts; ++k)
            if (bd.impactAge[k] < hero007::impactLife) {
                ++wounds;
                check(length(bd.impactWound[k] - pumpFoe[0].chest) < 1.2f && std::isfinite(bd.impactFloor[k]) &&
                          bd.impactFloor[k] < pumpFoe[0].chest.y,
                      "gun blood: the wound on him, the floor under him");
            }
        check(wounds == pellets, "gun blood: a wound a pellet");
    }
    in.list = nullptr;
    in.count = 0;
    frame(40);
    check(p.pump078.shots == 1 && (heard >> sfx::shotgunBlast & 1u), "pump: fired");
    R.trigger = 1;
    frame(1);
    R.trigger = 0;
    frame(2);
    check(p.pump078.shots == 1 && p.pump078.dryPulls == 1, "pump: no second shot until it is racked");
    const p78::GunPoints pg = p78::pointsOf(p78::pumpShotgun);
    const auto pp = p.pose078(in, 1);
    const Vec3 fore = p78::toWorld(pp.at, pp.forward, pp.up, pg.forend);
    L.position = fore;
    frame(1);
    L.squeeze = 1;
    frame(2);
    check(p.support078[1] && p.pump078.pump.holding, "pump: the left hand on the forend (two hands)");
    const uint64_t shellsWas = p.shells.thrown;
    for (int f = 1; f <= 6; ++f) {
        L.position = fore - pp.forward * (pg.rackTravel * f / 6.f);
        frame(1);
    }
    check(p.pump078.chamber == p78::chamberEmpty && (heard >> sfx::pumpRack & 1u), "pump: racked back, the spent shell out");
    check(p.shells.thrown != shellsWas, "pump: the shell flies out of the port");
    for (int f = 5; f >= 0; --f) {
        L.position = fore - pp.forward * (pg.rackTravel * f / 6.f);
        frame(1);
    }
    check(p.pump078.chamber == p78::chamberLive && p.pump078.tube == p78::pumpShot.rounds - 1, "pump: home, the next shell chambered");
    L.squeeze = 0;
    frame(2);
    L.position = belt;
    frame(2);
    L.squeeze = 1;
    frame(2);
    check(p.pump078.shell.carrying, "pump: a shell from the belt");
    L.position = p78::toWorld(pp.at, pp.forward, pp.up, pg.port);
    frame(2);
    check(p.pump078.tube == p78::pumpShot.rounds && p.pump078.loaded == 1, "pump: the shell into the port");
    L.squeeze = 0;
    L.position = rest;
    frame(2);
    R.squeeze = 0;
    R.position = backMid;
    frame(3);
    check(p.guns.heldIn(1) < 0, "pump: back on the back");
    // ---- the sniper: the left hand over the left shoulder; the scope for game_xr ----
    R.position = aimAt;
    L.position = backL;
    frame(3);
    L.squeeze = 1;
    frame(2);
    check(p.guns.heldIn(0) == 2, "sniper: drawn over the left shoulder");
    L.position = aimAt - Vec3{.4f, 0, 0};
    in.scopeZoom = 2;
    frame(2);
    check(p.optic078[0].kind == hero007::opticScope && p.optic078[0].zoom == 8.f && p.optic078[0].radius > 0,
          "sniper: the scope for the eye pass (8X)");
    check(p.optic078[1].kind == hero007::opticNone, "the other hand: no optic");
    // ---- OFF: the 0.0.77 loadout back ----
    in.arsenal078 = false;
    frame(2);
    check(!p.arsenalOn078 && p.guns.count == 3 && p.guns.slots[2].item == hero007::sawedOff, "OFF: the pistols and the sawed-off");
    if (failures == 0)
        std::printf("punisher078_play: all passed\n");
    return failures == 0 ? 0 : 1;
}
