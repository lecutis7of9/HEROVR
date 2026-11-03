// HERO VR 0.0.77 STORY MOVES (include/spidy/story077b.hpp): the ability button, the throw's aim, the dodge,
// pure and frame by frame. Made by Agent Mulder.
#include "spidy/story077b.hpp"

#include <cstdio>

using namespace spidy;
using namespace spidy::story077b;

namespace {
int failures = 0;
void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++failures;
    }
}
constexpr float dt = 1.f / 90;
using In = AbilityButton::In;

// Frames with `in`; what went out, as seconds each bit was down, and the first time it went down.
struct Sent {
    float lb{}, x{}, y{}, b{}, a{}, l3r3{}, up{}, firstLb = -1, firstFace = -1;
    int taps{}, owned{}, giveBack{};
};
void run(AbilityButton& m, In in, float seconds, Sent& s, float& t) {
    for (float u = 0; u < seconds; u += dt, t += dt) {
        in.seconds = dt;
        const auto o = m.update(in);
        if ((o.buttons & pad::lb) && s.firstLb < 0)
            s.firstLb = t;
        if ((o.buttons & (pad::x | pad::y | pad::b | pad::a)) && s.firstFace < 0)
            s.firstFace = t;
        s.lb += (o.buttons & pad::lb) ? dt : 0;
        s.x += (o.buttons & pad::x) ? dt : 0;
        s.y += (o.buttons & pad::y) ? dt : 0;
        s.b += (o.buttons & pad::b) ? dt : 0;
        s.a += (o.buttons & pad::a) ? dt : 0;
        s.l3r3 += ((o.buttons & (pad::l3 | pad::r3)) == (pad::l3 | pad::r3)) ? dt : 0;
        s.up += (o.buttons & pad::dpadUp) ? dt : 0;
        s.taps += o.menuTap;
        s.owned += o.owned;
        s.giveBack += o.giveBack;
    }
}

