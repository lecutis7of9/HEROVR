// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
// Made by Agent Mulder.
//
// 0.0.74 MP PROPS: the 'P' tail (mp_props.hpp).
#include "spidy/mp_props.hpp"

#include <algorithm>
#include <cmath>

namespace spidy::mp {
namespace {
bool finiteV(Vec3 v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }
bool reach(Vec3 v) {
    return finiteV(v) && std::abs(v.x) < propReach && std::abs(v.y) < propReach && std::abs(v.z) < propReach;
}
int16_t cm(float m) { return static_cast<int16_t>(std::clamp(std::lround(m * 100.f), -32767l, 32767l)); }
int16_t unit(float u) { return static_cast<int16_t>(std::clamp(std::lround((std::isfinite(u) ? u : 0.f) * 32767.f), -32767l, 32767l)); }
Vec3 safeDir(Vec3 d, Vec3 fallback) {
    const float n = finiteV(d) ? length(d) : 0.f;
    return n > 1e-4f ? d * (1.f / n) : fallback;
}
struct Out {
    uint8_t* p;
    size_t n{}, cap;
    bool ok = true;
    void u8(uint8_t v) {
        if (n >= cap) {
            ok = false;
            return;
        }
        p[n++] = v;
    }
    void i16(int16_t v) {
        const uint16_t u = static_cast<uint16_t>(v);
        u8(static_cast<uint8_t>(u)), u8(static_cast<uint8_t>(u >> 8));
    }
    void at(Vec3 v) { i16(cm(v.x)), i16(cm(v.y)), i16(cm(v.z)); }
    void way(Vec3 v, Vec3 fallback) {
        v = safeDir(v, fallback);
        i16(unit(v.x)), i16(unit(v.y)), i16(unit(v.z));
    }
};
struct In {
    const uint8_t* p;
    size_t n{}, size;
    uint8_t u8() { return n < size ? p[n++] : 0; }
    int16_t i16() {
        const uint16_t a = u8(), b = u8();
        return static_cast<int16_t>(a | (b << 8));
    }
    Vec3 at() {
        const float x = i16() / 100.f, y = i16() / 100.f, z = i16() / 100.f;
        return {x, y, z};
    }
    Vec3 way(Vec3 fallback) {
        const float x = i16() / 32767.f, y = i16() / 32767.f, z = i16() / 32767.f;
        return safeDir({x, y, z}, fallback);
    }
};
float wrapSpin(float s) {
    if (!std::isfinite(s))
        return 0;
    if (std::abs(s) <= 7.99f)
        return s;
    s = std::remainder(s, 6.2831853f);
    return s;
}
} // namespace

bool Props::add(const PropItem& it) {
    if (items >= propItemsMost || !it.kind || it.kind >= propKindEnd || (it.kind > propLookLast && it.kind < propHammer) ||
        !reach(it.at) || !finiteV(it.forward) || !finiteV(it.up))
        return false;
    item[items++] = it;
    return true;
}
bool Props::add(const PropLine& l) {
    if (lines >= propLinesMost || !l.kind || l.kind >= lineKindEnd || !reach(l.from) || !reach(l.to) || !(l.fade > 0))
        return false;
    line[lines++] = l;
    return true;
}
bool Props::add(const PropFx& f) {
    if (fx >= propFxMost || !f.kind || f.kind >= fxKindEnd || !reach(f.at) || !finiteV(f.way) || !(f.age >= 0) || f.age > 1.f)
        return false;
    effect[fx++] = f;
    return true;
}

size_t propsBytes(const Props& p) {
    if (!p.any())
        return 0;
    return propsHeaderBytes + std::min<size_t>(p.items, propItemsMost) * propItemBytes +
           std::min<size_t>(p.lines, propLinesMost) * propLineBytes + std::min<size_t>(p.fx, propFxMost) * propFxBytes;
}

size_t encodeProps(const Props& p, uint8_t* out, size_t capacity) {
    const size_t need = propsBytes(p);
    if (!need || !out || capacity < need)
        return 0;
    const uint8_t ni = static_cast<uint8_t>(std::min<size_t>(p.items, propItemsMost));
    const uint8_t nl = static_cast<uint8_t>(std::min<size_t>(p.lines, propLinesMost));
    const uint8_t nf = static_cast<uint8_t>(std::min<size_t>(p.fx, propFxMost));
    Out w{out, 0, capacity};
    w.u8(propsTag), w.u8(propsVersion), w.u8(ni), w.u8(nl), w.u8(nf), w.u8(p.flags), w.u8(0), w.u8(0);
    for (size_t i = 0; i < ni; ++i) {
        const PropItem& it = p.item[i];
        w.u8(it.id), w.u8(it.kind), w.u8(it.flags), w.u8(it.variant);
        w.i16(static_cast<int16_t>(std::clamp(std::lround(wrapSpin(it.spin) * 4096.f), -32767l, 32767l)));
        w.at(it.at);
        w.way(it.forward, {0, 0, -1});
        w.way(it.up, {0, 1, 0});
    }
    for (size_t i = 0; i < nl; ++i) {
        const PropLine& l = p.line[i];
        w.u8(l.kind);
        w.u8(static_cast<uint8_t>(std::lround(std::clamp(std::isfinite(l.fade) ? l.fade : 0.f, 0.f, 1.f) * 255.f)));
        w.at(l.from), w.at(l.to);
    }
    for (size_t i = 0; i < nf; ++i) {
        const PropFx& f = p.effect[i];
        w.u8(static_cast<uint8_t>(f.seq)), w.u8(static_cast<uint8_t>(f.seq >> 8));
        w.u8(f.kind);
        w.u8(static_cast<uint8_t>(std::clamp(std::lround((std::isfinite(f.age) ? f.age : 0.f) / .004f), 0l, 255l)));
        w.at(f.at);
        w.way(f.way, {0, 0, -1});
        w.u8(f.param), w.u8(0);
    }
    return w.ok && w.n == need ? w.n : 0;
}

bool decodeProps(const uint8_t* data, size_t size, Props& out) {
    out = Props{};
    if (!data || size < propsHeaderBytes)
        return false;
    In r{data, 0, size};
    if (r.u8() != propsTag || r.u8() != propsVersion)
        return false;
    const uint8_t ni = r.u8(), nl = r.u8(), nf = r.u8(), flags = r.u8();
    r.u8(), r.u8();
    if (ni > propItemsMost || nl > propLinesMost || nf > propFxMost ||
        size < propsHeaderBytes + ni * propItemBytes + nl * propLineBytes + nf * propFxBytes)
        return false;
    Props p;
    p.flags = flags;
    for (uint8_t i = 0; i < ni; ++i) {
        PropItem it;
        it.id = r.u8(), it.kind = r.u8(), it.flags = r.u8(), it.variant = r.u8();
        it.spin = r.i16() / 4096.f;
        it.at = r.at();
        it.forward = r.way({0, 0, -1});
        it.up = r.way({0, 1, 0});
        p.add(it); // an unknown kind (a newer build's): skipped
    }
    for (uint8_t i = 0; i < nl; ++i) {
        PropLine l;
        l.kind = r.u8();
        l.fade = r.u8() / 255.f;
        l.from = r.at(), l.to = r.at();
        p.add(l);
    }
    for (uint8_t i = 0; i < nf; ++i) {
        PropFx f;
        const uint16_t lo = r.u8(), hi = r.u8();
        f.seq = static_cast<uint16_t>(lo | (hi << 8));
        f.kind = r.u8();
        f.age = r.u8() * .004f;
        f.at = r.at();
        f.way = r.way({0, 0, -1});
        f.param = r.u8();
        r.u8();
        p.add(f);
    }
    out = p;
    return true;
}

Props quantized(const Props& p) {
    uint8_t buf[propsMostBytes];
    Props q;
    const size_t n = encodeProps(p, buf, sizeof buf);
    if (n)
        decodeProps(buf, n, q);
    return q;
}

namespace {
Vec3 mix(Vec3 a, Vec3 b, float t) { return a + (b - a) * t; }
Vec3 mixWay(Vec3 a, Vec3 b, float t, Vec3 fallback) {
    const Vec3 m = mix(safeDir(a, fallback), safeDir(b, fallback), t);
    return length(m) > 1e-3f ? safeDir(m, fallback) : safeDir(b, fallback);
}
float wrapPi(float a) { return std::remainder(a, 6.2831853f); }
bool lengthSpin(uint8_t kind) { return kind == propBaton || kind == propBubble || kind == 5 /* hero007::lookClaws */; }
} // namespace

Props blendProps(const Props& a, const Props& b, float t) {
    t = std::isfinite(t) ? std::clamp(t, 0.f, 1.f) : 1.f;
    Props s = b;
    for (uint8_t k = 0; k < s.items && k < propItemsMost; ++k) {
        PropItem& it = s.item[k];
        for (uint8_t j = 0; j < a.items && j < propItemsMost; ++j) {
            const PropItem& was = a.item[j];
            if (was.id != it.id || was.kind != it.kind || ((was.flags ^ it.flags) & (propFlagInHand | propFlagSheathed)))
                continue;
            if (!finiteV(was.at) || !finiteV(it.at) || length(it.at - was.at) > propSnap)
                break;
            it.at = mix(was.at, b.item[k].at, t);
            it.forward = mixWay(was.forward, b.item[k].forward, t, {0, 0, -1});
            it.up = mixWay(was.up, b.item[k].up, t, {0, 1, 0});
            if (std::isfinite(was.spin) && std::isfinite(it.spin))
                it.spin = lengthSpin(it.kind) ? was.spin + (b.item[k].spin - was.spin) * t
                                              : wrapPi(was.spin + wrapPi(b.item[k].spin - was.spin) * t);
            break;
        }
    }
    for (uint8_t k = 0; k < s.lines && k < propLinesMost; ++k) {
        PropLine& l = s.line[k];
        for (uint8_t j = 0; j < a.lines && j < propLinesMost; ++j) {
            const PropLine& was = a.line[j];
            if (was.kind != l.kind || !finiteV(was.from) || !finiteV(was.to) || length(l.to - was.to) > propSnap ||
                length(l.from - was.from) > propSnap)
                continue;
            l.from = mix(was.from, b.line[k].from, t);
            l.to = mix(was.to, b.line[k].to, t);
            l.fade = was.fade + (b.line[k].fade - was.fade) * t;
            break;
        }
    }
    return s;
}
} // namespace spidy::mp
