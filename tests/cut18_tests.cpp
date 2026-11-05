// HERO VR 0.0.78 cut18 UI DUMP tests: which render targets are kept, their pixels to RGBA8 and coverage, the plan.
#include "spidy/ui_dump.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>

using namespace spidy::ui_dump;

namespace {
int failures = 0;
void check(bool ok, const char* what) {
    std::printf("%s: %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok)
        ++failures;
}
} // namespace

int main() {
    // Formats.
    check(bytesPerPixel(fR8G8B8A8Unorm) == 4 && bytesPerPixel(fR16G16B16A16Float) == 8, "colour formats sized");
    check(bytesPerPixel(45) == 0 && bytesPerPixel(71) == 0, "depth and block formats not dumped");
    check(!hasAlpha(fR11G11B10Float) && hasAlpha(fB8G8R8A8Unorm), "R11G11B10 has no alpha");
    // Sizes, against a 2560x1440 back buffer.
    check(sized(2560, 1440, 2560, 1440) && sized(1280, 720, 2560, 1440) && sized(3840, 2160, 2560, 1440),
          "the frame's size, half of it and above it kept");
    check(!sized(2048, 2048, 2560, 1440) && !sized(128, 72, 2560, 1440), "square and tiny targets not kept");
    // Score: an 8-bit target before a float one, the frame's size first, then more transitions.
    const uint64_t ui = score(fR8G8B8A8Unorm, 2560, 1440, 50, 2560, 1440);
    const uint64_t hdr = score(fR16G16B16A16Float, 2560, 1440, 900, 2560, 1440);
    const uint64_t half = score(fB8G8R8A8Unorm, 1280, 720, 50, 2560, 1440);
    check(ui > hdr && ui > half && half > hdr, "8-bit first, the frame's size first");
    check(score(fR8G8B8A8Unorm, 2560, 1440, 60, 2560, 1440) > ui, "more transitions ahead");
    check(score(45, 2560, 1440, 50, 2560, 1440) == 0, "depth scores 0");
    // States.
    check(readableState(stPixel) && readableState(stPixel | stNonPixel) && readableState(stCopySource),
          "shader resource and copy source readable");
    check(!readableState(stRenderTarget) && !readableState(stCommon) && !readableState(stPixel | stUav),
          "render target, common and UAV not read");
    // Pixels to RGBA8.
    check(concrete(fR8G8B8A8Typeless) == fR8G8B8A8Unorm && concrete(fR16G16B16A16Typeless) == fR16G16B16A16Float,
          "typeless read as UNORM / FLOAT");
    const uint8_t bgra[8] = {10, 20, 30, 40, 1, 2, 3, 0};
    uint8_t out[8];
    check(rowToRgba8(fB8G8R8A8Unorm, bgra, 2, out, false) && out[0] == 30 && out[2] == 10 && out[3] == 40 &&
              out[7] == 0,
          "BGRA swapped, alpha kept");
    check(rowToRgba8(fB8G8R8A8Unorm, bgra, 2, out, true) && out[3] == 255 && out[7] == 255, "opaque: alpha 255");
    const uint32_t ten = 1023u | (512u << 10) | (0u << 20) | (3u << 30);
    const uint8_t tenB[4] = {uint8_t(ten), uint8_t(ten >> 8), uint8_t(ten >> 16), uint8_t(ten >> 24)};
    check(rowToRgba8(fR10G10B10A2Unorm, tenB, 1, out, false) && out[0] == 255 && out[1] == 128 && out[2] == 0 &&
              out[3] == 255,
          "10-bit unpacked");
    check(halfToFloat(0x3c00) == 1.f && halfToFloat(0x3800) == .5f && halfToFloat(0xc000) == -2.f &&
              halfToFloat(0) == 0.f,
          "halves");
    const uint8_t h16[8] = {0x00, 0x3c, 0x00, 0x38, 0x00, 0x00, 0x00, 0x38}; // 1, .5, 0, alpha .5
    check(rowToRgba8(fR16G16B16A16Float, h16, 1, out, false) && out[0] == 255 && out[1] > 180 && out[1] < 190 &&
              out[2] == 0 && out[3] == 128,
          "float: linear .5 shown as about sRGB 186, alpha .5 as 128");
    check(!rowToRgba8(45, h16, 1, out, false), "other formats refused");
    // Coverage: a clear 8x4 layer with a white glyph at (5,1)-(6,2).
    uint8_t layer[8 * 4 * 4] = {};
    for (uint32_t y = 1; y <= 2; ++y)
        for (uint32_t x = 5; x <= 6; ++x) {
            uint8_t* p = layer + (y * 8 + x) * 4;
            p[0] = p[1] = p[2] = 250;
            p[3] = 255;
        }
    const Coverage c = cover(layer, 8, 4);
    check(c.pixels == 32 && c.alpha == 4 && c.white == 4 && c.x0 == 5 && c.y0 == 1 && c.x1 == 6 && c.y1 == 2,
          "alpha 4/32, white 4, box (5,1)-(6,2)");
    uint8_t clear[16] = {};
    const Coverage n = cover(clear, 2, 2);
    check(n.alpha == 0 && n.x0 > n.x1, "no alpha: no box");
    // Plan: nothing while off.
    Plan off;
    check(off.frame(false, true, 1000) == reasonNone && off.frame(false, true, 30000) == reasonNone, "off: never");
    // The play baseline after 20 s of VR play in a row; a menu resets it.
    Plan p;
    check(p.frame(true, true, 1000) == reasonNone && p.frame(true, false, 15000) == reasonNone, "a menu resets play");
    check(p.frame(true, true, 16000) == reasonNone && p.frame(true, true, 35000) == reasonNone, "19 s: not yet");
    check(p.frame(true, true, 36000) == reasonPlay, "20 s of play: the baseline");
    check(p.frame(true, true, 60000) == reasonNone, "the baseline once");
    // A prompt moment, its delay, the gap; the gate once.
    p.ask(reasonPrompt, 38000);
    check(p.frame(true, true, 38200) == reasonNone, "the prompt's 400 ms delay");
    check(p.frame(true, true, 40100) == reasonPrompt, "the prompt dump");
    p.ask(reasonPrompt, 41000);
    p.ask(reasonBossGate, 41000);
    check(p.frame(true, true, 41500) == reasonNone, "4 s between dumps");
    check(p.frame(true, true, 44200) == reasonBossGate, "the gate dump");
    p.ask(reasonBossGate, 50000);
    check(p.frame(true, true, 60000) == reasonNone && p.count == 3, "each reason once");
    // A prompt moment while the baseline waits goes first; the baseline follows.
    Plan q;
    q.frame(true, true, 1);
    q.lastMs = 19000; // a dump just before
    q.frame(true, true, 20002); // play due, held by the gap
    q.ask(reasonBossGate, 20100);
    check(q.frame(true, true, 23100) == reasonBossGate, "the gate before the waiting baseline");
    check(q.frame(true, true, 27200) == reasonPlay, "the baseline after");
    // At most maxDumps.
    Plan m;
    m.count = maxDumps;
    m.ask(reasonPrompt, 0);
    check(m.frame(true, true, 100000) == reasonNone, "at most 4 a session");
    check(!std::strncmp(reasonName(reasonBossGate), "the boss's Y moment", 19), "reason names");
    // A moment asked in a menu waits for VR play.
    Plan v;
    v.ask(reasonPrompt, 1000);
    check(v.frame(true, false, 5000) == reasonNone && v.frame(true, true, 5100) == reasonPrompt, "dumps in VR play only");
    std::printf("%s (%d failed)\n", failures ? "FAILED" : "all ok", failures);
    return failures ? 1 : 0;
}
