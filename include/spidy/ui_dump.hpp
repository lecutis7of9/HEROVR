#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
// HERO VR 0.0.78 cut18 UI DUMP (diagnostic). Where are the game's own button prompts (the Y web strike on Sandman, the
// interact prompts) in VR play? CONFIRMED from the source and the logs: the SM2 HUD is not hooked (native_hud reports
// hudNotPorted 9599, SM2_STEREO_HUD 0), so no HUD panel texture exists ("WRIST MAP SOURCE ... no HUD panel ... looked
// at 0 render targets" in every cut13/pub077 log), and the eyes never carry the HUD. CANDIDATE (not proven): the game's
// cohtml (Coherent Gameface) UI is drawn into a render target of its own and/or onto the back buffer it presents.
// This dumps, a few times a session at the moments a prompt should be up, the presented back buffer and the render
// targets seen going from RENDER_TARGET to a readable state with the frame's shape, as PNG files beside spidy_vr.ini
// (%APPDATA%\Spidy\ui-dump), with one log line per target (size, format, transitions, alpha cover and its box).
// The pictures say which surface holds the prompts, alone or over the world; the fix crops that surface.
// Pure and allocation-free; nothing here reads or writes the game.
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace spidy::ui_dump {
// live.txt ui_dump (SpidyUiDump 5645): 0 on, 1 off (the default since cut21: a dump's read-back can hitch up to 2 s).
enum Mode : uint32_t { dumpOn = 0, dumpOff = 1 };
// Why a dump was asked for (each at most once a session, and at most maxDumps in all).
enum Reason : uint32_t { reasonNone = 0, reasonPlay = 1, reasonPrompt = 2, reasonBossGate = 3, reasonKinds = 4 };
inline const char* reasonName(uint32_t r) {
    static const char* names[] = {"none", "VR play 20 s", "a prompt card in reach",
                                  "the boss's Y moment (web strike on him or the phase gate)"};
    return r < reasonKinds ? names[r] : "?";
}
inline constexpr uint32_t maxDumps = 4;
inline constexpr uint32_t maxTargets = 4;      // render targets written per dump (plus the back buffer)
inline constexpr uint32_t maxCandidates = 96;   // kept while hunting
inline constexpr uint32_t huntPresents = 45;    // presents watched before a dump (about 0.5-1 s)
inline constexpr uint64_t playDelayMs = 20000;  // reasonPlay: after this much VR play in a row
inline constexpr uint64_t promptDelayMs = 400;  // after a prompt object / the gate: the prompt fades in
inline constexpr uint64_t minGapMs = 4000;      // between two dumps

// DXGI formats a UI or colour target is drawn in (numbers as dxgiformat.h).
inline constexpr uint32_t fR16G16B16A16Typeless = 9, fR16G16B16A16Float = 10, fR10G10B10A2Typeless = 23,
                          fR10G10B10A2Unorm = 24, fR11G11B10Float = 26, fR8G8B8A8Typeless = 27, fR8G8B8A8Unorm = 28,
                          fR8G8B8A8Srgb = 29, fB8G8R8A8Unorm = 87, fB8G8R8A8Typeless = 90, fB8G8R8A8Srgb = 91;
inline uint32_t bytesPerPixel(uint32_t f) {
    switch (f) {
    case fR16G16B16A16Typeless:
    case fR16G16B16A16Float:
        return 8;
    case fR10G10B10A2Typeless:
    case fR10G10B10A2Unorm:
    case fR11G11B10Float:
    case fR8G8B8A8Typeless:
    case fR8G8B8A8Unorm:
    case fR8G8B8A8Srgb:
    case fB8G8R8A8Unorm:
    case fB8G8R8A8Typeless:
    case fB8G8R8A8Srgb:
        return 4;
    default:
        return 0;
    }
}
inline bool hasAlpha(uint32_t f) {
    return bytesPerPixel(f) && f != fR11G11B10Float;
}
inline bool eightBit(uint32_t f) {
    return (f >= fR8G8B8A8Typeless && f <= fR8G8B8A8Srgb) || f == fB8G8R8A8Unorm || f == fB8G8R8A8Typeless ||
           f == fB8G8R8A8Srgb;
}
// A target worth keeping: 2D colour, at least 256x144, with the back buffer's aspect within 2% (any size: the UI may be
// drawn at the window size, a fraction of it or a render size above it).
inline bool sized(uint32_t w, uint32_t h, uint32_t backW, uint32_t backH) {
    if (w < 256 || h < 144 || !backW || !backH)
        return false;
    const double a = static_cast<double>(w) / h, b = static_cast<double>(backW) / backH;
    return a > b * .98 && a < b * 1.02;
}
// The order targets are written in: 8-bit with alpha (a UI layer's usual format) first, then by how often they went
// from RENDER_TARGET to readable (a layer drawn every frame), then the larger. Higher is first.
inline uint64_t score(uint32_t format, uint32_t w, uint32_t h, uint64_t toRead, uint32_t backW, uint32_t backH) {
    if (!bytesPerPixel(format) || !sized(w, h, backW, backH))
        return 0;
    uint64_t s = 1;
    if (eightBit(format))
        s += 1ull << 40;
    if (w == backW && h == backH)
        s += 1ull << 39;
    s += (toRead > 0xffff ? 0xffff : toRead) << 20;
    s += (w > 0xfffff ? 0xfffff : w);
    return s;
}
// Readable (copyable now) states: shader resources or copy source, and nothing that writes.
inline constexpr uint32_t stRenderTarget = 0x4, stUav = 0x8, stNonPixel = 0x40, stPixel = 0x80, stCopySource = 0x800,
                          stCommon = 0;
inline bool readableState(uint32_t s) {
    return (s & (stNonPixel | stPixel | stCopySource)) && !(s & (stRenderTarget | stUav));
}

// When to dump: each reason once, maxDumps in all, minGapMs apart, its delay after it was asked.
struct Plan {
    uint32_t done{}, count{}, due{};
    uint64_t dueMs{}, lastMs{}, playFromMs{};
    // Every VR loop frame: vrPlay now. Returns the reason due now (and takes it), else reasonNone; only in VR play.
    uint32_t frame(bool on, bool vrPlay, uint64_t nowMs) {
        if (!on)
            return reasonNone;
        if (!vrPlay)
            playFromMs = 0;
        else if (!playFromMs)
            playFromMs = nowMs ? nowMs : 1;
        if (vrPlay && playFromMs && nowMs - playFromMs >= playDelayMs)
            ask(reasonPlay, nowMs, 0);
        if (!due || !vrPlay || nowMs < dueMs || (lastMs && nowMs - lastMs < minGapMs) || count >= maxDumps)
            return reasonNone;
        const uint32_t r = due;
        due = 0;
        lastMs = nowMs ? nowMs : 1;
        ++count;
        return r;
    }
    // A moment a prompt should be up. Ignored once that reason was asked, or while another prompt moment waits.
    void ask(uint32_t reason, uint64_t nowMs, uint64_t delayMs = promptDelayMs) {
        if (!reason || reason >= reasonKinds || (done & (1u << reason)) || count >= maxDumps)
            return;
        if (due == reasonPlay && reason != reasonPlay)
            done &= ~(1u << reasonPlay); // a prompt moment goes first; the play baseline comes later
        else if (due)
            return;
        done |= 1u << reason;
        due = reason;
        dueMs = nowMs + delayMs;
    }
};

// The concrete format a typeless one is read as (UNORM, or FLOAT for 16-bit).
inline uint32_t concrete(uint32_t f) {
    switch (f) {
    case fR16G16B16A16Typeless:
        return fR16G16B16A16Float;
    case fR10G10B10A2Typeless:
        return fR10G10B10A2Unorm;
    case fR8G8B8A8Typeless:
        return fR8G8B8A8Unorm;
    case fB8G8R8A8Typeless:
        return fB8G8R8A8Unorm;
    default:
        return f;
    }
}
inline float halfToFloat(uint16_t h) {
    const uint32_t e = (h >> 10) & 31, m = h & 1023;
    float v;
    if (!e)
        v = m / 16777216.f; // m * 2^-24
    else if (e == 31)
        v = m ? 0.f : 65504.f; // NaN read as black, infinity as the brightest
    else {
        v = 1.f + m / 1024.f;
        for (uint32_t i = e; i > 15; --i)
            v *= 2.f;
        for (uint32_t i = e; i < 15; ++i)
            v *= .5f;
    }
    return (h & 0x8000) ? -v : v;
}
// 11- and 10-bit unsigned floats of R11G11B10_FLOAT (5-bit exponent as a half's, 6 or 5 bits of mantissa).
inline float smallFloat(uint32_t bits, uint32_t mantissaBits) {
    return halfToFloat(static_cast<uint16_t>(bits << (10 - mantissaBits)));
}
inline uint8_t unit8(float v, bool linear) {
    if (!(v > 0.f))
        return 0;
    if (v >= 1.f)
        return 255;
    if (linear)
        v = std::pow(v, 1.f / 2.2f); // linear light, shown as about sRGB
    return static_cast<uint8_t>(v * 255.f + .5f);
}
// One mapped row of `format` (concrete) to RGBA8. opaque: alpha written 255 (the back buffer, whose alpha means
// nothing on screen). Floats are read as linear light and shown as about sRGB; 8- and 10-bit values as they are.
inline bool rowToRgba8(uint32_t format, const uint8_t* src, uint32_t w, uint8_t* dst, bool opaque) {
    format = concrete(format);
    for (uint32_t x = 0; x < w; ++x, dst += 4) {
        uint8_t r{}, g{}, b{}, a = 255;
        if (format == fR8G8B8A8Unorm || format == fR8G8B8A8Srgb) {
            const uint8_t* p = src + x * 4;
            r = p[0], g = p[1], b = p[2], a = p[3];
        } else if (format == fB8G8R8A8Unorm || format == fB8G8R8A8Srgb) {
            const uint8_t* p = src + x * 4;
            b = p[0], g = p[1], r = p[2], a = p[3];
        } else if (format == fR10G10B10A2Unorm) {
            const uint8_t* p = src + x * 4;
            const uint32_t v = p[0] | p[1] << 8 | p[2] << 16 | static_cast<uint32_t>(p[3]) << 24;
            r = static_cast<uint8_t>((v & 1023) >> 2), g = static_cast<uint8_t>(((v >> 10) & 1023) >> 2);
            b = static_cast<uint8_t>(((v >> 20) & 1023) >> 2), a = static_cast<uint8_t>((v >> 30) * 85);
        } else if (format == fR11G11B10Float) {
            const uint8_t* p = src + x * 4;
            const uint32_t v = p[0] | p[1] << 8 | p[2] << 16 | static_cast<uint32_t>(p[3]) << 24;
            r = unit8(smallFloat(v & 2047, 6), true), g = unit8(smallFloat((v >> 11) & 2047, 6), true);
            b = unit8(smallFloat(v >> 22, 5), true);
        } else if (format == fR16G16B16A16Float) {
            const uint8_t* p = src + x * 8;
            const auto h = [&](int i) { return static_cast<uint16_t>(p[i * 2] | p[i * 2 + 1] << 8); };
            r = unit8(halfToFloat(h(0)), true), g = unit8(halfToFloat(h(1)), true), b = unit8(halfToFloat(h(2)), true);
            a = unit8(halfToFloat(h(3)), false);
        } else
            return false;
        dst[0] = r, dst[1] = g, dst[2] = b, dst[3] = opaque ? 255 : a;
    }
    return true;
}
// What a UI layer holds, from its RGBA8 picture: the share of pixels with any alpha (a UI layer is mostly clear), of
// near-white colourless ones (prompt text, button glyphs), and the box round the alpha (x0 > x1: no alpha).
struct Coverage {
    uint64_t pixels{}, alpha{}, white{};
    uint32_t x0 = UINT32_MAX, y0 = UINT32_MAX, x1{}, y1{};
};
inline Coverage cover(const uint8_t* rgba, uint32_t w, uint32_t h) {
    Coverage c;
    c.pixels = static_cast<uint64_t>(w) * h;
    for (uint32_t y = 0; y < h; ++y)
        for (uint32_t x = 0; x < w; ++x) {
            const uint8_t* p = rgba + (static_cast<std::size_t>(y) * w + x) * 4;
            if (p[3] >= 8) {
                ++c.alpha;
                if (x < c.x0)
                    c.x0 = x;
                if (y < c.y0)
                    c.y0 = y;
                if (x > c.x1)
                    c.x1 = x;
                if (y > c.y1)
                    c.y1 = y;
            }
            const uint8_t lo = p[0] < p[1] ? (p[0] < p[2] ? p[0] : p[2]) : (p[1] < p[2] ? p[1] : p[2]);
            const uint8_t hi = p[0] > p[1] ? (p[0] > p[2] ? p[0] : p[2]) : (p[1] > p[2] ? p[1] : p[2]);
            if (lo >= 200 && hi - lo <= 24)
                ++c.white;
        }
    if (!c.alpha)
        c.x0 = 1, c.y0 = 1, c.x1 = 0, c.y1 = 0;
    return c;
}

} // namespace spidy::ui_dump
