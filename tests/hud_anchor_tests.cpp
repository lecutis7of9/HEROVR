// 82.1 HUD ANCHOR: the world-anchored cards and the lazy HUD class. Standalone: no Windows headers.
#include "spidy/hud_anchor.hpp"
#include "spidy/native_view.hpp"
#include <cstdio>
#include <cstdlib>
#define CHECK(c) do { if(!(c)) { std::fprintf(stderr, "FAILED %s:%d %s\n", __FILE__, __LINE__, #c); std::exit(1); } } while(0)
using namespace spidy;
using namespace spidy::hud_anchor;
namespace {
// A native view pose (rows right, down, forward, position) from an OpenXR-style pose.
Mat4 nativePose(const Pose& p) {
    const Vec3 r = p.orientation.rotate({1, 0, 0}), u = p.orientation.rotate({0, 1, 0}), b = p.orientation.rotate({0, 0, 1});
    return {r.x, r.y, r.z, 0, -u.x, -u.y, -u.z, 0, -b.x, -b.y, -b.z, 0, p.position.x, p.position.y, p.position.z, 1};
}
constexpr float tl = -1.2f, tr = 1.0f, tu = 1.1f, td = -1.2f; // an eye's lens tangents (asymmetric on purpose)
Mat4 eyeVp(const Pose& eye) {
    return multiply(projection(std::atan(tl), std::atan(tr), std::atan(td), std::atan(tu), .03f, 200.f), native_view::view(nativePose(eye)));
}
// The game's own projection of a world point: tangent point (x right, y up) seen from a native camera pose.
bool gameProject(const Mat4& cam, Vec3 world, float& x, float& y) {
    const Vec3 d = world - position(cam);
    const Vec3 right{cam[0], cam[1], cam[2]}, down{cam[4], cam[5], cam[6]}, fwd{cam[8], cam[9], cam[10]};
    const float depth = dot(d, fwd);
    if (depth < .1f)
        return false;
    x = dot(d, right) / depth;
    y = -dot(d, down) / depth;
    return true;
}
// A small lit region of a 1280x720 layer centred on the tangent point.
prompt_layer::Region regionAt(const Lens& lens, float x, float y, uint32_t half = 20) {
    const float u = (x / lens.halfW + 1) * .5f, v = (1 - y / lens.halfH) * .5f;
    const uint32_t cx = static_cast<uint32_t>(u * 1280), cy = static_cast<uint32_t>(v * 720);
    return {cx - half, cy - half, cx + half, cy + half, 9};
}
} // namespace
int main() {
    const unsigned W = 1832, H = 1920;
    { // the lens: the game camera's own when it has the layer's shape, else the 72 degree guess
        const float good[4]{-.7f, .7f, -.39375f, .39375f}, odd[4]{-.7f, .7f, -.7f, .7f}, bad[4]{.7f, -.7f, -.4f, .4f};
        auto a = chooseLens(good, true, 1280, 720);
        CHECK(a.fromGame && std::abs(a.halfW - .7f) < 1e-5f && std::abs(a.halfH - .39375f) < 1e-5f);
        CHECK(!chooseLens(odd, true, 1280, 720).fromGame);
        CHECK(!chooseLens(bad, true, 1280, 720).fromGame);
        auto f = chooseLens(good, false, 1280, 720);
        CHECK(!f.fromGame && std::abs(f.halfW - prompt_layer::fieldTan) < 1e-6f && std::abs(f.halfH - prompt_layer::fieldTan * 9 / 16.f) < 1e-5f);
        CHECK(!chooseLens(nullptr, true, 1280, 720).fromGame && !chooseLens(good, true, 0, 0).fromGame);
    }
    const Lens lens = chooseLens(nullptr, false, 1280, 720);
    const Pose camPose{{0, 2, 5}, Quat::yaw(.3f)}; // the game camera: behind the hero, turned
    const Mat4 cam = nativePose(camPose);
    { // the game's projection run backwards: the ray through a marker's layer place passes through the marker
        const Vec3 target{4, 9, -30};
        float x{}, y{};
        CHECK(gameProject(cam, target, x, y));
        const Vec3 dir = rayDirection(cam, x, y);
        const Vec3 d = normalized(target - position(cam));
        CHECK(length(dir - d) < 1e-4f);
    }
    { // a WORLD piece stays over its target in both eyes and when the head turns
        const Vec3 target{4, 9, -30};
        float x{}, y{};
        CHECK(gameProject(cam, target, x, y));
        const float depth = length(target - position(cam)); // the anchor depth equals the target's: exact
        Tracker t;
        const auto r = regionAt(lens, x, y);
        t.read(&r, 1, 1280, 720, lens, &cam, depth, anchorAuto, 1000, .1f);
        const Slot* s = nullptr;
        for (const auto& slot : t.slots())
            if (slot.used)
                s = &slot;
        CHECK(s && !s->hud && s->haveWorld && length(s->world - target) < .05f);
        // two eyes 64 mm apart, then the head turned 25 degrees: the rect's centre is the target's own projection
        for (float yaw : {0.f, .45f}) {
            for (float side : {-.032f, .032f}) {
                const Pose head{{1, 1.7f, 2}, Quat::yaw(yaw)};
                const Pose eye = compose(head, Pose{{side, 0, 0}, {}});
                const Mat4 vp = eyeVp(eye);
                float rect[4], crop[4];
                const bool shown = anchoredRect(vp, s->world, .2f, .12f, s->region, 1280, 720, tl, tr, tu, td, W, H, rect, crop);
                const float cx = vp[0] * target.x + vp[1] * target.y + vp[2] * target.z + vp[3];
                const float cy = vp[4] * target.x + vp[5] * target.y + vp[6] * target.z + vp[7];
                const float cw = vp[12] * target.x + vp[13] * target.y + vp[14] * target.z + vp[15];
                if (cw > .05f && std::abs(cx / cw) < .9f && std::abs(cy / cw) < .9f) {
                    CHECK(shown);
                    const float px = (cx / cw + 1) * .5f * W, py = (1 - cy / cw) * .5f * H;
                    CHECK(std::abs((rect[0] + rect[2]) * .5f - px) < 1.5f && std::abs((rect[1] + rect[3]) * .5f - py) < 1.5f);
                    CHECK(crop[2] > crop[0] && crop[3] > crop[1] && crop[0] >= 0 && crop[2] <= 1);
                }
            }
        }
        // the two eyes see it at different pixels (per-eye parallax) at a near anchor
        Tracker near;
        near.read(&r, 1, 1280, 720, lens, &cam, 4.f, anchorWorld, 1000, .1f);
        const Slot* n = nullptr;
        for (const auto& slot : near.slots())
            if (slot.used)
                n = &slot;
        CHECK(n);
        const Pose head{{0, 2, 5}, Quat::yaw(.3f)};
        float a[4], b[4], ca[4], cb[4];
        CHECK(anchoredRect(eyeVp(compose(head, Pose{{-.032f, 0, 0}, {}})), n->world, .2f, .12f, n->region, 1280, 720, tl, tr, tu, td, W, H, a, ca));
        CHECK(anchoredRect(eyeVp(compose(head, Pose{{.032f, 0, 0}, {}})), n->world, .2f, .12f, n->region, 1280, 720, tl, tr, tu, td, W, H, b, cb));
        CHECK(std::abs(a[0] - b[0]) > .5f);
    }
    { // behind the eye or off the view: nothing; a tiny card: nothing
        const Mat4 vp = eyeVp(Pose{{0, 0, 0}, {}});
        float rect[4], crop[4];
        const prompt_layer::Region r{100, 100, 140, 140, 4};
        CHECK(!anchoredRect(vp, {0, 0, 10}, .2f, .12f, r, 1280, 720, tl, tr, tu, td, W, H, rect, crop)); // behind
        CHECK(!anchoredRect(vp, {500, 0, -5}, .2f, .12f, r, 1280, 720, tl, tr, tu, td, W, H, rect, crop)); // off to the side
        CHECK(!anchoredRect(vp, {0, 0, -5}, .001f, .001f, r, 1280, 720, tl, tr, tu, td, W, H, rect, crop)); // tiny
        CHECK(anchoredRect(vp, {0, 0, -5}, .2f, .12f, r, 1280, 720, tl, tr, tu, td, W, H, rect, crop));
        // a card cut by the view's edge keeps its picture's matching part
        const float edge = 5.f * tr;
        CHECK(anchoredRect(vp, {edge, 0, -5}, .4f, .24f, r, 1280, 720, tl, tr, tu, td, W, H, rect, crop));
        CHECK(rect[2] <= W + .01f && crop[2] < 140.f / 1280 + 1e-4f && crop[2] > .5f * (100.f / 1280 + 140.f / 1280) - .01f);
    }
    { // classes: the edges are HUD, the middle is world; hysteresis; the modes overrule
        Tracker t;
        const prompt_layer::Region minimap{20, 520, 200, 700, 80}, diamond{600, 300, 660, 380, 30}, health{20, 20, 200, 60, 30};
        const prompt_layer::Region all[3]{minimap, diamond, health};
        t.read(all, 3, 1280, 720, lens, &cam, 40.f, anchorAuto, 1000, .1f);
        int hud = 0, world = 0;
        for (const auto& s : t.slots())
            if (s.used)
                (s.hud ? hud : world)++;
        CHECK(hud == 2 && world == 1);
        CHECK(Tracker::classify(anchorHead, .5f, .5f, false, false) && !Tracker::classify(anchorWorld, .01f, .5f, true, true));
        // on the line: a world piece needs to go well inside the band before it is HUD, and the reverse
        CHECK(!Tracker::classify(anchorAuto, bandU - .01f, .5f, false, true));
        CHECK(Tracker::classify(anchorAuto, bandU + .01f, .5f, true, true));
        CHECK(Tracker::classify(anchorAuto, bandU - .01f, .5f, false, false));
    }
    { // smoothing: a piece whose box jitters by a cell is steadier than its raw boxes; a moving one follows
        Tracker t;
        const Vec3 target{0, 2, -20};
        float x{}, y{};
        CHECK(gameProject(cam, target, x, y));
        float rawMax = 0, smoothMax = 0;
        Vec3 first{}, last{};
        for (int i = 0; i < 40; ++i) {
            auto r = regionAt(lens, x, y, 20);
            const uint32_t jitter = (i % 2) ? 14 : 0; // a lit piece's box changes with the picture inside it
            r.x0 += jitter;
            r.x1 += jitter;
            t.read(&r, 1, 1280, 720, lens, &cam, 20.f, anchorWorld, 1000 + i * 100, .1f);
            for (const auto& s : t.slots())
                if (s.used) {
                    if (i == 5)
                        first = s.world;
                    last = s.world;
                    if (i > 5) {
                        smoothMax = std::max(smoothMax, length(s.world - first));
                        float rx{}, ry{};
                        tangentAt(lens, (r.x0 + r.x1) * .5f / 1280, (r.y0 + r.y1) * .5f / 720, rx, ry);
                        rawMax = std::max(rawMax, length(position(cam) + rayDirection(cam, rx, ry) * 20.f - first));
                    }
                }
        }
        CHECK(smoothMax < rawMax && smoothMax < .75f * rawMax + 1e-4f);
        CHECK(t.counts().created == 1 && t.counts().matched == 39);
        (void)last;
    }
    { // no camera: world pieces wait; HUD pieces do not need one; gone pieces expire
        Tracker t;
        const prompt_layer::Region diamond{600, 300, 660, 380, 30}, health{20, 20, 200, 60, 30};
        const prompt_layer::Region both[2]{diamond, health};
        t.read(both, 2, 1280, 720, lens, nullptr, 40.f, anchorAuto, 1000, .1f);
        int worldAnchored = 0, huds = 0;
        for (const auto& s : t.slots())
            if (s.used) {
                worldAnchored += s.haveWorld;
                huds += s.hud;
            }
        CHECK(worldAnchored == 0 && huds == 1 && t.counts().noCamera == 1);
        t.read(nullptr, 0, 1280, 720, lens, &cam, 40.f, anchorAuto, 1000 + slotMaxAgeMs + 10, .1f);
        for (const auto& s : t.slots())
            CHECK(!s.used);
        CHECK(t.counts().dropped == 2);
    }
    { // the HUD class's point: in front of the head along the panel's facing, at panelDistance
        const Vec3 head{3, 2, 1};
        const Quat facing = Quat::yaw(.5f);
        const Vec3 p = panelPoint(head, facing, 0, 0);
        CHECK(std::abs(length(p - head) - panelDistance) < 1e-4f);
        CHECK(dot(normalized(p - head), facing.rotate({0, 0, -1})) > .9999f);
        const Vec3 left = panelPoint(head, facing, -.5f, 0);
        CHECK(dot(left - head, facing.rotate({1, 0, 0})) < 0);
    }
    std::puts("hud_anchor tests passed");
    return 0;
}
