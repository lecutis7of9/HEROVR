// HERO VR 0.0.78 WORLD LIGHT tests (include/spidy/world_light.hpp, car_paint.hpp).
// 1. STUDIO is the 0.0.77 light to the bit: .32 + .68 |n.l| at normalized(.3, 1, .25), car paint unchanged.
// 2. The g_World spans: the fields read at the exe's RDEF offsets.
// 3. derive(): the game's numbers to display light; bad numbers refused (the studio light, said why).
// 4. One-sided sun, the sun's share (the GPU sun shadow's alpha), the soft shoulder, the shadow strength.
#include "spidy/world_light.hpp"
#include "spidy/car_paint.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

using namespace spidy;

namespace {
int failures = 0;
void check(bool ok, const char* what) {
    std::printf("%s: %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok)
        ++failures;
}
bool same(Vec3 a, Vec3 b) { return std::memcmp(&a, &b, sizeof a) == 0; }
bool near(Vec3 a, Vec3 b, float e) { return std::abs(a.x - b.x) < e && std::abs(a.y - b.y) < e && std::abs(a.z - b.z) < e; }
void put(std::vector<unsigned char>& b, uint32_t at, std::initializer_list<float> v) {
    for (float f : v)
        std::memcpy(b.data() + at, &f, 4), at += 4;
}
world_light::Sample noon() {
    world_light::Sample s;
    const Vec3 l = normalized(Vec3{.4f, .8f, -.3f});
    s.toLight[0] = l.x, s.toLight[1] = l.y, s.toLight[2] = l.z;
    s.color[0] = 9.f, s.color[1] = 8.5f, s.color[2] = 7.5f;
    s.fog[0] = .6f, s.fog[1] = .75f, s.fog[2] = 1.f;
    s.probe[0] = s.probe[1] = s.probe[2] = 1.f;
    s.adapted = 2.f;
    return s;
}
} // namespace

int main() {
    using namespace world_light;
    // 1. STUDIO
    const Light st = studio();
    check(!st.world && !st.oneSided && same(st.toSun, normalized(Vec3{.3f, 1.f, .25f})), "studio: the 0.0.77 light, both sides");
    bool identical = true;
    for (int i = 0; i < 200; ++i) {
        const float a = .031f * i, b = .017f * i * i;
        const Vec3 n{std::cos(a) * std::sin(b), std::cos(b), std::sin(a) * std::sin(b)};
        const Vec3 tint{.2f + .003f * i, .5f, .9f - .002f * i};
        const Vec3 old = tint * (.32f + .68f * std::abs(dot(n, st.toSun)));
        const Vec3 now = faceColour(st, tint, n, Vec3{1, 2, 3});
        identical = identical && near(old, now, 1e-6f);
        // car paint: the default light and an explicit studio light the same bits
        const Vec3 v = normalized(Vec3{.2f, .3f, 1.f}), up{0, 1, 0};
        identical = identical && same(car_paint::shade(tint, n, st.toSun, v, up, .7f, .3f),
                                      car_paint::shade(tint, n, st.toSun, v, up, .7f, .3f, st));
        car_paint::Material m;
        car_paint::material(m, tint, uint8_t{204}, uint8_t{64});
        const car_paint::Facing f = car_paint::facing(m, st.toSun, v, up);
        identical = identical && same(car_paint::corner(m, f, n, st.toSun, up), car_paint::corner(m, f, n, st.toSun, up, st));
    }
    check(identical, "studio: faces and car paint (shade, corner) unchanged");
    check(sunShare(st, .5f, 0) == 1.f && shadowAlpha(st, sunShare(st, .5f, 0)) == 1.f,
          "studio: the vertex alpha stays 1 (nothing for the sun shadow to take)");
    check(shadowStrength(st, shadowOn) == 0.f && !wantsDepthAlways(shadowOn, st), "studio: no sun shadow, no depth every frame");

    // 2. spans
    std::vector<unsigned char> a(spanA, 0), b(spanBBytes, 0);
    put(a, offKeyLightVec, {0, 1, 0});
    put(a, offKeyLightColor, {3, 2, 1});
    put(a, offFogColor, {.1f, .2f, .3f});
    put(a, offAdaptedLuminance, {.5f});
    put(a, offActualLuminance, {.25f});
    put(a, offPenumbraRadius, {.01f});
    put(a, offTimeOfDay, {.5f});
    put(b, offEnvProbeModulation - spanBFrom, {.9f, .8f, .7f});
    put(b, offLightGridModulation - spanBFrom, {1, 1, 1});
    put(b, offActiveViewPos - spanBFrom, {10, 20, 30});
    put(b, offToneMapAnchor - spanBFrom, {.18f});
    const Sample sp = fromSpans(a.data(), b.data(), 1);
    check(sp.toLight[1] == 1 && sp.color[0] == 3 && sp.fog[2] == .3f && sp.adapted == .5f && sp.actual == .25f &&
              sp.penumbra == .01f && sp.timeOfDay == .5f && sp.probe[0] == .9f && sp.viewPos[2] == 30 && sp.toneAnchor == .18f &&
              sp.slot == 1,
          "spans: every field at its RDEF offset");
    check(offActiveViewPos + 12 <= spanBFrom + spanBBytes && offActualLuminance + 4 <= spanA, "spans: cover the fields read");

    // 3. derive
    const Light w = derive(noon(), Tune{});
    check(w.world && w.oneSided && w.why == lit, "derive: a good frame is world light");
    check(near(w.toSun, normalized(Vec3{.4f, .8f, -.3f}), 1e-5f), "derive: toward the sun as the game says (KeyLightVec)");
    check(w.sun.x > w.sun.z && w.skyUp.z > w.skyUp.x, "derive: warm sun, blue sky (the hue kept)");
    check(std::max({w.sun.x, w.sun.y, w.sun.z}) <= 1.f + 1e-5f && luminance(w.skyDown) < luminance(w.skyUp), "derive: shoulder, ground darker");
    Sample dark = noon();
    dark.adapted = 8.f; // a brighter adapted eye: the same sun looks dimmer
    check(luminance(derive(dark, Tune{}).sun) < luminance(w.sun), "derive: the game's exposure darkens");
    Sample night = noon();
    night.color[0] = night.color[1] = night.color[2] = 0;
    night.fog[0] = night.fog[1] = night.fog[2] = 0;
    const Light nl = derive(night, Tune{});
    check(nl.world && luminance(nl.skyUp) >= .035f - 1e-6f, "derive: no key light, a night street is not black");
    Sample bad = noon();
    bad.adapted = std::nanf("");
    check(derive(bad, Tune{}).why == badNumbers && !derive(bad, Tune{}).world, "derive: NaN exposure refused");
    bad = noon();
    bad.toLight[0] = 3;
    check(derive(bad, Tune{}).why == badNumbers, "derive: a not-unit light vector refused");
    bad = noon();
    bad.color[1] = -1;
    check(derive(bad, Tune{}).why == badNumbers, "derive: a negative colour refused");
    check(luminance(derive(noon(), tuneFor(modeGame, 200)).sun) > luminance(w.sun), "gain: world_light_gain 200 brighter");
    check(tuneFor(modeGame, 9999).gain == 4.f && tuneFor(modeGame, 1).gain == .25f, "gain: clamped 25..400");
    check(tuneFor(modeGameSoft, 100).skyFloor > Tune{}.skyFloor, "GAME SOFT: more sky");

    // 4. one-sided, share, shadow
    const Vec3 back = w.toSun * -1.f;
    float share = -1;
    const Vec3 c = shade(w, {1, 1, 1}, back, true, &share);
    check(share == 0.f && near(c, sky(w, back.y), 1e-6f), "world: the shadow side gets the sky only");
    shade(w, {1, 1, 1}, w.toSun, true, &share);
    check(share > .5f && share < 1.f, "world: the lit side mostly sun");
    check(near(seenSide(Vec3{0, 0, 1}, Vec3{0, 0, -2}), Vec3{0, 0, -1}, 1e-6f), "seenSide: turned to the eye");
    {
        Light w = studio();
        w.world = true;
        check(shadowAlpha(w, .7f) > .29f && shadowAlpha(w, .7f) < .31f && shadowAlpha(w, 0) == 1.f &&
                  shadowAlpha(w, 2.f) == 0.f,
              "shadowAlpha: world lit keeps 1 - the sun's share (clamped)");
    }
    check(near(shoulder(Vec3{.01f, .02f, .03f}, 1.6f), Vec3{.01f, .02f, .03f}, 1e-3f), "shoulder: linear near 0");
    check(shadowStrength(w, shadowOn) > .8f && shadowStrength(w, shadowOff) == 0, "shadow: on at noon, off when OFF");
    Light under = w;
    under.toSun = Vec3{0, -1, 0};
    check(shadowStrength(under, shadowOn) == 0, "shadow: none from under the street");
    check(wantsDepthAlways(shadowOn, w) && !wantsDepthAlways(shadowVehicles, w), "depth: every frame only for ON");
    // car paint in world light: finite, and the sun's colour in its highlight
    const Vec3 v = normalized(w.toSun + Vec3{0, 0, .001f});
    const Vec3 p = car_paint::shade({.5f, .5f, .5f}, w.toSun, w.toSun, v, {0, 1, 0}, 1.f, .9f, w);
    std::printf("     car paint at the highlight: %.3f %.3f %.3f\n", p.x, p.y, p.z);
    check(std::isfinite(p.x + p.y + p.z) && p.x > p.z, "car paint: world light, a warm highlight");
    setCurrent(w);
    check(current().world, "current: the frame's light kept");
    check(describe(w, noon()).find("world light") == 0, "describe: says world light");
    std::printf("%s (%d failed)\n", failures ? "FAILED" : "all ok", failures);
    return failures ? 1 : 0;
}
