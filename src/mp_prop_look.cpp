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
    // 4. 0.0.74 MP PROPS phase 2: what he has thrown (batarangs, the spinning shield) and his smoke pellets
    if (d)
        for (uint8_t i = 0; i < 2; ++i)
            if (d->flying[i].look && d->flying[i].look <= propLookLast) {
                PropItem it = fromItem(static_cast<uint8_t>(idFlying + i), d->flying[i], f);
                if (it.kind == hero007::lookBatarang)
                    it.variant = static_cast<uint8_t>(std::min<uint32_t>(src.batarangStyle, 255));
                it.flags |= propFlagFlying;
                p.add(it);
            }
    if (d)
        for (uint8_t i = 0; i < 2; ++i)
            if (d->pellet[i] && fin(d->pelletAt[i])) {
                PropItem it;
                it.id = static_cast<uint8_t>(idPellet + i);
                it.kind = propPellet;
                it.flags = i ? propFlagSide : 0;
                it.at = f.at(d->pelletAt[i]);
                p.add(it);
            }
    // 0.0.74 MP PROPS phase 3: GREEN LANTERN's constructs and IRON MAN's palm glows (state, like the items)
    if (l) {
        for (uint8_t i = 0; i < 2; ++i)
            if (l->ring[i] && fin(l->ringAt[i])) {
                PropItem it;
                it.id = static_cast<uint8_t>(idRing + i);
                it.kind = propRing;
                it.flags = i ? propFlagSide : 0;
                it.at = f.at(l->ringAt[i]);
                p.add(it);
            }
        if (l->bubble && fin(l->bubbleAt)) {
            PropItem it;
            it.id = idBubble;
            it.kind = propBubble;
            it.spin = std::clamp(l->bubbleRadius, 0.f, 7.9f);
            it.at = f.at(l->bubbleAt);
            p.add(it);
        }
        if (l->fist && fin(l->fistAt)) {
            PropItem it;
            it.id = idFist;
            it.kind = propFist;
            it.variant = static_cast<uint8_t>(std::lround(std::clamp(l->fistAlpha, 0.f, 1.f) * 255));
            it.at = f.at(l->fistAt);
            it.forward = unitOr(f.way(l->fistForward), {0, 0, -1});
            p.add(it);
        }
    }
    const ShotSource* sh = src.shots;
    if (sh && sh->repulsor)
        for (uint8_t i = 0; i < 2; ++i)
            if (sh->palmGlow[i] > .01f && fin(sh->palmAt[i])) {
                PropItem it;
                it.id = static_cast<uint8_t>(idPalm + i);
                it.kind = propPalm;
                it.flags = i ? propFlagSide : 0;
                it.variant = static_cast<uint8_t>(std::lround(std::clamp(sh->palmGlow[i], 0.f, 1.f) * 255));
                it.at = f.at(sh->palmAt[i]);
                it.forward = unitOr(f.way(sh->palmWay[i]), {0, 0, -1});
                p.add(it);
            }
    // the lines (beams, ropes, cables, shots in flight), most telling first, up to propLinesMost
    const auto line = [&](uint8_t kind, Vec3 a, Vec3 b, float fade) {
        if (!fin(a) || !fin(b) || !std::isfinite(fade) || fade <= 0)
            return;
        PropLine x;
        x.kind = kind;
        x.fade = std::clamp(fade, 0.f, 1.f);
        x.from = f.at(a);
        x.to = f.at(b);
        p.add(x);
    };
    if (d && d->beamFade > 0)
        line(d->beamFreeze ? lineFreeze : lineHeat, (d->beamFrom[0] + d->beamFrom[1]) * .5f, d->beamTo, d->beamFade);
    if (sh && sh->repulsor && sh->chestFade > 0)
        line(lineChest, sh->chestFrom, sh->chestTo, sh->chestFade);
    if (l && l->beam)
        line(lineLantern, l->beamFrom, l->beamTo, 1);
    if (l && l->chain)
        line(lineChain, l->chainFrom, l->chainTo, 1);
    if (d)
        for (int i = 0; i < 2; ++i)
            if (d->grapple[i])
                line(lineGrapple, d->grappleFrom[i], d->grappleTo[i], std::max(d->grappleOut[i], .01f));
    if (d)
        for (int i = 0; i < 2; ++i)
            if (d->cable[i])
                line(lineCable, d->cableFrom[i], d->cableTo[i], 1);
    if (l && l->hook)
        line(lineHook, l->hookFrom, l->hookTo, 1);
    if (l && l->rope)
        line(lineRope, l->ropeFrom, l->ropeTo, 1);
    if (sh)
        for (size_t b = 0; b < ShotSource::most; ++b)
            if (sh->fade[b] > 0) {
                if (sh->repulsor)
                    line(lineRepulsor, sh->from[b], sh->at[b], sh->fade[b]);
                else
                    line(lineWeb, sh->tail[b], sh->at[b], sh->fade[b]);
            }
    if (l)
        for (uint32_t b = 0; b < l->bolts && b < hero072::boltsMost; ++b)
            line(lineBolt, l->boltFrom[b], l->boltTo[b], 1);
    // 5. what he wears (holsters, the shield on his back)
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
        } else if (it.kind == propPellet) {
            const uint32_t s = side(it.flags);
            r.h007.pellet[s] = true;
            r.h007.pelletAt[s] = f.at(it.at);
            r.h007.any = true;
        } else if (it.kind == propRing) {
            const uint32_t s = side(it.flags);
            r.h072.ring[s] = true;
            r.h072.ringAt[s] = f.at(it.at);
            r.h072.any = true;
        } else if (it.kind == propBubble) {
            r.h072.bubble = true;
            r.h072.bubbleAt = f.at(it.at);
            r.h072.bubbleRadius = std::clamp(it.spin, 0.f, 8.f);
            r.h072.any = true;
        } else if (it.kind == propFist) {
            r.h072.fist = true;
            r.h072.fistAt = f.at(it.at);
            r.h072.fistForward = f.way(it.forward);
            r.h072.fistAlpha = it.variant / 255.f;
            r.h072.any = true;
        } else if (it.kind == propPalm) {
            const uint32_t s = side(it.flags);
            r.shots.repulsor = true;
            r.shots.palmGlow[s] = it.variant / 255.f;
            r.shots.palmAt[s] = f.at(it.at);
            r.shots.palmWay[s] = f.way(it.forward);
        } else if (it.kind == propBaton) {
            const uint32_t s = side(it.flags);
            const float span = std::clamp(it.spin, .05f, 2.f);
            r.batons.on[s] = true;
            r.batons.held[s] = (it.flags & propFlagInHand) != 0;
            r.batons.grip[s] = f.at(it.at);
            r.batons.tip[s] = r.batons.grip[s] + f.way(it.forward) * span;
        }
    }
    // 0.0.74 MP PROPS phase 3: the lines
    uint32_t cables = 0;
    for (uint8_t k = 0; k < p.lines && k < propLinesMost; ++k) {
        const PropLine& x = p.line[k];
        const Vec3 a = f.at(x.from), b = f.at(x.to);
        const float fade = std::clamp(x.fade, 0.f, 1.f);
        switch (x.kind) {
        case lineHeat:
        case lineFreeze: {
            // the two eyes either side of the line's start (3.2 cm), level
            const Vec3 dir = unitOr(b - a, {0, 0, -1});
            const Vec3 right = unitOr(cross(dir, Vec3{0, 1, 0}), {1, 0, 0}) * .032f;
            r.h007.beamFreeze = x.kind == lineFreeze;
            r.h007.beamFrom[0] = a - right;
            r.h007.beamFrom[1] = a + right;
            r.h007.beamTo = b;
            r.h007.beamFade = std::max(fade, .01f);
            r.h007.beamReach = length(b - a);
            r.h007.any = true;
            break;
        }
        case lineChest:
            r.shots.repulsor = true;
            r.shots.chestFrom = a;
            r.shots.chestTo = b;
            r.shots.chestFade = std::max(fade, .01f);
            break;
        case lineLantern:
            r.h072.any = r.h072.beam = true;
            r.h072.beamFrom = a;
            r.h072.beamTo = b;
            break;
        case lineChain:
            r.h072.any = r.h072.chain = true;
            r.h072.chainFrom = a;
            r.h072.chainTo = b;
            break;
        case lineGrapple: {
            // the hand holding the grapple gun (the gun's muzzle then), else the first free one
            uint32_t h = r.h007.held[1].look == hero007::lookGrappleGun && !r.h007.grapple[1] ? 1u
                         : r.h007.held[0].look == hero007::lookGrappleGun && !r.h007.grapple[0] ? 0u
                         : r.h007.grapple[0]                                                       ? 1u
                                                                                                   : 0u;
            r.h007.grapple[h] = true;
            r.h007.grappleFrom[h] = a;
            r.h007.grappleTo[h] = b;
            r.h007.grappleOut[h] = std::max(fade, .01f);
            r.h007.any = true;
            break;
        }
        case lineCable:
            if (cables < 2) {
                r.h007.cable[cables] = true;
                r.h007.cableFrom[cables] = a;
                r.h007.cableTo[cables] = b;
                ++cables;
                r.h007.any = true;
            }
            break;
        case lineHook:
            r.h072.any = r.h072.hook = true;
            r.h072.hookFrom = a;
            r.h072.hookTo = b;
            break;
        case lineRope:
            r.h072.any = r.h072.rope = true;
            r.h072.ropeFrom = a;
            r.h072.ropeTo = b;
            break;
        case lineRepulsor:
        case lineWeb:
            if (r.shots.n < propLinesMost) {
                const uint32_t n = r.shots.n++;
                r.shots.web[n] = x.kind == lineWeb;
                r.shots.fade[n] = std::max(fade, .01f);
                if (x.kind == lineWeb) {
                    r.shots.at[n] = b;
                    r.shots.tail[n] = a;
                    r.shots.from[n] = a;
                } else {
                    r.shots.repulsor = true;
                    r.shots.from[n] = a;
                    r.shots.at[n] = b;
                    r.shots.tail[n] = b - unitOr(b - a, {0, 0, -1}) * std::min(length(b - a), repulsorTail);
                }
            }
            break;
        case lineBolt:
            if (r.h072.bolts < hero072::boltsMost) {
                const uint32_t n = r.h072.bolts++;
                r.h072.boltFrom[n] = a;
                r.h072.boltTo[n] = b;
                r.h072.boltSeed[n] = .11f + .37f * static_cast<float>(n);
                r.h072.any = true;
            }
            break;
        default:
            break;
        }
    }
    return r;
}

