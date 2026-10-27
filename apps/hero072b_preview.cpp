// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
//
// Version 0.0.72 FIX ROUND (the user's 0.0.71 videos): draws each fix without a headset through the
// overlay's own vertices (the same code the game runs) - made-up hands, bodies and enemies over a dusk
// sky. SIMULATED, NOT GAME CAPTURE. Writes into <folder> stills (<name>.bmp) and clips (<name>/NNN.bmp,
// 30 frames a second):
//   batman_gear.bmp      the batarang pinched in the fingers; the grapple gun's holster on the belt
//   batman_grapple/      the line flying out of the muzzle to the hook, then taut (the pull: simulated)
//   superman_heat/       two thin beams from the eyes, the head turning (locked), hit sparks, scorch
//   punisher_holsters.bmp the drop-leg thigh holsters
//   punisher_shots/      the aimed pistol firing: muzzle flash, hit puffs, the readout
//   cap_reticle.bmp      the right hand's target marker before (enemy-sized) and after (small, faint)
//   cap_shield/          the thrown shield flying home and caught onto the arm (no snap)
//   wolverine_claws/     the claws out, a slash
//   van.bmp              the van's state: MODEL MISSING, movement not wired
//
//   spidy_hero072b_preview <folder>
#include "spidy/captain.hpp"
#include "spidy/hero007_play.hpp"
#include "spidy/hero007_visual.hpp"
#include "spidy/hero_modes.hpp"
#include "spidy/pixel_font.hpp"
#include "spidy/soft_shapes.hpp"
#include "spidy/web_visual.hpp"
#include "preview_image.hpp"
#include <cmath>
#include <cstdio>
#include <string>
#include <sys/stat.h>
#include <vector>
using namespace spidy;
using namespace spidy::preview;
using namespace spidy::hero007;

