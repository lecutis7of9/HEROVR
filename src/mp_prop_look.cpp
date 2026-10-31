// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
// Made by Agent Mulder.
//
// 0.0.74 MP PROPS: the player's props to the PROPS tail and back onto PLAYER 2 (mp_prop_look.hpp).
#include "spidy/mp_prop_look.hpp"

#include <algorithm>
#include <cmath>

namespace spidy::mp {
namespace {
bool fin(Vec3 v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }
Vec3 unitOr(Vec3 d, Vec3 fallback) {
    const float n = fin(d) ? length(d) : 0.f;
    return n > 1e-4f ? d * (1.f / n) : fallback;
}
Vec3 flipX(Vec3 v) { return {-v.x, v.y, v.z}; }
uint8_t itemFlags(const hero007::Item& it) {
    return static_cast<uint8_t>((std::min<uint32_t>(it.holster, 7)) | (it.side ? propFlagSide : 0) | (it.palm ? propFlagPalm : 0));
}
PropItem fromItem(uint8_t id, const hero007::Item& it, const SendFrame& f) {
    PropItem p;
    p.id = id;
    p.kind = static_cast<uint8_t>(it.look);
    p.flags = itemFlags(it);
    p.spin = it.spin;
    p.at = f.at(it.at);
    p.forward = unitOr(f.way(it.forward), {0, 0, -1});
    p.up = unitOr(f.way(it.up), {0, 1, 0});
    return p;
}
} // namespace

SendFrame sendFrame(Vec3 anchor, Quat trackingTurn, float rootYaw) {
    SendFrame f;
    f.anchor = anchor;
    f.toLocal = Quat::yaw(-rootYaw) * trackingTurn.conjugate();
    return f;
}

Vec3 GhostFrame::at(Vec3 p) const { return origin + turn.rotate(feet + body.rotate(mirror ? flipX(p) : p)); }
Vec3 GhostFrame::way(Vec3 d) const { return unitOr(turn.rotate(body.rotate(mirror ? flipX(d) : d)), {0, 0, -1}); }

GhostFrame ghostFrame(const Ghost& g, bool mirror, Vec3 origin, Quat turn) {
    GhostFrame f;
    f.origin = origin;
    f.turn = turn;
    f.feet = g.feet;
    f.body = Quat::yaw(g.bodyYaw);
    f.mirror = mirror;
    return f;
}

Props captureProps(const PropSource& src, const SendFrame& f) {
    Props p;
    const hero007::Draw* d = src.h007 && src.h007->any ? src.h007 : nullptr;
    const hero072::Look* l = src.h072 && src.h072->any ? src.h072 : nullptr;
    if (d) {
        if (d->clawSteel)
            p.flags |= propsFlagClawSteel;
        if (d->pistolModel || d->sawedFrame)
            p.flags |= propsFlagGunModels;
        if (d->shieldModel)
            p.flags |= propsFlagShieldModel;
        if (d->batonModel[0] || d->batonModel[1])
            p.flags |= propsFlagBatonModel;
    }
    if (src.batons.club)
        p.flags |= propsFlagBatonClub;
    // 1. what is in his hands
    if (d)
        for (uint8_t i = 0; i < 2; ++i)
            if (d->held[i].look && d->held[i].look <= propLookLast) {
                PropItem it = fromItem(static_cast<uint8_t>(idHeld + i), d->held[i], f);
                if (it.kind == hero007::lookBatarang)
                    it.variant = static_cast<uint8_t>(std::min<uint32_t>(src.batarangStyle, 255));
                it.flags |= propFlagInHand;
                p.add(it);
            }
    // 2. Thor's hammer (in the fist, flying or on his back)
    if (l && l->hammer && fin(l->hammerAt)) {
        PropItem it;
        it.id = idHammer;
        it.kind = propHammer;
        it.flags = static_cast<uint8_t>((l->hammerInHand ? propFlagInHand : 0) | (l->hammerSheathed ? propFlagSheathed : 0) |
                                        (!l->hammerInHand && !l->hammerSheathed ? propFlagFlying : 0));
        it.variant = static_cast<uint8_t>(std::min<uint32_t>(l->hammerChoice, 255));
        it.spin = l->hammerSpin;
        it.at = f.at(l->hammerAt);
        it.forward = unitOr(f.way(l->hammerForward), {0, 1, 0});
        it.up = unitOr(f.way(l->hammerUp), {0, 0, -1});
        p.add(it);
    }
    // 3. Daredevil's batons (held, holstered or thrown)
    for (uint8_t i = 0; i < 2; ++i)
        if (src.batons.on[i] && fin(src.batons.grip[i]) && fin(src.batons.tip[i])) {
            PropItem it;
            it.id = static_cast<uint8_t>(idBaton + i);
            it.kind = propBaton;
            it.flags = static_cast<uint8_t>((i ? propFlagSide : 0) | (src.batons.held[i] ? propFlagInHand : 0));
            const Vec3 span = src.batons.tip[i] - src.batons.grip[i];
            it.spin = std::clamp(length(span), 0.f, 2.f);
            it.at = f.at(src.batons.grip[i]);
            it.forward = unitOr(f.way(span), {0, 0, -1});
            p.add(it);
        }
    // 4. what he wears (holsters, the shield on his back)
    if (d)
        for (uint8_t i = 0; i < 4; ++i)
            if (d->worn[i].look && d->worn[i].look <= propLookLast) {
                PropItem it = fromItem(static_cast<uint8_t>(idWorn + i), d->worn[i], f);
                if (it.kind == hero007::lookBatarang)
                    it.variant = static_cast<uint8_t>(std::min<uint32_t>(src.batarangStyle, 255));
                p.add(it);
            }
    return p;
}

RemoteProps rebuildProps(const Props& p, const GhostFrame& f) {
    RemoteProps r;
    r.h007.clawSteel = (p.flags & propsFlagClawSteel) != 0;
    r.batons.club = (p.flags & propsFlagBatonClub) != 0;
    r.batonModel = (p.flags & propsFlagBatonModel) != 0;
    r.gunModels = (p.flags & propsFlagGunModels) != 0;
    r.shieldModel = (p.flags & propsFlagShieldModel) != 0;
    const auto side = [&](uint8_t flags) -> uint32_t {
        const uint32_t s = (flags & propFlagSide) ? 1u : 0u;
        return f.mirror ? 1u - s : s;
    };
    uint32_t worn = 0;
    for (uint8_t k = 0; k < p.items && k < propItemsMost; ++k) {
        const PropItem& it = p.item[k];
        if (it.kind >= 1 && it.kind <= propLookLast) {
            hero007::Item h;
            h.look = it.kind;
            h.at = f.at(it.at);
            h.forward = f.way(it.forward);
            h.up = f.way(it.up);
            h.spin = f.mirror && it.kind != hero007::lookClaws ? -it.spin : it.spin;
            h.holster = it.flags & propFlagHolster;
            h.side = side(it.flags);
            h.palm = (it.flags & propFlagPalm) != 0;
            if (it.kind == hero007::lookBatarang && it.variant)
                r.batarangStyle = it.variant;
            if (it.kind == hero007::lookClaws)
                r.claws = true;
            if (it.id == idHeld || it.id == idHeld + 1) {
                // the hand it is in: mirrored, the other one
                const uint32_t hand = f.mirror ? 1u - (it.id - idHeld) : (it.id - idHeld);
                r.h007.held[hand] = h;
            } else if (it.id == idFlying || it.id == idFlying + 1) {
                r.h007.flying[it.id - idFlying] = h;
            } else if (worn < 4) {
                r.h007.worn[worn++] = h;
            }
            r.h007.any = true;
        } else if (it.kind == propHammer) {
            r.hammerChoice = it.variant;
            // on his back too: PLAYER 2's back is in view (appendLook skips a sheathed hammer - first person only)
            r.hammerSheathed = (it.flags & propFlagSheathed) != 0;
            r.h072.any = r.h072.hammer = true;
            r.h072.hammerInHand = (it.flags & propFlagInHand) != 0;
            r.h072.hammerAt = f.at(it.at);
            r.h072.hammerForward = f.way(it.forward);
            r.h072.hammerUp = f.way(it.up);
            r.h072.hammerSpin = f.mirror ? -it.spin : it.spin;
            r.h072.hammerChoice = it.variant;
        } else if (it.kind == propBaton) {
            const uint32_t s = side(it.flags);
            const float span = std::clamp(it.spin, .05f, 2.f);
            r.batons.on[s] = true;
            r.batons.held[s] = (it.flags & propFlagInHand) != 0;
            r.batons.grip[s] = f.at(it.at);
            r.batons.tip[s] = r.batons.grip[s] + f.way(it.forward) * span;
        }
    }
    return r;
}
} // namespace spidy::mp
