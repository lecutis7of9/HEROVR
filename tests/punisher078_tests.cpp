// HERO VR 0.0.78 PUNISHER ARSENAL (include/spidy/punisher078.hpp), pure: the pump, the magazine guns, the
// leg holster, the scope and the red dot.
#include "spidy/punisher078.hpp"

#include <cmath>
#include <cstdio>

using namespace spidy;
using namespace spidy::punisher078;

namespace {
int failures = 0;
void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++failures;
    }
}
constexpr float dt = 1.f / 90.f;
// The off hand somewhere harmless (far from every part) and at a part, in the gun's frame.
constexpr Local away{-.6f, -.4f, .3f};
Local nudge(Local p, float back) { return {p.forward - back, p.up, p.right}; }
// One full rack: grip at the handle, pull back the travel, push home, let go.
template <class G> void rack(G& g, const GunPoints& p, float t = 0) {
    for (int i = 0; i <= 12; ++i)
        g.update(dt, false, true, nudge(p.rack, p.rackTravel * i / 12.f + t), true, false);
    for (int i = 12; i >= 0; --i)
        g.update(dt, false, true, nudge(p.rack, p.rackTravel * i / 12.f + t), true, false);
    g.update(dt, false, false, away, true, false);
}
void rackMag(MagGun& g) {
    const GunPoints p = pointsOf(g.arm);
    for (int i = 0; i <= 12; ++i)
        g.update(dt, false, false, true, nudge(p.rack, p.rackTravel * i / 12.f), true, false);
    for (int i = 12; i >= 0; --i)
        g.update(dt, false, false, true, nudge(p.rack, p.rackTravel * i / 12.f), true, false);
    g.update(dt, false, false, false, away, true, false);
}
void pull(Pump& g) {
    g.update(dt, true, false, away, true, false);
    g.update(dt, false, false, away, true, false);
    for (int i = 0; i < 40; ++i)
        g.update(dt, false, false, away, true, false);
}
bool pullMag(MagGun& g) {
    g.update(dt, true, false, false, away, true, false);
    const bool f = g.fired;
    g.update(dt, false, false, false, away, true, false);
    for (int i = 0; i < 80; ++i)
        g.update(dt, false, false, false, away, true, false);
    return f;
}
void insertShell(Pump& g) {
    const GunPoints p = pointsOf(pumpShotgun);
    g.update(dt, false, true, away, true, true); // grip at the belt pouch: a shell
    g.update(dt, false, true, {p.port.forward - .3f, p.port.up - .2f, 0}, true, false);
    g.update(dt, false, true, p.port, true, false); // at the port: in
    g.update(dt, false, false, away, true, false);
}
void insertMag(MagGun& g) {
    const GunPoints p = pointsOf(g.arm);
    g.update(dt, false, false, true, away, true, true);
    g.update(dt, false, false, true, {p.magWell.forward, p.magWell.up - .25f, 0}, true, false);
    g.update(dt, false, false, true, p.magWell, true, false);
    g.update(dt, false, false, false, away, true, false);
}
} // namespace