namespace {
constexpr int size = 720;
constexpr const char* simulated = "SIMULATED (THE MOD'S OWN DRAW CODE), NOT GAME CAPTURE";
void dusk(Image& image, int x0, int w, int h) {
    for (int y = 0; y < h; ++y)
        for (int x = x0; x < x0 + w; ++x) {
            const float v = static_cast<float>(y) / static_cast<float>(h);
            float* o = image.at(x, y);
            const bool below = v > .70f;
            o[0] = decode(below ? .14f : .16f + .28f * v);
            o[1] = decode(below ? .14f : .15f + .16f * v);
            o[2] = decode(below ? .17f : .28f + .12f * v);
        }
}
std::vector<Vertex> look(const std::vector<Vertex>& world, Vec3 eye, Vec3 target) {
    const Vec3 back = normalized(eye - target);
    const Vec3 right = normalized(cross(Vec3{0, 1, 0}, back));
    const Vec3 up = cross(back, right);
    auto out = world;
    for (auto& v : out) {
        const Vec3 d = v.position - eye;
        v.position = {dot(d, right), dot(d, up), dot(d, back)};
    }
    return out;
}
void body(std::vector<Vertex>& w, Vec3 f, Vec3 eye, Vec3 colour) {
    soft::softBar(w, f + Vec3{0, .1f, 0}, f + Vec3{0, 1.45f, 0}, normalized(f + Vec3{0, 1, 0} - eye), .34f, colour, 1, .002f);
    soft::softBar(w, f + Vec3{0, 1.5f, 0}, f + Vec3{0, 1.78f, 0}, normalized(f + Vec3{0, 1.6f, 0} - eye), .17f, colour, 1, .002f);
}
void hand(std::vector<Vertex>& out, Vec3 at, Vec3 eye) {
    soft::softBar(out, at - Vec3{0, .045f, 0}, at + Vec3{0, .01f, 0}, normalized(at - eye), .02f, {.03f, .03f, .035f}, .9f, .002f);
}
struct Cell {
    std::vector<Vertex> world;
    Vec3 eye, target;
    float pixelAngle;
};
// The caption at the top of the cell, square to the camera: a 13-pixel heading.
void label(Cell& c, const std::string& a, const std::string& b, Vec3, Vec3, float) {
    std::vector<Vertex>& out = c.world;
    const Vec3 eye = c.eye, ahead = normalized(c.target - eye);
    const float k = c.pixelAngle * 13 / .022f; // one metre out
    const Vec3 at = eye + ahead + cross(normalized(cross(ahead, Vec3{0, 1, 0})), ahead) * (c.pixelAngle * size * .40f);
    const Vec3 back = normalized(eye - at);
    const Vec3 right = normalized(cross(Vec3{0, 1, 0}, back));
    const Vec3 up = cross(back, right);
    PixelTextStyle s;
    s.height = .022f * k;
    s.top = s.bottom = Vec3{.55f, .85f, 1};
    appendPixelText(out, a, at - right * (pixelTextWidth(a, s) * .5f) + up * (.03f * k), right, up, s);
    s.height = .016f * k;
    s.top = s.bottom = Vec3{1, 1, 1};
    appendPixelText(out, b, at - right * (pixelTextWidth(b, s) * .5f), right, up, s);
    s.height = .011f * k;
    s.top = s.bottom = Vec3{1, .75f, .3f};
    appendPixelText(out, simulated, at - right * (pixelTextWidth(simulated, s) * .5f) - up * (.03f * k), right, up, s);
}
bool write(const std::string& path, std::vector<Cell>& cells) {
    Image image(size * static_cast<int>(cells.size()), size);
    for (size_t c = 0; c < cells.size(); ++c) {
        const int x0 = static_cast<int>(c) * size;
        dusk(image, x0, size, size);
        const Camera camera{1 / cells[c].pixelAngle, size * .5f, size * .5f};
        draw(image, look(cells[c].world, cells[c].eye, cells[c].target), camera, 0, 0, x0, 0, size, size);
    }
    return save(image, path);
}
bool frame(const std::string& dir, int n, Cell& c) {
    std::vector<Cell> one{c};
    char name[32];
    std::snprintf(name, sizeof name, "/%03d.bmp", n);
    return write(dir + name, one);
}
std::string clipDir(const std::string& dir, const char* name) {
    const std::string d = dir + name;
    mkdir(d.c_str(), 0755);
    return d;
}
} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: spidy_hero072b_preview <folder>\n");
        return 2;
    }
    const std::string dir = std::string(argv[1]) + "/";
    bool ok = true;
    constexpr float fps = 30;
    { // BATMAN: the batarang pinch, the belt holster
        std::vector<Cell> cells(2);
        Cell& a = cells[0];
        a.eye = {-.45f, 1.45f, .25f};
        a.target = {.02f, 1.32f, -.25f};
        a.pixelAngle = .0008f;
        const Vec3 palm{0, 1.3f, -.25f}, fwd = normalized(Vec3{.1f, .15f, -1}), thumb{0, 1, 0};
        Vec3 at, f, u;
        batarangPinch(palm, fwd, thumb, at, f, u);
        hand(a.world, palm, a.eye);
        appendItem(a.world, Item{lookBatarang, at, f, u}, a.eye, a.pixelAngle);
        label(a, "BATARANG PINCHED IN THE FINGERS", "AHEAD OF THE PALM, ITS FACE SQUARE TO THE PALM (WAS FLAT ON THE HAND)",
              {.02f, 1.48f, -.25f}, a.eye, a.pixelAngle);
        Cell& b = cells[1];
        b.eye = {1.2f, 1.45f, 1.4f};
        b.target = {0, 1.0f, 0};
        b.pixelAngle = .0019f;
        body(b.world, {0, 0, 0}, b.eye, {.06f, .06f, .07f});
        const Vec3 head{0, 1.7f, 0};
        holster::BodyYaw yaw;
        Vec3 wa, wf, wu, oa, of, ou;
        wornPose(holster::rightHip, head, yaw, wa, wf, wu, hipBelt);
        wornPose(holster::rightHip, head, yaw, oa, of, ou, hipClassic);
        appendHolster(b.world, Item{lookGrappleGun, wa, wf, wu, 0, 1}, b.eye, b.pixelAngle);
        soft::softBar(b.world, {-.2f, head.y - beltHang + .02f, .17f}, {.2f, head.y - beltHang + .02f, .17f}, {0, 0, 1}, .02f,
                      {.25f, .22f, .1f}, .8f, .002f); // the belt line (a guide)
        label(b, "GRAPPLE GUN ON THE BELT", "0.60 M UNDER THE EYES (WAS 0.78: BY THE FOOT); THE MODEL DRAWS IN IT IN GAME",
              {0, 2.05f, 0}, b.eye, b.pixelAngle);
        std::printf("batman_gear\n");
        ok &= write(dir + "batman_gear.bmp", cells);
    }
    { // BATMAN grapple: the line flies out, then taut (the pull itself is the game's motion: simulated here)
        const std::string d = clipDir(dir, "batman_grapple");
        const Vec3 hook{2, 9, -14};
        for (int n = 0; n < 45; ++n) {
            const float t = n / fps;
            const float pulled = t > .2f ? std::min(1.f, (t - .2f) / 1.3f) : 0.f;
            Cell c;
            c.pixelAngle = .0028f;
            const Vec3 player = Vec3{0, 0, 0} + (hook - Vec3{0, 3, 0}) * (pulled * pulled * .5f);
            const Vec3 muzzle = player + Vec3{.25f, 1.45f, -.35f};
            c.eye = {-9, 4, -3};
            c.target = {1, 5, -8};
            body(c.world, player, c.eye, {.05f, .05f, .06f});
            hand(c.world, muzzle, c.eye);
            appendGrappleLine(c.world, muzzle, hook, c.eye, c.pixelAngle, grappleOut(t));
            label(c, "BATMAN GRAPPLE: THE LINE FLIES OUT, THEN PULLS", "TRIGGER (A GUN THAT FIRES A LINE); SUSTAINED PULL TO THE HOOK",
                  player + Vec3{.5f, 3.6f, -3}, c.eye, c.pixelAngle);
            ok &= frame(d, n, c);
        }
        std::printf("batman_grapple\n");
    }
    { // SUPERMAN: Play drives the beam; the head turns; headLockBeam redraws from the rendered head
        const std::string d = clipDir(dir, "superman_heat");
        Play p;
        PlayInput in;
        in.dt = 1 / fps;
        in.play = true;
        in.hero = hero_modes::superman;
        const hero_modes::BatonEnemy foe[2] = {{9, {-.8f, 1.3f, -7}}, {10, {1.6f, 1.3f, -7}}};
        in.list = foe;
        in.count = 2;
        for (int n = 0; n < 75; ++n) {
            const float t = n / fps;
            const float yaw = .12f - .35f * std::sin(t * 1.6f); // the head sweeping across
            in.head = {{0, 1.7f, 0}, Quat::around({0, 1, 0}, yaw) * Quat::around({1, 0, 0}, -.05f)};
            for (auto& h : in.hands)
                h = {{.2f, 1.2f, -.3f}, {0, 0, -1}, true, t < 2.2f ? 1.f : 0.f, 0, false};
            PlayOutput o;
            p.step(in, o);
            Draw dr;
            p.draw(dr, Pose{}, in, 0);
            headLockBeam(dr, in.head);
            Cell c;
            c.pixelAngle = .0021f;
            c.eye = {-4.5f, 2.6f, -.2f};
            c.target = {-.2f, 1.5f, -3.6f};
            body(c.world, {0, 0, 0}, c.eye, {.08f, .12f, .45f});
            for (const auto& e : foe)
                body(c.world, e.chest - Vec3{0, 1.3f, 0}, c.eye, {.05f, .05f, .07f});
            appendHero007(c.world, dr, {}, c.eye, c.pixelAngle);
            label(c, "SUPERMAN HEAT VISION: TWO THIN BEAMS, LOCKED TO THE HEAD",
                  o.loopsOn(sfx::heatVision) ? "HIT SPARKS, SCORCH ON HIM; SOUND: ONE SEAMLESS LOOP (ON)" : "LET GO: THE LOOP STOPS, THE SCORCH FADES",
                  {.3f, 2.9f, -4}, c.eye, c.pixelAngle);
            ok &= frame(d, n, c);
        }
        std::printf("superman_heat\n");
    }
    { // PUNISHER thigh holsters
        std::vector<Cell> cells(1);
        Cell& b = cells[0];
        b.eye = {1.3f, 1.5f, 1.5f};
        b.target = {0, 1.0f, 0};
        b.pixelAngle = .0019f;
        body(b.world, {0, 0, 0}, b.eye, {.06f, .06f, .07f});
        const Vec3 head{0, 1.7f, 0};
        holster::BodyYaw yaw;
        for (const uint32_t z : {static_cast<uint32_t>(holster::leftHip), static_cast<uint32_t>(holster::rightHip)}) {
            Vec3 a, f, u;
            wornPose(z, head, yaw, a, f, u, hipThigh);
            appendItem(b.world, Item{lookPistol, a, f, u, 0, 4, z == holster::leftHip ? 0u : 1u}, b.eye, b.pixelAngle);
        }
        label(b, "PUNISHER: DROP-LEG THIGH HOLSTERS", "A PISTOL ON EACH THIGH, LEG STRAPS (THE SAWED-OFF STAYS ON THE BACK)",
              {0, 2.05f, 0}, b.eye, b.pixelAngle);
        std::printf("punisher_holsters\n");
        ok &= write(dir + "punisher_holsters.bmp", cells);
    }
    { // PUNISHER shots: Play draws, aims along the aim, fires
        const std::string d = clipDir(dir, "punisher_shots");
        Play g;
        PlayInput in;
        in.dt = 1 / 90.f;
        in.play = true;
        in.hero = hero_modes::punisher;
        in.head = {{0, 1.7f, 0}, {}};
        const hero_modes::BatonEnemy foe[1] = {{77, {.35f, 1.35f, -6.f}}};
        in.list = foe;
        in.count = 1;
        const Vec3 rightHip{.25f, 1.0f, 0}, outR{.22f, 1.4f, -.45f};
        const Vec3 aim = normalized(foe[0].chest - outR);
        const auto set = [&](Vec3 r, float grip, float trig) {
            in.hands[0] = {{-.4f, 1.25f, -.2f}, {0, 0, -1}, true, 0, 0, false};
            in.hands[1] = {r, aim, true, trig, grip, false};
            in.hands[1].holdGun({r, normalized(aim + Vec3{0, -.5f, 0}), {0, 1, 0}, true}); // the forearm off the aim
            PlayOutput o;
            g.holsterStep(in, o);
            g.step(in, o);
        };
        set(rightHip, 1, 0);
        set(outR, 1, 0);
        for (int n = 0; n < 60; ++n) {
            for (int s = 0; s < 3; ++s) // 90 Hz steps, a 30 fps frame
                set(outR, 1, (n % 12) < 1 && s == 0 ? 1.f : 0.f);
            Draw dr;
            g.draw(dr, Pose{}, in, 0);
            Cell c;
            c.pixelAngle = .0013f;
            c.eye = {-1.4f, 1.75f, 1.0f};
            c.target = {.35f, 1.35f, -2.0f};
            body(c.world, foe[0].chest - Vec3{0, 1.3f, 0}, c.eye, {.05f, .05f, .07f});
            hand(c.world, outR, c.eye);
            appendHero007(c.world, dr, {}, c.eye, c.pixelAngle);
            label(c, "PUNISHER: BARREL ON THE AIM, MUZZLE FLASH, HIT PUFFS", "HE STAGGERS (140); THE SAWED-OFF SHOVES (380); AMMO READOUT KEPT",
                  {.3f, 2.3f, -3}, c.eye, c.pixelAngle);
            ok &= frame(d, n, c);
        }
        std::printf("punisher_shots\n");
    }
    { // CAP reticle before / after
        std::vector<Cell> cells(2);
        for (int k = 0; k < 2; ++k) {
            Cell& c = cells[static_cast<size_t>(k)];
            c.eye = {0, 1.7f, 0};
            c.target = {0, 1.3f, -8};
            c.pixelAngle = .0012f;
            body(c.world, {0, 0, -8}, c.eye, {.05f, .05f, .07f});
            AimMarker m;
            m.kind = AimMark::target;
            m.point = {0, 1.0f, -8};
            m.radius = 1.3f;
            m.hand = 1;
            m.lock = .4f;
            if (k == 1) {
                m.radius = captain::subtleRadius(m.radius);
                m.lock = 1;
                m.opacity *= captain::reticleOpacity;
            }
            appendAimMarker(c.world, m, c.eye, c.pixelAngle);
            label(c, k ? "AFTER: CAP'S MARKER SMALL AND FAINT" : "BEFORE: THE ENEMY-SIZED BLUE RETICLE",
                  k ? "0.15 M, HALF OPACITY, NO WIDE LOCK-ON RING; AIM UNCHANGED" : "HIS RADIUS (ABOUT 3 M ACROSS WHILE LOCKING ON)",
                  {0, 2.6f, -7}, c.eye, c.pixelAngle);
        }
        std::printf("cap_reticle\n");
        ok &= write(dir + "cap_reticle.bmp", cells);
    }
    { // CAP shield: thrown, flies home, caught onto the arm
        const std::string d = clipDir(dir, "cap_shield");
        hero_modes::Baton b;
        b.flyHome = true;
        const Vec3 handAt{.3f, 1.3f, -.25f};
        float since = 1e3f;
        Vec3 caughtAt{};
        bool was = true;
        for (int n = 0; n < 90; ++n) {
            const float dt = 1 / fps;
            const bool press = n == 3;
            for (int s = 0; s < 3; ++s)
                b.update(dt / 3, press && s == 0, handAt, {0, .05f, -1}, {.4f, 1.2f, -9}, true);
            const bool inHand = b.inHandNow();
            if (inHand && !was) {
                since = 0;
                caughtAt = b.position;
            }
            if (!inHand)
                caughtAt = b.position;
            was = inHand;
            since += dt;
            Cell c;
            c.pixelAngle = .0026f;
            c.eye = {-7.f, 3.f, -4.6f};
            c.target = {.3f, 1.3f, -4.8f};
            body(c.world, {.4f, 0, -9.5f}, c.eye, {.05f, .05f, .07f});
            hand(c.world, handAt, c.eye);
            captain::ShieldPose arm;
            arm.at = handAt;
            arm.face = normalized(Vec3{.7f, 0, -.7f});
            arm.up = normalized(Vec3{0, .3f, -1});
            arm.ok = true;
            if (inHand) {
                const auto p = captain::catchPose(arm, caughtAt, since - dt);
                appendItem(c.world, Item{lookShield, p.at, p.face, p.up, 0, 0, 1}, c.eye, c.pixelAngle);
            } else
                appendItem(c.world, Item{lookShieldSpin, b.position, {0, 1, 0}, {0, 0, -1}, n * .6f}, c.eye, c.pixelAngle);
            label(c, "CAP: THE SHIELD FLIES HOME AND IS CAUGHT", "SLOWS FOR THE CATCH, EASES ONTO THE ARM (NO SNAP)",
                  {.2f, 2.8f, -4}, c.eye, c.pixelAngle);
            ok &= frame(d, n, c);
        }
        std::printf("cap_shield\n");
    }
    { // WOLVERINE claws: a slash
        const std::string d = clipDir(dir, "wolverine_claws");
        for (int n = 0; n < 40; ++n) {
            const float t = n / 40.f, s = std::sin(t * 3.14159f);
            Cell c;
            c.pixelAngle = .0012f;
            c.eye = {-.55f, 1.8f, .55f};
            c.target = {0, 1.35f, -.35f};
            const Vec3 at{.35f - .6f * t, 1.55f - .4f * t, -.3f - .15f * s};
            const Vec3 f = normalized(Vec3{-.4f + .2f * t, .2f - .3f * t, -1});
            hand(c.world, at, c.eye);
            appendItem(c.world, Item{lookClaws, at, f, {0, 1, 0}}, c.eye, c.pixelAngle);
            label(c, "WOLVERINE: CLAWS OUT, A SLASH", "CURVED ADAMANTIUM CLAWS (0.0.72 LOOK), HEALING MID-FIGHT",
                  {0, 1.85f, -.4f}, c.eye, c.pixelAngle);
            ok &= frame(d, n, c);
        }
        std::printf("wolverine_claws\n");
    }
    { // the van
        std::vector<Cell> cells(1);
        Cell& c = cells[0];
        c.eye = {0, 1.7f, 2};
        c.target = {0, 1.5f, -3};
        c.pixelAngle = .0016f;
        label(c, "VAN: MODEL MISSING", "NO VAN MODEL PICKED: NOTHING SPAWNS; MOVEMENT NOT WIRED (CANDIDATE ONLY)", {0, 1.6f, -3}, c.eye, c.pixelAngle);
        std::printf("van\n");
        ok &= write(dir + "van.bmp", cells);
    }
    return ok ? 0 : 1;
}