// ---- 0.0.74 MP PROPS phase 3: the effects ring ----
void FxRing::add(uint8_t kind, uint8_t param, Vec3 at, Vec3 way, double born) {
    if (!fin(at))
        return;
    Fx& x = ring_[next_];
    x.seq = ++seq_;
    if (x.seq == 0)
        x.seq = ++seq_; // 0 is never a sequence number
    x.kind = kind;
    x.param = param;
    x.born = born;
    x.at = at;
    x.way = fin(way) ? way : Vec3{0, 0, -1};
    next_ = (next_ + 1) % most;
    count_ = std::min(count_ + 1, most);
}

void FxRing::feed(const FxSource& s, double now) {
    const hero007::Draw* d = s.h007;
    const hero072::Look* l = s.h072;
    const ShotSource* sh = s.shots;
    // an effect is new when its age goes back (a slot reused) and is still young; the first call only learns the ages
    const auto fresh = [&](float& prev, float age) {
        const bool yes = primed_ && std::isfinite(age) && age >= 0 && age < fxSendFor && age < prev - 1e-4f;
        prev = std::isfinite(age) ? age : 1e9f;
        return yes;
    };
    if (d) {
        for (uint8_t i = 0; i < 2; ++i)
            if (fresh(flash_[i], d->flashAge[i]))
                add(d->flashShotgun[i] ? fxFlashShotgun : fxFlash, i, d->flashAt[i], d->flashWay[i], now - d->flashAge[i]);
        for (uint32_t i = 0; i < hero007::drawImpacts; ++i)
            if (fresh(impact_[i], d->impactAge[i]))
                add(fxImpact, 0, d->impactAt[i], d->impactWay[i], now - d->impactAge[i]);
        for (uint32_t i = 0; i < hero007::drawSmokes; ++i)
            if (fresh(smoke_[i], d->smokeAge[i]))
                add(fxSmoke, 0, d->smokeAt[i], {0, 1, 0}, now - d->smokeAge[i]);
        for (uint32_t i = 0; i < hero007::drawScorches; ++i)
            if (fresh(scorch_[i], d->scorchAge[i]))
                add(fxScorch, 0, d->scorchAt[i], {0, 1, 0}, now - d->scorchAge[i]);
    }
    if (sh)
        for (size_t b = 0; b < ShotSource::most; ++b) {
            const float fade = std::isfinite(sh->splashFade[b]) ? sh->splashFade[b] : 0.f;
            // a splash starts at full and fades: new when it jumps up
            if (primed_ && fade > .5f && fade > splash_[b] + .05f)
                add(fxSplash, sh->repulsor ? 0 : 1, sh->splashAt[b], {0, 1, 0}, now);
            splash_[b] = fade;
        }
    if (l) {
        if (l->shock && std::isfinite(l->shockAge) && primed_ && (!shock_ || l->shockAge < shockAge_ - 1e-4f) && l->shockAge < .5f) {
            const float r = std::clamp(l->shockRadius * 10.f, 0.f, 127.f);
            add(fxShock, static_cast<uint8_t>(static_cast<uint8_t>(std::lround(r)) | (l->shockSubtle ? 128 : 0)), l->shockAt,
                {0, 1, 0}, now - static_cast<double>(std::max(l->shockAge, 0.f)) * shockLife);
        }
        shock_ = l->shock;
        shockAge_ = l->shockAge;
    } else {
        shock_ = false;
    }
    primed_ = true;
}