void abilityTests() {
    {
        // X held, then a punch: LB first, X 50 ms later, at least 0.2 s; LB held while X is.
        AbilityButton m;
        Sent s;
        float t = 0;
        In in{0, true, true};
        run(m, in, .1f, s, t);
        check(s.lb == 0 && s.x == 0, "X alone sent something");
        check(s.owned > 0, "X held was not the ability button");
        In p = in;
        p.punch = true;
        run(m, p, dt, s, t);
        run(m, in, .5f, s, t);
        check(s.firstLb > 0 && s.firstFace > s.firstLb + .04f, "the face press did not wait for LB's lead");
        check(s.x >= .19f && s.x < .3f, "the punch's X was not about 0.2 s");
        check(s.lb > .45f, "LB did not stay down while X was held");
        check(m.fired[slotX] == 1, "the punch did not count LB + X");
        check(s.taps == 0, "a chorded X was a menu tap");
        // Let go: no tap, and LB comes up.
        Sent after;
        run(m, In{0, true, false}, .2f, after, t);
        check(after.lb == 0 && after.taps == 0, "letting X go after a chord sent LB or a tap");
    }
    {
        // Each slot.
        struct Case {
            In in;
            uint32_t slot;
            const char* what;
        };
        In y{0, true, true}; y.y = true;
        In up{0, true, true}; up.stickUp = true;
        In b{0, true, true}; b.b = true;
        In a{0, true, true}; a.a = true;
        In click{0, true, true}; click.stickClick = true;
        In down{0, true, true}; down.stickDown = true;
        const Case cases[] = {{y, slotY, "Y"}, {up, slotY, "stick up"}, {b, slotB, "B"}, {a, slotA, "A"},
                              {click, slotUltimate, "stick click"}, {down, slotCamo, "stick down"}};
        for (const auto& c : cases) {
            AbilityButton m;
            Sent s;
            float t = 0;
            run(m, In{0, true, true}, .05f, s, t);
            run(m, c.in, .1f, s, t);
            run(m, In{0, true, true}, .4f, s, t);
            check(m.fired[c.slot] == 1, c.what);
            if (c.slot == slotY)
                check(s.y >= .19f && s.lb > 0, "LB + Y");
            if (c.slot == slotB)
                check(s.b >= .19f && s.lb > 0, "LB + B");
            if (c.slot == slotA)
                check(s.a >= .19f && s.lb > 0, "LB + A");
            if (c.slot == slotUltimate)
                check(s.l3r3 >= .19f && s.lb == 0, "the ultimate is L3 + R3, no LB");
            if (c.slot == slotCamo)
                check(s.up >= .19f && s.lb == 0, "camouflage is D-pad up, no LB");
        }
    }
    {
        // A face button held long holds its press as long.
        AbilityButton m;
        Sent s;
        float t = 0;
        In b{0, true, true};
        b.b = true;
        run(m, In{0, true, true}, .05f, s, t);
        run(m, b, 1.f, s, t);
        check(s.b > .9f, "a held B was not held");
    }
    {
        // X tapped: the old menu tap. X held long with nothing: given back.
        AbilityButton m;
        Sent s;
        float t = 0;
        run(m, In{0, true, true}, .2f, s, t);
        run(m, In{0, true, false}, .1f, s, t);
        check(s.taps == 1 && s.lb == 0, "a tap of X was not the menu tap");
        Sent l;
        run(m, In{0, true, true}, 1.2f, l, t);
        check(l.giveBack > 0 && m.givenBack == 1, "X held long was not given back");
        In y{0, true, true};
        y.y = true;
        run(m, y, .2f, l, t);
        check(l.lb == 0 && l.y == 0, "a chord after the X was given back");
        run(m, In{0, true, false}, .1f, l, t);
        check(l.taps == 0, "a long hold was a tap");
    }
    {
        // Off: nothing at all, even with X held.
        AbilityButton m;
        Sent s;
        float t = 0;
        In in{0, false, true};
        in.punch = true;
        run(m, in, .5f, s, t);
        check(s.lb == 0 && s.x == 0 && s.owned == 0 && s.taps == 0, "off sent something");
    }
    {
        // A second punch after the first press: a second LB + X (LB already down).
        AbilityButton m;
        Sent s;
        float t = 0;
        In in{0, true, true}, p{0, true, true};
        p.punch = true;
        run(m, in, .05f, s, t);
        run(m, p, dt, s, t);
        run(m, in, .4f, s, t);
        run(m, p, dt, s, t);
        run(m, in, .4f, s, t);
        check(m.fired[slotX] == 2 && s.x > .38f, "a second punch was not a second LB + X");
    }
}

void aimTests() {
    const Vec3 ahead{0, 0, -1};
    {
        ThrowAim a;
        // Looking 90 degrees right of the camera while throwing: stick right.
        auto o = a.update(dt, aimLook, true, Vec3{1, 0, 0}, {}, ahead, 0, 0);
        check(o.active && o.right > .5f && std::abs(o.forward) < .05f, "looking right did not aim right");
        check(std::abs(o.degrees - 90) < 1, "the aim's angle");
        // Straight ahead: stick forward.
        o = a.update(dt, aimLook, true, Vec3{0, -.3f, -1}, {}, ahead, 0, 0);
        check(o.active && o.forward > .5f && std::abs(o.right) < .05f, "looking ahead did not aim ahead");
        // Released: on for 0.4 s, then off.
        float on = 0;
        for (float t = 0; t < 1; t += dt)
            on += a.update(dt, aimLook, false, Vec3{0, 0, -1}, {}, ahead, 0, 0).active ? dt : 0;
        check(on > .35f && on < .45f, "the aim did not hold 0.4 s after the throw");
        // The player's own stick wins.
        o = a.update(dt, aimLook, true, Vec3{1, 0, 0}, {}, ahead, .8f, 0);
        check(!o.active && a.overridden > 0, "the player's stick did not win");
        check(a.throws == 2, "throws counted");
    }
    {
        // Point: the right hand's ray, latched at the throw's start.
        ThrowAim a;
        auto o = a.update(dt, aimPoint, true, Vec3{0, 0, -1}, Vec3{-1, -.2f, 0}, ahead, 0, 0);
        check(o.active && o.right < -.5f, "pointing left did not aim left");
        o = a.update(dt, aimPoint, true, Vec3{0, 0, -1}, Vec3{1, 0, 0}, ahead, 0, 0);
        check(o.right < -.5f, "the point was not latched");
        // Pointed at the floor: the head's way.
        ThrowAim f;
        o = f.update(dt, aimPoint, true, Vec3{1, 0, 0}, Vec3{0, -1, -.1f}, ahead, 0, 0);
        check(o.right > .5f, "a hand pointed down did not fall back to the look");
    }
    {
        ThrowAim a;
        check(!a.update(dt, aimOff, true, Vec3{1, 0, 0}, {}, ahead, 0, 0).active, "OFF aimed");
        check(!a.update(dt, aimLook, false, Vec3{1, 0, 0}, {}, ahead, 0, 0).active, "aimed with no throw");
    }
}