int main() {
    // ---- PUMP: fire once, then nothing until it is racked; never reloads itself ----
    {
        Pump g;
        pull(g);
        check(g.shots == 1 && g.chamber == chamberSpent, "pump: the first pull fires and leaves a spent shell");
        pull(g);
        check(g.shots == 1 && g.dryPulls == 1, "pump: no second shot without racking (no auto cycling)");
        rack(g, pointsOf(pumpShotgun));
        check(g.racks == 1 && g.chamber == chamberLive && g.tube == pumpShot.rounds - 1 && g.ejected == 1,
              "pump: a rack throws the spent shell out and chambers one from the tube");
        pull(g);
        check(g.shots == 2, "pump: fires after the rack");
        // Half a stroke does nothing.
        {
            const GunPoints p = pointsOf(pumpShotgun);
            for (int i = 0; i <= 5; ++i)
                g.update(dt, false, true, nudge(p.rack, p.rackTravel * .5f * i / 5.f), true, false);
            for (int i = 5; i >= 0; --i)
                g.update(dt, false, true, nudge(p.rack, p.rackTravel * .5f * i / 5.f), true, false);
            g.update(dt, false, false, away, true, false);
            check(g.chamber == chamberSpent && g.racks == 1, "pump: a short stroke neither ejects nor feeds");
        }
        // Empty the tube.
        for (int i = 0; i < 20; ++i) {
            rack(g, pointsOf(pumpShotgun));
            pull(g);
        }
        check(g.tube == 0 && g.chamber != chamberLive, "pump: runs dry");
        const uint64_t shots = g.shots;
        for (int i = 0; i < 400; ++i) // long wait: no automatic reload
            g.update(dt, false, false, away, true, false);
        rack(g, pointsOf(pumpShotgun));
        pull(g);
        check(g.shots == shots && g.tube == 0, "pump: no automatic reload, ever");
        insertShell(g);
        check(g.tube == 1 && g.loaded == 1, "pump: a shell carried from the belt to the loading port goes in");
        pull(g);
        check(g.shots == shots, "pump: a loaded tube still needs the rack before it fires");
        rack(g, pointsOf(pumpShotgun));
        pull(g);
        check(g.shots == shots + 1, "pump: shell in, rack, fire");
        // A shell let go away from the port falls; the tube stays.
        g.update(dt, false, true, away, true, true);
        g.update(dt, false, false, away, true, false);
        check(g.tube == 0 && g.shell.dropped == 1, "pump: a shell let go off the port is dropped");
        // The pump's grip is not a shell's grip: gripping at the forend never takes a shell.
        Pump h;
        h.update(dt, false, true, pointsOf(pumpShotgun).rack, true, true);
        check(h.pump.holding && !h.shell.carrying, "pump: the forend's grip racks, it takes no shell");
        // Off hand lost: the pump goes nowhere.
        Pump l;
        pull(l);
        l.update(dt, false, true, pointsOf(pumpShotgun).rack, false, false);
        check(!l.pump.holding, "pump: an untracked hand never racks");
        // The tube holds 7: a full tube takes no more.
        Pump f;
        insertShell(f);
        check(f.tube == pumpShot.rounds && f.loaded == 0, "pump: a full tube takes no shell");
    }
    // ---- SUPPRESSED PISTOL: eject, new magazine, rack before it fires ----
    {
        MagGun g;
        g.arm_(suppressedPistol);
        check(g.rounds() == 15 && g.chambered && g.magIn, "pistol: starts full, one up the spout");
        int fired = 0;
        for (int i = 0; i < 20; ++i)
            fired += pullMag(g) ? 1 : 0;
        check(fired == 15 && g.locked && !g.chambered, "pistol: 15 shots, then the slide locks back");
        g.update(dt, false, true, false, away, true, false); // the eject button
        check(!g.magIn && g.drops == 1, "pistol: the eject button drops the magazine");
        g.update(dt, false, false, false, away, true, false);
        insertMag(g);
        check(g.magIn && g.magRounds == 15 && g.seats == 1, "pistol: a magazine from the thigh pouch seats in the well");
        check(!pullMag(g), "pistol: a fresh magazine does not fire before the slide is racked");
        rackMag(g);
        check(g.chambered && g.magRounds == 14 && !g.locked, "pistol: the rack chambers a round");
        check(pullMag(g), "pistol: fires after the rack");
        // Eject with rounds still in: the magazine's rounds go with it, the chamber's stays.
        g.update(dt, false, true, false, away, true, false);
        g.update(dt, false, false, false, away, true, false);
        check(!g.magIn && g.chambered && g.rounds() == 1, "pistol: a dropped magazine takes its rounds, one stays chambered");
        check(pullMag(g) && !pullMag(g), "pistol: the chambered round fires, then nothing with no magazine");
        // A magazine needs an empty well.
        MagGun h;
        h.arm_(suppressedPistol);
        insertMag(h);
        check(h.seats == 0 && h.mag.dropped == 1, "pistol: no second magazine into a full well");
        // Rack with a live round chambered: it is ejected (and the next chambered).
        MagGun r;
        r.arm_(suppressedPistol);
        rackMag(r);
        check(r.rounds() == 14 && r.chambered, "pistol: racking a loaded gun throws a live round");
    }
    // ---- SNIPER: the bolt after every shot ----
    {
        MagGun g;
        g.arm_(sniperRifle);
        check(g.rounds() == 5, "sniper: 5 rounds");
        check(pullMag(g) && !pullMag(g), "sniper: one shot, then the bolt must be worked");
        rackMag(g);
        check(pullMag(g), "sniper: the bolt worked, it fires");
        check(g.magRounds == 3 && g.rounds() == 3, "sniper: the bolt fed from the magazine (the spent case is not a round)");
    }
    // ---- ASSAULT RIFLE: automatic while held ----
    {
        MagGun g;
        g.arm_(assaultRifle);
        for (int i = 0; i < 90; ++i) // one second held
            g.update(dt, true, false, false, away, true, false);
        check(g.shots >= 11 && g.shots <= 13, "rifle: about 750 rounds a minute while the trigger is held");
        for (int i = 0; i < 400; ++i)
            g.update(dt, true, false, false, away, true, false);
        check(g.shots == 30 && g.locked, "rifle: 30 rounds, then the bolt locks");
        g.update(dt, false, true, false, away, true, false);
        g.update(dt, false, false, false, away, true, false);
        insertMag(g);
        rackMag(g);
        check(g.rounds() == 30 && g.chambered, "rifle: new magazine and the charging handle: 30 again");
    }
    // ---- the places on the body ----
    {
        check(holster::inZone(holster::rightLeg, {legHolsterBelow, legHolsterSide, 0}),
              "the model's right-leg holster mouth is in the leg zone");
        check(!holster::inZone(holster::rightLeg, {legHolsterBelow, -legHolsterSide, 0}), "not the left leg");
        check(atMagPouch({.76f, -.17f, 0}) && !atMagPouch({.76f, .17f, 0}), "the left thigh's pouch, not the right");
        check(atShellPouch({.56f, 0, .12f}) && !atShellPouch({.56f, 0, -.2f}), "the belt's front pouches");
        check(holster::inZone(holster::backMiddle, {.2f, 0, -.2f}) && !holster::inZone(holster::backMiddle, {.2f, .2f, -.2f}),
              "the back's middle");
        check(holster::handsFor(holster::backMiddle, false) == 3u, "either hand draws from the back's middle");
        holster::BodyYaw body{};
        Vec3 at, fwd, up;
        legHolsterPose({0, 1.62f, 0}, body, at, fwd, up);
        check(std::abs(at.y - (1.62f - legHolsterBelow + .05f)) < 1e-4f && at.x > .1f && fwd.y < -.99f,
              "the holstered pistol: grip above the right leg's mouth, muzzle down");
    }
    // ---- SCOPE: magnified patch, centred on the axis, the eye box ----
    {
        EyeView v{};
        v.tanLeft = -1.f, v.tanRight = 1.f, v.tanDown = -1.f, v.tanUp = 1.f;
        v.eye.position = {0, 0, 0};
        const Vec3 lens{0, 0, -.08f}, axis{0, 0, -1};
        const ScopeView s = scopeView(v, lens, axis, .019f, 4.f);
        check(s.ok && std::abs(s.lensU - .5f) < 1e-4f && std::abs(s.srcU - .5f) < 1e-4f, "scope: on axis, lens and patch centred");
        check(std::abs(std::atan(s.lensRadiusU) / std::atan(s.srcRadiusU) - 4.f) < .1f, "scope: the patch is 1/4 the lens's angle");
        check(s.shade == 0.f, "scope: the eye on the axis sees the full picture");
        // Aim moves the patch: the scope turned right looks right.
        const ScopeView r = scopeView(v, lens, normalized(Vec3{.1f, 0, -1}), .019f, 4.f);
        check(r.srcU > .5f, "scope: turned right, the patch moves right");
        // The eye 2 cm off axis: dark.
        EyeView o = v;
        o.eye.position = {.02f, 0, 0};
        check(scopeView(o, lens, axis, .019f, 4.f).shade == 1.f, "scope: off the eye box it goes black");
        check(zoomOf(0) == 4.f && zoomOf(1) == 6.f && zoomOf(2) == 8.f && zoomOf(3) == 2.5f && zoomOf(9) == 4.f,
              "scope: the zoom choices");
    }
    // ---- RED DOT: on the lens, parallax-free ----
    {
        const Vec3 lens{0, 0, -.25f}, axis{0, 0, -1};
        const DotOnLens c = redDot({0, 0, 0}, lens, axis, .0125f);
        check(c.ok && c.offset < 1e-5f, "red dot: the eye on the axis sees it at the centre");
        const DotOnLens m = redDot({.005f, .003f, 0}, lens, axis, .0125f);
        check(m.ok && std::abs(m.at.x - .005f) < 1e-5f && std::abs(m.at.y - .003f) < 1e-5f,
              "red dot: the head moved, the dot moves with the eye (stays on the far target)");
        check(!redDot({.02f, 0, 0}, lens, axis, .0125f).ok, "red dot: the eye's ray off the window: no dot");
        check(!redDot({0, 0, -.5f}, lens, axis, .0125f).ok, "red dot: from in front of the sight: no dot");
    }
    // ---- the eye pass's constants (native_eye_gpu setEyeOptics) ----
    {
        EyeView v{};
        v.tanLeft = -1.f, v.tanRight = 1.f, v.tanDown = -1.f, v.tanUp = 1.f;
        const auto sc = opticConstants(1, v, {0, 0, -.08f}, {0, 0, -1}, .019f, 6.f);
        check(sc[7] == 1.f && std::abs(sc[0] - .5f) < 1e-4f && std::abs(sc[4] - .5f) < 1e-4f && sc[6] == 6.f && sc[2] > 0 &&
                  sc[8] == 0.f,
              "optics: the scope's constants");
        const auto dc = opticConstants(2, v, {0, 0, -.25f}, {0, 0, -1}, .0125f, 1.f);
        check(dc[7] == 2.f && std::abs(dc[4] - .5f) < 1e-4f && dc[9] > 0 && dc[10] > 0, "optics: the red dot at the centre");
        // the head moved 5 mm right: the dot stays over the far target (the axis's own point), not the lens centre
        EyeView m = v;
        m.eye.position = {.005f, 0, 0};
        const auto mc = opticConstants(2, m, {0, 0, -.25f}, {0, 0, -1}, .0125f, 1.f);
        check(mc[7] == 2.f && std::abs(mc[4] - .5f) < 1e-4f && mc[0] < .5f, "optics: the dot is parallax-free (lens moved, dot did not)");
        EyeView off = v;
        off.eye.position = {.02f, 0, 0};
        const auto oc = opticConstants(2, off, {0, 0, -.25f}, {0, 0, -1}, .0125f, 1.f);
        check(oc[7] == 2.f && oc[9] == 0.f && oc[4] < -1.f, "optics: the eye's ray off the window, no dot");
        check(opticConstants(0, v, {0, 0, -.25f}, {0, 0, -1}, .0125f, 1.f)[7] == 0.f &&
                  opticConstants(1, v, {NAN, 0, -.25f}, {0, 0, -1}, .0125f, 4.f)[7] == 0.f &&
                  opticConstants(1, v, {0, 0, .25f}, {0, 0, -1}, .0125f, 4.f)[7] == 0.f,
              "optics: none / bad / behind give all zero");
    }
    if (failures == 0)
        std::printf("punisher078: all passed\n");
    return failures == 0 ? 0 : 1;
}
