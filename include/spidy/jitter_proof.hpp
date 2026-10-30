#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
//
// 0.0.74 test 7 CLEAN JITTER PROOF: does CLEAN's lens step reach the pixels? Every ~2 s, with the head still,
// native_eye_gpu copies a 128 x 128 box from the middle of each RAW eye (staged, before the eye history) for two
// pairs in a row, reads them back, and here the CPU finds how far the picture moved between them (sum of squared
// differences, whole pixels within the range, then quarter and sixteenth pixel steps, bilinear). The head's own
// turn between the pairs (eye_history::historyReprojection at the box centre) is taken off, so what is left is
// the lens step's share: it should equal the step sent (stereo_probe cleanTaaJitter). Pure code, unit tested.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

namespace spidy::jitter_proof {
inline constexpr uint32_t box = 128;          // the box's side, pixels
inline constexpr uint32_t everyMs = 2000;     // between readings
inline constexpr float range = 2.f;           // the search, pixels each way
inline constexpr float bigRange = 10.f;       // with JITTER PROOF BIG (a +-4 pixel step)
inline constexpr float bigScale = 8.f;        // JITTER PROOF BIG: the +-0.5 pixel Halton step times 8
inline constexpr float stillTurn = .02f;      // radians between the two pairs at most (head still)
inline constexpr float stillTravel = .01f;    // metres
inline constexpr float flatVariance = 1e-5f;  // a box flatter than this (sky) is not measured

// The pixel kinds the box can be read in (DXGI_FORMAT values, so this header needs no Windows headers).
enum class Kind : uint32_t { unsupported, rgba8, rgb10a2, rgba16f };
inline Kind kindOf(uint32_t dxgi) {
    switch (dxgi) {
    case 27: case 28: case 29: // R8G8B8A8 TYPELESS / UNORM / UNORM_SRGB
    case 87: case 90: case 91: // B8G8R8A8 UNORM / TYPELESS / UNORM_SRGB (the luma below is order-free)
        return Kind::rgba8;
    case 23: case 24: // R10G10B10A2 TYPELESS / UNORM
        return Kind::rgb10a2;
    case 9: case 10: // R16G16B16A16 TYPELESS / FLOAT
        return Kind::rgba16f;
    default:
        return Kind::unsupported;
    }
}
inline uint32_t bytesPer(Kind k) { return k == Kind::rgba16f ? 8u : k == Kind::unsupported ? 0u : 4u; }
// The box's row pitch in the readback buffer (D3D12 needs 256-byte rows).
inline uint32_t rowPitch(Kind k) { return (box * bytesPer(k) + 255u) / 256u * 256u; }
inline float halfToFloat(uint16_t h) {
    const uint32_t sign = (h >> 15) & 1u, exponent = (h >> 10) & 0x1fu, mantissa = h & 0x3ffu;
    float v;
    if (exponent == 0)
        v = std::ldexp(static_cast<float>(mantissa), -24);
    else if (exponent == 31)
        v = mantissa ? 0.f : 65504.f; // NaN read as black, infinity as the largest half
    else
        v = std::ldexp(static_cast<float>(mantissa | 0x400u), static_cast<int>(exponent) - 25);
    return sign ? -v : v;
}
// One box (box x box pixels at rowPitch) to luma, the mean of the three colours, 0-1 for the unorm kinds.
inline bool toLuma(Kind k, const uint8_t* data, uint32_t pitch, std::vector<float>& out) {
    if (!data || k == Kind::unsupported || pitch < box * bytesPer(k))
        return false;
    out.assign(static_cast<size_t>(box) * box, 0.f);
    for (uint32_t y = 0; y < box; ++y) {
        const uint8_t* row = data + static_cast<size_t>(y) * pitch;
        for (uint32_t x = 0; x < box; ++x) {
            float l = 0;
            if (k == Kind::rgba8) {
                const uint8_t* p = row + x * 4;
                l = static_cast<float>(p[0] + p[1] + p[2]) / (3.f * 255.f);
            } else if (k == Kind::rgb10a2) {
                uint32_t v;
                std::memcpy(&v, row + x * 4, 4);
                l = static_cast<float>((v & 1023u) + ((v >> 10) & 1023u) + ((v >> 20) & 1023u)) / (3.f * 1023.f);
            } else {
                uint16_t c[3];
                std::memcpy(c, row + x * 8, 6);
                l = (halfToFloat(c[0]) + halfToFloat(c[1]) + halfToFloat(c[2])) / 3.f;
                if (!std::isfinite(l))
                    l = 0;
            }
            out[static_cast<size_t>(y) * box + x] = l;
        }
    }
    return true;
}
struct Shift {
    bool ok{};
    float x{}, y{};    // pixels the picture moved from the first box to the second (+x right, +y down)
    float error{};     // the mean squared difference left at the best shift
    float variance{};  // the first box's (inside the margin): its detail
};
// How far the picture moved from a to b (both box x box luma): the shift s for which b(p) is a(p - s), the
// nearest sixteenth of a pixel within +-reach. Only the inside of the box (a margin of reach + 2) is compared.
inline Shift measure(const std::vector<float>& a, const std::vector<float>& b, float reach) {
    Shift s{};
    const size_t n = static_cast<size_t>(box) * box;
    if (a.size() != n || b.size() != n || !(reach > 0) || reach > static_cast<float>(box / 4))
        return s;
    const int margin = static_cast<int>(std::ceil(reach)) + 2, lo = margin, hi = static_cast<int>(box) - margin;
    double sum = 0, square = 0;
    for (int y = lo; y < hi; ++y)
        for (int x = lo; x < hi; ++x) {
            const double v = a[static_cast<size_t>(y) * box + static_cast<size_t>(x)];
            sum += v;
            square += v * v;
        }
    const double count = static_cast<double>(hi - lo) * (hi - lo);
    s.variance = static_cast<float>(square / count - (sum / count) * (sum / count));
    if (!(s.variance > flatVariance))
        return s;
    auto at = [&](float fx, float fy) { // a, bilinear
        const float cx = std::clamp(fx, 0.f, static_cast<float>(box) - 1.001f), cy = std::clamp(fy, 0.f, static_cast<float>(box) - 1.001f);
        const int x0 = static_cast<int>(cx), y0 = static_cast<int>(cy);
        const float tx = cx - static_cast<float>(x0), ty = cy - static_cast<float>(y0);
        const size_t i = static_cast<size_t>(y0) * box + static_cast<size_t>(x0);
        return (a[i] * (1 - tx) + a[i + 1] * tx) * (1 - ty) + (a[i + box] * (1 - tx) + a[i + box + 1] * tx) * ty;
    };
    auto cost = [&](float sx, float sy) {
        double e = 0;
        for (int y = lo; y < hi; ++y)
            for (int x = lo; x < hi; ++x) {
                const double d = b[static_cast<size_t>(y) * box + static_cast<size_t>(x)] -
                                 at(static_cast<float>(x) - sx, static_cast<float>(y) - sy);
                e += d * d;
            }
        return e / count;
    };
    const int whole = static_cast<int>(std::floor(reach));
    float bx = 0, by = 0;
    double best = cost(0, 0);
    for (int y = -whole; y <= whole; ++y)
        for (int x = -whole; x <= whole; ++x)
            if (const double e = cost(static_cast<float>(x), static_cast<float>(y)); e < best) {
                best = e;
                bx = static_cast<float>(x);
                by = static_cast<float>(y);
            }
    for (const float step : {.25f, .0625f}) {
        const float cx = bx, cy = by;
        for (int y = -4; y <= 4; ++y)
            for (int x = -4; x <= 4; ++x) {
                const float sx = cx + static_cast<float>(x) * step, sy = cy + static_cast<float>(y) * step;
                if (std::abs(sx) > reach || std::abs(sy) > reach)
                    continue;
                if (const double e = cost(sx, sy); e < best) {
                    best = e;
                    bx = sx;
                    by = sy;
                }
            }
    }
    s.ok = true;
    s.x = bx;
    s.y = by;
    s.error = static_cast<float>(best);
    return s;
}
// The head's share of the move at the box centre: h takes a pixel of the second pair (u, v in 0-1) to the
// first's (eye_history::historyReprojection), so what sat at the first pair's h(c) is at c now.
inline bool headShift(const std::array<float, 9>& h, uint32_t width, uint32_t height, float& x, float& y) {
    const float u = .5f, v = .5f;
    const float U = h[0] * u + h[1] * v + h[2], V = h[3] * u + h[4] * v + h[5], W = h[6] * u + h[7] * v + h[8];
    if (!(W > 1e-6f) || !std::isfinite(U) || !std::isfinite(V))
        return false;
    x = (u - U / W) * static_cast<float>(width);
    y = (v - V / W) * static_cast<float>(height);
    return std::isfinite(x) && std::isfinite(y);
}
// What one reading found (native_gpu::jitterProof, game_xr's CLEAN JITTER PROOF lines).
struct Reading {
    uint64_t number{};          // readings so far (a new one when it grows)
    uint32_t fault{};           // nonzero: the probe is off for the session (faultText)
    bool big{};                 // JITTER PROOF BIG was on
    bool stepped{};             // both pairs had CLEAN's lens step
    bool ok[2]{};               // the eye's box had detail and was measured
    float sent[2][2]{};         // the step change between the pairs, headset pixels (eye, x / y)
    float measured[2][2]{};     // the move found, the head's share taken off
    float head[2][2]{};         // the head's share
    float error[2]{};           // the mean squared difference left
    uint64_t skippedMoving{};   // tries given up so far: the head moved between the pairs
    uint64_t skippedFlat{};     // eyes not measured so far: a flat box
    // 0.0.74 test 8 DISPLAY PROOF: the second pair's box again after the history (what the headset copy reads,
    // before CAS), against its RAW box: the mean luma difference (0-1 levels; 0 when no history ran) and its move
    // from the first pair's RAW box.
    bool shownRead{};           // the after-history boxes were read
    bool historyRan{};          // the eye history ran on that pair
    bool shownOk[2]{};          // the move could be measured
    float shownDiff[2]{};       // mean |displayed - raw| luma, -1 unread
    float shownMeasured[2][2]{}; // displayed vs the first RAW pair, the head's share taken off
};
// Mean absolute difference of two boxes of luma; -1 when they differ in size or are empty.
inline float meanAbsDiff(const std::vector<float>& a, const std::vector<float>& b) {
    if (a.empty() || a.size() != b.size())
        return -1.f;
    double sum = 0;
    for (size_t i = 0; i < a.size(); ++i)
        sum += std::fabs(static_cast<double>(a[i]) - static_cast<double>(b[i]));
    return static_cast<float>(sum / static_cast<double>(a.size()));
}
inline const char* faultText(uint32_t f) {
    switch (f) {
    case 0: return "none";
    case 1: return "the eye format cannot be read";
    case 2: return "the eyes are too small";
    case 3: return "the readback buffer could not be made";
    case 4: return "the readback could not be mapped";
    case 5: return "the box could not be read";
    case 6: return "the measurement could not be queued";
    default: return "unknown";
    }
}
} // namespace spidy::jitter_proof
