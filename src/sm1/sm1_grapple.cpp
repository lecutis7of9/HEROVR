// HERO VR 0.0.76 SM1: CAPTAIN AMERICA's and DAREDEVIL's grapple (see include/spidy/sm1_grapple.hpp). Made by Agent Mulder.
#include "spidy/sm1_grapple.hpp"

#include "spidy/grapple073.hpp"

#include <cmath>

namespace spidy::sm1 {

void GrappleDriver::stop() {
    pull_.stop();
    coast_.stop();
    carried_ = {};
}

GrappleStep GrappleDriver::step(const GrappleInput& in) {
    GrappleStep o;
    const bool was = driving();
    bool pressed[2]{};
    for (int i = 0; i < 2; ++i) {
        const bool g = grapple073::held(in.grip[i], grip_[i], grapple073::gripPress, grapple073::gripRelease);
        const bool t = grapple073::held(in.trigger[i], trigger_[i], grapple073::triggerPress, grapple073::triggerRelease);
        pressed[i] = t && !trigger_[i] && g;
        if (!g && grip_[i] && pull_.phase() == cape_glide::pull && pull_.hand() == static_cast<uint32_t>(i)) {
            // CANCEL: the line's grip let go - coast on with the speed.
            pull_.stop();
            coast_.start(carried_);
            o.cancelled = true;
        }
        grip_[i] = g;
        trigger_[i] = t;
    }
    if (!in.allowed || !finite(in.body)) {
        stop();
        jumpOut_.was = in.jump;
        o.ended = was;
        return o;
    }
    // JUMP OUT (A while pulled or vaulting).
    Vec3 launch{};
    if (jumpOut_.update(in.jump, true, true, pull_.phase(), carried_, launch)) {
        pull_.stop();
        coast_.start(launch);
        carried_ = launch;
        o.jumped = true;
    }
    // A shot from either hand (the newer press wins when both fire at once: the right hand).
    for (int i = 0; i < 2; ++i) {
        if (!pressed[i])
            continue;
        ++fires_;
        const bool hit = in.aimOk[i] && in.aimKind[i] == aimAnchor &&
                         cape_glide::hookable(in.body, in.aimPoint[i], false, true);
        if (!hit) {
            ++misses_;
            o.missed = true;
            continue;
        }
        const bool redirect = driving();
        pull_.tune = in.captain ? dd_grapple074::capTune() : dd_grapple074::quickTune();
        if (pull_.start(in.body, in.aimPoint[i], in.aimNormal[i], static_cast<uint32_t>(i))) {
            coast_.stop();
            if (redirect)
                pull_.carry(carried_);
            o.started = true;
            o.missed = false;
        }
    }
    if (pull_.phase() != cape_glide::idle) {
        const cape_glide::GrappleOut g = pull_.update(in.dt, in.body, in.velocity, true);
        if (g.drive) {
            o.drive = true;
            o.velocity = g.velocity;
        }
        if (g.ended && !g.arrived && pull_.phase() == cape_glide::idle && finite(carried_) && length(carried_) > 1.f)
            coast_.start(carried_); // the vault done, out of time or stuck: no sudden stop
    } else if (coast_.on()) {
        const dd_grapple074::CoastOut c = coast_.update(in.dt, in.grounded, in.velocity, true);
        if (c.drive) {
            o.drive = true;
            o.velocity = c.velocity;
        }
    }
    if (o.drive)
        carried_ = o.velocity;
    else if (!driving())
        carried_ = {};
    if (pull_.phase() == cape_glide::pull) {
        o.line = static_cast<int>(pull_.hand());
        o.anchor = pull_.anchor();
    }
    o.ended = was && !driving();
    return o;
}

} // namespace spidy::sm1