void FxRing::write(Props& p, const SendFrame& f, double now) const {
    for (size_t k = 0; k < count_; ++k) {
        const Fx& x = ring_[(next_ + most - 1 - k) % most];
        const double age = std::max(0.0, now - x.born);
        if (age > fxSendFor)
            break; // older ones are older still
        PropFx o;
        o.seq = x.seq;
        o.kind = x.kind;
        o.param = x.param;
        o.age = static_cast<float>(age);
        o.at = f.at(x.at);
        o.way = unitOr(f.way(x.way), {0, 0, -1});
        if (!p.add(o))
            break;
    }
}

void FxInbox::take(const Props& p, const GhostFrame& f, double now) {
    for (uint8_t k = 0; k < p.fx && k < propFxMost; ++k) {
        const PropFx& x = p.effect[k];
        if (x.kind < fxFlash || x.kind >= fxKindEnd || x.seq == 0)
            continue;
        bool seen = false;
        for (const E& e : e_)
            if (e.on && e.seq == x.seq && e.kind == x.kind && now - e.born < fxKeepFor)
                seen = true;
        if (seen)
            continue;
        E& e = e_[next_];
        next_ = (next_ + 1) % most;
        ++taken_;
        e.on = true;
        e.seq = x.seq;
        e.kind = x.kind;
        e.param = x.param;
        // a muzzle flash names its hand: mirrored, the other one
        if (f.mirror && (x.kind == fxFlash || x.kind == fxFlashShotgun))
            e.param ^= 1;
        e.born = now - std::clamp(static_cast<double>(x.age), 0.0, 2.0);
        e.at = f.at(x.at);
        e.way = f.way(x.way);
    }
}

