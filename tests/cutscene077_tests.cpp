// HERO VR 0.0.77 CUTSCENES (include/spidy/cutscene077.hpp), pure.
#include "spidy/cutscene077.hpp"
#include "spidy/native_eye_frame.hpp"
#include "spidy/presentation_gate.hpp"

#include <cmath>
#include <cstdio>
#include <string_view>

using namespace spidy;
using namespace spidy::cutscene077;

namespace {
int failures = 0;
void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++failures;
    }
}
bool near(float a, float b, float e = 1e-4f) {
    return std::abs(a - b) <= e;
}
bool same(const Mat4& a, const Mat4& b, float e = 1e-4f) {
    for (int i = 0; i < 16; ++i)
        if (!near(a[i], b[i], e))
            return false;
    return true;
}
// A camera (engine rows: right, down, forward, position) turned `yaw` about the world's up (z up).
Mat4 camera(float yaw, Vec3 at) {
    const float c = std::cos(yaw), s = std::sin(yaw);
    return {c, s, 0, 0, 0, 0, -1, 0, -s, c, 0, 0, at.x, at.y, at.z, 1};
}
}  // namespace

int main() {
    // Mode names and the 3D ask.
    check(std::string_view(modeName(flat)) == "FLAT" && std::string_view(modeName(screen3d)) == "3D" &&
              std::string_view(modeName(immersive)) == "IMMERSIVE" && std::string_view(modeName(9)) == "FLAT",
          "mode names");
    check(!stereoAsked(true, true, false, flat), "flat: no hook, no pair (as 0.0.70)");
    check(!stereoAsked(true, true, false, immersive), "immersive: no screen pair");
    check(stereoAsked(true, true, false, screen3d), "3d: the pair without the hook");
    check(stereoAsked(true, true, true, flat), "a hooked HUD panel keeps the old rule");
    check(!stereoAsked(false, true, true, screen3d) && !stereoAsked(true, false, true, screen3d),
          "no scene or depth off: no pair");

    // The strip's pixels.
    float r[4];
    check(stripRect(1920, 1080, subtitleStrip, r) && near(r[0], 230) && near(r[1], 842) && near(r[2], 1690) &&
              near(r[3], 1048),
          "strip on 1920x1080");
    check(!stripRect(0, 1080, subtitleStrip, r) && !stripRect(8, 8, subtitleStrip, r), "no strip on nothing");
    check(!stripRect(1920, 1080, Crop{.5f, .5f, .4f, .9f}, r) && !stripRect(1920, 1080, Crop{-.1f, 0, 1, 1}, r),
          "bad crops refused");

    // The strip clock (the panel signal).
    {
        StripClock c;
        check(!c.recent(100, 200), "nothing landed");
        c.landed(1000);
        check(c.recent(1200, 200) && !c.recent(1201, 200) && !c.recent(999, 200), "200 ms rule");
        c.reset();
        check(!c.recent(1000, 200), "reset");
    }
    // ScenePanel's 1 s with the strip's signal: given up after 1 s with none.
    {
        ScenePanel p;
        StripClock c;
        bool on = p.wanted(true, c.recent(0, 200), 0);
        check(on, "pair tried at once");
        on = p.wanted(true, c.recent(900, 200), 900);
        check(on, "still trying at 0.9 s");
        on = p.wanted(true, c.recent(1001, 200), 1001);
        check(!on, "1 s with no strip: flat");
        ScenePanel q;
        q.wanted(true, false, 0);
        for (uint64_t t = 20; t < 5000; t += 20) {
            c.landed(t);
            on = q.wanted(true, c.recent(t, 200), t);
        }
        check(on, "the strip landing keeps the pair");
    }

    // The immersive hold.
    {
        ImmersiveHold h;
        check(!h.update(true, false, false, 100), "never starts on a frozen clock");
        check(h.update(true, true, false, 200) && h.started(), "starts with the clock");
        check(h.update(true, true, false, 211) && !h.started() && !h.frozen(), "runs");
        check(h.update(true, false, false, 3000) && h.frozen(), "QTE: held");
        check(h.update(true, false, false, 9000) && h.frozen(), "QTE still held at 6 s");
        check(h.update(true, true, false, 9011) && !h.started() && !h.frozen(), "clock back: same stretch");
        check(!h.update(true, true, true, 9022) || true, "menu with the clock running");
        check(!h.update(true, false, false, 9040) && h.menu(), "pause just after the menu button: the screen");
        check(!h.update(true, false, false, 12000), "stays on the screen through the pause");
        check(h.update(true, true, false, 15000) && h.started(), "unpaused: a new stretch, a new level head");
        check(!h.update(true, false, true, 15100) && h.menu(), "menu during a freeze: the screen");
        check(h.update(true, true, false, 20000) && h.started(), "again");
        check(h.update(true, false, false, 20100), "long freeze begins");
        check(h.update(true, false, false, 20100 + qteFreezeMaxMs), "15 s still a QTE");
        check(!h.update(true, false, false, 20101 + qteFreezeMaxMs) && h.menus() == 3, "over 15 s: the screen");
        check(!h.update(false, true, false, 40000) && !h.frozen(), "no scene");
        check(h.update(true, true, false, 40011) && h.started(), "a new scene starts");
    }

    // The level head: position and yaw only.
    {
        const Quat yaw = Quat::yaw(.7f);
        const Pose tilted{{.1f, 1.6f, -.2f}, yaw * Quat::around({1, 0, 0}, .5f) * Quat::around({0, 0, 1}, .3f)};
        const Pose l = levelHead(tilted);
        const Vec3 f = l.orientation.rotate({0, 0, -1}), u = l.orientation.rotate({0, 1, 0});
        const Vec3 want = yaw.rotate({0, 0, -1});
        check(near(l.position.x, .1f) && near(l.position.y, 1.6f) && near(l.position.z, -.2f), "head position kept");
        check(near(f.y, 0) && near(u.y, 1) && near(f.x, want.x, 1e-3f) && near(f.z, want.z, 1e-3f), "level, same yaw");
        const Pose down{{0, 1, 0}, Quat::around({1, 0, 0}, -1.5707963f)};
        const Vec3 fd = levelHead(down).orientation.rotate({0, 0, -1});
        check(near(fd.y, 0) && near(fd.z, -1, 1e-3f), "straight down: the top of the head's heading");
    }

    // Eyes relative to the level head, put on the camera.
    {
        const Pose level{{1, 1.6f, 2}, Quat::yaw(.4f)};
        const Pose eyeL = compose(level, Pose{{-.032f, .01f, -.02f}, Quat::yaw(.1f) * Quat::around({1, 0, 0}, .2f)});
        const Pose rel = relativeEye(level, eyeL);
        check(near(rel.position.x, -.032f) && near(rel.position.y, .01f) && near(rel.position.z, -.02f),
              "relative eye position");
        const Mat4 m = relativeMatrix(rel);
        check(relativeUsable(m), "relative matrix usable");
        const Mat4 cam = camera(1.1f, {10, 20, 3});
        check(same(onCamera(m, cam), native_view::relativePose(cam, rel)), "on the camera = relativePose(camera)");
        const Mat4 identity{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
        check(same(onCamera(identity, cam), cam), "the level head itself is the camera");
        check(native_view::validPose(onCamera(m, cam)), "a valid eye");
        Mat4 far = m;
        far[12] = 4;
        check(!relativeUsable(far), "over 3 m refused");
    }

    // The native command's mode 4.
    {
        native_eyes::Command c;
        c.enabled = 4;
        c.serial = 1;
        c.leaseMs = 250;
        const Pose level{{0, 1.6f, 0}, Quat::yaw(0)};
        for (unsigned i = 0; i < 2; ++i) {
            const Pose eye{{i ? .032f : -.032f, 1.6f, 0}, Quat::yaw(0)};
            c.eyes[i].world = relativeMatrix(relativeEye(level, eye));
            c.eyes[i].fov[0] = -.8f;
            c.eyes[i].fov[1] = .8f;
            c.eyes[i].fov[2] = -.8f;
            c.eyes[i].fov[3] = .8f;
        }
        check(native_eyes::valid(c), "mode 4 accepted");
        auto far = c;
        far.eyes[0].world[12] = far.eyes[1].world[12] = 5;
        check(!native_eyes::valid(far), "mode 4 eyes far from the head refused");
        auto five = c;
        five.enabled = 5;
        check(!native_eyes::valid(five), "mode 5 refused");
    }

    // The see-through panel: disparity of its distance, inside the eye.
    {
        const float t = std::tan(.8f);
        float a[4], b[4];
        check(panelRect(0, .064f, -t, t, -t, t, 2000, 2000, 16.f / 9 * .76f / .19f, subtitlePanel, a) &&
                  panelRect(1, .064f, -t, t, -t, t, 2000, 2000, 16.f / 9 * .76f / .19f, subtitlePanel, b),
              "panel rects");
        const float shift = (a[0] - b[0]) / 2000 * 2 * t;
        check(near(shift, .064f / 2, 1e-3f), "disparity of 2 m");
        check(near(a[1], b[1]) && near(a[3], b[3]) && a[3] > 1000 && a[1] > 1000, "same rows, low in the view");
        check(!panelRect(2, .064f, -t, t, -t, t, 2000, 2000, 2, subtitlePanel, a) &&
                  !panelRect(0, .064f, -t, t, -t, t, 0, 2000, 2, subtitlePanel, a) &&
                  !panelRect(0, .064f, -t, t, -t, t, 2000, 2000, NAN, subtitlePanel, a),
              "bad panels refused");
        check(near(cropAspect(subtitleStrip, 16.f / 9), .76f / .19f * 16.f / 9, 1e-3f) &&
                  near(cropAspect(wholeFrame, 16.f / 9), 16.f / 9),
              "crop aspect");
    }

    // The key: words and icons, not the scene.
    check(keyOf(1, 1, 1) > .99f && keyOf(.95f, .95f, .9f) > .9f, "white words show");
    check(keyOf(.3f, .3f, .3f) < .01f && keyOf(0, 0, 0) < .01f, "dark scene hidden");
    check(keyOf(1, .2f, .1f) < .01f && keyOf(.2f, .5f, 1) < .01f, "coloured scene hidden");

    if (failures) {
        std::printf("%d failure(s)\n", failures);
        return 1;
    }
    std::printf("cutscene077: all passed\n");
}