void dodgeTests() {
    {
        // A one-frame B is a 0.12 s B.
        DodgeAssist d;
        d.update(dt, true, false, 0);
        float held = 0;
        held += d.update(dt, true, true, 0).dodge ? dt : 0;
        for (float t = 0; t < .5f; t += dt)
            held += d.update(dt, true, false, 0).dodge ? dt : 0;
        check(held >= .12f && held < .16f, "a short B was not at least 0.12 s");
        check(d.presses == 1, "B counted");
        // Held: held.
        held = 0;
        for (float t = 0; t < .6f; t += dt)
            held += d.update(dt, true, true, 0).dodge ? dt : 0;
        check(held > .55f, "a held B was not held");
        d.update(dt, true, false, 0);
        // A B down as play starts is no press.
        DodgeAssist g;
        auto o = g.update(dt, true, true, 0);
        check(o.dodge && !o.started, "a B already down counted as a fresh press");
    }
    {
        // A head sidestep: B with the stick that way.
        DodgeAssist d;
        d.update(dt, true, false, 0);
        auto o = d.update(dt, true, false, 1);
        check(o.dodge && o.right > .8f && o.started, "a step right was not a dodge right");
        o = d.update(dt, true, false, -1);
        check(o.right > .8f, "a second step mid-dodge changed it");
        check(d.headDodges == 1, "head dodges counted");
        check(!d.update(dt, false, true, 1).dodge, "off dodged");
    }
    {
        HeadSidestep h;
        const Vec3 right{1, 0, 0};
        // Still: nothing. A slow drift: nothing.
        int got = 0;
        Vec3 head{0, 1.6f, 0};
        for (float t = 0; t < 1; t += dt)
            got |= h.update(dt, true, head, right);
        for (float t = 0; t < 2; t += dt) {
            head.x += .1f * dt;
            got |= h.update(dt, true, head, right);
        }
        check(got == 0, "a still or slowly drifting head dodged");
        // A quick step left: 0.25 m in 0.15 s.
        int side = 0;
        for (float t = 0; t < .15f; t += dt) {
            head.x -= .25f / .15f * dt;
            if (const int s = h.update(dt, true, head, right))
                side = s;
        }
        check(side == -1 && h.steps == 1, "a quick step left was not a dodge left");
        // Ducking is not a sidestep.
        HeadSidestep k;
        Vec3 duck{0, 1.6f, 0};
        int d = 0;
        for (float t = 0; t < .2f; t += dt) {
            duck.y -= 1.5f * dt;
            duck.x += .9f * dt;
            d |= k.update(dt, true, duck, right);
        }
        check(d == 0, "a duck was a sidestep");
        check(k.update(dt, false, duck, right) == 0, "off");
    }
}
} // namespace

int main() {
    abilityTests();
    aimTests();
    dodgeTests();
    static_assert(sizeof(Data) == 128);
    if (failures) {
        std::printf("%d story077b checks failed\n", failures);
        return 1;
    }
    std::printf("story077b: all checks passed\n");
    return 0;
}