bool FxInbox::live(double now) const {
    for (const E& e : e_)
        if (e.on && now - e.born < fxKeepFor)
            return true;
    return false;
}

void FxInbox::apply(RemoteProps& r, double now) const {
    // newest first
    size_t order[most]{}, n = 0;
    for (size_t i = 0; i < most; ++i)
        if (e_[i].on && now - e_[i].born < fxKeepFor)
            order[n++] = i;
    std::sort(order, order + n, [&](size_t a, size_t b) { return e_[a].born > e_[b].born; });
    bool flash[2]{};
    uint32_t impacts = 0, smokes = 0, scorches = 0;
    bool shock = false;
    for (size_t k = 0; k < n; ++k) {
        const E& e = e_[order[k]];
        const float age = static_cast<float>(std::max(0.0, now - e.born));
        switch (e.kind) {
        case fxFlash:
        case fxFlashShotgun: {
            const uint32_t h = e.param & 1u;
            if (!flash[h] && age < hero007::flashLife) {
                flash[h] = true;
                r.h007.flashAt[h] = e.at;
                r.h007.flashWay[h] = e.way;
                r.h007.flashAge[h] = age;
                r.h007.flashShotgun[h] = e.kind == fxFlashShotgun;
                r.h007.any = true;
            }
            break;
        }
        case fxImpact:
            if (impacts < hero007::drawImpacts && age < hero007::impactLife) {
                r.h007.impactAt[impacts] = e.at;
                r.h007.impactWay[impacts] = e.way;
                r.h007.impactAge[impacts++] = age;
                r.h007.any = true;
            }
            break;
        case fxSmoke:
            if (smokes < hero007::drawSmokes) {
                r.h007.smokeAt[smokes] = e.at;
                r.h007.smokeAge[smokes++] = age;
                r.h007.any = true;
            }
            break;
        case fxScorch:
            if (scorches < hero007::drawScorches && age < hero007::scorchLife) {
                r.h007.scorchAt[scorches] = e.at;
                r.h007.scorchAge[scorches++] = age;
                r.h007.any = true;
            }
            break;
        case fxShock:
            if (!shock && age < shockLife) {
                shock = true;
                r.h072.any = r.h072.shock = true;
                r.h072.shockAt = e.at;
                r.h072.shockRadius = (e.param & 127u) / 10.f;
                r.h072.shockSubtle = (e.param & 128u) != 0;
                r.h072.shockAge = age / shockLife;
            }
            break;
        case fxSplash:
            if (r.shots.splashes < 8 && age < 1.f) {
                const uint32_t s = r.shots.splashes++;
                r.shots.splashAt[s] = e.at;
                r.shots.splashAge[s] = age;
                r.shots.splashWeb[s] = (e.param & 1u) != 0;
            }
            break;
        default:
            break;
        }
    }
}
} // namespace spidy::mp
