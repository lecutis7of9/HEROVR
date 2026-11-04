// HERO VR 0.0.77 WRIST MAP (include/spidy/story077h.hpp), pure: the gesture, the HUD crop and the wrist panel.
#include "spidy/story077h.hpp"

#include <cmath>
#include <cstdio>
#include <string_view>

using namespace spidy;
using namespace spidy::story077h;

namespace {
int failures = 0;
void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++failures;
    }
}
Gesture::Input at(Vec3 left, Vec3 wrist, float trigger, bool play = true) {
    Gesture::Input in;
    in.play = play;
    in.tracked[0] = in.tracked[1] = true;
    in.left = left;
    in.wrist = wrist;
    in.trigger = trigger;
    return in;
}
// A pull: from rest to full and back, one frame each step.
bool pull(Gesture& g, Vec3 left, Vec3 wrist, bool play = true, bool* claimed = nullptr) {
    bool toggled = false;
    for (float t : {0.f, .1f, .4f, .8f, 1.f, .5f, .2f, 0.f}) {
        toggled = g.update(1.f / 90, at(left, wrist, t, play)) || toggled;
        if (claimed && t > .05f && t < .9f)
            *claimed = *claimed && g.claims();
    }
    return toggled;
}
} // namespace

int main() {
    const Vec3 wrist{.2f, 1.f, -.3f};
    {   // THE GESTURE
        Gesture g;
        bool claimed = true;
        check(pull(g, wrist + Vec3{.04f, 0, 0}, wrist, true, &claimed), "at the wrist: toggles");
        check(g.shown(), "shown after one pull");
        check(claimed, "the whole pull is the map's, its light start too");
        check(!g.claims(), "let go: the trigger is the game's again");
        for (int i = 0; i < 40; ++i)
            g.update(1.f / 90, at(wrist + Vec3{.04f, 0, 0}, wrist, 0));
        check(pull(g, wrist + Vec3{.04f, 0, 0}, wrist), "again: toggles");
        check(!g.shown(), "hidden after the second pull");
        Gesture far;
        bool farClaim = true;
        check(!pull(far, wrist + Vec3{.15f, 0, 0}, wrist, true, &farClaim), "15 cm away: never (a web pull)");
        check(!farClaim && far.diagnostics().outOfReach == 1, "15 cm away: the trigger stays the game's");
        Gesture off;
        check(!pull(off, wrist, wrist, false), "not playing: never");
        check(off.diagnostics().blocked == 1, "not playing: counted");
        Gesture busy;
        auto in = at(wrist, wrist, .5f);
        in.busy = true;
        busy.update(.01f, in);
        check(!busy.claims() && busy.diagnostics().blocked == 1, "busy (holo, gun, web): never");
        Gesture lost;
        auto un = at(wrist, wrist, 1.f);
        un.tracked[1] = false;
        lost.update(.01f, un);
        check(!lost.claims() && !lost.shown(), "untracked: never");
        // A fast hand (a punch) passing the wrist.
        Gesture fast;
        fast.update(1.f / 90, at(wrist + Vec3{.5f, 0, 0}, wrist, 0));
        fast.update(1.f / 90, at(wrist + Vec3{.03f, 0, 0}, wrist, .5f));
        check(!fast.claims() && fast.diagnostics().tooFast == 1, "a fast hand: never");
        // Rearm: a second pull right after the first is held off.
        Gesture quick;
        quick.update(.01f, at(wrist, wrist, .1f));
        quick.update(.01f, at(wrist, wrist, .9f));
        quick.update(.01f, at(wrist, wrist, 0.f));
        quick.update(.01f, at(wrist, wrist, .1f));
        quick.update(.01f, at(wrist, wrist, .9f));
        check(quick.shown() && quick.diagnostics().rearming == 1, "a second pull within rearm: held off");
        Gesture nan;
        nan.update(NAN, at(wrist, wrist, NAN));
        check(!nan.claims() && !nan.shown(), "NaN: nothing");
        check(touchMetres <= .1f, "a tight radius (10 cm or less)");
    }
    {   // THE CROP
        Crop c;
        check(hudCrop(1920, 1080, 100, c), "1080p: a crop");
        check(c.u0 > 0 && c.u1 < .25f && c.v0 > .6f && c.v1 < 1.f, "1080p: the bottom-left corner");
        check(std::abs(cropAspect(c, 1920, 1080) - 1.f) < .01f, "1080p: square in pixels");
        Crop q;
        check(hudCrop(3840, 2160, 100, q), "4k: a crop");
        check(std::abs(q.u0 - c.u0) < 1e-4f && std::abs(q.v1 - c.v1) < 1e-4f, "4k: the same place (by height)");
        Crop wide;
        check(hudCrop(3440, 1440, 100, wide), "ultrawide: a crop");
        check(wide.u1 < c.u1 && std::abs(cropAspect(wide, 3440, 1440) - 1.f) < .01f, "ultrawide: at the left edge, square");
        Crop big;
        check(hudCrop(1920, 1080, 150, big), "HUD 150: a crop");
        check(big.u1 - big.u0 > (c.u1 - c.u0) * 1.4f, "HUD 150: bigger");
        Crop bad;
        check(!hudCrop(0, 1080, 100, bad), "no width: none");
        check(!hudCrop(1920, 1080, 20, bad), "HUD 20: refused");
        check(!hudCrop(1920, 1080, 200, bad), "HUD 200: refused");
    }
    {   // THE PANEL
        const float tl = -1.28f, tr = 1.f, tu = 1.07f, td = -1.43f;
        const Mat4 vp = multiply(projection(std::atan(tl), std::atan(tr), std::atan(td), std::atan(tu), .03f, 200.f),
                                 viewMatrix(Pose{}));
        Crop c{.1f, .6f, .3f, .9f};
        float r[4];
        check(panelRect(vp, {0, -.2f, -.4f}, .1f, 1.f, tl, tr, tu, td, 2064, 2208, c, r), "in front: a rectangle");
        const float wantW = .1f / .4f * 2064 / (tr - tl);
        check(std::abs((r[2] - r[0]) - wantW) < 2.f, "its width from its distance");
        check(std::abs((r[3] - r[1]) - .1f / .4f * 2208 / (tu - td)) < 2.f, "square in the world");
        check(r[1] > 1104, "below the centre: low in the view");
        check(c.u0 == .1f && c.u1 == .3f, "inside: the crop whole");
        Crop behind{.1f, .6f, .3f, .9f};
        check(!panelRect(vp, {0, 0, .4f}, .1f, 1.f, tl, tr, tu, td, 2064, 2208, behind, r), "behind the eye: none");
        Crop edge{0, 0, 1, 1};
        const float leftX = tl * .4f; // the view's left edge at 0.4 m
        check(panelRect(vp, {leftX, 0, -.4f}, .1f, 1.f, tl, tr, tu, td, 2064, 2208, edge, r), "on the edge: cut");
        check(r[0] == 0 && edge.u0 > .4f && edge.u0 < .6f && edge.u1 == 1, "cut: the crop cut alike (never squashed)");
        Crop nanCrop{0, 0, 1, 1};
        check(!panelRect(vp, {NAN, 0, -.4f}, .1f, 1.f, tl, tr, tu, td, 2064, 2208, nanCrop, r), "NaN: none");
        // The panel's centre over the wrist.
        const Vec3 p = panelCentre(Pose{{0, 1, 0}, {}}, 6);
        check(std::abs(p.y - (1.f + .085f + .06f)) < 1e-4f && std::abs(p.x - .045f) < 1e-4f, "6 cm over the wrist");
        check(length(rightWrist(Pose{{0, 1, 0}, {}}) - Vec3{.045f, 1.085f, 0}) < 1e-4f, "the right wrist: the hologram's emitter");
    }
    check(std::string_view(missingText(hudHidden)) == "MINIMAP NOT ON SCREEN", "the note");
    check(std::string_view(missingText(haveMap)).empty(), "a map: no note");
    if (failures)
        return 1;
    std::printf("story077h: all passed\n");
    return 0;
}
